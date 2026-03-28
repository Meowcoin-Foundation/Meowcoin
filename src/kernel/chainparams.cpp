// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

using namespace util::hex_literals;

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    // Meowcoin genesis coinbase uses CScriptNum(0) prefix before the nBits push
    txNew.vin[0].scriptSig = CScript() << CScriptNum(0) << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the Meowcoin genesis block.
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The WSJ 08/28/2022 Investors Ramp Up Bets Against Stock Market";
    const CScript genesisOutputScript = CScript() << "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"_hex << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Meowcoin main network.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 2100000; // ~4 yrs at 1 min blocks

        // Meowcoin: BIP enforcement booleans (always active from genesis)
        consensus.nBIP34Enabled = true;
        consensus.nBIP65Enabled = true;
        consensus.nBIP66Enabled = true;
        consensus.nSegwitEnabled = true;
        consensus.nCSVEnabled = true;

        // Buried heights — set to 1 so they're always active
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;

        // PoW limits per algorithm
        consensus.powLimit = uint256{"00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::MEOWPOW)] = uint256{"00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::SCRYPT)]  = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};

        consensus.nPowTargetTimespan = 2016 * 60; // 1.4 days
        consensus.nPowTargetSpacing = 1 * 60;     // 1 minute
        consensus.nLwmaAveragingWindow = 45;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;

        // Global BIP9 activation parameters
        consensus.nRuleChangeActivationThreshold = 1613; // ~80% of 2016
        consensus.nMinerConfirmationWindow = 2016;

        // --- Version-bits deployments ---
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1653004800;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1653264000;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nOverrideRuleChangeActivationThreshold = 1814;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nOverrideMinerConfirmationWindow = 2016;

        // Taproot: always active on Meowcoin (no historical Meowcoin taproot chain)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Assets (HIP2)
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].bit = 6;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nStartTime = 1653004800;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nTimeout = 1653264000;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nOverrideRuleChangeActivationThreshold = 1814;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nOverrideMinerConfirmationWindow = 2016;

        // Messaging + restricted assets (RIP5)
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].bit = 7;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nStartTime = 1653004800;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nTimeout = 1653264000;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nOverrideRuleChangeActivationThreshold = 1714;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nOverrideMinerConfirmationWindow = 2016;

        // Transfer script size
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].bit = 8;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nStartTime = 1653004800;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nTimeout = 1653264000;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nOverrideRuleChangeActivationThreshold = 1714;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nOverrideMinerConfirmationWindow = 2016;

        // Enforce value
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].bit = 9;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nStartTime = 1653004800;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nTimeout = 1653264000;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nOverrideRuleChangeActivationThreshold = 1411;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nOverrideMinerConfirmationWindow = 2016;

        // Coinbase assets
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].bit = 10;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nStartTime = 1653004800;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nTimeout = 1653264000;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nOverrideRuleChangeActivationThreshold = 1411;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nOverrideMinerConfirmationWindow = 2016;

        // AuxPoW parameters
        consensus.nAuxpowChainId = 9;
        consensus.nAuxpowStartHeight = 1614560;
        consensus.fStrictChainId = true;
        consensus.nLegacyBlocksBefore = 1614560;

        consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000000000859d8e6ba8224a4"};
        consensus.defaultAssumeValid = uint256{"0000000000043a9280011ff1382e99ade4d90d51821cc4dadfb20bd1a0905b1c"}; // Block 50000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         */
        pchMessageStart[0] = 0x4d; // M
        pchMessageStart[1] = 0x45; // E
        pchMessageStart[2] = 0x57; // W
        pchMessageStart[3] = 0x43; // C
        nDefaultPort = 8788;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 10;
        m_assumed_chain_state_size = 2;

        // Set activation timestamps BEFORE genesis creation so GetHash() uses
        // the correct PoW algorithm during serialization.
        // Must set both the class member and the global (block.h) because
        // CBlockHeader::GetHash() uses the global.
        nKAWPOWActivationTime = 1662493424;
        ::nKAWPOWActivationTime = 1662493424;
        nMEOWPOWActivationTime = 1710799200;
        ::nMEOWPOWActivationTime = 1710799200;

        // Meowcoin genesis: nTime=1661730843, nNonce=351574, nBits=0x1e00ffff, nVersion=4, reward=5000
        genesis = CreateGenesisBlock(1661730843, 351574, 0x1e00ffff, 4, 5000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // TODO: Genesis hash assertion requires X16R hashing — will be enabled after Patch 4
        // assert(consensus.hashGenesisBlock == uint256{"000000edd819220359469c54f2614b5602ebc775ea67a64602f354bdaa320f70"});
        assert(genesis.hashMerkleRoot == uint256{"e8916cf6592c8433d598c3a5fe60a9741fd2a997b39d93af2d789cdd9d9a7390"});

        vSeeds.emplace_back("seed-mainnet-mewc.meowcoin.cc");
        vSeeds.emplace_back("dnsseed.nodeslist.xyz");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,50);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,122);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,112);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "mewc";
        nExtCoinType = 1669;

        vFixedSeeds.clear(); // TODO: populate with Meowcoin seed nodes

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {}; // No assumeutxo data for Meowcoin yet

        chainTxData = ChainTxData{
            .nTime    = 1665504479,
            .tx_count = 99360,
            .dTxRate  = 0.03289449084985968,
        };

        /** Meowcoin asset parameters **/
        nIssueAssetBurnAmount = 500 * COIN;
        nReissueAssetBurnAmount = 100 * COIN;
        nIssueSubAssetBurnAmount = 100 * COIN;
        nIssueUniqueAssetBurnAmount = 5 * COIN;
        nIssueMsgChannelAssetBurnAmount = 100 * COIN;
        nIssueQualifierAssetBurnAmount = 1000 * COIN;
        nIssueSubQualifierAssetBurnAmount = 100 * COIN;
        nIssueRestrictedAssetBurnAmount = 1500 * COIN;
        nAddNullQualifierTagBurnAmount = .1 * COIN;

        nCommunityAutonomousAmount = 40; // 40% of 5000 COIN to donation

        strIssueAssetBurnAddress = "MCissueAssetXXXXXXXXXXXXXXXXa1oUfD";
        strReissueAssetBurnAddress = "MCReissueAssetXXXXXXXXXXXXXXUdjigq";
        strIssueSubAssetBurnAddress = "MCissueSubAssetXXXXXXXXXXXXXbCnNFk";
        strIssueUniqueAssetBurnAddress = "MCissueUniqueAssetXXXXXXXXXXSVUgF5";
        strIssueMsgChannelAssetBurnAddress = "MCissueMsgChanneLAssetXXXXXXUe6Pvr";
        strIssueQualifierAssetBurnAddress = "MCissueQuaLifierXXXXXXXXXXXXWLyvs5";
        strIssueSubQualifierAssetBurnAddress = "MCissueSubQuaLifierXXXXXXXXXVHmaXW";
        strIssueRestrictedAssetBurnAddress = "MCissueRestrictedXXXXXXXXXXXXfEYLU";
        strAddNullQualifierTagBurnAddress = "MCaddTagBurnXXXXXXXXXXXXXXXXUrKr7b";
        strGlobalBurnAddress = "MCBurnXXXXXXXXXXXXXXXXXXXXXXUkdzqy";
        strCommunityAutonomousAddress = "MPyNGZSSZ4rbjkVJRLn3v64pMcktpEYJnU";

        nDGWActivationBlock = 1;
        nMaxReorganizationDepth = 60;
        nMinReorganizationPeers = 4;
        nMinReorganizationAge = 60 * 60 * 12;
        nAssetActivationHeight = 1;
        nMessagingActivationBlock = 1;
        nRestrictedActivationBlock = 1;

        // (activation timestamps already set above, before genesis creation)
    }
};

/**
 * Meowcoin testnet (v7).
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 2100000;

        consensus.nBIP34Enabled = true;
        consensus.nBIP65Enabled = true;
        consensus.nBIP66Enabled = true;
        consensus.nSegwitEnabled = true;
        consensus.nCSVEnabled = true;

        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;

        consensus.powLimit = uint256{"00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::MEOWPOW)] = uint256{"00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::SCRYPT)]  = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};

        consensus.nPowTargetTimespan = 2016 * 60;
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.nLwmaAveragingWindow = 45;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;

        consensus.nRuleChangeActivationThreshold = 1613;
        consensus.nMinerConfirmationWindow = 2016;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1658055600;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1659178800;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nOverrideRuleChangeActivationThreshold = 1310;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nOverrideMinerConfirmationWindow = 2016;

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].bit = 6;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nStartTime = 1658055600;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nTimeout = 1659178800;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nOverrideRuleChangeActivationThreshold = 1310;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nOverrideMinerConfirmationWindow = 2016;

        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].bit = 7;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nStartTime = 1658055600;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nTimeout = 1659178800;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nOverrideRuleChangeActivationThreshold = 1310;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nOverrideMinerConfirmationWindow = 2016;

        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].bit = 8;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nStartTime = 1658055600;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nTimeout = 1659178800;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nOverrideRuleChangeActivationThreshold = 1310;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nOverrideMinerConfirmationWindow = 2016;

        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].bit = 9;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nStartTime = 1658055600;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nTimeout = 1659178800;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nOverrideRuleChangeActivationThreshold = 1411;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nOverrideMinerConfirmationWindow = 2016;

        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].bit = 10;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nStartTime = 1658055600;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nTimeout = 1659178800;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nOverrideRuleChangeActivationThreshold = 1411;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nOverrideMinerConfirmationWindow = 2016;

        consensus.nAuxpowChainId = 9;
        consensus.nAuxpowStartHeight = 46;
        consensus.fStrictChainId = true;
        consensus.nLegacyBlocksBefore = 100;

        consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000000000000000000100010"};
        consensus.defaultAssumeValid = uint256{"000000eaab417d6dfe9bd75119972e1d07ecfe8ff655bef7c2acb3d9a0eeed81"};

        pchMessageStart[0] = 0x6e;
        pchMessageStart[1] = 0x66;
        pchMessageStart[2] = 0x78;
        pchMessageStart[3] = 0x64;
        nDefaultPort = 4569;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 2;
        m_assumed_chain_state_size = 1;

        // Set activation timestamps BEFORE genesis creation.
        nKAWPOWActivationTime = 1661833868;
        ::nKAWPOWActivationTime = 1661833868;
        nMEOWPOWActivationTime = 1707354000;
        ::nMEOWPOWActivationTime = 1707354000;

        genesis = CreateGenesisBlock(1661734222, 7680541, 0x1e00ffff, 4, 5000 * COIN);
        consensus.hashGenesisBlock = genesis.GetX16RHash();
        assert(consensus.hashGenesisBlock == uint256{"000000eaab417d6dfe9bd75119972e1d07ecfe8ff655bef7c2acb3d9a0eeed81"});
        assert(genesis.hashMerkleRoot == uint256{"e8916cf6592c8433d598c3a5fe60a9741fd2a997b39d93af2d789cdd9d9a7390"});

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("seed-testnet-mewc.meowcoin.cc");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,109);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,124);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,114);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tmewc";
        nExtCoinType = 1;

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            .nTime    = 1661734222,
            .tx_count = 0,
            .dTxRate  = 0.0,
        };

        /** Meowcoin testnet asset parameters **/
        nIssueAssetBurnAmount = 500 * COIN;
        nReissueAssetBurnAmount = 100 * COIN;
        nIssueSubAssetBurnAmount = 100 * COIN;
        nIssueUniqueAssetBurnAmount = 5 * COIN;
        nIssueMsgChannelAssetBurnAmount = 100 * COIN;
        nIssueQualifierAssetBurnAmount = 1000 * COIN;
        nIssueSubQualifierAssetBurnAmount = 100 * COIN;
        nIssueRestrictedAssetBurnAmount = 1500 * COIN;
        nAddNullQualifierTagBurnAmount = .1 * COIN;

        nCommunityAutonomousAmount = 15;

        strIssueAssetBurnAddress = "mCissueAssetXXXXXXXXXXXXXXXXauYgzW";
        strReissueAssetBurnAddress = "mCReissueAssetXXXXXXXXXXXXXXViYbet";
        strIssueSubAssetBurnAddress = "mCissueSubAssetXXXXXXXXXXXXXb41gMc";
        strIssueUniqueAssetBurnAddress = "mCissueUniqueAssetXXXXXXXXXXWguvk3";
        strIssueMsgChannelAssetBurnAddress = "mCissueMsgChanneLAssetXXXXXXWPw4W6";
        strIssueQualifierAssetBurnAddress = "mCissueQuaLifierXXXXXXXXXXXXW2VtZy";
        strIssueSubQualifierAssetBurnAddress = "mCissueSubQuaLifierXXXXXXXXXTj8yy8";
        strIssueRestrictedAssetBurnAddress = "mCissueRestrictedXXXXXXXXXXXX3xpZV";
        strAddNullQualifierTagBurnAddress = "mCaddTagBurnXXXXXXXXXXXXXXXXapsVx8";
        strGlobalBurnAddress = "mCBurnXXXXXXXXXXXXXXXXXXXXXXUDUcus";
        strCommunityAutonomousAddress = "m1aNrki9MUb7FshxGiph6rR3eCmS9wU3rw";

        nDGWActivationBlock = 1;
        nMaxReorganizationDepth = 60;
        nMinReorganizationPeers = 4;
        nMinReorganizationAge = 60 * 60 * 12;
        nAssetActivationHeight = 1;
        nMessagingActivationBlock = 1;
        nRestrictedActivationBlock = 1;

        // (activation timestamps already set above, before genesis creation)
    }
};

/**
 * Meowcoin Signet stub — not used, but kept to satisfy ChainType enum.
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        // Meowcoin does not use signet; this is a minimal stub.
        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 2100000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 2016 * 60;
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0x53; // S
        pchMessageStart[1] = 0x49; // I
        pchMessageStart[2] = 0x47; // G
        pchMessageStart[3] = 0x4e; // N
        nDefaultPort = 38788;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        genesis = CreateGenesisBlock(1661730843, 351574, 0x1e00ffff, 4, 5000 * COIN);
        consensus.hashGenesisBlock = genesis.GetX16RHash();

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,109);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,124);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,114);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "smewc";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {};
        chainTxData = ChainTxData{.nTime = 0, .tx_count = 0, .dTxRate = 0};
    }
};

/**
 * Meowcoin regression test.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;

        consensus.nBIP34Enabled = true;
        consensus.nBIP65Enabled = true;
        consensus.nBIP66Enabled = true;
        consensus.nSegwitEnabled = true;
        consensus.nCSVEnabled = true;

        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0; // Always active
        consensus.MinBIP9WarningHeight = 0;

        consensus.powLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::MEOWPOW)] = uint256{"00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.powLimitPerAlgo[static_cast<uint8_t>(PowAlgo::SCRYPT)]  = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};

        consensus.nPowTargetTimespan = 2016 * 60;
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.nLwmaAveragingWindow = 45;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = opts.enforce_bip94;
        consensus.fPowNoRetargeting = true;

        consensus.nRuleChangeActivationThreshold = 108;
        consensus.nMinerConfirmationWindow = 144;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 108;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 144;

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 108;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 144;

        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].bit = 6;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nOverrideRuleChangeActivationThreshold = 108;
        consensus.vDeployments[Consensus::DEPLOYMENT_ASSETS].nOverrideMinerConfirmationWindow = 144;

        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].bit = 7;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nOverrideRuleChangeActivationThreshold = 108;
        consensus.vDeployments[Consensus::DEPLOYMENT_MSG_REST_ASSETS].nOverrideMinerConfirmationWindow = 144;

        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].bit = 8;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nOverrideRuleChangeActivationThreshold = 208;
        consensus.vDeployments[Consensus::DEPLOYMENT_TRANSFER_SCRIPT_SIZE].nOverrideMinerConfirmationWindow = 288;

        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].bit = 9;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nOverrideRuleChangeActivationThreshold = 108;
        consensus.vDeployments[Consensus::DEPLOYMENT_ENFORCE_VALUE].nOverrideMinerConfirmationWindow = 144;

        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].bit = 10;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nOverrideRuleChangeActivationThreshold = 400;
        consensus.vDeployments[Consensus::DEPLOYMENT_COINBASE_ASSETS].nOverrideMinerConfirmationWindow = 500;

        consensus.nAuxpowChainId = 9;
        consensus.nAuxpowStartHeight = 19200;
        consensus.fStrictChainId = true;
        consensus.nLegacyBlocksBefore = 19200;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0x44;
        pchMessageStart[1] = 0x52;
        pchMessageStart[2] = 0x4F;
        pchMessageStart[3] = 0x57;
        nDefaultPort = 18444;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        // Set activation timestamps BEFORE genesis creation.
        nKAWPOWActivationTime = 3582830167;
        ::nKAWPOWActivationTime = 3582830167;
        nMEOWPOWActivationTime = 3582830167;
        ::nMEOWPOWActivationTime = 3582830167;

        genesis = CreateGenesisBlock(1661734578, 1, 0x207fffff, 4, 5000 * COIN);
        consensus.hashGenesisBlock = genesis.GetX16RHash();
        // assert(consensus.hashGenesisBlock == uint256{"0b2c703dc93bb63a36c4e33b85be4855ddbca2ac951a7a0a29b8de0408200a3c"});
        assert(genesis.hashMerkleRoot == uint256{"e8916cf6592c8433d598c3a5fe60a9741fd2a997b39d93af2d789cdd9d9a7390"});

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            .nTime = 0,
            .tx_count = 0,
            .dTxRate = 0.001,
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,42);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,124);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,114);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rmewc";
        nExtCoinType = 1;

        /** Meowcoin regtest asset parameters **/
        nIssueAssetBurnAmount = 500 * COIN;
        nReissueAssetBurnAmount = 100 * COIN;
        nIssueSubAssetBurnAmount = 100 * COIN;
        nIssueUniqueAssetBurnAmount = 5 * COIN;
        nIssueMsgChannelAssetBurnAmount = 100 * COIN;
        nIssueQualifierAssetBurnAmount = 1000 * COIN;
        nIssueSubQualifierAssetBurnAmount = 100 * COIN;
        nIssueRestrictedAssetBurnAmount = 1500 * COIN;
        nAddNullQualifierTagBurnAmount = .1 * COIN;

        nCommunityAutonomousAmount = 10;

        strIssueAssetBurnAddress = "J1VQJKLSLVZ4syiCAx5hEPq8BrkFaxAXAi";
        strReissueAssetBurnAddress = "J2yh4DiLETuVVDvpvBNSq3QCmHcdMmNEdp";
        strIssueSubAssetBurnAddress = "J3PE3FsHqfszvz7nhwK2Gc32wykrc7pNMA";
        strIssueUniqueAssetBurnAddress = "J4yKRTYF2nRryYEnupsNnQQmRKsQhdspYB";
        strIssueMsgChannelAssetBurnAddress = "J58ndjHjLYKHMszr4ehUg9YMWPAiXNEepa";
        strIssueQualifierAssetBurnAddress = "J68wpmVvdE6bMSkiCEDQWCHCKZs4VVdE2G";
        strIssueSubQualifierAssetBurnAddress = "J7MSidYgNJrPE15ouEsXPYXFYH2AAPXmhr";
        strIssueRestrictedAssetBurnAddress = "J8uX8jfZn14P1VNzh6YjSzLaRTQAdoFSHn";
        strAddNullQualifierTagBurnAddress = "J9CrKy8m548AvSbcv1mcn7tyJQkgcwVfj6";
        strGlobalBurnAddress = "JGYQBki6wWWnJLp2dcgdtNZWs9a2e1nXM3";
        strCommunityAutonomousAddress = "JCPncGFawSDgP3CmG19MB6cbKP5XuhXY4u";

        nDGWActivationBlock = 200;
        nMaxReorganizationDepth = 60;
        nMinReorganizationPeers = 4;
        nMinReorganizationAge = 60 * 60 * 12;
        nAssetActivationHeight = 0;
        nMessagingActivationBlock = 0;
        nRestrictedActivationBlock = 0;

        // (activation timestamps already set above, before genesis creation)
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}

// Meowcoin does not use TestNet4; return TestNet as fallback.
std::unique_ptr<const CChainParams> CChainParams::TestNet4()
{
    return std::make_unique<const CTestNetParams>();
}

std::vector<int> CChainParams::GetAvailableSnapshotHeights() const
{
    std::vector<int> heights;
    heights.reserve(m_assumeutxo_data.size());

    for (const auto& data : m_assumeutxo_data) {
        heights.emplace_back(data.height);
    }
    return heights;
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    const auto testnet_msg = CChainParams::TestNet()->MessageStart();
    const auto testnet4_msg = CChainParams::TestNet4()->MessageStart();
    const auto regtest_msg = CChainParams::RegTest({})->MessageStart();
    const auto signet_msg = CChainParams::SigNet({})->MessageStart();

    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    } else if (std::ranges::equal(message, testnet_msg)) {
        return ChainType::TESTNET;
    } else if (std::ranges::equal(message, testnet4_msg)) {
        return ChainType::TESTNET4;
    } else if (std::ranges::equal(message, regtest_msg)) {
        return ChainType::REGTEST;
    } else if (std::ranges::equal(message, signet_msg)) {
        return ChainType::SIGNET;
    }
    return std::nullopt;
}
