// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <primitives/algos.h>
#include <uint256.h>

#include <array>
#include <chrono>
#include <limits>
#include <map>
#include <vector>

namespace Consensus {

/**
 * A buried deployment is one where the height of the activation has been hardcoded into
 * the client implementation long after the consensus change has activated. See BIP 90.
 */
enum BuriedDeployment : int16_t {
    // buried deployments get negative values to avoid overlap with DeploymentPos
    DEPLOYMENT_HEIGHTINCB = std::numeric_limits<int16_t>::min(),
    DEPLOYMENT_CLTV,
    DEPLOYMENT_DERSIG,
    DEPLOYMENT_CSV,
    DEPLOYMENT_SEGWIT,
};
constexpr bool ValidDeployment(BuriedDeployment dep) { return dep <= DEPLOYMENT_SEGWIT; }

enum DeploymentPos : uint16_t {
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_TAPROOT, // Deployment of Schnorr/Taproot (BIPs 340-342)
    DEPLOYMENT_ASSETS, // Deployment of HIP2 (asset layer)
    DEPLOYMENT_MSG_REST_ASSETS, // Deployment of RIP5 and restricted assets
    DEPLOYMENT_TRANSFER_SCRIPT_SIZE,
    DEPLOYMENT_ENFORCE_VALUE,
    DEPLOYMENT_COINBASE_ASSETS,
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in deploymentinfo.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};
constexpr bool ValidDeployment(DeploymentPos dep) { return dep < MAX_VERSION_BITS_DEPLOYMENTS; }

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit{28};
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime{NEVER_ACTIVE};
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout{NEVER_ACTIVE};
    /** If lock in occurs, delay activation until at least this block
     *  height.  Note that activation will only occur on a retarget
     *  boundary.
     */
    int min_activation_height{0};
    /** Period of blocks to check signalling in (usually retarget period, ie params.DifficultyAdjustmentInterval()) */
    uint32_t period{2016};
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t threshold{1916};

    /** Meowcoin: per-deployment override for miner confirmation window (0 = use global). */
    uint32_t nOverrideMinerConfirmationWindow{0};
    /** Meowcoin: per-deployment override for activation threshold (0 = use global). */
    uint32_t nOverrideRuleChangeActivationThreshold{0};

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;

    /** Special value for nStartTime indicating that the deployment is never active.
     *  This is useful for integrating the code changes for a new feature
     *  prior to deploying it on some or all networks. */
    static constexpr int64_t NEVER_ACTIVE = -2;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /**
     * Hashes of blocks that
     * - are known to be consensus valid, and
     * - buried in the chain, and
     * - fail if the default script verify flags are applied.
     */
    std::map<uint256, uint32_t> script_flag_exceptions;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which CSV (BIP68, BIP112 and BIP113) becomes active */
    int CSVHeight;
    /** Block height at which Segwit (BIP141, BIP143 and BIP147) becomes active.
     * Note that segwit v0 script rules are enforced on all blocks except the
     * BIP 16 exception blocks. */
    int SegwitHeight;
    /** Don't warn about unknown BIP 9 activations below this height.
     * This prevents us from warning about the CSV and segwit activations. */
    int MinBIP9WarningHeight;

    /** Meowcoin: global rule-change activation threshold (used when per-deployment override is 0). */
    uint32_t nRuleChangeActivationThreshold{1916};
    /** Meowcoin: global miner confirmation window (used when per-deployment override is 0). */
    uint32_t nMinerConfirmationWindow{2016};

    std::array<BIP9Deployment,MAX_VERSION_BITS_DEPLOYMENTS> vDeployments;

    /** Proof of work parameters — per-algorithm limits for multi-algo mining. */
    uint256 powLimit;
    uint256 powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::NUM_ALGOS)];

    bool fPowAllowMinDifficultyBlocks;
    /**
      * Enforce BIP94 timewarp attack mitigation. On testnet4 this also enforces
      * the block storm mitigation.
      */
    bool enforce_BIP94;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;

    /** Meowcoin: LWMA averaging window (typically 45). */
    int64_t nLwmaAveragingWindow{45};

    std::chrono::seconds PowTargetSpacing() const
    {
        return std::chrono::seconds{nPowTargetSpacing};
    }
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    /** The best chain should have at least this much work */
    uint256 nMinimumChainWork;
    /** By default assume that the signatures in ancestors of this block are valid */
    uint256 defaultAssumeValid;

    /**
     * If true, witness commitments contain a payload equal to a Meowcoin Script solution
     * to the signet challenge. See BIP325.
     */
    bool signet_blocks{false};
    std::vector<uint8_t> signet_challenge;

    /** Meowcoin: boolean toggles for BIP enforcement (replaces buried-height checks for Meowcoin). */
    bool nBIP34Enabled{true};
    bool nBIP65Enabled{true};
    bool nBIP66Enabled{true};
    bool nSegwitEnabled{true};
    bool nCSVEnabled{true};

    /** Meowcoin: AuxPoW parameters */
    int32_t nAuxpowChainId{9};
    int nAuxpowStartHeight{0};
    bool fStrictChainId{true};
    int nLegacyBlocksBefore{-1}; //!< -1 = always allow legacy

    bool IsAuxpowActive(int nHeight) const
    {
        return nHeight >= nAuxpowStartHeight;
    }

    bool AllowLegacyBlocks(unsigned nHeight) const
    {
        if (nLegacyBlocksBefore < 0)
            return true;
        return static_cast<int>(nHeight) < nLegacyBlocksBefore;
    }

    int DeploymentHeight(BuriedDeployment dep) const
    {
        switch (dep) {
        case DEPLOYMENT_HEIGHTINCB:
            return BIP34Height;
        case DEPLOYMENT_CLTV:
            return BIP65Height;
        case DEPLOYMENT_DERSIG:
            return BIP66Height;
        case DEPLOYMENT_CSV:
            return CSVHeight;
        case DEPLOYMENT_SEGWIT:
            return SegwitHeight;
        } // no default case, so the compiler can warn about missing cases
        return std::numeric_limits<int>::max();
    }
};

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
