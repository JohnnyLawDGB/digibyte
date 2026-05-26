#!/usr/bin/env python3
# Copyright (c) 2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Regression coverage for DigiDollar wallet bugs reported during RC33.

The scenarios here target the wallet state problems that isolated txbuilder
tests do not prove: fragmented collateral mints, rapid consecutive mints, and
rapid consecutive redemptions across mining and restart.
"""

from decimal import Decimal

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal


ORACLE_PRICE_MICRO_USD = 100_000_000  # $100/DGB keeps the test small.


def position_active(position):
    return position.get("is_active", position.get("status") in ("active", "unlocked"))


class WalletDigiDollarRC33RegressionsTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        self.generate(node, 160)
        result = node.setmockoracleprice(ORACLE_PRICE_MICRO_USD)
        assert_equal(result["price_micro_usd"], ORACLE_PRICE_MICRO_USD)
        self.default_wallet_name = node.listwallets()[0]

        self.test_mint_dgb_change_confirms_and_spends()
        self.test_fragmented_large_mint_consolidates_and_confirms()
        self.test_repeated_mints_spend_unconfirmed_change()
        self.test_rapid_mints_confirm_after_restart()
        self.test_rapid_redeems_confirm_after_restart()

    def create_descriptor_wallet(self, name):
        self.nodes[0].createwallet(wallet_name=name, descriptors=True)
        return self.nodes[0].get_wallet_rpc(name)

    def get_loaded_wallet(self, name):
        node = self.nodes[0]
        if name not in node.listwallets():
            node.loadwallet(name)
        return node.get_wallet_rpc(name)

    def funder_wallet(self):
        return self.get_loaded_wallet(self.default_wallet_name)

    def test_fragmented_large_mint_consolidates_and_confirms(self):
        self.log.info("Testing fragmented large mint auto-consolidation")
        node = self.nodes[0]
        fragmented = self.create_descriptor_wallet("rc33_fragmented")

        outputs = {
            fragmented.getnewaddress(): Decimal("0.25")
            for _ in range(450)
        }
        funding_txid = self.funder_wallet().sendmany("", outputs)
        self.generate(node, 1)
        assert_equal(self.funder_wallet().gettransaction(funding_txid)["confirmations"], 1)

        assert_equal(len(fragmented.listunspent(1)), 450)

        mint = fragmented.mintdigidollar(100000, 0)
        assert_equal(mint["utxos_consolidated"], True)
        assert mint["consolidation_txid"] in node.getrawmempool()
        assert mint["txid"] in node.getrawmempool()

        self.generate(node, 1)
        self.restart_node(0, extra_args=["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"])
        node = self.nodes[0]
        fragmented = self.get_loaded_wallet("rc33_fragmented")
        node.setmockoracleprice(ORACLE_PRICE_MICRO_USD)

        assert_got_mint = fragmented.gettransaction(mint["txid"])
        assert_got_consolidation = fragmented.gettransaction(mint["consolidation_txid"])
        assert_got_mint["confirmations"] > 0
        assert_got_consolidation["confirmations"] > 0

        positions = fragmented.listdigidollarpositions(False)
        matching = [p for p in positions if p["position_id"] == mint["position_id"]]
        assert_equal(len(matching), 1)
        assert_equal(matching[0]["confirmations"] > 0, True)
        assert_equal(position_active(matching[0]), True)

    def test_mint_dgb_change_confirms_and_spends(self):
        self.log.info("Testing mint DGB change remains visible and spendable")
        node = self.nodes[0]
        change_wallet = self.create_descriptor_wallet("rc41_mint_change")

        funding_txid = self.funder_wallet().sendtoaddress(change_wallet.getnewaddress(), Decimal("25.00"))
        self.generate(node, 1)
        assert_equal(self.funder_wallet().gettransaction(funding_txid)["confirmations"], 1)
        assert_equal(len(change_wallet.listunspent(1)), 1)

        mint = change_wallet.mintdigidollar(10000, 0)
        self.generate(node, 1)

        change_outputs = [
            utxo for utxo in change_wallet.listunspent(1)
            if utxo["txid"] == mint["txid"] and utxo["vout"] >= 3 and utxo["amount"] > 0
        ]
        assert_equal(len(change_outputs), 1)

        spend_txid = change_wallet.sendtoaddress(self.funder_wallet().getnewaddress(), Decimal("1.00"))
        assert spend_txid in node.getrawmempool()
        self.generate(node, 1)
        assert change_wallet.gettransaction(spend_txid)["confirmations"] > 0

    def test_repeated_mints_spend_unconfirmed_change(self):
        self.log.info("Testing repeated mints from one large UTXO do not corrupt wallet mempool state")
        node = self.nodes[0]
        rapid = self.create_descriptor_wallet("rc41_repeated_mint")

        funding_txid = self.funder_wallet().sendtoaddress(rapid.getnewaddress(), Decimal("80.00"))
        self.generate(node, 1)
        assert_equal(self.funder_wallet().gettransaction(funding_txid)["confirmations"], 1)
        assert_equal(len(rapid.listunspent(1)), 1)

        mints = []
        for _ in range(5):
            mint = rapid.mintdigidollar(10000, 0)
            mints.append(mint)
            assert mint["txid"] in node.getrawmempool()

        self.generate(node, 1)
        self.restart_node(0, extra_args=["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"])
        node = self.nodes[0]
        rapid = self.get_loaded_wallet("rc41_repeated_mint")
        node.setmockoracleprice(ORACLE_PRICE_MICRO_USD)

        positions = rapid.listdigidollarpositions(False)
        position_ids = {p["position_id"]: p for p in positions}
        for mint in mints:
            tx = rapid.gettransaction(mint["txid"])
            assert tx["confirmations"] > 0
            assert mint["position_id"] in position_ids
            assert position_ids[mint["position_id"]]["confirmations"] > 0
            assert_equal(position_active(position_ids[mint["position_id"]]), True)

    def test_rapid_mints_confirm_after_restart(self):
        self.log.info("Testing 22 rapid mints do not remain permanently pending")
        node = self.nodes[0]
        rapid = self.create_descriptor_wallet("rc33_rapid_mint")

        outputs = {
            rapid.getnewaddress(): Decimal("12.00")
            for _ in range(30)
        }
        self.funder_wallet().sendmany("", outputs)
        self.generate(node, 1)

        mints = []
        for _ in range(22):
            mint = rapid.mintdigidollar(10000, 0)
            mints.append(mint)
            assert mint["txid"] in node.getrawmempool()

        self.generate(node, 1)
        self.restart_node(0, extra_args=["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"])
        node = self.nodes[0]
        rapid = self.get_loaded_wallet("rc33_rapid_mint")
        node.setmockoracleprice(ORACLE_PRICE_MICRO_USD)

        positions = rapid.listdigidollarpositions(False)
        position_ids = {p["position_id"]: p for p in positions}
        for mint in mints:
            tx = rapid.gettransaction(mint["txid"])
            assert tx["confirmations"] > 0
            assert mint["position_id"] in position_ids
            assert position_ids[mint["position_id"]]["confirmations"] > 0
            assert_equal(position_active(position_ids[mint["position_id"]]), True)

    def test_rapid_redeems_confirm_after_restart(self):
        self.log.info("Testing rapid redemptions do not report success then disappear")
        node = self.nodes[0]
        redeem_wallet = self.create_descriptor_wallet("rc33_rapid_redeem")

        outputs = {
            redeem_wallet.getnewaddress(): Decimal("12.00")
            for _ in range(12)
        }
        self.funder_wallet().sendmany("", outputs)
        self.generate(node, 1)

        mints = []
        for _ in range(8):
            mints.append(redeem_wallet.mintdigidollar(10000, 0))
        self.generate(node, 1)

        positions = redeem_wallet.listdigidollarpositions(False)
        unlock_height = max(p["unlock_height"] for p in positions if p["position_id"] in {m["position_id"] for m in mints})
        blocks_needed = max(0, unlock_height - node.getblockcount())
        if blocks_needed:
            self.generate(node, blocks_needed)
        node.setmockoracleprice(ORACLE_PRICE_MICRO_USD)

        redeems = []
        for mint in mints:
            redeem = redeem_wallet.redeemdigidollar(mint["position_id"], 10000)
            redeems.append(redeem)
            assert redeem["txid"] in node.getrawmempool()

        self.generate(node, 1)
        self.restart_node(0, extra_args=["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"])
        node = self.nodes[0]
        redeem_wallet = self.get_loaded_wallet("rc33_rapid_redeem")
        node.setmockoracleprice(ORACLE_PRICE_MICRO_USD)

        for redeem in redeems:
            tx = redeem_wallet.gettransaction(redeem["txid"])
            assert tx["confirmations"] > 0

        positions = redeem_wallet.listdigidollarpositions(False)
        remaining = [p for p in positions if p["position_id"] in {m["position_id"] for m in mints} and position_active(p)]
        assert_equal(remaining, [])


if __name__ == "__main__":
    WalletDigiDollarRC33RegressionsTest().main()
