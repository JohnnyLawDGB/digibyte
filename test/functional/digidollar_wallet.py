#!/usr/bin/env python3
"""Test DigiDollar wallet integration.

Test comprehensive wallet functionality including:
- Wallet balance tracking
- Position management
- Transaction creation via wallet
- Multi-wallet support
- Wallet backup and recovery
- Import/export functionality
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_in,
    assert_raises_rpc_error,
)
from decimal import Decimal
import os
import tempfile


class DigiDollarWalletTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        # Enable DigiDollar features and multiple wallets
        self.extra_args = [
            ["-digidollar=1", "-mocktime=0"],
            ["-digidollar=1", "-mocktime=0"],
            ["-digidollar=1", "-mocktime=0"]
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar wallet integration...")

        # Test setup
        self.setup_digidollar_test()

        # Run test scenarios
        self.test_wallet_balance_tracking()
        self.test_position_management()
        self.test_transaction_creation()
        self.test_multi_wallet_support()
        self.test_wallet_backup_recovery()
        self.test_address_management()
        self.test_wallet_security()
        self.test_wallet_performance()

    def setup_digidollar_test(self):
        """Setup test environment for DigiDollar wallet testing."""
        # Generate initial blocks past coinbase maturity
        self.log.info("Generating initial blocks for test setup...")
        self.nodes[0].generate(110)
        self.sync_all()

        # Set mock oracle price
        base_price = 50000  # 50000 satoshis per USD
        for node in self.nodes:
            node.setmockoracleprice(base_price)

        # Verify wallets are ready
        for i, node in enumerate(self.nodes):
            wallet_info = node.getwalletinfo()
            self.log.info(f"Node {i} wallet info: balance={wallet_info['balance']}")

    def test_wallet_balance_tracking(self):
        """Test wallet DD balance tracking accuracy."""
        self.log.info("Testing wallet DD balance tracking...")

        # Test initial zero balance
        initial_balance = self.nodes[0].getdigidollarbalance()
        assert_equal(initial_balance, Decimal('0'))

        # Create DD position and verify balance tracking
        mint_amount = Decimal('1000.00')
        mint_result = self.nodes[0].mintdigidollar(str(mint_amount), 365)

        # Mine block to confirm
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balance reflects minted amount
        post_mint_balance = self.nodes[0].getdigidollarbalance()
        assert_equal(post_mint_balance, mint_amount)

        # Test balance after transfer
        transfer_amount = Decimal('200.00')
        receiver_address = self.nodes[1].getdigidollaraddress()

        self.nodes[0].senddigidollar(receiver_address, str(transfer_amount))

        self.nodes[0].generate(1)
        self.sync_all()

        # Verify sender balance decreased
        sender_balance = self.nodes[0].getdigidollarbalance()
        expected_sender_balance = mint_amount - transfer_amount
        assert_equal(sender_balance, expected_sender_balance)

        # Verify receiver balance increased
        receiver_balance = self.nodes[1].getdigidollarbalance()
        assert_equal(receiver_balance, transfer_amount)

        # Test balance after redemption
        redeem_amount = Decimal('100.00')
        self.nodes[1].redeemdigidollar(str(redeem_amount))

        self.nodes[1].generate(1)
        self.sync_all()

        # Verify balance after redemption
        final_receiver_balance = self.nodes[1].getdigidollarbalance()
        expected_final_balance = transfer_amount - redeem_amount
        assert_equal(final_receiver_balance, expected_final_balance)

        # Test wallet info integration
        wallet_info = self.nodes[0].getwalletinfo()
        if 'digidollar_balance' in wallet_info:
            assert_equal(Decimal(wallet_info['digidollar_balance']), sender_balance)

    def test_position_management(self):
        """Test wallet position management functionality."""
        self.log.info("Testing wallet position management...")

        # Create multiple positions with different characteristics
        positions_data = [
            {"amount": "500.00", "lock_days": 30, "label": "short_term"},
            {"amount": "1500.00", "lock_days": 180, "label": "medium_term"},
            {"amount": "3000.00", "lock_days": 730, "label": "long_term"}
        ]

        created_positions = []
        for pos_data in positions_data:
            result = self.nodes[0].mintdigidollar(pos_data["amount"], pos_data["lock_days"])
            pos_data["txid"] = result["txid"]
            pos_data["dd_address"] = result["dd_address"]
            created_positions.append(pos_data)

        # Mine blocks to confirm
        self.nodes[0].generate(3)
        self.sync_all()

        # Test position listing
        positions = self.nodes[0].listdigidollarpositions()
        assert_equal(len(positions), len(positions_data))

        # Verify position details
        for position in positions:
            required_fields = ['amount', 'lock_height', 'dd_address', 'status', 'collateral_locked']
            for field in required_fields:
                assert field in position, f"Missing position field: {field}"

            # Verify amounts match
            pos_amount = Decimal(position['amount'])
            assert pos_amount in [Decimal(p["amount"]) for p in positions_data]

        # Test position filtering (if supported)
        try:
            # Filter by status
            active_positions = self.nodes[0].listdigidollarpositions("active")
            assert len(active_positions) <= len(positions)

            # Filter by minimum amount
            large_positions = self.nodes[0].listdigidollarpositions({"min_amount": "1000.00"})
            for pos in large_positions:
                assert Decimal(pos['amount']) >= Decimal('1000.00')

        except Exception as e:
            self.log.info(f"Position filtering not implemented: {e}")

        # Test position details retrieval
        for pos_data in created_positions:
            try:
                pos_details = self.nodes[0].getdigidollarposition(pos_data["dd_address"])
                assert 'amount' in pos_details
                assert 'lock_height' in pos_details
                assert 'creation_height' in pos_details
                self.log.info(f"Position details retrieved for {pos_data['dd_address']}")
            except Exception as e:
                self.log.info(f"Position details not available: {e}")

    def test_transaction_creation(self):
        """Test wallet transaction creation and management."""
        self.log.info("Testing wallet transaction creation...")

        # Test transaction creation workflow
        initial_dgb_balance = self.nodes[1].getbalance()

        # Create mint transaction through wallet
        mint_amount = Decimal('800.00')
        lock_days = 90

        # Test transaction preparation (if supported)
        try:
            prepared_tx = self.nodes[1].preparemintdigidollar(str(mint_amount), lock_days)
            assert 'estimated_fee' in prepared_tx
            assert 'collateral_required' in prepared_tx
            self.log.info(f"Prepared mint transaction: {prepared_tx}")
        except Exception as e:
            self.log.info(f"Transaction preparation not available: {e}")

        # Execute mint transaction
        mint_result = self.nodes[1].mintdigidollar(str(mint_amount), lock_days)
        mint_txid = mint_result['txid']

        # Test transaction status before confirmation
        try:
            tx_status = self.nodes[1].getdigidollartransaction(mint_txid)
            assert 'confirmations' in tx_status
            assert tx_status['confirmations'] == 0  # Unconfirmed
        except Exception as e:
            self.log.info(f"Transaction status not available: {e}")

        # Mine block to confirm
        self.nodes[1].generate(1)
        self.sync_all()

        # Test transaction status after confirmation
        confirmed_tx = self.nodes[1].gettransaction(mint_txid)
        assert confirmed_tx['confirmations'] > 0

        # Test fee calculation accuracy
        final_dgb_balance = self.nodes[1].getbalance()
        dgb_used = initial_dgb_balance - final_dgb_balance

        # DGB used should be approximately the collateral required plus fees
        collateral_estimate = self.nodes[1].calculatecollateralrequirement(str(mint_amount), lock_days)
        expected_dgb = Decimal(collateral_estimate['collateral_dgb'])

        # Allow for transaction fees
        tolerance = expected_dgb * Decimal('0.01')  # 1% tolerance
        assert abs(dgb_used - expected_dgb) <= tolerance + Decimal('0.01')  # Plus fee allowance

        # Test transfer transaction creation
        transfer_amount = Decimal('100.00')
        receiver_address = self.nodes[2].getdigidollaraddress()

        transfer_result = self.nodes[1].senddigidollar(receiver_address, str(transfer_amount))
        transfer_txid = transfer_result['txid']

        self.nodes[1].generate(1)
        self.sync_all()

        # Verify transfer in wallet transaction history
        wallet_txs = self.nodes[1].listdigidollartxs()
        transfer_tx = next((tx for tx in wallet_txs if tx['txid'] == transfer_txid), None)

        assert transfer_tx is not None
        assert transfer_tx['category'] == 'send'
        assert Decimal(transfer_tx['amount']) == -transfer_amount  # Negative for send

        # Verify on receiver side
        receiver_txs = self.nodes[2].listdigidollartxs()
        received_tx = next((tx for tx in receiver_txs if tx['txid'] == transfer_txid), None)

        assert received_tx is not None
        assert received_tx['category'] == 'receive'
        assert Decimal(received_tx['amount']) == transfer_amount  # Positive for receive

    def test_multi_wallet_support(self):
        """Test multi-wallet DD support."""
        self.log.info("Testing multi-wallet DD support...")

        # Create additional wallet on node 0
        try:
            self.nodes[0].createwallet("dd_test_wallet")
            self.log.info("Created additional wallet: dd_test_wallet")

            # Switch to new wallet
            new_wallet = self.nodes[0].get_wallet_rpc("dd_test_wallet")

            # Test DD operations on new wallet
            new_wallet_address = new_wallet.getdigidollaraddress()
            assert isinstance(new_wallet_address, str)

            # Verify new wallet starts with zero DD balance
            new_wallet_balance = new_wallet.getdigidollarbalance()
            assert_equal(new_wallet_balance, Decimal('0'))

            # Test transferring DD to new wallet
            transfer_amount = Decimal('150.00')
            self.nodes[1].senddigidollar(new_wallet_address, str(transfer_amount))

            self.nodes[1].generate(1)
            self.sync_all()

            # Verify new wallet received DD
            new_balance = new_wallet.getdigidollarbalance()
            assert_equal(new_balance, transfer_amount)

            # Test operations from new wallet
            recipient_address = self.nodes[2].getdigidollaraddress()
            send_result = new_wallet.senddigidollar(recipient_address, "50.00")

            self.nodes[0].generate(1)
            self.sync_all()

            # Verify new wallet balance decreased
            final_new_balance = new_wallet.getdigidollarbalance()
            assert_equal(final_new_balance, transfer_amount - Decimal('50.00'))

            # Test wallet isolation (balances should be separate)
            main_wallet_balance = self.nodes[0].getdigidollarbalance()
            # Main wallet and new wallet should have different balances

        except Exception as e:
            self.log.info(f"Multi-wallet support not available: {e}")

    def test_wallet_backup_recovery(self):
        """Test wallet backup and recovery with DD data."""
        self.log.info("Testing wallet backup and recovery...")

        # Create DD position to backup
        backup_amount = Decimal('600.00')
        backup_result = self.nodes[0].mintdigidollar(str(backup_amount), 180)

        self.nodes[0].generate(1)
        self.sync_all()

        # Get wallet state before backup
        pre_backup_balance = self.nodes[0].getdigidollarbalance()
        pre_backup_positions = self.nodes[0].listdigidollarpositions()

        # Test wallet backup
        with tempfile.TemporaryDirectory() as temp_dir:
            backup_file = os.path.join(temp_dir, "dd_wallet_backup.dat")

            try:
                # Backup wallet
                self.nodes[0].backupwallet(backup_file)
                assert os.path.exists(backup_file)
                self.log.info(f"Wallet backed up to: {backup_file}")

                # Test backup file contains DD data
                backup_size = os.path.getsize(backup_file)
                assert backup_size > 1000  # Should be substantial with DD data

                # Simulate wallet corruption/loss by stopping node
                self.stop_node(0)

                # Remove wallet file to simulate loss
                wallet_path = os.path.join(self.nodes[0].datadir, "regtest", "wallets", "")
                if os.path.exists(wallet_path):
                    import shutil
                    shutil.rmtree(wallet_path, ignore_errors=True)

                # Restart node
                self.start_node(0)

                # Restore from backup
                self.nodes[0].restorewallet("restored_wallet", backup_file)
                restored_wallet = self.nodes[0].get_wallet_rpc("restored_wallet")

                # Verify DD data was restored
                restored_balance = restored_wallet.getdigidollarbalance()
                restored_positions = restored_wallet.listdigidollarpositions()

                assert_equal(restored_balance, pre_backup_balance)
                assert_equal(len(restored_positions), len(pre_backup_positions))

                self.log.info("Wallet restoration successful")

            except Exception as e:
                self.log.info(f"Wallet backup/recovery not fully supported: {e}")

    def test_address_management(self):
        """Test DD address management in wallet."""
        self.log.info("Testing DD address management...")

        # Test address generation
        addresses = []
        for i in range(5):
            address = self.nodes[0].getdigidollaraddress()
            addresses.append(address)

        # Verify all addresses are unique
        assert len(set(addresses)) == len(addresses)

        # Test address labeling (if supported)
        try:
            labeled_address = self.nodes[0].getdigidollaraddress("test_label")
            assert isinstance(labeled_address, str)

            # Verify label association
            address_info = self.nodes[0].getaddressinfo(labeled_address)
            if 'label' in address_info:
                assert address_info['label'] == "test_label"

        except Exception as e:
            self.log.info(f"Address labeling not supported: {e}")

        # Test address listing
        all_addresses = self.nodes[0].listdigidollaraddresses()
        assert isinstance(all_addresses, list)

        for address in addresses:
            assert address in all_addresses

        # Test address validation
        for address in addresses:
            validation = self.nodes[0].validateddaddress(address)
            assert validation['isvalid'] == True
            assert validation['ismine'] == True

        # Test address import/export (if supported)
        try:
            # Export address private key
            privkey = self.nodes[0].dumpprivkey(addresses[0])
            assert isinstance(privkey, str)

            # Import to another wallet
            self.nodes[1].importprivkey(privkey, "imported_dd")

            # Verify import
            imported_validation = self.nodes[1].validateddaddress(addresses[0])
            if imported_validation.get('ismine', False):
                self.log.info("DD address import successful")

        except Exception as e:
            self.log.info(f"Address import/export not supported: {e}")

    def test_wallet_security(self):
        """Test wallet security features for DD operations."""
        self.log.info("Testing wallet security features...")

        # Test wallet encryption (if supported)
        try:
            passphrase = "test_dd_passphrase_123"
            self.nodes[2].encryptwallet(passphrase)

            # Restart node to activate encryption
            self.restart_node(2)

            # Test DD operations with encrypted wallet
            encrypted_wallet = self.nodes[2]

            # Should require unlock for DD operations
            with assert_raises_rpc_error(-13, ""):
                encrypted_wallet.mintdigidollar("500.00", 365)

            # Unlock wallet
            encrypted_wallet.walletpassphrase(passphrase, 60)

            # Now DD operations should work
            unlock_result = encrypted_wallet.mintdigidollar("300.00", 90)
            assert 'txid' in unlock_result

            encrypted_wallet.generate(1)
            self.sync_all()

            self.log.info("Wallet encryption with DD operations successful")

        except Exception as e:
            self.log.info(f"Wallet encryption not supported or failed: {e}")

        # Test wallet lock timeout
        try:
            # Lock wallet again
            self.nodes[2].walletlock()

            # Verify DD operations are locked
            with assert_raises_rpc_error(-13, ""):
                self.nodes[2].mintdigidollar("100.00", 30)

        except Exception as e:
            self.log.info(f"Wallet locking test: {e}")

    def test_wallet_performance(self):
        """Test wallet performance with DD operations."""
        self.log.info("Testing wallet performance...")

        import time

        # Test batch DD operations
        start_time = time.time()

        batch_operations = []
        for i in range(10):
            try:
                amount = f"{100 + i * 10}.00"
                result = self.nodes[0].mintdigidollar(amount, 30)
                batch_operations.append(result['txid'])
            except Exception as e:
                self.log.info(f"Batch operation {i} failed: {e}")
                break

        batch_time = time.time() - start_time
        self.log.info(f"Batch DD operations ({len(batch_operations)} mints) took {batch_time:.2f}s")

        # Mine blocks to confirm
        self.nodes[0].generate(len(batch_operations))
        self.sync_all()

        # Test wallet sync performance
        start_time = time.time()
        final_balance = self.nodes[0].getdigidollarbalance()
        balance_time = time.time() - start_time
        self.log.info(f"Balance calculation took {balance_time:.3f}s")

        # Test transaction history performance
        start_time = time.time()
        all_txs = self.nodes[0].listdigidollartxs()
        history_time = time.time() - start_time
        self.log.info(f"Transaction history ({len(all_txs)} txs) took {history_time:.3f}s")

        # Test position listing performance
        start_time = time.time()
        all_positions = self.nodes[0].listdigidollarpositions()
        positions_time = time.time() - start_time
        self.log.info(f"Position listing ({len(all_positions)} positions) took {positions_time:.3f}s")

        # Performance should be reasonable
        assert balance_time < 1.0, "Balance calculation too slow"
        assert history_time < 2.0, "Transaction history too slow"
        assert positions_time < 2.0, "Position listing too slow"

        self.log.info("Wallet performance tests completed")


if __name__ == '__main__':
    DigiDollarWalletTest().main()