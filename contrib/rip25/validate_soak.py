#!/usr/bin/env python3
# Copyright (c) 2026 The Meowcoin developers
# Distributed under the MIT software license, see the accompanying file COPYING.
"""Exercise the actual soak engine against two owned, isolated regtest nodes."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from soak import Campaign, CheckFailed, RPC, RPCError, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output", required=True, help="Fresh validation directory")
    args = parser.parse_args()
    root = Path(args.output).resolve()
    root.mkdir(mode=0o700, parents=True, exist_ok=False)
    reference_data = root / "reference"
    reference_data.mkdir()
    log = (root / "reference.log").open("w")
    options = [args.binary, "-regtest", "-server=1", f"-datadir={reference_data}", "-port=19540", "-rpcport=19550",
               "-rpcbind=127.0.0.1", "-bind=127.0.0.1", "-connect=0", "-dnsseed=0", "-fixedseeds=0",
               "-listenonion=0", "-dbcache=16", "-maxmempool=16", "-keypool=5", "-par=1"]
    options += [f"-vbparams={d}:-2:9223372036854775807" for d in
                ("testdummy", "assets", "messaging_restricted", "transfer_script_size", "enforce_value", "coinbase_assets")]
    reference = RPC("http://127.0.0.1:19550", reference_data / "regtest/.cookie")
    proc = subprocess.Popen(options, stdout=log, stderr=subprocess.STDOUT)
    current = None
    try:
        end = time.monotonic() + 60
        while time.monotonic() < end:
            require(proc.poll() is None, "Validation reference failed to start")
            try:
                reference.call("getblockchaininfo")
                break
            except (OSError, RPCError):
                time.sleep(.1)
        else:
            raise CheckFailed("Validation reference RPC unavailable")
        settings = argparse.Namespace(binary=args.binary, state_dir=str(root / "campaign"),
            reference_url=reference.url, reference_cookie=str(reference.cookie), chain="regtest",
            peer=["127.0.0.1:19540"], rpc_port=19432, p2p_port=19433,
            duration=600, interval=0, confirm_timeout=60, stall_seconds=120,
            confirmations=1, cycles=2, min_cycles=2, min_blocks=6, restart_every=1,
            restore_every=1, fee_rate=2000, max_tx_fee=.1, max_total_fees=1,
            min_free_mib=64, max_state_mib=128, resume=False, self_mine=True, reorg_every=1)
        current = Campaign(settings)
        actual_event = current.event

        def crash_after_broadcast(kind, **fields):
            actual_event(kind, **fields)
            if kind == "broadcast":
                raise CheckFailed("validation interruption after persisted broadcast")

        current.event = crash_after_broadcast
        try:
            current.run()
        except CheckFailed as error:
            require("validation interruption" in str(error), str(error))
        else:
            raise CheckFailed("Resume fault injection did not run")
        pending = current.state["cycle"]["funding"]["txid"]
        current.close()
        current = None
        settings.resume = True
        current = Campaign(settings)
        current.run()
        report = current.report
        require(report["status"] == "passed", "Campaign did not pass")
        funding = [t for t in report["transactions"] if t["stage"] == "funding"]
        require(len(funding) == 2 and funding[0]["txid"] == pending, "Resume duplicated or replaced the pending payment")
        require(report["completed_cycles"] == 2, "Expected two completed cycles")
        for name in ("encrypted_backup_restore", "owned_observer_restart", "isolated_block_disconnect_reconnect"):
            require(report["checks"].get(name) == 2, "Missing recovery coverage: " + name)
        # Duplicate ownership must fail before altering the active campaign.
        other = Campaign(settings)
        before = (current.root / "private-state.json").read_bytes()
        try:
            other.acquire()
        except FileExistsError:
            pass
        else:
            raise CheckFailed("Concurrent campaign was allowed")
        require(before == (current.root / "private-state.json").read_bytes(), "Duplicate runner changed state")
        # A mainnet reference is rejected before any wallet RPC is attempted.
        calls = []

        class Mainnet:
            def call(self, method, *args, **kwargs):
                calls.append(method)
                return {"chain": "main"}

        try:
            current.verify_chain(Mainnet())
        except CheckFailed:
            pass
        else:
            raise CheckFailed("Mainnet reference was accepted")
        require(calls == ["getblockchaininfo"], "Wrong-chain check reached wallet RPC")
        for url in ("http://192.0.2.1:8332", "https://127.0.0.1:8332", "http://user:secret@127.0.0.1:8332"):
            try:
                RPC(url, reference.cookie)
            except CheckFailed:
                pass
            else:
                raise CheckFailed("Unsafe RPC URL was accepted")
        # Status/events must never contain the encrypted-wallet passphrase or cookie.
        public = (current.root / "status.json").read_text() + (current.root / "events.jsonl").read_text()
        require(current.state["password"] not in public and reference.cookie.read_text().strip() not in public,
                "Credential leaked into a status artifact")
        summary = {"passed": True, "completed_cycles": 2, "transactions": len(report["transactions"]),
                   "checks": report["checks"], "resume_reused_pending_transaction": True,
                   "concurrent_lock_guard": True, "wrong_chain_guard": True,
                   "loopback_rpc_guard": True, "public_artifact_secret_check": True}
        (root / "validation.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(json.dumps(summary), flush=True)
    finally:
        if current is not None:
            current.close()
        try:
            reference.call("stop")
        except Exception:
            pass
        try:
            proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            proc.terminate()
            proc.wait(timeout=15)
        log.close()


if __name__ == "__main__":
    main()
