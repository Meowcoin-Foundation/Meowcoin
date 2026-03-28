// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Real implementations of Meowcoin PoW hash functions.
 *
 * X16R / X16RV2  — 16-round chained SPH-512 hash, order selected by prevhash.
 * KAWPOW         — ProgPow (ethash-based, GPU-targeted).
 * MEOWPOW        — MeowPow (custom ProgPow fork).
 */

#include <pow_hash.h>

#include <hash.h>
#include <primitives/block.h>

#include <cassert>
#include <cstring>

// SPH hash library (from algo/ directory)
#include <algo/sph_blake.h>
#include <algo/sph_bmw.h>
#include <algo/sph_groestl.h>
#include <algo/sph_jh.h>
#include <algo/sph_keccak.h>
#include <algo/sph_skein.h>
#include <algo/sph_luffa.h>
#include <algo/sph_cubehash.h>
#include <algo/sph_shavite.h>
#include <algo/sph_simd.h>
#include <algo/sph_echo.h>
#include <algo/sph_hamsi.h>
#include <algo/sph_fugue.h>
#include <algo/sph_shabal.h>
#include <algo/sph_whirlpool.h>
#include <algo/sph_sha2.h>
#include <algo/sph_tiger.h>

// Ethash / ProgPow / MeowPow
#include <crypto/ethash/include/ethash/ethash.hpp>
#include <crypto/ethash/include/ethash/progpow.hpp>
#include <crypto/ethash/include/ethash/meowpow.hpp>
#include <crypto/ethash/helpers.hpp>

// ---------------------------------------------------------------------------
// Helper: select hash algorithm index from a nibble of hashPrevBlock
// ---------------------------------------------------------------------------

static inline int GetHashSelection(const uint256& PrevBlockHash, int index)
{
    assert(index >= 0 && index < 16);
    return PrevBlockHash.GetNibble(48 + index);
}

// ---------------------------------------------------------------------------
// X16R — 16-round chained hash, algorithm per round selected by prev hash
// ---------------------------------------------------------------------------

uint256 HashX16R(const unsigned char* pbegin, const unsigned char* pend,
                 const uint256& hashPrevBlock)
{
    sph_blake512_context     ctx_blake;
    sph_bmw512_context       ctx_bmw;
    sph_groestl512_context   ctx_groestl;
    sph_jh512_context        ctx_jh;
    sph_keccak512_context    ctx_keccak;
    sph_skein512_context     ctx_skein;
    sph_luffa512_context     ctx_luffa;
    sph_cubehash512_context  ctx_cubehash;
    sph_shavite512_context   ctx_shavite;
    sph_simd512_context      ctx_simd;
    sph_echo512_context      ctx_echo;
    sph_hamsi512_context     ctx_hamsi;
    sph_fugue512_context     ctx_fugue;
    sph_shabal512_context    ctx_shabal;
    sph_whirlpool_context    ctx_whirlpool;
    sph_sha512_context       ctx_sha512;

    static unsigned char pblank[1];
    unsigned char hash[16][64] = {}; // Zero-init for safety (matches X16RV2 fix)

    for (int i = 0; i < 16; i++)
    {
        const void* toHash;
        int lenToHash;
        if (i == 0) {
            toHash = (pbegin == pend ? pblank : static_cast<const void*>(pbegin));
            lenToHash = static_cast<int>(pend - pbegin);
        } else {
            toHash = static_cast<const void*>(&hash[i - 1]);
            lenToHash = 64;
        }

        int sel = GetHashSelection(hashPrevBlock, i);
        switch (sel) {
        case 0:
            sph_blake512_init(&ctx_blake);
            sph_blake512(&ctx_blake, toHash, lenToHash);
            sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[i]));
            break;
        case 1:
            sph_bmw512_init(&ctx_bmw);
            sph_bmw512(&ctx_bmw, toHash, lenToHash);
            sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[i]));
            break;
        case 2:
            sph_groestl512_init(&ctx_groestl);
            sph_groestl512(&ctx_groestl, toHash, lenToHash);
            sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[i]));
            break;
        case 3:
            sph_jh512_init(&ctx_jh);
            sph_jh512(&ctx_jh, toHash, lenToHash);
            sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[i]));
            break;
        case 4:
            sph_keccak512_init(&ctx_keccak);
            sph_keccak512(&ctx_keccak, toHash, lenToHash);
            sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[i]));
            break;
        case 5:
            sph_skein512_init(&ctx_skein);
            sph_skein512(&ctx_skein, toHash, lenToHash);
            sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[i]));
            break;
        case 6:
            sph_luffa512_init(&ctx_luffa);
            sph_luffa512(&ctx_luffa, toHash, lenToHash);
            sph_luffa512_close(&ctx_luffa, static_cast<void*>(&hash[i]));
            break;
        case 7:
            sph_cubehash512_init(&ctx_cubehash);
            sph_cubehash512(&ctx_cubehash, toHash, lenToHash);
            sph_cubehash512_close(&ctx_cubehash, static_cast<void*>(&hash[i]));
            break;
        case 8:
            sph_shavite512_init(&ctx_shavite);
            sph_shavite512(&ctx_shavite, toHash, lenToHash);
            sph_shavite512_close(&ctx_shavite, static_cast<void*>(&hash[i]));
            break;
        case 9:
            sph_simd512_init(&ctx_simd);
            sph_simd512(&ctx_simd, toHash, lenToHash);
            sph_simd512_close(&ctx_simd, static_cast<void*>(&hash[i]));
            break;
        case 10:
            sph_echo512_init(&ctx_echo);
            sph_echo512(&ctx_echo, toHash, lenToHash);
            sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[i]));
            break;
        case 11:
            sph_hamsi512_init(&ctx_hamsi);
            sph_hamsi512(&ctx_hamsi, toHash, lenToHash);
            sph_hamsi512_close(&ctx_hamsi, static_cast<void*>(&hash[i]));
            break;
        case 12:
            sph_fugue512_init(&ctx_fugue);
            sph_fugue512(&ctx_fugue, toHash, lenToHash);
            sph_fugue512_close(&ctx_fugue, static_cast<void*>(&hash[i]));
            break;
        case 13:
            sph_shabal512_init(&ctx_shabal);
            sph_shabal512(&ctx_shabal, toHash, lenToHash);
            sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[i]));
            break;
        case 14:
            sph_whirlpool_init(&ctx_whirlpool);
            sph_whirlpool(&ctx_whirlpool, toHash, lenToHash);
            sph_whirlpool_close(&ctx_whirlpool, static_cast<void*>(&hash[i]));
            break;
        case 15:
            sph_sha512_init(&ctx_sha512);
            sph_sha512(&ctx_sha512, toHash, lenToHash);
            sph_sha512_close(&ctx_sha512, static_cast<void*>(&hash[i]));
            break;
        }
    }

    uint256 result;
    std::memcpy(result.data(), hash[15], 32);
    return result;
}

// ---------------------------------------------------------------------------
// X16RV2 — Updated X16R: slots 4, 6, 15 chain Tiger hash before main hash
// ---------------------------------------------------------------------------

uint256 HashX16RV2(const unsigned char* pbegin, const unsigned char* pend,
                   const uint256& hashPrevBlock)
{
    sph_blake512_context     ctx_blake;
    sph_bmw512_context       ctx_bmw;
    sph_groestl512_context   ctx_groestl;
    sph_jh512_context        ctx_jh;
    sph_keccak512_context    ctx_keccak;
    sph_skein512_context     ctx_skein;
    sph_luffa512_context     ctx_luffa;
    sph_cubehash512_context  ctx_cubehash;
    sph_shavite512_context   ctx_shavite;
    sph_simd512_context      ctx_simd;
    sph_echo512_context      ctx_echo;
    sph_hamsi512_context     ctx_hamsi;
    sph_fugue512_context     ctx_fugue;
    sph_shabal512_context    ctx_shabal;
    sph_whirlpool_context    ctx_whirlpool;
    sph_sha512_context       ctx_sha512;
    sph_tiger_context        ctx_tiger;

    static unsigned char pblank[1];
    unsigned char hash[16][64] = {}; // Must be zero-initialized: Tiger (cases 4,6,15) writes
                                     // only 24 bytes but the subsequent 512-bit hash reads 64.

    for (int i = 0; i < 16; i++)
    {
        const void* toHash;
        int lenToHash;
        if (i == 0) {
            toHash = (pbegin == pend ? pblank : static_cast<const void*>(pbegin));
            lenToHash = static_cast<int>(pend - pbegin);
        } else {
            toHash = static_cast<const void*>(&hash[i - 1]);
            lenToHash = 64;
        }

        int sel = GetHashSelection(hashPrevBlock, i);
        switch (sel) {
        case 0:
            sph_blake512_init(&ctx_blake);
            sph_blake512(&ctx_blake, toHash, lenToHash);
            sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[i]));
            break;
        case 1:
            sph_bmw512_init(&ctx_bmw);
            sph_bmw512(&ctx_bmw, toHash, lenToHash);
            sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[i]));
            break;
        case 2:
            sph_groestl512_init(&ctx_groestl);
            sph_groestl512(&ctx_groestl, toHash, lenToHash);
            sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[i]));
            break;
        case 3:
            sph_jh512_init(&ctx_jh);
            sph_jh512(&ctx_jh, toHash, lenToHash);
            sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[i]));
            break;
        case 4:
            // X16RV2: Tiger pre-hash before Keccak
            sph_tiger_init(&ctx_tiger);
            sph_tiger(&ctx_tiger, toHash, lenToHash);
            sph_tiger_close(&ctx_tiger, static_cast<void*>(&hash[i]));
            sph_keccak512_init(&ctx_keccak);
            sph_keccak512(&ctx_keccak, static_cast<const void*>(&hash[i]), 64);
            sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[i]));
            break;
        case 5:
            sph_skein512_init(&ctx_skein);
            sph_skein512(&ctx_skein, toHash, lenToHash);
            sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[i]));
            break;
        case 6:
            // X16RV2: Tiger pre-hash before Luffa
            sph_tiger_init(&ctx_tiger);
            sph_tiger(&ctx_tiger, toHash, lenToHash);
            sph_tiger_close(&ctx_tiger, static_cast<void*>(&hash[i]));
            sph_luffa512_init(&ctx_luffa);
            sph_luffa512(&ctx_luffa, static_cast<const void*>(&hash[i]), 64);
            sph_luffa512_close(&ctx_luffa, static_cast<void*>(&hash[i]));
            break;
        case 7:
            sph_cubehash512_init(&ctx_cubehash);
            sph_cubehash512(&ctx_cubehash, toHash, lenToHash);
            sph_cubehash512_close(&ctx_cubehash, static_cast<void*>(&hash[i]));
            break;
        case 8:
            sph_shavite512_init(&ctx_shavite);
            sph_shavite512(&ctx_shavite, toHash, lenToHash);
            sph_shavite512_close(&ctx_shavite, static_cast<void*>(&hash[i]));
            break;
        case 9:
            sph_simd512_init(&ctx_simd);
            sph_simd512(&ctx_simd, toHash, lenToHash);
            sph_simd512_close(&ctx_simd, static_cast<void*>(&hash[i]));
            break;
        case 10:
            sph_echo512_init(&ctx_echo);
            sph_echo512(&ctx_echo, toHash, lenToHash);
            sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[i]));
            break;
        case 11:
            sph_hamsi512_init(&ctx_hamsi);
            sph_hamsi512(&ctx_hamsi, toHash, lenToHash);
            sph_hamsi512_close(&ctx_hamsi, static_cast<void*>(&hash[i]));
            break;
        case 12:
            sph_fugue512_init(&ctx_fugue);
            sph_fugue512(&ctx_fugue, toHash, lenToHash);
            sph_fugue512_close(&ctx_fugue, static_cast<void*>(&hash[i]));
            break;
        case 13:
            sph_shabal512_init(&ctx_shabal);
            sph_shabal512(&ctx_shabal, toHash, lenToHash);
            sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[i]));
            break;
        case 14:
            sph_whirlpool_init(&ctx_whirlpool);
            sph_whirlpool(&ctx_whirlpool, toHash, lenToHash);
            sph_whirlpool_close(&ctx_whirlpool, static_cast<void*>(&hash[i]));
            break;
        case 15:
            // X16RV2: Tiger pre-hash before SHA-512
            sph_tiger_init(&ctx_tiger);
            sph_tiger(&ctx_tiger, toHash, lenToHash);
            sph_tiger_close(&ctx_tiger, static_cast<void*>(&hash[i]));
            sph_sha512_init(&ctx_sha512);
            sph_sha512(&ctx_sha512, static_cast<const void*>(&hash[i]), 64);
            sph_sha512_close(&ctx_sha512, static_cast<void*>(&hash[i]));
            break;
        }
    }

    uint256 result;
    std::memcpy(result.data(), hash[15], 32);
    return result;
}

// ---------------------------------------------------------------------------
// KAWPOW — ProgPow (ethash-based)
// ---------------------------------------------------------------------------

uint256 KAWPOWHash(const CBlockHeader& blockHeader, uint256& mix_hash)
{
    static ethash::epoch_context_ptr context{nullptr, nullptr};

    const auto epoch_number = ethash::get_epoch_number(blockHeader.nHeight);

    if (!context || context->epoch_number != epoch_number)
        context = ethash::create_epoch_context(epoch_number);

    uint256 nHeaderHash = blockHeader.GetKAWPOWHeaderHash();
    const auto header_hash = to_hash256(nHeaderHash.GetHex());

    const auto result = progpow::hash(*context, blockHeader.nHeight, header_hash, blockHeader.nNonce64);

    mix_hash = uint256::FromHex(to_hex(result.mix_hash)).value_or(uint256{});
    return uint256::FromHex(to_hex(result.final_hash)).value_or(uint256{});
}

uint256 KAWPOWHash_OnlyMix(const CBlockHeader& blockHeader)
{
    uint256 nHeaderHash = blockHeader.GetKAWPOWHeaderHash();
    const auto header_hash = to_hash256(nHeaderHash.GetHex());

    const auto result = progpow::hash_no_verify(blockHeader.nHeight, header_hash,
                                                  to_hash256(blockHeader.mix_hash.GetHex()),
                                                  blockHeader.nNonce64);

    return uint256::FromHex(to_hex(result)).value_or(uint256{});
}

// ---------------------------------------------------------------------------
// MEOWPOW — Custom ProgPow fork
// ---------------------------------------------------------------------------

uint256 MEOWPOWHash(const CBlockHeader& blockHeader, uint256& mix_hash)
{
    static ethash::epoch_context_ptr context{nullptr, nullptr};

    const auto epoch_number = ethash::get_epoch_number(blockHeader.nHeight);

    if (!context || context->epoch_number != epoch_number)
        context = ethash::create_epoch_context(epoch_number);

    uint256 nHeaderHash = blockHeader.GetMEOWPOWHeaderHash();
    const auto header_hash = to_hash256(nHeaderHash.GetHex());

    const auto result = meowpow::hash(*context, blockHeader.nHeight, header_hash, blockHeader.nNonce64);

    mix_hash = uint256::FromHex(to_hex(result.mix_hash)).value_or(uint256{});
    return uint256::FromHex(to_hex(result.final_hash)).value_or(uint256{});
}

uint256 MEOWPOWHash_OnlyMix(const CBlockHeader& blockHeader)
{
    uint256 nHeaderHash = blockHeader.GetMEOWPOWHeaderHash();
    const auto header_hash = to_hash256(nHeaderHash.GetHex());

    const auto result = meowpow::hash_no_verify(blockHeader.nHeight, header_hash,
                                                  to_hash256(blockHeader.mix_hash.GetHex()),
                                                  blockHeader.nNonce64);

    return uint256::FromHex(to_hex(result)).value_or(uint256{});}

