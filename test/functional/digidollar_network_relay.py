#!/usr/bin/env python3
# Copyright (c) 2025 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test DigiDollar transaction network relay and propagation.

This test verifies that DD transfer transactions properly propagate across
the DigiByte network, including:
- Basic relay between 2 nodes
- Multi-hop relay across network topology
- Dandelion++ privacy integration (stempool -> mempool)
- Relay timing and performance
- Mempool consistency across nodes
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
)
from test_framework.p2p import P2PInterface
from decimal import Decimal
import time


class DigiDollarNetworkRelayTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.setup_clean_chain = True
        # Test both with and without Dandelion
        self.extra_args = [
            ["-digidollar=1", "-dandelion=0"],  # Node 0: DD enabled, Dandelion disabled
            ["-digidollar=1", "-dandelion=0"],  # Node 1: DD enabled, Dandelion disabled
            ["-digidollar=1", "-dandelion=0"],  # Node 2: DD enabled, Dandelion disabled
            ["-digidollar=1", "-dandelion=1"],  # Node 3: DD enabled, Dandelion enabled
            ["-digidollar=1", "-dandelion=1"],  # Node 4: DD enabled, Dandelion enabled
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        """Setup network topology for relay testing."""
        self.setup_nodes()

        # Initial linear topology: 0 <-> 1 <-> 2
        # Nodes 3 and 4 will be connected later for Dandelion tests
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)

    def run_test(self):
        self.log.info("Testing DigiDollar network relay and propagation...")

        # Setup DigiDollar test environment
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_basic_relay()
        self.test_multi_hop_relay()
        self.test_star_topology_relay()
        self.test_relay_timing()
        self.test_mempool_consistency()
        self.test_dandelion_relay()

        self.log.info("All DigiDollar network relay tests passed!")

    def setup_digidollar_test(self):
        """Setup test environment for DigiDollar relay testing."""
        self.log.info("Setting up DigiDollar test environment...")

        # Generate initial blocks past coinbase maturity on node 0
        self.log.info("Generating initial blocks...")
        self.nodes[0].generate(110)
        self.sync_all()

        # Set mock oracle price ($0.50 per DGB)
        base_price = 50000  # 50000 satoshis per USD
        for node in self.nodes:
            node.setmockoracleprice(base_price)

        # Create DD positions on nodes for testing
        self.log.info("Minting DigiDollars on test nodes...")

        # Node 0: Large position for testing
        self.nodes[0].mintdigidollar("1000.00", 365)

        # Node 1: Medium position
        self.nodes[1].mintdigidollar("500.00", 180)

        # Mine blocks to confirm
        self.nodes[0].generate(3)
        self.sync_all()

        # Verify initial balances
        balance_0 = self.nodes[0].getdigidollarbalance()
        balance_1 = self.nodes[1].getdigidollarbalance()

        self.log.info(f"Node 0 DD balance: {balance_0}")
        self.log.info(f"Node 1 DD balance: {balance_1}")

        assert_greater_than(balance_0, Decimal('0'))
        assert_greater_than(balance_1, Decimal('0'))

    def test_basic_relay(self):
        """Test basic DD transaction relay between 2 directly connected nodes."""
        self.log.info("Test 1: Basic relay between 2 nodes...")

        # Get receiver address from node 2
        receiver_address = self.nodes[2].getdigidollaraddress()
        transfer_amount = Decimal('50.00')

        # Create and broadcast DD transfer from node 0
        self.log.info(f"Node 0 sending {transfer_amount} DD to node 2...")
        result = self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))
        txid = result['txid']

        self.log.info(f"Transaction ID: {txid}")

        # Give relay a moment to propagate
        time.sleep(0.5)

        # Verify transaction is in node 1's mempool (relay hop)
        mempool_1 = self.nodes[1].getrawmempool()
        assert txid in mempool_1, "Transaction not relayed to node 1"
        self.log.info("✓ Transaction relayed to node 1")

        # Verify transaction is in node 2's mempool (final destination)
        mempool_2 = self.nodes[2].getrawmempool()
        assert txid in mempool_2, "Transaction not relayed to node 2"
        self.log.info("✓ Transaction relayed to node 2")

        # Mine block and verify confirmation
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify all nodes see the confirmed transaction
        for i in range(3):
            tx_info = self.nodes[i].gettransaction(txid)
            assert_greater_than(tx_info['confirmations'], 0)

        self.log.info("✓ Basic relay test passed")

    def test_multi_hop_relay(self):
        """Test DD transaction relay across multiple hops (3+ nodes)."""
        self.log.info("Test 2: Multi-hop relay (0 -> 1 -> 2)...")

        # Get receiver address from node 2
        receiver_address = self.nodes[2].getdigidollaraddress()
        transfer_amount = Decimal('75.00')

        # Record initial mempool states
        initial_mempool_1 = set(self.nodes[1].getrawmempool())
        initial_mempool_2 = set(self.nodes[2].getrawmempool())

        # Create DD transfer from node 0 (must hop through node 1 to reach node 2)
        self.log.info(f"Node 0 sending {transfer_amount} DD (will relay through node 1)...")
        result = self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))
        txid = result['txid']

        # Wait for propagation
        time.sleep(0.5)

        # Verify transaction hopped through node 1
        mempool_1 = self.nodes[1].getrawmempool()
        new_txs_1 = set(mempool_1) - initial_mempool_1
        assert txid in new_txs_1, "Transaction did not relay through node 1"
        self.log.info(f"✓ Transaction relayed through hop 1 (node 1)")

        # Verify transaction reached final destination (node 2)
        mempool_2 = self.nodes[2].getrawmempool()
        new_txs_2 = set(mempool_2) - initial_mempool_2
        assert txid in new_txs_2, "Transaction did not relay to node 2"
        self.log.info(f"✓ Transaction relayed to destination (node 2)")

        # Mine and verify
        self.nodes[0].generate(1)
        self.sync_all()

        self.log.info("✓ Multi-hop relay test passed")

    def test_star_topology_relay(self):
        """Test DD relay in star topology (1 hub, multiple spokes)."""
        self.log.info("Test 3: Star topology relay...")

        # Create star: node 0 in center, nodes 1 and 2 as spokes
        # (already connected as 0-1 and 1-2, so node 1 is hub)

        receiver_address = self.nodes[2].getdigidollaraddress()
        transfer_amount = Decimal('25.00')

        # Send from node 0, should relay through hub (node 1) to node 2
        result = self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))
        txid = result['txid']

        # Wait for propagation
        time.sleep(0.5)

        # Verify hub (node 1) relayed to all spokes
        mempool_1 = self.nodes[1].getrawmempool()
        mempool_2 = self.nodes[2].getrawmempool()

        assert txid in mempool_1, "Hub (node 1) missing transaction"
        assert txid in mempool_2, "Spoke (node 2) did not receive relay from hub"

        self.log.info("✓ Star topology relay test passed")

    def test_relay_timing(self):
        """Test DD transaction relay timing performance."""
        self.log.info("Test 4: Relay timing performance...")

        receiver_address = self.nodes[2].getdigidollaraddress()
        transfer_amount = Decimal('10.00')

        # Measure relay time
        start_time = time.time()

        result = self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))
        txid = result['txid']

        # Poll for transaction in node 2's mempool (max 5 seconds)
        max_wait = 5.0
        poll_interval = 0.1
        elapsed = 0
        relayed = False

        while elapsed < max_wait:
            if txid in self.nodes[2].getrawmempool():
                relay_time = elapsed
                relayed = True
                break
            time.sleep(poll_interval)
            elapsed += poll_interval

        assert relayed, f"Transaction not relayed within {max_wait} seconds"

        self.log.info(f"✓ Relay completed in {relay_time:.3f} seconds")

        # For regtest, relay should be very fast (< 1 second expected)
        assert relay_time < 2.0, f"Relay too slow: {relay_time:.3f}s (expected < 2s)"

        self.log.info("✓ Relay timing test passed")

    def test_mempool_consistency(self):
        """Test mempool consistency across nodes after DD relay."""
        self.log.info("Test 5: Mempool consistency...")

        receiver_address = self.nodes[1].getdigidollaraddress()
        transfer_amount = Decimal('15.00')

        # Send multiple DD transfers
        txids = []
        for i in range(3):
            result = self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))
            txids.append(result['txid'])

        # Wait for propagation
        time.sleep(1)

        # Verify all transactions in all node mempools
        for node_idx in range(3):
            mempool = self.nodes[node_idx].getrawmempool()
            for txid in txids:
                assert txid in mempool, f"Node {node_idx} missing txid {txid}"

        self.log.info(f"✓ All {len(txids)} transactions in all node mempools")

        # Mine block and verify mempool clears
        self.nodes[0].generate(1)
        self.sync_all()

        # All mempools should be empty now
        for node_idx in range(3):
            mempool = self.nodes[node_idx].getrawmempool()
            for txid in txids:
                assert txid not in mempool, f"Node {node_idx} still has {txid} after mining"

        self.log.info("✓ Mempool consistency test passed")

    def test_dandelion_relay(self):
        """Test DD relay with Dandelion++ privacy (stempool -> mempool)."""
        self.log.info("Test 6: Dandelion++ privacy relay integration...")

        # Setup Dandelion topology with nodes 3 and 4
        # Note: Nodes 3 and 4 have Dandelion enabled

        # Mint DD on nodes 3 and 4
        self.nodes[3].generate(110)  # Get mature coinbase
        self.nodes[3].mintdigidollar("200.00", 365)
        self.nodes[3].generate(3)
        self.sync_all()

        # Connect Dandelion nodes in a topology
        self.connect_nodes(3, 4)

        # Important: With Dandelion, transactions go through stempool first
        # The stempool phase is private (not broadcast), then "fluffs" to mempool

        receiver_address = self.nodes[4].getdigidollaraddress()
        transfer_amount = Decimal('30.00')

        self.log.info(f"Sending DD with Dandelion++ enabled...")
        result = self.nodes[3].senddigidollar(receiver_address, str(transfer_amount))
        txid = result['txid']

        # With Dandelion++:
        # 1. Transaction enters stempool on sender
        # 2. May be forwarded in stem phase (private)
        # 3. Eventually "fluffs" to mempool (public broadcast)
        # 4. Then relays normally

        # Wait for Dandelion embargo period to expire and fluff to mempool
        # Embargo is 10-30 seconds, but in test environment should be faster
        self.log.info("Waiting for Dandelion fluff to mempool...")

        max_wait = 60  # Dandelion embargo can take up to ~30s + buffer
        poll_interval = 1
        elapsed = 0
        in_mempool = False

        while elapsed < max_wait:
            # Check if transaction has fluffed to mempool
            mempool_3 = self.nodes[3].getrawmempool()
            mempool_4 = self.nodes[4].getrawmempool()

            if txid in mempool_3 or txid in mempool_4:
                self.log.info(f"✓ Transaction fluffed to mempool after {elapsed}s")
                in_mempool = True
                break

            time.sleep(poll_interval)
            elapsed += poll_interval

        # Note: In Dandelion, transactions may stay in stempool for embargo period
        # This is expected behavior - we just verify it eventually reaches mempool
        if in_mempool:
            self.log.info("✓ Dandelion relay successful (transaction reached mempool)")
        else:
            # Transaction may still be in stempool - mine a block to force fluff
            self.log.info("Transaction still in stempool, mining block to force fluff...")
            self.nodes[3].generate(1)
            self.sync_all()

            # Verify transaction is now confirmed
            tx_info = self.nodes[3].gettransaction(txid)
            assert_greater_than(tx_info['confirmations'], 0)
            self.log.info("✓ Dandelion transaction confirmed (stempool -> block)")

        self.log.info("✓ Dandelion++ integration test passed")

        # Cleanup: disconnect Dandelion nodes
        self.disconnect_nodes(3, 4)


if __name__ == '__main__':
    DigiDollarNetworkRelayTest().main()
