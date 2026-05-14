# Copyright (c) 2023-present The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

add_executable(bench_meowcoin
  bench_meowcoin.cpp
  bench.cpp
  nanobench.cpp
# Benchmarks:
  addrman.cpp
  base58.cpp
  bech32.cpp
  bip324_ecdh.cpp
  block_assemble.cpp
  blockencodings.cpp
  ccoins_caching.cpp
  chacha20.cpp
  checkblock.cpp
  checkblockindex.cpp
  checkqueue.cpp
  cluster_linearize.cpp
  connectblock.cpp
  crypto_hash.cpp
  descriptors.cpp
  disconnected_transactions.cpp
  duplicate_inputs.cpp
  ellswift.cpp
  examples.cpp
  gcs_filter.cpp
  hashpadding.cpp
  index_blockfilter.cpp
  load_external.cpp
  lockedpool.cpp
  logging.cpp
  mempool_ephemeral_spends.cpp
  mempool_eviction.cpp
  mempool_stress.cpp
  merkle_root.cpp
  obfuscation.cpp
  parse_hex.cpp
  peer_eviction.cpp
  poly1305.cpp
  pool.cpp
  prevector.cpp
  random.cpp
  readwriteblock.cpp
  rollingbloom.cpp
  rpc_blockchain.cpp
  rpc_mempool.cpp
  sign_transaction.cpp
  streams_findbyte.cpp
  strencodings.cpp
  txgraph.cpp
  txorphanage.cpp
  util_time.cpp
  verify_script.cpp
)

include(TargetDataSources)
target_raw_data_sources(bench_meowcoin NAMESPACE benchmark::data
  data/block413567.raw
)

target_link_libraries(bench_meowcoin
  core_interface
  test_util
  meowcoin_node
  Boost::headers
)

if(ENABLE_WALLET)
  target_sources(bench_meowcoin
    PRIVATE
      coin_selection.cpp
      wallet_balance.cpp
      wallet_create.cpp
      wallet_create_tx.cpp
      wallet_loading.cpp
      wallet_ismine.cpp
      wallet_migration.cpp
  )
  target_link_libraries(bench_meowcoin meowcoin_wallet)
  # bitcoin_assets (via wallet) needs CDBWrapper from meowcoin_node; repeat node after assets.
  target_link_libraries(bench_meowcoin bitcoin_assets meowcoin_node)
endif()

add_test(NAME bench_sanity_check
  COMMAND bench_meowcoin -sanity-check
)

install_binary_component(bench_meowcoin INTERNAL)
