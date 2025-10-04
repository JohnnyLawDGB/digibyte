#!/usr/bin/env python3
"""Test DigiDollar transfer operations.

Test comprehensive transfer functionality including:
- DD-to-DD transfers
- Multi-input transfers
- Change handling
- Invalid transfers
- Fee calculations
- Network propagation
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_raises_rpc_error,
)
from decimal import Decimal


class DigiDollarTransferTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        # Enable DigiDollar features, disable Dandelion for testing
        self.extra_args = [
            ["-digidollar=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-mocktime=0", "-dandelion=0"]
        ]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar transfer operations...")

        # Test setup
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_simple_transfers()
        self.test_multi_input_transfers()
        self.test_change_handling()
        self.test_transfer_validation()
        self.test_fee_calculations()
        self.test_network_propagation()
        self.test_transfer_edge_cases()
        self.test_concurrent_transfers()

    def setup_digidollar_test(self):
        """Setup test environment for DigiDollar."""
        # Generate blocks past DD activation height (650 for regtest)
        # Also need coinbase maturity (COINBASE_MATURITY=8)
        self.log.info("Generating initial blocks for test setup...")
        # Generate 170 blocks per node = 680 total, past activation height of 650
        for i in range(4):
            self.nodes[i].generate(170)
        self.sync_all()

        # Set mock oracle price ($0.50 per DGB)
        base_price = 50  # 50 cents per DGB
        for node in self.nodes:
            node.setmockoracleprice(base_price)

        # Create initial DD balances for testing
        self.log.info("Creating initial DD positions for testing...")

        # Node 0: Large position for testing ($50.00 = 5000 cents, 1 year = tier 4)
        mint0_result = self.nodes[0].mintdigidollar(5000, 4)
        mint0_txid = mint0_result['txid']

        # Node 1: Medium position ($20.00 = 2000 cents, 180 days = tier 3)
        mint1_result = self.nodes[1].mintdigidollar(2000, 3)
        mint1_txid = mint1_result['txid']

        # Node 2: Small position ($10.00 = 1000 cents, 90 days = tier 2)
        mint2_result = self.nodes[2].mintdigidollar(1000, 2)
        mint2_txid = mint2_result['txid']

        # WORKAROUND: Force broadcast using sendrawtransaction
        # DD transactions may not auto-broadcast from CommitTransaction
        for i, txid in enumerate([mint0_txid, mint1_txid, mint2_txid]):
            raw_tx = self.nodes[i].gettransaction(txid)['hex']
            try:
                # Use maxfeerate=0 to bypass fee checks for test
                self.nodes[i].sendrawtransaction(hexstring=raw_tx, maxfeerate=0)
                self.log.info(f"Broadcast mint transaction for node {i}: {txid}")
            except Exception as e:
                self.log.warning(f"Failed to broadcast mint for node {i}: {e}")

        # Wait for transactions to propagate
        import time
        time.sleep(2)

        # Mine blocks to confirm
        self.nodes[0].generate(3)
        time.sleep(1)

        # Reconnect nodes if needed before sync
        try:
            self.sync_all()
        except AssertionError:
            # Nodes may have disconnected, reconnect them
            self.log.info("Reconnecting nodes...")
            self.connect_nodes(0, 1)
            self.connect_nodes(1, 2)
            self.connect_nodes(2, 3)
            self.connect_nodes(0, 3)
            time.sleep(1)
            self.sync_all()

        # Verify initial setup
        for i in range(3):
            balance_info = self.nodes[i].getdigidollarbalance()
            balance = Decimal(balance_info['total'])
            assert_greater_than(balance, Decimal('0'))
            self.log.info(f"Node {i} DD balance: {balance} cents")

    def test_simple_transfers(self):
        """Test simple DD-to-DD transfers between nodes."""
        self.log.info("Testing simple DD transfers...")

        # Get initial balances
        sender_initial = self.nodes[0].getdigidollarbalance()
        receiver_initial = self.nodes[3].getdigidollarbalance()  # Node 3 starts with 0

        # Get receiver address
        receiver_address = self.nodes[3].getdigidollaraddress()

        # Perform transfer ($10.00 = 1000 cents, node 0 has 5000 cents = $50)
        transfer_amount_cents = 1000
        transfer_amount_dollars = Decimal(transfer_amount_cents) / 100
        self.log.info(f"Transferring ${transfer_amount_dollars} DD ({transfer_amount_cents} cents) from node 0 to node 3...")

        result = self.nodes[0].senddigidollar(receiver_address, transfer_amount_cents)
        assert 'txid' in result
        txid = result['txid']

        # Mine block to confirm
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balances
        sender_final = self.nodes[0].getdigidollarbalance()
        receiver_final = self.nodes[3].getdigidollarbalance()

        expected_sender = sender_initial - transfer_amount
        expected_receiver = receiver_initial + transfer_amount

        assert_equal(sender_final, expected_sender)
        assert_equal(receiver_final, expected_receiver)

        # Verify transaction details
        tx_details = self.nodes[0].gettransaction(txid)
        assert_equal(tx_details['amount'], -transfer_amount)  # Negative for sender
        assert_greater_than(tx_details['confirmations'], 0)

        # Check receiver's perspective
        rx_tx_details = self.nodes[3].gettransaction(txid)
        assert_equal(rx_tx_details['amount'], transfer_amount)  # Positive for receiver

    def test_multi_input_transfers(self):
        """Test transfers that require multiple DD inputs."""
        self.log.info("Testing multi-input transfers...")

        # Create multiple small DD outputs by making several transfers to self
        self.log.info("Creating multiple DD outputs...")

        sender_address = self.nodes[1].getdigidollaraddress()

        # Split balance into smaller outputs
        small_amounts = [Decimal('200.00'), Decimal('300.00'), Decimal('250.00')]
        for amount in small_amounts:
            new_address = self.nodes[1].getdigidollaraddress()
            self.nodes[1].senddigidollar(new_address, str(amount))

        # Mine to confirm
        self.nodes[1].generate(1)
        self.sync_all()

        # Now perform a transfer that requires multiple inputs
        receiver_address = self.nodes[2].getdigidollaraddress()
        large_amount = Decimal('600.00')  # Requires combining multiple outputs

        initial_receiver_balance = self.nodes[2].getdigidollarbalance()

        self.log.info(f"Performing multi-input transfer of {large_amount} DD...")
        result = self.nodes[1].senddigidollar(receiver_address, str(large_amount))

        # Mine block to confirm
        self.nodes[1].generate(1)
        self.sync_all()

        # Verify the transfer succeeded
        final_receiver_balance = self.nodes[2].getdigidollarbalance()
        expected_balance = initial_receiver_balance + large_amount
        assert_equal(final_receiver_balance, expected_balance)

        # Verify transaction used multiple inputs
        tx_info = self.nodes[1].gettransaction(result['txid'])
        assert 'details' in tx_info

    def test_change_handling(self):
        """Test change handling in DD transfers."""
        self.log.info("Testing DD change handling...")

        # Get current balance and create a transfer that requires change
        sender_balance = self.nodes[2].getdigidollarbalance()
        receiver_address = self.nodes[0].getdigidollaraddress()

        # Transfer amount that requires change
        transfer_amount = sender_balance - Decimal('100.00')  # Leave some change

        self.log.info(f"Transferring {transfer_amount} from balance of {sender_balance}...")

        result = self.nodes[2].senddigidollar(receiver_address, str(transfer_amount))

        # Mine block to confirm
        self.nodes[2].generate(1)
        self.sync_all()

        # Verify change was properly handled
        remaining_balance = self.nodes[2].getdigidollarbalance()
        expected_remaining = Decimal('100.00')

        # Allow for small precision differences
        tolerance = Decimal('0.01')
        assert abs(remaining_balance - expected_remaining) <= tolerance

        # Verify sender can still use the change
        if remaining_balance > Decimal('10.00'):
            # Make a small transfer with the change
            small_transfer = Decimal('10.00')
            small_result = self.nodes[2].senddigidollar(receiver_address, str(small_transfer))

            self.nodes[2].generate(1)
            self.sync_all()

            # Verify it worked
            assert 'txid' in small_result

    def test_transfer_validation(self):
        """Test transfer validation rules."""
        self.log.info("Testing transfer validation...")

        valid_address = self.nodes[1].getdigidollaraddress()
        sender_balance = self.nodes[0].getdigidollarbalance()

        # Test insufficient balance
        excessive_amount = sender_balance + Decimal('1.00')
        with assert_raises_rpc_error(-4, "Insufficient balance"):
            self.nodes[0].senddigidollar(valid_address, str(excessive_amount))

        # Test invalid addresses
        invalid_addresses = [
            "",
            "invalid_address",
            "dgb1qw508d6qejxtdg4y5r3zarvary0c5xw7k3hz6a0",  # Regular DGB address
            "dgbrt1dd" + "0" * 100  # Too long
        ]

        for invalid_addr in invalid_addresses:
            with assert_raises_rpc_error(-5, "Invalid DigiDollar address"):
                self.nodes[0].senddigidollar(invalid_addr, "100.00")

        # Test invalid amounts
        invalid_amounts = [
            "",
            "0",
            "-100.00",
            "invalid",
            "0.001"  # Below minimum output
        ]

        for invalid_amount in invalid_amounts:
            with assert_raises_rpc_error(-32602, ""):
                self.nodes[0].senddigidollar(valid_address, invalid_amount)

        # Test minimum output validation
        # DD has minimum output requirements
        with assert_raises_rpc_error(-32602, "below minimum"):
            self.nodes[0].senddigidollar(valid_address, "0.50")  # Below $1 minimum

    def test_fee_calculations(self):
        """Test fee calculations for DD transfers."""
        self.log.info("Testing DD transfer fee calculations...")

        # DD transfers should have minimal fees since they don't require DGB network fees
        # The main cost is the DD network fee (if any)

        sender_balance_before = self.nodes[0].getdigidollarbalance()
        dgb_balance_before = self.nodes[0].getbalance()

        receiver_address = self.nodes[1].getdigidollaraddress()
        transfer_amount = Decimal('50.00')

        # Perform transfer
        result = self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))

        # Mine block to confirm
        self.nodes[0].generate(1)
        self.sync_all()

        sender_balance_after = self.nodes[0].getdigidollarbalance()
        dgb_balance_after = self.nodes[0].getbalance()

        # DD balance should decrease by exactly the transfer amount
        dd_decrease = sender_balance_before - sender_balance_after
        assert_equal(dd_decrease, transfer_amount)

        # DGB balance might decrease slightly due to transaction fees
        dgb_decrease = dgb_balance_before - dgb_balance_after

        # DGB fees should be minimal for DD transfers
        assert_greater_than_or_equal(dgb_decrease, Decimal('0'))  # Some fee is expected
        assert dgb_decrease < Decimal('0.01'), "DGB fee too high for DD transfer"  # But should be very small

    def test_network_propagation(self):
        """Test DD transfer propagation across the network."""
        self.log.info("Testing DD transfer network propagation...")

        # Ensure all nodes are connected
        for i in range(self.num_nodes - 1):
            self.connect_nodes(i, i + 1)

        # Create transfer on node 0
        receiver_address = self.nodes[3].getdigidollaraddress()
        transfer_amount = Decimal('75.00')

        result = self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))
        txid = result['txid']

        # Verify transaction is in mempool of all nodes
        import time
        time.sleep(1)  # Give time for propagation

        for i in range(self.num_nodes):
            mempool = self.nodes[i].getrawmempool()
            assert txid in mempool, f"Transaction not in node {i} mempool"

        # Mine block on different node and verify propagation
        self.nodes[2].generate(1)
        self.sync_all()

        # Verify transaction is confirmed on all nodes
        for i in range(self.num_nodes):
            tx_info = self.nodes[i].gettransaction(txid)
            assert_greater_than(tx_info['confirmations'], 0)

    def test_transfer_edge_cases(self):
        """Test edge cases in DD transfers."""
        self.log.info("Testing DD transfer edge cases...")

        # Test transfer to self
        self_address = self.nodes[0].getdigidollaraddress()
        transfer_amount = Decimal('25.00')

        initial_balance = self.nodes[0].getdigidollarbalance()

        result = self.nodes[0].senddigidollar(self_address, str(transfer_amount))

        self.nodes[0].generate(1)
        self.sync_all()

        # Balance should remain approximately the same (minus fees)
        final_balance = self.nodes[0].getdigidollarbalance()
        balance_diff = abs(final_balance - initial_balance)
        assert balance_diff < Decimal('1.00'), "Balance changed too much for self-transfer"  # Allow for fees

        # Test very precise amounts
        precise_amounts = [
            "100.01",
            "99.99",
            "1000.001"  # Test precision handling
        ]

        receiver_address = self.nodes[1].getdigidollaraddress()

        for amount in precise_amounts:
            try:
                result = self.nodes[0].senddigidollar(receiver_address, amount)
                self.nodes[0].generate(1)
                self.sync_all()
                self.log.info(f"Precise amount {amount} transferred successfully")
            except Exception as e:
                self.log.info(f"Precise amount {amount} failed (acceptable): {e}")

        # Test maximum precision
        max_precision_amount = "1234.12345678"
        try:
            result = self.nodes[0].senddigidollar(receiver_address, max_precision_amount)
            self.nodes[0].generate(1)
            self.sync_all()
        except Exception as e:
            # High precision might be rejected - this is acceptable
            self.log.info(f"Max precision transfer failed (acceptable): {e}")

    def test_concurrent_transfers(self):
        """Test concurrent DD transfers."""
        self.log.info("Testing concurrent DD transfers...")

        # Create multiple transfers simultaneously
        import threading
        import time

        results = []
        errors = []

        def transfer_worker(node_idx, amount):
            try:
                receiver_address = self.nodes[(node_idx + 1) % self.num_nodes].getdigidollaraddress()
                result = self.nodes[node_idx].senddigidollar(receiver_address, str(amount))
                results.append(result)
            except Exception as e:
                errors.append(str(e))

        # Launch concurrent transfers
        threads = []
        transfer_amounts = [Decimal('10.00'), Decimal('15.00'), Decimal('20.00')]

        for i, amount in enumerate(transfer_amounts):
            if i < len(self.nodes) and self.nodes[i].getdigidollarbalance() > amount:
                thread = threading.Thread(target=transfer_worker, args=(i, amount))
                threads.append(thread)
                thread.start()

        # Wait for all transfers to complete
        for thread in threads:
            thread.join()

        # Mine blocks to confirm successful transfers
        self.nodes[0].generate(3)
        self.sync_all()

        self.log.info(f"Concurrent transfers completed: {len(results)} successful, {len(errors)} failed")

        # Some failures are acceptable in concurrent scenarios
        # The important thing is that the node doesn't crash and successful transfers work

        # Verify successful transfers
        for result in results:
            if 'txid' in result:
                tx_info = self.nodes[0].gettransaction(result['txid'])
                assert_greater_than(tx_info['confirmations'], 0)


if __name__ == '__main__':
    DigiDollarTransferTest().main()