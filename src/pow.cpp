// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"
#include "validation.h"
#include "chainparams.h"
#include "tinyformat.h"

unsigned int static DarkGravityWave(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {
    /* current difficulty formula, dash - DarkGravity v3, written by Evan Duffield - evan@dash.org */
    assert(pindexLast != nullptr);

    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(PowAlgo::MEOWPOW)]).GetCompact();
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(PowAlgo::MEOWPOW)]);
    int64_t nPastBlocks = 180; // ~3hr

    // make sure we have at least (nPastBlocks + 1) blocks, otherwise just return powLimit
    if (!pindexLast || pindexLast->nHeight < nPastBlocks) {
        return bnPowLimit.GetCompact();
    }

    if (params.fPowAllowMinDifficultyBlocks && params.fPowNoRetargeting) {
        // Special difficulty rule:
        // If the new block's timestamp is more than 2 * 1 minutes
        // then allow mining of a min-difficulty block.
        if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2)
            return nProofOfWorkLimit;
        else {
            // Return the last non-special-min-difficulty-rules-block
            const CBlockIndex *pindex = pindexLast;
            while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 &&
                   pindex->nBits == nProofOfWorkLimit)
                pindex = pindex->pprev;
            return pindex->nBits;
        }
    }

    const CBlockIndex *pindex = pindexLast;
    arith_uint256 bnPastTargetAvg;

    int nKAWPOWBlocksFound = 0;
    int nMEOWPOWBlocksFound = 0;
    for (unsigned int nCountBlocks = 1; nCountBlocks <= nPastBlocks; nCountBlocks++) {
        arith_uint256 bnTarget = arith_uint256().SetCompact(pindex->nBits);
        if (nCountBlocks == 1) {
            bnPastTargetAvg = bnTarget;
        } else {
            // NOTE: that's not an average really...
            bnPastTargetAvg = (bnPastTargetAvg * nCountBlocks + bnTarget) / (nCountBlocks + 1);
        }

        // Count how blocks are KAWPOW mined in the last 180 blocks
        if (pindex->nTime >= nKAWPOWActivationTime && pindex->nTime < nMEOWPOWActivationTime) {
            nKAWPOWBlocksFound++;
        }

        // Count how blocks are MEOWPOW mined in the last 180 blocks
        if (pindex->nTime >= nMEOWPOWActivationTime) {
            nMEOWPOWBlocksFound++;
        }

        if(nCountBlocks != nPastBlocks) {
            assert(pindex->pprev); // should never fail
            pindex = pindex->pprev;
        }
    }

    // If we are mining a KAWPOW block. We check to see if we have mined
    // 180 KAWPOW or MEOWPOW blocks already. If we haven't we are going to return our
    // temp limit. This will allow us to change algos to kawpow without having to
    // change the DGW math.
 if (pblock->nTime >= nKAWPOWActivationTime && pblock->nTime < nMEOWPOWActivationTime) {
        if (nKAWPOWBlocksFound != nPastBlocks) {
            const arith_uint256 bnKawPowLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(PowAlgo::MEOWPOW)]);
            return bnKawPowLimit.GetCompact();
        }
    }

    //Meowpow
    if (pblock->nTime >= nMEOWPOWActivationTime) {
        if (nMEOWPOWBlocksFound != nPastBlocks) {
            const arith_uint256 bnMeowPowLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(PowAlgo::MEOWPOW)]);
            return bnMeowPowLimit.GetCompact();
        }
    }

    arith_uint256 bnNew(bnPastTargetAvg);

    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindex->GetBlockTime();
    // NOTE: is this accurate? nActualTimespan counts it for (nPastBlocks - 1) blocks only...
    int64_t nTargetTimespan = nPastBlocks * params.nPowTargetSpacing;

    if (nActualTimespan < nTargetTimespan/3)
        nActualTimespan = nTargetTimespan/3;
    if (nActualTimespan > nTargetTimespan*3)
        nActualTimespan = nTargetTimespan*3;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    return bnNew.GetCompact();
}

// LWMA-1 for BTC & Zcash clones
// Copyright (c) 2017-2019 The Bitcoin Gold developers, Zawy, iamstenman (Microbitcoin)
// MIT License
// Algorithm by Zawy, a modification of WT-144 by Tom Harding
// For updates see
// https://github.com/zawy12/difficulty-algorithms/issues/3#issuecomment-442129791
// Do not use Zcash's / Digishield's method of ignoring the ~6 most recent 
// timestamps via the median past timestamp (MTP of 11).
// Changing MTP to 1 instead of 11 enforces sequential timestamps. Not doing this was the
// most serious, problematic, & fundamental consensus theory mistake made in bitcoin but
// this change may require changes elsewhere such as creating block headers or what pools do.
//  FTL should be lowered to about N*T/20.
//  FTL in BTC clones is MAX_FUTURE_BLOCK_TIME in chain.h.
//  FTL in Ignition, Numus, and others can be found in main.h as DRIFT.
//  FTL in Zcash & Dash clones need to change the 2*60*60 here:
//  if (block.GetBlockTime() > nAdjustedTime + 2 * 60 * 60)
//  which is around line 3700 in main.cpp in ZEC and validation.cpp in Dash
//  If your coin uses median network time instead of node's time, the "revert to 
//  node time" rule (70 minutes in BCH, ZEC, & BTC) should be reduced to FTL/2 
//  to prevent 33% Sybil attack that can manipulate difficulty via timestamps. See:
// https://github.com/zcash/zcash/issues/4021
unsigned int GetNextWorkRequired_LWMA_MultiAlgo(
    const CBlockIndex* pindexLast,
    const CBlockHeader* pblock,
    const Consensus::Params& params,
    bool fIsAuxPow)
{
    assert(pindexLast != nullptr);

    // Base chain design target (e.g., 60s for the whole chain)
    const int64_t T_chain = params.nPowTargetSpacing;

    // Number of parallel algos contributing blocks - make this height-pure
    const bool auxActive = (pindexLast->nHeight + 1) >= params.nAuxpowStartHeight;
    const int64_t ALGOS = auxActive ? 2 : 1; // 2 if AuxPoW active, 1 if not

    // Effective per-algo target to achieve ~T_chain overall:
    // with 2 algos ~50/50, set per-algo to 2 * T_chain = 120s
    const int64_t T = T_chain * ALGOS;

    const int64_t N = params.nLwmaAveragingWindow;
    const int64_t k = N * (N + 1) * T / 2;   // includes per-algo T (now 120s)
    const int64_t height = pindexLast->nHeight;

    PowAlgo algo = pblock->nVersion.GetAlgo();
    if (fIsAuxPow) {
        algo = PowAlgo::SCRYPT; // AuxPoW always uses Scrypt difficulty
    }

    const arith_uint256 powLimit =
        UintToArith256(params.powLimit[static_cast<uint8_t>(algo)]);

    if (height < N) {
        unsigned int result = powLimit.GetCompact();
        LogPrintf("LWMA h=%d algo=%s aux=%d auxActive=%d ALGOS=%d same=%d exp=%08x hdrBits=%08x (height < N)\n",
                  pindexLast->nHeight+1, pblock->nVersion.GetAlgoName().c_str(), pblock->nVersion.IsAuxpow(),
                  auxActive, (int)ALGOS, 0, result, pblock->nBits);
        return result;
    }

    // Gather last N+1 blocks of the SAME algo
    std::vector<const CBlockIndex*> sameAlgo;
    sameAlgo.reserve(N + 1);

    int searchLimit = std::min<int64_t>(height, N * 10);
    for (int64_t h = height; h >= 0
         && (int)sameAlgo.size() < (N + 1)
         && (height - h) <= searchLimit; --h) {
        const CBlockIndex* bi = pindexLast->GetAncestor(h);
        if (!bi) break;
        PowAlgo bialgo = bi->nVersion.IsAuxpow() ? PowAlgo::SCRYPT : PowAlgo::MEOWPOW;
        if (bialgo == algo) sameAlgo.push_back(bi);
    }

    if ((int)sameAlgo.size() < (N + 1)) {
        if (!sameAlgo.empty()) {
            unsigned int result = sameAlgo.front()->nBits;
            const CBlockIndex* first = sameAlgo.front();
            const CBlockIndex* last = sameAlgo.back();
            LogPrintf("LWMA h=%d algo=%s aux=%d auxActive=%d ALGOS=%d same=%d exp=%08x hdrBits=%08x (using first same-algo) firstH=%d first=%s lastH=%d last=%s\n",
                      pindexLast->nHeight+1, pblock->nVersion.GetAlgoName().c_str(), pblock->nVersion.IsAuxpow(),
                      auxActive, (int)ALGOS, (int)sameAlgo.size(), result, pblock->nBits,
                      first->nHeight, first->GetBlockHash().ToString(), last->nHeight, last->GetBlockHash().ToString());
            return result;
        }
        unsigned int result = powLimit.GetCompact();
        LogPrintf("LWMA h=%d algo=%s aux=%d auxActive=%d ALGOS=%d same=%d exp=%08x hdrBits=%08x (no same-algo, using powLimit)\n",
                  pindexLast->nHeight+1, pblock->nVersion.GetAlgoName().c_str(), pblock->nVersion.IsAuxpow(),
                  auxActive, (int)ALGOS, (int)sameAlgo.size(), result, pblock->nBits);
        return result;
    }

    std::reverse(sameAlgo.begin(), sameAlgo.end());

    arith_uint256 sumTargets;                 // Σ target_i
    int64_t sumWeightedSolvetimes = 0;        // Σ i * solvetime_i

    int64_t prevTs = sameAlgo[0]->GetBlockTime();

    for (int64_t i = 1; i <= N; ++i) {
        const CBlockIndex* blk = sameAlgo[i];

        int64_t ts = blk->GetBlockTime();
        if (ts <= prevTs) ts = prevTs + 1;

        int64_t st = ts - prevTs;
        prevTs = ts;

        // Clamp relative to the per-algo target
        if (st < 1) st = 1;
        if (st > 6 * T) st = 6 * T;

        sumWeightedSolvetimes += (int64_t)i * st;

        arith_uint256 tgt; tgt.SetCompact(blk->nBits);
        sumTargets += tgt;
    }

    arith_uint256 avgTarget = sumTargets / N;

    // LWMA-1 with k = N*(N+1)*T/2  (T is per-algo target)
    arith_uint256 nextTarget = avgTarget;
    if (sumWeightedSolvetimes < 1) sumWeightedSolvetimes = 1;
    nextTarget *= (uint64_t)sumWeightedSolvetimes;
    nextTarget /= (uint64_t)k;

    if (nextTarget > powLimit) nextTarget = powLimit;

    unsigned int result = nextTarget.GetCompact();
    
    // Debug logging to track difficulty calculation
    const CBlockIndex* first = sameAlgo.front();
    const CBlockIndex* last = sameAlgo.back();
    LogPrintf("LWMA h=%d algo=%s aux=%d auxActive=%d ALGOS=%d same=%d exp=%08x hdrBits=%08x firstH=%d first=%s lastH=%d last=%s\n",
              pindexLast->nHeight+1, pblock->nVersion.GetAlgoName().c_str(), pblock->nVersion.IsAuxpow(),
              auxActive, (int)ALGOS, (int)sameAlgo.size(), result, pblock->nBits,
              first->nHeight, first->GetBlockHash().ToString(), last->nHeight, last->GetBlockHash().ToString());
    
    return result;
}


unsigned int GetNextWorkRequiredBTC(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(PowAlgo::MEOWPOW)]).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}


unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params, bool fIsAuxPow /*ignored*/)
{
    if (params.IsAuxpowActive(pindexLast->nHeight + 1)) {
        // IMPORTANT: derive AuxPoW from the header's version bit during headers-first sync
        const bool fIsAuxPowBlock = pblock->nVersion.IsAuxpow();
        return GetNextWorkRequired_LWMA_MultiAlgo(pindexLast, pblock, params, fIsAuxPowBlock);
    }

    if (IsDGWActive(pindexLast->nHeight + 1)) {
        return DarkGravityWave(pindexLast, pblock, params);
    }
    else {
        return GetNextWorkRequiredBTC(pindexLast, pblock, params);
    }

}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(PowAlgo::MEOWPOW)]);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, PowAlgo algo, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit[static_cast<uint8_t>(algo)]))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
