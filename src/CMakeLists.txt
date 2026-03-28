# Copyright (c) 2023-present The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(AddWindowsResources)

configure_file(${PROJECT_SOURCE_DIR}/cmake/meowcoin-build-config.h.in meowcoin-build-config.h USE_SOURCE_PERMISSIONS @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

#=============================
# Subprojects
#=============================
# Subprojects include subdirectories that do or could have tests
# and/or benchmark binaries, such as all subtrees and univalue.
# These need to be included before CMAKE_*_OUTPUT_DIRECTORY variables
# are set, so output locations of subproject tests and libraries are
# not overridden.
include(../cmake/crc32c.cmake)
include(../cmake/leveldb.cmake)
include(../cmake/minisketch.cmake)
add_subdirectory(univalue)
if (ENABLE_IPC AND NOT WITH_EXTERNAL_LIBMULTIPROCESS)
  include(../cmake/libmultiprocess.cmake)
  add_libmultiprocess(ipc/libmultiprocess)
endif()
include(../cmake/secp256k1.cmake)
add_secp256k1(secp256k1)
add_subdirectory(algo)

# Set top-level target output locations.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()

add_custom_target(generate_build_info
  BYPRODUCTS ${PROJECT_BINARY_DIR}/src/meowcoin-build-info.h
  COMMAND ${CMAKE_COMMAND} -DBUILD_INFO_HEADER_PATH=${PROJECT_BINARY_DIR}/src/meowcoin-build-info.h -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/cmake/script/GenerateBuildInfo.cmake
  COMMENT "Generating meowcoin-build-info.h"
  VERBATIM
)
add_library(meowcoin_clientversion STATIC EXCLUDE_FROM_ALL
  clientversion.cpp
)
target_link_libraries(meowcoin_clientversion
  PRIVATE
    core_interface
)
add_dependencies(meowcoin_clientversion generate_build_info)

add_subdirectory(assets)
add_subdirectory(crypto)
add_subdirectory(util)
if(ENABLE_IPC)
  add_subdirectory(ipc)
endif()

add_library(meowcoin_consensus STATIC EXCLUDE_FROM_ALL
  arith_uint256.cpp
  auxpow.cpp
  consensus/merkle.cpp
  consensus/tx_check.cpp
  hash.cpp
  pow_hash.cpp
  primitives/block.cpp
  primitives/pureheader.cpp
  primitives/transaction.cpp
  pubkey.cpp
  script/interpreter.cpp
  script/script.cpp
  script/script_error.cpp
  uint256.cpp
)
target_link_libraries(meowcoin_consensus
  PRIVATE
    core_interface
    meowcoin_crypto
    secp256k1
    ethash
    sph_algo
)

if(WITH_ZMQ)
  add_subdirectory(zmq)
endif()

# Home for common functionality shared by different executables and libraries.
# Similar to `meowcoin_util` library, but higher-level.
add_library(meowcoin_common STATIC EXCLUDE_FROM_ALL
  addresstype.cpp
  base58.cpp
  bech32.cpp
  chain.cpp
  chainparams.cpp
  chainparamsbase.cpp
  coins.cpp
  common/args.cpp
  common/bloom.cpp
  common/config.cpp
  common/init.cpp
  common/interfaces.cpp
  common/messages.cpp
  common/netif.cpp
  common/pcp.cpp
  common/run_command.cpp
  common/settings.cpp
  common/signmessage.cpp
  common/system.cpp
  common/url.cpp
  compressor.cpp
  core_read.cpp
  core_write.cpp
  deploymentinfo.cpp
  external_signer.cpp
  init/common.cpp
  kernel/chainparams.cpp
  key.cpp
  key_io.cpp
  merkleblock.cpp
  musig.cpp
  net_permissions.cpp
  net_types.cpp
  netaddress.cpp
  netbase.cpp
  outputtype.cpp
  policy/feerate.cpp
  policy/policy.cpp
  pow.cpp
  protocol.cpp
  psmt.cpp
  rpc/rawtransaction_util.cpp
  rpc/request.cpp
  rpc/util.cpp
  scheduler.cpp
  script/descriptor.cpp
  script/miniscript.cpp
  script/parsing.cpp
  script/sign.cpp
  script/signingprovider.cpp
  script/solver.cpp
)
target_link_libraries(meowcoin_common
  PRIVATE
    core_interface
    meowcoin_consensus
    meowcoin_util
    univalue
    secp256k1
    ethash
    sph_algo
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

include(InstallBinaryComponent)

if(ENABLE_WALLET)
  add_subdirectory(wallet)

  if(BUILD_WALLET_TOOL)
    add_executable(meowcoin-wallet
      meowcoin-wallet.cpp
      init/meowcoin-wallet.cpp
      wallet/wallettool.cpp
    )
    add_windows_resources(meowcoin-wallet meowcoin-wallet-res.rc)
    add_windows_application_manifest(meowcoin-wallet)
    target_link_libraries(meowcoin-wallet
      core_interface
      meowcoin_wallet
      meowcoin_common
      meowcoin_util
      meowcoin_node
      bitcoin_assets
      meowcoin_node
      Boost::headers
    )
    install_binary_component(meowcoin-wallet HAS_MANPAGE)
  endif()
endif()


# P2P and RPC server functionality used by `meowcoind` and `meowcoin-qt` executables.
add_library(meowcoin_node STATIC EXCLUDE_FROM_ALL
  addrdb.cpp
  addrman.cpp
  banman.cpp
  bip324.cpp
  blockencodings.cpp
  blockfilter.cpp
  consensus/tx_verify.cpp
  dbwrapper.cpp
  deploymentstatus.cpp
  flatfile.cpp
  headerssync.cpp
  httprpc.cpp
  httpserver.cpp
  i2p.cpp
  index/base.cpp
  index/blockfilterindex.cpp
  index/coinstatsindex.cpp
  index/txindex.cpp
  init.cpp
  kernel/chain.cpp
  kernel/checks.cpp
  kernel/coinstats.cpp
  kernel/context.cpp
  kernel/cs_main.cpp
  kernel/disconnected_transactions.cpp
  kernel/mempool_removal_reason.cpp
  mapport.cpp
  net.cpp
  net_processing.cpp
  netgroup.cpp
  node/abort.cpp
  node/blockmanager_args.cpp
  node/blockstorage.cpp
  node/caches.cpp
  node/chainstate.cpp
  node/chainstatemanager_args.cpp
  node/coin.cpp
  node/coins_view_args.cpp
  node/connection_types.cpp
  node/context.cpp
  node/database_args.cpp
  node/eviction.cpp
  node/interface_ui.cpp
  node/interfaces.cpp
  node/kernel_notifications.cpp
  node/mempool_args.cpp
  node/mempool_persist.cpp
  node/mempool_persist_args.cpp
  node/miner.cpp
  node/mini_miner.cpp
  node/minisketchwrapper.cpp
  node/peerman_args.cpp
  node/psmt.cpp
  node/timeoffsets.cpp
  node/transaction.cpp
  node/txdownloadman_impl.cpp
  node/txorphanage.cpp
  node/txreconciliation.cpp
  node/utxo_snapshot.cpp
  node/warnings.cpp
  noui.cpp
  policy/ephemeral_policy.cpp
  policy/fees.cpp
  policy/fees_args.cpp
  policy/packages.cpp
  policy/rbf.cpp
  policy/settings.cpp
  policy/truc_policy.cpp
  rest.cpp
  rpc/assets.cpp
  rpc/blockchain.cpp
  rpc/external_signer.cpp
  rpc/fees.cpp
  rpc/mempool.cpp
  rpc/messages.cpp
  rpc/mining.cpp
  rpc/auxpow_miner.cpp
  rpc/net.cpp
  rpc/node.cpp
  rpc/output_script.cpp
  rpc/rawtransaction.cpp
  rpc/server.cpp
  rpc/server_util.cpp
  rpc/signmessage.cpp
  rpc/txoutproof.cpp
  script/sigcache.cpp
  signet.cpp
  torcontrol.cpp
  txdb.cpp
  txgraph.cpp
  txmempool.cpp
  txrequest.cpp
  validation.cpp
  validationinterface.cpp
  versionbits.cpp
  $<$<TARGET_EXISTS:meowcoin_wallet>:wallet/init.cpp>
  $<$<NOT:$<TARGET_EXISTS:meowcoin_wallet>>:dummywallet.cpp>
)
target_link_libraries(meowcoin_node
  PRIVATE
    core_interface
    bitcoin_assets
    meowcoin_common
    meowcoin_util
    $<TARGET_NAME_IF_EXISTS:meowcoin_zmq>
    leveldb
    minisketch
    univalue
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:libevent::core>
    $<TARGET_NAME_IF_EXISTS:libevent::extra>
    $<TARGET_NAME_IF_EXISTS:libevent::pthreads>
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
)

# Meowcoin wrapper executable that can call other executables.
if(BUILD_BITCOIN_BIN)
  add_executable(meowcoin meowcoin.cpp)
  add_windows_resources(meowcoin meowcoin-res.rc)
  add_windows_application_manifest(meowcoin)
  target_link_libraries(meowcoin core_interface meowcoin_common meowcoin_util)
  install_binary_component(meowcoin HAS_MANPAGE)
endif()

# Meowcoin Core meowcoind.
if(BUILD_DAEMON)
  add_executable(meowcoind
    meowcoind.cpp
    init/meowcoind.cpp
  )
  add_windows_resources(meowcoind meowcoind-res.rc)
  add_windows_application_manifest(meowcoind)
  target_link_libraries(meowcoind
    core_interface
    meowcoin_node
    $<TARGET_NAME_IF_EXISTS:meowcoin_wallet>
  )
  install_binary_component(meowcoind HAS_MANPAGE)
endif()
if(ENABLE_IPC AND BUILD_DAEMON)
  add_executable(meowcoin-node
    meowcoind.cpp
    init/meowcoin-node.cpp
  )
  target_link_libraries(meowcoin-node
    core_interface
    meowcoin_node
    meowcoin_ipc
    $<TARGET_NAME_IF_EXISTS:meowcoin_wallet>
  )
  install_binary_component(meowcoin-node INTERNAL)
endif()

if(ENABLE_IPC AND BUILD_TESTS)
    # meowcoin_ipc_test library target is defined here in src/CMakeLists.txt
    # instead of src/test/CMakeLists.txt so capnp files in src/test/ are able to
    # reference capnp files in src/ipc/capnp/ by relative path. The Cap'n Proto
    # compiler only allows importing by relative path when the importing and
    # imported files are underneath the same compilation source prefix, so the
    # source prefix must be src/, not src/test/
    add_library(meowcoin_ipc_test STATIC EXCLUDE_FROM_ALL
      test/ipc_test.cpp
    )
    target_capnp_sources(meowcoin_ipc_test ${PROJECT_SOURCE_DIR}
      test/ipc_test.capnp
    )
    add_dependencies(meowcoin_ipc_test meowcoin_ipc_headers)
endif()


add_library(meowcoin_cli STATIC EXCLUDE_FROM_ALL
  compat/stdin.cpp
  rpc/client.cpp
)
target_link_libraries(meowcoin_cli
  PUBLIC
    core_interface
    univalue
)


# Meowcoin Core RPC client
if(BUILD_CLI)
  add_executable(meowcoin-cli meowcoin-cli.cpp)
  add_windows_resources(meowcoin-cli meowcoin-cli-res.rc)
  add_windows_application_manifest(meowcoin-cli)
  target_link_libraries(meowcoin-cli
    core_interface
    meowcoin_cli
    meowcoin_common
    meowcoin_util
    libevent::core
    libevent::extra
  )
  install_binary_component(meowcoin-cli HAS_MANPAGE)
endif()


if(BUILD_TX)
  add_executable(meowcoin-tx meowcoin-tx.cpp)
  add_windows_resources(meowcoin-tx meowcoin-tx-res.rc)
  add_windows_application_manifest(meowcoin-tx)
  target_link_libraries(meowcoin-tx
    core_interface
    meowcoin_common
    meowcoin_util
    univalue
  )
  install_binary_component(meowcoin-tx HAS_MANPAGE)
endif()


if(BUILD_UTIL)
  add_executable(meowcoin-util meowcoin-util.cpp)
  add_windows_resources(meowcoin-util meowcoin-util-res.rc)
  add_windows_application_manifest(meowcoin-util)
  target_link_libraries(meowcoin-util
    core_interface
    meowcoin_common
    meowcoin_util
  )
  install_binary_component(meowcoin-util HAS_MANPAGE)
endif()


if(BUILD_GUI)
  add_subdirectory(qt)
endif()


if(BUILD_KERNEL_LIB)
  add_subdirectory(kernel)
endif()

if(BUILD_UTIL_CHAINSTATE)
  add_executable(meowcoin-chainstate
    meowcoin-chainstate.cpp
  )
  # TODO: The `SKIP_BUILD_RPATH` property setting can be deleted
  #       in the future after reordering Guix script commands to
  #       perform binary checks after the installation step.
  # Relevant discussions:
  # - https://github.com/hebasto/meowcoin/pull/236#issuecomment-2183120953
  # - https://github.com/meowcoin/meowcoin/pull/30312#issuecomment-2191235833
  set_target_properties(meowcoin-chainstate PROPERTIES
    SKIP_BUILD_RPATH OFF
  )
  target_link_libraries(meowcoin-chainstate
    PRIVATE
      core_interface
      meowcoinkernel
  )
  install_binary_component(meowcoin-chainstate INTERNAL)
endif()


add_subdirectory(test/util)
if(BUILD_BENCH)
  add_subdirectory(bench)
endif()

if(BUILD_TESTS)
  add_subdirectory(test)
endif()

if(BUILD_FUZZ_BINARY)
  add_subdirectory(test/fuzz)
endif()
