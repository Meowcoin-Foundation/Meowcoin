# Meowcoin Core 30.2.9rc2 — RIP-25 public testnet candidate

This candidate gives the Quantum/RIP-25 test build a distinct version. It
includes the merged RIP-25 implementation plus the soak-runner and legacy
fixture-repair changes prepared in PRs #42 and #43. Build manifests identify
the exact source revision and platform artifacts.

## Network scope

Mainnet ML-DSA-44 activation remains disabled. This prerelease does not set a
mainnet activation schedule. PQ rules are active on the experimental test
networks. Use a separate testnet data directory and fresh test wallets.

Released v30.2.8 already enforces an earlier experimental PQ rule on testnet;
it rejects the campaign's valid PQ spend at block 102. Testnet participants
must use a RIP-25 candidate. Old nodes with mainnet's disabled-PQ consensus
state follow valid new PQ blocks, but their mempool policy rejects new PQ
spends. PQ relay and mining require upgraded peers/miners.

## Changes since candidate 1

- Application full version is `30.2.9rc2` with source identification, and the
  build is marked as a prerelease. Numeric RPC/P2P client version is `300209`;
  the peer version base is `30.2.9`. Testnet launchers add an RC2/source comment.
- Add a bounded, resumable soak runner with explicit coverage, duration, fee,
  disk, and chain-stall gates. It exercises PQ receive/change, single/multiple
  input spends, signature rejection, encrypted backup/restore, and observer
  restart. Controlled reorg exercises are isolated-regtest-only.
- Fix the three inherited descriptor/script/wallet regression suites: Meowcoin
  key/address encodings, existing asset-opcode expectations, deterministic
  genesis-relative fixture time, and owned miner rewards. Preserve the original
  key/script payloads and add those suites to the three-platform PQ QA jobs.

## Testing and qualification

See [wallet/rollout instructions](../rip25-wallet-and-rollout.md) and the
[soak campaign guide](../rip25-soak.md). A completed local or CI check does not
establish that the 72-hour public soak, independent operator qualification,
pool/miner integration, or independent crypto/consensus review has passed.
The existing public soak remains pinned to its original source/binary so its
results remain attributable. RC2 is validated separately against that network.

The targeted QA gate includes the PQ unit vectors, wallet functional checks,
the repaired legacy suites, and isolated soak-engine recovery/resume tests.
The full repository test suite is not claimed clean by this candidate.
