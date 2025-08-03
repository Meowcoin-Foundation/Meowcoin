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
    LogPrintf("DEBUG: GetNextWorkRequired - height=%d, IsAuxpowActive=%d, algo=%d\n", pindexLast->nHeight + 1, params.IsAuxpowActive(pindexLast->nHeight + 1), pblock->nVersion.GetAlgoName());
    if (params.IsAuxpowActive(pindexLast->nHeight + 1)) {
        if (pblock->nVersion.IsAuxpow()) {
            fIsAuxPow = true;
        }
        LogPrintf("DEBUG: GetNextWorkRequired - Using LWMA. IsAuxPow: %s\n", fIsAuxPow);
        return DarkGravityWave3(pindexLast, pblock, params, fIsAuxPow);
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
