# RIP-25 repeatable soak campaign

`contrib/rip25/soak.py` runs a bounded campaign against an existing upgraded
testnet reference node and a fresh observer that it owns. It uses Python 3.10+
and the standard library. It never schedules mainnet activation.

## What a passing campaign means

The default campaign runs for 72 hours, requires at least 24 completed cycles
and 12 new blocks, and stops on the first failed check. Each cycle does:

1. Fund two fresh PQ addresses on the observer.
2. Spend both inputs together, paying the reference wallet and producing PQ change.
3. Spend that PQ change as a single-input return, bringing remaining funds back.
4. For every transaction, check all witness elements are exactly 2420/1312
   bytes and all outputs/change are witness-v2 programs. Test the valid signature
   first, then require a damaged signature to fail specifically for signature
   verification on both nodes. Damaged transactions are never broadcast.
5. Confirm inclusion in the same block on both nodes, with two confirmations
   by default. Both nodes must agree at a common chain height during every wait.

Every sixth cycle restarts only the owned observer. Every twelfth cycle backs
up its encrypted wallet, unloads the original, restores under a separate name,
compares addresses and UTXOs, and signs a spend of funded PQ inputs with the
restored private keys. Both nodes must accept that signed transaction in a
dry-run mempool check. The restored wallet is then unloaded and the original
reloaded. No duplicate-wallet transaction is broadcast. A passing campaign
requires both restore and restart coverage, and locked signing refusal.

The runner records observed reorgs. Controlled block disconnect/reconnect is
available only with `--chain regtest --reorg-every N`; it cannot run on public
testnet. Public testing needs separate coordinated reorg and pool/miner testing.
Two nodes on one operator's infrastructure do not establish independent
operator qualification, and a running campaign is not a passed campaign.

## Run against public testnet

Use the RIP-25 candidate, not released v30.2.8: that release rejects the current
testnet's valid PQ block 102. Use an existing reference node with loopback RPC,
real liboqs, and active `mldsa44`. Its wallet RPC must be enabled. The runner
creates a uniquely named campaign wallet and does not use existing wallets.

```sh
python3 contrib/rip25/soak.py \
  --binary /path/to/candidate/bin/meowcoind \
  --state-dir /private/path/rip25-soak-72h \
  --reference-url http://127.0.0.1:18332 \
  --reference-cookie /path/to/reference/testnet3/.cookie \
  --peer 127.0.0.1:4569 --peer 209.145.48.36:4569 \
  --duration 259200 --interval 600
```

The state directory must be empty for a new campaign. RPC URLs must use
loopback HTTP without embedded credentials, paths, or query parameters. Both
nodes are checked for chain, genesis, and active deployment before wallet use.
The observer binds P2P/RPC only to loopback, with default ports 19433/19432.
Its initial synchronization and reference funding are covered by the configured
confirmation timeout. The runner prints a public PQ `funding_address` in
`status.json` and a `ready` event. Send about 100 expendable **test MEWC** to that
address, allowing it to confirm twice. It then cycles funds and consumes fees.

For the slow, bounded operator canary miner, use `--confirm-timeout 5400
--stall-seconds 5400` (90 minutes). The generic defaults are 45 minutes.
Each transaction and aggregate fee budget is checked before broadcast:
`--fee-rate 2000`, `--max-tx-fee 0.1`, and `--max-total-fees 75` test MEWC.
The wallet's current fee floor is 1000 atomic units/vB. There is no automatic
top-up, production wallet import, or mainnet transaction path.

At the duration boundary, no new cycle starts; an in-flight cycle can finish
within at most three confirmation timeouts of grace. An external service should
cap total runtime beyond that grace and constrain CPU/memory. The runner stops
at 512 MiB campaign data or below 1024 MiB free space by default; operators can
choose stricter free-space thresholds. It also fails if the reference chain
does not advance within the stall timeout. Temporary RPC/transport errors are
reported as failures, not silently treated as passing checks.

`status.json` and `events.jsonl` contain test receipts and no credentials.
`private-state.json` contains the generated wallet passphrase, exact pending
transaction bytes, and private operational paths. Protect the whole directory
as wallet material. On Unix, the runner sets directory/file modes 0700/0600;
on Windows, use a directory with an appropriate private Windows ACL. Wallet
backups and observer node files stay there. Do not publish the directory.

On success the runner exits 0 and writes `status: passed`; failed or interrupted
campaigns exit nonzero. SIGINT/SIGTERM stop the owned observer, lock/unload only
the campaign wallet on the reference, and preserve state. Run with the same
arguments plus `--resume` to reuse pending signed transaction bytes and avoid
duplicating payments. The original deadline and fee counter are preserved.
A resumed integrity or transport failure remains in the failure history and
prevents a qualification pass. A deliberate operator interruption can resume
without counting as a failed check. Resume is for preserving/recovering work,
not erasing a failed campaign.
A duplicate campaign is refused by an exclusive lock. After an unclean crash,
verify the prior runner and its owned node are stopped before removing the stale
`campaign.lock`. `private-state.json` is the authoritative atomic checkpoint;
`status.json` is a derived public view.

## Validate the actual engine on an isolated chain

```sh
python3 contrib/rip25/validate_soak.py \
  --binary /path/to/candidate/bin/meowcoind \
  --output /private/path/fresh-soak-validation
```

This starts two loopback regtest nodes and runs two complete cycles, including
two funded encrypted restores, two observer restarts, and two explicit block
disconnect/reconnect checks. It interrupts immediately after a persisted
broadcast, resumes the campaign, and verifies the same transaction is reused.
It also checks duplicate-runner locking, wrong-chain refusal, loopback RPC
requirements, and absence of the passphrase/cookie from public artifacts.
No public chain is modified. A `validation.json` receipt records the result.

## Mainnet release gates still outside this runner

- Independently operated nodes and cross-platform confirmation of receipts.
- Pool/miner, explorer, exchange, and wallet integration qualification.
- Resolution of the three documented broader baseline test failures.
- A distinct candidate version identifier for broader distribution.
- Independent cryptography/consensus review and a separately reviewed BIP9
  activation release. Mainnet remains `NEVER_ACTIVE` throughout this campaign.
