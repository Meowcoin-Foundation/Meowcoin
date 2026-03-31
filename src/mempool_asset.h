// Copyright (c) 2025 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MEOWCOIN_MEMPOOL_ASSET_H
#define MEOWCOIN_MEMPOOL_ASSET_H

#include <cstddef>
#include <vector>

#include <primitives/transaction.h>
#include <sync.h>

class CCoinsViewCache;
class CTransaction;
class CTxMemPool;
class TxValidationState;

/** Policy-only checks (no map mutation). Caller must hold pool.cs. */
bool CheckAssetMempoolPolicy(CTxMemPool& pool, const CTransaction& tx, TxValidationState& state)
    EXCLUSIVE_LOCKS_REQUIRED(pool.cs);

/**
 * Same as CheckAssetMempoolPolicy but treats txns as one atomic package (staging duplicates
 * across transactions in order). On failure, failed_index is the index into txns.
 */
bool CheckAssetMempoolPolicyPackage(CTxMemPool& pool, const std::vector<CTransactionRef>& txns,
                                    TxValidationState& state, std::size_t& failed_index)
    EXCLUSIVE_LOCKS_REQUIRED(pool.cs);

/** Register output-side asset mempool indexes (called from CTxMemPool::addNewTransaction). */
void RegisterAssetMempoolTxOutputs(CTxMemPool& pool, const CTransaction& tx)
    EXCLUSIVE_LOCKS_REQUIRED(pool.cs);

/**
 * Register input-side restricted-asset indexes (vin spends restricted asset outputs).
 * Call after the transaction is in the mempool so coin lookups match submission order.
 */
void RegisterAssetMempoolTxInputs(CTxMemPool& pool, const CTransaction& tx, const CCoinsViewCache& coins_view)
    EXCLUSIVE_LOCKS_REQUIRED(pool.cs);

/** Remove all asset-related mempool indexes for this transaction (called from removeUnchecked). */
void UnregisterAssetMempoolTx(CTxMemPool& pool, const CTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(pool.cs);

#endif // MEOWCOIN_MEMPOOL_ASSET_H
