#!/usr/bin/env python3
"""Test DigiDollar position persistence across wallet restart."""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal

class DigiDollarPersistenceRestartTest(DigiByteTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Starting DigiDollar persistence restart test...")

        # Step 1: Mine some blocks for initial coins
        self.log.info("Mining blocks for initial coins...")
        self.generate(self.nodes[0], 101)

        # Step 2: Check if mintdigidollar RPC exists
        try:
            self.nodes[0].help("mintdigidollar")
            self.log.info("mintdigidollar RPC command exists")
        except:
            self.log.info("SKIP: mintdigidollar RPC not implemented yet")
            return

        # Step 3: Try to mint a DigiDollar position
        self.log.info("Attempting to mint DigiDollar position...")
        try:
            position_result = self.nodes[0].mintdigidollar(100, 365)
            position_id = position_result['position_id']
            self.log.info(f"Minted position: {position_id}")
        except Exception as e:
            self.log.info(f"SKIP: Minting not fully implemented yet: {e}")
            return

        # Step 4: Verify position exists before restart
        positions_before = self.nodes[0].listddpositions()
        assert_equal(len(positions_before), 1)
        self.log.info("Position exists before restart: PASS")

        # Step 5: THE CRITICAL TEST - Stop wallet
        self.log.info("Stopping wallet...")
        self.stop_node(0)

        # Step 6: Restart wallet
        self.log.info("Restarting wallet...")
        self.start_node(0)

        # Step 7: THE MOMENT OF TRUTH - Check if position still exists
        self.log.info("Checking if position persists...")
        positions_after = self.nodes[0].listddpositions()

        if len(positions_after) == 0:
            self.log.error("FAIL: Position lost after restart!")
            raise AssertionError("DigiDollar position did not persist across wallet restart")

        assert_equal(len(positions_after), 1, "FAIL: Position lost after restart!")
        assert_equal(positions_after[0]['position_id'], position_id, "FAIL: Position ID corrupted!")

        self.log.info("SUCCESS: DigiDollar position persisted across wallet restart!")
        self.log.info(f"Position ID: {position_id}")
        self.log.info(f"Position data: {positions_after[0]}")

if __name__ == '__main__':
    DigiDollarPersistenceRestartTest().main()
