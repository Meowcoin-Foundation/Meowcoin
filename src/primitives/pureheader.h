// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Meowcoin developers
// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_PUREHEADER_H
#define BITCOIN_PRIMITIVES_PUREHEADER_H

#include <primitives/algos.h>
#include <serialize.h>
#include <uint256.h>

#include <cstdint>
#include <string>

/**
 * Encapsulate a block version.  This takes care of building it up
 * from a base version, the modifier flags (like auxpow) and
 * also the auxpow chain ID.
 */
class CBlockVersion
{
public:
    static const int32_t CHAINID = 9; // Meowcoin chain ID

private:
    static const int32_t VERSIONAUXPOW_TOP_MASK = (1 << 28) | (1 << 29) | (1 << 30);
    static const uint8_t VERSION_START_BIT = 16;
    static const int32_t VERSION_AUXPOW = (1 << 8);
    static const int32_t VERSION_CHAIN_START = (1 << 16);
    static const int32_t MASK_AUXPOW_CHAINID_SHIFTED = (0x001f << VERSION_START_BIT);
    static const int32_t VERSION_AUXPOW_CHAINID_SHIFTED = (CHAINID << VERSION_START_BIT);

    int32_t nVersion;

public:
    CBlockVersion() : nVersion{0} {}

    /** Allow implicit construction from int32_t for backward compatibility. */
    CBlockVersion(int32_t v) : nVersion{v} {} // NOLINT(google-explicit-constructor)

    SERIALIZE_METHODS(CBlockVersion, obj) { READWRITE(obj.nVersion); }

    /** Allow implicit conversion to int32_t for backward compatibility. */
    operator int32_t() const { return nVersion; } // NOLINT(google-explicit-constructor)

    /** Allow direct int32 assignment (sets raw version like SetGenesisVersion). */
    CBlockVersion& operator=(int32_t v) { nVersion = v; return *this; }

    /** Bitwise compound assignment operators for version-bit manipulation. */
    CBlockVersion& operator&=(int32_t mask) { nVersion &= mask; return *this; }
    CBlockVersion& operator|=(int32_t mask) { nVersion |= mask; return *this; }

    void SetNull() { nVersion = 0; }

    int32_t GetChainId() const
    {
        return (nVersion & MASK_AUXPOW_CHAINID_SHIFTED) >> VERSION_START_BIT;
    }

    PowAlgo GetAlgo() const
    {
        if (IsAuxpow())
            return PowAlgo::SCRYPT;
        return PowAlgo::MEOWPOW;
    }

    std::string GetAlgoName() const
    {
        if (GetAlgo() == PowAlgo::SCRYPT) return "scrypt";
        return "meowpow";
    }

    void SetChainId(int32_t chainId)
    {
        nVersion %= VERSION_CHAIN_START;
        nVersion |= chainId * VERSION_CHAIN_START;
    }

    int32_t GetBaseVersion() const
    {
        return (nVersion & ~VERSION_AUXPOW) & ~VERSION_AUXPOW_CHAINID_SHIFTED;
    }

    void SetBaseVersion(int32_t nBaseVersion, int32_t nChainId);

    int32_t GetFullVersion() const { return nVersion; }

    void SetGenesisVersion(int32_t nGenesisVersion) { nVersion = nGenesisVersion; }

    bool IsAuxpow() const { return nVersion & VERSION_AUXPOW; }

    void SetAuxpow(bool auxpow)
    {
        if (auxpow)
            nVersion |= VERSION_AUXPOW;
        else
            nVersion &= ~VERSION_AUXPOW;
    }

    bool IsLegacy() const
    {
        return nVersion <= 4 || nVersion == 805306368;
    }
};

/**
 * A block header without auxpow information.  This "intermediate step"
 * in constructing the full header is useful, because it breaks the cyclic
 * dependency between auxpow (referencing a parent block header) and
 * the block header (referencing an auxpow).  The parent block header
 * does not have auxpow itself, so it is a pure header.
 */
class CPureBlockHeader
{
public:
    CBlockVersion nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CPureBlockHeader()
    {
        SetNull();
    }

    SERIALIZE_METHODS(CPureBlockHeader, obj)
    {
        READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nNonce);
    }

    void SetNull()
    {
        nVersion.SetNull();
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    std::string ToString() const;
};

#endif // BITCOIN_PRIMITIVES_PUREHEADER_H
