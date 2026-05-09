#!/usr/bin/env python3
# Copyright (c) 2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test pending DigiDollar mint position reporting.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal


class DigiDollarPendingPositionStatusTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.generate(self.nodes[0], 110)
        result = self.nodes[0].setmockoracleprice(500000)
        assert_equal(result["price_micro_usd"], 500000)

        result = self.nodes[0].mintdigidollar(1000, 0)
        txid = result["txid"]
        assert txid in self.nodes[0].getrawmempool(), "broadcast mint should enter mempool"

        positions = self.nodes[0].listdigidollarpositions(False)
        matching = [pos for pos in positions if pos["position_id"] == txid]
        assert_equal(len(matching), 1)

        position = matching[0]
        assert_equal(position["confirmations"], 0)
        assert_equal(position["status"], "pending")
        assert_equal(position["can_redeem"], False)


if __name__ == "__main__":
    DigiDollarPendingPositionStatusTest().main()
