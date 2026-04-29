#!/usr/bin/env python3
# Copyright (c) 2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Regression test for DD-RH-066: oracle cache rollback must scan the whole
coinbase for OP_ORACLE data, not only vout[1].
"""

import struct

from test_framework.blocktools import create_block, create_coinbase
from test_framework.messages import CTxOut
from test_framework.script import CScript, CScriptOp, OP_RETURN, OP_TRUE
from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal


OP_ORACLE = CScriptOp(0xBF)
REGTEST_CONFIRMATION_WINDOW = 144


def oracle_script(price_micro_usd, timestamp, oracle_id=0):
    compact = bytes([oracle_id])
    compact += struct.pack("<Q", price_micro_usd)
    compact += struct.pack("<q", timestamp)
    return CScript([OP_RETURN, OP_ORACLE, b"\x01", compact])


class DigiDollarOracleReorgCacheTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            "-digidollaractivationheight=200",
            "-dandelion=0",
            "-txindex=1",
        ]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def activate_digidollar(self):
        node = self.nodes[0]
        while True:
            info = node.getdeploymentinfo()
            status = info["deployments"]["digidollar"]["bip9"]["status"]
            if status == "active":
                return
            current = node.getblockcount()
            remaining = REGTEST_CONFIRMATION_WINDOW - (current % REGTEST_CONFIRMATION_WINDOW)
            if remaining == 0:
                remaining = REGTEST_CONFIRMATION_WINDOW
            node.generate(remaining)

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Activate DigiDollar before Phase 2 so compact v0x01 oracle bundles are valid")
        node.generate(150)
        self.activate_digidollar()
        assert node.getblockcount() < 650

        self.log.info("Mine an honest baseline oracle block")
        baseline_price = 500000
        attacker_price = 777777
        node.setmockoracleprice(baseline_price)
        baseline_hash = node.generate(1)[0]
        baseline_height = node.getblockcount()
        assert_equal(node.getmockoracleprice()["price_micro_usd"], baseline_price)

        self.log.info("Submit a valid block whose oracle output is vout[2], not vout[1]")
        prev_hash = int(node.getbestblockhash(), 16)
        next_height = baseline_height + 1
        block_time = node.getblock(baseline_hash)["time"] + 1
        coinbase = create_coinbase(next_height)
        coinbase.vout.append(CTxOut(0, CScript([OP_TRUE])))
        coinbase.vout.append(CTxOut(0, oracle_script(attacker_price, block_time)))
        coinbase.rehash()

        block = create_block(prev_hash, coinbase, block_time)
        block.solve()
        assert_equal(node.submitblock(block.serialize().hex()), None)
        assert_equal(node.getblockcount(), next_height)
        assert_equal(node.getmockoracleprice()["price_micro_usd"], attacker_price)

        self.log.info("Invalidate the block and require oracle cache/mock price to roll back")
        node.invalidateblock(block.hash)
        assert_equal(node.getblockcount(), baseline_height)
        assert_equal(node.getmockoracleprice()["price_micro_usd"], baseline_price)


if __name__ == '__main__':
    DigiDollarOracleReorgCacheTest().main()
