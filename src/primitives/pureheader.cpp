// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/pureheader.h"

#include "hash.h"
#include "utilstrencodings.h"
#include "tinyformat.h"
#include "crypto/common.h"
#include "crypto/scrypt.h"

static const uint32_t MAINNET_X16RV2ACTIVATIONTIME = 1569945600;
static const uint32_t TESTNET_X16RV2ACTIVATIONTIME = 1567533600;
static const uint32_t REGTEST_X16RV2ACTIVATIONTIME = 1569931200;

uint32_t nKAWPOWActivationTime;
uint32_t nMEOWPOWActivationTime;

BlockNetwork bNetwork = BlockNetwork();

BlockNetwork::BlockNetwork()
{
    fOnTestnet = false;
    fOnRegtest = false;
}

void BlockNetwork::SetNetwork(const std::string& net)
{
    if (net == "test") {
        fOnTestnet = true;
    } else if (net == "regtest") {
        fOnRegtest = true;
    }
}

void CPureBlockHeader::SetBaseVersion(int32_t nBaseVersion, int32_t nChainId)
{
    assert(nBaseVersion >= 1 && nBaseVersion < VERSION_AUXPOW);
    assert(!IsAuxpow());
    nVersion = nBaseVersion | (nChainId * VERSION_CHAIN_START);
}

uint256 CPureBlockHeader::GetHash() const
{
    if (nTime < nKAWPOWActivationTime) {
        uint32_t nTimeToUse = MAINNET_X16RV2ACTIVATIONTIME;
        if (bNetwork.fOnTestnet) {
            nTimeToUse = TESTNET_X16RV2ACTIVATIONTIME;
        } else if (bNetwork.fOnRegtest) {
            nTimeToUse = REGTEST_X16RV2ACTIVATIONTIME;
        }
        if (nTime >= nTimeToUse) {
            return HashX16RV2(BEGIN(nVersion), END(nNonce), hashPrevBlock);
        }
        else {
            return HashX16R(BEGIN(nVersion), END(nNonce), hashPrevBlock);
        }
    }
    
    if (nTime < nMEOWPOWActivationTime) {
        return KAWPOWHash_OnlyMix(*this);
    } else {
        if (IsAuxpow()) {
            return GetAuxPowHash();
        }
        return MEOWPOWHash_OnlyMix(*this); //MEOWPOW to engage as the primary algo
    }
}

uint256 CPureBlockHeader::GetHashFull(uint256& mix_hash) const
{
    if (nTime < nKAWPOWActivationTime) {
        uint32_t nTimeToUse = MAINNET_X16RV2ACTIVATIONTIME;
        if (bNetwork.fOnTestnet) {
            nTimeToUse = TESTNET_X16RV2ACTIVATIONTIME;
        } else if (bNetwork.fOnRegtest) {
            nTimeToUse = REGTEST_X16RV2ACTIVATIONTIME;
        }
        if (nTime >= nTimeToUse) {
            return HashX16RV2(BEGIN(nVersion), END(nNonce), hashPrevBlock);
        }

        return HashX16R(BEGIN(nVersion), END(nNonce), hashPrevBlock);
    } if (nTime < nMEOWPOWActivationTime) {
        return KAWPOWHash(*this, mix_hash);
    } else {
        return MEOWPOWHash(*this, mix_hash); //MEOWPOW to engage as the primary algo
    }
}

uint256 CPureBlockHeader::GetX16RHash() const
{
    return HashX16R(BEGIN(nVersion), END(nNonce), hashPrevBlock);
}

uint256 CPureBlockHeader::GetX16RV2Hash() const
{
    return HashX16RV2(BEGIN(nVersion), END(nNonce), hashPrevBlock);
}

/**
 * @brief This takes a block header, removes the nNonce64 and the mixHash. Then performs a serialized hash of it SHA256D.
 * This will be used as the input to the KAAAWWWPOW hashing function
 * @note Only to be called and used on KAAAWWWPOW block headers
 */
uint256 CPureBlockHeader::GetKAWPOWHeaderHash() const
{
    CKAWPOWInput input{*this};

    return SerializeHash(input);
}

uint256 CPureBlockHeader::GetMEOWPOWHeaderHash() const
{
    CMEOWPOWInput input{*this};

    return SerializeHash(input);
}

uint256 CPureBlockHeader::GetAuxPowHash() const
{
    uint256 thash;
    scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));
    return thash;
}