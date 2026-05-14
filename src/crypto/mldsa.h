// Copyright (c) 2026 ALENOC <https://github.com/ALENOC>
// Copyright (c) 2024-present The Avian Core developers
// Portions Copyright (c) 2026 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

// ML-DSA-44 (FIPS 204) constants and interface.
// Ported from Ravencoin and Avian post-quantum work.

#ifndef BITCOIN_CRYPTO_MLDSA_H
#define BITCOIN_CRYPTO_MLDSA_H

#include <cstddef>
#include <cstdint>
#include <span>

namespace mldsa {

static constexpr size_t PUBKEY_SIZE    = 1312;
static constexpr size_t SECRETKEY_SIZE = 2560;
static constexpr size_t SIG_SIZE       = 2420;
static constexpr size_t SEED_SIZE      = 32;

/** Generate a keypair deterministically from a 32-byte seed. */
bool KeyGenFromSeed(std::span<uint8_t, PUBKEY_SIZE>  pubkey,
                    std::span<uint8_t, SECRETKEY_SIZE> seckey,
                    std::span<const uint8_t, SEED_SIZE> seed);

/** Generate a keypair with OS randomness. */
bool KeyGenRandom(std::span<uint8_t, PUBKEY_SIZE>  pubkey,
                  std::span<uint8_t, SECRETKEY_SIZE> seckey);

/** Sign msg with seckey, writing SIG_SIZE bytes into sig. */
bool Sign(std::span<uint8_t, SIG_SIZE>     sig,
          std::span<const uint8_t>          msg,
          std::span<const uint8_t, SECRETKEY_SIZE> seckey);

/** Verify sig over msg with pubkey. */
bool Verify(std::span<const uint8_t, SIG_SIZE>     sig,
            std::span<const uint8_t>                msg,
            std::span<const uint8_t, PUBKEY_SIZE>   pubkey);

} // namespace mldsa

#endif // BITCOIN_CRYPTO_MLDSA_H
