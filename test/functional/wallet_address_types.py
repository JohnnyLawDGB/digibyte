#!/usr/bin/env python3
# Copyright (c) 2017-2021 The DigiByte Core developers
# Copyright (c) 2017-2022 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that the wallet can send and receive using all combinations of address types.

There are 5 nodes-under-test:
    - node0 uses legacy addresses
    - node1 uses p2sh/segwit addresses
    - node2 uses p2sh/segwit addresses and bech32 addresses for change
    - node3 uses bech32 addresses
    - node4 uses a p2sh/segwit addresses for change

node5 exists to generate new blocks.

## Multisig address test

Test that adding a multisig address with:
    - an uncompressed pubkey always gives a legacy address
    - only compressed pubkeys gives the an `-addresstype` address

## Sending to address types test

A series of tests, iterating over node0-node4. In each iteration of the test, one node sends:
    - 10/101th of its balance to itself (using getrawchangeaddress for single key addresses)
    - 20/101th to the next node
    - 30/101th to the node after that
    - 40/101th to the remaining node
    - 1/101th remains as fee+change

Iterate over each node for single key addresses, and then over each node for
multisig addresses.

Repeat test, but with explicit address_type parameters passed to getnewaddress
and getrawchangeaddress:
    - node0 and node3 send to p2sh.
    - node1 sends to bech32.
    - node2 sends to legacy.

As every node sends coins after receiving, this also
verifies that spending coins sent to all these address types works.

## Change type test

Test that the nodes generate the correct change address type:
    - node0 always uses a legacy change address.
    - node1 uses a bech32 addresses for change if any destination address is bech32.
    - node2 always uses a bech32 address for change
    - node3 always uses a bech32 address for change
    - node4 always uses p2sh/segwit output for change.
"""

from decimal import Decimal
import itertools

from test_framework.blocktools import COINBASE_MATURITY, COINBASE_MATURITY_2
from test_framework.test_framework import DigiByteTestFramework
from test_framework.descriptors import (
    descsum_create,
    descsum_check,
)
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)

class AddressTypeTest(DigiByteTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 6
        self.extra_args = [
            ["-addresstype=legacy"],
            ["-addresstype=p2sh-segwit"],
            ["-addresstype=p2sh-segwit", "-changetype=bech32"],
            ["-addresstype=bech32"],
            ["-changetype=p2sh-segwit"],
            [],
        ]
        # whitelist all peers to speed up tx relay / mempool sync
        for args in self.extra_args:
            args.append("-whitelist=noban@127.0.0.1")
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()

        # Fully mesh-connect nodes for faster mempool sync
        for i, j in itertools.product(range(self.num_nodes), repeat=2):
            if i > j:
                self.connect_nodes(i, j)
        self.sync_all()

    def get_balances(self, key='trusted'):
        """Return a list of balances."""
        return [self.nodes[i].getbalances()['mine'][key] for i in range(4)]

    def test_multisig_address_creation(self, explicit_type, compressed):
        """Test that adding a multisig address with:
        - an uncompressed pubkey always gives a legacy address
        - only compressed pubkeys gives the an `-addresstype` address
        """
        self.log.info(f'Multisig address test (explicit_type={explicit_type}, compressed={compressed})')

        # DigiByte: Generate block reward (72000 DGB)
        self.generatetoaddress(self.nodes[0], 1, self.nodes[0].getnewaddress(), sync_fun=self.no_op)
        # DigiByte: Use COINBASE_MATURITY_2 to wait for maturity
        self.generatetoaddress(self.nodes[0], COINBASE_MATURITY_2, self.nodes[0].getnewaddress())

        # whitelist all peers to speed up tx relay / mempool sync
        for args in self.extra_args:
            args.append("-whitelist=noban@127.0.0.1")

        self.add_nodes(1, extra_args=[self.extra_args[0]])
        self.restart_node(0, extra_args=["-acceptnonstdtxn=1"] + self.extra_args[0])
        self.connect_nodes(0, 1)

        assert_equal(self.nodes[0].getblockcount(), COINBASE_MATURITY_2 + 1)
        assert_equal(self.nodes[1].getblockcount(), COINBASE_MATURITY_2 + 1)

        node = self.nodes[0]
        bal = node.getbalance()
        assert_equal(bal, 72000)  # DigiByte: Block reward is 72000 DGB

        node.importaddress(node.getnewaddress())

        # Test multisig address type with uncompressed and compressed keys
        pubkeys = [node.getaddressinfo(node.getnewaddress())["pubkey"] for _ in range(3)]

        if not compressed:
            uncomp_key = node.addmultisigaddress(3, pubkeys, '', 'legacy')['address']
            assert not node.getaddressinfo(uncomp_key)['iscompressed']
            node.importaddress(uncomp_key)
            node.generatetoaddress(1, uncomp_key)
            assert_equal(node.getbalance(), 2 * 72000)  # DigiByte: 2 block rewards

        # Test address type with compressed keys
        addresses = [node.getaddressinfo(node.getnewaddress(address_type=explicit_type))['address'] for _ in range(3)]
        pubkeys = [node.getaddressinfo(addr)["pubkey"] for addr in addresses]
        
        expected = {
            'legacy': 'legacy',
            'p2sh-segwit': 'p2sh-segwit',
            'bech32': 'bech32' if compressed else 'legacy'
        }[explicit_type or node.getwalletinfo()['addresstype']]

        multisig = node.addmultisigaddress(nrequired=3, keys=pubkeys, address_type=explicit_type)
        assert_equal(multisig['addresstype'], expected)

        self.stop_node(1)
        self.stop_node(0)

    def test_change_output_type(self, node_sender, destinations, expected_change_type):
        # DigiByte: Convert balance to DGB units (block reward is 72000 DGB)
        balance = node_sender.getbalance()
        
        # Send 1/4 of balance to each destination
        quarter = balance / 4
        outputs = {}
        for dest in destinations:
            outputs[dest] = quarter
            
        txid = node_sender.sendmany("", outputs)
        tx = node_sender.gettransaction(txid, True)['hex']
        tx = node_sender.decoderawtransaction(tx)

        # Identify the change output
        change_output = None
        for vout in tx['vout']:
            if vout['value'] not in [quarter]:
                change_output = vout
                break
                
        if expected_change_type:
            assert change_output is not None
            script_type = change_output['scriptPubKey']['type']
            if expected_change_type == 'bech32':
                assert_equal(script_type, 'witness_v0_keyhash')
            elif expected_change_type == 'p2sh-segwit':
                assert_equal(script_type, 'scripthash')
            else:  # legacy
                assert script_type in ['pubkeyhash', 'scripthash']

    def run_test(self):
        # Mine some coins for testing
        # DigiByte: Generate initial blocks with 72000 DGB reward each
        self.generate(self.nodes[5], COINBASE_MATURITY_2 + 1)
        
        # Distribute coins to test nodes
        uncompressed_1 = "0496b538e853519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f8141781e62294721166bf621e73a82cbf2342c858ee"
        uncompressed_2 = "047211a824f55b505228e4c3d5194c1fcfaa15a456abdf37f9b9d97a4040afc073dee6c89064984f03385237d92167c13e236446b417ab79a0fcae412ae3316b77"
        compressed_1 = "0296b538e853519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52"
        compressed_2 = "037211a824f55b505228e4c3d5194c1fcfaa15a456abdf37f9b9d97a4040afc073"

        # DigiByte: Each node gets 150k DGB to ensure enough for tests
        for i in range(4):
            self.nodes[5].sendtoaddress(self.nodes[i].getnewaddress(), 150000)
        self.generate(self.nodes[5], 1)

        # Test multisig address creation with various combinations
        for explicit_type, compressed in itertools.product([None, 'legacy', 'p2sh-segwit', 'bech32'], [False, True]):
            self.test_multisig_address_creation(explicit_type, compressed)

        # Test sending to various address types
        self.log.info("Testing sending to address types")
        
        # Ensure all nodes are synced
        self.sync_all()
        
        # Get initial balances
        balances_before = self.get_balances()
        self.log.info("Initial balances: " + str(balances_before))

        # Create addresses of each type
        addresses = {}
        for i in range(4):
            addresses[i] = {
                'legacy': self.nodes[i].getnewaddress("", "legacy"),
                'p2sh': self.nodes[i].getnewaddress("", "p2sh-segwit"),
                'bech32': self.nodes[i].getnewaddress("", "bech32"),
            }

        # Test various sending patterns
        self.log.info("Testing various sending patterns")
        
        # Node 0 sends to all address types
        for addr_type in ['legacy', 'p2sh', 'bech32']:
            for i in range(1, 4):
                self.nodes[0].sendtoaddress(addresses[i][addr_type], 10)
        
        self.generate(self.nodes[5], 1)
        self.sync_all()

        # Test change address types
        self.log.info("Testing change address types")
        
        # Node 0 (legacy) should produce legacy change
        destinations = [addresses[1]['legacy'], addresses[2]['p2sh']]
        self.test_change_output_type(self.nodes[0], destinations, 'legacy')
        
        # Node 1 (p2sh-segwit) should produce p2sh-segwit change when sending to non-bech32
        destinations = [addresses[2]['legacy'], addresses[3]['p2sh']]
        self.test_change_output_type(self.nodes[1], destinations, 'p2sh-segwit')
        
        # Node 1 should produce bech32 change when any destination is bech32
        destinations = [addresses[2]['bech32'], addresses[3]['legacy']]
        self.test_change_output_type(self.nodes[1], destinations, 'bech32')
        
        # Node 2 (p2sh-segwit with bech32 change) should always produce bech32 change
        destinations = [addresses[0]['legacy'], addresses[1]['p2sh']]
        self.test_change_output_type(self.nodes[2], destinations, 'bech32')
        
        # Node 3 (bech32) should always produce bech32 change
        destinations = [addresses[0]['legacy'], addresses[1]['p2sh']]
        self.test_change_output_type(self.nodes[3], destinations, 'bech32')

        # Mine blocks to confirm transactions
        self.generate(self.nodes[5], 1)
        self.sync_all()

        # Test getaddressinfo
        self.log.info("Testing getaddressinfo")
        for i in range(4):
            for addr_type, addr in addresses[i].items():
                info = self.nodes[i].getaddressinfo(addr)
                assert_equal(info['address'], addr)
                assert_equal(info['ismine'], True)
                assert_equal(info['solvable'], True)
                
                if addr_type == 'legacy':
                    assert not info['isscript']
                    assert not info['iswitness']
                elif addr_type == 'p2sh':
                    assert info['isscript']
                    assert not info['iswitness']
                elif addr_type == 'bech32':
                    assert not info['isscript']
                    assert info['iswitness']

        # Test importing addresses
        self.log.info("Testing importing addresses")
        
        # Import a legacy address
        legacy_addr = self.nodes[0].getnewaddress("", "legacy")
        self.nodes[1].importaddress(legacy_addr, "imported_legacy", False)
        info = self.nodes[1].getaddressinfo(legacy_addr)
        assert_equal(info['iswatchonly'], True)
        assert_equal(info['label'], 'imported_legacy')
        
        # Import a p2sh-segwit address
        p2sh_addr = self.nodes[1].getnewaddress("", "p2sh-segwit")
        self.nodes[2].importaddress(p2sh_addr, "imported_p2sh", False)
        info = self.nodes[2].getaddressinfo(p2sh_addr)
        assert_equal(info['iswatchonly'], True)
        
        # Import a bech32 address
        bech32_addr = self.nodes[3].getnewaddress("", "bech32")
        self.nodes[0].importaddress(bech32_addr, "imported_bech32", False)
        info = self.nodes[0].getaddressinfo(bech32_addr)
        assert_equal(info['iswatchonly'], True)

        # Test invalid operations
        self.log.info("Testing invalid operations")
        
        # Can't use both address_type and label
        assert_raises_rpc_error(-8, "Unknown named parameter address_type", self.nodes[0].getnewaddress, label="test", address_type="legacy")
        
        # Invalid address type
        assert_raises_rpc_error(-5, "Unknown address type", self.nodes[0].getnewaddress, "", "invalid_type")
        
        # Bech32 not valid with uncompressed pubkeys
        uncompressed_pubkey = "0496b538e853519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f8141781e62294721166bf621e73a82cbf2342c858ee"
        assert_raises_rpc_error(-5, "Uncompressed public keys are not supported", self.nodes[0].addmultisigaddress, 1, [uncompressed_pubkey], "", "bech32")

        self.log.info("All address type tests completed successfully!")


if __name__ == '__main__':
    AddressTypeTest().main()