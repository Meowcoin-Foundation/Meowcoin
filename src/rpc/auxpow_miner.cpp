// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/auxpow_miner.h>

#include <auxpow.h>
#include <consensus/merkle.h>
#include <interfaces/mining.h>
#include <logging.h>
#include <node/miner.h>
#include <pow.h>
#include <primitives/block.h>
#include <script/script.h>
#include <streams.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>

#include <cassert>

namespace auxpow_miner {

uint256 TemplateCache::createBlock(const CScript& scriptPubKey,
                                   interfaces::Mining& miner,
                                   ChainstateManager& chainman,
                                   const CTxMemPool& mempool)
{
    const unsigned int mempool_seq = mempool.GetTransactionsUpdated();

    // Reuse the most recently built candidate for this payout address unless
    // something that should actually change it has happened: the chain tip
    // moving, or the mempool having changed *and* the rebuild debounce
    // having elapsed. A static mempool and unchanged tip means we keep
    // returning the same hash indefinitely -- matching legacy, where
    // repeated polls only get a new job when there's a real reason for one.
    // Kept per address so a node fielding more than one payout address
    // doesn't thrash a single shared slot and lose this benefit entirely.
    {
        std::lock_guard<std::mutex> lock(m_cs);
        auto it = m_last_by_address.find(scriptPubKey);
        if (it != m_last_by_address.end() &&
            m_templates.count(it->second.hash) &&
            (mempool_seq == it->second.mempool_seq ||
             std::chrono::steady_clock::now() - it->second.build_time < MEMPOOL_REBUILD_DEBOUNCE)) {
            const CBlockIndex* tip = WITH_LOCK(chainman.GetMutex(), return chainman.ActiveTip());
            if (tip && tip->GetBlockHash() == it->second.prev_hash) {
                return it->second.hash;
            }
        }
    }

    // Create a new block template via the Mining interface.
    node::BlockCreateOptions opts;
    opts.coinbase_output_script = scriptPubKey;

    auto block_template = miner.createNewBlock(opts);
    if (!block_template) {
        throw std::runtime_error("Failed to create block template");
    }

    auto pblock = std::make_shared<CBlock>(block_template->getBlock());

    // Mark this block as an AuxPoW block.
    pblock->nVersion.SetAuxpow(true);

    // Set chain ID from consensus params.
    {
        LOCK(chainman.GetMutex());
        const auto& consensus = chainman.GetConsensus();
        pblock->nVersion.SetChainId(consensus.nAuxpowChainId);

        // Recalculate nBits for the scrypt difficulty (AuxPoW uses scrypt).
        //
        // Must use the same parent that createNewBlock() already baked into
        // hashPrevBlock, not a fresh ActiveTip() read: createNewBlock() locks
        // and releases cs_main internally, so by the time we get here the active
        // tip may have advanced past hashPrevBlock. Recomputing against the new
        // tip would desync nBits from hashPrevBlock, producing a header every
        // validator rejects with bad-diffbits despite a perfectly valid AuxPoW.
        CBlockIndex* pindexPrev = chainman.m_blockman.LookupBlockIndex(pblock->hashPrevBlock);
        if (pindexPrev) {
            pblock->nBits = GetNextWorkRequired(pindexPrev, pblock.get(),
                                                 chainman.GetConsensus(), true);
        }
    }

    // Recompute the merkle root after any modifications.
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);

    // The hash the parent chain must solve for (SHA256d of the pure header).
    uint256 hash = pblock->GetHash();

    // Cache the template.
    {
        std::lock_guard<std::mutex> lock(m_cs);

        // Read the tip while already holding m_cs, not before acquiring it:
        // reading it first and acting on that snapshot afterward leaves a
        // window where a different thread can acquire m_cs first, observe a
        // newer tip, and publish a correct, up-to-date candidate for it --
        // which this thread would then evict/overwrite using its own,
        // already-stale snapshot. Reading it under the same lock we use to
        // act on it closes that window: whichever thread gets here first
        // sees a tip and acts on it atomically with respect to the other.
        //
        // This is also why the eviction below uses the actual tip rather
        // than this build's own hashPrevBlock: createNewBlock() releases
        // cs_main internally, so a slow or racing call can finish after the
        // tip has already moved again, and evicting based on a stale
        // hashPrevBlock would wrongly delete a different, fresher template.
        const uint256 current_tip_hash = WITH_LOCK(chainman.GetMutex(),
            return chainman.ActiveTip() ? chainman.ActiveTip()->GetBlockHash() : uint256());

        // Drop templates left over from a previous tip: once the tip moves,
        // they can no longer be submitted successfully, so there's no
        // reason to keep them around.
        for (auto it = m_templates.begin(); it != m_templates.end(); ) {
            if (it->second->hashPrevBlock != current_tip_hash) {
                it = m_templates.erase(it);
            } else {
                ++it;
            }
        }

        // Always keep the built template retrievable by hash: even a build
        // that lost a race against a tip change (below) may already have had
        // its hash handed to a caller, who must still be able to submit it.
        m_templates[hash] = pblock;

        // Only publish this build as the recommended candidate for this
        // address if it actually reflects the current tip. createNewBlock()
        // releases cs_main internally, so a slow or racing call can finish
        // after the tip has already moved again; pblock->hashPrevBlock is
        // fixed at that point and may no longer equal current_tip_hash. If we
        // recorded current_tip_hash here regardless, a subsequent poll's
        // cache-hit check (which only compares the tip against this
        // recorded value) would incorrectly treat the stale build as current
        // and keep serving it indefinitely, rather than rebuilding against
        // the real tip.
        if (pblock->hashPrevBlock == current_tip_hash) {
            auto it = m_last_by_address.find(scriptPubKey);
            if (it == m_last_by_address.end() && m_last_by_address.size() >= MAX_CACHED_ADDRESSES) {
                // Bound memory use: evict whichever tracked address was built
                // longest ago to make room for this one.
                auto oldest = m_last_by_address.begin();
                for (auto cand = m_last_by_address.begin(); cand != m_last_by_address.end(); ++cand) {
                    if (cand->second.build_time < oldest->second.build_time) oldest = cand;
                }
                m_last_by_address.erase(oldest);
            }
            CachedCandidate& entry = m_last_by_address[scriptPubKey];
            entry.hash = hash;
            entry.prev_hash = current_tip_hash;
            entry.mempool_seq = mempool_seq;
            entry.build_time = std::chrono::steady_clock::now();
        }
    }

    return hash;
}

bool TemplateCache::submitBlock(const uint256& hashBlock,
                                const std::string& auxpowHex,
                                ChainstateManager& chainman)
{
    std::shared_ptr<CBlock> pblock;
    {
        std::lock_guard<std::mutex> lock(m_cs);
        auto it = m_templates.find(hashBlock);
        if (it == m_templates.end()) {
            LogError("submitauxblock: block template not found for hash %s\n",
                     hashBlock.GetHex());
            return false;
        }
        pblock = it->second;
        m_templates.erase(it);
    }

    // Deserialize the AuxPoW from hex.
    auto auxpowBytes = ParseHex(auxpowHex);
    DataStream ss{auxpowBytes};

    auto auxpow = std::make_shared<CAuxPow>();
    ss >> TX_WITH_WITNESS(*auxpow);

    pblock->SetAuxpow(std::move(auxpow));

    // Submit the block.
    bool newBlock = false;
    bool accepted = chainman.ProcessNewBlock(pblock,
                                              /*force_processing=*/true,
                                              /*min_pow_checked=*/true,
                                              &newBlock);

    if (accepted && newBlock) {
        LogInfo("AuxPoW block accepted: %s\n", pblock->GetHash().GetHex());
    }

    return accepted;
}

std::shared_ptr<CBlock> TemplateCache::getBlock(const uint256& hash)
{
    std::lock_guard<std::mutex> lock(m_cs);
    auto it = m_templates.find(hash);
    if (it != m_templates.end()) return it->second;
    return nullptr;
}

} // namespace auxpow_miner
