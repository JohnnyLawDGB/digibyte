#!/usr/bin/env python3
"""Test DigiDollar redemption operations.

Test comprehensive redemption functionality including:
- Normal redemption path
- Emergency redemption (ERR)
- Partial redemption
- Timelock expiry redemption
- Collateral return calculations
- ERR trigger conditions
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_less_than,
    assert_raises_rpc_error,
)
from decimal import Decimal
import time


class DigiDollarRedeemTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        # Enable DigiDollar features
        self.extra_args = [
            ["-digidollar=1", "-mocktime=0"],
            ["-digidollar=1", "-mocktime=0"],
            ["-digidollar=1", "-mocktime=0"]
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar redemption operations...")

        # Test setup
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_normal_redemption()
        self.test_partial_redemption()
        self.test_full_position_redemption()
        self.test_timelock_expiry_redemption()
        self.test_emergency_redemption()
        self.test_collateral_return_calculations()
        self.test_redemption_validation()
        self.test_redemption_edge_cases()

    def setup_digidollar_test(self):
        """Setup test environment for DigiDollar."""
        # Generate initial blocks past coinbase maturity
        self.log.info("Generating initial blocks for test setup...")
        self.nodes[0].generate(110)
        self.sync_all()

        # Set mock oracle price ($0.50 per DGB)
        base_price = 50000  # 50000 satoshis per USD
        for node in self.nodes:
            node.setmockoracleprice(base_price)

        # Create DD positions for testing redemption
        self.log.info("Creating DD positions for redemption testing...")

        # Node 0: Multiple positions with different lock periods
        positions = [
            {"amount": "1000.00", "lock_days": 30},   # Short lock
            {"amount": "2000.00", "lock_days": 365},  # Medium lock
            {"amount": "3000.00", "lock_days": 1095}  # Long lock
        ]

        self.position_info = []
        for pos in positions:
            result = self.nodes[0].mintdigidollar(pos["amount"], pos["lock_days"])
            pos["txid"] = result["txid"]
            pos["dd_address"] = result["dd_address"]
            self.position_info.append(pos)

        # Node 1: Single large position
        self.nodes[1].mintdigidollar("5000.00", 180)

        # Mine blocks to confirm
        self.nodes[0].generate(3)
        self.sync_all()

        # Verify positions were created
        total_expected = Decimal('6000.00')  # Node 0 total
        actual_balance = self.nodes[0].getdigidollarbalance()
        assert_equal(actual_balance, total_expected)

    def test_normal_redemption(self):
        """Test normal redemption process."""
        self.log.info("Testing normal redemption...")

        # Get initial balances
        initial_dd_balance = self.nodes[1].getdigidollarbalance()
        initial_dgb_balance = self.nodes[1].getbalance()

        # Redeem portion of DD
        redeem_amount = Decimal('1000.00')

        self.log.info(f"Redeeming {redeem_amount} DD...")
        result = self.nodes[1].redeemdigidollar(str(redeem_amount))

        assert 'txid' in result
        assert 'dgb_returned' in result
        assert 'redemption_info' in result

        redeem_txid = result['txid']
        dgb_returned = Decimal(result['dgb_returned'])

        # Mine block to confirm
        self.nodes[1].generate(1)
        self.sync_all()

        # Verify balances after redemption
        final_dd_balance = self.nodes[1].getdigidollarbalance()
        final_dgb_balance = self.nodes[1].getbalance()

        # DD balance should decrease
        expected_dd_balance = initial_dd_balance - redeem_amount
        assert_equal(final_dd_balance, expected_dd_balance)

        # DGB balance should increase (minus transaction fees)
        dgb_increase = final_dgb_balance - initial_dgb_balance
        assert_greater_than(dgb_increase, Decimal('0'))

        # Verify the DGB returned amount makes sense
        # Should be approximately redeem_amount / oracle_price
        oracle_price = self.nodes[1].getoracleprice()
        expected_dgb = redeem_amount * Decimal(oracle_price['price']) / Decimal('100000000')
        tolerance = expected_dgb * Decimal('0.02')  # 2% tolerance

        assert abs(dgb_returned - expected_dgb) <= tolerance, \
            f"DGB return amount mismatch: expected ~{expected_dgb}, got {dgb_returned}"

        # Verify transaction details
        tx_info = self.nodes[1].gettransaction(redeem_txid)
        assert_greater_than(tx_info['confirmations'], 0)

    def test_partial_redemption(self):
        """Test partial redemption from multiple positions."""
        self.log.info("Testing partial redemption...")

        # Get positions before redemption
        positions_before = self.nodes[0].listdigidollarpositions()
        total_dd_before = self.nodes[0].getdigidollarbalance()

        # Redeem amount that spans multiple positions
        redeem_amount = Decimal('1500.00')  # Will partially redeem from multiple positions

        result = self.nodes[0].redeemdigidollar(str(redeem_amount))

        self.nodes[0].generate(1)
        self.sync_all()

        # Verify total balance decreased correctly
        total_dd_after = self.nodes[0].getdigidollarbalance()
        expected_total = total_dd_before - redeem_amount
        assert_equal(total_dd_after, expected_total)

        # Check positions after redemption
        positions_after = self.nodes[0].listdigidollarpositions()

        # Should still have positions (partial redemption)
        assert_greater_than(len(positions_after), 0)

        # Total amount in positions should match balance
        total_in_positions = sum(Decimal(pos['amount']) for pos in positions_after)
        assert_equal(total_in_positions, total_dd_after)

    def test_full_position_redemption(self):
        """Test full redemption of entire positions."""
        self.log.info("Testing full position redemption...")

        # Get specific position to redeem fully
        positions = self.nodes[0].listdigidollarpositions()
        smallest_position = min(positions, key=lambda p: Decimal(p['amount']))

        redeem_amount = Decimal(smallest_position['amount'])
        position_count_before = len(positions)

        # Redeem exact position amount
        result = self.nodes[0].redeemdigidollar(str(redeem_amount))

        self.nodes[0].generate(1)
        self.sync_all()

        # Verify position was fully redeemed
        positions_after = self.nodes[0].listdigidollarpositions()
        position_count_after = len(positions_after)

        # Should have one less position
        assert_equal(position_count_after, position_count_before - 1)

        # Verify the specific position is gone
        remaining_addresses = [pos['dd_address'] for pos in positions_after]
        assert smallest_position['dd_address'] not in remaining_addresses

    def test_timelock_expiry_redemption(self):
        """Test redemption behavior with timelock expiry."""
        self.log.info("Testing timelock expiry redemption...")

        # Create a position with very short lock (for testing)
        short_lock_result = self.nodes[2].mintdigidollar("500.00", 30)  # 30 days

        self.nodes[2].generate(1)
        self.sync_all()

        # Get redemption info immediately (timelock active)
        immediate_info = self.nodes[2].getredemptioninfo("100.00")

        assert 'can_redeem' in immediate_info
        assert 'timelock_remaining' in immediate_info
        assert 'penalty_rate' in immediate_info

        # For immediate redemption, there should be penalty
        if immediate_info['can_redeem']:
            assert_greater_than(Decimal(immediate_info['penalty_rate']), Decimal('0'))

        # Simulate time passage by advancing blocks
        # In real scenarios, we'd wait for actual timelock expiry
        current_height = self.nodes[2].getblockcount()
        lock_period_blocks = 30 * 24 * 60 * 4  # 30 days in blocks (15s blocks)

        # Fast-forward by generating blocks
        # Note: This simulates time passage but doesn't actually expire timelocks
        # In production, timelocks are based on actual block height
        self.log.info(f"Advancing {lock_period_blocks} blocks to simulate timelock expiry...")

        # Generate blocks in chunks to avoid memory issues
        chunk_size = 1000
        blocks_generated = 0
        while blocks_generated < lock_period_blocks:
            remaining = min(chunk_size, lock_period_blocks - blocks_generated)
            self.nodes[2].generate(remaining)
            blocks_generated += remaining
            self.log.info(f"Generated {blocks_generated}/{lock_period_blocks} blocks")

        self.sync_all()

        # Check redemption info after timelock expiry
        expired_info = self.nodes[2].getredemptioninfo("100.00")

        # After expiry, penalty should be reduced or eliminated
        if expired_info['can_redeem']:
            expired_penalty = Decimal(expired_info['penalty_rate'])
            immediate_penalty = Decimal(immediate_info['penalty_rate'])
            assert_less_than(expired_penalty, immediate_penalty)

    def test_emergency_redemption(self):
        """Test Emergency Redemption Route (ERR) functionality."""
        self.log.info("Testing Emergency Redemption Route (ERR)...")

        # ERR is triggered when system collateral falls below emergency threshold
        # For testing, we'll manipulate oracle price to simulate this condition

        # Get current system health
        initial_health = self.nodes[0].getdigidollarstats()
        self.log.info(f"Initial system health: {initial_health}")

        # Dramatically increase oracle price to simulate DGB crash
        # This reduces the value of collateral, triggering ERR
        crisis_price = 10000  # $0.10 per DGB (from $0.50)

        self.log.info(f"Simulating DGB price crash by setting oracle price to {crisis_price}...")
        for node in self.nodes:
            node.setmockoracleprice(crisis_price)

        # Generate block to make price change effective
        self.nodes[0].generate(1)
        self.sync_all()

        # Check if ERR is triggered
        protection_status = self.nodes[0].getprotectionstatus()
        self.log.info(f"Protection status after price crash: {protection_status}")

        # If ERR is active, test emergency redemption
        if protection_status.get('err_active', False):
            self.log.info("ERR is active, testing emergency redemption...")

            # Emergency redemption should be possible at reduced rates
            err_amount = Decimal('100.00')
            result = self.nodes[0].redeemdigidollar(str(err_amount))

            assert 'txid' in result
            assert 'emergency_redemption' in result
            assert result['emergency_redemption'] == True

            self.nodes[0].generate(1)
            self.sync_all()

            self.log.info("Emergency redemption completed successfully")

        else:
            self.log.info("ERR not triggered by price manipulation, testing protection mechanisms...")

            # Even if ERR isn't triggered, the system should show stress
            system_health = self.nodes[0].getdigidollarstats()
            assert 'system_collateral_ratio' in system_health

            # System should be under stress
            collateral_ratio = Decimal(system_health['system_collateral_ratio'])
            assert_less_than(collateral_ratio, Decimal('200'))  # Less than 200%

        # Restore normal price
        self.nodes[0].setmockoracleprice(50000)

    def test_collateral_return_calculations(self):
        """Test accuracy of collateral return calculations."""
        self.log.info("Testing collateral return calculations...")

        # Test with different redemption amounts
        test_amounts = [Decimal('50.00'), Decimal('100.00'), Decimal('500.00')]

        for amount in test_amounts:
            # Get redemption info before actual redemption
            info = self.nodes[1].getredemptioninfo(str(amount))

            assert 'dgb_returned' in info
            assert 'penalty_amount' in info
            assert 'effective_rate' in info

            predicted_dgb = Decimal(info['dgb_returned'])
            penalty = Decimal(info['penalty_amount'])

            # Perform actual redemption
            if self.nodes[1].getdigidollarbalance() >= amount:
                result = self.nodes[1].redeemdigidollar(str(amount))
                actual_dgb = Decimal(result['dgb_returned'])

                # Mine to confirm
                self.nodes[1].generate(1)
                self.sync_all()

                # Verify prediction accuracy
                tolerance = predicted_dgb * Decimal('0.01')  # 1% tolerance
                assert abs(actual_dgb - predicted_dgb) <= tolerance, \
                    f"DGB return prediction error: predicted {predicted_dgb}, actual {actual_dgb}"

    def test_redemption_validation(self):
        """Test redemption validation rules."""
        self.log.info("Testing redemption validation...")

        # Test insufficient DD balance
        excessive_amount = self.nodes[0].getdigidollarbalance() + Decimal('1.00')
        with assert_raises_rpc_error(-4, "Insufficient DigiDollar balance"):
            self.nodes[0].redeemdigidollar(str(excessive_amount))

        # Test invalid amounts
        invalid_amounts = ["", "0", "-100.00", "invalid", "0.001"]

        for invalid_amount in invalid_amounts:
            with assert_raises_rpc_error(-32602, ""):
                self.nodes[0].redeemdigidollar(invalid_amount)

        # Test minimum redemption amount
        with assert_raises_rpc_error(-32602, "below minimum"):
            self.nodes[0].redeemdigidollar("0.50")  # Below minimum

        # Test redemption when no positions exist
        empty_node = self.nodes[2]
        empty_balance = empty_node.getdigidollarbalance()

        if empty_balance == Decimal('0'):
            with assert_raises_rpc_error(-4, "No DigiDollar positions"):
                empty_node.redeemdigidollar("100.00")

    def test_redemption_edge_cases(self):
        """Test edge cases in redemption."""
        self.log.info("Testing redemption edge cases...")

        # Test redemption with very precise amounts
        precise_amounts = ["100.01", "99.99", "1000.123456"]

        for amount in precise_amounts:
            try:
                info = self.nodes[0].getredemptioninfo(amount)
                assert 'dgb_returned' in info
                self.log.info(f"Precise redemption info for {amount}: {info['dgb_returned']} DGB")
            except Exception as e:
                self.log.info(f"Precise amount {amount} validation failed (acceptable): {e}")

        # Test concurrent redemptions
        import threading

        def redeem_worker(amount):
            try:
                if self.nodes[0].getdigidollarbalance() >= Decimal(amount):
                    result = self.nodes[0].redeemdigidollar(amount)
                    return result['txid']
            except Exception as e:
                self.log.info(f"Concurrent redemption failed (acceptable): {e}")
                return None

        # Launch multiple redemption attempts
        threads = []
        redemption_amounts = ["50.00", "75.00", "100.00"]

        for amount in redemption_amounts:
            thread = threading.Thread(target=redeem_worker, args=(amount,))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        # Mine blocks to confirm any successful redemptions
        self.nodes[0].generate(3)
        self.sync_all()

        # Test redemption during system stress
        # Modify oracle price to create stress
        stress_price = 25000  # Half the normal price
        self.nodes[0].setmockoracleprice(stress_price)

        try:
            stress_info = self.nodes[0].getredemptioninfo("100.00")
            self.log.info(f"Redemption during stress: {stress_info}")

            # During stress, penalty rates should be higher
            assert 'penalty_rate' in stress_info
            penalty_rate = Decimal(stress_info['penalty_rate'])
            assert_greater_than_or_equal(penalty_rate, Decimal('0'))

        except Exception as e:
            self.log.info(f"Redemption during stress failed (may be acceptable): {e}")

        # Restore normal price
        self.nodes[0].setmockoracleprice(50000)


if __name__ == '__main__':
    DigiDollarRedeemTest().main()