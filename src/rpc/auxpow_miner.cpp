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
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>

#include <cassert>

namespace auxpow_miner {

uint256 TemplateCache::createBlock(const CScript& scriptPubKey,
                                   interfaces::Mining& miner,
                                   ChainstateManager& chainman)
{
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
        CBlockIndex* pindexPrev = chainman.ActiveTip();
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
        m_templates[hash] = pblock;
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
