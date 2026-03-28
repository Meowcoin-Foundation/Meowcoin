// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/pureheader.h>

#include <crypto/scrypt.h>
#include <hash.h>
#include <serialize.h>
#include <streams.h>
#include <tinyformat.h>

void CBlockVersion::SetBaseVersion(int32_t nBaseVersion, int32_t nChainId)
{
    nVersion = nBaseVersion | (nChainId << VERSION_START_BIT);
}

uint256 CPureBlockHeader::GetHash() const
{
    // Meowcoin: CPureBlockHeader is used for AuxPoW parent blocks
    // (merge-mined Scrypt chains like Litecoin).  The hash must be
    // Scrypt-1024-1-1-256 over the serialized 80-byte header, matching
    // the reference Meowcoin implementation.
    DataStream ss{};
    ss << *this;
    uint256 thash;
    scrypt_1024_1_1_256(reinterpret_cast<const char*>(ss.data()),
                        reinterpret_cast<char*>(thash.data()));
    return thash;
}

std::string CPureBlockHeader::ToString() const
{
    return strprintf(
        "CPureBlockHeader(ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u)",
        nVersion.GetFullVersion(),
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce);
}
