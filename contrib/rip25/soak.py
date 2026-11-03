#!/usr/bin/env python3
# Copyright (c) 2026 The Meowcoin developers
# Distributed under the MIT software license, see the accompanying file COPYING.
"""Bounded, resumable RIP-25 testnet campaign. Python standard library only."""

import argparse
import base64
from decimal import Decimal
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

GENESIS = "000000eaab417d6dfe9bd75119972e1d07ecfe8ff655bef7c2acb3d9a0eeed81"
SIZES = [2420, 1312]


class CheckFailed(Exception):
    pass


class RPCError(Exception):
    def __init__(self, code):
        # RPC messages can contain caller input; retain only the numeric code.
        self.code = code
        super().__init__(f"RPC error {code}")


def require(condition, message):
    if not condition:
        raise CheckFailed(message)


def save(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        os.chmod(temporary, 0o600)
        json.dump(value, stream, indent=2)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


class RPC:
    def __init__(self, url, cookie):
        parsed = urllib.parse.urlparse(url)
        require(parsed.scheme == "http" and parsed.hostname in ("127.0.0.1", "::1", "localhost")
                and not parsed.username and not parsed.password and parsed.path in ("", "/")
                and not parsed.query and not parsed.fragment, "RPC must use a loopback HTTP URL")
        self.url, self.cookie = url.rstrip("/"), Path(cookie)
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def call(self, method, params=None, wallet=None):
        # Read the cookie for every call: owned-node restarts rotate it.
        auth = base64.b64encode(self.cookie.read_bytes().strip()).decode()
        url = self.url + ("/wallet/" + urllib.parse.quote(wallet, safe="") if wallet else "/")
        request = urllib.request.Request(url, json.dumps({"jsonrpc": "2.0", "id": "rip25-soak",
            "method": method, "params": params if params is not None else []}).encode(),
            {"Content-Type": "application/json", "Authorization": "Basic " + auth})
        try:
            response = self.opener.open(request, timeout=15)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            data = json.load(response)
        if data.get("error"):
            raise RPCError(data["error"]["code"])
        return data["result"]


class Campaign:
    def __init__(self, args):
        self.args = args
        self.root = Path(args.state_dir).resolve()
        self.root.mkdir(parents=True, exist_ok=True)
        os.chmod(self.root, 0o700)
        self.lock = self.root / "campaign.lock"
        self.lock_fd = None
        self.proc = None
        self.log = None
        self.state = None
        self.stopping = False
        self.reference_verified = False
        self.reference = RPC(args.reference_url, args.reference_cookie)
        chain_dir = "testnet3" if args.chain == "test" else "regtest"
        self.observer = RPC(f"http://127.0.0.1:{args.rpc_port}", self.root / "node" / chain_dir / ".cookie")
        self.report = {"schema": 1, "status": "starting", "chain": args.chain,
                       "binary_sha256": hashlib.sha256(Path(args.binary).read_bytes()).hexdigest(),
                       "checks": {}, "completed_cycles": 0, "fees": "0", "transactions": []}

    def checkpoint(self):
        self.report.update(completed_cycles=self.state["completed_cycles"], fees=self.state["fees"],
                           started=self.state["started"], deadline=self.state["deadline"], updated=int(time.time()))
        self.state["summary"] = self.report
        save(self.root / "private-state.json", self.state)
        save(self.root / "status.json", self.report)

    def event(self, kind, **fields):
        item = {"time": int(time.time()), "event": kind, **fields}
        with (self.root / "events.jsonl").open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(item) + "\n")
        print(json.dumps(item), flush=True)

    def passed(self, name):
        self.report["checks"][name] = self.report["checks"].get(name, 0) + 1

    def acquire(self):
        # A crash requires the operator to verify the prior process stopped and
        # remove this lock; guessing from a possibly reused PID is unsafe.
        self.lock_fd = os.open(self.lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        os.write(self.lock_fd, str(os.getpid()).encode())
        path = self.root / "private-state.json"
        if path.exists():
            require(self.args.resume, "Existing campaign requires --resume")
            saved = json.loads(path.read_text(encoding="utf-8"))
            require(saved["settings"] == self.settings(), "Resume settings differ from saved campaign")
            self.state = saved
            self.report = self.state["summary"]
        else:
            require(not self.args.resume, "No campaign exists to resume")
            require(set(self.root.iterdir()) == {self.lock}, "New campaign directory must be empty")
            now = int(time.time())
            self.state = {"settings": self.settings(), "started": now, "deadline": now + self.args.duration,
                          "password": secrets.token_urlsafe(32), "wallet": "rip25-soak-" + secrets.token_hex(6),
                          "completed_cycles": 0, "fees": "0", "cycle": None,
                          "last_progress": now, "last_height": None, "last_hash": None}
        self.report["status"] = "running"
        self.report.pop("failure", None)
        self.checkpoint()

    def settings(self):
        return {k: v for k, v in vars(self.args).items() if k != "resume"}

    def wallet(self, rpc):
        return self.state["wallet"] + ("-reference" if rpc is self.reference else "-observer")

    def start_observer(self):
        require(self.proc is None, "Observer is already owned by this process")
        data = self.root / "node"
        data.mkdir(exist_ok=True)
        # Never attach to an existing daemon or stop a process we did not spawn.
        try:
            self.observer.call("getblockchaininfo")
        except (OSError, RPCError):
            pass
        else:
            raise CheckFailed("Observer RPC port is already occupied")
        self.log = (self.root / "node.log").open("a", encoding="utf-8")
        options = [self.args.binary, "-testnet" if self.args.chain == "test" else "-regtest",
                   f"-datadir={data}", f"-rpcport={self.args.rpc_port}", f"-port={self.args.p2p_port}",
                   "-rpcbind=127.0.0.1", "-bind=127.0.0.1", "-server=1", "-dnsseed=0", "-fixedseeds=0",
                   "-discover=0", "-listenonion=0", "-dbcache=16", "-maxmempool=16", "-par=1", "-keypool=5",
                   "-maxconnections=8", "-maxuploadtarget=16", "-debuglogfile=0", "-uacomment=RIP25-soak"]
        options += ["-connect=" + peer for peer in self.args.peer]
        if self.args.chain == "regtest":
            options += [f"-vbparams={d}:-2:9223372036854775807" for d in
                        ("testdummy", "assets", "messaging_restricted", "transfer_script_size", "enforce_value", "coinbase_assets")]
            if self.args.self_mine and self.state.get("mock"):
                options += [f'-mocktime={self.state["mock"]}']
        self.proc = subprocess.Popen(options, stdout=self.log, stderr=subprocess.STDOUT)
        end = time.monotonic() + 60
        while time.monotonic() < end:
            require(self.proc.poll() is None, "Owned observer exited during startup; inspect private node.log")
            try:
                self.verify_chain(self.observer)
                return
            except (OSError, RPCError):
                time.sleep(.2)
        raise CheckFailed("Observer RPC did not become available")

    def stop_observer(self):
        if self.proc is None:
            return
        if self.proc.poll() is None:
            try:
                self.observer.call("stop")
            except (OSError, RPCError):
                pass
            try:
                self.proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                self.proc.terminate()
                self.proc.wait(timeout=15)
        self.proc = None
        self.log.close()
        self.log = None

    def verify_chain(self, rpc):
        info = rpc.call("getblockchaininfo")
        require(info["chain"] == self.args.chain, "Chain mismatch; refusing wallet or transaction operations")
        if self.args.chain == "test":
            require(rpc.call("getblockhash", [0]) == GENESIS, "Unexpected testnet genesis")
        require(rpc.call("getdeploymentinfo")["deployments"].get("mldsa44", {}).get("active"),
                "PQ consensus must be active")
        return info

    def ensure_wallet(self, rpc):
        wallet = self.wallet(rpc)
        if wallet not in rpc.call("listwallets"):
            if any(w["name"] == wallet for w in rpc.call("listwalletdir")["wallets"]):
                rpc.call("loadwallet", {"filename": wallet, "load_on_startup": False})
            else:
                rpc.call("createwallet", {"wallet_name": wallet, "passphrase": self.state["password"],
                                        "load_on_startup": False, "avoid_reuse": False})
        require("unlocked_until" in rpc.call("getwalletinfo", wallet=wallet), "Campaign wallet must be encrypted")
        rpc.call("walletpassphrase", [self.state["password"], 120], wallet)
        address = rpc.call("getnewaddress", ["soak", "pq"], wallet)
        rpc.call("walletlock", wallet=wallet)
        require(rpc.call("getaddressinfo", [address], wallet)["ispostquantum"], "Expected a PQ address")
        return address

    def address(self, rpc):
        return rpc.call("getnewaddress", ["soak", "pq"], self.wallet(rpc))

    def pause(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end and not self.stopping:
            time.sleep(min(1, max(0, end - time.monotonic())))
        require(not self.stopping, "Campaign interrupted")

    def tick(self):
        if self.args.self_mine:
            mock = max(int(time.time()), self.state.get("mock", 0) + 1)
            self.state["mock"] = mock
            for rpc in (self.reference, self.observer):
                rpc.call("setmocktime", [mock])

    def mine(self, count=1):
        require(self.args.chain == "regtest" and self.args.self_mine, "Mining is isolated-regtest-only")
        wallet = self.wallet(self.reference)
        address = self.reference.call("getnewaddress", ["mining", "legacy"], wallet)
        for _ in range(count):
            self.state["mock"] = max(int(time.time()), self.state.get("mock", 0) + 121)
            for rpc in (self.reference, self.observer):
                rpc.call("setmocktime", [self.state["mock"]])
            require(len(self.reference.call("generatetoaddress", [1, address])) == 1, "Regtest mining failed")

    def monitor(self):
        require(self.proc is not None and self.proc.poll() is None, "Owned observer exited unexpectedly")
        free = shutil.disk_usage(self.root).free
        size = 0
        for path in self.root.rglob("*"):
            try:
                if path.is_file():
                    size += path.stat().st_size
            except FileNotFoundError:
                pass  # Wallet/database temporary files may disappear during the scan.
        require(free >= self.args.min_free_mib * 1024**2, "Disk free-space limit reached")
        require(size <= self.args.max_state_mib * 1024**2, "Campaign data-size limit reached")
        ref, obs = self.verify_chain(self.reference), self.verify_chain(self.observer)
        height = min(ref["blocks"], obs["blocks"])
        block = self.reference.call("getblockhash", [height])
        require(self.observer.call("getblockhash", [height]) == block, "Nodes disagree at a common height")
        if self.state["last_height"] is not None:
            previous = self.state["last_height"]
            if ref["blocks"] >= previous and self.reference.call("getblockhash", [previous]) != self.state["last_hash"]:
                self.event("observed_reorg", previous_height=previous)
                self.passed("observed_reorgs")
        if self.state["last_height"] is None or ref["blocks"] > self.state["last_height"]:
            self.state["last_progress"] = int(time.time())
        self.state.update(last_height=ref["blocks"], last_hash=ref["bestblockhash"])
        require(time.time() - self.state["last_progress"] <= self.args.stall_seconds, "Reference chain stopped advancing")
        self.report["tips"] = {"reference_height": ref["blocks"], "observer_height": obs["blocks"],
                               "common_height": height, "common_hash": block}
        self.passed("common_chain")
        self.checkpoint()

    def wait_synced(self):
        end = time.monotonic() + self.args.confirm_timeout
        while time.monotonic() < end:
            ref, obs = self.verify_chain(self.reference), self.verify_chain(self.observer)
            height = min(ref["blocks"], obs["blocks"])
            if ref["blocks"] == obs["blocks"] and self.reference.call("getblockhash", [height]) == self.observer.call("getblockhash", [height]):
                return
            self.tick()
            self.pause(.1 if self.args.self_mine else 2)
        raise CheckFailed("Observer did not synchronize")

    def prepare(self, rpc, outputs, inputs=None, subtract=False):
        wallet = self.wallet(rpc)
        if inputs is None:
            coins = [c for c in rpc.call("listunspent", [self.args.confirmations], wallet)
                     if c["scriptPubKey"].startswith("5220") and c["spendable"]]
            selected, total = [], Decimal(0)
            for coin in sorted(coins, key=lambda c: c["amount"], reverse=True):
                selected.append(coin)
                total += Decimal(str(coin["amount"]))
                if total >= sum(Decimal(str(v)) for v in outputs.values()) + Decimal(str(self.args.max_tx_fee)):
                    break
            require(total >= sum(Decimal(str(v)) for v in outputs.values()) + Decimal(str(self.args.max_tx_fee)),
                    "Insufficient confirmed PQ test funds")
            inputs = selected
        raw = rpc.call("createrawtransaction", [[{"txid": c["txid"], "vout": c["vout"]} for c in inputs], outputs])
        options = {"add_inputs": False, "change_type": "pq", "fee_rate": self.args.fee_rate}
        if subtract:
            options["subtractFeeFromOutputs"] = [0]
        funded = rpc.call("fundrawtransaction", [raw, options], wallet)
        fee = Decimal(str(funded["fee"]))
        require(fee <= Decimal(str(self.args.max_tx_fee)), "Per-transaction fee budget exceeded")
        require(Decimal(self.state["fees"]) + fee <= Decimal(str(self.args.max_total_fees)), "Total test fee budget exceeded")
        # A locked signer must refuse to sign before the runner unlocks it.
        try:
            locked = rpc.call("signrawtransactionwithwallet", [funded["hex"]], wallet)
        except RPCError as error:
            require(error.code == -13, "Unexpected locked-wallet error")
        else:
            require(not locked["complete"], "Locked encrypted wallet signed a transaction")
        self.passed("locked_signing_refused")
        rpc.call("walletpassphrase", [self.state["password"], 120], wallet)
        try:
            signed = rpc.call("signrawtransactionwithwallet", [funded["hex"]], wallet)
        finally:
            rpc.call("walletlock", wallet=wallet)
        require(signed["complete"], "PQ signing incomplete")
        decoded = rpc.call("decoderawtransaction", [signed["hex"]])
        require(len(decoded["vin"]) == len(inputs), "Unexpected transaction inputs")
        for vin in decoded["vin"]:
            require([len(w) // 2 for w in vin.get("txinwitness", [])] == SIZES, "Noncanonical PQ witness")
        require(all(o["scriptPubKey"]["hex"].startswith("5220") for o in decoded["vout"]), "Expected exclusively PQ outputs/change")
        signature = decoded["vin"][0]["txinwitness"][0]
        require(signed["hex"].count(signature) == 1, "Ambiguous signature mutation")
        damaged = bytes([int(signature[:2], 16) ^ 1]).hex() + signature[2:]
        bad = signed["hex"].replace(signature, damaged, 1)
        for endpoint in (self.reference, self.observer):
            valid = endpoint.call("testmempoolaccept", [[signed["hex"]], self.args.max_tx_fee])[0]
            require(valid["allowed"], "Valid PQ transaction rejected before broadcast")
            invalid = endpoint.call("testmempoolaccept", [[bad], self.args.max_tx_fee])[0]
            require(not invalid["allowed"] and "signature" in invalid.get("reject-reason", "").lower(),
                    "Damaged signature did not fail signature validation")
            self.passed("damaged_signature_rejected")
        self.passed("canonical_witness_and_pq_change")
        return {"hex": signed["hex"], "txid": decoded["txid"], "fee": str(fee), "inputs": len(inputs)}

    def transact(self, key, source, outputs=None, inputs=None, subtract=False):
        cycle = self.state["cycle"]
        if key not in cycle:
            cycle[key] = self.prepare(source, outputs, inputs, subtract)
            self.state["fees"] = str(Decimal(self.state["fees"]) + Decimal(cycle[key]["fee"]))
            # Persist signed bytes before broadcast, so resume reuses exactly the
            # same transaction and never allocates a second payment or fee.
            self.checkpoint()
        tx = cycle[key]
        if tx.get("confirmed"):
            return tx
        try:
            source.call("sendrawtransaction", [tx["hex"], self.args.max_tx_fee])
        except RPCError as error:
            require(error.code == -27, "Broadcast failed")  # already confirmed
        self.event("broadcast", cycle=cycle["number"], stage=key, txid=tx["txid"], inputs=tx["inputs"])
        end = time.monotonic() + self.args.confirm_timeout
        hard_deadline = self.state["deadline"] + 3 * self.args.confirm_timeout
        mined = 0
        while time.monotonic() < end and time.time() < hard_deadline:
            self.tick()
            self.monitor()
            records = []
            for rpc in (self.reference, self.observer):
                try:
                    records.append(rpc.call("gettransaction", [tx["txid"]], self.wallet(rpc)))
                except RPCError as error:
                    require(error.code == -5, "Wallet transaction query failed")
                    records.append({})  # P2P relay may not have reached the other wallet yet.
            if all(r.get("confirmations", 0) >= self.args.confirmations for r in records):
                require(records[0]["blockhash"] == records[1]["blockhash"], "Transaction inclusion block mismatch")
                tx.update(confirmed=True, blockhash=records[0]["blockhash"])
                self.report["transactions"].append({"cycle": cycle["number"], "stage": key,
                    "txid": tx["txid"], "blockhash": tx["blockhash"], "inputs": tx["inputs"], "fee": tx["fee"]})
                self.passed("confirmed_on_both_nodes")
                self.checkpoint()
                return tx
            if self.args.self_mine and mined < self.args.confirmations and (mined or tx["txid"] in self.reference.call("getrawmempool")):
                self.mine()
                mined += 1
            self.pause(.1 if self.args.self_mine else 5)
        raise CheckFailed("Transaction confirmation deadline exceeded")

    def recovery(self, number, before_spend=False):
        if before_spend and number % self.args.restore_every == 0 and not self.state["cycle"].get("restored"):
            rpc = self.observer
            wallet = self.wallet(rpc)
            addresses = self.state["cycle"]["receive"]
            before = sorted((c["txid"], c["vout"], c["amount"]) for c in rpc.call("listunspent", [0], wallet))
            backup = self.root / "backups" / f"cycle-{number}.dat"
            backup.parent.mkdir(exist_ok=True)
            rpc.call("backupwallet", [str(backup)], wallet)
            os.chmod(backup, 0o600)
            restored = self.state["wallet"] + f"-restored-{number}"
            rpc.call("unloadwallet", {"wallet_name": wallet, "load_on_startup": False})
            try:
                if any(w["name"] == restored for w in rpc.call("listwalletdir")["wallets"]):
                    rpc.call("loadwallet", {"filename": restored, "load_on_startup": False})
                else:
                    rpc.call("restorewallet", {"wallet_name": restored, "backup_file": str(backup), "load_on_startup": False})
                for address in addresses:
                    require(rpc.call("getaddressinfo", [address], restored)["ismine"], "Restored wallet lost an address")
                after = sorted((c["txid"], c["vout"], c["amount"]) for c in rpc.call("listunspent", [0], restored))
                require(before == after, "Restored wallet UTXO mismatch")
                require(rpc.call("getwalletinfo", wallet=restored)["unlocked_until"] == 0, "Restored wallet is not locked")
                # Prove the encrypted private root survived by signing a real,
                # currently spendable transaction and validating it on both nodes.
                rpc.call("walletpassphrase", [self.state["password"], 30], restored)
                coins = rpc.call("listunspent", [self.args.confirmations, 9999999, addresses], restored)
                require(len(coins) == 2, "Restored wallet lost funded PQ inputs")
                raw = rpc.call("createrawtransaction", [[{"txid": c["txid"], "vout": c["vout"]} for c in coins],
                                                       {self.state["cycle"]["return"]: 1.5}])
                funded = rpc.call("fundrawtransaction", [raw, {"add_inputs": False, "change_type": "pq", "fee_rate": self.args.fee_rate}], restored)
                require(Decimal(str(funded["fee"])) <= Decimal(str(self.args.max_tx_fee)), "Restored signing fee exceeded")
                signed = rpc.call("signrawtransactionwithwallet", [funded["hex"]], restored)
                require(signed["complete"], "Restored wallet could not sign PQ inputs")
                for endpoint in (self.reference, self.observer):
                    require(endpoint.call("testmempoolaccept", [[signed["hex"]], self.args.max_tx_fee])[0]["allowed"],
                            "Restored wallet signature did not verify")
                rpc.call("walletlock", wallet=restored)
                rpc.call("unloadwallet", {"wallet_name": restored, "load_on_startup": False})
            finally:
                rpc.call("loadwallet", {"filename": wallet, "load_on_startup": False})
            self.passed("encrypted_backup_restore")
            self.state["cycle"]["restored"] = True
            self.checkpoint()
        if not before_spend and number % self.args.restart_every == 0:
            self.stop_observer()
            self.start_observer()
            self.ensure_wallet(self.observer)
            self.wait_synced()
            self.passed("owned_observer_restart")
        if not before_spend and self.args.reorg_every and number % self.args.reorg_every == 0:
            require(self.args.chain == "regtest", "Reorgs are isolated-regtest-only")
            tip = self.reference.call("getbestblockhash")
            for rpc in (self.reference, self.observer):
                rpc.call("invalidateblock", [tip])
            require(self.reference.call("getbestblockhash") == self.observer.call("getbestblockhash"), "Regtest disconnect mismatch")
            for rpc in (self.reference, self.observer):
                rpc.call("reconsiderblock", [tip])
            self.wait_synced()
            require(self.reference.call("getbestblockhash") == tip, "Regtest reconnect failed")
            self.passed("isolated_block_disconnect_reconnect")

    def cycle(self):
        if self.state["cycle"] is None:
            self.state["cycle"] = {"number": self.state["completed_cycles"] + 1,
                                   "receive": [self.address(self.observer), self.address(self.observer)],
                                   "return": self.address(self.reference)}
            self.checkpoint()
        cycle = self.state["cycle"]
        require(len(set(cycle["receive"])) == 2, "PQ receive addresses were reused")
        funding = self.transact("funding", self.reference, {a: 1 for a in cycle["receive"]})
        self.recovery(cycle["number"], before_spend=True)
        if "multi" not in cycle:
            coins = self.observer.call("listunspent", [self.args.confirmations, 9999999, cycle["receive"]], self.wallet(self.observer))
            require(len(coins) == 2 and all(c["txid"] == funding["txid"] for c in coins), "Missing two dedicated PQ inputs")
        else:
            coins = None
        multi = self.transact("multi", self.observer, {cycle["return"]: 1.5}, coins)
        if "single" not in cycle:
            coins = [c for c in self.observer.call("listunspent", [self.args.confirmations], self.wallet(self.observer)) if c["txid"] == multi["txid"]]
            require(len(coins) == 1, "Missing dedicated PQ change for single-input return")
            outputs = {cycle["return"]: coins[0]["amount"]}
        else:
            coins, outputs = None, None
        self.transact("single", self.observer, outputs, coins, subtract=True)
        self.recovery(cycle["number"])
        self.state["completed_cycles"] += 1
        self.state["cycle"] = None
        self.checkpoint()
        self.event("cycle_complete", cycle=self.state["completed_cycles"], fees=self.state["fees"])

    def run(self):
        self.acquire()
        self.verify_chain(self.reference)  # validate before touching either wallet
        self.reference_verified = True
        self.start_observer()
        reference_address = self.ensure_wallet(self.reference)
        self.ensure_wallet(self.observer)
        self.wait_synced()
        if "funding_address" not in self.state:
            self.state["funding_address"] = reference_address
            self.report["funding_address"] = reference_address
            self.checkpoint()
        if self.args.self_mine and not self.state.get("seeded"):
            self.mine(101)
            self.reference.call("walletpassphrase", [self.state["password"], 120], self.wallet(self.reference))
            try:
                self.reference.call("sendtoaddress", {"address": self.state["funding_address"], "amount": 100, "fee_rate": self.args.fee_rate}, self.wallet(self.reference))
            finally:
                self.reference.call("walletlock", wallet=self.wallet(self.reference))
            self.mine(self.args.confirmations)
            self.wait_synced()
            self.state["seeded"] = True
            self.checkpoint()
        self.event("ready", funding_address=self.state["funding_address"], deadline=self.state["deadline"])
        funding_end = time.monotonic() + self.args.confirm_timeout
        while not [c for c in self.reference.call("listunspent", [self.args.confirmations], self.wallet(self.reference)) if c["scriptPubKey"].startswith("5220")]:
            require(time.monotonic() < funding_end, "Initial testnet funding deadline exceeded")
            self.monitor()
            self.pause(5)
        self.report.setdefault("start_height", self.reference.call("getblockcount"))
        while time.time() < self.state["deadline"] and (not self.args.cycles or self.state["completed_cycles"] < self.args.cycles):
            self.monitor()
            self.cycle()
            end = min(time.monotonic() + self.args.interval, time.monotonic() + max(0, self.state["deadline"] - time.time()))
            while time.monotonic() < end and (not self.args.cycles or self.state["completed_cycles"] < self.args.cycles):
                self.monitor()
                self.pause(min(30, max(0, end - time.monotonic())))
        require(self.state["completed_cycles"] >= self.args.min_cycles, "Minimum completed-cycle gate not reached")
        require(self.reference.call("getblockcount") - self.report["start_height"] >= self.args.min_blocks, "Minimum new-block gate not reached")
        for name in ("encrypted_backup_restore", "owned_observer_restart", "locked_signing_refused", "damaged_signature_rejected"):
            require(self.report["checks"].get(name, 0) > 0, "Missing required coverage: " + name)
        self.report["status"] = "passed"
        self.checkpoint()
        self.event("campaign_passed", cycles=self.state["completed_cycles"])

    def close(self):
        self.stop_observer()
        if self.state is not None and self.reference_verified:
            # Unload only the newly created campaign wallet on the reference.
            try:
                self.verify_chain(self.reference)
                wallet = self.wallet(self.reference)
                self.reference.call("walletlock", wallet=wallet)
                self.reference.call("unloadwallet", {"wallet_name": wallet, "load_on_startup": False})
            except (OSError, RPCError, CheckFailed):
                pass
        if self.state is not None:
            self.checkpoint()
        if self.lock_fd is not None:
            os.close(self.lock_fd)
            self.lock.unlink()


def arguments():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--binary", required=True)
    p.add_argument("--state-dir", required=True)
    p.add_argument("--reference-url", required=True)
    p.add_argument("--reference-cookie", required=True)
    p.add_argument("--chain", choices=("test", "regtest"), default="test")
    p.add_argument("--peer", action="append", required=True)
    p.add_argument("--rpc-port", type=int, default=19432)
    p.add_argument("--p2p-port", type=int, default=19433)
    p.add_argument("--duration", type=int, default=72 * 3600)
    p.add_argument("--interval", type=int, default=600)
    p.add_argument("--confirm-timeout", type=int, default=2700)
    p.add_argument("--stall-seconds", type=int, default=2700)
    p.add_argument("--confirmations", type=int, default=2)
    p.add_argument("--cycles", type=int, default=0, help="Optional early stop for isolated validation")
    p.add_argument("--min-cycles", type=int, default=24)
    p.add_argument("--min-blocks", type=int, default=12)
    p.add_argument("--restart-every", type=int, default=6)
    p.add_argument("--restore-every", type=int, default=12)
    p.add_argument("--fee-rate", type=int, default=2000)
    p.add_argument("--max-tx-fee", type=float, default=.1)
    p.add_argument("--max-total-fees", type=float, default=75)
    p.add_argument("--min-free-mib", type=int, default=1024)
    p.add_argument("--max-state-mib", type=int, default=512)
    p.add_argument("--resume", action="store_true")
    p.add_argument("--self-mine", action="store_true", help="Isolated regtest only")
    p.add_argument("--reorg-every", type=int, default=0, help="Isolated regtest only")
    args = p.parse_args()
    require(not (args.self_mine or args.reorg_every or args.cycles) or args.chain == "regtest", "Mining/reorg/early-stop exercises require isolated regtest")
    require(all(getattr(args, k) > 0 for k in ("duration", "confirm_timeout", "stall_seconds", "confirmations", "min_cycles", "min_blocks", "restart_every", "restore_every", "fee_rate", "max_tx_fee", "max_total_fees", "min_free_mib", "max_state_mib")), "Invalid campaign limits")
    require(args.interval >= 0 and args.cycles >= 0 and args.reorg_every >= 0, "Invalid interval/cycle count")
    return args


def main():
    campaign = Campaign(arguments())
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: setattr(campaign, "stopping", True))
    code = 0
    try:
        campaign.run()
    except Exception as error:
        code = 1
        campaign.report["status"] = "interrupted" if campaign.stopping else "failed"
        # Do not serialize arbitrary exceptions (HTTP auth, private paths, or RPC input).
        campaign.report["failure"] = str(error) if isinstance(error, (CheckFailed, RPCError)) else type(error).__name__
        if campaign.lock_fd is not None:
            campaign.event("campaign_failed", reason=campaign.report["failure"])
        else:
            print(json.dumps({"event": "campaign_not_started", "reason": type(error).__name__}), file=sys.stderr)
    finally:
        campaign.close()
    return code


if __name__ == "__main__":
    sys.exit(main())
