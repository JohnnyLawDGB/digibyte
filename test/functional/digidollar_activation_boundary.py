#!/usr/bin/env python3
# Copyright (c) 2025-2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test DigiDollar activation boundary — pre-activation rejection & post-activation acceptance.

Verifies that:
  - DD transactions are rejected from mempool before BIP9 activation
  - DD transactions are accepted into mempool after BIP9 activation
  - getdigidollardeploymentinfo reflects correct state throughout
  - BIP9 state machine transitions correctly (DEFINED → STARTED → LOCKED_IN → ACTIVE)

Uses -digidollaractivationheight=200 to enable real BIP9 signaling with
min_activation_height=200 on regtest.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal

# Regtest BIP9 parameters
REGTEST_CONFIRMATION_WINDOW = 144


class DigiDollarActivationBoundaryTest(DigiByteTestFramework):
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

    def run_test(self):
        node = self.nodes[0]

        # ── Phase 1: Verify initial DEFINED state at genesis ──
        self.log.info("Phase 1: Checking initial BIP9 state...")
        info = node.getdeploymentinfo()
        dd_dep = info["deployments"]["digidollar"]["bip9"]
        self.log.info(f"  Genesis state: {dd_dep['status']}")
        assert_equal(dd_dep["status"], "defined")

        # ── Phase 2: Mine to height 150 and verify STARTED state ──
        self.log.info("Phase 2: Mining to height 150 (pre-activation)...")
        node.generate(150)
        height = node.getblockcount()
        assert_equal(height, 150)

        # After first period boundary (144), state transitions to STARTED
        info = node.getdeploymentinfo()
        dd_dep = info["deployments"]["digidollar"]["bip9"]
        self.log.info(f"  State at height {height}: {dd_dep['status']}")
        assert_equal(dd_dep["status"], "started")

        # Verify getdigidollardeploymentinfo also shows correct state
        dep = node.getdigidollardeploymentinfo()
        self.log.info(f"  Deployment info: status={dep['status']}, enabled={dep['enabled']}")
        assert dep['status'] != 'active', f"Should not be active at height {height}"
        assert_equal(dep['enabled'], False)

        # ── Phase 3: Test pre-activation RPC rejection ──
        self.log.info("Phase 3: Testing pre-activation RPC rejection...")

        # mintdigidollar should fail pre-activation with activation check error
        try:
            node.mintdigidollar(100000, 4)
            assert False, "mintdigidollar should have been rejected pre-activation"
        except Exception as e:
            error_msg = str(e)
            self.log.info(f"  RPC correctly rejected: {error_msg}")
            assert "not yet active" in error_msg.lower(), \
                f"Expected 'not yet active' error, got: {error_msg}"

        # ── Phase 4: Verify block version bit 23 signaling ──
        self.log.info("Phase 4: Verifying block version bit signaling...")
        template = node.getblocktemplate({"rules": ["segwit"]})
        version = template["version"]
        bit_23_set = (version & (1 << 23)) != 0
        self.log.info(f"  Block template version: 0x{version:08x}, bit 23: {bit_23_set}")
        assert bit_23_set, "Bit 23 should be set when STARTED"

        # ── Phase 5: Mine through BIP9 state machine to ACTIVE ──
        self.log.info("Phase 5: Progressing BIP9 state machine...")

        # Track state transitions
        seen_states = set()
        seen_states.add("started")

        for i in range(10):  # Safety limit
            # Mine to next period boundary
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
            assert False, f"Failed to activate after height {height}"

        # Verify we went through expected transitions
        assert "started" in seen_states, "Should have been STARTED"
        assert "locked_in" in seen_states, "Should have been LOCKED_IN"
        assert "active" in seen_states, "Should have reached ACTIVE"
        self.log.info(f"  State transitions observed: {seen_states}")

        # Verify activation height respects min_activation_height
        dep_info = info["deployments"]["digidollar"]
        active_since = dep_info["bip9"]["since"]
        self.log.info(f"  Active since height: {active_since}")
        assert active_since >= 200, \
            f"Activation height {active_since} should be >= min_activation_height 200"

        # ── Phase 6: Verify getdigidollardeploymentinfo shows ACTIVE ──
        self.log.info("Phase 6: Verifying deployment info post-activation...")
        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'active')
        assert_equal(dep['enabled'], True)
        self.log.info(f"  Deployment: status={dep['status']}, enabled={dep['enabled']}")

        # ── Phase 7: Test post-activation acceptance ──
        self.log.info("Phase 7: Testing post-activation acceptance...")

        # Mine extra blocks for coinbase maturity
        node.generate(110)
        height = node.getblockcount()

        # Set oracle price for minting
        node.setmockoracleprice(500000)  # $0.50/DGB

        # mintdigidollar should now succeed
        result = node.mintdigidollar(100000, 4)  # $1000, tier 4
        txid = result['txid']
        self.log.info(f"  Mint succeeded: txid={txid}")

        # TX should be in mempool
        mempool = node.getrawmempool()
        assert txid in mempool, \
            f"DD tx {txid} should be in mempool post-activation"
        self.log.info(f"  TX in mempool: confirmed")

        # Mine block containing the DD transaction
        block_hash = node.generate(1)[0]
        block = node.getblock(block_hash)
        assert txid in block['tx'], "DD tx should be in mined block"
        self.log.info(f"  Block {block['height']} contains DD tx")

        # TX should be confirmed (no longer in mempool)
        mempool = node.getrawmempool()
        assert txid not in mempool, "TX should be confirmed"

        # Verify position was created
        positions = node.listdigidollarpositions()
        assert len(positions) > 0, "Expected at least one DD position"
        self.log.info(f"  Positions: {len(positions)}")

        # Final verification: deployment info still ACTIVE
        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'active')
        assert_equal(dep['enabled'], True)

        # ── Phase 8: Reorg below activation purges stale DD mempool txs ──
        self.log.info("Phase 8: Verifying reorg below activation purges DD mempool entries...")
        locked_in_since = active_since - REGTEST_CONFIRMATION_WINDOW
        rollback_tip = locked_in_since - 1
        mature_at_rollback_height = rollback_tip - 100
        current_height = node.getblockcount()
        transient_utxos = []
        for utxo in node.listunspent():
            utxo_height = current_height - utxo["confirmations"] + 1
            if utxo_height > mature_at_rollback_height:
                transient_utxos.append({"txid": utxo["txid"], "vout": utxo["vout"]})
        if transient_utxos:
            node.lockunspent(False, transient_utxos)

        pending_result = node.mintdigidollar(50000, 4)
        pending_txid = pending_result['txid']
        assert pending_txid in node.getrawmempool(), \
            f"Pending DD tx {pending_txid} should be in mempool before reorg"

        rollback_hash = node.getblockhash(locked_in_since)
        node.invalidateblock(rollback_hash)

        dep = node.getdigidollardeploymentinfo()
        self.log.info(f"  Deployment after reorg: status={dep['status']}, enabled={dep['enabled']}")
        assert_equal(dep['enabled'], False)
        assert pending_txid not in node.getrawmempool(), \
            f"DD tx {pending_txid} must be removed from mempool after reorg below activation"

        self.log.info("All activation boundary tests PASSED ✓")


if __name__ == '__main__':
    DigiDollarActivationBoundaryTest().main()
