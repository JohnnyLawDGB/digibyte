#!/usr/bin/env python3
# Copyright (c) 2025 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test fix for Bug #2 (sub-cent price_cents) and Bug #8 (hardcoded 24h/volatility)."""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_greater_than
from decimal import Decimal


class DigiDollarBug2Bug8OraclePriceTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-digidollar=1", "-txindex=1"]]

    def run_test(self):
        node = self.nodes[0]

        # Generate enough blocks to activate DigiDollar
        self.generate(node, 200)

        # Set sub-cent mock oracle price: 4042 micro_usd = $0.004042
        node.setmockoracleprice(4042)

        result = node.getoracleprice()

        # Bug #2: price_cents should NOT be 0 for sub-cent price
        # 4042 micro_usd / 10000 = 0.4042 cents
        price_cents = result["price_cents"]
        self.log.info(f"price_cents = {price_cents}")
        assert_greater_than(price_cents, 0)
        assert abs(float(price_cents) - 0.4042) < 0.001, f"Expected ~0.4042, got {price_cents}"

        # Bug #8: 24h_high and 24h_low should not be 0
        high_24h = result["24h_high"]
        low_24h = result["24h_low"]
        self.log.info(f"24h_high = {high_24h}, 24h_low = {low_24h}")
        assert_greater_than(high_24h, 0)
        assert_greater_than(low_24h, 0)

        # Bug #8: volatility should not be hardcoded 2.5
        volatility = result["volatility"]
        self.log.info(f"volatility = {volatility}")
        # With a single constant mock price, volatility should be 0.0 (no variation)
        assert volatility != 2.5, f"Volatility should not be hardcoded 2.5, got {volatility}"


if __name__ == '__main__':
    DigiDollarBug2Bug8OraclePriceTest().main()
