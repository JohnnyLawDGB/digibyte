#!/usr/bin/env python3
<<<<<<< HEAD
# Copyright (c) 2014-2020 The Bitcoin Core developers
# Copyright (c) 2020-2022 The DigiByte Core developers
=======
# Copyright (c) 2014-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test fee estimation code."""
from copy import deepcopy
from decimal import Decimal
import os
import random
import time

from test_framework.messages import (
    COIN,
<<<<<<< HEAD
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
)
from test_framework.script import (
    CScript,
    OP_1,
    OP_2,
    OP_DROP,
    OP_TRUE,
)
from test_framework.script_util import (
    script_to_p2sh_script,
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
)
from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_raises_rpc_error,
    satoshi_round,
)
from test_framework.wallet import MiniWallet

<<<<<<< HEAD
# Construct 2 trivial P2SH's and the ScriptSigs that spend them
# So we can create many transactions without needing to spend
# time signing.
REDEEM_SCRIPT_1 = CScript([OP_1, OP_DROP])
REDEEM_SCRIPT_2 = CScript([OP_2, OP_DROP])
P2SH_1 = script_to_p2sh_script(REDEEM_SCRIPT_1)
P2SH_2 = script_to_p2sh_script(REDEEM_SCRIPT_2)
=======
MAX_FILE_AGE = 60
SECONDS_PER_HOUR = 60 * 60
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

def small_txpuzzle_randfee(
    wallet, from_node, conflist, unconflist, amount, min_fee, fee_increment, batch_reqs
):
    """Create and send a transaction with a random fee using MiniWallet.

<<<<<<< HEAD

def small_txpuzzle_randfee(from_node, conflist, unconflist, amount, min_fee, fee_increment):
    """Create and send a transaction with a random fee.

    The transaction pays to a trivial P2SH script, and assumes that its inputs
    are of the same form.
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    The function takes a list of confirmed outputs and unconfirmed outputs
    and attempts to use the confirmed list first for its inputs.
    It adds the newly created outputs to the unconfirmed list.
    Returns (raw transaction, fee)."""

    # It's best to exponentially distribute our random fees
    # because the buckets are exponentially spaced.
    # Exponentially distributed from 1-128 * fee_increment
    rand_fee = float(fee_increment) * (1.1892 ** random.randint(0, 28))
    # Total fee ranges from min_fee to min_fee + 127*fee_increment
    fee = min_fee - fee_increment + satoshi_round(rand_fee)
    utxos_to_spend = []
    total_in = Decimal("0.00000000")
    while total_in <= (amount + fee) and len(conflist) > 0:
        t = conflist.pop(0)
        total_in += t["value"]
        utxos_to_spend.append(t)
    while total_in <= (amount + fee) and len(unconflist) > 0:
        t = unconflist.pop(0)
        total_in += t["value"]
        utxos_to_spend.append(t)
    if total_in <= amount + fee:
<<<<<<< HEAD
        while total_in <= (amount + fee) and len(unconflist) > 0:
            t = unconflist.pop(0)
            total_in += t["amount"]
            tx.vin.append(CTxIn(COutPoint(int(t["txid"], 16), t["vout"]), b""))
        if total_in <= amount + fee:
            raise RuntimeError("Insufficient funds: need %d, have %d" % (amount + fee, total_in))
    tx.vout.append(CTxOut(int((total_in - amount - fee) * COIN), P2SH_1))
    tx.vout.append(CTxOut(int(amount * COIN), P2SH_2))
    # These transactions don't need to be signed, but we still have to insert
    # the ScriptSig that will satisfy the ScriptPubKey.
    for inp in tx.vin:
        inp.scriptSig = SCRIPT_SIG[inp.prevout.n]
    txid = from_node.sendrawtransaction(hexstring=tx.serialize().hex(), maxfeerate=0)
    unconflist.append({"txid": txid, "vout": 0, "amount": total_in - amount - fee})
    unconflist.append({"txid": txid, "vout": 1, "amount": amount})

    return (tx.serialize().hex(), fee)

=======
        raise RuntimeError(f"Insufficient funds: need {amount + fee}, have {total_in}")
    tx = wallet.create_self_transfer_multi(
        utxos_to_spend=utxos_to_spend,
        fee_per_output=0,
    )["tx"]
    tx.vout[0].nValue = int((total_in - amount - fee) * COIN)
    tx.vout.append(deepcopy(tx.vout[0]))
    tx.vout[1].nValue = int(amount * COIN)
    tx.rehash()
    txid = tx.hash
    tx_hex = tx.serialize().hex()

    batch_reqs.append(from_node.sendrawtransaction.get_request(hexstring=tx_hex, maxfeerate=0))
    unconflist.append({"txid": txid, "vout": 0, "value": total_in - amount - fee})
    unconflist.append({"txid": txid, "vout": 1, "value": amount})
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    return (tx.get_vsize(), fee)


def check_raw_estimates(node, fees_seen):
    """Call estimaterawfee and verify that the estimates meet certain invariants."""

<<<<<<< HEAD
    half_change = satoshi_round(prevtxout["amount"] / 2)
    rem_change = prevtxout["amount"] - half_change - Decimal("0.00100000")
    tx.vout.append(CTxOut(int(half_change * COIN), P2SH_1))
    tx.vout.append(CTxOut(int(rem_change * COIN), P2SH_2))

    # If this is the initial split we actually need to sign the transaction
    # Otherwise we just need to insert the proper ScriptSig
    if (initial_split):
        completetx = from_node.signrawtransactionwithwallet(tx.serialize().hex())["hex"]
    else:
        tx.vin[0].scriptSig = SCRIPT_SIG[prevtxout["vout"]]
        completetx = tx.serialize().hex()
    txid = from_node.sendrawtransaction(hexstring=completetx, maxfeerate=0)
    txouts.append({"txid": txid, "vout": 0, "amount": half_change})
    txouts.append({"txid": txid, "vout": 1, "amount": rem_change})

def check_raw_estimates(node, fees_seen):
    """Call estimaterawfee and verify that the estimates meet certain invariants."""

    delta = 1.0e-6  # account for rounding error
    for i in range(1, 26):
        for _, e in node.estimaterawfee(i).items():
            feerate = float(e["feerate"])
            assert_greater_than(feerate, 0)

            if feerate + delta < min(fees_seen) or feerate - delta > max(fees_seen):
                raise AssertionError("Estimated fee (%f) out of range (%f,%f)"
                                     % (feerate, min(fees_seen), max(fees_seen)))
=======
    delta = 1.0e-6  # account for rounding error
    for i in range(1, 26):
        for _, e in node.estimaterawfee(i).items():
            feerate = float(e["feerate"])
            assert_greater_than(feerate, 0)

            if feerate + delta < min(fees_seen) or feerate - delta > max(fees_seen):
                raise AssertionError(
                    f"Estimated fee ({feerate}) out of range ({min(fees_seen)},{max(fees_seen)})"
                )

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

def check_smart_estimates(node, fees_seen):
    """Call estimatesmartfee and verify that the estimates meet certain invariants."""

    delta = 1.0e-6  # account for rounding error
    last_feerate = float(max(fees_seen))
    all_smart_estimates = [node.estimatesmartfee(i) for i in range(1, 26)]
    mempoolMinFee = node.getmempoolinfo()["mempoolminfee"]
    minRelaytxFee = node.getmempoolinfo()["minrelaytxfee"]
    for i, e in enumerate(all_smart_estimates):  # estimate is for i+1
        feerate = float(e["feerate"])
        assert_greater_than(feerate, 0)
        assert_greater_than_or_equal(feerate, float(mempoolMinFee))
        assert_greater_than_or_equal(feerate, float(minRelaytxFee))

        if feerate + delta < min(fees_seen) or feerate - delta > max(fees_seen):
            raise AssertionError(
                f"Estimated fee ({feerate}) out of range ({min(fees_seen)},{max(fees_seen)})"
            )
        if feerate - delta > last_feerate:
            raise AssertionError(
                f"Estimated fee ({feerate}) larger than last fee ({last_feerate}) for lower number of confirms"
            )
        last_feerate = feerate

        if i == 0:
            assert_equal(e["blocks"], 2)
        else:
            assert_greater_than_or_equal(i + 1, e["blocks"])

<<<<<<< HEAD
=======

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
def check_estimates(node, fees_seen):
    check_raw_estimates(node, fees_seen)
    check_smart_estimates(node, fees_seen)

<<<<<<< HEAD
class EstimateFeeTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        # mine non-standard txs (e.g. txs with "dust" outputs)
        # Force fSendTrickle to true (via whitelist.noban)
        self.extra_args = [
            ["-acceptnonstdtxn", "-whitelist=noban@127.0.0.1"],
            ["-acceptnonstdtxn", "-whitelist=noban@127.0.0.1", "-blockmaxweight=68000"],
            ["-acceptnonstdtxn", "-whitelist=noban@127.0.0.1", "-blockmaxweight=32000"],
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
=======

def make_tx(wallet, utxo, feerate):
    """Create a 1in-1out transaction with a specific input and feerate (sat/vb)."""
    return wallet.create_self_transfer(
        utxo_to_spend=utxo,
        fee_rate=Decimal(feerate * 1000) / COIN,
    )


class EstimateFeeTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        # Force fSendTrickle to true (via whitelist.noban)
        self.extra_args = [
            ["-whitelist=noban@127.0.0.1"],
            ["-whitelist=noban@127.0.0.1", "-blockmaxweight=68000"],
            ["-whitelist=noban@127.0.0.1", "-blockmaxweight=32000"],
        ]
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    def setup_network(self):
        """
        We'll setup the network to have 3 nodes that all mine with different parameters.
        But first we need to use one node to create a lot of outputs
        which we will use to generate our transactions.
        """
        self.add_nodes(3, extra_args=self.extra_args)
        # Use node0 to mine blocks for input splitting
        # Node1 mines small blocks but that are bigger than the expected transaction rate.
        # NOTE: the CreateNewBlock code starts counting block weight at 4,000 weight,
        # (68k weight is room enough for 120 or so transactions)
        # Node2 is a stingy miner, that
        # produces too small blocks (room for only 55 or so transactions)
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()
        self.stop_nodes()

    def transact_and_mine(self, numblocks, mining_node):
        min_fee = Decimal("0.0002")
        # We will now mine numblocks blocks generating on average 100 transactions between each block
        # We shuffle our confirmed txout set before each set of transactions
        # small_txpuzzle_randfee will use the transactions that have inputs already in the chain when possible
        # resorting to tx's that depend on the mempool when those run out
        for _ in range(numblocks):
            random.shuffle(self.confutxo)
<<<<<<< HEAD
            for _ in range(random.randrange(100 - 50, 100 + 50)):
                from_index = random.randint(1, 2)
                (txhex, fee) = small_txpuzzle_randfee(self.nodes[from_index], self.confutxo,
                                                      self.memutxo, Decimal("0.007"), min_fee, min_fee)
                tx_kbytes = (len(txhex) // 2) / 1000.0
                self.fees_per_kb.append(float(fee) / tx_kbytes)
            self.sync_mempools(wait=.1)
            mined = mining_node.getblock(self.generate(mining_node, 1)[0], True)["tx"]
            self.sync_blocks(wait=.1)
=======
            batch_sendtx_reqs = []
            for _ in range(random.randrange(100 - 50, 100 + 50)):
                from_index = random.randint(1, 2)
                (tx_bytes, fee) = small_txpuzzle_randfee(
                    self.wallet,
                    self.nodes[from_index],
                    self.confutxo,
                    self.memutxo,
                    Decimal("0.005"),
                    min_fee,
                    min_fee,
                    batch_sendtx_reqs,
                )
                tx_kbytes = tx_bytes / 1000.0
                self.fees_per_kb.append(float(fee) / tx_kbytes)
            for node in self.nodes:
                node.batch(batch_sendtx_reqs)
            self.sync_mempools(wait=0.1)
            mined = mining_node.getblock(self.generate(mining_node, 1)[0], True)["tx"]
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            # update which txouts are confirmed
            newmem = []
            for utx in self.memutxo:
                if utx["txid"] in mined:
                    self.confutxo.append(utx)
                else:
                    newmem.append(utx)
            self.memutxo = newmem

<<<<<<< HEAD
    def run_test(self):
        self.log.info("This test is time consuming, please be patient")
        self.log.info("Splitting inputs so we can generate tx's")

        # Start node0
        self.start_node(0)
        self.txouts = []
        self.txouts2 = []
        # Split a coinbase into two transaction puzzle outputs
        split_inputs(self.nodes[0], self.nodes[0].listunspent(0), self.txouts, True)

        # Mine
        while len(self.nodes[0].getrawmempool()) > 0:
            self.generate(self.nodes[0], 1, sync_fun=self.no_op)

        # Repeatedly split those 2 outputs, doubling twice for each rep
        # Use txouts to monitor the available utxo, since these won't be tracked in wallet
        reps = 0
        while reps < 5:
            # Double txouts to txouts2
            while len(self.txouts) > 0:
                split_inputs(self.nodes[0], self.txouts, self.txouts2)
            while len(self.nodes[0].getrawmempool()) > 0:
                self.generate(self.nodes[0], 1, sync_fun=self.no_op)
            # Double txouts2 to txouts
            while len(self.txouts2) > 0:
                split_inputs(self.nodes[0], self.txouts2, self.txouts)
            while len(self.nodes[0].getrawmempool()) > 0:
                self.generate(self.nodes[0], 1, sync_fun=self.no_op)
            reps += 1
        self.log.info("Finished splitting")

        # Now we can connect the other nodes, didn't want to connect them earlier
        # so the estimates would not be affected by the splitting transactions
        self.start_node(1)
        self.start_node(2)
        self.connect_nodes(1, 0)
        self.connect_nodes(0, 2)
        self.connect_nodes(2, 1)

        self.sync_all()
=======
    def initial_split(self, node):
        """Split two coinbase UTxOs into many small coins"""
        self.confutxo = self.wallet.send_self_transfer_multi(
            from_node=node,
            utxos_to_spend=[self.wallet.get_utxo() for _ in range(2)],
            num_outputs=2048)['new_utxos']
        while len(node.getrawmempool()) > 0:
            self.generate(node, 1, sync_fun=self.no_op)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    def sanity_check_estimates_range(self):
        """Populate estimation buckets, assert estimates are in a sane range and
        are strictly increasing as the target decreases."""
        self.fees_per_kb = []
        self.memutxo = []
        self.log.info("Will output estimates for 1/2/3/6/15/25 blocks")

        for _ in range(2):
<<<<<<< HEAD
            self.log.info("Creating transactions and mining them with a block size that can't keep up")
=======
            self.log.info(
                "Creating transactions and mining them with a block size that can't keep up"
            )
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            # Create transactions and mine 10 small blocks with node 2, but create txs faster than we can mine
            self.transact_and_mine(10, self.nodes[2])
            check_estimates(self.nodes[1], self.fees_per_kb)

            self.log.info(
                "Creating transactions and mining them at a block size that is just big enough"
            )
            # Generate transactions while mining 10 more blocks, this time with node1
            # which mines blocks with capacity just above the rate that transactions are being created
            self.transact_and_mine(10, self.nodes[1])
            check_estimates(self.nodes[1], self.fees_per_kb)

        # Finish by mining a normal-sized block:
        while len(self.nodes[1].getrawmempool()) > 0:
            self.generate(self.nodes[1], 1)

<<<<<<< HEAD
        self.sync_blocks(self.nodes[0:3], wait=.1)
        self.log.info("Final estimates after emptying mempools")
        check_estimates(self.nodes[1], self.fees_per_kb)

        self.log.info("Testing that fee estimation is disabled in blocksonly.")
        self.restart_node(0, ["-blocksonly"])
        assert_raises_rpc_error(-32603, "Fee estimation disabled",
                                self.nodes[0].estimatesmartfee, 2)


if __name__ == '__main__':
    EstimateFeeTest().main()
=======
        self.log.info("Final estimates after emptying mempools")
        check_estimates(self.nodes[1], self.fees_per_kb)

    def test_feerate_mempoolminfee(self):
        high_val = 3 * self.nodes[1].estimatesmartfee(1)["feerate"]
        self.restart_node(1, extra_args=[f"-minrelaytxfee={high_val}"])
        check_estimates(self.nodes[1], self.fees_per_kb)
        self.restart_node(1)

    def sanity_check_rbf_estimates(self, utxos):
        """During 5 blocks, broadcast low fee transactions. Only 10% of them get
        confirmed and the remaining ones get RBF'd with a high fee transaction at
        the next block.
        The block policy estimator should return the high feerate.
        """
        # The broadcaster and block producer
        node = self.nodes[0]
        miner = self.nodes[1]
        # In sat/vb
        low_feerate = 1
        high_feerate = 10
        # Cache the utxos of which to replace the spender after it failed to get
        # confirmed
        utxos_to_respend = []
        txids_to_replace = []

        assert_greater_than_or_equal(len(utxos), 250)
        for _ in range(5):
            # Broadcast 45 low fee transactions that will need to be RBF'd
            txs = []
            for _ in range(45):
                u = utxos.pop(0)
                tx = make_tx(self.wallet, u, low_feerate)
                utxos_to_respend.append(u)
                txids_to_replace.append(tx["txid"])
                txs.append(tx)
            # Broadcast 5 low fee transaction which don't need to
            for _ in range(5):
                tx = make_tx(self.wallet, utxos.pop(0), low_feerate)
                txs.append(tx)
            batch_send_tx = [node.sendrawtransaction.get_request(tx["hex"]) for tx in txs]
            for n in self.nodes:
                n.batch(batch_send_tx)
            # Mine the transactions on another node
            self.sync_mempools(wait=0.1, nodes=[node, miner])
            for txid in txids_to_replace:
                miner.prioritisetransaction(txid=txid, fee_delta=-COIN)
            self.generate(miner, 1)
            # RBF the low-fee transactions
            while len(utxos_to_respend) > 0:
                u = utxos_to_respend.pop(0)
                tx = make_tx(self.wallet, u, high_feerate)
                node.sendrawtransaction(tx["hex"])
                txs.append(tx)
            dec_txs = [res["result"] for res in node.batch([node.decoderawtransaction.get_request(tx["hex"]) for tx in txs])]
            self.wallet.scan_txs(dec_txs)


        # Mine the last replacement txs
        self.sync_mempools(wait=0.1, nodes=[node, miner])
        self.generate(miner, 1)

        # Only 10% of the transactions were really confirmed with a low feerate,
        # the rest needed to be RBF'd. We must return the 90% conf rate feerate.
        high_feerate_kvb = Decimal(high_feerate) / COIN * 10 ** 3
        est_feerate = node.estimatesmartfee(2)["feerate"]
        assert_equal(est_feerate, high_feerate_kvb)

    def test_old_fee_estimate_file(self):
        # Get the initial fee rate while node is running
        fee_rate = self.nodes[0].estimatesmartfee(1)["feerate"]

        # Restart node to ensure fee_estimate.dat file is read
        self.restart_node(0)
        assert_equal(self.nodes[0].estimatesmartfee(1)["feerate"], fee_rate)

        fee_dat = self.nodes[0].chain_path / "fee_estimates.dat"

        # Stop the node and backdate the fee_estimates.dat file more than MAX_FILE_AGE
        self.stop_node(0)
        last_modified_time = time.time() - (MAX_FILE_AGE + 1) * SECONDS_PER_HOUR
        os.utime(fee_dat, (last_modified_time, last_modified_time))

        # Start node and ensure the fee_estimates.dat file was not read
        self.start_node(0)
        assert_equal(self.nodes[0].estimatesmartfee(1)["errors"], ["Insufficient data or no feerate found"])


    def test_estimate_dat_is_flushed_periodically(self):
        fee_dat = self.nodes[0].chain_path / "fee_estimates.dat"
        os.remove(fee_dat) if os.path.exists(fee_dat) else None

        # Verify that fee_estimates.dat does not exist
        assert_equal(os.path.isfile(fee_dat), False)

        # Verify if the string "Flushed fee estimates to fee_estimates.dat." is present in the debug log file.
        # If present, it indicates that fee estimates have been successfully flushed to disk.
        with self.nodes[0].assert_debug_log(expected_msgs=["Flushed fee estimates to fee_estimates.dat."], timeout=1):
            # Mock the scheduler for an hour to flush fee estimates to fee_estimates.dat
            self.nodes[0].mockscheduler(SECONDS_PER_HOUR)

        # Verify that fee estimates were flushed and fee_estimates.dat file is created
        assert_equal(os.path.isfile(fee_dat), True)

        # Verify that the estimates remain the same if there are no blocks in the flush interval
        block_hash_before = self.nodes[0].getbestblockhash()
        fee_dat_initial_content = open(fee_dat, "rb").read()
        with self.nodes[0].assert_debug_log(expected_msgs=["Flushed fee estimates to fee_estimates.dat."], timeout=1):
            # Mock the scheduler for an hour to flush fee estimates to fee_estimates.dat
            self.nodes[0].mockscheduler(SECONDS_PER_HOUR)

        # Verify that there were no blocks in between the flush interval
        assert_equal(block_hash_before, self.nodes[0].getbestblockhash())

        fee_dat_current_content = open(fee_dat, "rb").read()
        assert_equal(fee_dat_current_content, fee_dat_initial_content)

        # Verify that the estimates remain the same after shutdown with no blocks before shutdown
        self.restart_node(0)
        fee_dat_current_content = open(fee_dat, "rb").read()
        assert_equal(fee_dat_current_content, fee_dat_initial_content)

        # Verify that the estimates are not the same if new blocks were produced in the flush interval
        with self.nodes[0].assert_debug_log(expected_msgs=["Flushed fee estimates to fee_estimates.dat."], timeout=1):
            # Mock the scheduler for an hour to flush fee estimates to fee_estimates.dat
            self.generate(self.nodes[0], 5, sync_fun=self.no_op)
            self.nodes[0].mockscheduler(SECONDS_PER_HOUR)

        fee_dat_current_content = open(fee_dat, "rb").read()
        assert fee_dat_current_content != fee_dat_initial_content

        fee_dat_initial_content = fee_dat_current_content

        # Generate blocks before shutdown and verify that the fee estimates are not the same
        self.generate(self.nodes[0], 5, sync_fun=self.no_op)
        self.restart_node(0)
        fee_dat_current_content = open(fee_dat, "rb").read()
        assert fee_dat_current_content != fee_dat_initial_content


    def test_acceptstalefeeestimates_option(self):
        # Get the initial fee rate while node is running
        fee_rate = self.nodes[0].estimatesmartfee(1)["feerate"]

        self.stop_node(0)

        fee_dat = self.nodes[0].chain_path / "fee_estimates.dat"

        # Stop the node and backdate the fee_estimates.dat file more than MAX_FILE_AGE
        last_modified_time = time.time() - (MAX_FILE_AGE + 1) * SECONDS_PER_HOUR
        os.utime(fee_dat, (last_modified_time, last_modified_time))

        # Restart node with -acceptstalefeeestimates option to ensure fee_estimate.dat file is read
        self.start_node(0,extra_args=["-acceptstalefeeestimates"])
        assert_equal(self.nodes[0].estimatesmartfee(1)["feerate"], fee_rate)


    def run_test(self):
        self.log.info("This test is time consuming, please be patient")
        self.log.info("Splitting inputs so we can generate tx's")

        # Split two coinbases into many small utxos
        self.start_node(0)
        self.wallet = MiniWallet(self.nodes[0])
        self.initial_split(self.nodes[0])
        self.log.info("Finished splitting")

        # Now we can connect the other nodes, didn't want to connect them earlier
        # so the estimates would not be affected by the splitting transactions
        self.start_node(1)
        self.start_node(2)
        self.connect_nodes(1, 0)
        self.connect_nodes(0, 2)
        self.connect_nodes(2, 1)
        self.sync_all()

        self.log.info("Testing estimates with single transactions.")
        self.sanity_check_estimates_range()

        self.log.info("Test fee_estimates.dat is flushed periodically")
        self.test_estimate_dat_is_flushed_periodically()

        # check that the effective feerate is greater than or equal to the mempoolminfee even for high mempoolminfee
        self.log.info(
            "Test fee rate estimation after restarting node with high MempoolMinFee"
        )
        self.test_feerate_mempoolminfee()

        self.log.info("Test acceptstalefeeestimates option")
        self.test_acceptstalefeeestimates_option()

        self.log.info("Test reading old fee_estimates.dat")
        self.test_old_fee_estimate_file()

        self.log.info("Restarting node with fresh estimation")
        self.stop_node(0)
        fee_dat = os.path.join(self.nodes[0].chain_path, "fee_estimates.dat")
        os.remove(fee_dat)
        self.start_node(0)
        self.connect_nodes(0, 1)
        self.connect_nodes(0, 2)

        self.log.info("Testing estimates with RBF.")
        self.sanity_check_rbf_estimates(self.confutxo + self.memutxo)

        self.log.info("Testing that fee estimation is disabled in blocksonly.")
        self.restart_node(0, ["-blocksonly"])
        assert_raises_rpc_error(
            -32603, "Fee estimation disabled", self.nodes[0].estimatesmartfee, 2
        )


if __name__ == "__main__":
    EstimateFeeTest().main()
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
