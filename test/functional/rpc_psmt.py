#!/usr/bin/env python3
# Copyright (c) 2018-2022 The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the Partially Signed Transaction RPCs.
"""
from decimal import Decimal
from itertools import product
from random import randbytes

from test_framework.blocktools import (
    MAX_STANDARD_TX_WEIGHT,
)
from test_framework.descriptors import descsum_create
from test_framework.key import H_POINT
from test_framework.messages import (
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    MAX_BIP125_RBF_SEQUENCE,
    WITNESS_SCALE_FACTOR,
)
from test_framework.psmt import (
    PSMT,
    PSMTMap,
    PSMT_GLOBAL_UNSIGNED_TX,
    PSMT_IN_RIPEMD160,
    PSMT_IN_SHA256,
    PSMT_IN_SIGHASH_TYPE,
    PSMT_IN_HASH160,
    PSMT_IN_HASH256,
    PSMT_IN_MUSIG2_PARTIAL_SIG,
    PSMT_IN_MUSIG2_PARTICIPANT_PUBKEYS,
    PSMT_IN_MUSIG2_PUB_NONCE,
    PSMT_IN_NON_WITNESS_UTXO,
    PSMT_IN_WITNESS_UTXO,
    PSMT_OUT_MUSIG2_PARTICIPANT_PUBKEYS,
    PSMT_OUT_TAP_TREE,
)
from test_framework.script import CScript, OP_TRUE, SIGHASH_ALL, SIGHASH_ANYONECANPAY
from test_framework.script_util import MIN_STANDARD_TX_NONWITNESS_SIZE
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_not_equal,
    assert_approx,
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_raises_rpc_error,
    find_vout_for_address,
    wallet_importprivkey,
)
from test_framework.wallet_util import (
    calculate_input_weight,
    generate_keypair,
    get_generate_key,
)

import json
import os


class PSMTTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [
            ["-walletrbf=1", "-addresstype=bech32", "-changetype=bech32"], #TODO: Remove address type restrictions once taproot has psmt extensions
            ["-walletrbf=0", "-changetype=legacy"],
            []
        ]
        # whitelist peers to speed up tx relay / mempool sync
        for args in self.extra_args:
            args.append("-whitelist=noban@127.0.0.1")

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def test_psmt_incomplete_after_invalid_modification(self):
        self.log.info("Check that PSMT is correctly marked as incomplete after invalid modification")
        node = self.nodes[2]
        wallet = node.get_wallet_rpc(self.default_wallet_name)
        address = wallet.getnewaddress()
        wallet.sendtoaddress(address=address, amount=1.0)
        self.generate(node, nblocks=1)

        utxos = wallet.listunspent(addresses=[address])
        psmt = wallet.createpsmt([{"txid": utxos[0]["txid"], "vout": utxos[0]["vout"]}], [{wallet.getnewaddress(): 0.9999}])
        signed_psmt = wallet.walletprocesspsmt(psmt)["psmt"]

        # Modify the raw transaction by changing the output address, so the signature is no longer valid
        signed_psmt_obj = PSMT.from_base64(signed_psmt)
        substitute_addr = wallet.getnewaddress()
        raw = wallet.createrawtransaction([{"txid": utxos[0]["txid"], "vout": utxos[0]["vout"]}], [{substitute_addr: 0.9999}])
        signed_psmt_obj.g.map[PSMT_GLOBAL_UNSIGNED_TX] = bytes.fromhex(raw)

        # Check that the walletprocesspsmt call succeeds but also recognizes that the transaction is not complete
        signed_psmt_incomplete = wallet.walletprocesspsmt(psmt=signed_psmt_obj.to_base64(), finalize=False)
        assert signed_psmt_incomplete["complete"] is False

    def test_utxo_conversion(self):
        self.log.info("Check that non-witness UTXOs are removed for segwit v1+ inputs")
        mining_node = self.nodes[2]
        offline_node = self.nodes[0]
        online_node = self.nodes[1]

        # Disconnect offline node from others
        # Topology of test network is linear, so this one call is enough
        self.disconnect_nodes(0, 1)

        # Create watchonly on online_node
        online_node.createwallet(wallet_name='wonline', disable_private_keys=True)
        wonline = online_node.get_wallet_rpc('wonline')
        w2 = online_node.get_wallet_rpc(self.default_wallet_name)

        # Mine a transaction that credits the offline address
        offline_addr = offline_node.getnewaddress(address_type="bech32m")
        online_addr = w2.getnewaddress(address_type="bech32m")
        import_res = wonline.importdescriptors([{"desc": offline_node.getaddressinfo(offline_addr)["desc"], "timestamp": "now"}])
        assert_equal(import_res[0]["success"], True)
        mining_wallet = mining_node.get_wallet_rpc(self.default_wallet_name)
        mining_wallet.sendtoaddress(address=offline_addr, amount=1.0)
        self.generate(mining_node, nblocks=1, sync_fun=lambda: self.sync_all([online_node, mining_node]))

        # Construct an unsigned PSMT on the online node
        utxos = wonline.listunspent(addresses=[offline_addr])
        raw = wonline.createrawtransaction([{"txid":utxos[0]["txid"], "vout":utxos[0]["vout"]}],[{online_addr:0.9999}])
        psmt = wonline.walletprocesspsmt(online_node.converttopsmt(raw))["psmt"]
        assert not "not_witness_utxo" in mining_node.decodepsmt(psmt)["inputs"][0]

        # add non-witness UTXO manually
        psmt_new = PSMT.from_base64(psmt)
        prev_tx = wonline.gettransaction(utxos[0]["txid"])["hex"]
        psmt_new.i[0].map[PSMT_IN_NON_WITNESS_UTXO] = bytes.fromhex(prev_tx)
        assert "non_witness_utxo" in mining_node.decodepsmt(psmt_new.to_base64())["inputs"][0]

        # Have the offline node sign the PSMT (which will remove the non-witness UTXO)
        signed_psmt = offline_node.walletprocesspsmt(psmt_new.to_base64())
        assert not "non_witness_utxo" in mining_node.decodepsmt(signed_psmt["psmt"])["inputs"][0]

        # Make sure we can mine the resulting transaction
        txid = mining_node.sendrawtransaction(signed_psmt["hex"])
        self.generate(mining_node, nblocks=1, sync_fun=lambda: self.sync_all([online_node, mining_node]))
        assert_equal(online_node.gettxout(txid,0)["confirmations"], 1)

        wonline.unloadwallet()

        # Reconnect
        self.connect_nodes(1, 0)
        self.connect_nodes(0, 2)

    def test_input_confs_control(self):
        self.nodes[0].createwallet("minconf")
        wallet = self.nodes[0].get_wallet_rpc("minconf")

        # Fund the wallet with different chain heights
        for _ in range(2):
            self.nodes[1].sendmany("", {wallet.getnewaddress():1, wallet.getnewaddress():1})
            self.generate(self.nodes[1], 1)

        unconfirmed_txid = wallet.sendtoaddress(wallet.getnewaddress(), 0.5)

        self.log.info("Crafting PSMT using an unconfirmed input")
        target_address = self.nodes[1].getnewaddress()
        psmtx1 = wallet.walletcreatefundedpsmt([], {target_address: 0.1}, 0, {'fee_rate': 1, 'maxconf': 0})['psmt']

        # Make sure we only had the one input
        tx1_inputs = self.nodes[0].decodepsmt(psmtx1)['tx']['vin']
        assert_equal(len(tx1_inputs), 1)

        utxo1 = tx1_inputs[0]
        assert_equal(unconfirmed_txid, utxo1['txid'])

        signed_tx1 = wallet.walletprocesspsmt(psmtx1)
        txid1 = self.nodes[0].sendrawtransaction(signed_tx1['hex'])

        mempool = self.nodes[0].getrawmempool()
        assert txid1 in mempool

        self.log.info("Fail to craft a new PSMT that sends more funds with add_inputs = False")
        assert_raises_rpc_error(-4, "The preselected coins total amount does not cover the transaction target. Please allow other inputs to be automatically selected or include more coins manually", wallet.walletcreatefundedpsmt, [{'txid': utxo1['txid'], 'vout': utxo1['vout']}], {target_address: 1}, 0, {'add_inputs': False})

        self.log.info("Fail to craft a new PSMT with minconf above highest one")
        assert_raises_rpc_error(-4, "Insufficient funds", wallet.walletcreatefundedpsmt, [{'txid': utxo1['txid'], 'vout': utxo1['vout']}], {target_address: 1}, 0, {'add_inputs': True, 'minconf': 3, 'fee_rate': 10})

        self.log.info("Fail to broadcast a new PSMT with maxconf 0 due to BIP125 rules to verify it actually chose unconfirmed outputs")
        psmt_invalid = wallet.walletcreatefundedpsmt([{'txid': utxo1['txid'], 'vout': utxo1['vout']}], {target_address: 1}, 0, {'add_inputs': True, 'maxconf': 0, 'fee_rate': 10})['psmt']
        signed_invalid = wallet.walletprocesspsmt(psmt_invalid)
        assert_raises_rpc_error(-26, "bad-txns-spends-conflicting-tx", self.nodes[0].sendrawtransaction, signed_invalid['hex'])

        self.log.info("Craft a replacement adding inputs with highest confs possible")
        psmtx2 = wallet.walletcreatefundedpsmt([{'txid': utxo1['txid'], 'vout': utxo1['vout']}], {target_address: 1}, 0, {'add_inputs': True, 'minconf': 2, 'fee_rate': 10})['psmt']
        tx2_inputs = self.nodes[0].decodepsmt(psmtx2)['tx']['vin']
        assert_greater_than_or_equal(len(tx2_inputs), 2)
        for vin in tx2_inputs:
            if vin['txid'] != unconfirmed_txid:
                assert_greater_than_or_equal(self.nodes[0].gettxout(vin['txid'], vin['vout'])['confirmations'], 2)

        signed_tx2 = wallet.walletprocesspsmt(psmtx2)
        txid2 = self.nodes[0].sendrawtransaction(signed_tx2['hex'])

        mempool = self.nodes[0].getrawmempool()
        assert txid1 not in mempool
        assert txid2 in mempool

        wallet.unloadwallet()

    def test_decodepsmt_musig2_input_output_types(self):
        self.log.info("Test decoding PSMT with MuSig2 per-input and per-output types")
        # create 2-of-2 musig2 using fake aggregate key, leaf hash, pubnonce, and partial sig
        # TODO: actually implement MuSig2 aggregation (for decoding only it doesn't matter though)
        _, in_pubkey1 = generate_keypair()
        _, in_pubkey2 = generate_keypair()
        _, in_fake_agg_pubkey = generate_keypair()
        fake_leaf_hash = randbytes(32)
        fake_pubnonce = randbytes(66)
        fake_partialsig = randbytes(32)
        tx = CTransaction()
        tx.vin = [CTxIn(outpoint=COutPoint(hash=int('ee' * 32, 16), n=0), scriptSig=b"")]
        tx.vout = [CTxOut(nValue=0, scriptPubKey=b"")]
        psmt = PSMT()
        psmt.g = PSMTMap({PSMT_GLOBAL_UNSIGNED_TX: tx.serialize()})
        participant1_keydata = in_pubkey1 + in_fake_agg_pubkey + fake_leaf_hash
        psmt.i = [PSMTMap({
                    bytes([PSMT_IN_MUSIG2_PARTICIPANT_PUBKEYS]) + in_fake_agg_pubkey: [in_pubkey1, in_pubkey2],
                    bytes([PSMT_IN_MUSIG2_PUB_NONCE]) + participant1_keydata: fake_pubnonce,
                    bytes([PSMT_IN_MUSIG2_PARTIAL_SIG]) + participant1_keydata: fake_partialsig,
                 })]
        _, out_pubkey1 = generate_keypair()
        _, out_pubkey2 = generate_keypair()
        _, out_fake_agg_pubkey = generate_keypair()
        psmt.o = [PSMTMap({
                    bytes([PSMT_OUT_MUSIG2_PARTICIPANT_PUBKEYS]) + out_fake_agg_pubkey: [out_pubkey1, out_pubkey2],
                 })]
        res = self.nodes[0].decodepsmt(psmt.to_base64())
        assert_equal(len(res["inputs"]), 1)
        res_input = res["inputs"][0]
        assert_equal(len(res["outputs"]), 1)
        res_output = res["outputs"][0]

        assert "musig2_participant_pubkeys" in res_input
        in_participant_pks = res_input["musig2_participant_pubkeys"][0]
        assert "aggregate_pubkey" in in_participant_pks
        assert_equal(in_participant_pks["aggregate_pubkey"], in_fake_agg_pubkey.hex())
        assert "participant_pubkeys" in in_participant_pks
        assert_equal(in_participant_pks["participant_pubkeys"], [in_pubkey1.hex(), in_pubkey2.hex()])

        assert "musig2_pubnonces" in res_input
        in_pubnonce = res_input["musig2_pubnonces"][0]
        assert "participant_pubkey" in in_pubnonce
        assert_equal(in_pubnonce["participant_pubkey"], in_pubkey1.hex())
        assert "aggregate_pubkey" in in_pubnonce
        assert_equal(in_pubnonce["aggregate_pubkey"], in_fake_agg_pubkey.hex())
        assert "leaf_hash" in in_pubnonce
        assert_equal(in_pubnonce["leaf_hash"], fake_leaf_hash.hex())
        assert "pubnonce" in in_pubnonce
        assert_equal(in_pubnonce["pubnonce"], fake_pubnonce.hex())

        assert "musig2_partial_sigs" in res_input
        in_partialsig = res_input["musig2_partial_sigs"][0]
        assert "participant_pubkey" in in_partialsig
        assert_equal(in_partialsig["participant_pubkey"], in_pubkey1.hex())
        assert "aggregate_pubkey" in in_partialsig
        assert_equal(in_partialsig["aggregate_pubkey"], in_fake_agg_pubkey.hex())
        assert "leaf_hash" in in_partialsig
        assert_equal(in_partialsig["leaf_hash"], fake_leaf_hash.hex())
        assert "partial_sig" in in_partialsig
        assert_equal(in_partialsig["partial_sig"], fake_partialsig.hex())

        assert "musig2_participant_pubkeys" in res_output
        out_participant_pks = res_output["musig2_participant_pubkeys"][0]
        assert "aggregate_pubkey" in out_participant_pks
        assert_equal(out_participant_pks["aggregate_pubkey"], out_fake_agg_pubkey.hex())
        assert "participant_pubkeys" in out_participant_pks
        assert_equal(out_participant_pks["participant_pubkeys"], [out_pubkey1.hex(), out_pubkey2.hex()])

    def test_sighash_mismatch(self):
        self.log.info("Test sighash type mismatches")
        self.nodes[0].createwallet("sighash_mismatch")
        wallet = self.nodes[0].get_wallet_rpc("sighash_mismatch")
        def_wallet = self.nodes[0].get_wallet_rpc(self.default_wallet_name)

        addr = wallet.getnewaddress(address_type="bech32")
        def_wallet.sendtoaddress(addr, 5)
        self.generate(self.nodes[0], 6)

        # Retrieve the descriptors so we can do all of the tests with descriptorprocesspsmt as well
        descs = wallet.listdescriptors(True)["descriptors"]

        # Make a PSMT
        psmt = wallet.walletcreatefundedpsmt([], [{def_wallet.getnewaddress(): 1}])["psmt"]

        # Modify the PSMT and insert a sighash field for ALL|ANYONECANPAY on input 0
        mod_psmt = PSMT.from_base64(psmt)
        mod_psmt.i[0].map[PSMT_IN_SIGHASH_TYPE] = (SIGHASH_ALL | SIGHASH_ANYONECANPAY).to_bytes(4, byteorder="little")
        psmt = mod_psmt.to_base64()

        # Mismatching sighash type fails, including when no type is specified
        for sighash in ["DEFAULT", "ALL", "NONE", "SINGLE", "NONE|ANYONECANPAY", "SINGLE|ANYONECANPAY", None]:
            assert_raises_rpc_error(-22, "Specified sighash value does not match value stored in PSMT", wallet.walletprocesspsmt, psmt, True, sighash)

        # Matching sighash type succeeds
        proc = wallet.walletprocesspsmt(psmt, True, "ALL|ANYONECANPAY")
        assert_equal(proc["complete"], True)

        # Repeat with descriptorprocesspsmt
        # Mismatching sighash type fails, including when no type is specified
        for sighash in ["DEFAULT", "ALL", "NONE", "SINGLE", "NONE|ANYONECANPAY", "SINGLE|ANYONECANPAY", None]:
            assert_raises_rpc_error(-22, "Specified sighash value does not match value stored in PSMT", self.nodes[0].descriptorprocesspsmt, psmt, descs, sighash)

        # Matching sighash type succeeds
        proc = self.nodes[0].descriptorprocesspsmt(psmt, descs, "ALL|ANYONECANPAY")
        assert_equal(proc["complete"], True)

        wallet.unloadwallet()

    def test_sighash_adding(self):
        self.log.info("Test adding of sighash type field")
        self.nodes[0].createwallet("sighash_adding")
        wallet = self.nodes[0].get_wallet_rpc("sighash_adding")
        def_wallet = self.nodes[0].get_wallet_rpc(self.default_wallet_name)

        outputs = [{wallet.getnewaddress(address_type="bech32"): 1}]
        outputs.append({wallet.getnewaddress(address_type="bech32m"): 1})
        descs = wallet.listdescriptors(True)["descriptors"]
        def_wallet.send(outputs)
        self.generate(self.nodes[0], 6)
        utxos = wallet.listunspent()

        # Make a PSMT
        psmt = wallet.walletcreatefundedpsmt(utxos, [{def_wallet.getnewaddress(): 0.5}])["psmt"]

        # Process the PSMT with the wallet
        wallet_psmt = wallet.walletprocesspsmt(psmt=psmt, sighashtype="ALL|ANYONECANPAY", finalize=False)["psmt"]

        # Separately process the PSMT with descriptors
        desc_psmt = self.nodes[0].descriptorprocesspsmt(psmt=psmt, descriptors=descs, sighashtype="ALL|ANYONECANPAY", finalize=False)["psmt"]

        for psmt in [wallet_psmt, desc_psmt]:
            # Check that the PSMT has a sighash field on all inputs
            dec_psmt = self.nodes[0].decodepsmt(psmt)
            for input in dec_psmt["inputs"]:
                assert_equal(input["sighash"], "ALL|ANYONECANPAY")

            # Make sure we can still finalize the transaction
            fin_res = self.nodes[0].finalizepsmt(psmt)
            assert_equal(fin_res["complete"], True)
            fin_hex = fin_res["hex"]
            assert_equal(self.nodes[0].testmempoolaccept([fin_hex])[0]["allowed"], True)

            # Change the sighash field to a different value and make sure we can no longer finalize
            mod_psmt = PSMT.from_base64(psmt)
            mod_psmt.i[0].map[PSMT_IN_SIGHASH_TYPE] = (SIGHASH_ALL).to_bytes(4, byteorder="little")
            mod_psmt.i[1].map[PSMT_IN_SIGHASH_TYPE] = (SIGHASH_ALL).to_bytes(4, byteorder="little")
            psmt = mod_psmt.to_base64()
            fin_res = self.nodes[0].finalizepsmt(psmt)
            assert_equal(fin_res["complete"], False)

        self.nodes[0].sendrawtransaction(fin_hex)
        self.generate(self.nodes[0], 1)

        wallet.unloadwallet()

    def assert_change_type(self, psmtx, expected_type):
        """Assert that the given PSMT has a change output with the given type."""

        # The decodepsmt RPC is stateless and independent of any settings, we can always just call it on the first node
        decoded_psmt = self.nodes[0].decodepsmt(psmtx["psmt"])
        changepos = psmtx["changepos"]
        assert_equal(decoded_psmt["tx"]["vout"][changepos]["scriptPubKey"]["type"], expected_type)

    def run_test(self):
        # Create and fund a raw tx for sending 10 MEWC
        psmtx1 = self.nodes[0].walletcreatefundedpsmt([], {self.nodes[2].getnewaddress():10})['psmt']

        self.log.info("Test for invalid maximum transaction weights")
        dest_arg = [{self.nodes[0].getnewaddress(): 1}]
        min_tx_weight = MIN_STANDARD_TX_NONWITNESS_SIZE * WITNESS_SCALE_FACTOR
        assert_raises_rpc_error(-4, f"Maximum transaction weight must be between {min_tx_weight} and {MAX_STANDARD_TX_WEIGHT}", self.nodes[0].walletcreatefundedpsmt, [], dest_arg, 0, {"max_tx_weight": -1})
        assert_raises_rpc_error(-4, f"Maximum transaction weight must be between {min_tx_weight} and {MAX_STANDARD_TX_WEIGHT}", self.nodes[0].walletcreatefundedpsmt, [], dest_arg, 0, {"max_tx_weight": 0})
        assert_raises_rpc_error(-4, f"Maximum transaction weight must be between {min_tx_weight} and {MAX_STANDARD_TX_WEIGHT}", self.nodes[0].walletcreatefundedpsmt, [], dest_arg, 0, {"max_tx_weight": MAX_STANDARD_TX_WEIGHT + 1})

        # Base transaction vsize: version (4) + locktime (4) + input count (1) + witness overhead (1) = 10 vbytes
        base_tx_vsize = 10
        # One P2WPKH output vsize: outpoint (31 vbytes)
        p2wpkh_output_vsize = 31
        # 1 vbyte for output count
        output_count = 1
        tx_weight_without_inputs = (base_tx_vsize + output_count + p2wpkh_output_vsize) * WITNESS_SCALE_FACTOR
        # min_tx_weight is greater than transaction weight without inputs
        assert_greater_than(min_tx_weight, tx_weight_without_inputs)

        # In order to test for when the passed max weight is less than the transaction weight without inputs
        # Define destination with two outputs.
        dest_arg_large = [{self.nodes[0].getnewaddress(): 1}, {self.nodes[0].getnewaddress(): 1}]
        large_tx_vsize_without_inputs = base_tx_vsize + output_count + (p2wpkh_output_vsize * 2)
        large_tx_weight_without_inputs = large_tx_vsize_without_inputs * WITNESS_SCALE_FACTOR
        assert_greater_than(large_tx_weight_without_inputs, min_tx_weight)
        # Test for max_tx_weight less than Transaction weight without inputs
        assert_raises_rpc_error(-4, "Maximum transaction weight is less than transaction weight without inputs", self.nodes[0].walletcreatefundedpsmt, [], dest_arg_large, 0, {"max_tx_weight": min_tx_weight})
        assert_raises_rpc_error(-4, "Maximum transaction weight is less than transaction weight without inputs", self.nodes[0].walletcreatefundedpsmt, [], dest_arg_large, 0, {"max_tx_weight": large_tx_weight_without_inputs})

        # Test for max_tx_weight just enough to include inputs but not change output
        assert_raises_rpc_error(-4, "Maximum transaction weight is too low, can not accommodate change output", self.nodes[0].walletcreatefundedpsmt, [], dest_arg_large, 0, {"max_tx_weight": (large_tx_vsize_without_inputs + 1) * WITNESS_SCALE_FACTOR})
        self.log.info("Test that a funded PSMT is always faithful to max_tx_weight option")
        large_tx_vsize_with_change = large_tx_vsize_without_inputs + p2wpkh_output_vsize
        # It's enough but won't accommodate selected input size
        assert_raises_rpc_error(-4, "The inputs size exceeds the maximum weight", self.nodes[0].walletcreatefundedpsmt, [], dest_arg_large, 0, {"max_tx_weight": (large_tx_vsize_with_change) * WITNESS_SCALE_FACTOR})

        max_tx_weight_sufficient = 1000 # 1k vbytes is enough
        psmt = self.nodes[0].walletcreatefundedpsmt(outputs=dest_arg,locktime=0, options={"max_tx_weight": max_tx_weight_sufficient})["psmt"]
        weight = self.nodes[0].decodepsmt(psmt)["tx"]["weight"]
        # ensure the transaction's weight is below the specified max_tx_weight.
        assert_greater_than_or_equal(max_tx_weight_sufficient, weight)

        # If inputs are specified, do not automatically add more:
        utxo1 = self.nodes[0].listunspent()[0]
        assert_raises_rpc_error(-4, "The preselected coins total amount does not cover the transaction target. "
                                    "Please allow other inputs to be automatically selected or include more coins manually",
                                self.nodes[0].walletcreatefundedpsmt, [{"txid": utxo1['txid'], "vout": utxo1['vout']}], {self.nodes[2].getnewaddress():90})

        psmtx1 = self.nodes[0].walletcreatefundedpsmt([{"txid": utxo1['txid'], "vout": utxo1['vout']}], {self.nodes[2].getnewaddress():90}, 0, {"add_inputs": True})['psmt']
        assert_equal(len(self.nodes[0].decodepsmt(psmtx1)['tx']['vin']), 2)

        # Inputs argument can be null
        self.nodes[0].walletcreatefundedpsmt(None, {self.nodes[2].getnewaddress():10})

        # Node 1 should not be able to add anything to it but still return the psmtx same as before
        psmtx = self.nodes[1].walletprocesspsmt(psmtx1)['psmt']
        assert_equal(psmtx1, psmtx)

        # Node 0 should not be able to sign the transaction with the wallet is locked
        self.nodes[0].encryptwallet("password")
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase with walletpassphrase first", self.nodes[0].walletprocesspsmt, psmtx)

        # Node 0 should be able to process without signing though
        unsigned_tx = self.nodes[0].walletprocesspsmt(psmtx, False)
        assert_equal(unsigned_tx['complete'], False)

        self.nodes[0].walletpassphrase(passphrase="password", timeout=1000000)

        # Sign the transaction but don't finalize
        processed_psmt = self.nodes[0].walletprocesspsmt(psmt=psmtx, finalize=False)
        assert "hex" not in processed_psmt
        signed_psmt = processed_psmt['psmt']

        # Finalize and send
        finalized_hex = self.nodes[0].finalizepsmt(signed_psmt)['hex']
        self.nodes[0].sendrawtransaction(finalized_hex)

        # Alternative method: sign AND finalize in one command
        processed_finalized_psmt = self.nodes[0].walletprocesspsmt(psmt=psmtx, finalize=True)
        finalized_psmt = processed_finalized_psmt['psmt']
        finalized_psmt_hex = processed_finalized_psmt['hex']
        assert_not_equal(signed_psmt, finalized_psmt)
        assert finalized_psmt_hex == finalized_hex

        # Manually selected inputs can be locked:
        assert_equal(len(self.nodes[0].listlockunspent()), 0)
        utxo1 = self.nodes[0].listunspent()[0]
        psmtx1 = self.nodes[0].walletcreatefundedpsmt([{"txid": utxo1['txid'], "vout": utxo1['vout']}], {self.nodes[2].getnewaddress():1}, 0,{"lockUnspents": True})["psmt"]
        assert_equal(len(self.nodes[0].listlockunspent()), 1)

        # Locks are ignored for manually selected inputs
        self.nodes[0].walletcreatefundedpsmt([{"txid": utxo1['txid'], "vout": utxo1['vout']}], {self.nodes[2].getnewaddress():1}, 0)

        # Create p2sh, p2wpkh, and p2wsh addresses
        pubkey0 = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())['pubkey']
        pubkey1 = self.nodes[1].getaddressinfo(self.nodes[1].getnewaddress())['pubkey']
        pubkey2 = self.nodes[2].getaddressinfo(self.nodes[2].getnewaddress())['pubkey']

        # Setup watchonly wallets
        self.nodes[2].createwallet(wallet_name='wmulti', disable_private_keys=True)
        wmulti = self.nodes[2].get_wallet_rpc('wmulti')

        # Create all the addresses
        p2sh_ms = wmulti.createmultisig(2, [pubkey0, pubkey1, pubkey2], address_type="legacy")
        p2sh = p2sh_ms["address"]
        p2wsh_ms = wmulti.createmultisig(2, [pubkey0, pubkey1, pubkey2], address_type="bech32")
        p2wsh = p2wsh_ms["address"]
        p2sh_p2wsh_ms = wmulti.createmultisig(2, [pubkey0, pubkey1, pubkey2], address_type="p2sh-segwit")
        p2sh_p2wsh = p2sh_p2wsh_ms["address"]
        import_res = wmulti.importdescriptors(
            [
                {"desc": p2sh_ms["descriptor"], "timestamp": "now"},
                {"desc": p2wsh_ms["descriptor"], "timestamp": "now"},
                {"desc": p2sh_p2wsh_ms["descriptor"], "timestamp": "now"},
            ])
        assert_equal(all([r["success"] for r in import_res]), True)
        p2wpkh = self.nodes[1].getnewaddress("", "bech32")
        p2pkh = self.nodes[1].getnewaddress("", "legacy")
        p2sh_p2wpkh = self.nodes[1].getnewaddress("", "p2sh-segwit")

        # fund those addresses
        rawtx = self.nodes[0].createrawtransaction([], {p2sh:10, p2wsh:10, p2wpkh:10, p2sh_p2wsh:10, p2sh_p2wpkh:10, p2pkh:10})
        rawtx = self.nodes[0].fundrawtransaction(rawtx, {"changePosition":3})
        signed_tx = self.nodes[0].signrawtransactionwithwallet(rawtx['hex'])['hex']
        txid = self.nodes[0].sendrawtransaction(signed_tx)
        self.generate(self.nodes[0], 6)

        # Find the output pos
        p2sh_pos = -1
        p2wsh_pos = -1
        p2wpkh_pos = -1
        p2pkh_pos = -1
        p2sh_p2wsh_pos = -1
        p2sh_p2wpkh_pos = -1
        decoded = self.nodes[0].decoderawtransaction(signed_tx)
        for out in decoded['vout']:
            if out['scriptPubKey']['address'] == p2sh:
                p2sh_pos = out['n']
            elif out['scriptPubKey']['address'] == p2wsh:
                p2wsh_pos = out['n']
            elif out['scriptPubKey']['address'] == p2wpkh:
                p2wpkh_pos = out['n']
            elif out['scriptPubKey']['address'] == p2sh_p2wsh:
                p2sh_p2wsh_pos = out['n']
            elif out['scriptPubKey']['address'] == p2sh_p2wpkh:
                p2sh_p2wpkh_pos = out['n']
            elif out['scriptPubKey']['address'] == p2pkh:
                p2pkh_pos = out['n']

        inputs = [{"txid": txid, "vout": p2wpkh_pos}, {"txid": txid, "vout": p2sh_p2wpkh_pos}, {"txid": txid, "vout": p2pkh_pos}]
        outputs = [{self.nodes[1].getnewaddress(): 29.99}]

        # spend single key from node 1
        created_psmt = self.nodes[1].walletcreatefundedpsmt(inputs, outputs)
        walletprocesspsmt_out = self.nodes[1].walletprocesspsmt(created_psmt['psmt'])
        # Make sure it has both types of UTXOs
        decoded = self.nodes[1].decodepsmt(walletprocesspsmt_out['psmt'])
        assert 'non_witness_utxo' in decoded['inputs'][0]
        assert 'witness_utxo' in decoded['inputs'][0]
        # Check decodepsmt fee calculation (input values shall only be counted once per UTXO)
        assert_equal(decoded['fee'], created_psmt['fee'])
        assert_equal(walletprocesspsmt_out['complete'], True)
        self.nodes[1].sendrawtransaction(walletprocesspsmt_out['hex'])

        self.log.info("Test walletcreatefundedpsmt fee rate of 10000 mewc/vB and 0.1 MEWC/kvB produces a total fee at or slightly below -maxtxfee (~0.05290000)")
        res1 = self.nodes[1].walletcreatefundedpsmt(inputs, outputs, 0, {"fee_rate": 10000, "add_inputs": True})
        assert_approx(res1["fee"], 0.055, 0.005)
        res2 = self.nodes[1].walletcreatefundedpsmt(inputs, outputs, 0, {"feeRate": "0.1", "add_inputs": True})
        assert_approx(res2["fee"], 0.055, 0.005)

        self.log.info("Test min fee rate checks with walletcreatefundedpsmt are bypassed, e.g. a fee_rate under 1 mewc/vB is allowed")
        res3 = self.nodes[1].walletcreatefundedpsmt(inputs, outputs, 0, {"fee_rate": "0.999", "add_inputs": True})
        assert_approx(res3["fee"], 0.00000381, 0.0000001)
        res4 = self.nodes[1].walletcreatefundedpsmt(inputs, outputs, 0, {"feeRate": 0.00000999, "add_inputs": True})
        assert_approx(res4["fee"], 0.00000381, 0.0000001)

        self.log.info("Test min fee rate checks with walletcreatefundedpsmt are bypassed and that funding non-standard 'zero-fee' transactions is valid")
        for param, zero_value in product(["fee_rate", "feeRate"], [0, 0.000, 0.00000000, "0", "0.000", "0.00000000"]):
            assert_equal(0, self.nodes[1].walletcreatefundedpsmt(inputs, outputs, 0, {param: zero_value, "add_inputs": True})["fee"])

        self.log.info("Test invalid fee rate settings")
        for param, value in {("fee_rate", 100000), ("feeRate", 1)}:
            assert_raises_rpc_error(-4, "Fee exceeds maximum configured by user (e.g. -maxtxfee, maxfeerate)",
                self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {param: value, "add_inputs": True})
            assert_raises_rpc_error(-3, "Amount out of range",
                self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {param: -1, "add_inputs": True})
            assert_raises_rpc_error(-3, "Amount is not a number or string",
                self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {param: {"foo": "bar"}, "add_inputs": True})
            # Test fee rate values that don't pass fixed-point parsing checks.
            for invalid_value in ["", 0.000000001, 1e-09, 1.111111111, 1111111111111111, "31.999999999999999999999"]:
                assert_raises_rpc_error(-3, "Invalid amount",
                    self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {param: invalid_value, "add_inputs": True})
        # Test fee_rate values that cannot be represented in mewc/vB.
        for invalid_value in [0.0001, 0.00000001, 0.00099999, 31.99999999]:
            assert_raises_rpc_error(-3, "Invalid amount",
                self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {"fee_rate": invalid_value, "add_inputs": True})

        self.log.info("- raises RPC error if both feeRate and fee_rate are passed")
        assert_raises_rpc_error(-8, "Cannot specify both fee_rate (mewc/vB) and feeRate (MEWC/kvB)",
            self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {"fee_rate": 0.1, "feeRate": 0.1, "add_inputs": True})

        self.log.info("- raises RPC error if both feeRate and estimate_mode passed")
        assert_raises_rpc_error(-8, "Cannot specify both estimate_mode and feeRate",
            self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {"estimate_mode": "economical", "feeRate": 0.1, "add_inputs": True})

        for param in ["feeRate", "fee_rate"]:
            self.log.info("- raises RPC error if both {} and conf_target are passed".format(param))
            assert_raises_rpc_error(-8, "Cannot specify both conf_target and {}. Please provide either a confirmation "
                "target in blocks for automatic fee estimation, or an explicit fee rate.".format(param),
                self.nodes[1].walletcreatefundedpsmt ,inputs, outputs, 0, {param: 1, "conf_target": 1, "add_inputs": True})

        self.log.info("- raises RPC error if both fee_rate and estimate_mode are passed")
        assert_raises_rpc_error(-8, "Cannot specify both estimate_mode and fee_rate",
            self.nodes[1].walletcreatefundedpsmt ,inputs, outputs, 0, {"fee_rate": 1, "estimate_mode": "economical", "add_inputs": True})

        self.log.info("- raises RPC error with invalid estimate_mode settings")
        for k, v in {"number": 42, "object": {"foo": "bar"}}.items():
            assert_raises_rpc_error(-3, f"JSON value of type {k} for field estimate_mode is not of expected type string",
                self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {"estimate_mode": v, "conf_target": 0.1, "add_inputs": True})
        for mode in ["", "foo", Decimal("3.141592")]:
            assert_raises_rpc_error(-8, 'Invalid estimate_mode parameter, must be one of: "unset", "economical", "conservative"',
                self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {"estimate_mode": mode, "conf_target": 0.1, "add_inputs": True})

        self.log.info("- raises RPC error with invalid conf_target settings")
        for mode in ["unset", "economical", "conservative"]:
            self.log.debug("{}".format(mode))
            for k, v in {"string": "", "object": {"foo": "bar"}}.items():
                assert_raises_rpc_error(-3, f"JSON value of type {k} for field conf_target is not of expected type number",
                    self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {"estimate_mode": mode, "conf_target": v, "add_inputs": True})
            for n in [-1, 0, 1009]:
                assert_raises_rpc_error(-8, "Invalid conf_target, must be between 1 and 1008",  # max value of 1008 per src/policy/fees.h
                    self.nodes[1].walletcreatefundedpsmt, inputs, outputs, 0, {"estimate_mode": mode, "conf_target": n, "add_inputs": True})

        self.log.info("Test walletcreatefundedpsmt with too-high fee rate produces total fee well above -maxtxfee and raises RPC error")
        # previously this was silently capped at -maxtxfee
        for bool_add, outputs_array in {True: outputs, False: [{self.nodes[1].getnewaddress(): 1}]}.items():
            msg = "Fee exceeds maximum configured by user (e.g. -maxtxfee, maxfeerate)"
            assert_raises_rpc_error(-4, msg, self.nodes[1].walletcreatefundedpsmt, inputs, outputs_array, 0, {"fee_rate": 1000000, "add_inputs": bool_add})
            assert_raises_rpc_error(-4, msg, self.nodes[1].walletcreatefundedpsmt, inputs, outputs_array, 0, {"feeRate": 1, "add_inputs": bool_add})

        self.log.info("Test various PSMT operations")
        # partially sign multisig things with node 1
        psmtx = wmulti.walletcreatefundedpsmt(inputs=[{"txid":txid,"vout":p2wsh_pos},{"txid":txid,"vout":p2sh_pos},{"txid":txid,"vout":p2sh_p2wsh_pos}], outputs={self.nodes[1].getnewaddress():29.99}, changeAddress=self.nodes[1].getrawchangeaddress())['psmt']
        walletprocesspsmt_out = self.nodes[1].walletprocesspsmt(psmtx)
        psmtx = walletprocesspsmt_out['psmt']
        assert_equal(walletprocesspsmt_out['complete'], False)

        # Unload wmulti, we don't need it anymore
        wmulti.unloadwallet()

        # partially sign with node 2. This should be complete and sendable
        walletprocesspsmt_out = self.nodes[2].walletprocesspsmt(psmtx)
        assert_equal(walletprocesspsmt_out['complete'], True)
        self.nodes[2].sendrawtransaction(walletprocesspsmt_out['hex'])

        # check that walletprocesspsmt fails to decode a non-psmt
        rawtx = self.nodes[1].createrawtransaction([{"txid":txid,"vout":p2wpkh_pos}], {self.nodes[1].getnewaddress():9.99})
        assert_raises_rpc_error(-22, "TX decode failed", self.nodes[1].walletprocesspsmt, rawtx)

        # Convert a non-psmt to psmt and make sure we can decode it
        rawtx = self.nodes[0].createrawtransaction([], {self.nodes[1].getnewaddress():10})
        rawtx = self.nodes[0].fundrawtransaction(rawtx)
        new_psmt = self.nodes[0].converttopsmt(rawtx['hex'])
        self.nodes[0].decodepsmt(new_psmt)

        # Make sure that a non-psmt with signatures cannot be converted
        signedtx = self.nodes[0].signrawtransactionwithwallet(rawtx['hex'])
        assert_raises_rpc_error(-22, "Inputs must not have scriptSigs and scriptWitnesses",
                                self.nodes[0].converttopsmt, hexstring=signedtx['hex'])  # permitsigdata=False by default
        assert_raises_rpc_error(-22, "Inputs must not have scriptSigs and scriptWitnesses",
                                self.nodes[0].converttopsmt, hexstring=signedtx['hex'], permitsigdata=False)
        assert_raises_rpc_error(-22, "Inputs must not have scriptSigs and scriptWitnesses",
                                self.nodes[0].converttopsmt, hexstring=signedtx['hex'], permitsigdata=False, iswitness=True)
        # Unless we allow it to convert and strip signatures
        self.nodes[0].converttopsmt(hexstring=signedtx['hex'], permitsigdata=True)

        # Create outputs to nodes 1 and 2
        # (note that we intentionally create two different txs here, as we want
        #  to check that each node is missing prevout data for one of the two
        #  utxos, see "should only have data for one input" test below)
        node1_addr = self.nodes[1].getnewaddress()
        node2_addr = self.nodes[2].getnewaddress()
        utxo1 = self.create_outpoints(self.nodes[0], outputs=[{node1_addr: 13}])[0]
        utxo2 = self.create_outpoints(self.nodes[0], outputs=[{node2_addr: 13}])[0]
        self.generate(self.nodes[0], 6)[0]

        # Create a psmt spending outputs from nodes 1 and 2
        psmt_orig = self.nodes[0].createpsmt([utxo1, utxo2], {self.nodes[0].getnewaddress():25.999})

        # Update psmts, should only have data for one input and not the other
        psmt1 = self.nodes[1].walletprocesspsmt(psmt_orig, False, "ALL")['psmt']
        psmt1_decoded = self.nodes[0].decodepsmt(psmt1)
        assert psmt1_decoded['inputs'][0] and not psmt1_decoded['inputs'][1]
        # Check that BIP32 path was added
        assert "bip32_derivs" in psmt1_decoded['inputs'][0]
        psmt2 = self.nodes[2].walletprocesspsmt(psmt_orig, False, "ALL", False)['psmt']
        psmt2_decoded = self.nodes[0].decodepsmt(psmt2)
        assert not psmt2_decoded['inputs'][0] and psmt2_decoded['inputs'][1]
        # Check that BIP32 paths were not added
        assert "bip32_derivs" not in psmt2_decoded['inputs'][1]

        # Sign PSMTs (workaround issue #18039)
        psmt1 = self.nodes[1].walletprocesspsmt(psmt_orig)['psmt']
        psmt2 = self.nodes[2].walletprocesspsmt(psmt_orig)['psmt']

        # Combine, finalize, and send the psmts
        combined = self.nodes[0].combinepsmt([psmt1, psmt2])
        finalized = self.nodes[0].finalizepsmt(combined)['hex']
        self.nodes[0].sendrawtransaction(finalized)
        self.generate(self.nodes[0], 6)

        # Test additional args in walletcreatepsmt
        # Make sure both pre-included and funded inputs
        # have the correct sequence numbers based on
        # replaceable arg
        block_height = self.nodes[0].getblockcount()
        unspent = self.nodes[0].listunspent()[0]
        psmtx_info = self.nodes[0].walletcreatefundedpsmt([{"txid":unspent["txid"], "vout":unspent["vout"]}], [{self.nodes[2].getnewaddress():unspent["amount"]+1}], block_height+2, {"replaceable": False, "add_inputs": True}, False)
        decoded_psmt = self.nodes[0].decodepsmt(psmtx_info["psmt"])
        for tx_in, psmt_in in zip(decoded_psmt["tx"]["vin"], decoded_psmt["inputs"]):
            assert_greater_than(tx_in["sequence"], MAX_BIP125_RBF_SEQUENCE)
            assert "bip32_derivs" not in psmt_in
        assert_equal(decoded_psmt["tx"]["locktime"], block_height+2)

        # Same construction with only locktime set and RBF explicitly enabled
        psmtx_info = self.nodes[0].walletcreatefundedpsmt([{"txid":unspent["txid"], "vout":unspent["vout"]}], [{self.nodes[2].getnewaddress():unspent["amount"]+1}], block_height, {"replaceable": True, "add_inputs": True}, True)
        decoded_psmt = self.nodes[0].decodepsmt(psmtx_info["psmt"])
        for tx_in, psmt_in in zip(decoded_psmt["tx"]["vin"], decoded_psmt["inputs"]):
            assert_equal(tx_in["sequence"], MAX_BIP125_RBF_SEQUENCE)
            assert "bip32_derivs" in psmt_in
        assert_equal(decoded_psmt["tx"]["locktime"], block_height)

        # Same construction without optional arguments
        psmtx_info = self.nodes[0].walletcreatefundedpsmt([], [{self.nodes[2].getnewaddress():unspent["amount"]+1}])
        decoded_psmt = self.nodes[0].decodepsmt(psmtx_info["psmt"])
        for tx_in, psmt_in in zip(decoded_psmt["tx"]["vin"], decoded_psmt["inputs"]):
            assert_equal(tx_in["sequence"], MAX_BIP125_RBF_SEQUENCE)
            assert "bip32_derivs" in psmt_in
        assert_equal(decoded_psmt["tx"]["locktime"], 0)

        # Same construction without optional arguments, for a node with -walletrbf=0
        unspent1 = self.nodes[1].listunspent()[0]
        psmtx_info = self.nodes[1].walletcreatefundedpsmt([{"txid":unspent1["txid"], "vout":unspent1["vout"]}], [{self.nodes[2].getnewaddress():unspent1["amount"]+1}], block_height, {"add_inputs": True})
        decoded_psmt = self.nodes[1].decodepsmt(psmtx_info["psmt"])
        for tx_in, psmt_in in zip(decoded_psmt["tx"]["vin"], decoded_psmt["inputs"]):
            assert_greater_than(tx_in["sequence"], MAX_BIP125_RBF_SEQUENCE)
            assert "bip32_derivs" in psmt_in

        # Make sure change address wallet does not have P2SH innerscript access to results in success
        # when attempting BnB coin selection
        self.nodes[0].walletcreatefundedpsmt([], [{self.nodes[2].getnewaddress():unspent["amount"]+1}], block_height+2, {"changeAddress":self.nodes[1].getnewaddress()}, False)

        # Make sure the wallet's change type is respected by default
        small_output = {self.nodes[0].getnewaddress():0.1}
        psmtx_native = self.nodes[0].walletcreatefundedpsmt([], [small_output])
        self.assert_change_type(psmtx_native, "witness_v0_keyhash")
        psmtx_legacy = self.nodes[1].walletcreatefundedpsmt([], [small_output])
        self.assert_change_type(psmtx_legacy, "pubkeyhash")

        # Make sure the change type of the wallet can also be overwritten
        psmtx_np2wkh = self.nodes[1].walletcreatefundedpsmt([], [small_output], 0, {"change_type":"p2sh-segwit"})
        self.assert_change_type(psmtx_np2wkh, "scripthash")

        # Make sure the change type cannot be specified if a change address is given
        invalid_options = {"change_type":"legacy","changeAddress":self.nodes[0].getnewaddress()}
        assert_raises_rpc_error(-8, "both change address and address type options", self.nodes[0].walletcreatefundedpsmt, [], [small_output], 0, invalid_options)

        # Regression test for 14473 (mishandling of already-signed witness transaction):
        psmtx_info = self.nodes[0].walletcreatefundedpsmt([{"txid":unspent["txid"], "vout":unspent["vout"]}], [{self.nodes[2].getnewaddress():unspent["amount"]+1}], 0, {"add_inputs": True})
        complete_psmt = self.nodes[0].walletprocesspsmt(psmtx_info["psmt"])
        double_processed_psmt = self.nodes[0].walletprocesspsmt(complete_psmt["psmt"])
        assert_equal(complete_psmt, double_processed_psmt)
        # We don't care about the decode result, but decoding must succeed.
        self.nodes[0].decodepsmt(double_processed_psmt["psmt"])

        # Make sure unsafe inputs are included if specified
        self.nodes[2].createwallet(wallet_name="unsafe")
        wunsafe = self.nodes[2].get_wallet_rpc("unsafe")
        self.nodes[0].sendtoaddress(wunsafe.getnewaddress(), 2)
        self.sync_mempools()
        assert_raises_rpc_error(-4, "Insufficient funds", wunsafe.walletcreatefundedpsmt, [], [{self.nodes[0].getnewaddress(): 1}])
        wunsafe.walletcreatefundedpsmt([], [{self.nodes[0].getnewaddress(): 1}], 0, {"include_unsafe": True})

        # BIP 174 Test Vectors

        # Check that unknown values are just passed through
        unknown_psmt = "cHNidP8BAD8CAAAAAf//////////////////////////////////////////AAAAAAD/////AQAAAAAAAAAAA2oBAAAAAAAACg8BAgMEBQYHCAkPAQIDBAUGBwgJCgsMDQ4PAAA="
        unknown_out = self.nodes[0].walletprocesspsmt(unknown_psmt)['psmt']
        assert_equal(unknown_psmt, unknown_out)

        # Open the data file
        with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'data/rpc_psmt.json'), encoding='utf-8') as f:
            d = json.load(f)
            invalids = d['invalid']
            invalid_with_msgs = d["invalid_with_msg"]
            valids = d['valid']
            creators = d['creator']
            signers = d['signer']
            combiners = d['combiner']
            finalizers = d['finalizer']
            extractors = d['extractor']

        # Invalid PSMTs
        for invalid in invalids:
            assert_raises_rpc_error(-22, "TX decode failed", self.nodes[0].decodepsmt, invalid)
        for invalid in invalid_with_msgs:
            psmt, msg = invalid
            assert_raises_rpc_error(-22, f"TX decode failed {msg}", self.nodes[0].decodepsmt, psmt)

        # Valid PSMTs
        for valid in valids:
            self.nodes[0].decodepsmt(valid)

        # Creator Tests
        for creator in creators:
            created_tx = self.nodes[0].createpsmt(inputs=creator['inputs'], outputs=creator['outputs'], replaceable=False)
            assert_equal(created_tx, creator['result'])

        # Signer tests
        for i, signer in enumerate(signers):
            self.nodes[2].createwallet(wallet_name="wallet{}".format(i))
            wrpc = self.nodes[2].get_wallet_rpc("wallet{}".format(i))
            for key in signer['privkeys']:
                wallet_importprivkey(wrpc, key, "now")
            signed_tx = wrpc.walletprocesspsmt(signer['psmt'], True, "ALL")['psmt']
            assert_equal(signed_tx, signer['result'])

        # Combiner test
        for combiner in combiners:
            combined = self.nodes[2].combinepsmt(combiner['combine'])
            assert_equal(combined, combiner['result'])

        # Empty combiner test
        assert_raises_rpc_error(-8, "Parameter 'txs' cannot be empty", self.nodes[0].combinepsmt, [])

        # Finalizer test
        for finalizer in finalizers:
            finalized = self.nodes[2].finalizepsmt(finalizer['finalize'], False)['psmt']
            assert_equal(finalized, finalizer['result'])

        # Extractor test
        for extractor in extractors:
            extracted = self.nodes[2].finalizepsmt(extractor['extract'], True)['hex']
            assert_equal(extracted, extractor['result'])

        # Unload extra wallets
        for i, signer in enumerate(signers):
            self.nodes[2].unloadwallet("wallet{}".format(i))

        self.test_utxo_conversion()
        self.test_psmt_incomplete_after_invalid_modification()

        self.test_input_confs_control()

        # Test that psmts with p2pkh outputs are created properly
        p2pkh = self.nodes[0].getnewaddress(address_type='legacy')
        psmt = self.nodes[1].walletcreatefundedpsmt(inputs=[], outputs=[{p2pkh : 1}], bip32derivs=True)
        self.nodes[0].decodepsmt(psmt['psmt'])

        # Test decoding error: invalid base64
        assert_raises_rpc_error(-22, "TX decode failed invalid base64", self.nodes[0].decodepsmt, ";definitely not base64;")

        # Send to all types of addresses
        addr1 = self.nodes[1].getnewaddress("", "bech32")
        addr2 = self.nodes[1].getnewaddress("", "legacy")
        addr3 = self.nodes[1].getnewaddress("", "p2sh-segwit")
        utxo1, utxo2, utxo3 = self.create_outpoints(self.nodes[1], outputs=[{addr1: 11}, {addr2: 11}, {addr3: 11}])
        self.sync_all()

        def test_psmt_input_keys(psmt_input, keys):
            """Check that the psmt input has only the expected keys."""
            assert_equal(set(keys), set(psmt_input.keys()))

        # Create a PSMT. None of the inputs are filled initially
        psmt = self.nodes[1].createpsmt([utxo1, utxo2, utxo3], {self.nodes[0].getnewaddress():32.999})
        decoded = self.nodes[1].decodepsmt(psmt)
        test_psmt_input_keys(decoded['inputs'][0], [])
        test_psmt_input_keys(decoded['inputs'][1], [])
        test_psmt_input_keys(decoded['inputs'][2], [])

        # Update a PSMT with UTXOs from the node
        # Bech32 inputs should be filled with witness UTXO. Other inputs should not be filled because they are non-witness
        updated = self.nodes[1].utxoupdatepsmt(psmt)
        decoded = self.nodes[1].decodepsmt(updated)
        test_psmt_input_keys(decoded['inputs'][0], ['witness_utxo', 'non_witness_utxo'])
        test_psmt_input_keys(decoded['inputs'][1], ['non_witness_utxo'])
        test_psmt_input_keys(decoded['inputs'][2], ['non_witness_utxo'])

        # Try again, now while providing descriptors, making P2SH-segwit work, and causing bip32_derivs and redeem_script to be filled in
        descs = [self.nodes[1].getaddressinfo(addr)['desc'] for addr in [addr1,addr2,addr3]]
        updated = self.nodes[1].utxoupdatepsmt(psmt=psmt, descriptors=descs)
        decoded = self.nodes[1].decodepsmt(updated)
        test_psmt_input_keys(decoded['inputs'][0], ['witness_utxo', 'non_witness_utxo', 'bip32_derivs'])
        test_psmt_input_keys(decoded['inputs'][1], ['non_witness_utxo', 'bip32_derivs'])
        test_psmt_input_keys(decoded['inputs'][2], ['non_witness_utxo','witness_utxo', 'bip32_derivs', 'redeem_script'])

        # Two PSMTs with a common input should not be joinable
        psmt1 = self.nodes[1].createpsmt([utxo1], {self.nodes[0].getnewaddress():Decimal('10.999')})
        assert_raises_rpc_error(-8, "exists in multiple PSMTs", self.nodes[1].joinpsmts, [psmt1, updated])

        # Join two distinct PSMTs
        addr4 = self.nodes[1].getnewaddress("", "p2sh-segwit")
        utxo4 = self.create_outpoints(self.nodes[0], outputs=[{addr4: 5}])[0]
        self.generate(self.nodes[0], 6)
        psmt2 = self.nodes[1].createpsmt([utxo4], {self.nodes[0].getnewaddress():Decimal('4.999')})
        psmt2 = self.nodes[1].walletprocesspsmt(psmt2)['psmt']
        psmt2_decoded = self.nodes[0].decodepsmt(psmt2)
        assert "final_scriptwitness" in psmt2_decoded['inputs'][0] and "final_scriptSig" in psmt2_decoded['inputs'][0]
        joined = self.nodes[0].joinpsmts([psmt, psmt2])
        joined_decoded = self.nodes[0].decodepsmt(joined)
        assert len(joined_decoded['inputs']) == 4 and len(joined_decoded['outputs']) == 2 and "final_scriptwitness" not in joined_decoded['inputs'][3] and "final_scriptSig" not in joined_decoded['inputs'][3]

        # Check that joining shuffles the inputs and outputs
        # 10 attempts should be enough to get a shuffled join
        shuffled = False
        for _ in range(10):
            shuffled_joined = self.nodes[0].joinpsmts([psmt, psmt2])
            shuffled |= joined != shuffled_joined
            if shuffled:
                break
        assert shuffled

        # Newly created PSMT needs UTXOs and updating
        addr = self.nodes[1].getnewaddress("", "p2sh-segwit")
        utxo = self.create_outpoints(self.nodes[0], outputs=[{addr: 7}])[0]
        addrinfo = self.nodes[1].getaddressinfo(addr)
        self.generate(self.nodes[0], 6)[0]
        psmt = self.nodes[1].createpsmt([utxo], {self.nodes[0].getnewaddress("", "p2sh-segwit"):Decimal('6.999')})
        analyzed = self.nodes[0].analyzepsmt(psmt)
        assert not analyzed['inputs'][0]['has_utxo'] and not analyzed['inputs'][0]['is_final'] and analyzed['inputs'][0]['next'] == 'updater' and analyzed['next'] == 'updater'

        # After update with wallet, only needs signing
        updated = self.nodes[1].walletprocesspsmt(psmt, False, 'ALL', True)['psmt']
        analyzed = self.nodes[0].analyzepsmt(updated)
        assert analyzed['inputs'][0]['has_utxo'] and not analyzed['inputs'][0]['is_final'] and analyzed['inputs'][0]['next'] == 'signer' and analyzed['next'] == 'signer' and analyzed['inputs'][0]['missing']['signatures'][0] == addrinfo['embedded']['witness_program']

        # Check fee and size things
        assert analyzed['fee'] == Decimal('0.001') and analyzed['estimated_vsize'] == 134 and analyzed['estimated_feerate'] == Decimal('0.00746268')

        # After signing and finalizing, needs extracting
        signed = self.nodes[1].walletprocesspsmt(updated)['psmt']
        analyzed = self.nodes[0].analyzepsmt(signed)
        assert analyzed['inputs'][0]['has_utxo'] and analyzed['inputs'][0]['is_final'] and analyzed['next'] == 'extractor'

        self.log.info("PSMT spending unspendable outputs should have error message and Creator as next")
        analysis = self.nodes[0].analyzepsmt('cHNidP8BAJoCAAAAAljoeiG1ba8MI76OcHBFbDNvfLqlyHV5JPVFiHuyq911AAAAAAD/////g40EJ9DsZQpoqka7CwmK6kQiwHGyyng1Kgd5WdB86h0BAAAAAP////8CcKrwCAAAAAAWAEHYXCtx0AYLCcmIauuBXlCZHdoSTQDh9QUAAAAAFv8/wADXYP/7//////8JxOh0LR2HAI8AAAAAAAEBIADC6wsAAAAAF2oUt/X69ELjeX2nTof+fZ10l+OyAokDAQcJAwEHEAABAACAAAEBIADC6wsAAAAAF2oUt/X69ELjeX2nTof+fZ10l+OyAokDAQcJAwEHENkMak8AAAAA')
        assert_equal(analysis['next'], 'creator')
        assert_equal(analysis['error'], 'PSMT is not valid. Input 0 spends unspendable output')

        self.log.info("PSMT with invalid values should have error message and Creator as next")
        analysis = self.nodes[0].analyzepsmt('cHNidP8BAHECAAAAAfA00BFgAm6tp86RowwH6BMImQNL5zXUcTT97XoLGz0BAAAAAAD/////AgD5ApUAAAAAFgAUKNw0x8HRctAgmvoevm4u1SbN7XL87QKVAAAAABYAFPck4gF7iL4NL4wtfRAKgQbghiTUAAAAAAABAR8AgIFq49AHABYAFJUDtxf2PHo641HEOBOAIvFMNTr2AAAA')
        assert_equal(analysis['next'], 'creator')
        assert_equal(analysis['error'], 'PSMT is not valid. Input 0 has invalid value')

        self.log.info("PSMT with signed, but not finalized, inputs should have Finalizer as next")
        analysis = self.nodes[0].analyzepsmt('cHNidP8BAHECAAAAAZYezcxdnbXoQCmrD79t/LzDgtUo9ERqixk8wgioAobrAAAAAAD9////AlDDAAAAAAAAFgAUy/UxxZuzZswcmFnN/E9DGSiHLUsuGPUFAAAAABYAFLsH5o0R38wXx+X2cCosTMCZnQ4baAAAAAABAR8A4fUFAAAAABYAFOBI2h5thf3+Lflb2LGCsVSZwsltIgIC/i4dtVARCRWtROG0HHoGcaVklzJUcwo5homgGkSNAnJHMEQCIGx7zKcMIGr7cEES9BR4Kdt/pzPTK3fKWcGyCJXb7MVnAiALOBgqlMH4GbC1HDh/HmylmO54fyEy4lKde7/BT/PWxwEBAwQBAAAAIgYC/i4dtVARCRWtROG0HHoGcaVklzJUcwo5homgGkSNAnIYDwVpQ1QAAIABAACAAAAAgAAAAAAAAAAAAAAiAgL+CIiB59NSCssOJRGiMYQK1chahgAaaJpIXE41Cyir+xgPBWlDVAAAgAEAAIAAAACAAQAAAAAAAAAA')
        assert_equal(analysis['next'], 'finalizer')

        analysis = self.nodes[0].analyzepsmt('cHNidP8BAHECAAAAAfA00BFgAm6tp86RowwH6BMImQNL5zXUcTT97XoLGz0BAAAAAAD/////AgCAgWrj0AcAFgAUKNw0x8HRctAgmvoevm4u1SbN7XL87QKVAAAAABYAFPck4gF7iL4NL4wtfRAKgQbghiTUAAAAAAABAR8A8gUqAQAAABYAFJUDtxf2PHo641HEOBOAIvFMNTr2AAAA')
        assert_equal(analysis['next'], 'creator')
        assert_equal(analysis['error'], 'PSMT is not valid. Output amount invalid')

        assert_raises_rpc_error(-22, "TX decode failed", self.nodes[0].analyzepsmt, "cHNidP8BAJoCAAAAAkvEW8NnDtdNtDpsmze+Ht2LH35IJcKv00jKAlUs21RrAwAAAAD/////S8Rbw2cO1020OmybN74e3Ysffkglwq/TSMoCVSzbVGsBAAAAAP7///8CwLYClQAAAAAWABSNJKzjaUb3uOxixsvh1GGE3fW7zQD5ApUAAAAAFgAUKNw0x8HRctAgmvoevm4u1SbN7XIAAAAAAAEAnQIAAAACczMa321tVHuN4GKWKRncycI22aX3uXgwSFUKM2orjRsBAAAAAP7///9zMxrfbW1Ue43gYpYpGdzJwjbZpfe5eDBIVQozaiuNGwAAAAAA/v///wIA+QKVAAAAABl2qRT9zXUVA8Ls5iVqynLHe5/vSe1XyYisQM0ClQAAAAAWABRmWQUcjSjghQ8/uH4Bn/zkakwLtAAAAAAAAQEfQM0ClQAAAAAWABRmWQUcjSjghQ8/uH4Bn/zkakwLtAAAAA==")

        assert_raises_rpc_error(-22, "TX decode failed", self.nodes[0].walletprocesspsmt, "cHNidP8BAJoCAAAAAkvEW8NnDtdNtDpsmze+Ht2LH35IJcKv00jKAlUs21RrAwAAAAD/////S8Rbw2cO1020OmybN74e3Ysffkglwq/TSMoCVSzbVGsBAAAAAP7///8CwLYClQAAAAAWABSNJKzjaUb3uOxixsvh1GGE3fW7zQD5ApUAAAAAFgAUKNw0x8HRctAgmvoevm4u1SbN7XIAAAAAAAEAnQIAAAACczMa321tVHuN4GKWKRncycI22aX3uXgwSFUKM2orjRsBAAAAAP7///9zMxrfbW1Ue43gYpYpGdzJwjbZpfe5eDBIVQozaiuNGwAAAAAA/v///wIA+QKVAAAAABl2qRT9zXUVA8Ls5iVqynLHe5/vSe1XyYisQM0ClQAAAAAWABRmWQUcjSjghQ8/uH4Bn/zkakwLtAAAAAAAAQEfQM0ClQAAAAAWABRmWQUcjSjghQ8/uH4Bn/zkakwLtAAAAA==")

        self.log.info("Test that we can fund psmts with external inputs specified")

        privkey, _ = generate_keypair(wif=True)

        self.nodes[1].createwallet("extfund")
        wallet = self.nodes[1].get_wallet_rpc("extfund")

        # Make a weird but signable script. sh(wsh(pkh())) descriptor accomplishes this
        desc = descsum_create("sh(wsh(pkh({})))".format(privkey))
        res = self.nodes[0].importdescriptors([{"desc": desc, "timestamp": "now"}])
        assert res[0]["success"]
        addr = self.nodes[0].deriveaddresses(desc)[0]
        addr_info = self.nodes[0].getaddressinfo(addr)

        self.nodes[0].sendtoaddress(addr, 10)
        self.nodes[0].sendtoaddress(wallet.getnewaddress(), 10)
        self.generate(self.nodes[0], 6)
        ext_utxo = self.nodes[0].listunspent(addresses=[addr])[0]

        # An external input without solving data should result in an error
        assert_raises_rpc_error(-4, "Not solvable pre-selected input COutPoint(%s, %s)" % (ext_utxo["txid"][0:10], ext_utxo["vout"]), wallet.walletcreatefundedpsmt, [ext_utxo], {self.nodes[0].getnewaddress(): 15})

        # But funding should work when the solving data is provided
        psmt = wallet.walletcreatefundedpsmt([ext_utxo], {self.nodes[0].getnewaddress(): 15}, 0, {"add_inputs": True, "solving_data": {"pubkeys": [addr_info['pubkey']], "scripts": [addr_info["embedded"]["scriptPubKey"], addr_info["embedded"]["embedded"]["scriptPubKey"]]}})
        signed = wallet.walletprocesspsmt(psmt['psmt'])
        assert not signed['complete']
        signed = self.nodes[0].walletprocesspsmt(signed['psmt'])
        assert signed['complete']

        psmt = wallet.walletcreatefundedpsmt([ext_utxo], {self.nodes[0].getnewaddress(): 15}, 0, {"add_inputs": True, "solving_data":{"descriptors": [desc]}})
        signed = wallet.walletprocesspsmt(psmt['psmt'])
        assert not signed['complete']
        signed = self.nodes[0].walletprocesspsmt(signed['psmt'])
        assert signed['complete']
        final = signed['hex']

        dec = self.nodes[0].decodepsmt(signed["psmt"])
        for i, txin in enumerate(dec["tx"]["vin"]):
            if txin["txid"] == ext_utxo["txid"] and txin["vout"] == ext_utxo["vout"]:
                input_idx = i
                break
        psmt_in = dec["inputs"][input_idx]
        scriptsig_hex = psmt_in["final_scriptSig"]["hex"] if "final_scriptSig" in psmt_in else ""
        witness_stack_hex = psmt_in["final_scriptwitness"] if "final_scriptwitness" in psmt_in else None
        input_weight = calculate_input_weight(scriptsig_hex, witness_stack_hex)
        low_input_weight = input_weight // 2
        high_input_weight = input_weight * 2

        # Input weight error conditions
        assert_raises_rpc_error(
            -8,
            "Input weights should be specified in inputs rather than in options.",
            wallet.walletcreatefundedpsmt,
            inputs=[ext_utxo],
            outputs={self.nodes[0].getnewaddress(): 15},
            options={"input_weights": [{"txid": ext_utxo["txid"], "vout": ext_utxo["vout"], "weight": 1000}]}
        )

        # Funding should also work if the input weight is provided
        psmt = wallet.walletcreatefundedpsmt(
            inputs=[{"txid": ext_utxo["txid"], "vout": ext_utxo["vout"], "weight": input_weight}],
            outputs={self.nodes[0].getnewaddress(): 15},
            add_inputs=True,
        )
        signed = wallet.walletprocesspsmt(psmt["psmt"])
        signed = self.nodes[0].walletprocesspsmt(signed["psmt"])
        final = signed["hex"]
        assert self.nodes[0].testmempoolaccept([final])[0]["allowed"]
        # Reducing the weight should have a lower fee
        psmt2 = wallet.walletcreatefundedpsmt(
            inputs=[{"txid": ext_utxo["txid"], "vout": ext_utxo["vout"], "weight": low_input_weight}],
            outputs={self.nodes[0].getnewaddress(): 15},
            add_inputs=True,
        )
        assert_greater_than(psmt["fee"], psmt2["fee"])
        # Increasing the weight should have a higher fee
        psmt2 = wallet.walletcreatefundedpsmt(
            inputs=[{"txid": ext_utxo["txid"], "vout": ext_utxo["vout"], "weight": high_input_weight}],
            outputs={self.nodes[0].getnewaddress(): 15},
            add_inputs=True,
        )
        assert_greater_than(psmt2["fee"], psmt["fee"])
        # The provided weight should override the calculated weight when solving data is provided
        psmt3 = wallet.walletcreatefundedpsmt(
            inputs=[{"txid": ext_utxo["txid"], "vout": ext_utxo["vout"], "weight": high_input_weight}],
            outputs={self.nodes[0].getnewaddress(): 15},
            add_inputs=True, solving_data={"descriptors": [desc]},
        )
        assert_equal(psmt2["fee"], psmt3["fee"])

        # Import the external utxo descriptor so that we can sign for it from the test wallet
        res = wallet.importdescriptors([{"desc": desc, "timestamp": "now"}])
        assert res[0]["success"]
        # The provided weight should override the calculated weight for a wallet input
        psmt3 = wallet.walletcreatefundedpsmt(
            inputs=[{"txid": ext_utxo["txid"], "vout": ext_utxo["vout"], "weight": high_input_weight}],
            outputs={self.nodes[0].getnewaddress(): 15},
            add_inputs=True,
        )
        assert_equal(psmt2["fee"], psmt3["fee"])

        self.log.info("Test signing inputs that the wallet has keys for but is not watching the scripts")
        self.nodes[1].createwallet(wallet_name="scriptwatchonly", disable_private_keys=True)
        watchonly = self.nodes[1].get_wallet_rpc("scriptwatchonly")

        privkey, pubkey = generate_keypair(wif=True)

        desc = descsum_create("wsh(pkh({}))".format(pubkey.hex()))
        res = watchonly.importdescriptors([{"desc": desc, "timestamp": "now"}])
        assert res[0]["success"]
        addr = self.nodes[0].deriveaddresses(desc)[0]
        self.nodes[0].sendtoaddress(addr, 10)
        self.generate(self.nodes[0], 1)
        wallet_importprivkey(self.nodes[0], privkey, "now")

        psmt = watchonly.sendall([wallet.getnewaddress()])["psmt"]
        signed_tx = self.nodes[0].walletprocesspsmt(psmt)
        self.nodes[0].sendrawtransaction(signed_tx["hex"])

        # Same test but for taproot
        privkey, pubkey = generate_keypair(wif=True)

        desc = descsum_create("tr({},pk({}))".format(H_POINT, pubkey.hex()))
        res = watchonly.importdescriptors([{"desc": desc, "timestamp": "now"}])
        assert res[0]["success"]
        addr = self.nodes[0].deriveaddresses(desc)[0]
        self.nodes[0].sendtoaddress(addr, 10)
        self.generate(self.nodes[0], 1)
        self.nodes[0].importdescriptors([{"desc": descsum_create("tr({})".format(privkey)), "timestamp":"now"}])

        psmt = watchonly.sendall([wallet.getnewaddress(), addr])["psmt"]
        processed_psmt = self.nodes[0].walletprocesspsmt(psmt)
        txid = self.nodes[0].sendrawtransaction(processed_psmt["hex"])
        vout = find_vout_for_address(self.nodes[0], txid, addr)

        # Make sure tap tree is in psmt
        parsed_psmt = PSMT.from_base64(psmt)
        assert_greater_than(len(parsed_psmt.o[vout].map[PSMT_OUT_TAP_TREE]), 0)
        assert "taproot_tree" in self.nodes[0].decodepsmt(psmt)["outputs"][vout]
        parsed_psmt.make_blank()
        comb_psmt = self.nodes[0].combinepsmt([psmt, parsed_psmt.to_base64()])
        assert_equal(comb_psmt, psmt)

        self.log.info("Test that walletprocesspsmt both updates and signs a non-updated psmt containing Taproot inputs")
        addr = self.nodes[0].getnewaddress("", "bech32m")
        utxo = self.create_outpoints(self.nodes[0], outputs=[{addr: 1}])[0]
        psmt = self.nodes[0].createpsmt([utxo], [{self.nodes[0].getnewaddress(): 0.9999}])
        signed = self.nodes[0].walletprocesspsmt(psmt)
        rawtx = signed["hex"]
        self.nodes[0].sendrawtransaction(rawtx)
        self.generate(self.nodes[0], 1)

        # Make sure tap tree is not in psmt
        parsed_psmt = PSMT.from_base64(psmt)
        assert PSMT_OUT_TAP_TREE not in parsed_psmt.o[0].map
        assert "taproot_tree" not in self.nodes[0].decodepsmt(psmt)["outputs"][0]
        parsed_psmt.make_blank()
        comb_psmt = self.nodes[0].combinepsmt([psmt, parsed_psmt.to_base64()])
        assert_equal(comb_psmt, psmt)

        self.log.info("Test walletprocesspsmt raises if an invalid sighashtype is passed")
        assert_raises_rpc_error(-8, "'all' is not a valid sighash parameter.", self.nodes[0].walletprocesspsmt, psmt, sighashtype="all")

        self.log.info("Test decoding PSMT with per-input preimage types")
        # note that the decodepsmt RPC doesn't check whether preimages and hashes match
        hash_ripemd160, preimage_ripemd160 = randbytes(20), randbytes(50)
        hash_sha256, preimage_sha256 = randbytes(32), randbytes(50)
        hash_hash160, preimage_hash160 = randbytes(20), randbytes(50)
        hash_hash256, preimage_hash256 = randbytes(32), randbytes(50)

        tx = CTransaction()
        tx.vin = [CTxIn(outpoint=COutPoint(hash=int('aa' * 32, 16), n=0), scriptSig=b""),
                  CTxIn(outpoint=COutPoint(hash=int('bb' * 32, 16), n=0), scriptSig=b""),
                  CTxIn(outpoint=COutPoint(hash=int('cc' * 32, 16), n=0), scriptSig=b""),
                  CTxIn(outpoint=COutPoint(hash=int('dd' * 32, 16), n=0), scriptSig=b"")]
        tx.vout = [CTxOut(nValue=0, scriptPubKey=b"")]
        psmt = PSMT()
        psmt.g = PSMTMap({PSMT_GLOBAL_UNSIGNED_TX: tx.serialize()})
        psmt.i = [PSMTMap({bytes([PSMT_IN_RIPEMD160]) + hash_ripemd160: preimage_ripemd160}),
                  PSMTMap({bytes([PSMT_IN_SHA256]) + hash_sha256: preimage_sha256}),
                  PSMTMap({bytes([PSMT_IN_HASH160]) + hash_hash160: preimage_hash160}),
                  PSMTMap({bytes([PSMT_IN_HASH256]) + hash_hash256: preimage_hash256})]
        psmt.o = [PSMTMap()]
        res_inputs = self.nodes[0].decodepsmt(psmt.to_base64())["inputs"]
        assert_equal(len(res_inputs), 4)
        preimage_keys = ["ripemd160_preimages", "sha256_preimages", "hash160_preimages", "hash256_preimages"]
        expected_hashes = [hash_ripemd160, hash_sha256, hash_hash160, hash_hash256]
        expected_preimages = [preimage_ripemd160, preimage_sha256, preimage_hash160, preimage_hash256]
        for res_input, preimage_key, hash, preimage in zip(res_inputs, preimage_keys, expected_hashes, expected_preimages):
            assert preimage_key in res_input
            assert_equal(len(res_input[preimage_key]), 1)
            assert hash.hex() in res_input[preimage_key]
            assert_equal(res_input[preimage_key][hash.hex()], preimage.hex())

        self.test_decodepsmt_musig2_input_output_types()

        self.log.info("Test that combining PSMTs with different transactions fails")
        tx = CTransaction()
        tx.vin = [CTxIn(outpoint=COutPoint(hash=int('aa' * 32, 16), n=0), scriptSig=b"")]
        tx.vout = [CTxOut(nValue=0, scriptPubKey=b"")]
        psmt1 = PSMT(g=PSMTMap({PSMT_GLOBAL_UNSIGNED_TX: tx.serialize()}), i=[PSMTMap()], o=[PSMTMap()]).to_base64()
        tx.vout[0].nValue += 1  # slightly modify tx
        psmt2 = PSMT(g=PSMTMap({PSMT_GLOBAL_UNSIGNED_TX: tx.serialize()}), i=[PSMTMap()], o=[PSMTMap()]).to_base64()
        assert_raises_rpc_error(-8, "PSMTs not compatible (different transactions)", self.nodes[0].combinepsmt, [psmt1, psmt2])
        assert_equal(self.nodes[0].combinepsmt([psmt1, psmt1]), psmt1)

        self.log.info("Test that PSMT inputs are being checked via script execution")
        acs_prevout = CTxOut(nValue=0, scriptPubKey=CScript([OP_TRUE]))
        tx = CTransaction()
        tx.vin = [CTxIn(outpoint=COutPoint(hash=int('dd' * 32, 16), n=0), scriptSig=b"")]
        tx.vout = [CTxOut(nValue=0, scriptPubKey=b"")]
        psmt = PSMT()
        psmt.g = PSMTMap({PSMT_GLOBAL_UNSIGNED_TX: tx.serialize()})
        psmt.i = [PSMTMap({bytes([PSMT_IN_WITNESS_UTXO]) : acs_prevout.serialize()})]
        psmt.o = [PSMTMap()]
        assert_equal(self.nodes[0].finalizepsmt(psmt.to_base64()),
            {'hex': '0200000001dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd0000000000000000000100000000000000000000000000', 'complete': True})

        self.log.info("Test we don't crash when making a 0-value funded transaction at 0 fee without forcing an input selection")
        assert_raises_rpc_error(-4, "Transaction requires one destination of non-zero value, a non-zero feerate, or a pre-selected input", self.nodes[0].walletcreatefundedpsmt, [], [{"data": "deadbeef"}], 0, {"fee_rate": "0"})

        self.log.info("Test descriptorprocesspsmt updates and signs a psmt with descriptors")

        self.generate(self.nodes[2], 1)

        # Disable the wallet for node 2 since `descriptorprocesspsmt` does not use the wallet
        self.restart_node(2, extra_args=["-disablewallet"])
        self.connect_nodes(0, 2)
        self.connect_nodes(1, 2)

        key_info = get_generate_key()
        key = key_info.privkey
        address = key_info.p2wpkh_addr

        descriptor = descsum_create(f"wpkh({key})")

        utxo = self.create_outpoints(self.nodes[0], outputs=[{address: 1}])[0]
        self.sync_all()

        psmt = self.nodes[2].createpsmt([utxo], {self.nodes[0].getnewaddress(): 0.99999})
        decoded = self.nodes[2].decodepsmt(psmt)
        test_psmt_input_keys(decoded['inputs'][0], [])

        # Test that even if the wrong descriptor is given, `witness_utxo` and `non_witness_utxo`
        # are still added to the psmt
        alt_descriptor = descsum_create(f"wpkh({get_generate_key().privkey})")
        alt_psmt = self.nodes[2].descriptorprocesspsmt(psmt=psmt, descriptors=[alt_descriptor], sighashtype="ALL")["psmt"]
        decoded = self.nodes[2].decodepsmt(alt_psmt)
        test_psmt_input_keys(decoded['inputs'][0], ['witness_utxo', 'non_witness_utxo'])

        # Test that the psmt is not finalized and does not have bip32_derivs unless specified
        processed_psmt = self.nodes[2].descriptorprocesspsmt(psmt=psmt, descriptors=[descriptor], sighashtype="ALL", bip32derivs=True, finalize=False)
        decoded = self.nodes[2].decodepsmt(processed_psmt['psmt'])
        test_psmt_input_keys(decoded['inputs'][0], ['witness_utxo', 'non_witness_utxo', 'partial_signatures', 'bip32_derivs'])

        # If psmt not finalized, test that result does not have hex
        assert "hex" not in processed_psmt

        processed_psmt = self.nodes[2].descriptorprocesspsmt(psmt=psmt, descriptors=[descriptor], sighashtype="ALL", bip32derivs=False, finalize=True)
        decoded = self.nodes[2].decodepsmt(processed_psmt['psmt'])
        test_psmt_input_keys(decoded['inputs'][0], ['witness_utxo', 'non_witness_utxo', 'final_scriptwitness'])

        # Test psmt is complete
        assert_equal(processed_psmt['complete'], True)

        # Broadcast transaction
        self.nodes[2].sendrawtransaction(processed_psmt['hex'])

        self.log.info("Test descriptorprocesspsmt raises if an invalid sighashtype is passed")
        assert_raises_rpc_error(-8, "'all' is not a valid sighash parameter.", self.nodes[2].descriptorprocesspsmt, psmt, [descriptor], sighashtype="all")

        if not self.options.usecli:
            self.test_sighash_mismatch()
        self.test_sighash_adding()

if __name__ == '__main__':
    PSMTTest(__file__).main()
