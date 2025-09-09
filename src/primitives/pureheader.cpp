// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/pureheader.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"
#include "crypto/scrypt.h"

void CBlockVersion::SetBaseVersion(int32_t nBaseVersion, int32_t nChainId)
{
    const int32_t withoutTopMask = nBaseVersion & ~VERSIONAUXPOW_TOP_MASK;
    assert(withoutTopMask >= 0 && withoutTopMask < VERSION_CHAIN_START);
    assert(!IsAuxpow());
    nVersion = nBaseVersion | (nChainId << VERSION_START_BIT);
}


uint256 CPureBlockHeader::GetHash() const
{
    uint256 thash;
    scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));
    return thash;
}

std::string CPureBlockHeader::ToString() const
{
    std::stringstream s;
    s << strprintf("CPureBlockHeader(ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u)\n",
                   nVersion.GetBaseVersion(),
                   hashPrevBlock.ToString(),
                   hashMerkleRoot.ToString(),
                   nTime, nBits, nNonce);
    return s.str();
}
