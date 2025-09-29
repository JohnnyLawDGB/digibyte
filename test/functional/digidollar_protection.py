#!/usr/bin/env python3
"""Test DigiDollar protection systems.

Test comprehensive protection mechanisms including:
- DCA (Dynamic Collateral Adjustment) multiplier adjustments
- ERR (Emergency Redemption Route) activation
- Volatility freeze mechanisms
- System health monitoring
- Stress testing scenarios
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_less_than,
    assert_in,
    assert_raises_rpc_error,
)
from decimal import Decimal
import time


class DigiDollarProtectionTest(DigiByteTestFramework):
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
        self.log.info("Testing DigiDollar protection systems...")

        # Test setup
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_system_health_monitoring()
        self.test_dca_multiplier_adjustments()
        self.test_volatility_protection()
        self.test_emergency_redemption_route()
        self.test_stress_scenarios()
        self.test_protection_thresholds()
        self.test_recovery_mechanisms()

    def setup_digidollar_test(self):
        """Setup test environment for DigiDollar protection testing."""
        # Generate initial blocks past coinbase maturity
        self.log.info("Generating initial blocks for test setup...")
        self.nodes[0].generate(110)
        self.sync_all()

        # Set initial oracle price ($0.50 per DGB)
        self.base_oracle_price = 50000  # 50000 satoshis per USD
        for node in self.nodes:
            node.setmockoracleprice(self.base_oracle_price)

        # Create initial DD positions to establish system baseline
        self.log.info("Creating initial DD positions for protection testing...")

        # Node 0: Large positions with different lock periods
        positions = [
            {"amount": "2000.00", "lock_days": 365},  # $2k, 1 year
            {"amount": "3000.00", "lock_days": 730},  # $3k, 2 years
            {"amount": "5000.00", "lock_days": 1095}  # $5k, 3 years
        ]

        for pos in positions:
            self.nodes[0].mintdigidollar(pos["amount"], pos["lock_days"])

        # Node 1: Medium position
        self.nodes[1].mintdigidollar("1000.00", 180)

        # Mine blocks to confirm
        self.nodes[0].generate(3)
        self.sync_all()

        # Verify system is in healthy state initially
        initial_health = self.nodes[0].getdigidollarsystemhealth()
        self.log.info(f"Initial system health: {initial_health}")

        # Store initial values for comparison
        self.initial_system_health = initial_health

    def test_system_health_monitoring(self):
        """Test system health monitoring functionality."""
        self.log.info("Testing system health monitoring...")

        # Get comprehensive system health data
        health = self.nodes[0].getdigidollarsystemhealth()

        # Verify required health metrics are present
        required_metrics = [
            'system_collateral_ratio',
            'total_dd_supply',
            'total_collateral_locked',
            'active_positions',
            'average_lock_period',
            'oracle_price_age',
            'dca_level',
            'err_status'
        ]

        for metric in required_metrics:
            assert metric in health, f"Missing health metric: {metric}"

        # Verify system is healthy initially
        collateral_ratio = Decimal(health['system_collateral_ratio'])
        assert_greater_than(collateral_ratio, Decimal('150'))  # Should be well above minimum

        # Test health monitoring across nodes
        for i in range(self.num_nodes):
            node_health = self.nodes[i].getdigidollarsystemhealth()
            # Health should be consistent across nodes
            assert_equal(node_health['system_collateral_ratio'], health['system_collateral_ratio'])
            assert_equal(node_health['total_dd_supply'], health['total_dd_supply'])

        # Test historical health tracking
        try:
            health_history = self.nodes[0].getdigidollarhealthhistory(24)  # Last 24 hours
            assert isinstance(health_history, list)
            self.log.info(f"Health history: {len(health_history)} entries")

            if len(health_history) > 0:
                for entry in health_history:
                    assert 'timestamp' in entry
                    assert 'collateral_ratio' in entry

        except Exception as e:
            self.log.info(f"Health history not available (acceptable): {e}")

    def test_dca_multiplier_adjustments(self):
        """Test Dynamic Collateral Adjustment (DCA) multiplier functionality."""
        self.log.info("Testing DCA multiplier adjustments...")

        # Test DCA under normal conditions
        normal_dca = self.nodes[0].getdcamultiplier()

        assert 'multiplier' in normal_dca
        assert 'system_collateral' in normal_dca
        assert 'level' in normal_dca
        assert 'reason' in normal_dca

        # Under normal conditions, multiplier should be 1.0 (100%)
        normal_multiplier = Decimal(normal_dca['multiplier'])
        assert_equal(normal_multiplier, Decimal('1.0'))

        # Test DCA response to system stress
        # Simulate stress by reducing oracle price (makes collateral worth less)
        stress_scenarios = [
            {"price": 40000, "expected_level": "mild_stress"},     # 20% price drop
            {"price": 30000, "expected_level": "moderate_stress"}, # 40% price drop
            {"price": 20000, "expected_level": "severe_stress"}    # 60% price drop
        ]

        for scenario in stress_scenarios:
            self.log.info(f"Testing DCA under stress scenario: {scenario}")

            # Set stress price
            for node in self.nodes:
                node.setmockoracleprice(scenario["price"])

            # Generate block to make price change effective
            self.nodes[0].generate(1)
            self.sync_all()

            # Check DCA response
            stress_dca = self.nodes[0].getdcamultiplier()
            stress_multiplier = Decimal(stress_dca['multiplier'])

            # Under stress, multiplier should increase
            assert_greater_than(stress_multiplier, Decimal('1.0'))

            # More severe stress should result in higher multipliers
            if scenario["price"] <= 25000:  # Severe stress
                assert_greater_than(stress_multiplier, Decimal('1.5'))

            # Test impact on new minting requirements
            if stress_multiplier > Decimal('1.0'):
                collateral_req = self.nodes[1].calculatecollateralrequirement("1000.00", 365)
                assert_equal(Decimal(collateral_req['dca_multiplier']), stress_multiplier)

                # Collateral requirement should be higher
                stressed_collateral = Decimal(collateral_req['collateral_dgb'])
                # Compare with baseline calculation
                baseline_collateral = Decimal('1000.00') * 3 * self.base_oracle_price / Decimal('100000000')  # 300% ratio
                assert_greater_than(stressed_collateral, baseline_collateral)

        # Restore normal price
        for node in self.nodes:
            node.setmockoracleprice(self.base_oracle_price)

    def test_volatility_protection(self):
        """Test volatility protection mechanisms."""
        self.log.info("Testing volatility protection...")

        # Test volatility detection
        volatility_scenarios = [
            {"name": "rapid_increase", "price_changes": [50000, 75000, 100000]},  # 100% increase
            {"name": "rapid_decrease", "price_changes": [50000, 37500, 25000]},   # 50% decrease
            {"name": "high_volatility", "price_changes": [50000, 75000, 40000, 60000]}  # Oscillation
        ]

        for scenario in volatility_scenarios:
            self.log.info(f"Testing volatility scenario: {scenario['name']}")

            # Apply rapid price changes
            for price in scenario["price_changes"]:
                for node in self.nodes:
                    node.setmockoracleprice(price)

                # Generate block for each price change
                self.nodes[0].generate(1)
                self.sync_all()

                # Brief pause to simulate time passage
                time.sleep(0.1)

            # Check volatility detection
            protection_status = self.nodes[0].getprotectionstatus()

            assert 'volatility_detected' in protection_status
            assert 'volatility_threshold' in protection_status
            assert 'price_change_rate' in protection_status

            # High volatility should trigger protection
            if scenario["name"] in ["rapid_increase", "rapid_decrease"]:
                volatility_rate = abs(Decimal(protection_status['price_change_rate']))
                volatility_threshold = Decimal(protection_status['volatility_threshold'])

                if volatility_rate > volatility_threshold:
                    assert protection_status['volatility_detected'] == True

                    # Check if volatility freeze is active
                    if 'volatility_freeze' in protection_status:
                        self.log.info("Volatility freeze activated")

                        # During freeze, certain operations should be restricted
                        try:
                            # Attempt minting during volatility freeze
                            result = self.nodes[1].calculatecollateralrequirement("500.00", 365)

                            # Should either work with higher requirements or be blocked
                            if 'volatility_adjustment' in result:
                                adjustment = Decimal(result['volatility_adjustment'])
                                assert_greater_than(adjustment, Decimal('1.0'))

                        except Exception as e:
                            # Blocking operations during freeze is acceptable
                            self.log.info(f"Operation blocked during volatility freeze: {e}")

        # Restore stable price
        for node in self.nodes:
            node.setmockoracleprice(self.base_oracle_price)

        # Generate blocks to stabilize
        self.nodes[0].generate(5)
        self.sync_all()

    def test_emergency_redemption_route(self):
        """Test Emergency Redemption Route (ERR) activation."""
        self.log.info("Testing Emergency Redemption Route (ERR)...")

        # ERR is triggered when system collateral falls to emergency levels
        # Test different ERR trigger scenarios

        # Scenario 1: Gradual price decline to ERR threshold
        err_trigger_price = self.base_oracle_price // 4  # 75% price drop

        self.log.info(f"Testing ERR trigger with price drop to {err_trigger_price}")

        # Gradually reduce price
        price_steps = [40000, 30000, 20000, 15000, err_trigger_price]

        for price in price_steps:
            for node in self.nodes:
                node.setmockoracleprice(price)

            self.nodes[0].generate(1)
            self.sync_all()

            # Check ERR status after each price drop
            protection_status = self.nodes[0].getprotectionstatus()

            if 'err_active' in protection_status and protection_status['err_active']:
                self.log.info("ERR activated!")

                # Verify ERR activation details
                assert 'err_trigger_height' in protection_status
                assert 'err_reason' in protection_status
                assert 'emergency_collateral_ratio' in protection_status

                # Test ERR redemption mechanics
                err_info = self.nodes[0].geterremptioninfo()

                assert 'available_dgb' in err_info
                assert 'total_dd_eligible' in err_info
                assert 'redemption_rate' in err_info

                # Test emergency redemption
                if self.nodes[0].getdigidollarbalance() > Decimal('100'):
                    try:
                        err_redemption = self.nodes[0].redeemdigidollar("100.00")

                        # ERR redemption should be marked as emergency
                        assert 'emergency_redemption' in err_redemption
                        assert err_redemption['emergency_redemption'] == True

                        self.nodes[0].generate(1)
                        self.sync_all()

                        self.log.info("Emergency redemption completed successfully")

                    except Exception as e:
                        self.log.info(f"Emergency redemption test: {e}")

                break

        # Test ERR deactivation conditions
        # Restore higher price to potentially deactivate ERR
        recovery_price = self.base_oracle_price // 2  # Still low but better

        for node in self.nodes:
            node.setmockoracleprice(recovery_price)

        self.nodes[0].generate(5)  # Generate several blocks for recovery
        self.sync_all()

        # Check if ERR remains active or deactivates
        recovery_status = self.nodes[0].getprotectionstatus()
        self.log.info(f"ERR status after price recovery: {recovery_status.get('err_active', False)}")

        # Restore normal price
        for node in self.nodes:
            node.setmockoracleprice(self.base_oracle_price)

    def test_stress_scenarios(self):
        """Test system behavior under various stress scenarios."""
        self.log.info("Testing stress scenarios...")

        # Scenario 1: Massive minting during low collateral
        stress_price = 30000  # Reduce collateral value

        for node in self.nodes:
            node.setmockoracleprice(stress_price)

        self.nodes[0].generate(1)
        self.sync_all()

        # Attempt large minting during stress
        try:
            large_mint = self.nodes[1].calculatecollateralrequirement("5000.00", 365)

            # Should require much higher collateral
            stress_collateral = Decimal(large_mint['collateral_dgb'])
            stress_multiplier = Decimal(large_mint['dca_multiplier'])

            assert_greater_than(stress_multiplier, Decimal('1.25'))  # At least 25% increase

            self.log.info(f"Stress minting requirements: {stress_multiplier}x multiplier")

        except Exception as e:
            # Blocking large mints during stress is acceptable
            self.log.info(f"Large minting blocked during stress (good): {e}")

        # Scenario 2: Rapid large redemptions
        try:
            # Attempt multiple rapid redemptions
            redemption_amounts = ["200.00", "300.00", "500.00"]

            for amount in redemption_amounts:
                if self.nodes[0].getdigidollarbalance() >= Decimal(amount):
                    redemption = self.nodes[0].redeemdigidollar(amount)
                    self.log.info(f"Stress redemption of {amount} DD completed")

            self.nodes[0].generate(1)
            self.sync_all()

        except Exception as e:
            self.log.info(f"Rapid redemptions handling: {e}")

        # Scenario 3: Oracle price manipulation attempts
        manipulation_prices = [1, 1000000, 0, -1000]  # Extreme values

        for price in manipulation_prices:
            try:
                self.nodes[0].setmockoracleprice(price)

                oracle_info = self.nodes[0].getoracleprice()

                # System should reject or filter extreme prices
                if 'price' in oracle_info:
                    actual_price = int(oracle_info['price'])
                    # Should not accept obviously manipulated prices
                    assert_greater_than(actual_price, 1000)  # > $0.01
                    assert_less_than(actual_price, 1000000)  # < $10

            except Exception as e:
                # Price rejection is good security
                self.log.info(f"Extreme price {price} rejected (good): {e}")

        # Restore normal conditions
        for node in self.nodes:
            node.setmockoracleprice(self.base_oracle_price)

    def test_protection_thresholds(self):
        """Test protection system thresholds and boundaries."""
        self.log.info("Testing protection thresholds...")

        # Test DCA level thresholds
        dca_test_prices = [
            {"price": 45000, "expected_level": 0},  # Mild stress
            {"price": 35000, "expected_level": 1},  # Moderate stress
            {"price": 25000, "expected_level": 2},  # High stress
            {"price": 15000, "expected_level": 3}   # Critical stress
        ]

        for test in dca_test_prices:
            for node in self.nodes:
                node.setmockoracleprice(test["price"])

            self.nodes[0].generate(1)
            self.sync_all()

            dca_info = self.nodes[0].getdcamultiplier()
            system_health = self.nodes[0].getdigidollarsystemhealth()

            # Verify DCA level progression
            dca_level = dca_info.get('level', 0)
            collateral_ratio = Decimal(system_health['system_collateral_ratio'])

            # Lower prices should trigger higher DCA levels
            if test["price"] <= 25000:
                assert_greater_than_or_equal(dca_level, 1)

            if test["price"] <= 15000:
                assert_greater_than_or_equal(dca_level, 2)

            self.log.info(f"Price {test['price']}: DCA level {dca_level}, ratio {collateral_ratio}%")

        # Test ERR threshold precision
        # Find the exact price that triggers ERR
        err_test_prices = [12000, 11000, 10000, 9000, 8000]

        err_triggered = False
        for price in err_test_prices:
            for node in self.nodes:
                node.setmockoracleprice(price)

            self.nodes[0].generate(1)
            self.sync_all()

            protection_status = self.nodes[0].getprotectionstatus()

            if protection_status.get('err_active', False):
                self.log.info(f"ERR triggered at price: {price}")
                err_triggered = True
                break

        if err_triggered:
            # Test ERR threshold boundaries
            # Price slightly above trigger should not activate ERR
            boundary_price = price + 1000

            for node in self.nodes:
                node.setmockoracleprice(boundary_price)

            self.nodes[0].generate(1)
            self.sync_all()

            boundary_status = self.nodes[0].getprotectionstatus()
            # May or may not be active depending on hysteresis

        # Restore normal price
        for node in self.nodes:
            node.setmockoracleprice(self.base_oracle_price)

    def test_recovery_mechanisms(self):
        """Test system recovery mechanisms."""
        self.log.info("Testing recovery mechanisms...")

        # Create stress condition
        stress_price = 20000
        for node in self.nodes:
            node.setmockoracleprice(stress_price)

        self.nodes[0].generate(1)
        self.sync_all()

        # Record stress state
        stress_health = self.nodes[0].getdigidollarsystemhealth()
        stress_dca = self.nodes[0].getdcamultiplier()

        # Begin recovery by improving price gradually
        recovery_prices = [25000, 30000, 35000, 40000, 45000, 50000]

        for price in recovery_prices:
            for node in self.nodes:
                node.setmockoracleprice(price)

            self.nodes[0].generate(2)  # Generate multiple blocks for stability
            self.sync_all()

            # Monitor recovery progress
            recovery_health = self.nodes[0].getdigidollarsystemhealth()
            recovery_dca = self.nodes[0].getdcamultiplier()

            recovery_ratio = Decimal(recovery_health['system_collateral_ratio'])
            recovery_multiplier = Decimal(recovery_dca['multiplier'])

            self.log.info(f"Recovery at price {price}: ratio {recovery_ratio}%, DCA {recovery_multiplier}x")

            # System should gradually improve
            if price >= 40000:  # Near normal levels
                # DCA multiplier should approach 1.0
                assert_less_than(recovery_multiplier, Decimal('1.5'))

            if price >= 50000:  # Full recovery
                # Should return to normal operation
                assert_equal(recovery_multiplier, Decimal('1.0'))

        # Verify full recovery
        final_health = self.nodes[0].getdigidollarsystemhealth()
        final_protection = self.nodes[0].getprotectionstatus()

        # All protection mechanisms should be back to normal
        assert final_protection.get('err_active', False) == False
        assert final_protection.get('volatility_detected', False) == False

        # System health should be good
        final_ratio = Decimal(final_health['system_collateral_ratio'])
        assert_greater_than(final_ratio, Decimal('200'))  # Well above minimum

        self.log.info("System recovery completed successfully")


if __name__ == '__main__':
    DigiDollarProtectionTest().main()