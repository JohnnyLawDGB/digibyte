#!/usr/bin/env python3
# Copyright (c) 2025-2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""DigiDollar RPC amount, filter, and confirmation boundary regressions."""

from decimal import Decimal

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal


class DigiDollarRPCAmountFiltersTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.extra_args = [
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"],
            ["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"],
        ]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node0, node1, node2 = self.nodes

        self.log.info("Preparing active DigiDollar chain")
        self.generate(node0, 110)
        self.sync_all()
        for node in self.nodes:
            node.setmockoracleprice(500000)

        self.log.info("Minting two positions with different tiers and amounts")
        tier0_mint = node0.mintdigidollar(20000, 0)
        tier1_mint = node0.mintdigidollar(5000, 1)
        self.generate(node0, 1)
        self.sync_all()

        self.log.info("DD-RH-025: minconf must filter one-confirmation DD balances")
        assert_equal(node0.getdigidollarbalance("", 1)["total"], 25000)
        assert_equal(node0.getdigidollarbalance("", 2)["total"], 0)
        self.generate(node0, 1)
        self.sync_all()
        assert_equal(node0.getdigidollarbalance("", 2)["total"], 25000)

        self.log.info("DD-RH-023: position tier 0 and min_amount filters use DD units")
        tier0_positions = node0.listdigidollarpositions(False, 0)
        assert_equal(len(tier0_positions), 1)
        assert_equal(tier0_positions[0]["position_id"], tier0_mint["position_id"])
        assert_equal(tier0_positions[0]["lock_tier"], 0)

        large_positions = node0.listdigidollarpositions(False, None, 7500)
        assert_equal(len(large_positions), 1)
        assert_equal(large_positions[0]["position_id"], tier0_mint["position_id"])
        assert_equal(large_positions[0]["dd_minted"], 20000)

        self.log.info("DD-RH-024: redemption info amount parameter uses DD cents")
        partial_info = node0.getredemptioninfo(tier0_mint["position_id"], 5000)
        assert_equal(partial_info["redeemable_dd"], 5000)
        whole_dollar_info = node0.getredemptioninfo(tier0_mint["position_id"], "50.00")
        assert_equal(whole_dollar_info["redeemable_dd"], 5000)

        self.log.info("DD-RH-022: integral decimal strings are decimal dollars, not cents")
        node1_addr = node1.getdigidollaraddress()
        before_node1 = Decimal(node1.getdigidollarbalance()["total"])
        send_result = node0.senddigidollar(node1_addr, "50.00")
        assert "txid" in send_result
        self.generate(node0, 1)
        self.sync_all()
        assert_equal(Decimal(node1.getdigidollarbalance()["total"]), before_node1 + Decimal(5000))

        node2_addr = node2.getdigidollaraddress()
        before_node2 = Decimal(node2.getdigidollarbalance()["total"])
        sendmany_result = node0.sendmanydigidollar("", {node2_addr: "25.00"}, "decimal string regression")
        assert_equal(sendmany_result["total_amount"], 2500)
        assert_equal(sendmany_result["amounts"][node2_addr], 2500)
        self.generate(node0, 1)
        self.sync_all()
        assert_equal(Decimal(node2.getdigidollarbalance()["total"]), before_node2 + Decimal(2500))


if __name__ == "__main__":
    DigiDollarRPCAmountFiltersTest().main()
