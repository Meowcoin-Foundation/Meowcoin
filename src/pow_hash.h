// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_HASH_H
#define BITCOIN_POW_HASH_H

/**
 * Meowcoin PoW hash function declarations.
 *
 * These are stubs that return SHA256d for now.  Replace with real
 * implementations once the ethash / sphlib libraries are vendored.
 */

#include <primitives/pureheader.h>
#include <uint256.h>

#include <cstdint>
#include <span>

class CBlockHeader;

/**
 * X16R hash: 16-round chained hash function where the algorithm for each
 * round is selected by a nibble of hashPrevBlock.
 *
 * TODO: Port sphlib algorithms from meowcoin/src/algo/.
 */
uint256 HashX16R(const unsigned char* pbegin, const unsigned char* pend,
                 const uint256& hashPrevBlock);

/**
 * X16RV2 hash: Updated variant of X16R with tiger pre-hash on certain rounds.
 *
 * TODO: Port sphlib algorithms from meowcoin/src/algo/.
 */
uint256 HashX16RV2(const unsigned char* pbegin, const unsigned char* pend,
                   const uint256& hashPrevBlock);

/**
 * Full KAWPOW ProgPow hash (creates epoch context, returns final hash).
 * @param blockHeader The block header.
 * @param[out] mix_hash The mix hash output.
 * @return The final KAWPOW hash.
 *
 * TODO: Port ethash/progpow from meowcoin/src/crypto/ethash/.
 */
uint256 KAWPOWHash(const CBlockHeader& blockHeader, uint256& mix_hash);

/**
 * KAWPOW hash that only verifies using the mix_hash already in the header.
 * Does not require a full epoch context.
 * @param blockHeader The block header (must contain valid mix_hash).
 * @return The verification hash.
 */
uint256 KAWPOWHash_OnlyMix(const CBlockHeader& blockHeader);

/**
 * Full MEOWPOW ProgPow hash.
 * @param blockHeader The block header.
 * @param[out] mix_hash The mix hash output.
 * @return The final MEOWPOW hash.
 *
 * TODO: Port ethash/meowpow from meowcoin/src/crypto/ethash/.
 */
uint256 MEOWPOWHash(const CBlockHeader& blockHeader, uint256& mix_hash);

/**
 * MEOWPOW hash that only verifies using the mix_hash already in the header.
 * @param blockHeader The block header (must contain valid mix_hash).
 * @return The verification hash.
 */
uint256 MEOWPOWHash_OnlyMix(const CBlockHeader& blockHeader);

#endif // BITCOIN_POW_HASH_H
