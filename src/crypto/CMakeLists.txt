# Copyright (c) 2023-present The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

add_library(meowcoin_crypto STATIC EXCLUDE_FROM_ALL
  aes.cpp
  chacha20.cpp
  chacha20poly1305.cpp
  hex_base.cpp
  hkdf_sha256_32.cpp
  hmac_sha256.cpp
  hmac_sha512.cpp
  muhash.cpp
  poly1305.cpp
  ripemd160.cpp
  scrypt.cpp
  sha1.cpp
  sha256.cpp
  sha256_sse4.cpp
  sha3.cpp
  sha512.cpp
  siphash.cpp
  ../support/cleanse.cpp
)

target_link_libraries(meowcoin_crypto
  PRIVATE
    core_interface
)

if(HAVE_SSE41)
  target_compile_definitions(meowcoin_crypto PRIVATE ENABLE_SSE41)
  target_sources(meowcoin_crypto PRIVATE sha256_sse41.cpp)
  set_property(SOURCE sha256_sse41.cpp PROPERTY
    COMPILE_OPTIONS ${SSE41_CXXFLAGS}
  )
endif()

if(HAVE_AVX2)
  target_compile_definitions(meowcoin_crypto PRIVATE ENABLE_AVX2)
  target_sources(meowcoin_crypto PRIVATE sha256_avx2.cpp)
  set_property(SOURCE sha256_avx2.cpp PROPERTY
    COMPILE_OPTIONS ${AVX2_CXXFLAGS}
  )
endif()

if(HAVE_SSE41 AND HAVE_X86_SHANI)
  target_compile_definitions(meowcoin_crypto PRIVATE ENABLE_SSE41 ENABLE_X86_SHANI)
  target_sources(meowcoin_crypto PRIVATE sha256_x86_shani.cpp)
  set_property(SOURCE sha256_x86_shani.cpp PROPERTY
    COMPILE_OPTIONS ${SSE41_CXXFLAGS} ${X86_SHANI_CXXFLAGS}
  )
endif()

if(HAVE_ARM_SHANI)
  target_compile_definitions(meowcoin_crypto PRIVATE ENABLE_ARM_SHANI)
  target_sources(meowcoin_crypto PRIVATE sha256_arm_shani.cpp)
  set_property(SOURCE sha256_arm_shani.cpp PROPERTY
    COMPILE_OPTIONS ${ARM_SHANI_CXXFLAGS}
  )
endif()

# ---------------------------------------------------------------------------
# Ethash / ProgPow / MeowPow library
# ---------------------------------------------------------------------------
add_library(ethash STATIC EXCLUDE_FROM_ALL
  ethash/lib/ethash/ethash.cpp
  ethash/lib/ethash/managed.cpp
  ethash/lib/ethash/primes.c
  ethash/lib/ethash/progpow.cpp
  ethash/lib/ethash/meowpow.cpp
  ethash/lib/keccak/keccak.c
  ethash/lib/keccak/keccakf800.c
  ethash/lib/keccak/keccakf1600.c
)
target_include_directories(ethash
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ethash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ethash/lib/ethash
    ${CMAKE_CURRENT_SOURCE_DIR}/ethash/lib/support
    ${CMAKE_CURRENT_SOURCE_DIR}/ethash/lib/keccak
    ${CMAKE_CURRENT_SOURCE_DIR}/ethash
)
target_link_libraries(ethash PRIVATE core_interface)
