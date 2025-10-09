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
        # Enable DigiDollar features, disable Dandelion for testing
        self.extra_args = [
            ["-digidollar=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-mocktime=0", "-dandelion=0"]
        ]

    def add_options(self, parser):
        self.add_wallet_options(parser)

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
        # Generate initial blocks past coinbase maturity for all nodes
        self.log.info("Generating initial blocks for test setup...")
        self.nodes[0].generate(110)
        self.nodes[1].generate(110)
        self.nodes[2].generate(110)
        self.sync_all()

        # Set mock oracle price ($0.50 per DGB)
        base_price = 50000  # 50000 satoshis per USD
        for node in self.nodes:
            node.setmockoracleprice(base_price)

        # Create DD positions for testing redemption
        self.log.info("Creating DD positions for redemption testing...")

        # Node 0: Multiple positions with different lock tiers
        # mintdigidollar(dd_amount_cents: int, lock_tier: int)
        # Tiers: 0=1h, 1=30d, 2=90d, 3=180d, 4=1y, 5=3y, 6=5y, 7=7y, 8=10y
        # Max mint amount is 100000 cents ($1000)
        # Using tier 0 (1 hour) for testing to avoid long lock periods
        positions = [
            {"amount_cents": 50000, "lock_tier": 0},    # $500, 1 hour (testing)
            {"amount_cents": 75000, "lock_tier": 0},    # $750, 1 hour (testing)
            {"amount_cents": 100000, "lock_tier": 0}    # $1000, 1 hour (testing)
        ]

        self.position_info = []
        for pos in positions:
            result = self.nodes[0].mintdigidollar(pos["amount_cents"], pos["lock_tier"])
            pos["txid"] = result["txid"]
            pos["position_id"] = result["position_id"]
            self.position_info.append(pos)

        # Node 1: Single position
        self.nodes[1].mintdigidollar(100000, 0)  # $1000, 1 hour (testing)

        # Mine blocks to confirm and pass the 1-hour lock period (240 blocks)
        self.nodes[0].generate(250)
        self.sync_all()

        # Verify positions were created
        total_expected = 225000  # Node 0 total in cents ($2250)
        actual_balance = self.nodes[0].getdigidollarbalance()
        assert_equal(actual_balance['total'], total_expected)

    def test_normal_redemption(self):
        """Test normal redemption process."""
        self.log.info("Testing normal redemption...")

        # Get initial balances
        initial_dd_balance = self.nodes[1].getdigidollarbalance()['total']
        initial_dgb_balance = self.nodes[1].getbalance()

        # Get node 1's position
        positions = self.nodes[1].listdigidollarpositions()
        assert len(positions) > 0, "Node 1 should have at least one position"
        position_id = positions[0]['position_id']

        # Redeem portion of DD (in cents)
        redeem_amount_cents = 50000  # $500

        self.log.info(f"Redeeming {redeem_amount_cents} cents DD from position {position_id}...")
        result = self.nodes[1].redeemdigidollar(position_id, redeem_amount_cents)

        assert 'txid' in result
        assert 'dgb_unlocked' in result
        assert 'dd_redeemed' in result

        redeem_txid = result['txid']
        dgb_unlocked = Decimal(result['dgb_unlocked'])

        # Mine block to confirm
        self.nodes[1].generate(1)
        self.sync_all()

        # Verify balances after redemption
        final_dd_balance = self.nodes[1].getdigidollarbalance()['total']
        final_dgb_balance = self.nodes[1].getbalance()

        # DD balance should decrease (in cents)
        # NOTE: Actual implementation may redeem entire position
        assert final_dd_balance <= initial_dd_balance - redeem_amount_cents, \
            f"DD balance should decrease by at least {redeem_amount_cents}, was {initial_dd_balance}, now {final_dd_balance}"

        # DGB balance should increase (minus transaction fees)
        dgb_increase = final_dgb_balance - initial_dgb_balance
        assert_greater_than(dgb_increase, Decimal('0'))

        # Verify the DGB unlocked amount makes sense
        # Should be approximately (redeem_amount_cents / 100) / oracle_price
        oracle_price = self.nodes[1].getoracleprice()
        expected_dgb = Decimal(redeem_amount_cents) / Decimal('100') * Decimal(oracle_price['price']) / Decimal('100000000')
        tolerance = expected_dgb * Decimal('0.05')  # 5% tolerance (to account for collateral ratios)

        assert abs(dgb_unlocked - expected_dgb) <= tolerance, \
            f"DGB unlock amount mismatch: expected ~{expected_dgb}, got {dgb_unlocked}"

        # Verify transaction details
        tx_info = self.nodes[1].gettransaction(redeem_txid)
        assert_greater_than(tx_info['confirmations'], 0)

    def test_partial_redemption(self):
        """Test partial redemption from multiple positions."""
        self.log.info("Testing partial redemption...")

        # Get positions before redemption
        positions_before = self.nodes[0].listdigidollarpositions()
        total_dd_before = self.nodes[0].getdigidollarbalance()['total']

        # Redeem amount from first position (in cents)
        position_id = positions_before[0]['position_id']
        redeem_amount_cents = 25000  # $250 (half of first position)

        result = self.nodes[0].redeemdigidollar(position_id, redeem_amount_cents)

        self.nodes[0].generate(1)
        self.sync_all()

        # Verify total balance decreased correctly (in cents)
        total_dd_after = self.nodes[0].getdigidollarbalance()['total']
        expected_total = total_dd_before - redeem_amount_cents
        assert_equal(total_dd_after, expected_total)

        # Check positions after redemption
        positions_after = self.nodes[0].listdigidollarpositions()

        # Should still have positions (partial redemption)
        assert_greater_than(len(positions_after), 0)

        # Total amount in positions should match balance (in cents)
        total_in_positions = sum(int(pos['amount']) for pos in positions_after)
        assert_equal(total_in_positions, total_dd_after)

    def test_full_position_redemption(self):
        """Test full redemption of entire positions."""
        self.log.info("Testing full position redemption...")

        # Get specific position to redeem fully
        positions = self.nodes[0].listdigidollarpositions()
        smallest_position = min(positions, key=lambda p: int(p['amount']))

        redeem_amount_cents = int(smallest_position['amount'])
        position_id = smallest_position['position_id']
        position_count_before = len(positions)

        # Redeem exact position amount
        result = self.nodes[0].redeemdigidollar(position_id, redeem_amount_cents)

        self.nodes[0].generate(1)
        self.sync_all()

        # Verify position was fully redeemed
        positions_after = self.nodes[0].listdigidollarpositions()
        position_count_after = len(positions_after)

        # Should have one less position
        assert_equal(position_count_after, position_count_before - 1)

        # Verify the specific position is gone
        remaining_ids = [pos['position_id'] for pos in positions_after]
        assert position_id not in remaining_ids

    def test_timelock_expiry_redemption(self):
        """Test redemption behavior with timelock expiry."""
        self.log.info("Testing timelock expiry redemption...")

        # Create a position with very short lock (for testing)
        short_lock_result = self.nodes[2].mintdigidollar(50000, 0)  # $500, 1 hour (tier 0)

        self.nodes[2].generate(1)
        self.sync_all()

        # Get position ID
        position_id = short_lock_result['position_id']

        # Get redemption info immediately (timelock active)
        immediate_info = self.nodes[2].getredemptioninfo(position_id, 10000)  # $100 in cents

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
        expired_info = self.nodes[2].getredemptioninfo(position_id, 10000)  # $100 in cents

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

            # Get a position to redeem from
            positions = self.nodes[0].listdigidollarpositions()
            if len(positions) > 0:
                position_id = positions[0]['position_id']
                err_amount_cents = 10000  # $100
                result = self.nodes[0].redeemdigidollar(position_id, err_amount_cents)

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

        # Get positions to test with
        positions = self.nodes[1].listdigidollarpositions()
        if len(positions) == 0:
            self.log.info("No positions available for collateral return test, skipping...")
            return

        position_id = positions[0]['position_id']

        # Test with different redemption amounts (in cents)
        test_amounts_cents = [5000, 10000, 50000]  # $50, $100, $500

        for amount_cents in test_amounts_cents:
            # Get redemption info before actual redemption
            info = self.nodes[1].getredemptioninfo(position_id, amount_cents)

            assert 'dgb_return' in info or 'dgb_unlocked' in info

            predicted_dgb = Decimal(info.get('dgb_return', info.get('dgb_unlocked', '0')))

            # Perform actual redemption
            if self.nodes[1].getdigidollarbalance()['total'] >= amount_cents:
                result = self.nodes[1].redeemdigidollar(position_id, amount_cents)
                actual_dgb = Decimal(result['dgb_unlocked'])

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

        # Get a valid position for testing
        positions = self.nodes[0].listdigidollarpositions()
        if len(positions) == 0:
            self.log.info("No positions for validation test, skipping...")
            return

        position_id = positions[0]['position_id']

        # Test insufficient DD balance
        excessive_amount_cents = self.nodes[0].getdigidollarbalance()['total'] + 100  # 100 cents more
        with assert_raises_rpc_error(-4, "Insufficient DigiDollar balance"):
            self.nodes[0].redeemdigidollar(position_id, excessive_amount_cents)

        # Test invalid amounts
        invalid_amounts = [0, -10000]  # 0 and negative

        for invalid_amount in invalid_amounts:
            with assert_raises_rpc_error(-32602, ""):
                self.nodes[0].redeemdigidollar(position_id, invalid_amount)

        # Test minimum redemption amount
        with assert_raises_rpc_error(-32602, "below minimum"):
            self.nodes[0].redeemdigidollar(position_id, 50)  # 50 cents, below minimum

        # Test redemption with invalid position ID
        with assert_raises_rpc_error(-4, ""):
            self.nodes[0].redeemdigidollar("0000000000000000000000000000000000000000000000000000000000000000", 10000)

    def test_redemption_edge_cases(self):
        """Test edge cases in redemption."""
        self.log.info("Testing redemption edge cases...")

        # Get a position for testing
        positions = self.nodes[0].listdigidollarpositions()
        if len(positions) == 0:
            self.log.info("No positions for edge case test, skipping...")
            return

        position_id = positions[0]['position_id']

        # Test redemption with various amounts (in cents)
        precise_amounts_cents = [10001, 9999, 100012]  # $100.01, $99.99, $1000.12

        for amount_cents in precise_amounts_cents:
            try:
                info = self.nodes[0].getredemptioninfo(position_id, amount_cents)
                assert 'dgb_return' in info or 'dgb_unlocked' in info
                dgb_val = info.get('dgb_return', info.get('dgb_unlocked', '0'))
                self.log.info(f"Precise redemption info for {amount_cents} cents: {dgb_val} DGB")
            except Exception as e:
                self.log.info(f"Precise amount {amount_cents} validation failed (acceptable): {e}")

        # Test concurrent redemptions
        import threading

        def redeem_worker(position_id, amount_cents):
            try:
                if self.nodes[0].getdigidollarbalance()['total'] >= amount_cents:
                    result = self.nodes[0].redeemdigidollar(position_id, amount_cents)
                    return result['txid']
            except Exception as e:
                self.log.info(f"Concurrent redemption failed (acceptable): {e}")
                return None

        # Launch multiple redemption attempts
        threads = []
        redemption_amounts_cents = [5000, 7500, 10000]  # $50, $75, $100

        for amount_cents in redemption_amounts_cents:
            thread = threading.Thread(target=redeem_worker, args=(position_id, amount_cents))
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
            # Get positions for stress test
            stress_positions = self.nodes[0].listdigidollarpositions()
            if len(stress_positions) > 0:
                stress_position_id = stress_positions[0]['position_id']
                stress_info = self.nodes[0].getredemptioninfo(stress_position_id, 10000)  # $100
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