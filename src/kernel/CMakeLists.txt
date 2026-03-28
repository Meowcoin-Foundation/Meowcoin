# Copyright (c) 2024-present The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(GNUInstallDirs)

# TODO: libmeowcoinkernel is a work in progress consensus engine
#       library, as more and more modules are decoupled from the
#       consensus engine, this list will shrink to only those
#       which are absolutely necessary.
add_library(meowcoinkernel
  meowcoinkernel.cpp
  chain.cpp
  checks.cpp
  chainparams.cpp
  coinstats.cpp
  context.cpp
  cs_main.cpp
  disconnected_transactions.cpp
  mempool_removal_reason.cpp
  ../arith_uint256.cpp
  ../chain.cpp
  ../coins.cpp
  ../compressor.cpp
  ../consensus/merkle.cpp
  ../consensus/tx_check.cpp
  ../consensus/tx_verify.cpp
  ../core_read.cpp
  ../dbwrapper.cpp
  ../deploymentinfo.cpp
  ../deploymentstatus.cpp
  ../flatfile.cpp
  ../hash.cpp
  ../logging.cpp
  ../node/blockstorage.cpp
  ../node/chainstate.cpp
  ../node/utxo_snapshot.cpp
  ../policy/ephemeral_policy.cpp
  ../policy/feerate.cpp
  ../policy/packages.cpp
  ../policy/policy.cpp
  ../policy/rbf.cpp
  ../policy/settings.cpp
  ../policy/truc_policy.cpp
  ../pow.cpp
  ../primitives/block.cpp
  ../primitives/transaction.cpp
  ../pubkey.cpp
  ../random.cpp
  ../randomenv.cpp
  ../script/interpreter.cpp
  ../script/script.cpp
  ../script/script_error.cpp
  ../script/sigcache.cpp
  ../script/solver.cpp
  ../signet.cpp
  ../streams.cpp
  ../support/lockedpool.cpp
  ../sync.cpp
  ../txdb.cpp
  ../txmempool.cpp
  ../uint256.cpp
  ../util/chaintype.cpp
  ../util/check.cpp
  ../util/feefrac.cpp
  ../util/fs.cpp
  ../util/fs_helpers.cpp
  ../util/hasher.cpp
  ../util/moneystr.cpp
  ../util/rbf.cpp
  ../util/serfloat.cpp
  ../util/signalinterrupt.cpp
  ../util/strencodings.cpp
  ../util/string.cpp
  ../util/syserror.cpp
  ../util/threadnames.cpp
  ../util/time.cpp
  ../util/tokenpipe.cpp
  ../validation.cpp
  ../validationinterface.cpp
  ../versionbits.cpp
  $<TARGET_OBJECTS:meowcoin_clientversion>
  $<TARGET_OBJECTS:meowcoin_crypto>
  $<TARGET_OBJECTS:leveldb>
  $<TARGET_OBJECTS:crc32c>
)
target_link_libraries(meowcoinkernel
  PRIVATE
    core_interface
    secp256k1_objs
    $<$<PLATFORM_ID:Windows>:bcrypt>
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
  PUBLIC
    Boost::headers
)

target_include_directories(meowcoinkernel PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/leveldb/include>)

# libmeowcoinkernel requires default symbol visibility, explicitly
# specify that here so that things still work even when user
# configures with -DREDUCE_EXPORTS=ON
#
# Note this is a quick hack that will be removed as we
# incrementally define what to export from the library.
set_target_properties(meowcoinkernel PROPERTIES
  CXX_VISIBILITY_PRESET default
)

# Add a convenience libmeowcoinkernel target as a synonym for meowcoinkernel.
add_custom_target(libmeowcoinkernel)
add_dependencies(libmeowcoinkernel meowcoinkernel)

configure_file(${PROJECT_SOURCE_DIR}/libmeowcoinkernel.pc.in ${PROJECT_BINARY_DIR}/libmeowcoinkernel.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/libmeowcoinkernel.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig" COMPONENT libmeowcoinkernel)

install(TARGETS meowcoinkernel
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT libmeowcoinkernel
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT libmeowcoinkernel
  ARCHIVE
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT libmeowcoinkernel
)
