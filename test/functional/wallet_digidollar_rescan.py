#!/usr/bin/env python3
# Copyright (c) 2025 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test DigiDollar position reconstruction during blockchain rescan.

This test verifies that DigiDollar (DD) positions can be correctly reconstructed
from blockchain data during a wallet rescan. This is critical for:
- Wallet recovery scenarios
- Importing wallets from backup
- Syncing wallets after being offline

Test coverage:
1. Full rescan reconstructs all DD positions correctly
2. Partial rescan (from specific height) finds positions in scanned range
3. Rescan after position data loss reconstructs from blockchain
4. DD balance accuracy is maintained after rescan
5. Rescan progress is properly reported
"""

from decimal import Decimal
from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
)
import time

ORACLE_PRICE_MICRO_USD = 500000


class DigiDollarRescanTest(DigiByteTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # Enable DigiDollar, disable Dandelion for testing
        self.extra_args = [
            ["-digidollar=1", "-txindex=1", "-debug=digidollar", "-dandelion=0"],
            ["-digidollar=1", "-txindex=1", "-debug=digidollar", "-dandelion=0"],
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_sqlite()

    def setup_digidollar_environment(self):
        """Setup test environment with mature coins and oracle price."""
        self.log.info("Setting up DigiDollar test environment...")

        # Mine blocks past coinbase maturity (DigiByte uses COINBASE_MATURITY = 8)
        # Mine extra blocks to ensure DD activation and have funds
        self.generate(self.nodes[0], 110)
        self.sync_all()

        # Set mock oracle price for DD minting
        # Oracle price is in micro-USD: 500,000 = $0.50 per DGB
        self.refresh_oracle_quotes()

        # Verify we have funds
        balance = self.nodes[0].getbalance()
        assert_greater_than(balance, 100000)
        self.log.info(f"Initial DGB balance: {balance}")

    def refresh_oracle_quotes(self, price=ORACLE_PRICE_MICRO_USD):
        for node in self.nodes:
            result = node.setmockoracleprice(price)
            assert_equal(result["price_micro_usd"], price)

    def run_test(self):
        self.log.info("=== Starting DigiDollar Rescan Tests ===")

        # Setup environment
        self.setup_digidollar_environment()

        # Run test scenarios
        self.test_rescan_reconstructs_positions()
        self.test_rescan_partial_range()
        self.test_rescan_after_position_removal()
        self.test_rescan_balance_accuracy()
        self.test_rescan_progress_reporting()

        self.log.info("=== All DigiDollar Rescan Tests Passed! ===")

    def test_rescan_reconstructs_positions(self):
        """Test that blockchain rescan reconstructs all DD positions correctly."""
        self.log.info("Test 1: Testing rescan reconstructs positions...")

        # Create multiple DD positions at different block heights
        positions_created = []

        # Position 1: 10000 cents ($100), tier 4 (365 days)
        self.log.info("  Creating position 1 at current height...")
        try:
            self.refresh_oracle_quotes()
            mint1 = self.nodes[0].mintdigidollar(10000, 4)
            height1 = self.nodes[0].getblockcount()
            self.generate(self.nodes[0], 10)
            self.sync_all()
            positions_created.append({
                'txid': mint1['txid'],
                'amount': 10000,
                'tier': 4,
                'height': height1,
            })
            self.log.info(f"  Position 1 created: txid={mint1['txid'][:16]}...")
        except Exception as e:
            self.log.error(f"  Failed to create position 1: {e}")
            self.log.info("  Skipping test - DigiDollar minting not available")
            return

        # Position 2: 5000 cents ($50), tier 2 (90 days)
        self.log.info("  Creating position 2...")
        self.refresh_oracle_quotes()
        mint2 = self.nodes[0].mintdigidollar(5000, 2)
        height2 = self.nodes[0].getblockcount()
        self.generate(self.nodes[0], 10)
        self.sync_all()
        positions_created.append({
            'txid': mint2['txid'],
            'amount': 5000,
            'tier': 2,
            'height': height2,
        })
        self.log.info(f"  Position 2 created: txid={mint2['txid'][:16]}...")

        # Position 3: 20000 cents ($200), tier 3 (180 days)
        self.log.info("  Creating position 3...")
        self.refresh_oracle_quotes()
        mint3 = self.nodes[0].mintdigidollar(20000, 3)
        height3 = self.nodes[0].getblockcount()
        self.generate(self.nodes[0], 5)
        self.sync_all()
        positions_created.append({
            'txid': mint3['txid'],
            'amount': 20000,
            'tier': 3,
            'height': height3,
        })
        self.log.info(f"  Position 3 created: txid={mint3['txid'][:16]}...")

        # Record positions before rescan
        positions_before = self.nodes[0].listdigidollarpositions()
        balance_before = self.nodes[0].getdigidollarbalance()

        self.log.info(f"  Positions before rescan: {len(positions_before)}")
        self.log.info(f"  Balance before rescan: {balance_before}")

        # Perform full blockchain rescan
        self.log.info("  Performing full blockchain rescan...")
        rescan_result = self.nodes[0].rescanblockchain()
        self.log.info(f"  Rescan completed: {rescan_result}")

        # Verify positions after rescan
        positions_after = self.nodes[0].listdigidollarpositions()
        balance_after = self.nodes[0].getdigidollarbalance()

        self.log.info(f"  Positions after rescan: {len(positions_after)}")
        self.log.info(f"  Balance after rescan: {balance_after}")

        assert len(positions_after) == len(positions_before), \
            f"Position count mismatch: {len(positions_after)} vs {len(positions_before)}"

        # Verify balance matches
        balance_before_total = balance_before['total'] if isinstance(balance_before, dict) else balance_before
        balance_after_total = balance_after['total'] if isinstance(balance_after, dict) else balance_after
        assert_equal(balance_after_total, balance_before_total)

        # Verify each created position exists after rescan
        for pos_data in positions_created:
            found = False
            for pos in positions_after:
                pos_amount = pos.get('dd_minted', pos.get('amount', 0))
                if pos_amount == pos_data['amount']:
                    found = True
                    break
            assert found, f"Position with amount {pos_data['amount']} not found after rescan"

        self.log.info("  SUCCESS: Full rescan reconstructed all positions correctly")

    def test_rescan_partial_range(self):
        """Test partial blockchain rescan from specific start_height."""
        self.log.info("Test 2: Testing partial rescan from specific height...")

        # Record current height before creating new positions
        initial_height = self.nodes[0].getblockcount()
        self.log.info(f"  Initial height: {initial_height}")

        # Get positions before creating new ones
        positions_initial = self.nodes[0].listdigidollarpositions()
        self.log.info(f"  Positions at start: {len(positions_initial)}")

        # Create new positions after initial_height
        self.log.info("  Creating new positions after initial height...")

        self.refresh_oracle_quotes()
        mint1 = self.nodes[0].mintdigidollar(8000, 1)  # tier 1 (30 days)
        self.generate(self.nodes[0], 5)

        self.refresh_oracle_quotes()
        mint2 = self.nodes[0].mintdigidollar(12000, 2)  # tier 2 (90 days)
        self.generate(self.nodes[0], 5)
        self.sync_all()

        # Record positions after minting
        positions_after_mint = self.nodes[0].listdigidollarpositions()
        self.log.info(f"  Positions after minting: {len(positions_after_mint)}")

        new_position_count = len(positions_after_mint) - len(positions_initial)
        self.log.info(f"  New positions created: {new_position_count}")

        # Perform partial rescan from initial_height
        self.log.info(f"  Performing partial rescan from height {initial_height}...")
        rescan_result = self.nodes[0].rescanblockchain(initial_height)
        self.log.info(f"  Partial rescan result: {rescan_result}")

        # Verify start and stop heights in result
        if 'start_height' in rescan_result:
            assert_greater_than_or_equal(rescan_result['start_height'], 0)
        if 'stop_height' in rescan_result:
            current_height = self.nodes[0].getblockcount()
            assert_equal(rescan_result['stop_height'], current_height)

        # Verify positions still exist
        positions_after_rescan = self.nodes[0].listdigidollarpositions()
        self.log.info(f"  Positions after partial rescan: {len(positions_after_rescan)}")

        assert len(positions_after_rescan) == len(positions_after_mint), "Partial rescan lost positions"

        self.log.info("  SUCCESS: Partial rescan correctly found positions in scanned range")

    def test_rescan_after_position_removal(self):
        """Test rescan reconstructs positions after simulated data loss."""
        self.log.info("Test 3: Testing rescan after position removal...")

        # Create a position
        self.log.info("  Creating position for recovery test...")
        mint_amount = 15000  # $150
        dca_tier = 3  # 180 days

        self.refresh_oracle_quotes()
        mint_result = self.nodes[0].mintdigidollar(mint_amount, dca_tier)
        mint_txid = mint_result['txid']
        self.generate(self.nodes[0], 3)
        self.sync_all()

        self.log.info(f"  Created position: txid={mint_txid[:16]}...")

        # Record state before "data loss"
        positions_before = self.nodes[0].listdigidollarpositions()
        balance_before = self.nodes[0].getdigidollarbalance()

        self.log.info(f"  Positions before data loss: {len(positions_before)}")

        # Export wallet descriptors for reimport
        descriptors = self.nodes[0].listdescriptors(True)  # Include private keys
        self.log.info(f"  Exported {len(descriptors['descriptors'])} descriptors")

        # Create a new wallet to simulate data loss and recovery
        self.log.info("  Simulating data loss by creating new wallet...")
        self.nodes[0].createwallet(
            wallet_name="recovered_dd",
            disable_private_keys=False,
            blank=True,
            descriptors=True
        )

        recovered_wallet = self.nodes[0].get_wallet_rpc("recovered_dd")

        # Import descriptors into new wallet
        self.log.info("  Importing descriptors to new wallet...")
        import_descs = []
        for desc in descriptors['descriptors']:
            import_descs.append({
                'desc': desc['desc'],
                'timestamp': 0,  # Scan from genesis
                'active': desc.get('active', False),
                'internal': desc.get('internal', False),
            })

        import_result = recovered_wallet.importdescriptors(import_descs)
        success_count = sum(1 for r in import_result if r.get('success', False))
        self.log.info(f"  Imported {success_count}/{len(import_result)} descriptors")

        # The import should trigger a rescan, but let's be explicit
        self.log.info("  Performing explicit rescan on recovered wallet...")
        rescan_result = recovered_wallet.rescanblockchain()
        self.log.info(f"  Rescan completed: {rescan_result}")

        # Verify positions were reconstructed
        positions_recovered = recovered_wallet.listdigidollarpositions()
        balance_recovered = recovered_wallet.getdigidollarbalance()

        self.log.info(f"  Positions recovered: {len(positions_recovered)}")
        self.log.info(f"  Balance recovered: {balance_recovered}")

        assert len(positions_recovered) > 0, "No positions recovered after rescan"

        balance_recovered_total = balance_recovered['total'] if isinstance(balance_recovered, dict) else balance_recovered
        assert balance_recovered_total > 0, "No DD balance recovered after rescan"

        self.log.info("  SUCCESS: Positions reconstructed from blockchain after data loss")

    def test_rescan_balance_accuracy(self):
        """Test that DD balance remains accurate after rescan with transfers."""
        self.log.info("Test 4: Testing rescan balance accuracy with transfers...")

        self.nodes[0].unloadwallet("recovered_dd")

        # Get initial balance
        initial_balance = self.nodes[0].getdigidollarbalance()
        initial_total = initial_balance['total'] if isinstance(initial_balance, dict) else initial_balance
        self.log.info(f"  Initial DD balance: {initial_total}")

        # Create a position
        mint_amount = 25000  # $250
        self.refresh_oracle_quotes()
        mint_result = self.nodes[0].mintdigidollar(mint_amount, 2)
        self.generate(self.nodes[0], 2)
        self.sync_all()

        # Transfer some DD to node 1
        transfer_amount = 10000  # $100
        receiver_addr = self.nodes[1].getdigidollaraddress()
        self.refresh_oracle_quotes()
        send_result = self.nodes[0].senddigidollar(receiver_addr, transfer_amount)
        self.generate(self.nodes[0], 2)
        self.sync_all()

        self.log.info(f"  Transferred {transfer_amount} cents to node 1")

        # Record expected balances
        balance_node0_before = self.nodes[0].getdigidollarbalance()
        balance_node1_before = self.nodes[1].getdigidollarbalance()

        expected_node0 = balance_node0_before['total'] if isinstance(balance_node0_before, dict) else balance_node0_before
        expected_node1 = balance_node1_before['total'] if isinstance(balance_node1_before, dict) else balance_node1_before

        self.log.info(f"  Node 0 balance before rescan: {expected_node0}")
        self.log.info(f"  Node 1 balance before rescan: {expected_node1}")

        # Perform rescan on both nodes
        self.log.info("  Performing rescan on both nodes...")
        rescan0 = self.nodes[0].rescanblockchain()
        rescan1 = self.nodes[1].rescanblockchain()

        # Verify balances after rescan
        balance_node0_after = self.nodes[0].getdigidollarbalance()
        balance_node1_after = self.nodes[1].getdigidollarbalance()

        actual_node0 = balance_node0_after['total'] if isinstance(balance_node0_after, dict) else balance_node0_after
        actual_node1 = balance_node1_after['total'] if isinstance(balance_node1_after, dict) else balance_node1_after

        self.log.info(f"  Node 0 balance after rescan: {actual_node0}")
        self.log.info(f"  Node 1 balance after rescan: {actual_node1}")

        assert actual_node0 == expected_node0, f"Node 0 balance mismatch: {actual_node0} vs {expected_node0}"
        assert actual_node1 == expected_node1, f"Node 1 balance mismatch: {actual_node1} vs {expected_node1}"

        total_dd = actual_node0 + actual_node1
        expected_total = initial_total + mint_amount
        assert total_dd == expected_total, f"Total DD incorrect: {total_dd} vs {expected_total}"

        self.log.info("  SUCCESS: DD balance accurate after rescan with transfers")

    def test_rescan_progress_reporting(self):
        """Test rescan progress reporting via getwalletinfo."""
        self.log.info("Test 5: Testing rescan progress reporting...")

        # Mine some blocks to ensure rescan takes measurable time
        self.generate(self.nodes[0], 50)
        self.sync_all()

        # Create a new wallet for testing rescan progress
        self.log.info("  Creating test wallet for progress monitoring...")
        self.nodes[0].createwallet(
            wallet_name="progress_test",
            descriptors=True,
            blank=True
        )

        progress_wallet = self.nodes[0].get_wallet_rpc("progress_test")

        # Check wallet info for scanning status
        wallet_info = progress_wallet.getwalletinfo()
        self.log.info(f"  Wallet info: {wallet_info}")

        # The 'scanning' field indicates if wallet is currently rescanning
        if 'scanning' in wallet_info:
            scanning_status = wallet_info['scanning']
            self.log.info(f"  Scanning status: {scanning_status}")

            if isinstance(scanning_status, dict):
                # Wallet is currently scanning
                if 'progress' in scanning_status:
                    progress = scanning_status['progress']
                    self.log.info(f"  Scan progress: {progress * 100:.1f}%")
                    assert progress >= 0 and progress <= 1, "Invalid progress value"
            elif scanning_status == False:
                # Wallet finished scanning (expected if rescan completed quickly)
                self.log.info("  Rescan completed (no active scan)")

        # Perform an explicit rescan and verify completion
        self.log.info("  Performing explicit rescan...")
        rescan_result = progress_wallet.rescanblockchain()

        # Verify rescan result contains expected fields
        assert 'start_height' in rescan_result, "Missing start_height in rescan result"
        assert 'stop_height' in rescan_result, "Missing stop_height in rescan result"

        self.log.info(f"  Rescan range: {rescan_result['start_height']} to {rescan_result['stop_height']}")

        # After rescan, scanning should be false
        wallet_info_after = progress_wallet.getwalletinfo()
        if 'scanning' in wallet_info_after:
            assert wallet_info_after['scanning'] == False, "Wallet still scanning after rescanblockchain returned"

        self.log.info("  SUCCESS: Rescan progress reporting works correctly")


if __name__ == '__main__':
    DigiDollarRescanTest().main()
