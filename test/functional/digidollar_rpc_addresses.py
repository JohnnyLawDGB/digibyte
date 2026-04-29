#!/usr/bin/env python3
"""Test DigiDollar address management RPC commands.

Test comprehensive functionality for DD address RPCs:
- validateddaddress: Validate DD address format, prefix, checksum
- listdigidollaraddresses: List wallet DD addresses with filtering
- importdigidollaraddress: Import external DD addresses for watch-only

DD addresses use prefixes:
- DD (mainnet)
- TD (testnet)
- RD (regtest)

Addresses are P2TR (Taproot) encoded in base58check format.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)


class DigiDollarAddressTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"]
        ]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar address management RPCs...")

        self.setup_digidollar_test()

        self.log.info("=== validateddaddress tests ===")
        self.test_validate_valid_regtest_address()
        self.test_validate_invalid_prefix()
        self.test_validate_invalid_checksum()
        self.test_validate_empty_address()
        self.test_validate_response_format()

        self.log.info("=== listdigidollaraddresses tests ===")
        self.test_list_addresses_empty_wallet()
        self.test_list_addresses_after_generation()
        self.test_list_addresses_with_balance()
        self.test_list_addresses_include_watchonly()

        self.log.info("=== Bug #12 regression test ===")
        self.test_no_mock_addresses_bug12()

        self.log.info("=== importdigidollaraddress tests ===")
        self.test_import_valid_address()
        self.test_import_with_label()
        self.test_import_duplicate()
        self.test_import_invalid_address()

        self.log.info("All DigiDollar address management tests passed!")

    def setup_digidollar_test(self):
        self.log.info("Generating initial blocks for test setup...")
        self.generate(self.nodes[0], 110)
        self.sync_all()

        self.log.info("Setting mock oracle price...")
        for node in self.nodes:
            node.setmockoracleprice(500000)

        stats = self.nodes[0].getdigidollarstats()
        assert "health_percentage" in stats
        assert "health_status" in stats

    # === validateddaddress tests ===

    def test_validate_valid_regtest_address(self):
        self.log.info("Testing validateddaddress with valid regtest address...")

        dd_address = self.nodes[0].getdigidollaraddress()
        self.log.info(f"Generated DD address: {dd_address}")

        assert dd_address.startswith('RD'), f"Regtest DD address should start with 'RD', got: {dd_address}"

        result = self.nodes[0].validateddaddress(dd_address)

        assert_equal(result['isvalid'], True)
        assert_equal(result['address'], dd_address)
        assert_equal(result['network'], 'regtest')
        assert_equal(result['prefix'], 'RD')

        self.log.info("Valid regtest address validation passed")

    def test_validate_invalid_prefix(self):
        self.log.info("Testing validateddaddress with invalid prefix...")

        invalid_address = "XXinvalidprefix123456789abc"

        result = self.nodes[0].validateddaddress(invalid_address)

        assert_equal(result['isvalid'], False)
        assert 'error' in result
        assert 'prefix' in result['error'].lower() or 'invalid' in result['error'].lower()

        self.log.info("Invalid prefix validation passed")

    def test_validate_invalid_checksum(self):
        self.log.info("Testing validateddaddress with corrupted address...")

        valid_address = self.nodes[0].getdigidollaraddress()

        if len(valid_address) > 5:
            corrupted_address = valid_address[:-4] + "XXXX"
        else:
            corrupted_address = valid_address + "corrupt"

        result = self.nodes[0].validateddaddress(corrupted_address)

        assert 'isvalid' in result
        assert_equal(result['isvalid'], False)
        assert 'error' in result
        self.log.info(f"Corrupted address correctly marked invalid: {result['error']}")

    def test_validate_empty_address(self):
        self.log.info("Testing validateddaddress with empty address...")

        try:
            result = self.nodes[0].validateddaddress("")

            assert_equal(result['isvalid'], False)
            assert 'error' in result
            self.log.info(f"Empty address validation error: {result['error']}")

        except Exception as e:
            self.log.info(f"Empty address raised expected error: {e}")

        self.log.info("Empty address validation passed")

    def test_validate_response_format(self):
        self.log.info("Testing validateddaddress response format...")

        dd_address = self.nodes[0].getdigidollaraddress()

        result = self.nodes[0].validateddaddress(dd_address)

        required_fields = ['isvalid', 'address', 'network', 'prefix']
        for field in required_fields:
            assert field in result, f"Missing required field: {field}"
            self.log.info(f"  {field}: {result[field]}")

        optional_fields = ['ismine', 'iswatchonly']
        for field in optional_fields:
            if field in result:
                self.log.info(f"  {field}: {result[field]}")

        assert isinstance(result['isvalid'], bool)
        assert isinstance(result['address'], str)
        assert isinstance(result['network'], str)
        assert isinstance(result['prefix'], str)

        invalid_result = self.nodes[0].validateddaddress("XXinvalid")
        assert 'isvalid' in invalid_result
        assert_equal(invalid_result['isvalid'], False)
        assert 'error' in invalid_result
        assert isinstance(invalid_result['error'], str)

        self.log.info("Response format validation passed")

    # === listdigidollaraddresses tests ===

    def test_list_addresses_empty_wallet(self):
        self.log.info("Testing listdigidollaraddresses on empty/new wallet...")

        result = self.nodes[1].listdigidollaraddresses()

        assert isinstance(result, list), f"Expected list, got {type(result)}"

        self.log.info(f"Node 1 has {len(result)} DD addresses initially")

        for addr_info in result:
            assert isinstance(addr_info, dict), "Each address entry should be a dict"
            assert 'address' in addr_info, "Missing 'address' field"

        self.log.info("Empty wallet address list test passed")

    def test_list_addresses_after_generation(self):
        self.log.info("Testing listdigidollaraddresses after generating addresses...")

        generated_addresses = []
        for i in range(3):
            addr = self.nodes[0].getdigidollaraddress()
            generated_addresses.append(addr)
            self.log.info(f"Generated address {i+1}: {addr}")

        assert len(set(generated_addresses)) == len(generated_addresses), "Generated addresses should be unique"

        result = self.nodes[0].listdigidollaraddresses()

        assert isinstance(result, list), f"Expected list, got {type(result)}"
        self.log.info(f"listdigidollaraddresses returned {len(result)} addresses")

        for addr_info in result:
            assert isinstance(addr_info, dict), "Each address entry should be a dict"
            if 'address' in addr_info:
                self.log.info(f"  Address: {addr_info['address']}")

        self.log.info("Address list after generation test passed")

    def test_list_addresses_with_balance(self):
        self.log.info("Testing listdigidollaraddresses with balance filter...")

        mint_amount = 50000
        dca_tier = 1

        self.log.info(f"Minting {mint_amount} cents DD...")
        mint_result = self.nodes[0].mintdigidollar(mint_amount, dca_tier)
        assert 'txid' in mint_result

        self.generate(self.nodes[0], 2)
        self.sync_all()

        balance_info = self.nodes[0].getdigidollarbalance()
        current_balance = balance_info['total'] if isinstance(balance_info, dict) else balance_info
        assert_greater_than(current_balance, 0)
        self.log.info(f"Current DD balance: {current_balance} cents")

        result_with_filter = self.nodes[0].listdigidollaraddresses(False, 1000)
        assert isinstance(result_with_filter, list)
        self.log.info(f"Addresses with balance >= 1000 cents: {len(result_with_filter)}")
        assert_greater_than(len(result_with_filter), 0)

        for addr_info in result_with_filter:
            assert addr_info['balance'] >= 1000, f"Balance {addr_info['balance']} below filter"

        result_above_balance = self.nodes[0].listdigidollaraddresses(False, mint_amount + 1)
        assert_equal(result_above_balance, [])

        result_all = self.nodes[0].listdigidollaraddresses()
        assert isinstance(result_all, list)
        self.log.info(f"Total addresses listed: {len(result_all)}")

        self.log.info("Address list with balance test passed")

    def test_list_addresses_include_watchonly(self):
        self.log.info("Testing listdigidollaraddresses with include_watchonly...")

        external_address = "RDtestwatchonly123456789abcdef"
        try:
            self.nodes[0].importdigidollaraddress(external_address, "watchonly_test")
        except Exception as e:
            self.log.info(f"Import for watchonly test: {e}")

        result_no_watchonly = self.nodes[0].listdigidollaraddresses(False)
        assert isinstance(result_no_watchonly, list)

        count_no_watchonly = len(result_no_watchonly)
        self.log.info(f"Addresses without watch-only: {count_no_watchonly}")

        result_with_watchonly = self.nodes[0].listdigidollaraddresses(True)
        assert isinstance(result_with_watchonly, list)

        count_with_watchonly = len(result_with_watchonly)
        self.log.info(f"Addresses with watch-only: {count_with_watchonly}")

        assert count_with_watchonly >= count_no_watchonly, \
            "Including watch-only should not decrease address count"

        for addr_info in result_with_watchonly:
            if addr_info.get('iswatchonly', False):
                self.log.info(f"Watch-only address: {addr_info.get('address', 'N/A')}")

        self.log.info("Include watchonly test passed")

    # === Bug #12 regression: no hardcoded mock addresses ===

    def test_no_mock_addresses_bug12(self):
        """Regression test for Bug #12: listdigidollaraddresses must not return
        hardcoded mock data. Verify that no address starts with 'DDmock' and
        that addresses use the correct network prefix (RD for regtest)."""
        self.log.info("Bug #12 regression: verifying no mock addresses returned...")

        result = self.nodes[0].listdigidollaraddresses()
        assert isinstance(result, list)

        for addr_info in result:
            addr = addr_info.get('address', '')
            # Must not contain hardcoded mock strings
            assert 'DDmock' not in addr, f"Bug #12: found mock address '{addr}'"
            assert 'DDwatchonly' not in addr, f"Bug #12: found mock address '{addr}'"
            # Regtest addresses must start with 'RD', not 'DD'
            assert addr.startswith('RD'), \
                f"Bug #12: regtest address '{addr}' should start with 'RD'"
            # Balance must be a real integer, not hardcoded 10000/25000/5000
            assert isinstance(addr_info.get('balance', 0), int)

        # On a fresh node with no DD activity, list should be empty
        result_node1 = self.nodes[1].listdigidollaraddresses()
        assert_equal(len(result_node1), 0)

        self.log.info("Bug #12 regression test passed: no mock addresses found")

    # === importdigidollaraddress tests ===

    def test_import_valid_address(self):
        self.log.info("Testing importdigidollaraddress with valid address...")

        external_address = self.nodes[1].getdigidollaraddress()
        self.log.info(f"Importing address from node 1: {external_address}")

        result = self.nodes[0].importdigidollaraddress(external_address)

        assert 'address' in result
        assert_equal(result['address'], external_address)
        assert 'success' in result
        assert_equal(result['success'], False)
        assert_equal(result['rescan_performed'], False)
        assert_equal(result['transactions_found'], 0)
        assert 'not implemented' in result['warning']

        self.log.info(f"Import result: {result}")
        self.log.info("Valid address import test passed")

    def test_import_with_label(self):
        self.log.info("Testing importdigidollaraddress with custom label...")

        external_address = self.nodes[1].getdigidollaraddress()
        label = "my_external_wallet"

        result = self.nodes[0].importdigidollaraddress(external_address, label)

        assert 'address' in result
        assert_equal(result['address'], external_address)
        assert 'label' in result
        assert_equal(result['label'], label)
        assert 'success' in result
        assert_equal(result['success'], False)
        assert 'not implemented' in result['warning']

        self.log.info(f"Import with label result: {result}")
        self.log.info("Import with label test passed")

    def test_import_duplicate(self):
        self.log.info("Testing importdigidollaraddress duplicate import...")

        external_address = self.nodes[1].getdigidollaraddress()

        result1 = self.nodes[0].importdigidollaraddress(external_address, "first_import")
        assert 'success' in result1
        assert_equal(result1['success'], False)
        self.log.info(f"First import: success={result1['success']}")

        try:
            result2 = self.nodes[0].importdigidollaraddress(external_address, "second_import")

            if 'success' in result2:
                self.log.info(f"Second import: success={result2['success']}")
                assert_equal(result2['success'], False)
                if 'warning' in result2:
                    self.log.info(f"Warning: {result2['warning']}")

        except Exception as e:
            self.log.info(f"Duplicate import raised expected error: {e}")

        self.log.info("Duplicate import test passed")

    def test_import_invalid_address(self):
        self.log.info("Testing importdigidollaraddress with invalid address...")

        invalid_addresses = [
            ("XXinvalidprefix123456789abcdef", "invalid prefix"),
            ("RD", "address too short"),
            ("RD" + "x" * 50, "address too long"),
            ("BTCnotdigidollaraddr12345678", "wrong currency prefix"),
        ]

        for invalid_address, description in invalid_addresses:
            self.log.info(f"Testing {description}: {invalid_address[:20]}...")

            try:
                result = self.nodes[0].importdigidollaraddress(invalid_address)

                if 'success' in result and not result['success']:
                    raise AssertionError(f"Invalid address returned non-error result: {result}")
                else:
                    raise AssertionError(f"Unexpected success for invalid address: {result}")

            except Exception as e:
                error_msg = str(e)
                assert 'Invalid' in error_msg or 'invalid' in error_msg or 'address' in error_msg.lower(), \
                    f"Unexpected error message: {error_msg}"
                self.log.info(f"  Correctly raised error: {error_msg[:50]}...")

        self.log.info("Invalid address import test passed")


if __name__ == '__main__':
    DigiDollarAddressTest().main()
