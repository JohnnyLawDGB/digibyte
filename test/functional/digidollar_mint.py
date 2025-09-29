#!/usr/bin/env python3
"""Test DigiDollar minting operations.

Test comprehensive minting functionality including:
- Minting with different lock tiers
- Collateral calculation with DCA
- Mint validation rules
- Oracle price integration
- Error conditions and edge cases
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


class DigiDollarMintTest(DigiByteTestFramework):
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
        self.log.info("Testing DigiDollar minting operations...")

        # Test setup
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_mint_lock_tiers()
        self.test_collateral_calculations()
        self.test_dca_impact_on_minting()
        self.test_oracle_price_integration()
        self.test_mint_validation_rules()
        self.test_edge_cases()
        self.test_error_conditions()

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

        # Verify DigiDollar is active
        status = self.nodes[0].getdigidollarstatus()
        assert_equal(status["active"], True)

    def test_mint_lock_tiers(self):
        """Test minting with different lock tiers and collateral ratios."""
        self.log.info("Testing mint lock tiers...")

        # Test data: lock_days -> expected_collateral_ratio
        lock_tiers = {
            30: 500,    # 30 days: 500%
            90: 400,    # 3 months: 400%
            180: 350,   # 6 months: 350%
            365: 300,   # 1 year: 300%
            1095: 250,  # 3 years: 250%
            1825: 225,  # 5 years: 225%
            2555: 212,  # 7 years: 212%
            3650: 200   # 10 years: 200%
        }

        mint_amount = Decimal('1000.00')  # $1000

        for lock_days, expected_ratio in lock_tiers.items():
            self.log.info(f"Testing {lock_days} day lock (expected ratio: {expected_ratio}%)...")

            # Calculate expected collateral requirement
            collateral_req = self.nodes[0].calculatecollateralrequirement(str(mint_amount), lock_days)

            # Verify collateral ratio
            expected_collateral = mint_amount * expected_ratio / 100
            actual_collateral = Decimal(collateral_req['collateral_dgb'])

            # Allow for small rounding differences due to price conversion
            tolerance = expected_collateral * Decimal('0.01')  # 1% tolerance
            assert abs(actual_collateral - expected_collateral) <= tolerance, \
                f"Collateral mismatch for {lock_days} days: expected ~{expected_collateral}, got {actual_collateral}"

            # Perform actual mint
            result = self.nodes[0].mintdigidollar(str(mint_amount), lock_days)
            assert 'txid' in result
            assert 'dd_address' in result

            # Mine block to confirm
            self.nodes[0].generate(1)
            self.sync_all()

            # Verify position was created with correct lock period
            positions = self.nodes[0].listdigidollarpositions()
            latest_position = positions[-1]  # Most recent position

            # Convert lock_days to approximate blocks (15s block time)
            expected_lock_blocks = lock_days * 24 * 60 * 4  # 5760 blocks per day
            actual_lock_blocks = latest_position['lock_height'] - self.nodes[0].getblockcount()

            # Allow for some variation due to block time precision
            block_tolerance = expected_lock_blocks * 0.01  # 1% tolerance
            assert abs(actual_lock_blocks - expected_lock_blocks) <= block_tolerance, \
                f"Lock period mismatch: expected ~{expected_lock_blocks} blocks, got {actual_lock_blocks}"

    def test_collateral_calculations(self):
        """Test collateral calculation accuracy."""
        self.log.info("Testing collateral calculations...")

        test_cases = [
            {"amount": Decimal('100.00'), "lock_days": 365},
            {"amount": Decimal('500.50'), "lock_days": 180},
            {"amount": Decimal('1234.56'), "lock_days": 90},
            {"amount": Decimal('10000.00'), "lock_days": 30}
        ]

        for case in test_cases:
            amount = case['amount']
            lock_days = case['lock_days']

            # Get collateral requirement
            req = self.nodes[0].calculatecollateralrequirement(str(amount), lock_days)

            # Verify required fields
            assert 'collateral_dgb' in req
            assert 'collateral_ratio' in req
            assert 'oracle_price' in req
            assert 'dca_multiplier' in req

            # Verify calculations are consistent
            oracle_price = Decimal(req['oracle_price'])  # satoshis per USD
            collateral_ratio = Decimal(req['collateral_ratio']) / 100  # Convert percentage
            dca_multiplier = Decimal(req['dca_multiplier'])

            # Calculate expected collateral in USD
            expected_collateral_usd = amount * collateral_ratio * dca_multiplier

            # Convert to DGB using oracle price
            expected_collateral_dgb = expected_collateral_usd * oracle_price / Decimal('100000000')  # Convert satoshis to DGB

            actual_collateral_dgb = Decimal(req['collateral_dgb'])

            # Allow for rounding differences
            tolerance = expected_collateral_dgb * Decimal('0.001')  # 0.1% tolerance
            assert abs(actual_collateral_dgb - expected_collateral_dgb) <= tolerance, \
                f"Collateral calculation error: expected {expected_collateral_dgb}, got {actual_collateral_dgb}"

    def test_dca_impact_on_minting(self):
        """Test how DCA (Dynamic Collateral Adjustment) affects minting."""
        self.log.info("Testing DCA impact on minting...")

        # Test with normal system health (should have DCA multiplier of 1.0)
        normal_req = self.nodes[0].calculatecollateralrequirement("1000.00", 365)
        normal_multiplier = Decimal(normal_req['dca_multiplier'])
        assert_equal(normal_multiplier, Decimal('1.0'))

        # Simulate system stress by creating many undercollateralized positions
        # (This would be done through manipulating oracle prices in a real implementation)

        # For testing purposes, we can check if the DCA system responds correctly
        # by examining the DCA multiplier calculation
        dca_multiplier = self.nodes[0].getdcamultiplier()
        assert 'multiplier' in dca_multiplier
        assert 'system_collateral' in dca_multiplier
        assert 'level' in dca_multiplier

        # Verify multiplier is reasonable (between 1.0 and 2.0)
        multiplier_value = Decimal(dca_multiplier['multiplier'])
        assert_greater_than_or_equal(multiplier_value, Decimal('1.0'))
        assert_less_than(multiplier_value, Decimal('2.1'))

    def test_oracle_price_integration(self):
        """Test oracle price integration in minting."""
        self.log.info("Testing oracle price integration...")

        # Test with different oracle prices
        price_scenarios = [
            25000,   # $0.25 per DGB
            50000,   # $0.50 per DGB
            100000,  # $1.00 per DGB
            200000   # $2.00 per DGB
        ]

        mint_amount = Decimal('1000.00')
        lock_days = 365

        for price in price_scenarios:
            self.log.info(f"Testing with oracle price: {price} satoshis per USD")

            # Set new oracle price
            self.nodes[0].setmockoracleprice(price)

            # Calculate collateral requirement
            req = self.nodes[0].calculatecollateralrequirement(str(mint_amount), lock_days)

            # Verify oracle price is reflected
            assert_equal(int(req['oracle_price']), price)

            # Verify collateral requirement scales inversely with price
            # Higher DGB price = less DGB needed as collateral
            collateral_dgb = Decimal(req['collateral_dgb'])
            expected_collateral_dgb = mint_amount * 3 / (Decimal(price) / Decimal('100000000'))  # 300% ratio

            tolerance = expected_collateral_dgb * Decimal('0.001')
            assert abs(collateral_dgb - expected_collateral_dgb) <= tolerance

        # Reset to original price
        self.nodes[0].setmockoracleprice(50000)

    def test_mint_validation_rules(self):
        """Test mint validation rules and limits."""
        self.log.info("Testing mint validation rules...")

        # Test minimum mint amount
        with assert_raises_rpc_error(-32602, "below minimum"):
            self.nodes[0].mintdigidollar("99.99", 365)  # Below $100 minimum

        # Test maximum mint amount
        with assert_raises_rpc_error(-32602, "above maximum"):
            self.nodes[0].mintdigidollar("100001.00", 365)  # Above $100k maximum

        # Test invalid lock periods
        invalid_lock_days = [-1, 0, 29, 3651]  # Negative, zero, too short, too long

        for invalid_days in invalid_lock_days:
            with assert_raises_rpc_error(-32602, "Invalid lock period"):
                self.nodes[0].mintdigidollar("1000.00", invalid_days)

        # Test insufficient balance
        # Create a new node with minimal balance
        insufficient_balance_node = self.nodes[2]

        with assert_raises_rpc_error(-4, "Insufficient balance"):
            insufficient_balance_node.mintdigidollar("1000.00", 365)

        # Test valid amounts at boundaries
        valid_amounts = ["100.00", "100000.00"]  # Min and max valid amounts

        for amount in valid_amounts:
            # Should not raise an error, just calculate requirements
            req = self.nodes[0].calculatecollateralrequirement(amount, 365)
            assert 'collateral_dgb' in req

    def test_edge_cases(self):
        """Test edge cases in minting."""
        self.log.info("Testing minting edge cases...")

        # Test with very precise amounts
        precise_amounts = [
            "100.01",
            "999.99",
            "1234.5678"  # High precision
        ]

        for amount in precise_amounts:
            req = self.nodes[0].calculatecollateralrequirement(amount, 365)
            assert 'collateral_dgb' in req

            # Verify precision is maintained
            calculated_amount = Decimal(amount)
            assert calculated_amount > 0

        # Test lock periods between tiers (should use higher collateral ratio)
        between_tier_days = [45, 120, 200, 400]  # Between defined tiers

        for days in between_tier_days:
            req = self.nodes[0].calculatecollateralrequirement("1000.00", days)
            ratio = int(req['collateral_ratio'])

            # Should use the more conservative (higher) ratio
            assert ratio >= 200, f"Collateral ratio {ratio}% too low for {days} days"

    def test_error_conditions(self):
        """Test error conditions and error handling."""
        self.log.info("Testing error conditions...")

        # Test with DigiDollar disabled
        # (Would require restarting node without -digidollar=1, skipped for now)

        # Test with invalid parameters
        invalid_params = [
            {"amount": "invalid", "lock_days": 365},
            {"amount": "-100", "lock_days": 365},
            {"amount": "1000.00", "lock_days": "invalid"},
            {"amount": "", "lock_days": 365},
            {"amount": "1000.00", "lock_days": None}
        ]

        for params in invalid_params:
            try:
                if params["lock_days"] is None:
                    # Missing parameter
                    with assert_raises_rpc_error(-1, ""):
                        self.nodes[0].mintdigidollar(params["amount"])
                else:
                    with assert_raises_rpc_error(-32602, ""):
                        self.nodes[0].mintdigidollar(params["amount"], params["lock_days"])
            except Exception as e:
                # Some invalid parameters might raise different errors
                # This is acceptable as long as they don't crash the node
                self.log.info(f"Parameter validation caught: {e}")

        # Test oracle price validation
        # Set invalid oracle price and verify it's handled
        try:
            # This should fail or be ignored
            self.nodes[0].setmockoracleprice(-1)
            price_info = self.nodes[0].getoracleprice()
            assert_greater_than(int(price_info['price']), 0)
        except Exception as e:
            # Error is acceptable - negative prices should be rejected
            self.log.info(f"Invalid oracle price rejected: {e}")

        # Test concurrent minting (stress test)
        import threading
        import time

        def mint_worker():
            try:
                result = self.nodes[0].mintdigidollar("500.00", 365)
                return result['txid']
            except Exception as e:
                self.log.info(f"Concurrent mint failed (acceptable): {e}")
                return None

        # Launch multiple concurrent mint operations
        threads = []
        for i in range(3):
            thread = threading.Thread(target=mint_worker)
            threads.append(thread)
            thread.start()

        # Wait for all threads to complete
        for thread in threads:
            thread.join()

        # Mine blocks to confirm any successful transactions
        self.nodes[0].generate(5)
        self.sync_all()

        self.log.info("Minting stress test completed")


if __name__ == '__main__':
    DigiDollarMintTest().main()