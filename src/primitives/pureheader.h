// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_PUREHEADER_H
#define BITCOIN_PRIMITIVES_PUREHEADER_H

#include "serialize.h"
#include "uint256.h"
#include "pow.h"

/**
 * Encapsulate a block version.  This takes care of building it up
 * from a base version, the modifier flags (like auxpow) and
 * also the auxpow chain ID.
 */
/**
 * Encapsulate a block version.  This takes care of building it up
 * from a base version, the modifier flags (like auxpow) and
 * also the auxpow chain ID.
 */
class CBlockVersion
{
public:
    /** The chain ID for the auxpow chain.  This is used to identify
     * the auxpow chain in the version field.
     */
    static const int32_t CHAINID = 9; // Meowcoin chain ID
private:
    static const int32_t VERSIONAUXPOW_TOP_MASK = (1 << 28) + (1 << 29) + (1 << 30);
    static const uint8_t VERSION_START_BIT = 16;


    /* Modifiers to the version.  */
    static const int32_t VERSION_AUXPOW = (1 << 8);

    /** Bits above are reserved for the auxpow chain ID.  */
    static const int32_t VERSION_CHAIN_START = (1 << 16);

    // mask to get Chain ID from version field, chainid is 16 in SYS so 0x001f mask should be OK
    static const int32_t MASK_AUXPOW_CHAINID_SHIFTED = (0x001f << VERSION_START_BIT);

    // shifted Chain ID in version field
    static const int32_t VERSION_AUXPOW_CHAINID_SHIFTED = (CHAINID << VERSION_START_BIT);

    /** The version as integer.  Should not be accessed directly.  */
    int nVersion;

public:
    inline CBlockVersion()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
    }

    inline void SetNull()
    {
        nVersion = 0;
    }

    /**
     * Extract the chain ID.
     * @return The chain ID encoded in the version.
     */
    inline int32_t GetChainId() const
    {
        return (nVersion & MASK_AUXPOW_CHAINID_SHIFTED) >> VERSION_START_BIT;
    }

    inline PowAlgo GetAlgo() const
    {
        if (IsAuxpow())
            return PowAlgo::SCRYPT;
        return PowAlgo::MEOWPOW;
    }
    
    /**
     * Set the chain ID.  This is used for the test suite.
     * @param ch The chain ID to set.
     */
    inline void SetChainId(int32_t chainId)
    {
        nVersion %= VERSION_CHAIN_START;
        nVersion |= chainId * VERSION_CHAIN_START;
    }

    /**
     * Extract the base version (without modifiers and chain ID).
     * @return The base version./
     */
    inline int32_t GetBaseVersion() const
    {
        return (nVersion & ~VERSION_AUXPOW) & ~VERSION_AUXPOW_CHAINID_SHIFTED;
    }

    /**
     * Set the base version (apart from chain ID and auxpow flag) to
     * the one given.  This should only be called when auxpow is not yet
     * set, to initialise a block!
     * @param nBaseVersion The base version.
     */
    void SetBaseVersion(int32_t nBaseVersion, int32_t nChainId);

    /**
     * Extract the full version.  Used for RPC results and debug prints.
     * @return The full version.
     */
    inline int32_t GetFullVersion() const
    {
        return nVersion;
    }

    /**
     * Set the genesis block version.  This must be a literal write
     * through, to get the correct historic version.
     * @param nGenesisVersion The version to set.
     */
    inline void SetGenesisVersion(int32_t nGenesisVersion)
    {
        nVersion = nGenesisVersion;
    }

    /**
     * Check if the auxpow flag is set in the version.
     * @return True iff this block version is marked as auxpow.
     */
    inline bool IsAuxpow() const
    {
        return nVersion & VERSION_AUXPOW;
    }

    /**
     * Set the auxpow flag.  This is used for testing.
     * @param auxpow Whether to mark auxpow as true.
     */
    inline void SetAuxpow(bool auxpow)
    {
        if (auxpow)
            nVersion |= VERSION_AUXPOW;
        else
            nVersion &= ~VERSION_AUXPOW;
    }

    /**
     * Check whether this is a "legacy" block without chain ID.
     * @return True if it is.
     */
    inline bool IsLegacy() const
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
    // header
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

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
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
