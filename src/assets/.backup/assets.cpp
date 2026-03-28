// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assets/assets.h>

// Global maps (empty until asset subsystem is fully wired).
std::map<uint256, std::string> mapReissuedTx;
std::map<std::string, uint256> mapReissuedAssets;

// Singleton assets cache (stub).
static CAssetsCache g_assetsCache;

CAssetsCache* GetCurrentAssetCache()
{
    return &g_assetsCache;
}
