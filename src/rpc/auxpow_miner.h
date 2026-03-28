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
#include <uint256.h>
#include <util/hasher.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class CScript;
class ChainstateManager;

namespace node { struct NodeContext; }
namespace interfaces { class Mining; }

namespace auxpow_miner {

/** Hold recent block templates keyed by block hash so a solved AuxPoW
 *  can be matched back to its template.  Thread-safe. */
class TemplateCache
{
public:
    /** Create a new block template for merge-mining.
     *  Returns the pure-header hash (the hash the parent chain must solve for). */
    uint256 createBlock(const CScript& scriptPubKey,
                        interfaces::Mining& miner,
                        ChainstateManager& chainman);

    /** Submit a solved AuxPoW for a previously-created template.
     *  @param hashBlock   The block hash returned by createBlock.
     *  @param auxpowHex   Serialized CAuxPow in hex.
     *  @return true if the block was accepted. */
    bool submitBlock(const uint256& hashBlock, const std::string& auxpowHex,
                     ChainstateManager& chainman);

    /** Get the currently cached block (for RPC result building). */
    std::shared_ptr<CBlock> getBlock(const uint256& hash);

private:
    std::mutex m_cs;
    std::unordered_map<uint256, std::shared_ptr<CBlock>, SaltedUint256Hasher> m_templates;
};

} // namespace auxpow_miner

#endif // BITCOIN_RPC_AUXPOW_MINER_H
