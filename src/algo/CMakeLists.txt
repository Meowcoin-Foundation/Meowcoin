# Copyright (c) 2017-2021 The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# SPH hash library used by X16R / X16RV2.

add_library(sph_algo STATIC EXCLUDE_FROM_ALL
  blake.c
  bmw.c
  cubehash.c
  echo.c
  fugue.c
  groestl.c
  hamsi.c
  hamsi_helper.c
  jh.c
  keccak.c
  luffa.c
  sha2.c
  shabal.c
  shavite.c
  simd.c
  skein.c
  sph_sha2big.c
  tiger.cpp
  whirlpool.c
)
target_include_directories(sph_algo
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(sph_algo PRIVATE core_interface)
