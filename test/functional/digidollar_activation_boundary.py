#!/usr/bin/env python3
# Copyright (c) 2025-2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test DigiDollar activation boundary — pre-activation rejection & post-activation acceptance.

Verifies that:
  - DD transactions are rejected from mempool before the buried activation height
  - DD transactions are accepted into mempool at/after the buried activation height
  - getdigidollardeploymentinfo reflects correct state throughout
  - A reorg below the activation height flips DigiDollar back off and purges
    stale DD mempool entries

DigiDollar is a buried deployment (BIP90): -digidollaractivationheight=200
hardcodes activation at exactly height 200 on regtest (no BIP9 signaling).
The first DD-active block is block 200, so the RPCs report enabled=True once
the tip reaches height 199 (the *next* block is DD-active).
"""

from decimal import Decimal

from test_framework.address import address_to_scriptpubkey
from test_framework.blocktools import create_block, create_coinbase
from test_framework.messages import COutPoint, CTransaction, CTxIn, CTxOut, tx_from_hex
from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal

# Buried activation height used by this test (-digidollaractivationheight).
ACTIVATION_HEIGHT = 200
DD_TX_MINT_VERSION = (1 << 24) | 0x0770
DD_TX_TRANSFER_VERSION = (2 << 24) | 0x0770


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

        # ── Phase 1: Verify buried deployment reporting at genesis ──
        self.log.info("Phase 1: Checking buried deployment state at genesis...")
        info = node.getdeploymentinfo()
        dd_dep = info["deployments"]["digidollar"]
        self.log.info(f"  Genesis entry: {dd_dep}")
        assert_equal(dd_dep["type"], "buried")
        assert_equal(dd_dep["active"], False)
        assert_equal(dd_dep["height"], ACTIVATION_HEIGHT)
        assert "bip9" not in dd_dep, "buried deployment must not carry a bip9 sub-object"

        # ── Phase 2: Mine to height 150 and verify still pre-activation ──
        self.log.info("Phase 2: Mining to height 150 (pre-activation)...")
        node.generate(150)
        height = node.getblockcount()
        assert_equal(height, 150)

        info = node.getdeploymentinfo()
        dd_dep = info["deployments"]["digidollar"]
        assert_equal(dd_dep["active"], False)

        # Verify getdigidollardeploymentinfo also shows correct state
        dep = node.getdigidollardeploymentinfo()
        self.log.info(f"  Deployment info: status={dep['status']}, enabled={dep['enabled']}")
        assert_equal(dep['status'], 'defined')
        assert_equal(dep['enabled'], False)
        # The buried activation height is reported even before activation.
        assert_equal(dep['activation_height'], ACTIVATION_HEIGHT)

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

        # A DD-looking coinbase must not change base-chain validity before the
        # deployment is active. Before DD-FA-SEC-022, ConnectBlock rejected
        # this block with bad-cb-dd-marker even though the deployment was not
        # yet active.
        self.log.info("Phase 3b: Pre-activation DD-marker coinbase remains a valid DGB block...")
        pre_marker_hash = self.submit_coinbase_marker_block(node, expected_result=None)
        self.log.info(f"  Accepted pre-activation DD-marker coinbase block {pre_marker_hash}")

        # A signed, otherwise ordinary body transaction with DD marker bits must
        # also remain a valid pre-activation DGB transaction when mined directly.
        # Before DD-FA-SEC-023, ConnectBlock rejected this block with
        # digidollar-not-active even though the deployment was not yet active.
        self.log.info("Phase 3c: Pre-activation DD-marker body tx remains a valid DGB block...")
        pre_body_hash = self.submit_body_marker_block(node, expected_result=None)
        self.log.info(f"  Accepted pre-activation DD-marker body block {pre_body_hash}")

        # ── Phase 4: Verify no versionbits signaling for the buried deployment ──
        self.log.info("Phase 4: Verifying block template does not signal bit 23...")
        template = node.getblocktemplate({"rules": ["segwit"]})
        version = template["version"]
        self.log.info(f"  Block template version: 0x{version:08x}")
        assert (version & (1 << 23)) == 0, \
            "Bit 23 must not be signaled for the buried deployment"
        assert "digidollar" not in template["rules"], \
            "digidollar rule must not be advertised pre-activation"
        assert "digidollar" not in template["vbavailable"], \
            "digidollar must not appear in vbavailable post-burial"

        # ── Phase 5: Mine across the buried activation boundary ──
        self.log.info("Phase 5: Mining across the activation boundary...")

        # Tip N-2 is the last height with DigiDollar disabled: the RPCs report
        # whether the *next* block is DD-active, and block N-1 is not.
        node.generate(ACTIVATION_HEIGHT - 2 - node.getblockcount())
        assert_equal(node.getblockcount(), ACTIVATION_HEIGHT - 2)
        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'defined')
        assert_equal(dep['enabled'], False)
        self.log.info(f"  Height {ACTIVATION_HEIGHT - 2}: still disabled")

        # Tip N-1: the next block is block N, the first DD-active block, so
        # the deployment now reports enabled/active.
        node.generate(1)
        assert_equal(node.getblockcount(), ACTIVATION_HEIGHT - 1)
        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'active')
        assert_equal(dep['enabled'], True)
        self.log.info(f"  Height {ACTIVATION_HEIGHT - 1}: enabled (next block is DD-active)")

        # The template for block N advertises the digidollar rule without
        # signaling bit 23 (buried deployments never set version bits).
        template = node.getblocktemplate({"rules": ["segwit"]})
        assert (template["version"] & (1 << 23)) == 0
        assert "digidollar" in template["rules"], \
            "digidollar rule must be advertised once active"

        # Mine the first DD-active block; getdeploymentinfo reports the exact
        # buried height (the pre-burial back-scan reported first-active-tip).
        node.generate(1)
        assert_equal(node.getblockcount(), ACTIVATION_HEIGHT)
        info = node.getdeploymentinfo()
        dd_dep = info["deployments"]["digidollar"]
        assert_equal(dd_dep["type"], "buried")
        assert_equal(dd_dep["active"], True)
        assert_equal(dd_dep["height"], ACTIVATION_HEIGHT)

        # ── Phase 6: Verify getdigidollardeploymentinfo shows ACTIVE ──
        self.log.info("Phase 6: Verifying deployment info post-activation...")
        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'active')
        assert_equal(dep['enabled'], True)
        self.log.info(f"  Deployment: status={dep['status']}, enabled={dep['enabled']}")

        # ── Phase 7: Test post-activation acceptance and coinbase defense ──
        self.log.info("Phase 7: Testing post-activation acceptance...")

        self.submit_coinbase_marker_block(node, expected_result="bad-cb-dd-marker")
        self.log.info("  Rejected post-activation DD-marker coinbase block")

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

        # Mine block containing the DD transaction. This also proves the miner
        # can preselect DD mempool transactions and then commit the deterministic
        # oracle bundle required by block validation.
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

        # A non-final DD transaction must be rejected before the expensive DD
        # parser/validation path. Before DD-RH-120 this returned a DD-specific
        # structural error because ATMP ran DD validation before finality.
        future_dd = CTransaction()
        future_dd.nVersion = (2 << 24) | 0x0770  # DD_TX_TRANSFER
        future_dd.nLockTime = node.getblockcount() + 20
        future_dd.vin = [CTxIn(COutPoint(1, 0), b"", 0xFFFFFFFE)]
        future_dd.vout = [CTxOut(100000, address_to_scriptpubkey(node.getnewaddress()))]
        nonfinal = node.testmempoolaccept([future_dd.serialize().hex()], maxfeerate=0)[0]
        assert not nonfinal["allowed"], nonfinal
        assert_equal(nonfinal["reject-reason"], "non-final")
        self.log.info("  Non-final DD tx rejected before DD validation")

        # Final verification: deployment info still ACTIVE
        dep = node.getdigidollardeploymentinfo()
        assert_equal(dep['status'], 'active')
        assert_equal(dep['enabled'], True)

        # ── Phase 8: Reorg below activation purges stale DD mempool txs ──
        self.log.info("Phase 8: Verifying reorg below activation purges DD mempool entries...")
        # Invalidate block N-1 so the tip rolls back to N-2, the last height
        # where the buried deployment is disabled (the next block, N-1, is
        # below the activation height).
        rollback_tip = ACTIVATION_HEIGHT - 2
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

        rollback_hash = node.getblockhash(rollback_tip + 1)
        node.invalidateblock(rollback_hash)
        assert_equal(node.getblockcount(), rollback_tip)

        dep = node.getdigidollardeploymentinfo()
        self.log.info(f"  Deployment after reorg: status={dep['status']}, enabled={dep['enabled']}")
        assert_equal(dep['status'], 'defined')
        assert_equal(dep['enabled'], False)
        assert pending_txid not in node.getrawmempool(), \
            f"DD tx {pending_txid} must be removed from mempool after reorg below activation"

        self.log.info("All activation boundary tests PASSED ✓")

    def submit_coinbase_marker_block(self, node, expected_result):
        prev_hash_hex = node.getbestblockhash()
        prev_hash = int(prev_hash_hex, 16)
        prev_time = node.getblock(prev_hash_hex)["time"]
        next_height = node.getblockcount() + 1

        coinbase = create_coinbase(next_height)
        coinbase.nVersion = DD_TX_MINT_VERSION
        coinbase.rehash()
        block = create_block(prev_hash, coinbase, prev_time + 1)
        block.solve()

        baseline_hash = node.getbestblockhash()
        baseline_height = node.getblockcount()
        result = node.submitblock(block.serialize().hex())
        assert_equal(result, expected_result)
        if expected_result is None:
            assert_equal(node.getblockcount(), baseline_height + 1)
            return node.getbestblockhash()
        assert_equal(node.getbestblockhash(), baseline_hash)
        assert_equal(node.getblockcount(), baseline_height)
        return block.hash

    def submit_body_marker_block(self, node, expected_result):
        raw = node.createrawtransaction([], {node.getnewaddress(): Decimal("1.0")})
        funded = node.fundrawtransaction(raw, {"fee_rate": 1000})
        tx = tx_from_hex(funded["hex"])
        tx.nVersion = DD_TX_TRANSFER_VERSION
        tx.rehash()

        signed = node.signrawtransactionwithwallet(tx.serialize().hex())
        assert signed["complete"], signed
        signed_tx = tx_from_hex(signed["hex"])
        assert_equal(signed_tx.nVersion, DD_TX_TRANSFER_VERSION)

        prev_hash_hex = node.getbestblockhash()
        prev_hash = int(prev_hash_hex, 16)
        prev_time = node.getblock(prev_hash_hex)["time"]
        next_height = node.getblockcount() + 1

        coinbase = create_coinbase(next_height)
        coinbase.rehash()
        block = create_block(prev_hash, coinbase, prev_time + 1)
        block.vtx.append(signed_tx)
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()

        baseline_hash = node.getbestblockhash()
        baseline_height = node.getblockcount()
        result = node.submitblock(block.serialize().hex())
        assert_equal(result, expected_result)
        if expected_result is None:
            assert_equal(node.getblockcount(), baseline_height + 1)
            return node.getbestblockhash()
        assert_equal(node.getbestblockhash(), baseline_hash)
        assert_equal(node.getblockcount(), baseline_height)
        return block.hash


if __name__ == '__main__':
    DigiDollarActivationBoundaryTest().main()
