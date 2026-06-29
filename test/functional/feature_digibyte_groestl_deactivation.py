#!/usr/bin/env python3
# Copyright (c) 2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Validator-level enforcement of active mining algorithms.

A block mined with a deactivated algorithm (Groestl, retired at the Odocrypt
fork) must be rejected at block acceptance, not merely refused by the local
miner. This exercises the submit/validate path with externally-crafted blocks,
which is the path the local miner does not cover.

On regtest DEPLOYMENT_DIGIDOLLAR is ALWAYS_ACTIVE and Odocrypt activates at
height 600, so above height 600 Groestl is deactivated and crafted Groestl
blocks must be rejected; below it Groestl is still active and accepted.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.blocktools import create_block, create_coinbase
from test_framework.util import assert_equal

ODOCRYPT_HEIGHT = 600  # Groestl deactivation / Odocrypt activation (regtest)

BLOCK_VERSION_GROESTL = 0x20000402
BLOCK_VERSION_SHA256D = 0x20000202


class GroestlDeactivationTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-easypow=1"]]
        self.mining_address = "dgbrt1qtmp74ayg7p24uslctssvjm06q5phz4yrgndnyh"

    def skip_test_if_missing_module(self):
        pass

    def craft(self, node, version):
        tip = node.getbestblockhash()
        hdr = node.getblockheader(tip)
        height = node.getblockcount() + 1
        return create_block(int(tip, 16), create_coinbase(height, nValue=0),
                            hdr["time"] + 1, version=version)

    def submit_until_pow_ok(self, node, block):
        # -easypow keeps the target at powLimit, so ~half of nonces satisfy any
        # algorithm's PoW. The node computes the algo-specific hash, so no Python
        # implementation of Groestl is needed: grind until PoW passes, then return
        # whatever the validator says next.
        for nonce in range(2000):
            block.nNonce = nonce
            res = node.submitblock(block.serialize().hex())
            if res != "high-hash":
                return res
        raise AssertionError("could not satisfy easypow PoW")

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Below Odocrypt: Groestl is active, crafted Groestl block is accepted")
        self.generatetoaddress(node, 150, self.mining_address, 1000000, "scrypt")
        assert_equal(node.getblockcount(), 150)
        accepted = self.craft(node, BLOCK_VERSION_GROESTL)
        assert_equal(self.submit_until_pow_ok(node, accepted), None)
        assert_equal(node.getblockcount(), 151)

        self.log.info("Above Odocrypt: Groestl is deactivated, crafted Groestl block is rejected")
        self.generatetoaddress(node, ODOCRYPT_HEIGHT + 5 - node.getblockcount(),
                               self.mining_address, 1000000, "scrypt")
        assert node.getblockcount() > ODOCRYPT_HEIGHT
        tip = node.getbestblockhash()
        rejected = self.craft(node, BLOCK_VERSION_GROESTL)
        assert_equal(self.submit_until_pow_ok(node, rejected), "bad-algo")
        assert_equal(node.getbestblockhash(), tip)

        self.log.info("Control: an active algorithm via the same submit path is accepted")
        control = self.craft(node, BLOCK_VERSION_SHA256D)
        assert_equal(self.submit_until_pow_ok(node, control), None)
        assert node.getbestblockhash() != tip


if __name__ == '__main__':
    GroestlDeactivationTest().main()
