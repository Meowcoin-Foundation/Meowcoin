// Copyright (c) 2017-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_ALGOS_H
#define BITCOIN_PRIMITIVES_ALGOS_H

#include <cstdint>

/** Proof-of-work algorithm identifiers for Meowcoin's multi-algo mining. */
enum class PowAlgo : uint8_t
{
    MEOWPOW = 0,   //!< Native PoW (KAWPOW → MEOWPOW lineage)
    SCRYPT  = 1,    //!< AuxPoW / merge-mined algo
    NUM_ALGOS
};

#endif // BITCOIN_PRIMITIVES_ALGOS_H
