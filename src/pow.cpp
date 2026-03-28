// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Meowcoin Core developers
// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <kernel/chainparams.h>
#include <logging.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/check.h>

#include <algorithm>
#include <vector>

// ---------------------------------------------------------------------------
// DarkGravityWave v3 (pre-AuxPoW era)
// ---------------------------------------------------------------------------
static unsigned int DarkGravityWave(const CBlockIndex* pindexLast,
                                    const CBlockHeader* pblock,
                                    const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::MEOWPOW)]);
    unsigned int nProofOfWorkLimit = bnPowLimit.GetCompact();
    const int64_t nPastBlocks = 180; // ~3 hr

    if (!pindexLast || pindexLast->nHeight < nPastBlocks) {
        return bnPowLimit.GetCompact();
    }

    if (params.fPowAllowMinDifficultyBlocks && params.fPowNoRetargeting) {
        if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2)
            return nProofOfWorkLimit;
        const CBlockIndex* pindex = pindexLast;
        while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 &&
               pindex->nBits == nProofOfWorkLimit)
            pindex = pindex->pprev;
        return pindex->nBits;
    }

    const CBlockIndex* pindex = pindexLast;
    arith_uint256 bnPastTargetAvg;

    int nKAWPOWBlocksFound = 0;
    int nMEOWPOWBlocksFound = 0;
    for (int64_t nCountBlocks = 1; nCountBlocks <= nPastBlocks; nCountBlocks++) {
        arith_uint256 bnTarget = arith_uint256().SetCompact(pindex->nBits);
        if (nCountBlocks == 1) {
            bnPastTargetAvg = bnTarget;
        } else {
            bnPastTargetAvg = (bnPastTargetAvg * nCountBlocks + bnTarget) / (nCountBlocks + 1);
        }

        if (pindex->nTime >= nKAWPOWActivationTime && pindex->nTime < nMEOWPOWActivationTime)
            nKAWPOWBlocksFound++;
        if (pindex->nTime >= nMEOWPOWActivationTime)
            nMEOWPOWBlocksFound++;

        if (nCountBlocks != nPastBlocks) {
            assert(pindex->pprev);
            pindex = pindex->pprev;
        }
    }

    if (pblock->nTime >= nKAWPOWActivationTime && pblock->nTime < nMEOWPOWActivationTime) {
        if (nKAWPOWBlocksFound != nPastBlocks) {
            return bnPowLimit.GetCompact();
        }
    }
    if (pblock->nTime >= nMEOWPOWActivationTime) {
        if (nMEOWPOWBlocksFound != nPastBlocks) {
            return bnPowLimit.GetCompact();
        }
    }

    arith_uint256 bnNew(bnPastTargetAvg);
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindex->GetBlockTime();
    int64_t nTargetTimespan = nPastBlocks * params.nPowTargetSpacing;

    if (nActualTimespan < nTargetTimespan / 3)
        nActualTimespan = nTargetTimespan / 3;
    if (nActualTimespan > nTargetTimespan * 3)
        nActualTimespan = nTargetTimespan * 3;

    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// ---------------------------------------------------------------------------
// LWMA-1 multi-algo (post-AuxPoW era)
// Copyright (c) 2017-2019 The Meowcoin Gold developers, Zawy, iamstenman
// Algorithm by Zawy, a modification of WT-144 by Tom Harding
// ---------------------------------------------------------------------------
static unsigned int GetNextWorkRequired_LWMA_MultiAlgo(
    const CBlockIndex* pindexLast,
    const CBlockHeader* pblock,
    const Consensus::Params& params,
    bool fIsAuxPow)
{
    assert(pindexLast != nullptr);

    const int64_t T_chain = params.nPowTargetSpacing;
    const bool auxActive = (pindexLast->nHeight + 1) >= params.nAuxpowStartHeight;
    const int64_t ALGOS = auxActive ? 2 : 1;
    const int64_t T = T_chain * ALGOS;

    const int64_t N = params.nLwmaAveragingWindow;
    const int64_t k = N * (N + 1) * T / 2;
    const int64_t height = pindexLast->nHeight;

    PowAlgo algo = pblock->nVersion.GetAlgo();
    if (fIsAuxPow) {
        algo = PowAlgo::SCRYPT;
    }

    const arith_uint256 powLimit =
        UintToArith256(params.powLimitPerAlgo[static_cast<uint8_t>(algo)]);

    if (height < N) {
        return powLimit.GetCompact();
    }

    // Gather last N+1 blocks of the SAME algo
    std::vector<const CBlockIndex*> sameAlgo;
    sameAlgo.reserve(N + 1);

    int64_t searchLimit = std::min<int64_t>(height, N * 10);
    for (int64_t h = height; h >= 0
         && static_cast<int64_t>(sameAlgo.size()) < (N + 1)
         && (height - h) <= searchLimit; --h)
    {
        const CBlockIndex* bi = pindexLast->GetAncestor(h);
        if (!bi) break;
        PowAlgo bialgo = bi->nVersion.IsAuxpow() ? PowAlgo::SCRYPT : PowAlgo::MEOWPOW;
        if (bialgo == algo) sameAlgo.push_back(bi);
    }

    if (static_cast<int64_t>(sameAlgo.size()) < (N + 1)) {
        if (!sameAlgo.empty()) {
            return sameAlgo.front()->nBits;
        }
        return powLimit.GetCompact();
    }

    std::reverse(sameAlgo.begin(), sameAlgo.end());

    arith_uint256 sumTargets;
    int64_t sumWeightedSolvetimes = 0;
    int64_t prevTs = sameAlgo[0]->GetBlockTime();

    for (int64_t i = 1; i <= N; ++i) {
        const CBlockIndex* blk = sameAlgo[i];

        int64_t ts = blk->GetBlockTime();
        if (ts <= prevTs) ts = prevTs + 1;

        int64_t st = ts - prevTs;
        prevTs = ts;

        if (st < 1) st = 1;
        if (st > 6 * T) st = 6 * T;

        sumWeightedSolvetimes += i * st;

        arith_uint256 tgt;
        tgt.SetCompact(blk->nBits);
        sumTargets += tgt;
    }

    arith_uint256 avgTarget = sumTargets / N;

    arith_uint256 nextTarget = avgTarget;
    if (sumWeightedSolvetimes < 1) sumWeightedSolvetimes = 1;
    nextTarget *= static_cast<uint64_t>(sumWeightedSolvetimes);
    nextTarget /= static_cast<uint64_t>(k);

    if (nextTarget > powLimit) nextTarget = powLimit;

    return nextTarget.GetCompact();
}

// ---------------------------------------------------------------------------
// Original MEWC-style retarget (genesis era, before DGW)
// ---------------------------------------------------------------------------
static unsigned int GetNextWorkRequiredBTC(const CBlockIndex* pindexLast,
                                           const CBlockHeader* pblock,
                                           const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::MEOWPOW)]).GetCompact();

    if ((pindexLast->nHeight + 1) % params.DifficultyAdjustmentInterval() != 0) {
        if (params.fPowAllowMinDifficultyBlocks) {
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2)
                return nProofOfWorkLimit;
            const CBlockIndex* pindex = pindexLast;
            while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                pindex = pindex->pprev;
            return pindex->nBits;
        }
        return pindexLast->nBits;
    }

    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval() - 1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

// ---------------------------------------------------------------------------
// Top-level dispatcher
// ---------------------------------------------------------------------------
bool IsDGWActive(unsigned int nBlockNumber)
{
    // TODO: Wire to CChainParams::DGWActivationBlock() once init sets the global.
    // For now DGW activates at block 1 (always on for Meowcoin mainnet).
    (void)nBlockNumber;
    return true;
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast,
                                 const CBlockHeader* pblock,
                                 const Consensus::Params& params,
                                 bool fIsAuxPow)
{
    if (params.IsAuxpowActive(pindexLast->nHeight + 1)) {
        const bool fIsAuxPowBlock = pblock->nVersion.IsAuxpow();
        return GetNextWorkRequired_LWMA_MultiAlgo(pindexLast, pblock, params, fIsAuxPowBlock);
    }

    if (IsDGWActive(pindexLast->nHeight + 1)) {
        return DarkGravityWave(pindexLast, pblock, params);
    }

    return GetNextWorkRequiredBTC(pindexLast, pblock, params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast,
                                       int64_t nFirstBlockTime,
                                       const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan / 4)
        nActualTimespan = params.nPowTargetTimespan / 4;
    if (nActualTimespan > params.nPowTargetTimespan * 4)
        nActualTimespan = params.nPowTargetTimespan * 4;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::MEOWPOW)]);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// ---------------------------------------------------------------------------
// PermittedDifficultyTransition (kept from Meowcoin Core for test harness)
// ---------------------------------------------------------------------------
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height,
                                   uint32_t old_nbits, uint32_t new_nbits)
{
    if (params.fPowAllowMinDifficultyBlocks) return true;

    // For Meowcoin's per-block retarget we always allow the transition.
    // The DGW / LWMA algorithm handles bounds internally.
    (void)height;
    (void)old_nbits;
    (void)new_nbits;
    return true;
}

// ---------------------------------------------------------------------------
// CheckProofOfWork — multi-algo version
// ---------------------------------------------------------------------------
bool CheckProofOfWork(uint256 hash, unsigned int nBits, PowAlgo algo,
                      const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow ||
        bnTarget > UintToArith256(params.powLimitPerAlgo[static_cast<uint8_t>(algo)]))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

// ---------------------------------------------------------------------------
// DeriveTarget + CheckProofOfWorkImpl (legacy scalar powLimit wrappers)
// ---------------------------------------------------------------------------
std::optional<arith_uint256> DeriveTarget(unsigned int nBits, const uint256 pow_limit)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(pow_limit))
        return {};

    return bnTarget;
}

bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    // Use the original scalar powLimit (most permissive, e.g. 7fff... for regtest).
    auto bnTarget{DeriveTarget(nBits, params.powLimit)};
    if (!bnTarget) return false;

    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
