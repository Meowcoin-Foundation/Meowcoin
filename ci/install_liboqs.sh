#!/usr/bin/env bash
# Copyright (c) 2026 The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.
#
# Build and install the pinned liboqs used for ML-DSA-44 (RIP-25) into PREFIX.
# Matches depends/packages/liboqs.mk: minimal static SIG_ml_dsa_44 build.
# cmake/liboqs.cmake treats a missing liboqs as a hard error, so release
# builds that do not use depends must run this first and pass
# -DCMAKE_PREFIX_PATH=PREFIX.
#
# Usage: ci/install_liboqs.sh PREFIX [extra cmake configure args...]

export LC_ALL=C
set -euo pipefail

PREFIX="$1"
shift

LIBOQS_VERSION="0.16.0"
LIBOQS_SHA256="162d5b510518ee5f285f82fa1f16402a885176e818bf1b1a4c3c91c9a2f01eae"

WORK="$(mktemp -d)"
cd "${WORK}"
curl --location --fail --retry 3 -o liboqs.tar.gz \
  "https://github.com/open-quantum-safe/liboqs/archive/refs/tags/${LIBOQS_VERSION}.tar.gz"
if command -v sha256sum >/dev/null 2>&1; then
  echo "${LIBOQS_SHA256}  liboqs.tar.gz" | sha256sum -c -
else
  # macOS provides shasum by default, without GNU coreutils.
  echo "${LIBOQS_SHA256}  liboqs.tar.gz" | shasum -a 256 -c -
fi
tar xzf liboqs.tar.gz

cmake -S "liboqs-${LIBOQS_VERSION}" -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DOQS_BUILD_ONLY_LIB=ON \
  -DOQS_MINIMAL_BUILD=SIG_ml_dsa_44 \
  -DOQS_USE_OPENSSL=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DOQS_DIST_BUILD=ON \
  "$@"
cmake --build build --config Release --parallel
cmake --install build --config Release
