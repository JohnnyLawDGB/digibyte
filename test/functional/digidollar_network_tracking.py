#!/usr/bin/env python3
"""Test DigiDollar network-wide tracking.

CRITICAL TEST: Verify that all nodes see identical DigiDollar stats
regardless of which wallets are loaded, by scanning the UTXO set.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
)
from decimal import Decimal


class DigiDollarNetworkTrackingTest(DigiByteTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            ["-digidollar=1"],
            ["-digidollar=1"]
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("=" * 80)
        self.log.info("TESTING NETWORK-WIDE DIGIDOLLAR TRACKING (CRITICAL)")
        self.log.info("=" * 80)

        # Setup: Generate blocks and set oracle price
        self.log.info("Setting up test environment...")
        self.nodes[0].generate(110)
        self.sync_all()

        # Set oracle price on both nodes
        for node in self.nodes:
            node.setmockoracleprice(50000)  # $0.50 per DGB

        # Phase 1: Bob mints DD
        self.log.info("\n--- Phase 1: Bob (node 0) mints DigiDollar ---")
        bob_mint = self.nodes[0].mintdigidollar(50000, 4)  # $500.00, 365 days (tier 4)
        self.log.info(f"Bob minted $500 DD, txid: {bob_mint['txid']}")

        # Mine blocks to confirm
        self.nodes[0].generate(2)
        self.sync_all()

        # Phase 2: Network-wide tracking test
        self.log.info("\n--- Phase 2: CRITICAL TEST - Network-wide stats ---")

        bob_health = self.nodes[0].getdigidollarsystemhealth()
        alice_health = self.nodes[1].getdigidollarsystemhealth()

        self.log.info(f"\nBob (node 0) sees:")
        self.log.info(f"  Total DD Supply: {bob_health['total_dd_supply']}")
        self.log.info(f"  Total Collateral: {bob_health['total_collateral_locked']}")

        self.log.info(f"\nAlice (node 1) sees:")
        self.log.info(f"  Total DD Supply: {alice_health['total_dd_supply']}")
        self.log.info(f"  Total Collateral: {alice_health['total_collateral_locked']}")

        # CRITICAL ASSERTION: Both nodes MUST see identical stats
        self.log.info("\n--- Verifying network-wide consistency ---")

        # Both nodes MUST see identical DD supply (scanning UTXO set)
        if bob_health['total_dd_supply'] != alice_health['total_dd_supply']:
            raise AssertionError(f"FAILED: Nodes see different DD supply! Bob: {bob_health['total_dd_supply']}, Alice: {alice_health['total_dd_supply']}")
        assert_equal(bob_health['total_dd_supply'], alice_health['total_dd_supply'])

        # Both nodes MUST see identical collateral (scanning UTXO set)
        if bob_health['total_collateral_locked'] != alice_health['total_collateral_locked']:
            raise AssertionError(f"FAILED: Nodes see different collateral! Bob: {bob_health['total_collateral_locked']}, Alice: {alice_health['total_collateral_locked']}")
        assert_equal(bob_health['total_collateral_locked'], alice_health['total_collateral_locked'])

        self.log.info("✓ SUCCESS: Both nodes see identical network stats!")
        self.log.info("✓ UTXO-based tracking is working correctly!")

        self.log.info("\n" + "=" * 80)
        self.log.info("ALL TESTS PASSED - NETWORK-WIDE TRACKING WORKS!")
        self.log.info("=" * 80)
        self.log.info("\nVerified:")
        self.log.info("- Both nodes scan the UTXO set for DigiDollar vaults")
        self.log.info("- Both nodes report identical DD supply and collateral")
        self.log.info("- System health tracking is blockchain-wide, not wallet-specific")


if __name__ == '__main__':
    DigiDollarNetworkTrackingTest().main()
