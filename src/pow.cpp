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

unsigned int DarkGravityWave3(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params, bool fIsAuxPow) {
    /* difficulty formula, DarkGravity v3, written by Evan Duffield - evan@darkcoin.io */
    const CBlockIndex *pindex = pindexLast;

    long long nActualTimespan = 0;
    long long LastBlockTime = 0;
    long long PastBlocksMin = 180;
    long long PastBlocksMax = 180;
    long long CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
    PowAlgo algo = pblock->nVersion.GetAlgo();
    if (fIsAuxPow) {
        algo = PowAlgo::SCRYPT;
    }

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(algo)]);

    if (!pindexLast || pindexLast->nHeight < PastBlocksMin)
    {
        return bnPowLimit.GetCompact();
    }

    while (pindex && pindex->nHeight > 0)
    {
        if (PastBlocksMax > 0 && CountBlocks >= PastBlocksMax) {
        	break;
        }

        // we only consider proof-of-work blocks for the configured mining algo here
        if (pindex->nVersion.GetAlgo() != algo)
        {
        	pindex = pindex->pprev;
            continue;
        }

        CountBlocks++;

		if (CountBlocks <= PastBlocksMin)
        {
			if (CountBlocks == 1) {
				PastDifficultyAverage.SetCompact(pindex->nBits);
            }
			else
            {
				PastDifficultyAverage =
					((PastDifficultyAveragePrev * CountBlocks) + (arith_uint256().SetCompact(pindex->nBits))) / (CountBlocks + 1);
            }
			PastDifficultyAveragePrev = PastDifficultyAverage;
		}

        if (LastBlockTime > 0){
        	long long Diff = (LastBlockTime - pindex->GetBlockTime());
            nActualTimespan += Diff;
        }
        LastBlockTime = pindex->GetBlockTime();

        if (pindex->pprev == NULL) {
            assert(pindex);
            break;
        }
        pindex = pindex->pprev;
    }

    if (!CountBlocks)
        return bnPowLimit.GetCompact();

    arith_uint256 bnNew(PastDifficultyAverage);

    long long nTargetTimespan = CountBlocks * params.nPowTargetSpacing;

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
//  FTL for CAT is 45 * (10 * 60)/ 20 == 1350 
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
unsigned int GetNextWorkRequired_LWMA_MultiAlgo(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params& params, bool fIsAuxPow)
{
    assert(pindexLast != nullptr);
    
    LogPrintf("LWMA_DEBUG: === START LWMA_MultiAlgo ===\n");
    LogPrintf("LWMA_DEBUG: pindexLast->nHeight=%d, fIsAuxPow=%s\n", pindexLast->nHeight, fIsAuxPow ? "true" : "false");
    
    const int64_t T = params.nPowTargetSpacing;
    const int64_t N = params.nLwmaAveragingWindow;
    const int64_t k = N * (N + 1) * T / 2;
    const int64_t height = pindexLast->nHeight;
    PowAlgo algo = pblock->nVersion.GetAlgo();
    
    LogPrintf("LWMA_DEBUG: T=%d, N=%d, k=%d, height=%d, original_algo=%d\n", 
              (int)T, (int)N, (int)k, (int)height, static_cast<int>(algo));
    
    // For AuxPoW blocks, always use SCRYPT difficulty
    if (fIsAuxPow) {
        algo = PowAlgo::SCRYPT;
        LogPrintf("LWMA_DEBUG: Forcing SCRYPT for AuxPoW (merge mining)\n");
    }
    
    LogPrintf("LWMA_DEBUG: Final algo=%d (%s)\n", static_cast<int>(algo), 
              algo == PowAlgo::SCRYPT ? "SCRYPT" : "MEOWPOW");
    
    const arith_uint256 powLimit = UintToArith256(params.powLimit[static_cast<uint8_t>(algo)]);
    LogPrintf("LWMA_DEBUG: powLimit for algo %d = %08x\n", static_cast<int>(algo), powLimit.GetCompact());

    // New coins just "give away" first N blocks. It's better to guess
    // this value instead of using powLimit, but err on high side to not get stuck.
    if (height < N) {
        LogPrintf("LWMA_DEBUG: Height %d < N %d, returning powLimit: %08x\n", (int)height, (int)N, powLimit.GetCompact());
        return powLimit.GetCompact();
    }

    arith_uint256 avgTarget, nextTarget;
    int64_t thisTimestamp, previousTimestamp;
    int64_t sumWeightedSolvetimes = 0, j = 0;

    std::vector<const CBlockIndex*> SameAlgoBlocks;
    int searchLimit = std::min(height, N * 10); // Search up to 10x N blocks back
    
    LogPrintf("LWMA_DEBUG: Searching for %d blocks of algo %d, searchLimit=%d\n", (int)N, static_cast<int>(algo), searchLimit);
    
    for (int c = height-1; c >= 0 && SameAlgoBlocks.size() < (N + 1) && (height - c) <= searchLimit; c--){
        const CBlockIndex* block = pindexLast->GetAncestor(c);
        if (!block) {
            LogPrintf("LWMA_DEBUG: GetAncestor(%d) returned null, breaking\n", c);
            break;
        }
        
        PowAlgo blockAlgo = block->GetBlockHeader(params).nVersion.GetAlgo();
        if (blockAlgo == algo){
            SameAlgoBlocks.push_back(block);
            LogPrintf("LWMA_DEBUG: Found matching block at height %d, algo=%d, total_found=%d\n", 
                      block->nHeight, static_cast<int>(blockAlgo), (int)SameAlgoBlocks.size());
        }
    }
    
    LogPrintf("LWMA_DEBUG: Found %d blocks of algo %d, need %d\n", 
              (int)SameAlgoBlocks.size(), static_cast<int>(algo), (int)N);
    
    // If we don't have enough blocks of this algorithm, fall back to a reasonable difficulty
    if (SameAlgoBlocks.size() < N) {
        LogPrintf("LWMA_DEBUG: INSUFFICIENT BLOCKS - Only found %d blocks of algorithm %d, need %d\n", 
                  (int)SameAlgoBlocks.size(), static_cast<int>(algo), (int)N);
        
        // For AuxPoW (SCRYPT), use a reasonable difficulty that's not the maximum
        if (algo == PowAlgo::SCRYPT) {
            // Use a difficulty that's reasonable for SCRYPT - not the maximum
            arith_uint256 reasonableTarget = powLimit / 1000;
            LogPrintf("LWMA_DEBUG: Using reasonable SCRYPT difficulty: %08x (powLimit/1000)\n", reasonableTarget.GetCompact());
            return reasonableTarget.GetCompact();
        }
        
        LogPrintf("LWMA_DEBUG: Using maximum difficulty fallback: %08x\n", powLimit.GetCompact());
        return powLimit.GetCompact();
    }

    LogPrintf("LWMA_DEBUG: SUFFICIENT BLOCKS - Proceeding with LWMA calculation\n");
    
    // Loop through N most recent blocks.
    for (int64_t i = N; i > 0; i--) {
        const CBlockIndex* block = SameAlgoBlocks[i-1];
        const CBlockIndex* blockPreviousTimestamp = SameAlgoBlocks[i-1];
        previousTimestamp = blockPreviousTimestamp->GetBlockTime();

        // Prevent solvetimes from being negative in a safe way. It must be done like this.
        // Do not attempt anything like  if (solvetime < 1) {solvetime=1;}
        // The +1 ensures new coins do not calculate nextTarget = 0.
        thisTimestamp = (block->GetBlockTime() > previousTimestamp) ?
                            block->GetBlockTime() :
                            previousTimestamp + 1;

        // 6*T limit prevents large drops in diff from long solvetimes which would cause oscillations.
        int64_t solvetime = std::min(6 * T, thisTimestamp - previousTimestamp);

        // The following is part of "preventing negative solvetimes".
        previousTimestamp = thisTimestamp;

        // Give linearly higher weight to more recent solvetimes.
        j++;
        sumWeightedSolvetimes += solvetime * j;

        arith_uint256 target;
        target.SetCompact(block->nBits);
        avgTarget += target / N / k; // Dividing by k here prevents an overflow below.
        
        LogPrintf("LWMA_DEBUG: Block %d: height=%d, time=%d, solvetime=%d, nBits=%08x, weight=%d\n", 
                  (int)i, block->nHeight, (int)block->GetBlockTime(), (int)solvetime, block->nBits, (int)j);
    }
    
    nextTarget = avgTarget * sumWeightedSolvetimes;
    LogPrintf("LWMA_DEBUG: sumWeightedSolvetimes=%d, avgTarget=%08x, nextTarget=%08x\n", 
              (int)sumWeightedSolvetimes, avgTarget.GetCompact(), nextTarget.GetCompact());

    if (nextTarget > powLimit) {
        LogPrintf("LWMA_DEBUG: nextTarget > powLimit, clamping to powLimit: %08x\n", powLimit.GetCompact());
        nextTarget = powLimit;
    }

    LogPrintf("LWMA_DEBUG: === END LWMA_MultiAlgo - Returning: %08x ===\n", nextTarget.GetCompact());
    return nextTarget.GetCompact();
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


unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params, bool fIsAuxPow)
{
    LogPrintf("DEBUG: GetNextWorkRequired - height=%d, IsAuxpowActive=%d, algo=%d\n", pindexLast->nHeight + 1, params.IsAuxpowActive(pindexLast->nHeight + 1), static_cast<int>(pblock->nVersion.GetAlgo()));
    if (params.IsAuxpowActive(pindexLast->nHeight + 1)) {
        // FIX: Use the block's version to determine if it's AuxPoW, not the parameter
        bool fIsAuxPowBlock = pblock->nVersion.IsAuxpow();
        LogPrintf("DEBUG: GetNextWorkRequired - Using LWMA. IsAuxPow: %s, block.IsAuxpow(): %s\n", fIsAuxPow ? "true" : "false", fIsAuxPowBlock ? "true" : "false");
        return GetNextWorkRequired_LWMA_MultiAlgo(pindexLast, pblock, params, fIsAuxPowBlock);
    }

    if (IsDGWActive(pindexLast->nHeight + 1)) {
        LogPrintf("DEBUG: GetNextWorkRequired - Using DarkGravityWave\n");
        return DarkGravityWave(pindexLast, pblock, params);
    }
    else {
        LogPrintf("DEBUG: GetNextWorkRequired - Using BTC\n");
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
