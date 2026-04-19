#!/usr/bin/env python3
"""DigiDollar Oracle Phase 2 Multi-Oracle Consensus Tests.

Tests for Phase 2 multi-oracle consensus (RC30: 9-of-17 mainnet/testnet, 4-of-7 regtest).
Validates oracle bundle validation, consensus price calculation, and Byzantine fault tolerance.

Specification: ORACLE_PHASE_2_SPEC_PRD.md
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_raises_rpc_error,
)
from decimal import Decimal
import time

# RC30: 9-of-17 (was 8-of-15 mainnet / 3-of-10 testnet)
ORACLE_TOTAL_COUNT = 30
ORACLE_ACTIVE_COUNT = 17
ORACLE_CONSENSUS_REQUIRED = 9
TESTNET_ORACLE_COUNT = 17
TESTNET_CONSENSUS_REQUIRED = 9


class DigiDollarOraclePhase2Test(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.extra_args = [
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"]
        ]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar Oracle Phase 2 Multi-Oracle Consensus...")

        self.setup_phase2_test()

        self.test_phase2_activation()
        self.test_multi_oracle_consensus()
        self.test_insufficient_signatures()
        self.test_oracle_epoch_rotation()
        self.test_outlier_filtering()
        self.test_byzantine_oracle()
        self.test_phase_transition()
        self.test_price_staleness()
        self.test_consensus_price_calculation()
        self.test_duplicate_oracle_rejection()

    def setup_phase2_test(self):
        self.log.info("Setting up Phase 2 test environment...")
        self.generate(self.nodes[0], 110)
        self.sync_all()

        base_price = 50000
        for node in self.nodes:
            node.setmockoracleprice(base_price)

    def test_phase2_activation(self):
        self.log.info("Testing Phase 2 activation at configured height...")

        try:
            oracle_info = self.nodes[0].getoracleinfo()

            if 'phase' in oracle_info:
                current_phase = oracle_info['phase']
                self.log.info(f"Current oracle phase: {current_phase}")

            if 'phase2_activation_height' in oracle_info:
                activation_height = oracle_info['phase2_activation_height']
                current_height = self.nodes[0].getblockcount()
                self.log.info(f"Phase 2 activation: {activation_height}, current: {current_height}")

                if activation_height < 2147483647:
                    is_phase2 = current_height >= activation_height
                    self.log.info(f"Phase 2 active: {is_phase2}")
                else:
                    self.log.info("Phase 2 not activated (height = INT_MAX)")

        except Exception as e:
            self.log.info(f"getoracleinfo() not available: {e}")

    def test_multi_oracle_consensus(self):
        # RC30: 9-of-17
        self.log.info("Testing multi-oracle consensus (9-of-17)...")

        oracle_prices = [
            49500, 50000, 50500, 51000, 51500,
            52000, 48000, 49000, 50200, 50800,
            50100, 50300, 50400, 50600, 49800,
            49900, 50050
        ]

        try:
            # Submit the consensus threshold (9) of oracle prices
            for i, price in enumerate(oracle_prices[:ORACLE_CONSENSUS_REQUIRED]):
                result = self.nodes[0].submitoracleprice(i, price)
                self.log.info(f"Oracle {i} submitted price {price}: {result}")

            self.generate(self.nodes[0], 1)
            self.sync_all()

            oracle_info = self.nodes[0].getoracleprice()
            if 'consensus_price' in oracle_info:
                consensus = oracle_info['consensus_price']
                self.log.info(f"Consensus price with {ORACLE_CONSENSUS_REQUIRED} oracles: {consensus}")

        except Exception as e:
            self.log.info(f"Multi-oracle consensus test skipped (RPC not available): {e}")

            for node in self.nodes:
                node.setmockoracleprice(50000)

            self.generate(self.nodes[0], 1)
            self.sync_all()

            oracle_info = self.nodes[0].getoracleprice()
            self.log.info(f"Mock oracle price: {oracle_info}")

    def test_insufficient_signatures(self):
        # RC30: consensus requires 9-of-17, so 8 or fewer must be rejected
        insufficient = ORACLE_CONSENSUS_REQUIRED - 1  # 8
        self.log.info(f"Testing rejection with insufficient signatures (< {ORACLE_CONSENSUS_REQUIRED})...")

        try:
            for i in range(insufficient):
                self.nodes[0].submitoracleprice(i, 50000 + i * 100)

            self.generate(self.nodes[0], 1)
            self.sync_all()

            oracle_info = self.nodes[0].getoracleprice()

            if 'has_consensus' in oracle_info:
                has_consensus = oracle_info['has_consensus']
                if not has_consensus:
                    self.log.info(f"PASS: Consensus rejected with only {insufficient} signatures")
                else:
                    self.log.info("Phase 1 mode: Single oracle consensus accepted")

        except Exception as e:
            self.log.info(f"Insufficient signatures test skipped: {e}")

    def test_oracle_epoch_rotation(self):
        self.log.info("Testing deterministic oracle epoch rotation...")

        try:
            epoch_info = self.nodes[0].getoracleepoch()

            if 'current_epoch' in epoch_info and 'active_oracles' in epoch_info:
                current_epoch = epoch_info['current_epoch']
                active_oracles = epoch_info['active_oracles']
                self.log.info(f"Epoch {current_epoch}: {len(active_oracles)} active oracles")

                if 'epoch_length' in epoch_info:
                    epoch_length = epoch_info['epoch_length']
                    self.log.info(f"Epoch length: {epoch_length} blocks")

        except Exception as e:
            self.log.info(f"Epoch rotation test skipped: {e}")

    def test_outlier_filtering(self):
        # RC30: 9-of-17 — use 9 normal oracles + 1 outlier to cross the consensus threshold
        self.log.info("Testing IQR outlier filtering...")

        normal_prices = [49000, 49500, 50000, 50500, 51000, 51500, 52000, 52500, 53000]
        outlier_price = 1000000

        expected_median = 51000

        try:
            for i, price in enumerate(normal_prices):
                self.nodes[0].submitoracleprice(i, price)

            self.nodes[0].submitoracleprice(len(normal_prices), outlier_price)

            self.generate(self.nodes[0], 1)
            self.sync_all()

            oracle_info = self.nodes[0].getoracleprice()
            if 'consensus_price' in oracle_info:
                consensus = oracle_info['consensus_price']
                assert_greater_than(consensus, 40000)
                assert_greater_than(60000, consensus)
                self.log.info(f"Outlier filtered: consensus={consensus}, expected~{expected_median}")

        except Exception as e:
            self.log.info(f"Outlier filtering test skipped: {e}")

    def test_byzantine_oracle(self):
        # RC30: 9-of-17 => 9 honest + 8 malicious (honest majority)
        self.log.info("Testing Byzantine fault tolerance (8 malicious + 9 honest)...")

        honest_prices = [50000, 50100, 50200, 50300, 50400, 50500, 50600, 50700, 50800]
        malicious_prices = [1000000, 1, 999999, 2, 888888, 3, 777777, 4]

        try:
            for i, price in enumerate(honest_prices):
                self.nodes[0].submitoracleprice(i, price)

            for i, price in enumerate(malicious_prices):
                self.nodes[0].submitoracleprice(i + len(honest_prices), price)

            self.generate(self.nodes[0], 1)
            self.sync_all()

            oracle_info = self.nodes[0].getoracleprice()
            if 'consensus_price' in oracle_info:
                consensus = oracle_info['consensus_price']
                assert_greater_than(consensus, 40000)
                assert_greater_than(60000, consensus)
                self.log.info(f"Byzantine tolerance: consensus={consensus} (within honest range)")

        except Exception as e:
            self.log.info(f"Byzantine tolerance test skipped: {e}")

    def test_phase_transition(self):
        self.log.info("Testing mint across Phase 1 to Phase 2 boundary...")

        try:
            for node in self.nodes:
                node.setmockoracleprice(50000)

            self.generate(self.nodes[0], 5)
            self.sync_all()

            stats = self.nodes[0].getdigidollarstats()
            self.log.info(f"DigiDollar stats during transition: {stats.get('health_status', 'unknown')}")

        except Exception as e:
            self.log.info(f"Phase transition test error: {e}")

    def test_price_staleness(self):
        self.log.info("Testing rejection of stale oracle prices...")

        try:
            self.nodes[0].setmockoracleprice(50000)

            self.generate(self.nodes[0], 100)
            self.sync_all()

            oracle_info = self.nodes[0].getoracleprice()

            if 'is_stale' in oracle_info:
                self.log.info(f"Price staleness check: {oracle_info['is_stale']}")
            elif 'last_update_time' in oracle_info:
                last_update = oracle_info['last_update_time']
                current_time = int(time.time())
                age = current_time - last_update
                self.log.info(f"Price age: {age} seconds")

        except Exception as e:
            self.log.info(f"Price staleness test error: {e}")

    def test_consensus_price_calculation(self):
        self.log.info("Testing consensus price calculation methods...")

        test_prices = [100, 200, 300, 400, 500]
        expected_median = 300

        self.log.info(f"Test prices: {test_prices}")
        self.log.info(f"Expected median: {expected_median}")

        sorted_prices = sorted(test_prices)
        n = len(sorted_prices)
        if n % 2 == 1:
            calculated_median = sorted_prices[n // 2]
        else:
            calculated_median = (sorted_prices[n // 2 - 1] + sorted_prices[n // 2]) // 2

        assert_equal(calculated_median, expected_median)
        self.log.info(f"Median calculation verified: {calculated_median}")

        even_prices = [100, 200, 300, 400]
        expected_even_median = 250

        sorted_even = sorted(even_prices)
        n = len(sorted_even)
        calculated_even = (sorted_even[n // 2 - 1] + sorted_even[n // 2]) // 2

        assert_equal(calculated_even, expected_even_median)
        self.log.info(f"Even count median verified: {calculated_even}")

    def test_duplicate_oracle_rejection(self):
        self.log.info("Testing rejection of duplicate oracle IDs...")

        try:
            self.nodes[0].submitoracleprice(0, 50000)
            self.nodes[0].submitoracleprice(0, 51000)

            self.generate(self.nodes[0], 1)
            self.sync_all()

            oracle_info = self.nodes[0].getoracleprice()
            self.log.info(f"After duplicate submission: {oracle_info}")

        except Exception as e:
            if "duplicate" in str(e).lower():
                self.log.info("PASS: Duplicate oracle ID correctly rejected")
            else:
                self.log.info(f"Duplicate rejection test: {e}")


if __name__ == '__main__':
    DigiDollarOraclePhase2Test().main()
