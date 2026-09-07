// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_AUXPOW_MINER_H
#define BITCOIN_RPC_AUXPOW_MINER_H

/**
 * AuxPoW merge-mining helper declarations.
 *
 * These provide the block-template creation and submission logic that
 * backs the createauxblock / submitauxblock RPCs.
 */

#include <primitives/block.h>
#include <script/script.h>
#include <uint256.h>
#include <util/hasher.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class ChainstateManager;
class CTxMemPool;

namespace node { struct NodeContext; }
namespace interfaces { class Mining; }

namespace auxpow_miner {

/** Hold recent block templates keyed by block hash so a solved AuxPoW
 *  can be matched back to its template.  Thread-safe.
 *
 *  Also caches the most recently built candidate so repeated polls (as
 *  merge-miners typically do every few seconds) return the same hash
 *  instead of minting a brand-new template -- and therefore a new hash,
 *  since the header commits to nTime -- on every call. This mirrors the
 *  legacy (pre-rebase) AuxMiningCreateBlock caching rule: the candidate
 *  is rebuilt only when the chain tip changes, or when the mempool has
 *  actually changed *and* at least MEMPOOL_REBUILD_DEBOUNCE has passed
 *  since the last rebuild. A static mempool and unchanged tip means the
 *  same hash forever -- there is no rebuild-on-elapsed-time-alone case. */
class TemplateCache
{
public:
    /** Create a new block template for merge-mining, or return the hash of
     *  an already-cached one still valid for this scriptPubKey and tip.
     *  Returns the pure-header hash (the hash the parent chain must solve for). */
    uint256 createBlock(const CScript& scriptPubKey,
                        interfaces::Mining& miner,
                        ChainstateManager& chainman,
                        const CTxMemPool& mempool);

    /** Submit a solved AuxPoW for a previously-created template.
     *  @param hashBlock   The block hash returned by createBlock.
     *  @param auxpowHex   Serialized CAuxPow in hex.
     *  @return true if the block was accepted. */
    bool submitBlock(const uint256& hashBlock, const std::string& auxpowHex,
                     ChainstateManager& chainman);

    /** Get the currently cached block (for RPC result building). */
    std::shared_ptr<CBlock> getBlock(const uint256& hash);

private:
    /** Minimum time between rebuilds that are triggered purely by the
     *  mempool changing (a tip change always rebuilds immediately). Matches
     *  legacy's AuxMiningCreateBlock, which used the same 60s debounce. */
    static constexpr std::chrono::seconds MEMPOOL_REBUILD_DEBOUNCE{60};

    std::mutex m_cs;
    std::unordered_map<uint256, std::shared_ptr<CBlock>, SaltedUint256Hasher> m_templates;

    // Bookkeeping for the most recently built candidate, to decide whether
    // a poll can be served from m_templates without rebuilding.
    uint256 m_last_hash;
    CScript m_last_scriptPubKey;
    uint256 m_last_prev_hash;
    unsigned int m_last_mempool_seq{0};
    std::chrono::steady_clock::time_point m_last_build_time{};
};

} // namespace auxpow_miner

#endif // BITCOIN_RPC_AUXPOW_MINER_H
