#!/usr/bin/env python3
# Copyright (c) 2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test DigiDollar wallet bookkeeping across block disconnect.

A confirmed redeem marks the collateral position inactive. If the redeem block
is disconnected, wallet DD state must make the position active again so the
wallet view matches the active chain.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal


def position_is_active(position):
    return position.get("is_active", position.get("status") in ("active", "unlocked"))


class WalletDigiDollarReorgTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-txindex=1"]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Mining spendable funds")
        node.generate(200)
        node.setmockoracleprice(500000)

        self.log.info("Minting tier-0 DigiDollar position")
        mint = node.mintdigidollar(100000, 0)
        position_id = mint["position_id"]
        node.generate(1)

        positions = node.listdigidollarpositions(False)
        position = next(p for p in positions if p["position_id"] == position_id)
        assert_equal(position_is_active(position), True)
        assert_equal(node.getdigidollarbalance()["total"], 100000)

        self.log.info("Mining to timelock expiry and redeeming")
        unlock_height = position["unlock_height"]
        blocks_needed = max(0, unlock_height - node.getblockcount())
        if blocks_needed:
            node.generate(blocks_needed)

        redeem = node.redeemdigidollar(position_id, 100000)
        redeem_txid = redeem["txid"]
        redeem_block = node.generate(1)[0]

        positions = node.listdigidollarpositions(False)
        position = next(p for p in positions if p["position_id"] == position_id)
        assert_equal(position_is_active(position), False)
        assert_equal(node.getdigidollarbalance()["total"], 0)

        self.log.info("Disconnecting the redeem block")
        node.invalidateblock(redeem_block)

        positions = node.listdigidollarpositions(False)
        position = next(p for p in positions if p["position_id"] == position_id)
        assert_equal(position_is_active(position), True)

        # The redeem transaction may be back in mempool, so spendable DD can
        # remain zero while the mempool spend is live. The important invariant
        # here is that the confirmed-chain position state was reversed.
        assert redeem_txid in node.getrawmempool()


if __name__ == "__main__":
    WalletDigiDollarReorgTest().main()
