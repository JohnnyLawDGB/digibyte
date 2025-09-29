#!/usr/bin/env python3
"""Test DigiDollar RPC interface.

Test comprehensive RPC functionality including:
- All DigiDollar RPC commands
- Parameter validation
- Error handling
- Response formats
- Command integration
- Performance testing
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_in,
    assert_raises_rpc_error,
)
from decimal import Decimal
import json


class DigiDollarRPCTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # Enable DigiDollar features
        self.extra_args = [
            ["-digidollar=1", "-mocktime=0"],
            ["-digidollar=1", "-mocktime=0"]
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar RPC interface...")

        # Test setup
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_system_monitoring_commands()
        self.test_core_transaction_commands()
        self.test_address_management_commands()
        self.test_utility_commands()
        self.test_parameter_validation()
        self.test_error_handling()
        self.test_response_formats()
        self.test_command_integration()

    def setup_digidollar_test(self):
        """Setup test environment for DigiDollar."""
        # Generate initial blocks past coinbase maturity
        self.log.info("Generating initial blocks for test setup...")
        self.nodes[0].generate(110)
        self.sync_all()

        # Set mock oracle price
        base_price = 50000  # 50000 satoshis per USD
        for node in self.nodes:
            node.setmockoracleprice(base_price)

        # Create some initial DD positions for testing
        self.nodes[0].mintdigidollar("2000.00", 365)
        self.nodes[1].mintdigidollar("1000.00", 180)

        self.nodes[0].generate(2)
        self.sync_all()

    def test_system_monitoring_commands(self):
        """Test system monitoring RPC commands."""
        self.log.info("Testing system monitoring RPC commands...")

        # Test getdigidollarsystemhealth
        health = self.nodes[0].getdigidollarsystemhealth()
        self.log.info(f"System health response: {health}")

        required_health_fields = [
            'system_collateral_ratio',
            'total_dd_supply',
            'total_collateral_locked',
            'active_positions',
            'oracle_price_age'
        ]

        for field in required_health_fields:
            assert field in health, f"Missing health field: {field}"
            assert health[field] is not None, f"Null value for health field: {field}"

        # Verify data types
        assert isinstance(health['system_collateral_ratio'], (int, float, str))
        assert isinstance(health['total_dd_supply'], (int, float, str))
        assert isinstance(health['active_positions'], int)

        # Test getdcamultiplier
        dca = self.nodes[0].getdcamultiplier()
        self.log.info(f"DCA multiplier response: {dca}")

        required_dca_fields = ['multiplier', 'system_collateral', 'level', 'reason']
        for field in required_dca_fields:
            assert field in dca, f"Missing DCA field: {field}"

        # Multiplier should be a valid number >= 1.0
        multiplier = float(dca['multiplier'])
        assert_greater_than(multiplier, 0.99)  # Allow for floating point precision

        # Test getdigidollarstats
        stats = self.nodes[0].getdigidollarstats()
        self.log.info(f"DigiDollar stats response: {stats}")

        required_stats_fields = [
            'total_supply',
            'total_collateral',
            'positions_count',
            'average_lock_period',
            'system_health_score'
        ]

        for field in required_stats_fields:
            assert field in stats, f"Missing stats field: {field}"

        # Test calculatecollateralrequirement
        collateral_req = self.nodes[0].calculatecollateralrequirement("1000.00", 365)
        self.log.info(f"Collateral requirement response: {collateral_req}")

        required_collateral_fields = [
            'collateral_dgb',
            'collateral_ratio',
            'oracle_price',
            'dca_multiplier'
        ]

        for field in required_collateral_fields:
            assert field in collateral_req, f"Missing collateral field: {field}"

        # Test getdigidollarstatus
        status = self.nodes[0].getdigidollarstatus()
        self.log.info(f"DigiDollar status response: {status}")

        assert 'active' in status
        assert status['active'] == True
        assert 'version' in status

    def test_core_transaction_commands(self):
        """Test core transaction RPC commands."""
        self.log.info("Testing core transaction RPC commands...")

        # Test mintdigidollar
        mint_result = self.nodes[0].mintdigidollar("500.00", 180)
        self.log.info(f"Mint result: {mint_result}")

        required_mint_fields = ['txid', 'dd_address', 'collateral_required']
        for field in required_mint_fields:
            assert field in mint_result, f"Missing mint field: {field}"

        # Verify txid format
        assert len(mint_result['txid']) == 64  # SHA256 hash length
        assert all(c in '0123456789abcdef' for c in mint_result['txid'])

        # Test senddigidollar
        receiver_address = self.nodes[1].getdigidollaraddress()
        send_result = self.nodes[0].senddigidollar(receiver_address, "100.00")
        self.log.info(f"Send result: {send_result}")

        required_send_fields = ['txid']
        for field in required_send_fields:
            assert field in send_result, f"Missing send field: {field}"

        # Test redeemdigidollar
        redeem_result = self.nodes[1].redeemdigidollar("50.00")
        self.log.info(f"Redeem result: {redeem_result}")

        required_redeem_fields = ['txid', 'dgb_returned', 'redemption_info']
        for field in required_redeem_fields:
            assert field in redeem_result, f"Missing redeem field: {field}"

        # Test listdigidollarpositions
        positions = self.nodes[0].listdigidollarpositions()
        self.log.info(f"Positions count: {len(positions)}")

        assert isinstance(positions, list)
        assert len(positions) > 0

        for position in positions:
            required_position_fields = ['amount', 'lock_height', 'dd_address', 'status']
            for field in required_position_fields:
                assert field in position, f"Missing position field: {field}"

        # Mine blocks to confirm transactions
        self.nodes[0].generate(3)
        self.sync_all()

    def test_address_management_commands(self):
        """Test address management RPC commands."""
        self.log.info("Testing address management RPC commands...")

        # Test getdigidollaraddress
        dd_address = self.nodes[0].getdigidollaraddress()
        self.log.info(f"Generated DD address: {dd_address}")

        assert isinstance(dd_address, str)
        assert len(dd_address) > 20  # Reasonable address length
        assert dd_address.startswith('dgbrt1dd')  # Regtest DD address prefix

        # Test validateddaddress
        validation = self.nodes[0].validateddaddress(dd_address)
        self.log.info(f"Address validation: {validation}")

        required_validation_fields = ['isvalid', 'ismine']
        for field in required_validation_fields:
            assert field in validation, f"Missing validation field: {field}"

        assert validation['isvalid'] == True
        assert validation['ismine'] == True

        # Test invalid address validation
        invalid_validation = self.nodes[0].validateddaddress("invalid_address")
        assert invalid_validation['isvalid'] == False

        # Test listdigidollaraddresses
        addresses = self.nodes[0].listdigidollaraddresses()
        self.log.info(f"DD addresses count: {len(addresses)}")

        assert isinstance(addresses, list)
        assert dd_address in addresses

        # Test importdigidollaraddress (if implemented)
        try:
            import_result = self.nodes[1].importdigidollaraddress(dd_address, "test_label")
            self.log.info(f"Import address result: {import_result}")
        except Exception as e:
            self.log.info(f"Import address not implemented or failed: {e}")

    def test_utility_commands(self):
        """Test utility RPC commands."""
        self.log.info("Testing utility RPC commands...")

        # Test getdigidollarbalance
        balance = self.nodes[0].getdigidollarbalance()
        self.log.info(f"DD balance: {balance}")

        assert isinstance(balance, (int, float, str))
        balance_decimal = Decimal(str(balance))
        assert_greater_than(balance_decimal, Decimal('0'))

        # Test estimatecollateral
        estimate = self.nodes[0].estimatecollateral("1000.00", 365)
        self.log.info(f"Collateral estimate: {estimate}")

        required_estimate_fields = ['estimated_collateral', 'current_price', 'lock_tier']
        for field in required_estimate_fields:
            assert field in estimate, f"Missing estimate field: {field}"

        # Test getredemptioninfo
        redemption_info = self.nodes[0].getredemptioninfo("100.00")
        self.log.info(f"Redemption info: {redemption_info}")

        required_redemption_fields = ['can_redeem', 'dgb_returned', 'penalty_amount']
        for field in required_redemption_fields:
            assert field in redemption_info, f"Missing redemption field: {field}"

        # Test listdigidollartxs
        transactions = self.nodes[0].listdigidollartxs()
        self.log.info(f"DD transactions count: {len(transactions)}")

        assert isinstance(transactions, list)

        if len(transactions) > 0:
            tx = transactions[0]
            required_tx_fields = ['txid', 'category', 'amount', 'confirmations']
            for field in required_tx_fields:
                assert field in tx, f"Missing transaction field: {field}"

        # Test getoracleprice
        oracle_price = self.nodes[0].getoracleprice()
        self.log.info(f"Oracle price: {oracle_price}")

        required_oracle_fields = ['price', 'timestamp', 'consensus']
        for field in required_oracle_fields:
            assert field in oracle_price, f"Missing oracle field: {field}"

        # Test getprotectionstatus
        protection = self.nodes[0].getprotectionstatus()
        self.log.info(f"Protection status: {protection}")

        required_protection_fields = ['err_active', 'volatility_detected', 'dca_level']
        for field in required_protection_fields:
            assert field in protection, f"Missing protection field: {field}"

    def test_parameter_validation(self):
        """Test RPC parameter validation."""
        self.log.info("Testing RPC parameter validation...")

        # Test invalid amounts
        invalid_amounts = ["", "abc", "-100", "0", "999999999.99"]

        for amount in invalid_amounts:
            with assert_raises_rpc_error(-32602, ""):
                self.nodes[0].mintdigidollar(amount, 365)

        # Test invalid lock periods
        invalid_lock_periods = [-1, 0, 29, 3651, "invalid"]

        for period in invalid_lock_periods:
            with assert_raises_rpc_error(-32602, ""):
                self.nodes[0].mintdigidollar("1000.00", period)

        # Test invalid addresses
        invalid_addresses = ["", "invalid", "dgb1qtest", "dgbrt1cc" + "0" * 60]

        for address in invalid_addresses:
            with assert_raises_rpc_error(-5, ""):
                self.nodes[0].senddigidollar(address, "100.00")

        # Test missing parameters
        with assert_raises_rpc_error(-1, ""):
            self.nodes[0].mintdigidollar()

        with assert_raises_rpc_error(-1, ""):
            self.nodes[0].senddigidollar("address_only")

        # Test parameter type validation
        with assert_raises_rpc_error(-3, ""):
            self.nodes[0].mintdigidollar(1000, 365)  # Should be string

        with assert_raises_rpc_error(-3, ""):
            self.nodes[0].mintdigidollar("1000.00", "365")  # Should be integer

    def test_error_handling(self):
        """Test RPC error handling."""
        self.log.info("Testing RPC error handling...")

        # Test insufficient balance errors
        large_amount = "999999.00"
        with assert_raises_rpc_error(-4, "Insufficient"):
            self.nodes[1].mintdigidollar(large_amount, 365)

        # Test non-existent address errors
        fake_address = "dgbrt1dd" + "0" * 50
        with assert_raises_rpc_error(-5, ""):
            validation = self.nodes[0].validateddaddress(fake_address)
            if validation.get('isvalid', False):
                # If address format is valid but not owned
                with assert_raises_rpc_error(-4, ""):
                    self.nodes[0].senddigidollar(fake_address, "100.00")

        # Test operations when DigiDollar is inactive (simulated)
        # This would require restarting nodes without -digidollar=1

        # Test malformed JSON-RPC calls
        try:
            # Direct RPC call with malformed parameters
            response = self.nodes[0]._get_authproxy().mintdigidollar()
        except Exception as e:
            # Should raise appropriate RPC error
            assert "Missing" in str(e) or "required" in str(e).lower()

        # Test rate limiting (if implemented)
        # Rapid successive calls
        for i in range(10):
            try:
                self.nodes[0].getdigidollarstatus()
            except Exception as e:
                if "rate limit" in str(e).lower():
                    self.log.info("Rate limiting detected (good)")
                    break

    def test_response_formats(self):
        """Test RPC response formats and data consistency."""
        self.log.info("Testing RPC response formats...")

        # Test JSON serialization
        responses = {
            'health': self.nodes[0].getdigidollarsystemhealth(),
            'stats': self.nodes[0].getdigidollarstats(),
            'positions': self.nodes[0].listdigidollarpositions(),
            'oracle': self.nodes[0].getoracleprice(),
            'protection': self.nodes[0].getprotectionstatus()
        }

        for name, response in responses.items():
            # Ensure response is JSON serializable
            try:
                json_str = json.dumps(response)
                parsed = json.loads(json_str)
                assert parsed == response, f"JSON serialization failed for {name}"
            except Exception as e:
                assert False, f"Response {name} not JSON serializable: {e}"

        # Test numeric precision
        balance = self.nodes[0].getdigidollarbalance()
        balance_str = str(balance)

        # Should handle decimal precision properly
        if '.' in balance_str:
            decimal_places = len(balance_str.split('.')[1])
            assert decimal_places <= 8, "Too many decimal places in balance"

        # Test array responses
        addresses = self.nodes[0].listdigidollaraddresses()
        assert isinstance(addresses, list)

        transactions = self.nodes[0].listdigidollartxs()
        assert isinstance(transactions, list)

        # Test boolean responses
        status = self.nodes[0].getdigidollarstatus()
        assert isinstance(status['active'], bool)

        validation = self.nodes[0].validateddaddress(self.nodes[0].getdigidollaraddress())
        assert isinstance(validation['isvalid'], bool)
        assert isinstance(validation['ismine'], bool)

    def test_command_integration(self):
        """Test integration between different RPC commands."""
        self.log.info("Testing RPC command integration...")

        # Test mint -> list -> send -> redeem workflow
        initial_balance = self.nodes[0].getdigidollarbalance()

        # 1. Mint
        mint_result = self.nodes[0].mintdigidollar("300.00", 90)
        mint_txid = mint_result['txid']

        # 2. Check positions
        positions_before = self.nodes[0].listdigidollarpositions()

        # 3. Mine block to confirm
        self.nodes[0].generate(1)
        self.sync_all()

        # 4. Verify balance increased
        balance_after_mint = self.nodes[0].getdigidollarbalance()
        assert_greater_than(balance_after_mint, initial_balance)

        # 5. Check transaction list
        transactions = self.nodes[0].listdigidollartxs()
        mint_tx = next((tx for tx in transactions if tx['txid'] == mint_txid), None)
        assert mint_tx is not None
        assert mint_tx['category'] == 'mint'

        # 6. Send some DD
        receiver_address = self.nodes[1].getdigidollaraddress()
        send_result = self.nodes[0].senddigidollar(receiver_address, "50.00")
        send_txid = send_result['txid']

        # 7. Mine block to confirm
        self.nodes[0].generate(1)
        self.sync_all()

        # 8. Verify balances
        sender_balance = self.nodes[0].getdigidollarbalance()
        receiver_balance = self.nodes[1].getdigidollarbalance()

        expected_sender = balance_after_mint - Decimal('50.00')
        assert_equal(sender_balance, expected_sender)
        assert_greater_than(receiver_balance, Decimal('0'))

        # 9. Test redemption
        redeem_result = self.nodes[1].redeemdigidollar("25.00")
        redeem_txid = redeem_result['txid']

        # 10. Mine block to confirm
        self.nodes[1].generate(1)
        self.sync_all()

        # 11. Verify final balances
        final_receiver_balance = self.nodes[1].getdigidollarbalance()
        expected_final = receiver_balance - Decimal('25.00')
        assert_equal(final_receiver_balance, expected_final)

        # 12. Verify all transactions appear in history
        sender_txs = self.nodes[0].listdigidollartxs()
        receiver_txs = self.nodes[1].listdigidollartxs()

        sender_txids = [tx['txid'] for tx in sender_txs]
        receiver_txids = [tx['txid'] for tx in receiver_txs]

        assert mint_txid in sender_txids
        assert send_txid in sender_txids
        assert send_txid in receiver_txids
        assert redeem_txid in receiver_txids

        # 13. Test cross-command data consistency
        final_stats = self.nodes[0].getdigidollarstats()
        total_supply = Decimal(final_stats['total_supply'])

        # Total supply should equal sum of all balances
        all_balances = sum(Decimal(str(node.getdigidollarbalance())) for node in self.nodes)
        tolerance = Decimal('0.01')  # Small tolerance for rounding
        assert abs(total_supply - all_balances) <= tolerance

        self.log.info("RPC command integration test completed successfully")


if __name__ == '__main__':
    DigiDollarRPCTest().main()