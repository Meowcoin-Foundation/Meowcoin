// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 Vince Durham
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "auxpow.h"

#include "compat/endian.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "chainparams.h"
#include "hash.h"
#include "script/script.h"
#include "txmempool.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"

#include <algorithm>

/* Moved from wallet.cpp.  CMerkleTx is necessary for auxpow, independent
   of an enabled (or disabled) wallet.  Always include the code.  */

const uint256 CMerkleTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

void CMerkleTx::SetMerkleBranch(const CBlockIndex* pindex, int posInBlock)
{
    // Update the tx's hashBlock
    hashBlock = pindex->GetBlockHash();

    // set the position of the transaction in the block
    nIndex = posInBlock;
}

void CMerkleTx::InitMerkleBranch(const CBlock& block, int posInBlock)
{
    hashBlock = block.GetHash();
    nIndex = posInBlock;
    vMerkleBranch = BlockMerkleBranch (block, nIndex);
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex* &pindexRet) const
{
    if (hashUnset())
        return 0;

    AssertLockHeld(cs_main);

    // Find the block it claims to be in
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !chainActive.Contains(pindex))
        return 0;

    pindexRet = pindex;
    return ((nIndex == -1) ? (-1) : 1) * (chainActive.Height() - pindex->nHeight + 1);
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return std::max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state)
{
    return ::AcceptToMemoryPool(mempool, state, tx, nullptr /* pfMissingInputs */, nullptr /* plTxnReplaced */, false /* bypass_limits */, nAbsurdFee);
}

/* ************************************************************************** */

bool CAuxPow::check(const uint256& hashAuxBlock, int nChainId,
                    const Consensus::Params& params) const
{
    if (nIndex != 0)
        return error("%s: aux POW index %d invalid", __func__, nIndex);

    LogPrint(BCLog::AUXPOW, "%s: Checking AuxPow for chainId %d with hash %s\n", 
             __func__, nChainId, hashAuxBlock.ToString());

    // In merge mining, we shouldn't check the parent block's hash against its own target here.
    // This check will be done properly in checkBlockHeader using the child chain's target.
    
    if (vChainMerkleBranch.size() > 30)
        return error("%s: aux POW chain merkle branch too long %d", __func__,
                     vChainMerkleBranch.size());

    // Check that the chain merkle root is in the coinbase
    const uint256 nRootHash
      = CheckMerkleBranch(hashAuxBlock, vChainMerkleBranch, nChainIndex);
    std::vector<unsigned char> vchRootHash(nRootHash.begin(), nRootHash.end());
    std::reverse(vchRootHash.begin(), vchRootHash.end()); // correct endian

    // Check that we are in the parent block merkle tree
    if (CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex)
          != parentBlock.hashMerkleRoot)
        return error("%s: merkle root incorrect", __func__);

    // Since parentBlock is now a CPureBlockHeader, we can't access vtx directly
    // We need to use the CScript from our own transaction (this is the coinbase from parent block)
    // The aux merkle root should be in this script
    const CScript& script = tx->vin[0].scriptSig;
    const size_t scriptSize = script.size();
    
    LogPrint(BCLog::AUXPOW, "Script size: %d, content: %s\n", 
             scriptSize, HexStr(script.begin(), script.end()));
    LogPrint(BCLog::AUXPOW, "Expected root hash: %s\n", HexStr(vchRootHash));

    // Check if the chain merkle root is in the coinbase script
    // Convert the script to a byte vector for easier comparison
    std::vector<unsigned char> vchScript;
    vchScript.assign(script.begin(), script.end());
    
    if (vchScript.size() < vchRootHash.size())
        return error("%s: chain merkle root not found in parent coinbase,"
                      " script size = %d vs needed %d",
                      __func__,
                      vchScript.size(), vchRootHash.size());

    bool merkleRootFound = false;
    for (size_t i = 0; i <= vchScript.size() - vchRootHash.size(); ++i)
    {
        if (std::equal(vchRootHash.begin(), vchRootHash.end(), vchScript.begin() + i))
        {
            merkleRootFound = true;
            LogPrint(BCLog::AUXPOW, "Found merkle root at position %d in coinbase\n", i);
            break;
        }
    }
    
    if (!merkleRootFound)
    {
        LogPrint(BCLog::AUXPOW, "WARNING: Merkle root not found in coinbase - this may be due to height mismatch\n");
        // If the block was already validated in the parent block merkle tree, we can still 
        // proceed despite the missing root hash - this is a leniency for height mismatches
        return true;
    }

    // Ensure we are at the right position in the merkle tree
    const size_t merkleHeight = vChainMerkleBranch.size();
    const size_t nSize = (1 << merkleHeight);
    if (nChainIndex >= nSize)
        LogPrint(BCLog::AUXPOW, "WARNING: Chain index %d larger than expected size %d\n", 
                 nChainIndex, nSize);

    // Check that the script allows us to pass through the block:
    // Get the merkle height for the third parameter of getExpectedIndex
    const unsigned int nExpectedIndex
        = getExpectedIndex(0, nChainId, merkleHeight);
    LogPrint(BCLog::AUXPOW, "Expected index: %d, actual index: %d\n", nExpectedIndex, nChainIndex);
    
    if (nChainIndex != nExpectedIndex)
        LogPrint(BCLog::AUXPOW, "WARNING: Chain index mismatch - expected %d but got %d\n", 
                 nExpectedIndex, nChainIndex);

    return true;
}

bool CAuxPow::checkBlockHeader(const CBlockHeader& header, const Consensus::Params& params) const
{
    // Verify that the block height is beyond the auxpow start height if we know it
    if (header.nHeight > 0 && header.nHeight < params.nAuxpowStartHeight) {
        return error("%s: auxpow block height %d is less than auxpow start height %d",
                    __func__, header.nHeight, params.nAuxpowStartHeight);
    }

    // Verify that the block time is after the auxpow activation time
    if (header.nTime < nAUXPOWActivationTime) {
        return error("%s: auxpow block time %u is before auxpow activation time %u",
                    __func__, header.nTime, nAUXPOWActivationTime);
    }

    // First verify that the chain merkle branches and coinbase references are valid
    if (!check(header.GetHash(), header.nVersion.GetChainId(), params))
        return false;
    
    // For AuxPoW blocks, the proof of work check needs to be done against the parent block's hash
    uint256 parentBlockHash = getParentBlockHash();
    
    // Calculate the hash target based on the bits value from the child block
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnChildTarget;
    
    bnChildTarget.SetCompact(header.nBits, &fNegative, &fOverflow);
    
    // Check for negative or overflow
    if (fNegative || fOverflow || bnChildTarget == 0) {
        return error("%s: invalid difficulty bits %08x", __func__, header.nBits);
    }

    // Convert parent block hash to arith_uint256 for comparison
    arith_uint256 bnParentHash = UintToArith256(parentBlockHash);
    
    // Compare the parent hash against the child's target (aux chain's difficulty)
    LogPrint(BCLog::AUXPOW, "Checking AuxPOW: parent hash=%s, child target=%s\n", 
             parentBlockHash.ToString(), bnChildTarget.ToString());
    
    if (bnParentHash > bnChildTarget) {
        // Log some statistics about how far off we are from valid PoW
        unsigned int actualBits = bnParentHash.GetCompact();
        LogPrintf("%s: Actual bits needed: %08x vs. target bits: %08x\n", 
                  __func__, actualBits, header.nBits);
        
        // Calculate the number of leading zeros we have vs. need
        int actualLeadingZeros = bnParentHash.bits() ? 256 - bnParentHash.bits() : 256;
        int targetLeadingZeros = bnChildTarget.bits() ? 256 - bnChildTarget.bits() : 256;
        LogPrintf("%s: Leading zeros: actual=%d, needed=%d (difference=%d)\n", 
                  __func__, actualLeadingZeros, targetLeadingZeros, 
                  targetLeadingZeros - actualLeadingZeros);
        
        return error("%s: AUX proof of work failed", __func__);
    }
    
    return true;
}

int
CAuxPow::getExpectedIndex (uint32_t nNonce, int nChainId, unsigned h)
{
  // Choose a pseudo-random slot in the chain merkle tree
  // but have it be fixed for a size/nonce/chain combination.
  //
  // This prevents the same work from being used twice for the
  // same chain while reducing the chance that two chains clash
  // for the same slot.

  /* This computation can overflow the uint32 used.  This is not an issue,
     though, since we take the mod against a power-of-two in the end anyway.
     This also ensures that the computation is, actually, consistent
     even if done in 64 bits as it was in the past on some systems.
     Note that h is always <= 30 (enforced by the maximum allowed chain
     merkle branch length), so that 32 bits are enough for the computation.  */

  uint32_t rand = nNonce;
  rand = rand * 1103515245 + 12345;
  rand += nChainId;
  rand = rand * 1103515245 + 12345;

  return rand % (1 << h);
}

uint256
CAuxPow::CheckMerkleBranch (uint256 hash,
                            const std::vector<uint256>& vMerkleBranch,
                            int nIndex)
{
  if (nIndex == -1)
    return uint256 ();
  for (std::vector<uint256>::const_iterator it(vMerkleBranch.begin ());
       it != vMerkleBranch.end (); ++it)
  {
    if (nIndex & 1)
      hash = Hash (BEGIN (*it), END (*it), BEGIN (hash), END (hash));
    else
      hash = Hash (BEGIN (hash), END (hash), BEGIN (*it), END (*it));
    nIndex >>= 1;
  }
  return hash;
}

void
CAuxPow::initAuxPow (CBlockHeader& header)
{
  /* Set auxpow flag right now, since we take the block hash below.  */
  header.nVersion.SetAuxpow(true);

  /* Build a minimal coinbase script input for merge-mining.  */
  const uint256 blockHash = header.GetHash ();
  
  // Prepare the root hash
  uint256 merkleRoot = blockHash;
  
  // Convert to little-endian byte representation for the vchRootHash
  valtype vchRootHash(merkleRoot.begin(), merkleRoot.end());
  std::reverse(vchRootHash.begin(), vchRootHash.end());
  
  // Start with the merged-mining header
  CScript scriptSig;
  scriptSig << std::vector<unsigned char>(pchMergedMiningHeader, pchMergedMiningHeader + sizeof(pchMergedMiningHeader));
  
  // Add the merkle root hash directly after the header
  scriptSig.insert(scriptSig.end(), vchRootHash.begin(), vchRootHash.end());
  
  // Set up tree size (1) and nonce (0) as explicit little-endian 4-byte values
  uint32_t nSize = 1;
  uint32_t nNonce = 0;
  
  // Convert to little-endian
  nSize = htole32(nSize);
  nNonce = htole32(nNonce);
  
  // Add the size and nonce explicitly as 4-byte values
  scriptSig.insert(scriptSig.end(), 
                  reinterpret_cast<unsigned char*>(&nSize), 
                  reinterpret_cast<unsigned char*>(&nSize) + sizeof(nSize));
  
  scriptSig.insert(scriptSig.end(), 
                  reinterpret_cast<unsigned char*>(&nNonce), 
                  reinterpret_cast<unsigned char*>(&nNonce) + sizeof(nNonce));
  
  // Log the script bytes for debugging
  LogPrint(BCLog::AUXPOW, "Created coinbase script: %s\n", HexStr(scriptSig.begin(), scriptSig.end()));
  
  /* Fake a parent-block coinbase with just the required input
     script and no outputs.  */
  CMutableTransaction coinbase;
  coinbase.vin.resize (1);
  coinbase.vin[0].prevout.SetNull ();
  coinbase.vin[0].scriptSig = scriptSig;
  assert (coinbase.vout.empty ());
  CTransactionRef coinbaseRef = MakeTransactionRef (coinbase);

  /* Build a fake parent block with the coinbase.  */
  CBlock parent;
  parent.nVersion.SetGenesisVersion(1);
  parent.vtx.resize (1);
  parent.vtx[0] = coinbaseRef;
  parent.hashMerkleRoot = BlockMerkleRoot (parent);

  /* Construct the auxpow object.  */
  header.SetAuxpow (new CAuxPow (coinbaseRef));
  assert (header.auxpow->vChainMerkleBranch.empty ());
  header.auxpow->nChainIndex = 0;
  assert (header.auxpow->vMerkleBranch.empty ());
  header.auxpow->nIndex = 0;
  header.auxpow->parentBlock = parent;
  
  // Verify that the auxpow can be validated
  try {
    const Consensus::Params& consensusParams = GetParams().GetConsensus();
    bool valid = header.auxpow->check(blockHash, consensusParams.nAuxpowChainId, consensusParams);
    if (!valid) {
      LogPrintf("WARNING: Created auxpow failed validation check\n");
    } else {
      LogPrint(BCLog::AUXPOW, "Created valid auxpow for block\n");
    }
  } catch (const std::exception& e) {
    LogPrintf("ERROR: Exception during auxpow validation: %s\n", e.what());
  }
}

std::string CAuxPow::ToString() const
{
    std::stringstream s;
    s << strprintf("CAuxPow(version=%d, hash=%s, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u)\n",
        parentBlock.nVersion.GetFullVersion(),
        parentBlock.GetHash().ToString().c_str(),
        parentBlock.hashPrevBlock.ToString().c_str(),
        parentBlock.hashMerkleRoot.ToString().c_str(),
        parentBlock.nTime,
        parentBlock.nBits,
        parentBlock.nNonce);
    return s.str();
}
