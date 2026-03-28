// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Meowcoin developers
// Copyright (c) 2014-2016 Daniel Kraft
// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_AUXPOW_H
#define BITCOIN_AUXPOW_H

#include <consensus/params.h>
#include <primitives/pureheader.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

class CBlock;
class CBlockHeader;
class CBlockIndex;

/** Header for merge-mining data in the coinbase.  */
static const unsigned char pchMergedMiningHeader[] = { 0xfa, 0xbe, 'm', 'm' };

/**
 * A transaction with a merkle branch linking it to the block chain.
 * Moved from wallet.h because it is needed for auxpow.
 */
class CMerkleTx
{
public:
    CTransactionRef tx;
    uint256 hashBlock;
    std::vector<uint256> vMerkleBranch;
    int nIndex;

    CMerkleTx()
    {
        SetTx(MakeTransactionRef(CMutableTransaction{}));
        Init();
    }

    explicit CMerkleTx(CTransactionRef arg)
    {
        SetTx(std::move(arg));
        Init();
    }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    void SetTx(CTransactionRef arg)
    {
        tx = std::move(arg);
    }

    SERIALIZE_METHODS(CMerkleTx, obj)
    {
        READWRITE(TX_WITH_WITNESS(obj.tx), obj.hashBlock, obj.vMerkleBranch, obj.nIndex);
    }

    uint256 GetHash() const { return tx->GetHash().ToUint256(); }
    bool IsCoinBase() const { return tx->IsCoinBase(); }
};

/**
 * Data for the merge-mining auxpow.  This is a merkle tx (the parent block's
 * coinbase tx) that can be verified to be in the parent block, and this
 * transaction's input (the coinbase script) contains the reference
 * to the actual merge-mined block.
 */
class CAuxPow : public CMerkleTx
{
public:
    /** The merkle branch connecting the aux block to our coinbase.  */
    std::vector<uint256> vChainMerkleBranch;

    /** Merkle tree index of the aux block header in the coinbase.  */
    int nChainIndex;

    /** Parent block header (on which the real PoW is done).  */
    CPureBlockHeader parentBlock;

    explicit CAuxPow(CTransactionRef txIn)
        : CMerkleTx(std::move(txIn)), nChainIndex{0}
    {}

    CAuxPow()
        : CMerkleTx(), nChainIndex{0}
    {}

    SERIALIZE_METHODS(CAuxPow, obj)
    {
        READWRITE(AsBase<CMerkleTx>(obj), obj.vChainMerkleBranch, obj.nChainIndex, obj.parentBlock);
    }

    /**
     * Check the auxpow, given the merge-mined block's hash and our chain ID.
     * Note that this does not verify the actual PoW on the parent block!  It
     * just confirms that all the merkle branches are valid.
     * @param hashAuxBlock Hash of the merge-mined block.
     * @param nChainId The auxpow chain ID of the block to check.
     * @param params Consensus parameters.
     * @return True if the auxpow is valid.
     */
    bool check(const uint256& hashAuxBlock, int nChainId,
               const Consensus::Params& params) const;

    /**
     * Get the parent block's hash.
     * @return The parent block hash.
     */
    uint256 getParentBlockHash() const
    {
        return parentBlock.GetHash();
    }

    /**
     * Calculate the expected index in the merkle tree.
     * @param nNonce The coinbase's nonce value.
     * @param nChainId The chain ID.
     * @param h The merkle block height.
     * @return The expected index for the aux hash.
     */
    static int getExpectedIndex(uint32_t nNonce, int nChainId, unsigned h);

    /**
     * Check a merkle branch.  This used to be in CBlock, but was removed upstream.
     */
    static uint256 CheckMerkleBranch(uint256 hash,
                                     const std::vector<uint256>& vMerkleBranch,
                                     int nIndex);

    /**
     * Initialise the auxpow of the given block header.
     * @param header The header to set the auxpow on.
     */
    static void initAuxPow(CBlockHeader& header);

    std::string ToString() const
    {
        std::stringstream s;
        s << "CAuxPow(ver=" << parentBlock.nVersion.GetFullVersion()
          << ", parentHash=" << parentBlock.GetHash().ToString()
          << ", nChainIndex=" << nChainIndex << ")";
        return s.str();
    }
};

#endif // BITCOIN_AUXPOW_H
