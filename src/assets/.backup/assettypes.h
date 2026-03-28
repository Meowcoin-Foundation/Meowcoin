// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ASSETS_ASSETTYPES_H
#define BITCOIN_ASSETS_ASSETTYPES_H

#include <consensus/amount.h>
#include <serialize.h>
#include <uint256.h>

#include <cstdint>
#include <string>

/** The different types of assets supported by the protocol. */
enum class AssetType : int {
    ROOT = 0,
    SUB = 1,
    UNIQUE = 2,
    MSGCHANNEL = 3,
    QUALIFIER = 4,
    SUB_QUALIFIER = 5,
    RESTRICTED = 6,
    VOTE = 7,
    REISSUE = 8,
    OWNER = 9,
    NULL_ADD_QUALIFIER = 10,
    INVALID = 11,
};

/** Data structure for a newly-issued asset. */
class CNewAsset
{
public:
    std::string strName;
    CAmount nAmount{0};
    int8_t units{0};
    int8_t nReissuable{1};
    int8_t nHasIPFS{0};
    std::string strIPFSHash;

    CNewAsset() = default;
    CNewAsset(const std::string& name, CAmount amount, int8_t u, int8_t reissuable,
              int8_t hasIPFS, const std::string& ipfs)
        : strName(name), nAmount(amount), units(u), nReissuable(reissuable),
          nHasIPFS(hasIPFS), strIPFSHash(ipfs) {}

    CNewAsset(const std::string& name, CAmount amount)
        : strName(name), nAmount(amount) {}

    SERIALIZE_METHODS(CNewAsset, obj)
    {
        READWRITE(obj.strName, obj.nAmount, obj.units, obj.nReissuable, obj.nHasIPFS);
        if (obj.nHasIPFS == 1) {
            READWRITE(obj.strIPFSHash);
        }
    }

    bool IsNull() const { return strName.empty(); }
    void SetNull() { strName.clear(); nAmount = 0; units = 0; nReissuable = 1; nHasIPFS = 0; strIPFSHash.clear(); }

    std::string ToString() const
    {
        return strName + " : " + std::to_string(nAmount);
    }
};

/** Data structure for an asset transfer output. */
class CAssetTransfer
{
public:
    std::string strName;
    CAmount nAmount{0};
    std::string message;
    int64_t nExpireTime{0};

    CAssetTransfer() = default;
    CAssetTransfer(const std::string& name, CAmount amount, const std::string& msg = "",
                   int64_t expire = 0)
        : strName(name), nAmount(amount), message(msg), nExpireTime(expire) {}

    SERIALIZE_METHODS(CAssetTransfer, obj)
    {
        READWRITE(obj.strName, obj.nAmount);
        bool hasMessage = !obj.message.empty() || obj.nExpireTime != 0;
        READWRITE(hasMessage);
        if (hasMessage) {
            READWRITE(obj.message, obj.nExpireTime);
        }
    }

    bool IsNull() const { return strName.empty(); }
};

/** Data for reissuing an existing asset. */
class CReissueAsset
{
public:
    std::string strName;
    CAmount nAmount{0};
    int8_t nUnits{-1};
    int8_t nReissuable{-1};
    std::string strIPFSHash;

    CReissueAsset() = default;
    CReissueAsset(const std::string& name, CAmount amount, int8_t units = -1,
                  int8_t reissuable = -1, const std::string& ipfs = "")
        : strName(name), nAmount(amount), nUnits(units), nReissuable(reissuable),
          strIPFSHash(ipfs) {}

    SERIALIZE_METHODS(CReissueAsset, obj)
    {
        READWRITE(obj.strName, obj.nAmount, obj.nUnits, obj.nReissuable, obj.strIPFSHash);
    }

    bool IsNull() const { return strName.empty(); }
};

/** Asset data stored in the database (asset + block info). */
class CDatabasedAssetData
{
public:
    CNewAsset asset;
    int nHeight{-1};
    uint256 blockHash;

    CDatabasedAssetData() = default;
    CDatabasedAssetData(const CNewAsset& a, int height, const uint256& hash)
        : asset(a), nHeight(height), blockHash(hash) {}

    SERIALIZE_METHODS(CDatabasedAssetData, obj)
    {
        READWRITE(obj.asset, obj.nHeight, obj.blockHash);
    }
};

#endif // BITCOIN_ASSETS_ASSETTYPES_H
