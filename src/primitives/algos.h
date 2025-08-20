#ifndef BITCOIN_PRIMITIVES_ALGOS_H
#define BITCOIN_PRIMITIVES_ALGOS_H

#include <cstdint>

enum class PowAlgo : long unsigned int
{
    MEOWPOW = 0,
    SCRYPT = 1,
    NUM_ALGOS
};

#endif // BITCOIN_PRIMITIVES_ALGOS_H