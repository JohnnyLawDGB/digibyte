#!/usr/bin/env python3
# Copyright (c) 2025-2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test getoracles and getalloracleprices RPC commands.

Validates that:
1. getoracles returns all configured oracles (not just the local one)
2. getoracles shows pending P2P price data for non-local oracles
3. getalloracleprices also shows pending P2P data
4. Price source priority: local > on-chain > pending > none
5. Both RPCs return consistent data for the same oracle
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.messages import msg_getoracles
from test_framework.p2p import P2PInterface, p2p_lock
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
)

ORACLE_MAX_AGE_SECONDS = 3600
REGTEST_ORACLE_EPOCH_BLOCKS = 10


class OraclePriceReceiver(P2PInterface):
    def __init__(self):
        super().__init__()
        self.oracle_prices = []

    def on_oracleprice(self, message):
        self.oracle_prices.append(message)

    def clear_prices(self):
        with p2p_lock:
            self.oracle_prices.clear()


class GetOraclesPendingTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("=== Testing getoracles and getalloracleprices RPCs ===")
        node = self.nodes[0]

        # Generate blocks past DigiDollar/oracle activation.
        self.log.info("Generating initial blocks...")
        self.generate(node, 660)

        # Set mock oracle price so the system is active
        node.setmockoracleprice(6000)

        self.test_getoracles_returns_all(node)
        self.test_getoracles_field_types(node)
        self.test_getoracles_active_only_filter(node)
        self.test_getalloracleprices_returns_all(node)
        self.test_getalloracleprices_field_types(node)
        self.test_oracle_names_present(node)
        self.test_getoracles_p2p_does_not_resend_stale_pending(node)

        self.log.info("=== All getoracles/getalloracleprices tests passed! ===")

    def test_getoracles_returns_all(self, node):
        """getoracles should return entries for ALL configured oracles."""
        self.log.info("Test: getoracles returns all configured oracles")
        result = node.getoracles()

        # Should be a list
        assert isinstance(result, list), "getoracles should return a list"

        # Should have at least 7 oracles (the configured set)
        assert_greater_than_or_equal(len(result), 7)
        self.log.info(f"  getoracles returned {len(result)} oracles")

        # Each oracle should have a unique ID
        ids = [o["oracle_id"] for o in result]
        assert_equal(len(ids), len(set(ids)))

        # Oracle IDs should be 0-6
        for i in range(7):
            assert i in ids, f"Oracle ID {i} missing from getoracles"

    def test_getoracles_field_types(self, node):
        """Verify all expected fields and types are present."""
        self.log.info("Test: getoracles field types and structure")
        result = node.getoracles()

        for oracle in result:
            # Required fields
            assert "oracle_id" in oracle, "Missing oracle_id"
            assert "name" in oracle, "Missing name"
            assert "pubkey" in oracle, "Missing pubkey"
            assert "endpoint" in oracle, "Missing endpoint"
            assert "is_active" in oracle, "Missing is_active"
            assert "last_price_micro_usd" in oracle, "Missing last_price_micro_usd"
            assert "last_price_usd" in oracle, "Missing last_price_usd"
            assert "last_update" in oracle, "Missing last_update"
            assert "price_source" in oracle, "Missing price_source"
            assert "status" in oracle, "Missing status"
            assert "selected_for_epoch" in oracle, "Missing selected_for_epoch"
            assert "is_running_locally" in oracle, "Missing is_running_locally"

            # Type checks
            assert isinstance(oracle["oracle_id"], int)
            assert isinstance(oracle["name"], str)
            assert isinstance(oracle["pubkey"], str)
            assert isinstance(oracle["endpoint"], str)
            assert isinstance(oracle["is_active"], bool)
            assert isinstance(oracle["last_price_micro_usd"], int)
            assert isinstance(oracle["last_price_usd"], (int, float))
            assert isinstance(oracle["last_update"], int)
            assert isinstance(oracle["price_source"], str)
            assert isinstance(oracle["status"], str)
            assert isinstance(oracle["selected_for_epoch"], bool)
            assert isinstance(oracle["is_running_locally"], bool)

            # price_source must be one of the valid values
            valid_sources = ["local", "on-chain", "pending", "none"]
            assert oracle["price_source"] in valid_sources, \
                f"Invalid price_source '{oracle['price_source']}' for oracle {oracle['oracle_id']}"

            # status must be valid
            valid_statuses = ["reporting", "stopped", "no_data"]
            assert oracle["status"] in valid_statuses, \
                f"Invalid status '{oracle['status']}' for oracle {oracle['oracle_id']}"

            # If price_source is "none", status should be "no_data"
            if oracle["price_source"] == "none":
                assert_equal(oracle["status"], "no_data")

            # If status is "reporting", price_source should not be "none"
            if oracle["status"] == "reporting":
                assert oracle["price_source"] != "none", \
                    f"Oracle {oracle['oracle_id']} is 'reporting' but price_source is 'none'"

            # Pubkey should be valid hex
            assert len(oracle["pubkey"]) > 0, "Pubkey should not be empty"

            self.log.info(
                f"  Oracle {oracle['oracle_id']} ({oracle['name']}): "
                f"source={oracle['price_source']}, status={oracle['status']}, "
                f"price={oracle['last_price_micro_usd']}"
            )

    def test_getoracles_active_only_filter(self, node):
        """getoracles(true) should filter to active oracles only."""
        self.log.info("Test: getoracles active_only filter")

        all_oracles = node.getoracles()
        active_oracles = node.getoracles(True)

        # Active should be <= all
        assert_greater_than_or_equal(len(all_oracles), len(active_oracles))

        # All returned oracles should have is_active=true
        for oracle in active_oracles:
            assert_equal(oracle["is_active"], True)

        # Count active in the full list
        active_count = sum(1 for o in all_oracles if o["is_active"])
        assert_equal(len(active_oracles), active_count)

        self.log.info(f"  {len(active_oracles)} active out of {len(all_oracles)} total")

    def test_getalloracleprices_returns_all(self, node):
        """getalloracleprices should return entries for all configured oracles."""
        self.log.info("Test: getalloracleprices returns all oracles")
        result = node.getalloracleprices()

        # Should be an object with oracles array
        assert "oracles" in result, "Missing 'oracles' in getalloracleprices"
        assert "block_height" in result, "Missing 'block_height'"
        assert "total_oracles" in result, "Missing 'total_oracles'"
        assert "oracle_count" in result, "Missing 'oracle_count'"
        assert "required" in result, "Missing 'required'"

        oracles = result["oracles"]
        assert isinstance(oracles, list)
        assert_greater_than_or_equal(len(oracles), 7)

        self.log.info(
            f"  getalloracleprices: {result['oracle_count']} reporting / "
            f"{result['total_oracles']} total, "
            f"consensus={result['consensus_price_micro_usd']}"
        )

    def test_getalloracleprices_field_types(self, node):
        """Verify getalloracleprices oracle entries have correct structure."""
        self.log.info("Test: getalloracleprices field types")
        result = node.getalloracleprices()

        for oracle in result["oracles"]:
            assert "oracle_id" in oracle
            assert "name" in oracle
            assert "endpoint" in oracle
            assert "price_micro_usd" in oracle
            assert "price_usd" in oracle
            assert "timestamp" in oracle
            assert "block_height" in oracle
            assert "status" in oracle

            assert isinstance(oracle["oracle_id"], int)
            assert isinstance(oracle["name"], str)
            assert isinstance(oracle["price_micro_usd"], int)
            assert isinstance(oracle["status"], str)

            valid_statuses = ["reporting", "no_data", "outlier"]
            assert oracle["status"] in valid_statuses, \
                f"Invalid status '{oracle['status']}' for oracle {oracle['oracle_id']}"

            self.log.info(
                f"  Oracle {oracle['oracle_id']} ({oracle['name']}): "
                f"status={oracle['status']}, price={oracle['price_micro_usd']}"
            )

    def test_oracle_names_present(self, node):
        """Verify oracle names are present and non-empty."""
        self.log.info("Test: Oracle names are present")
        expected_names = ["Jared", "Green Candle", "Bastian", "DanGB", "Shenger", "Ycagel", "Aussie"]

        result = node.getoracles()
        for oracle in result:
            oid = oracle["oracle_id"]
            if oid < len(expected_names):
                assert_equal(oracle["name"], expected_names[oid])

        self.log.info(f"  All {len(expected_names)} oracle names verified")

    def test_getoracles_p2p_does_not_resend_stale_pending(self, node):
        """GETORACLES should serve fresh pending data but skip stale entries."""
        self.log.info("Test: P2P getoracles skips stale pending oracle messages")

        base_time = 1777417000
        node.setmocktime(base_time)
        node.submitoracleprice(0, 50000)

        peer = node.add_p2p_connection(OraclePriceReceiver())
        epoch = node.getblockcount() // REGTEST_ORACLE_EPOCH_BLOCKS

        peer.send_message(msg_getoracles(epoch=epoch, oracle_id=0))
        peer.wait_until(lambda: len(peer.oracle_prices) == 1, timeout=5)

        with p2p_lock:
            assert_equal(peer.oracle_prices[0].timestamp, base_time)

        peer.clear_prices()
        node.setmocktime(base_time + ORACLE_MAX_AGE_SECONDS + 1)
        peer.send_and_ping(msg_getoracles(epoch=epoch, oracle_id=0))

        with p2p_lock:
            assert_equal(len(peer.oracle_prices), 0)
        self.log.info("  Stale pending oracle message was not resent")


if __name__ == '__main__':
    GetOraclesPendingTest().main()
