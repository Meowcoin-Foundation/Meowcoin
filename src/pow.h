// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Meowcoin Core developers
// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>
#include <primitives/algos.h>

#include <cstdint>
#include <optional>

class CBlockHeader;
class CBlockIndex;
class uint256;
class arith_uint256;

/**
 * Convert nBits value to target.
 *
 * @param[in] nBits     compact representation of the target
 * @param[in] pow_limit PoW limit (consensus parameter)
 *
 * @return              the proof-of-work target or nullopt if the nBits value
 *                      is invalid (due to overflow or exceeding pow_limit)
 */
std::optional<arith_uint256> DeriveTarget(unsigned int nBits, const uint256 pow_limit);

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&, bool fIsAuxPow = false);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, PowAlgo algo, const Consensus::Params&);

/**
 * Convenience overload — uses the scalar params.powLimit (most permissive).
 * This matches the original Meowcoin Core CheckProofOfWork signature and is used
 * by callers that don't have algorithm context (blockstorage, tests, etc.).
 */
bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, const Consensus::Params&);

inline bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    return CheckProofOfWorkImpl(hash, nBits, params);
}

/**
 * Return false if the proof-of-work requirement specified by new_nbits at a
 * given height is not possible, given the proof-of-work on the prior block as
 * specified by old_nbits.
 *
 * Always returns true on networks where min difficulty blocks are allowed.
 */
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits);

/** Return true if nBlockNumber >= the DGW activation block. */
bool IsDGWActive(unsigned int nBlockNumber);

#endif // BITCOIN_POW_H
