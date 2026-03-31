// Copyright (c) 2025 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mempool_asset.h>

#include <assets/assets.h>
#include <assets/assettypes.h>
#include <coins.h>
#include <consensus/validation.h>
#include <key_io.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <txmempool.h>

#include <map>
#include <set>
#include <string>
#include <utility>

using TagKey = std::pair<std::string, std::string>;

namespace {

struct AssetPolicyScratch {
    std::set<std::string> staged_new_assets;
    std::set<std::string> staged_global_freeze;
    std::set<std::string> staged_global_unfreeze;
    std::set<TagKey> staged_add_tag;
    std::set<TagKey> staged_remove_tag;
};

static void EraseTxidFromSetMap(std::map<std::string, std::set<Txid>>& m, const std::string& key, const Txid& txid)
{
    auto it = m.find(key);
    if (it == m.end()) return;
    it->second.erase(txid);
    if (it->second.empty()) m.erase(it);
}

static void EraseTxidFromPairSetMap(std::map<TagKey, std::set<Txid>>& m, const TagKey& key, const Txid& txid)
{
    auto it = m.find(key);
    if (it == m.end()) return;
    it->second.erase(txid);
    if (it->second.empty()) m.erase(it);
}

static void EraseFromHashIndex(std::map<Txid, std::set<std::string>>& hash_map,
                               std::map<std::string, std::set<Txid>>& value_map,
                               const Txid& txid)
{
    auto hit = hash_map.find(txid);
    if (hit == hash_map.end()) return;
    for (const std::string& item : hit->second) {
        EraseTxidFromSetMap(value_map, item, txid);
    }
    hash_map.erase(hit);
}

static void EraseFromHashPairIndex(std::map<Txid, std::set<TagKey>>& hash_map,
                                   std::map<TagKey, std::set<Txid>>& value_map,
                                   const Txid& txid)
{
    auto hit = hash_map.find(txid);
    if (hit == hash_map.end()) return;
    for (const TagKey& item : hit->second) {
        EraseTxidFromPairSetMap(value_map, item, txid);
    }
    hash_map.erase(hit);
}

/** Returns false if state was set invalid. */
static bool CheckAndStageVouts(CTxMemPool& pool, const CTransaction& tx, AssetPolicyScratch& scratch,
                               TxValidationState& state)
{
    if (!AreAssetsDeployed()) return true;

    for (const auto& out : tx.vout) {
        CAssetOutputEntry data;
        if (GetAssetData(out.scriptPubKey, data)) {
            if (data.type == TX_NEW_ASSET && !IsAssetNameAnOwner(data.assetName)) {
                if (pool.mapAssetToHash.count(data.assetName) || scratch.staged_new_assets.count(data.assetName)) {
                    return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY, "bad-txns-asset-mempool-duplicate", "");
                }
                scratch.staged_new_assets.insert(data.assetName);
            }

            continue;
        }

        if (out.scriptPubKey.IsNullGlobalRestrictionAssetTxDataScript()) {
            if (!AreRestrictedAssetsDeployed()) continue;
            CNullAssetTxData globalNullData;
            if (!GlobalAssetNullDataFromScript(out.scriptPubKey, globalNullData)) continue;
            if (globalNullData.flag == 1) {
                if (pool.mapGlobalFreezingAssetTransactions.count(globalNullData.asset_name) ||
                    scratch.staged_global_freeze.count(globalNullData.asset_name)) {
                    return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY,
                                         "bad-txns-global-freeze-already-in-mempool", "");
                }
                scratch.staged_global_freeze.insert(globalNullData.asset_name);
            } else if (globalNullData.flag == 0) {
                if (pool.mapGlobalUnFreezingAssetTransactions.count(globalNullData.asset_name) ||
                    scratch.staged_global_unfreeze.count(globalNullData.asset_name)) {
                    return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY,
                                         "bad-txns-global-unfreeze-already-in-mempool", "");
                }
                scratch.staged_global_unfreeze.insert(globalNullData.asset_name);
            }
            continue;
        }

        if (out.scriptPubKey.IsNullAssetTxDataScript()) {
            if (!AreRestrictedAssetsDeployed()) continue;
            CNullAssetTxData addressNullData;
            std::string address;
            if (!AssetNullDataFromScript(out.scriptPubKey, addressNullData, address)) continue;
            if (!IsAssetNameAQualifier(addressNullData.asset_name)) continue;

            const TagKey tag_key{address, addressNullData.asset_name};
            if (addressNullData.flag == static_cast<int8_t>(QualifierType::ADD_QUALIFIER)) {
                if (pool.mapAddressAddedTag.count(tag_key) || scratch.staged_add_tag.count(tag_key)) {
                    return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY,
                                         "bad-txns-adding-tag-already-in-mempool", "");
                }
                scratch.staged_add_tag.insert(tag_key);
            } else {
                if (pool.mapAddressRemoveTag.count(tag_key) || scratch.staged_remove_tag.count(tag_key)) {
                    return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY,
                                         "bad-txns-remove-tag-already-in-mempool", "");
                }
                scratch.staged_remove_tag.insert(tag_key);
            }
        }
    }

    return true;
}

static void RegisterVouts(CTxMemPool& pool, const CTransaction& tx)
{
    if (!AreAssetsDeployed()) return;

    const Txid txid = tx.GetHash();

    for (const auto& out : tx.vout) {
        CAssetOutputEntry data;
        if (GetAssetData(out.scriptPubKey, data)) {
            if (data.type == TX_NEW_ASSET && !IsAssetNameAnOwner(data.assetName)) {
                pool.mapAssetToHash[data.assetName] = txid;
            }

            if (AreRestrictedAssetsDeployed() && IsAssetNameAnRestricted(data.assetName)) {
                const std::string address = EncodeDestination(data.destination);
                pool.mapAddressesQualifiersChanged[address].insert(txid);
                pool.mapHashQualifiersChanged[txid].insert(address);

                pool.mapAssetVerifierChanged[data.assetName].insert(txid);
                pool.mapHashVerifierChanged[txid].insert(data.assetName);
            }
            continue;
        }

        if (out.scriptPubKey.IsNullGlobalRestrictionAssetTxDataScript()) {
            if (!AreRestrictedAssetsDeployed()) continue;
            CNullAssetTxData globalNullData;
            if (!GlobalAssetNullDataFromScript(out.scriptPubKey, globalNullData)) continue;
            if (globalNullData.flag == 1) {
                pool.mapGlobalFreezingAssetTransactions[globalNullData.asset_name].insert(txid);
                pool.mapHashGlobalFreezingAssetTransactions[txid].insert(globalNullData.asset_name);
            } else if (globalNullData.flag == 0) {
                pool.mapGlobalUnFreezingAssetTransactions[globalNullData.asset_name].insert(txid);
                pool.mapHashGlobalUnFreezingAssetTransactions[txid].insert(globalNullData.asset_name);
            }
            continue;
        }

        if (out.scriptPubKey.IsNullAssetTxDataScript()) {
            if (!AreRestrictedAssetsDeployed()) continue;
            CNullAssetTxData addressNullData;
            std::string address;
            if (!AssetNullDataFromScript(out.scriptPubKey, addressNullData, address)) continue;
            if (!IsAssetNameAQualifier(addressNullData.asset_name)) continue;

            const TagKey tag_key{address, addressNullData.asset_name};
            if (addressNullData.flag == static_cast<int8_t>(QualifierType::ADD_QUALIFIER)) {
                pool.mapAddressAddedTag[tag_key].insert(txid);
                pool.mapHashToAddressAddedTag[txid].insert(tag_key);
            } else {
                pool.mapAddressRemoveTag[tag_key].insert(txid);
                pool.mapHashToAddressRemoveTag[txid].insert(tag_key);
            }
        }
    }
}

static void RegisterVins(CTxMemPool& pool, const CTransaction& tx, const CCoinsViewCache& coins_view)
{
    if (!AreRestrictedAssetsDeployed()) return;

    const Txid txid = tx.GetHash();

    for (const auto& txin : tx.vin) {
        const Coin& coin = coins_view.AccessCoin(txin.prevout);
        if (coin.IsSpent() || !coin.out.scriptPubKey.IsAssetScript()) continue;

        CAssetOutputEntry data;
        if (!GetAssetData(coin.out.scriptPubKey, data)) continue;
        if (!IsAssetNameAnRestricted(data.assetName)) continue;

        pool.mapAssetMarkedGlobalFrozen[data.assetName].insert(txid);
        pool.mapHashMarkedGlobalFrozen[txid].insert(data.assetName);

        const TagKey frozen_key{EncodeDestination(data.destination), data.assetName};
        pool.mapAddressesMarkedFrozen[frozen_key].insert(txid);
        pool.mapHashToAddressMarkedFrozen[txid].insert(frozen_key);
    }
}

} // namespace

bool CheckAssetMempoolPolicy(CTxMemPool& pool, const CTransaction& tx, TxValidationState& state)
{
    AssetPolicyScratch scratch;
    return CheckAndStageVouts(pool, tx, scratch, state);
}

bool CheckAssetMempoolPolicyPackage(CTxMemPool& pool, const std::vector<CTransactionRef>& txns,
                                    TxValidationState& state, std::size_t& failed_index)
{
    AssetPolicyScratch scratch;
    for (std::size_t i = 0; i < txns.size(); ++i) {
        if (!CheckAndStageVouts(pool, *txns[i], scratch, state)) {
            failed_index = i;
            return false;
        }
    }
    return true;
}

void RegisterAssetMempoolTxOutputs(CTxMemPool& pool, const CTransaction& tx)
{
    RegisterVouts(pool, tx);
}

void RegisterAssetMempoolTxInputs(CTxMemPool& pool, const CTransaction& tx, const CCoinsViewCache& coins_view)
{
    RegisterVins(pool, tx, coins_view);
}

void UnregisterAssetMempoolTx(CTxMemPool& pool, const CTransaction& tx)
{
    if (!AreAssetsDeployed()) return;

    const Txid txid = tx.GetHash();

    for (const auto& out : tx.vout) {
        CAssetOutputEntry data;
        if (GetAssetData(out.scriptPubKey, data)) {
            if (data.type == TX_NEW_ASSET && !IsAssetNameAnOwner(data.assetName)) {
                auto it = pool.mapAssetToHash.find(data.assetName);
                if (it != pool.mapAssetToHash.end() && it->second == txid) {
                    pool.mapAssetToHash.erase(it);
                }
            }
        }
    }

    if (AreRestrictedAssetsDeployed()) {
        EraseFromHashIndex(pool.mapHashQualifiersChanged, pool.mapAddressesQualifiersChanged, txid);
        EraseFromHashIndex(pool.mapHashVerifierChanged, pool.mapAssetVerifierChanged, txid);

        {
            auto it = pool.mapHashGlobalFreezingAssetTransactions.find(txid);
            if (it != pool.mapHashGlobalFreezingAssetTransactions.end()) {
                for (const std::string& asset_name : it->second) {
                    EraseTxidFromSetMap(pool.mapGlobalFreezingAssetTransactions, asset_name, txid);
                }
                pool.mapHashGlobalFreezingAssetTransactions.erase(it);
            }
        }
        {
            auto it = pool.mapHashGlobalUnFreezingAssetTransactions.find(txid);
            if (it != pool.mapHashGlobalUnFreezingAssetTransactions.end()) {
                for (const std::string& asset_name : it->second) {
                    EraseTxidFromSetMap(pool.mapGlobalUnFreezingAssetTransactions, asset_name, txid);
                }
                pool.mapHashGlobalUnFreezingAssetTransactions.erase(it);
            }
        }

        EraseFromHashPairIndex(pool.mapHashToAddressAddedTag, pool.mapAddressAddedTag, txid);
        EraseFromHashPairIndex(pool.mapHashToAddressRemoveTag, pool.mapAddressRemoveTag, txid);
        EraseFromHashPairIndex(pool.mapHashToAddressMarkedFrozen, pool.mapAddressesMarkedFrozen, txid);
        EraseFromHashIndex(pool.mapHashMarkedGlobalFrozen, pool.mapAssetMarkedGlobalFrozen, txid);
    }
}
