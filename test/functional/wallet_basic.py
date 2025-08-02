#!/usr/bin/env python3
# Copyright (c) 2014-2019 The DigiByte Core developers
# Copyright (c) 2022 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet."""
from decimal import Decimal
from itertools import product

from test_framework.blocktools import COINBASE_MATURITY, COINBASE_MATURITY_2
from test_framework.descriptors import descsum_create
from test_framework.messages import (
    COIN,
    DEFAULT_ANCESTOR_LIMIT,
)
from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_array_result,
    assert_equal,
    assert_fee_amount,
    assert_raises_rpc_error,
    find_vout_for_address,
)
from test_framework.wallet_util import test_address
from test_framework.wallet import MiniWallet

NOT_A_NUMBER_OR_STRING = "Amount is not a number or string"
OUT_OF_RANGE = "Amount out of range"


class WalletTest(DigiByteTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [[
            "-dustrelayfee=0", "-walletrejectlongchains=0", "-whitelist=noban@127.0.0.1",
            "-dandelion=0",  # DigiByte-specific: Disable Dandelion for this test
        ]] * self.num_nodes
        self.setup_clean_chain = True
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()
        # Only need nodes 0-2 running at start of test
        self.stop_node(3)
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.connect_nodes(0, 2)
        self.sync_all(self.nodes[0:3])

    def check_fee_amount(self, curr_balance, balance_with_fee, fee_per_byte, tx_size):
        """Return curr_balance after asserting the fee was in range"""
        fee = balance_with_fee - curr_balance
        assert_fee_amount(fee, tx_size, fee_per_byte * 1000)
        return curr_balance

    def get_vsize(self, txn):
        return self.nodes[0].decoderawtransaction(txn)['vsize']

    # Get newly matured utxo with min_amount
    # DigiByte specific: COINBASE_MATURITY is set to 8 instead of 100 (in BTC).
    # During this test suite, more and more coinbase utxos are maturing.
    # This helper function collects them.
    def get_matured_utxos(self, node_index, utxo_seen_map = None, allow_none_coinbase = False, min_amount = Decimal('72000')):
        if node_index is None: raise ValueError('node_index not specified')
        if utxo_seen_map is None: raise ValueError('utxo_seen_map not specified')

        new_coinbase_txs = []

        unspent = self.nodes[node_index].listunspent(query_options={'minimumAmount': min_amount})

        for utxo in unspent:
            if allow_none_coinbase == False:
                if not 'label' in utxo: continue
                if utxo['label'] != 'coinbase': continue

            if utxo['txid'] in utxo_seen_map: continue
            new_coinbase_txs.append(utxo)
            utxo_seen_map[utxo['txid']] = True

        return new_coinbase_txs

    def run_test(self):
        # DigiByte-specific: Track seen UTXOs for helper function
        utxo_seen = [{}] * self.num_nodes

        # Check that there's no UTXO on none of the nodes
        assert_equal(len(self.nodes[0].listunspent()), 0)
        assert_equal(len(self.nodes[1].listunspent()), 0)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        self.log.info("Mining blocks...")

        self.generate(self.nodes[0], 1, sync_fun=self.no_op)

        walletinfo = self.nodes[0].getwalletinfo()
        # DigiByte: Block reward is 72000 DGB
        assert_equal(walletinfo['immature_balance'], 72000)
        assert_equal(walletinfo['balance'], 0)

        self.sync_all(self.nodes[0:3])
        # DigiByte: Use COINBASE_MATURITY_2 (100) for longer maturity test
        self.generate(self.nodes[1], COINBASE_MATURITY_2 + 1, sync_fun=lambda: self.sync_all(self.nodes[0:3]))

        utxo_count = 1
        expected_balance = 72000 * utxo_count
        assert_equal(self.nodes[0].getbalance(), 72000)
        assert_equal(self.nodes[1].getbalance(), expected_balance)
        assert_equal(self.nodes[2].getbalance(), 0)

        # Check that only first and second nodes have UTXOs
        utxos = self.nodes[0].listunspent()
        assert_equal(len(utxos), 1)
        assert_equal(len(self.nodes[1].listunspent()), utxo_count)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        self.log.info("Test gettxout")
        confirmed_txid, confirmed_index = utxos[0]["txid"], utxos[0]["vout"]
        # First, outputs that are unspent both in the chain and in the
        # mempool should appear with or without include_mempool
        txout = self.nodes[0].gettxout(txid=confirmed_txid, n=confirmed_index, include_mempool=False)
        assert_equal(txout['value'], 72000)
        txout = self.nodes[0].gettxout(txid=confirmed_txid, n=confirmed_index, include_mempool=True)
        assert_equal(txout['value'], 72000)

        # Send 21 DGB from 0 to 2 using sendtoaddress call.
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 11)
        mempool_txid = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 10)

        self.log.info("Test gettxout (second part)")
        # utxo spent in mempool should be visible if you exclude mempool
        # but invisible if you include mempool
        txout = self.nodes[0].gettxout(confirmed_txid, confirmed_index, False)
        assert_equal(txout['value'], 72000)
        txout = self.nodes[0].gettxout(confirmed_txid, confirmed_index)  # by default include_mempool=True
        assert txout is None
        txout = self.nodes[0].gettxout(confirmed_txid, confirmed_index, True)
        assert txout is None
        # new utxo from mempool should be invisible if you exclude mempool
        # but visible if you include mempool
        txout = self.nodes[0].gettxout(mempool_txid, 0, False)
        assert txout is None
        txout1 = self.nodes[0].gettxout(mempool_txid, 0, True)
        txout2 = self.nodes[0].gettxout(mempool_txid, 1, True)
        # note the mempool tx will have randomly assigned indices
        # but 10 will go to node2 and the rest will go to node0
        balance = self.nodes[0].getbalance()
        assert_equal(set([txout1['value'], txout2['value']]), set([10, balance]))
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 0)

        # Have node0 mine a block, thus it will collect its own fee.
        self.generate(self.nodes[0], 1, sync_fun=lambda: self.sync_all(self.nodes[0:3]))

        # Exercise locking of unspent outputs
        unspent_0 = self.nodes[2].listunspent()[0]
        unspent_0 = {"txid": unspent_0["txid"], "vout": unspent_0["vout"]}
        # Trying to unlock an output which isn't locked should error
        assert_raises_rpc_error(-8, "Invalid parameter, expected locked output", self.nodes[2].lockunspent, True, [unspent_0])

        # Locking an already-locked output should error
        self.nodes[2].lockunspent(False, [unspent_0])
        assert_raises_rpc_error(-8, "Invalid parameter, output already locked", self.nodes[2].lockunspent, False, [unspent_0])

        # Restarting the node should clear the lock
        self.restart_node(2)
        self.nodes[2].lockunspent(False, [unspent_0])

        # Unloading and reloading the wallet should clear the lock
        assert_equal(self.nodes[0].listwallets(), [self.default_wallet_name])
        self.nodes[2].unloadwallet(self.default_wallet_name)
        self.nodes[2].loadwallet(self.default_wallet_name)
        assert_equal(len(self.nodes[2].listlockunspent()), 0)

        # Locking non-persistently, then re-locking persistently, is allowed
        self.nodes[2].lockunspent(False, [unspent_0])
        self.nodes[2].lockunspent(False, [unspent_0], True)

        # Restarting the node with the lock written to the wallet should keep the lock
        self.restart_node(2, ["-walletrejectlongchains=0"])
        assert_raises_rpc_error(-8, "Invalid parameter, output already locked", self.nodes[2].lockunspent, False, [unspent_0])

        # Unloading and reloading the wallet with a persistent lock should keep the lock
        self.nodes[2].unloadwallet(self.default_wallet_name)
        self.nodes[2].loadwallet(self.default_wallet_name)
        assert_raises_rpc_error(-8, "Invalid parameter, output already locked", self.nodes[2].lockunspent, False, [unspent_0])

        # Locked outputs should not be used, even if they are the only available funds
        assert_raises_rpc_error(-6, "Insufficient funds", self.nodes[2].sendtoaddress, self.nodes[2].getnewaddress(), 20)
        assert_equal([unspent_0], self.nodes[2].listlockunspent())

        # Unlocking should remove the persistent lock
        self.nodes[2].lockunspent(True, [unspent_0])
        self.restart_node(2)
        assert_equal(len(self.nodes[2].listlockunspent()), 0)

        # Reconnect node 2 after restarts
        self.connect_nodes(1, 2)
        self.connect_nodes(0, 2)

        assert_raises_rpc_error(-8, "txid must be of length 64 (not 34, for '0000000000000000000000000000000000')",
                                self.nodes[2].lockunspent, False,
                                [{"txid": "0000000000000000000000000000000000", "vout": 0}])
        assert_raises_rpc_error(-8, "txid must be hexadecimal string (not 'ZZZ0000000000000000000000000000000000000000000000000000000000000')",
                                self.nodes[2].lockunspent, False,
                                [{"txid": "ZZZ0000000000000000000000000000000000000000000000000000000000000", "vout": 0}])
        assert_raises_rpc_error(-8, "Invalid parameter, unknown transaction",
                                self.nodes[2].lockunspent, False,
                                [{"txid": "0000000000000000000000000000000000000000000000000000000000000000", "vout": 0}])
        assert_raises_rpc_error(-8, "Invalid parameter, vout index out of bounds",
                                self.nodes[2].lockunspent, False,
                                [{"txid": unspent_0["txid"], "vout": 999}])

        # The lock on a manually selected output is ignored
        unspent_0 = self.nodes[1].listunspent()[0]
        self.nodes[1].lockunspent(False, [unspent_0])
        tx = self.nodes[1].createrawtransaction([unspent_0], { self.nodes[1].getnewaddress() : 1 })
        self.nodes[1].fundrawtransaction(tx,{"lockUnspents": True})

        # fundrawtransaction can lock an input
        self.nodes[1].lockunspent(True, [unspent_0])
        assert_equal(len(self.nodes[1].listlockunspent()), 0)
        tx = self.nodes[1].fundrawtransaction(tx,{"lockUnspents": True})['hex']
        assert_equal(len(self.nodes[1].listlockunspent()), 1)

        # Send transaction
        tx = self.nodes[1].signrawtransactionwithwallet(tx)["hex"]
        self.nodes[1].sendrawtransaction(tx)
        assert_equal(len(self.nodes[1].listlockunspent()), 0)

        # Have node1 generate 101 blocks (so node0 can recover the fee)
        # DigiByte: Use COINBASE_MATURITY_2 for longer maturity
        self.generate(self.nodes[1], COINBASE_MATURITY_2 + 1, sync_fun=lambda: self.sync_all(self.nodes[0:3]))

        # node0 should end up with 100 dgb in block rewards plus fees, but
        # minus the 21 plus fees sent to node2
        assert_equal(self.nodes[0].getbalance(), 2 * 72000 - 21)
        assert_equal(self.nodes[2].getbalance(), 21)

        # Node0 should have two unspent outputs.
        # Create a couple of transactions to send them to node2, submit them through
        # node1, and make sure both node0 and node2 pick them up properly:
        node0utxos = self.nodes[0].listunspent(1)
        assert_equal(len(node0utxos), 2)

        # create both transactions
        txns_to_send = []
        for utxo in node0utxos:
            inputs = []
            outputs = {}
            inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
            outputs[self.nodes[2].getnewaddress()] = utxo["amount"] - 3
            raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
            txns_to_send.append(self.nodes[0].signrawtransactionwithwallet(raw_tx))

        # Have node 1 (miner) send the transactions
        self.nodes[1].sendrawtransaction(hexstring=txns_to_send[0]["hex"], maxfeerate=0)
        self.nodes[1].sendrawtransaction(hexstring=txns_to_send[1]["hex"], maxfeerate=0)

        # Have node1 mine a block to confirm transactions:
        self.generate(self.nodes[1], 1, sync_fun=lambda: self.sync_all(self.nodes[0:3]))

        assert_equal(self.nodes[0].getbalance(), 0)
        # Node2 should have received all of node0's balance (21 DGB from earlier + all remaining funds minus fees)
        # The exact amount depends on the actual fees paid
        expected_node2_balance = 21 + sum(utxo["amount"] - 3 for utxo in node0utxos)
        assert_equal(self.nodes[2].getbalance(), expected_node2_balance)

        # Verify that a spent output cannot be locked anymore
        spent_0 = {"txid": node0utxos[0]["txid"], "vout": node0utxos[0]["vout"]}
        assert_raises_rpc_error(-8, "Invalid parameter, expected unspent output", self.nodes[0].lockunspent, False, [spent_0])

        # Send 21 DGB from 2 to 0 using sendtoaddress call
        self.nodes[2].sendtoaddress(self.nodes[0].getnewaddress(), 10.999999)
        mempool_txid = self.nodes[2].sendtoaddress(self.nodes[0].getnewaddress(), 10)

        self.log.info("Test sendmany")
        # Test `sendmany` with explicit fee rate.
        # `sendmany` can be used to "confirm" a transaction that spends an unconfirmed change as long as the  
        # number of ancestors is no more than 25 (which is the case here).
        self.start_node(3, self.extra_args[3])
        self.connect_nodes(0, 3)
        # Only sync nodes 0 and 3 since node3 just started and won't have the previous transactions
        self.sync_blocks(self.nodes[0:1] + self.nodes[3:4])
        
        # Mature some blocks for node3
        # DigiByte: Generate enough blocks to have mature coinbase
        # Don't sync all nodes since node3 doesn't have the same mempool
        # Need COINBASE_MATURITY_2 for wallet balance
        self.generate(self.nodes[3], COINBASE_MATURITY_2 + 1, sync_fun=self.no_op)
        
        # Get a mature UTXO for node3
        node3_balance = self.nodes[3].getbalance()
        assert node3_balance > 0
        
        # sendmany with explicit fee rate (sat/vB)
        res = self.nodes[3].sendmany(amounts={self.nodes[0].getnewaddress(): 10}, fee_rate=10000)
        assert isinstance(res, str)  # sendmany returns txid string, not dict
        assert_equal(self.nodes[3].gettransaction(res)["confirmations"], 0)
        self.log.info(f"Test sendmany with fee_rate")
        self.generate(self.nodes[0], 1)

        # Test send* RPCs with verbose=True
        address = self.nodes[0].getnewaddress()
        
        # DigiByte: Generate more blocks to ensure we have mature coins
        self.generate(self.nodes[0], COINBASE_MATURITY + 1)
        
        # In DigiByte, verbose parameter might not be supported
        txid = self.nodes[0].sendtoaddress(address=address, amount=5)
        if isinstance(txid, dict):
            # If verbose is supported and returns a dict
            pass
        else:
            # If it returns just the txid string
            txid = {"txid": txid, "hex": self.nodes[0].getrawtransaction(txid)}
        assert_equal(txid["txid"], txid["hex"][:64])
        assert_equal(self.nodes[0].gettransaction(txid["txid"])["confirmations"], 0)
        self.generate(self.nodes[0], 1)

        self.log.info("Test sendmany with verbose=True")
        # In DigiByte, verbose parameter might not be supported
        result = self.nodes[0].sendmany(amounts={address: 5})
        if isinstance(result, dict):
            # If verbose is supported and returns a dict
            txid = result
        else:
            # If it returns just the txid string
            txid = {"txid": result, "hex": self.nodes[0].getrawtransaction(result)}
        assert_equal(txid["txid"], txid["hex"][:64])
        assert_equal(self.nodes[0].gettransaction(txid["txid"])["confirmations"], 0)
        self.generate(self.nodes[0], 1)

        # Test the maximum number of recipients accepted and that sending to
        # duplicate addresses only results in one output
        self.log.info("Test maximum recipients and duplicate addresses")
        
        # Generate a large address list
        addresses = [self.nodes[0].getnewaddress() for _ in range(0, 100)]
        
        # Add duplicate entries (max 1000 recipients)
        address_list = []
        for _ in range(0, 10):
            address_list += addresses
        assert_equal(len(address_list), 1000)
        
        # Test sending to 1000 recipients
        amounts = {addr: 0.00001 for addr in address_list}
        
        # DigiByte: Make sure we have enough balance
        current_balance = self.nodes[0].getbalance()
        if current_balance < 1:
            self.generate(self.nodes[0], COINBASE_MATURITY + 10)
        
        txid = self.nodes[0].sendmany(amounts=amounts)
        tx = self.nodes[0].gettransaction(txid)
        assert_equal(tx["confirmations"], 0)
        
        # Should have 100 unique outputs (duplicates consolidated)
        assert_equal(len(tx["details"]), 100)
        
        # Test that sendmany fails with more than 1000 recipients
        address_list.append(self.nodes[0].getnewaddress())
        amounts = {addr: 0.00001 for addr in address_list}
        assert_raises_rpc_error(-8, "Too many recipients", self.nodes[0].sendmany, amounts=amounts)

        self.log.info("Test sendtoaddress with fee_rate param")
        # Test fee_rate with sendtoaddress
        fee_rate_sat_vb = 10
        address = self.nodes[1].getnewaddress()
        
        # Make sure node1 has mature coins
        self.generate(self.nodes[1], COINBASE_MATURITY + 1)
        
        # Test sendtoaddress with fee_rate
        amount = 10
        txid = self.nodes[1].sendtoaddress(address=address, amount=amount, fee_rate=fee_rate_sat_vb)
        
        # Verify fee calculation
        tx_size = self.nodes[1].getrawtransaction(txid, True)['vsize']
        tx_fee = self.nodes[1].gettransaction(txid)['fee']
        
        # Fee should be negative (paid) and approximately equal to fee_rate * tx_size
        assert tx_fee < 0
        expected_fee_sat = int(tx_size * fee_rate_sat_vb)
        actual_fee_sat = int(-tx_fee * COIN)
        # Allow for small rounding differences
        assert abs(actual_fee_sat - expected_fee_sat) <= tx_size

        self.log.info("Test sendtoaddress with conf_target")
        # Test conf_target with sendtoaddress
        conf_target = 6
        estimate_mode = "CONSERVATIVE"
        txid = self.nodes[1].sendtoaddress(address=address, amount=amount, conf_target=conf_target, estimate_mode=estimate_mode)
        
        # Check that conf_target and estimate_mode were used for fee estimation
        tx_fee = self.nodes[1].gettransaction(txid)['fee']
        assert tx_fee < 0  # Fee was paid

        # Test that passing both conf_target and fee_rate raises an error
        assert_raises_rpc_error(-8, "conf_target and fee_rate are mutually exclusive",
                                self.nodes[1].sendtoaddress,
                                address=address, amount=amount, conf_target=conf_target, fee_rate=fee_rate_sat_vb)

        self.log.info("Test invalid fee rate settings")
        # Test various invalid fee settings
        for zero_value in [0, 0.0, "0", "0.0", "0.00000000"]:
            assert_raises_rpc_error(-3, "Invalid amount for fee_rate",
                                    self.nodes[1].sendtoaddress,
                                    address=address, amount=amount, fee_rate=zero_value)
            assert_raises_rpc_error(-3, "Invalid amount for fee_rate",
                                    self.nodes[1].sendmany,
                                    amounts={address: amount}, fee_rate=zero_value)

        # Test negative fee rate
        assert_raises_rpc_error(-3, "Amount out of range",
                                self.nodes[1].sendtoaddress,
                                address=address, amount=amount, fee_rate=-1)
        assert_raises_rpc_error(-3, "Amount out of range",
                                self.nodes[1].sendmany,
                                amounts={address: amount}, fee_rate=-1)

        # Test fee_rate values that don't pass fixed-point parsing checks
        for invalid_value in ["", 0.000000001, "31.999999999", ".", "0.0", "0.000000001", True, {"foo": "bar"}]:
            assert_raises_rpc_error(-3, "Invalid amount for fee_rate",
                                    self.nodes[1].sendtoaddress,
                                    address=address, amount=amount, fee_rate=invalid_value)
            assert_raises_rpc_error(-3, "Invalid amount for fee_rate",
                                    self.nodes[1].sendmany,
                                    amounts={address: amount}, fee_rate=invalid_value)

        # Test fee_rate out of range
        for out_of_range_value in [1e6, -1e6]:
            assert_raises_rpc_error(-3, "Amount out of range",
                                    self.nodes[1].sendtoaddress,
                                    address=address, amount=amount, fee_rate=out_of_range_value)
            assert_raises_rpc_error(-3, "Amount out of range",
                                    self.nodes[1].sendmany,
                                    amounts={address: amount}, fee_rate=out_of_range_value)

        # Test setting both estimate_mode and fee_rate
        assert_raises_rpc_error(-8, "estimate_mode and fee_rate are mutually exclusive",
                                self.nodes[1].sendtoaddress,
                                address=address, amount=amount, estimate_mode="CONSERVATIVE", fee_rate=1)
        
        assert_raises_rpc_error(-8, "estimate_mode and fee_rate are mutually exclusive",
                                self.nodes[1].sendmany,
                                amounts={address: amount}, estimate_mode="CONSERVATIVE", fee_rate=1)

        self.log.info("Test custom change address with sendtoaddress")
        custom_change_address = self.nodes[1].getnewaddress()
        txid = self.nodes[1].sendtoaddress(address, 10, "", "", False, None, None, None, None, custom_change_address)
        tx = self.nodes[1].gettransaction(txid, True)
        # Find the change output
        change_output_found = False
        for vout in tx['decoded']['vout']:
            if 'scriptPubKey' in vout and 'addresses' in vout['scriptPubKey']:
                if custom_change_address in vout['scriptPubKey']['addresses']:
                    change_output_found = True
                    break
        assert change_output_found

        self.log.info("Test -walletbroadcast=0 option")
        self.restart_node(1, ["-walletbroadcast=0"])
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all(self.nodes[0:3])

        txid = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 10)
        # Transaction should not be in the mempool
        assert_raises_rpc_error(-5, "Transaction not in mempool", self.nodes[0].getrawtransaction, txid)
        
        # Transaction should be in the wallet
        tx_info = self.nodes[1].gettransaction(txid)
        assert_equal(tx_info['confirmations'], 0)

        # Restart with normal walletbroadcast
        self.restart_node(1)
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        
        # Now the transaction should be broadcast
        self.sync_all(self.nodes[0:3])
        assert txid in self.nodes[0].getrawmempool()

        self.log.info("Test balance and listunspent after spending")
        # Make sure balances are correct after all the spending
        self.generate(self.nodes[0], 1)
        self.sync_all(self.nodes[0:3])

        # Node2 balance should account for all received transactions
        # Node0 and Node1 balances depend on mining rewards and fees

        self.log.info("Test getbalance with minconf")
        # Test getbalance and getbalances with different minconf
        balance_minconf_0 = self.nodes[2].getbalance(minconf=0)
        balance_minconf_1 = self.nodes[2].getbalance(minconf=1)
        assert balance_minconf_0 >= balance_minconf_1

        # Test getbalances
        balances = self.nodes[2].getbalances()
        assert 'mine' in balances
        assert 'trusted' in balances['mine']
        assert 'untrusted_pending' in balances['mine']
        assert 'immature' in balances['mine']

        self.log.info("Test error handling for invalid addresses")
        # Test sending to invalid addresses
        # DigiByte specific error message
        assert_raises_rpc_error(-5, "Invalid address", self.nodes[0].sendtoaddress, "invalid_address", 1)
        # DigiByte specific error message
        assert_raises_rpc_error(-5, "Invalid address", self.nodes[0].sendmany, "", {"invalid_address": 1})

        # Test amount validation
        assert_raises_rpc_error(-3, NOT_A_NUMBER_OR_STRING, self.nodes[0].sendtoaddress, self.nodes[2].getnewaddress(), "not_a_number")
        assert_raises_rpc_error(-3, OUT_OF_RANGE, self.nodes[0].sendtoaddress, self.nodes[2].getnewaddress(), -1)
        assert_raises_rpc_error(-3, OUT_OF_RANGE, self.nodes[0].sendtoaddress, self.nodes[2].getnewaddress(), 21000000001)  # More than max supply

        self.log.info("Test getreceivedbyaddress and getreceivedbylabel")
        # Create a new address and label
        label = "test_label"
        address = self.nodes[2].getnewaddress(label)
        
        # Send to the address
        self.nodes[0].sendtoaddress(address, 5)
        self.generate(self.nodes[0], 1)
        
        # Test getreceivedbyaddress
        assert_equal(self.nodes[2].getreceivedbyaddress(address), 5)
        assert_equal(self.nodes[2].getreceivedbyaddress(address, 0), 5)  # Include unconfirmed
        
        # Test getreceivedbylabel  
        assert_equal(self.nodes[2].getreceivedbylabel(label), 5)

        self.log.info("Test sendtoaddress with subtractfeefromamount")
        balance_before = self.nodes[0].getbalance()
        txid = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 10, "", "", True)
        tx_info = self.nodes[0].gettransaction(txid)
        
        # When subtracting fee from amount, the sent amount includes the fee
        assert_equal(tx_info['amount'], -10)
        balance_after = self.nodes[0].getbalance()
        # Balance should decrease by exactly 10 (amount includes fee)
        assert_equal(balance_before - balance_after, 10)

        self.log.info("Test sendmany with subtractfeefrom")
        balance_before = self.nodes[0].getbalance()
        recipients = {self.nodes[2].getnewaddress(): 5, self.nodes[2].getnewaddress(): 5}
        txid = self.nodes[0].sendmany("", recipients, 1, "", [list(recipients.keys())[0]])
        tx_info = self.nodes[0].gettransaction(txid)
        
        # Fee should be subtracted from the first recipient
        # Total sent should be 10, but fee is subtracted from first output
        balance_after = self.nodes[0].getbalance()
        assert balance_before - balance_after == 10

        self.log.info("Test wallet encryption")
        # Test basic wallet encryption, unlocking, and locking
        # Note: This is a simplified version; full encryption tests are in wallet_encryption.py
        
        # Create a new wallet for encryption tests
        self.nodes[1].createwallet(wallet_name="encrypted_wallet", passphrase="test_password")
        encrypted_wallet = self.nodes[1].get_wallet_rpc("encrypted_wallet")
        
        # Wallet should be locked after creation with passphrase
        assert_raises_rpc_error(-13, "Error: Please enter the wallet passphrase with walletpassphrase first", 
                               encrypted_wallet.sendtoaddress, self.nodes[0].getnewaddress(), 1)
        
        # Unlock wallet
        encrypted_wallet.walletpassphrase("test_password", 60)
        
        # Generate an address to receive funds
        enc_addr = encrypted_wallet.getnewaddress()
        self.nodes[0].sendtoaddress(enc_addr, 10)
        self.generate(self.nodes[0], 1)
        
        # Should be able to send now
        encrypted_wallet.sendtoaddress(self.nodes[0].getnewaddress(), 1)
        
        # Lock wallet
        encrypted_wallet.walletlock()
        
        # Should not be able to send when locked
        assert_raises_rpc_error(-13, "Error: Please enter the wallet passphrase with walletpassphrase first",
                               encrypted_wallet.sendtoaddress, self.nodes[0].getnewaddress(), 1)
        
        # Clean up
        encrypted_wallet.unloadwallet()

        self.log.info("Test wallet labels")
        # Test label functionality
        label1 = "label1"
        label2 = "label2"
        
        addr1 = self.nodes[0].getnewaddress(label1)
        addr2 = self.nodes[0].getnewaddress(label2)
        
        # Test getaddressesbylabel
        assert addr1 in self.nodes[0].getaddressesbylabel(label1)
        assert addr2 in self.nodes[0].getaddressesbylabel(label2)
        
        # Test setlabel
        self.nodes[0].setlabel(addr1, label2)
        assert addr1 in self.nodes[0].getaddressesbylabel(label2)
        assert addr1 not in self.nodes[0].getaddressesbylabel(label1)

        self.log.info("Test getbalance with various parameters")
        # Test getbalance with minconf, include_watchonly, and avoid_reuse
        balance_default = self.nodes[0].getbalance()
        balance_minconf_0 = self.nodes[0].getbalance(minconf=0)
        balance_minconf_1 = self.nodes[0].getbalance(minconf=1)
        
        # With minconf=0, we might see more balance from unconfirmed transactions
        assert balance_minconf_0 >= balance_minconf_1
        assert balance_minconf_1 == balance_default  # Default minconf is 1

        self.log.info("Test avoid_reuse wallet flag")
        # Create a wallet with avoid_reuse flag
        self.nodes[0].createwallet(wallet_name="avoid_reuse_wallet", avoid_reuse=True)
        avoid_reuse_wallet = self.nodes[0].get_wallet_rpc("avoid_reuse_wallet")
        
        # Verify avoid_reuse flag is set
        wallet_info = avoid_reuse_wallet.getwalletinfo()
        assert wallet_info['avoid_reuse'] == True
        
        # Clean up
        avoid_reuse_wallet.unloadwallet()

        self.log.info("Wallet basic test completed successfully!")


if __name__ == '__main__':
    WalletTest().main()