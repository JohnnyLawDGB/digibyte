#!/usr/bin/env python3
# Copyright (c) 2025-2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test DigiDollar RPC gating — all 27 gated RPCs must be blocked before activation.

Verifies that:
  - All 27 gated DD/Oracle RPCs return "DigiDollar is not yet active" pre-activation
  - getdigidollardeploymentinfo (ungated) works at any time
  - After BIP9 activation, key RPCs become functional

Uses -digidollaractivationheight=200 for real BIP9 signaling on regtest.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal

REGTEST_CONFIRMATION_WINDOW = 144


class DigiDollarRPCGatingTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            "-digidollar=1",
            "-digidollaractivationheight=200",
            "-txindex=1",
            "-dandelion=0",
        ]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def get_gated_rpc_calls(self, node):
        """Return list of (name, callable) for all 27 gated RPCs with reasonable args."""
        dummy_addr = "dgbt1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqad0mt2"
        dummy_txid = "ab" * 32  # valid 64-char hex
        return [
            # DD transaction RPCs
            ("mintdigidollar", lambda: node.mintdigidollar(10000, 0)),
            ("redeemdigidollar", lambda: node.redeemdigidollar(dummy_txid, 5000)),
            ("senddigidollar", lambda: node.senddigidollar(dummy_addr, 1000)),
            # DD wallet/address RPCs
            ("getdigidollaraddress", lambda: node.getdigidollaraddress()),
            ("getdigidollarbalance", lambda: node.getdigidollarbalance()),
            ("listdigidollarpositions", lambda: node.listdigidollarpositions()),
            ("listdigidollartxs", lambda: node.listdigidollartxs()),
            ("listdigidollaraddresses", lambda: node.listdigidollaraddresses()),
            ("importdigidollaraddress", lambda: node.importdigidollaraddress(dummy_addr)),
            # DD info/stats RPCs
            ("getdigidollarstats", lambda: node.getdigidollarstats()),
            ("calculatecollateralrequirement", lambda: node.calculatecollateralrequirement(10000, 4)),
            ("estimatecollateral", lambda: node.estimatecollateral(10000, 3)),
            # DD utility RPCs
            ("getredemptioninfo", lambda: node.getredemptioninfo(dummy_txid, 0)),
            ("getprotectionstatus", lambda: node.getprotectionstatus()),
            ("getdcamultiplier", lambda: node.getdcamultiplier()),
            ("validateddaddress", lambda: node.validateddaddress(dummy_addr)),
            # Oracle query RPCs
            ("getoracleprice", lambda: node.getoracleprice()),
            ("getalloracleprices", lambda: node.getalloracleprices()),
            ("getoracles", lambda: node.getoracles()),
            ("listoracle", lambda: node.listoracle()),
            ("getoraclepubkey", lambda: node.getoraclepubkey(0)),
            # Oracle management RPCs
            ("createoraclekey", lambda: node.createoraclekey(0)),
            ("startoracle", lambda: node.startoracle(0)),
            ("stoporacle", lambda: node.stoporacle(0)),
            # Oracle price submission RPCs
            ("sendoracleprice", lambda: node.sendoracleprice(0.05)),
            ("submitoracleprice", lambda: node.submitoracleprice(0, 6500)),
            ("simulatepricevolatility", lambda: node.simulatepricevolatility(10)),
        ]

    def test_rpc_gated(self, node, name, call):
        """Assert that an RPC call fails with 'not yet active' error."""
        try:
            call()
            assert False, f"{name} should have been rejected pre-activation"
        except Exception as e:
            error_msg = str(e)
            assert "not yet active" in error_msg.lower(), \
                f"{name}: expected 'not yet active' error, got: {error_msg}"
            self.log.info(f"  ✓ {name} — correctly gated")

    def activate_digidollar(self, node):
        """Mine through BIP9 DEFINED → STARTED → LOCKED_IN → ACTIVE."""
        seen_states = set()
        info = node.getdeploymentinfo()
        seen_states.add(info["deployments"]["digidollar"]["bip9"]["status"])

        for _ in range(10):
            current = node.getblockcount()
            remaining = REGTEST_CONFIRMATION_WINDOW - (current % REGTEST_CONFIRMATION_WINDOW)
            if remaining == 0:
                remaining = REGTEST_CONFIRMATION_WINDOW
            node.generate(remaining)

            height = node.getblockcount()
            info = node.getdeploymentinfo()
            status = info["deployments"]["digidollar"]["bip9"]["status"]
            seen_states.add(status)
            self.log.info(f"  Height {height}: {status}")

            if status == 'active':
                break
        else:
            assert False, f"Failed to reach ACTIVE after height {node.getblockcount()}"

        assert "started" in seen_states, "Should have been STARTED"
        assert "locked_in" in seen_states, "Should have been LOCKED_IN"
        assert "active" in seen_states, "Should have reached ACTIVE"

    def run_test(self):
        node = self.nodes[0]

        # ── Phase 1: Verify all 27 gated RPCs are blocked at DEFINED state ──
        self.log.info("Phase 1: Testing all 27 gated RPCs at DEFINED state (height 0)...")
        info = node.getdeploymentinfo()
        assert_equal(info["deployments"]["digidollar"]["bip9"]["status"], "defined")

        gated_rpcs = self.get_gated_rpc_calls(node)
        assert_equal(len(gated_rpcs), 27)

        blocked_count = 0
        for name, call in gated_rpcs:
            self.test_rpc_gated(node, name, call)
            blocked_count += 1

        self.log.info(f"  All {blocked_count}/27 gated RPCs correctly blocked")

        # ── Phase 2: Verify ungated RPC works pre-activation ──
        self.log.info("Phase 2: Verifying getdigidollardeploymentinfo works without activation...")
        dep = node.getdigidollardeploymentinfo()
        self.log.info(f"  ✓ getdigidollardeploymentinfo — status={dep['status']}, enabled={dep['enabled']}")
        assert dep['status'] != 'active'
        assert_equal(dep['enabled'], False)

        # ── Phase 3: Mine through BIP9 to ACTIVE ──
        self.log.info("Phase 3: Activating DigiDollar via BIP9...")
        self.activate_digidollar(node)

        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'active')
        assert_equal(dep['enabled'], True)
        self.log.info(f"  DigiDollar is now ACTIVE")

        # ── Phase 4: Verify key RPCs work post-activation ──
        self.log.info("Phase 4: Testing RPCs post-activation...")

        # Mine for coinbase maturity
        node.generate(110)

        # Set oracle price
        node.setmockoracleprice(500000)

        # getdigidollardeploymentinfo (ungated, still works)
        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'active')
        assert_equal(dep['enabled'], True)
        self.log.info(f"  ✓ getdigidollardeploymentinfo — active")

        # getdigidollarbalance
        bal = node.getdigidollarbalance()
        self.log.info(f"  ✓ getdigidollarbalance — {bal}")

        # getdigidollarstats
        stats = node.getdigidollarstats()
        self.log.info(f"  ✓ getdigidollarstats — total_supply={stats.get('total_supply', stats.get('totalSupply', 'N/A'))}")

        # mintdigidollar
        result = node.mintdigidollar(100000, 4)
        self.log.info(f"  ✓ mintdigidollar — txid={result['txid']}")

        # Mine the mint tx
        node.generate(1)

        # listdigidollarpositions
        positions = node.listdigidollarpositions()
        assert len(positions) > 0, "Should have at least 1 position after mint"
        self.log.info(f"  ✓ listdigidollarpositions — {len(positions)} position(s)")

        self.log.info("All DigiDollar RPC gating tests PASSED ✓")


if __name__ == '__main__':
    DigiDollarRPCGatingTest().main()
