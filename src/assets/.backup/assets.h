// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ASSETS_ASSETS_H
#define BITCOIN_ASSETS_ASSETS_H

/**
 * Meowcoin asset layer — type definitions and feature gates.
 *
 * The full implementation (cache, database, validation integration) from the
 * original Meowcoin source is preserved as reference in the sibling *db.cpp/h
 * files.  This header provides the types that the rest of the codebase needs
 * (P2P, RPC, script) and stub feature gates that return false/empty to keep
 * the node functional without the asset subsystem fully wired.
 */

#include <assets/assettypes.h>

#include <consensus/amount.h>
#include <script/script.h>
#include <uint256.h>

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define MEWC_R 114
#define MEWC_V 118
#define MEWC_N 110
#define MEWC_Q 113
#define MEWC_T 116
#define MEWC_O 111

#define DEFAULT_UNITS 0
#define DEFAULT_REISSUABLE 1
#define DEFAULT_HAS_IPFS 0
#define DEFAULT_IPFS ""
#define MIN_ASSET_LENGTH 3
#define MAX_ASSET_LENGTH 32
#define OWNER_TAG "!"
#define OWNER_LENGTH 1
#define OWNER_UNITS 0
#define OWNER_ASSET_AMOUNT 1 * COIN
#define UNIQUE_ASSET_AMOUNT 1 * COIN
#define UNIQUE_ASSET_UNITS 0
#define UNIQUE_ASSETS_REISSUABLE 0

#define RESTRICTED_CHAR '$'
#define QUALIFIER_CHAR '#'

#define QUALIFIER_ASSET_MIN_AMOUNT 1 * COIN
#define QUALIFIER_ASSET_MAX_AMOUNT 10 * COIN
#define QUALIFIER_ASSET_UNITS 0

#define MAX_CACHE_ASSETS_SIZE 2500

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

class CScript;
class CTransaction;
class CTxOut;
class CBlockIndex;

// ---------------------------------------------------------------------------
// Global maps (empty until asset subsystem is wired)
// ---------------------------------------------------------------------------

extern std::map<uint256, std::string> mapReissuedTx;
extern std::map<std::string, uint256> mapReissuedAssets;

// ---------------------------------------------------------------------------
// CAssetsCache — will hold the in-memory asset state; stub for now
// ---------------------------------------------------------------------------

class CAssetsCache
{
public:
    CAssetsCache() = default;

    /** Query whether an asset exists by name. */
    bool CheckIfAssetExists(const std::string& name, bool fMemoryOnly = false) const
    {
        (void)name; (void)fMemoryOnly;
        return false;
    }

    /** Try to retrieve asset metadata. */
    bool GetAssetMetaDataIfExists(const std::string& name, CNewAsset& asset) const
    {
        (void)name; (void)asset;
        return false;
    }

    bool GetAssetMetaDataIfExists(const std::string& name, CNewAsset& asset,
                                  int& nHeight, uint256& blockHash) const
    {
        (void)name; (void)asset; (void)nHeight; (void)blockHash;
        return false;
    }
};

// ---------------------------------------------------------------------------
// Global accessor
// ---------------------------------------------------------------------------

CAssetsCache* GetCurrentAssetCache();

// ---------------------------------------------------------------------------
// Feature-gate functions — all return false / disabled
// ---------------------------------------------------------------------------

/** Are assets active at this block height / time? */
inline bool AreAssetsDeployed() { return false; }
inline bool AreRestrictedAssetsDeployed() { return false; }
inline bool AreEnforcedValuesDeployed() { return false; }
inline bool AreMessagesDeployed() { return false; }

/** Burn amounts for different asset operations. */
inline CAmount GetIssueAssetBurnAmount()            { return 500 * COIN; }
inline CAmount GetReissueAssetBurnAmount()           { return 100 * COIN; }
inline CAmount GetIssueSubAssetBurnAmount()          { return 100 * COIN; }
inline CAmount GetIssueUniqueAssetBurnAmount()       { return 5 * COIN; }
inline CAmount GetIssueMsgChannelAssetBurnAmount()   { return 100 * COIN; }
inline CAmount GetIssueQualifierAssetBurnAmount()    { return 1000 * COIN; }
inline CAmount GetIssueSubQualifierAssetBurnAmount() { return 100 * COIN; }
inline CAmount GetIssueRestrictedAssetBurnAmount()   { return 1500 * COIN; }
inline CAmount GetAddNullQualifierTagBurnAmount()    { return static_cast<CAmount>(0.1 * COIN); }

/** Validate an asset name. */
inline bool IsAssetNameValid(const std::string& name)
{
    return !name.empty() && name.size() <= MAX_ASSET_LENGTH;
}

inline bool IsAssetNameValid(const std::string& name, AssetType& type, std::string& error)
{
    (void)error;
    type = AssetType::INVALID;
    if (name.empty() || name.size() > MAX_ASSET_LENGTH) return false;
    type = AssetType::ROOT;
    return true;
}

inline bool IsAssetNameAnOwner(const std::string& name)
{
    return !name.empty() && name.back() == '!';
}

#endif // BITCOIN_ASSETS_ASSETS_H
