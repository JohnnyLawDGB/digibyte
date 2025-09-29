#!/usr/bin/env python3
"""Test DigiDollar basic functionality.

Test basic DigiDollar operations including:
- DD address generation and validation
- Basic mint/transfer/redeem cycle
- Balance tracking
- Integration with DigiByte blockchain
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    connect_nodes,
)
from decimal import Decimal


class DigiDollarBasicTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # Enable DigiDollar features and wallet
        self.extra_args = [
            ["-digidollar=1", "-mocktime=0"],
            ["-digidollar=1", "-mocktime=0"]
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar basic functionality...")

        # Test setup
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_address_generation()
        self.test_address_validation()
        self.test_basic_mint_cycle()
        self.test_balance_tracking()
        self.test_multi_node_sync()

    def setup_digidollar_test(self):
        """Setup test environment for DigiDollar."""
        # Generate initial blocks past coinbase maturity
        self.log.info("Generating initial blocks for test setup...")
        self.nodes[0].generate(110)
        self.sync_all()

        # Set mock oracle price ($0.50 per DGB)
        self.log.info("Setting mock oracle price...")
        self.nodes[0].setmockoracleprice(50000)  # 50000 satoshis per USD
        self.nodes[1].setmockoracleprice(50000)

        # Verify DigiDollar is active
        status = self.nodes[0].getdigidollarstatus()
        assert_equal(status["active"], True)

    def test_address_generation(self):
        """Test DigiDollar address generation."""
        self.log.info("Testing DD address generation...")

        # Generate new DD address
        dd_address = self.nodes[0].getdigidollaraddress()
        self.log.info(f"Generated DD address: {dd_address}")

        # Verify address format (should start with 'dd' for regtest)
        assert dd_address.startswith('dgbrt1dd'), f"DD address should start with 'dgbrt1dd', got: {dd_address}"

        # Generate multiple addresses and verify they're different
        dd_address2 = self.nodes[0].getdigidollaraddress()
        assert dd_address != dd_address2, "DD addresses should be unique"

        # Verify addresses are in wallet
        addresses = self.nodes[0].listdigidollaraddresses()
        assert dd_address in addresses, "DD address should be in wallet"
        assert dd_address2 in addresses, "Second DD address should be in wallet"

    def test_address_validation(self):
        """Test DigiDollar address validation."""
        self.log.info("Testing DD address validation...")

        # Generate valid address
        valid_address = self.nodes[0].getdigidollaraddress()

        # Test valid address
        validation = self.nodes[0].validateddaddress(valid_address)
        assert_equal(validation["isvalid"], True)
        assert_equal(validation["ismine"], True)

        # Test invalid addresses
        invalid_addresses = [
            "",
            "invalid",
            "dgb1qw508d6qejxtdg4y5r3zarvary0c5xw7k3hz6a0",  # Regular DGB address
            "dgbrt1dd0000000000000000000000000000000000000000000000000000000000000000000",  # Too long
            "dgbrt1cc" + "0" * 60  # Wrong prefix
        ]

        for invalid_addr in invalid_addresses:
            validation = self.nodes[0].validateddaddress(invalid_addr)
            assert_equal(validation["isvalid"], False, f"Address {invalid_addr} should be invalid")

    def test_basic_mint_cycle(self):
        """Test basic mint/transfer/redeem cycle."""
        self.log.info("Testing basic mint/transfer/redeem cycle...")

        # Get initial balances
        initial_dgb_balance = self.nodes[0].getbalance()
        initial_dd_balance = self.nodes[0].getdigidollarbalance()

        # Mint DigiDollars
        mint_amount = Decimal('1000.00')  # $1000 worth of DD
        lock_days = 365  # 1 year lock

        self.log.info(f"Minting {mint_amount} DD with {lock_days} day lock...")
        mint_result = self.nodes[0].mintdigidollar(str(mint_amount), lock_days)

        # Verify mint result
        assert 'txid' in mint_result, "Mint should return transaction ID"
        assert 'dd_address' in mint_result, "Mint should return DD address"
        assert 'collateral_required' in mint_result, "Mint should return collateral required"

        mint_txid = mint_result['txid']
        dd_address = mint_result['dd_address']

        # Mine a block to confirm the transaction
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balances after mint
        new_dgb_balance = self.nodes[0].getbalance()
        new_dd_balance = self.nodes[0].getdigidollarbalance()

        assert_greater_than(initial_dgb_balance, new_dgb_balance)  # DGB locked as collateral
        assert_equal(new_dd_balance, mint_amount)  # DD minted

        # Test transfer to node1
        node1_dd_address = self.nodes[1].getdigidollaraddress()
        transfer_amount = Decimal('100.00')

        self.log.info(f"Transferring {transfer_amount} DD to node1...")
        transfer_result = self.nodes[0].senddigidollar(node1_dd_address, str(transfer_amount))

        assert 'txid' in transfer_result, "Transfer should return transaction ID"
        transfer_txid = transfer_result['txid']

        # Mine a block to confirm
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balances after transfer
        node0_dd_balance = self.nodes[0].getdigidollarbalance()
        node1_dd_balance = self.nodes[1].getdigidollarbalance()

        expected_node0_balance = mint_amount - transfer_amount
        assert_equal(node0_dd_balance, expected_node0_balance)
        assert_equal(node1_dd_balance, transfer_amount)

        # Test partial redemption from node1
        redeem_amount = Decimal('50.00')

        self.log.info(f"Redeeming {redeem_amount} DD from node1...")
        redeem_result = self.nodes[1].redeemdigidollar(str(redeem_amount))

        assert 'txid' in redeem_result, "Redemption should return transaction ID"
        redeem_txid = redeem_result['txid']

        # Mine a block to confirm
        self.nodes[1].generate(1)
        self.sync_all()

        # Verify final balances
        final_node1_dd_balance = self.nodes[1].getdigidollarbalance()
        final_node1_dgb_balance = self.nodes[1].getbalance()

        expected_final_dd = transfer_amount - redeem_amount
        assert_equal(final_node1_dd_balance, expected_final_dd)

        # Verify DGB was received (should be approximately redeem_amount * oracle_price)
        # Account for transaction fees
        assert_greater_than(final_node1_dgb_balance, Decimal('0'))

    def test_balance_tracking(self):
        """Test DigiDollar balance tracking and position management."""
        self.log.info("Testing DD balance tracking...")

        # Create multiple DD positions with different lock periods
        positions_data = [
            {"amount": Decimal('500.00'), "lock_days": 30},
            {"amount": Decimal('1000.00'), "lock_days": 180},
            {"amount": Decimal('2000.00'), "lock_days": 730}
        ]

        for position in positions_data:
            self.log.info(f"Creating position: {position['amount']} DD, {position['lock_days']} days")
            result = self.nodes[0].mintdigidollar(str(position['amount']), position['lock_days'])
            position['txid'] = result['txid']
            position['address'] = result['dd_address']

        # Mine blocks to confirm
        self.nodes[0].generate(3)
        self.sync_all()

        # Verify total balance
        total_expected = sum(pos['amount'] for pos in positions_data)
        actual_balance = self.nodes[0].getdigidollarbalance()
        assert_equal(actual_balance, total_expected)

        # Test position listing
        positions = self.nodes[0].listdigidollarpositions()
        assert_equal(len(positions), len(positions_data))

        # Verify position details
        for position in positions:
            assert 'amount' in position
            assert 'lock_height' in position
            assert 'collateral_locked' in position
            assert 'dd_address' in position
            assert 'status' in position

        # Test transaction history
        transactions = self.nodes[0].listdigidollartxs()
        mint_txs = [tx for tx in transactions if tx['category'] == 'mint']
        assert_equal(len(mint_txs), len(positions_data))

    def test_multi_node_sync(self):
        """Test DigiDollar state synchronization across nodes."""
        self.log.info("Testing multi-node DD state synchronization...")

        # Ensure nodes are connected and synced
        connect_nodes(self.nodes[0], self.nodes[1])
        self.sync_all()

        # Verify both nodes have same DD system status
        status0 = self.nodes[0].getdigidollarstatus()
        status1 = self.nodes[1].getdigidollarstatus()

        assert_equal(status0['active'], status1['active'])
        assert_equal(status0['system_health'], status1['system_health'])
        assert_equal(status0['oracle_price'], status1['oracle_price'])

        # Perform transaction on node0
        mint_amount = Decimal('250.00')
        result = self.nodes[0].mintdigidollar(str(mint_amount), 90)

        # Mine block and sync
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify transaction is visible on both nodes
        tx_details0 = self.nodes[0].gettransaction(result['txid'])
        tx_details1 = self.nodes[1].gettransaction(result['txid'])

        assert_equal(tx_details0['txid'], tx_details1['txid'])
        assert_equal(tx_details0['confirmations'], tx_details1['confirmations'])

        # Verify DD stats are synchronized
        stats0 = self.nodes[0].getdigidollarstats()
        stats1 = self.nodes[1].getdigidollarstats()

        assert_equal(stats0['total_supply'], stats1['total_supply'])
        assert_equal(stats0['total_collateral'], stats1['total_collateral'])


if __name__ == '__main__':
    DigiDollarBasicTest().main()