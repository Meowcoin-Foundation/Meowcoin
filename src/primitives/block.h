// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MEOWCOIN_PRIMITIVES_BLOCK_H
#define MEOWCOIN_PRIMITIVES_BLOCK_H

#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"
#include <string>

// AuxPow Algo - An impossible pow hash
const uint256 HIGH_HASH = uint256S("0x0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

const std::string DEFAULT_POW_TYPE = "meowpow";

const std::string POW_TYPE_NAMES[] = {
    "meowpow",
    "auxpow",
};

enum POW_TYPE {
    POW_TYPE_MEOWPOW,
    POW_TYPE_AUXPOW,
    NUM_BLOCK_TYPES
};

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */

extern uint32_t nKAWPOWActivationTime;
extern uint32_t nMEOWPOWActivationTime;

class BlockNetwork
{
public:
    BlockNetwork();
    bool fOnRegtest;
    bool fOnTestnet;
    void SetNetwork(const std::string& network);
};

extern BlockNetwork bNetwork;


class CBlockHeader
{
public:

    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    //KAAAWWWPOW+Meowpow data
    uint32_t nHeight;
    uint64_t nNonce64;
    uint256 mix_hash;

    CBlockHeader()
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
        if (nTime < nKAWPOWActivationTime) {
            READWRITE(nNonce);
        } else { //This should be more than adequte for Meowpow
            READWRITE(nHeight);
            READWRITE(nNonce64);
            READWRITE(mix_hash);
        }
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;

        nNonce64 = 0;
        nHeight = 0;
        mix_hash.SetNull();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;
    uint256 GetX16RHash() const;
    uint256 GetX16RV2Hash() const;

    //AuxPow Algo
    static uint256 AuxPowHashArbitrary(const char* data);

    uint256 GetHashFull(uint256& mix_hash) const;
    uint256 GetKAWPOWHeaderHash() const;
    uint256 GetMEOWPOWHeaderHash() const;
    std::string ToString() const;

    /// Use for testing algo switch
    uint256 TestTiger() const;
    uint256 TestSha512() const;
    uint256 TestGost512() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    // AuxPow Algo
    POW_TYPE GetPoWType() const {
        return (POW_TYPE)((nVersion >> 16) & 0xFF);
    }

    // AuxPow Algo
    std::string GetPoWTypeName() const {
        // if (nVersion >= 0x20000000)
        //     return POW_TYPE_NAMES[0];
        POW_TYPE pt = GetPoWType();
        if (pt >= NUM_BLOCK_TYPES)
            return "unrecognised";
        return POW_TYPE_NAMES[pt];
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked;


    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CBlockHeader*)this);
        READWRITE(vtx);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;

        // KAWPOW
        block.nHeight        = nHeight;
        block.nNonce64       = nNonce64;
        block.mix_hash       = mix_hash;
        return block;
    }

    // void SetPrevBlockHash(uint256 prevHash) 
    // {
    //     block.hashPrevBlock = prevHash;
    // }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

/**
 * Custom serializer for CBlockHeader that omits the nNonce and mixHash, for use
 * as input to ProgPow.
 */
class CKAWPOWInput : private CBlockHeader
{
public:
    CKAWPOWInput(const CBlockHeader &header)
    {
        CBlockHeader::SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nHeight);
    }
};

//MEOWPOW
class CMEOWPOWInput : private CBlockHeader
{
public:
    CMEOWPOWInput(const CBlockHeader &header)
    {
        CBlockHeader::SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nHeight);
    }
};

#endif // MEOWCOIN_PRIMITIVES_BLOCK_H
