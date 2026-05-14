# Copyright (c) 2025-present The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(ts_files.cmake)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.7)
  qt6_add_lrelease(TS_FILES ${ts_files} LRELEASE_TARGET meowcoinqt_lrelease OPTIONS -silent)
else()
  qt6_add_lrelease(meowcoinqt TS_FILES ${ts_files} OPTIONS -silent)
endif()

add_dependencies(meowcoinqt meowcoinqt_lrelease)
