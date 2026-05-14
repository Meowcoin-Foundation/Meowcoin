// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Meowcoin Core developers
// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <auxpow.h>
#include <primitives/pureheader.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <util/time.h>

#include <memory>

/** Global activation timestamps set from chainparams at init time. */
extern uint32_t nKAWPOWActivationTime;
extern uint32_t nMEOWPOWActivationTime;

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader : public CPureBlockHeader
{
public:
    // KAWPOW / MEOWPOW fields
    uint32_t nHeight;
    uint64_t nNonce64;
    uint256 mix_hash;

    // auxpow (if this is a merge-mined block)
    std::shared_ptr<CAuxPow> auxpow;

    CBlockHeader()
    {
        SetNull();
    }

    SERIALIZE_METHODS(CBlockHeader, obj)
    {
        // Always serialize the pure header fields.
        READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits);

        // Pre-KAWPOW or AuxPoW blocks use nNonce + optional auxpow.
        // KAWPOW/MEOWPOW blocks use nHeight/nNonce64/mix_hash.
        if (obj.nTime < nKAWPOWActivationTime || obj.nVersion.IsAuxpow()) {
            READWRITE(obj.nNonce);
            if (obj.nVersion.IsAuxpow()) {
                SER_READ(obj, obj.auxpow = std::make_shared<CAuxPow>());
                if (obj.auxpow) {
                    READWRITE(*obj.auxpow);
                }
            } else {
                SER_READ(obj, obj.auxpow.reset());
            }
        } else {
            READWRITE(obj.nHeight, obj.nNonce64, obj.mix_hash);
        }
    }

    void SetNull()
    {
        CPureBlockHeader::SetNull();
        nHeight = 0;
        nNonce64 = 0;
        mix_hash.SetNull();
        auxpow.reset();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;
    uint256 GetX16RHash() const;
    uint256 GetX16RV2Hash() const;

    uint256 GetHashFull(uint256& mix_hash) const;
    uint256 GetKAWPOWHeaderHash() const;
    uint256 GetMEOWPOWHeaderHash() const;

    NodeSeconds Time() const
    {
        return NodeSeconds{std::chrono::seconds{nTime}};
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    std::string ToString() const;

    /**
     * Set the block's auxpow (or unset it).  This takes care of updating
     * the version accordingly.
     * @param apow Pointer to the auxpow to use or nullptr.
     */
    void SetAuxpow(std::shared_ptr<CAuxPow> apow);
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // Memory-only flags for caching expensive checks
    mutable bool fChecked;                            // CheckBlock()
    mutable bool m_checked_witness_commitment{false}; // CheckWitnessCommitment()
    mutable bool m_checked_merkle_root{false};        // CheckMerkleRoot()

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    SERIALIZE_METHODS(CBlock, obj)
    {
        READWRITE(AsBase<CBlockHeader>(obj), obj.vtx);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
        m_checked_witness_commitment = false;
        m_checked_merkle_root = false;
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
        block.auxpow         = auxpow;
        block.nHeight        = nHeight;
        block.nNonce64       = nNonce64;
        block.mix_hash       = mix_hash;
        return block;
    }

    std::string ToString() const;
};

/**
 * Custom serializer for CBlockHeader that omits the nNonce64 and mixHash,
 * for use as input to KAWPOW ProgPow.
 */
class CKAWPOWInput : private CBlockHeader
{
public:
    CKAWPOWInput(const CBlockHeader& header)
    {
        CBlockHeader::SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    SERIALIZE_METHODS(CKAWPOWInput, obj)
    {
        READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nHeight);
    }
};

/**
 * Custom serializer for CBlockHeader that omits the nNonce64 and mixHash,
 * for use as input to MEOWPOW ProgPow.
 */
class CMEOWPOWInput : private CBlockHeader
{
public:
    CMEOWPOWInput(const CBlockHeader& header)
    {
        CBlockHeader::SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    SERIALIZE_METHODS(CMEOWPOWInput, obj)
    {
        READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nHeight);
    }
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    /** Historically CBlockLocator's version field has been written to network
     * streams as the negotiated protocol version and to disk streams as the
     * client version, but the value has never been used.
     *
     * Hard-code to the highest protocol version ever written to a network stream.
     * SerParams can be used if the field requires any meaning in the future,
     **/
    static constexpr int DUMMY_VERSION = 70016;

    std::vector<uint256> vHave;

    CBlockLocator() = default;

    explicit CBlockLocator(std::vector<uint256>&& have) : vHave(std::move(have)) {}

    SERIALIZE_METHODS(CBlockLocator, obj)
    {
        int nVersion = DUMMY_VERSION;
        READWRITE(nVersion);
        READWRITE(obj.vHave);
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

#endif // BITCOIN_PRIMITIVES_BLOCK_H
