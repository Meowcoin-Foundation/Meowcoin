# Copyright (c) 2023-present The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(generate_setup_nsi)
  set(abs_top_srcdir ${PROJECT_SOURCE_DIR})
  set(abs_top_builddir ${PROJECT_BINARY_DIR})
  set(CLIENT_URL ${PROJECT_HOMEPAGE_URL})
  set(CLIENT_TARNAME "meowcoin")
  set(BITCOIN_WRAPPER_NAME "meowcoin")
  set(BITCOIN_GUI_NAME "meowcoin-qt")
  set(BITCOIN_DAEMON_NAME "meowcoind")
  set(BITCOIN_CLI_NAME "meowcoin-cli")
  set(BITCOIN_TX_NAME "meowcoin-tx")
  set(BITCOIN_WALLET_TOOL_NAME "meowcoin-wallet")
  set(BITCOIN_TEST_NAME "test_meowcoin")
  set(EXEEXT ${CMAKE_EXECUTABLE_SUFFIX})
  configure_file(${PROJECT_SOURCE_DIR}/share/setup.nsi.in ${PROJECT_BINARY_DIR}/meowcoin-win64-setup.nsi USE_SOURCE_PERMISSIONS @ONLY)
endfunction()
