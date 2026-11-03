# RIP-25 wallet integration and rollout

RIP-25 adds voluntary ML-DSA-44 witness-v2 addresses for MEWC. Existing ECDSA,
SegWit, and Taproot outputs keep their existing spending rules. Moving coins to a
PQ address protects those outputs after activation; this does not convert every
existing balance or every part of the network to post-quantum cryptography.

## Activation boundary

Mainnet remains `NEVER_ACTIVE`. This change does not schedule or activate a
mainnet soft fork. Testnet, signet, and regtest enable the consensus rules for
testing. A validating/mining node needs the real liboqs backend on any network
where RIP-25 can activate; a stub-only build refuses to start there.

Before activation, witness-v2 outputs are anyone-can-spend under consensus.
The wallet rejects PQ receive addresses, raw change addresses, payments, and
explicit PQ change before activation. Do not fund PQ addresses on an inactive
network using manually constructed transactions or other software.

## Wallet behavior

After activation, new locally managed descriptor wallets create both receive
and change PQ descriptors. `getnewaddress "" "pq"` and
`getrawchangeaddress "pq"` return witness-v2 bech32m addresses.

For a wallet created by an earlier release, the first explicit PQ address
request adds missing PQ descriptors using its existing backed-up root. Other
descriptors and address indexes are preserved. Unlock an encrypted wallet for
this first request. No new unrelated seed is generated. Make a fresh wallet
backup after enabling PQ addresses so it includes the new descriptor records
and address caches.

The automatic upgrade only uses an unambiguous depth-zero root with a local
private key. An account-only, external-signer, or ambiguous multi-root wallet
must import explicit PQ descriptors or use a new locally managed descriptor
wallet. If inactive ranged PQ descriptors are already stored, explicitly
activate/import the intended descriptors with `importdescriptors`; the
automatic upgrade preserves their address indexes, including equivalent paths
written with `h` or apostrophes. An explicitly imported PQ receive descriptor can generate receive
addresses without a change descriptor or local private keys, while its cached
range remains available.

Payments to a PQ recipient use PQ change when the wallet has an active PQ
change descriptor. An explicit change type still takes precedence. PQ spends
carry much larger witnesses than ordinary spends; compare the actual fee and
virtual size rather than assuming ordinary transaction costs.

## Reproducible checks

Build with the SHA-256-pinned liboqs 0.16.0 from `ci/install_liboqs.sh` or
`depends`, with tests and the wallet enabled. Build all command-line tools,
not just the test executable: this also checks consensus-library linkage.

On macOS, first run `ci/install_boost_headers.sh "$PWD/boost-prefix"` and
add `-DBoost_ROOT="$PWD/boost-prefix"` to the configure command below. This
uses the SHA-256-pinned Boost 1.88.0 from the dependency recipe; current
Homebrew Boost releases are incompatible with the existing index-list types.

```sh
ci/install_liboqs.sh "$PWD/liboqs-prefix" -G Ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$PWD/liboqs-prefix" -DBUILD_GUI=OFF \
  -DBUILD_TESTS=ON -DBUILD_BENCH=OFF -DBUILD_FUZZ_BINARY=OFF \
  -DENABLE_IPC=OFF -DWITH_ZMQ=OFF
cmake --build build --parallel 3
build/bin/test_meowcoin \
  --run_test=descriptor_tests,script_tests,wallet_tests,pqkey_tests,pqkey_vectors_tests,mldsa44_sighash_tests,mldsa44_witness_tests \
  --report_level=detailed
python3 test/functional/feature_mldsa44.py --configfile=build/test/config.ini
```

The functional test uses isolated regtest nodes. It covers inactive-network
guards, distinct addresses, funding and confirmed PQ spends, multiple PQ
inputs, PQ change, backup/restore, encrypted signing, and existing descriptor
wallet upgrades followed by reload and spending. It uses Meowcoin-generated
mining addresses instead of the upstream Bitcoin WIF fixtures.

`.github/workflows/rip25-qa.yml` runs these checks on Linux x86_64, Windows
x86_64, and macOS ARM64. A passing local run is not evidence that the other
platforms or a public testnet campaign have passed.

## Public deployment gates

1. Review and merge the implementation after platform CI passes. Build test
   artifacts with real liboqs, and publish their exact source revision and
   checksums.
2. Exercise a public testnet with independently operated nodes: receive,
   change, multi-input spends, restart, encrypted backup/restore, and reorgs.
   Confirm signatures and the frozen vectors across the supported platforms.
3. Confirm pool, exchange, explorer, and wallet readiness. Keep ordinary address
   defaults until activation and clearly distinguish PQ addresses from older
   outputs.
4. Prepare a separately reviewed activation release with explicit BIP9 start,
   timeout, and minimum activation height. Current mainnet parameters reserve
   bit 11 and require 1815 of 2016 blocks; do not change these without a network
   rollout decision.
5. Observe signalling, lock-in, and activation on live nodes before inviting
   users to fund mainnet PQ addresses.

Do not downgrade a node across activation or restore an old binary against a
PQ-enabled wallet without checking its consensus and wallet compatibility.
