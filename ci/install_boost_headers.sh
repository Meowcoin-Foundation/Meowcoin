#!/usr/bin/env bash
# Copyright (c) 2026 The Meowcoin Core developers
# Distributed under the MIT software license; see COPYING.
# Install the same SHA-256-pinned header-only Boost as depends/packages/boost.mk.
set -euo pipefail
export LC_ALL=C
PREFIX="$1"
SOURCE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION=1.88.0
SHA256=dcea50f40ba1ecfc448fdf886c0165cf3e525fef2c9e3e080b9804e8117b9694
WORK="$(mktemp -d)"
cd "$WORK"
curl --location --fail --retry 3 -o boost.tar.gz \
  "https://github.com/boostorg/boost/releases/download/boost-${VERSION}/boost-${VERSION}-cmake.tar.gz"
if command -v sha256sum >/dev/null 2>&1; then
  echo "${SHA256}  boost.tar.gz" | sha256sum -c -
else
  echo "${SHA256}  boost.tar.gz" | shasum -a 256 -c -
fi
tar xzf boost.tar.gz
cd "boost-${VERSION}"
patch -p1 < "$SOURCE_ROOT/depends/patches/boost/skip_compiled_targets.patch"
cmake -S . -B build -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  '-DBOOST_INCLUDE_LIBRARIES=multi_index;signals2;test' \
  -DBOOST_TEST_HEADERS_ONLY=ON -DBOOST_ENABLE_MPI=OFF -DBOOST_ENABLE_PYTHON=OFF \
  -DBOOST_INSTALL_LAYOUT=system -DBUILD_TESTING=OFF -DCMAKE_DISABLE_FIND_PACKAGE_ICU=ON
cmake --build build --parallel 3
cmake --install build
