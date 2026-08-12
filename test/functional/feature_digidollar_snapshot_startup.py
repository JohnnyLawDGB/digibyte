#!/usr/bin/env python3
# Copyright (c) 2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that a snapshot-loaded node can restart while DigiDollar is active.

OracleBundleManager::LoadPricesFromChain() rebuilds the oracle price cache and
volatility state at every startup by reading raw blocks over the trailing
VOLATILITY_HISTORY_BLOCKS window. If any block at or above the DigiDollar
activation floor cannot be read it fails closed -- the node refuses to start and
advises -reindex.

A node bootstrapped from a UTXO snapshot has, by design, not yet downloaded the
blocks beneath the snapshot base; the background chainstate fetches them later.
On a chain where the activation floor sits below the snapshot base, those
unreadable blocks are inside the rescan window and above the floor, so the node
cannot start again after bootstrapping.

feature_assumeutxo.py cannot catch this. It runs with default regtest params,
where EarliestActivationFloor() is min(650, 0) == 0, so the guard
(dd_floor > 0) is unreachable -- which is why that suite restarts a node
mid-validation and still passes.

This test forces dd_floor > 0 with -digidollaractivationheight so the guard is
live, then performs the same restart.
"""
from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal

START_HEIGHT = 199
SNAPSHOT_BASE_HEIGHT = 299
FINAL_HEIGHT = 399

# Must sit above START_HEIGHT and below SNAPSHOT_BASE_HEIGHT so that blocks the
# snapshot node is missing fall at or above the floor -- the condition that arms
# the fail-closed branch.
DD_ACTIVATION_HEIGHT = 250


class DigiDollarSnapshotStartupTest(DigiByteTestFramework):
    def set_test_params(self):
        """Use the pregenerated, deterministic chain up to height 199."""
        self.num_nodes = 2
        self.rpc_timeout = 120
        dd_arg = f"-digidollaractivationheight={DD_ACTIVATION_HEIGHT}"
        self.extra_args = [
            [dd_arg],
            [dd_arg],
        ]

    def setup_network(self):
        """Start disconnected so n0 can build a chain n1 has not seen."""
        self.add_nodes(self.num_nodes, extra_args=self.extra_args)
        self.start_nodes(extra_args=self.extra_args)

    def run_test(self):
        n0 = self.nodes[0]
        n1 = self.nodes[1]

        self.log.info(f"DigiDollar activation floor forced to {DD_ACTIVATION_HEIGHT}")
        assert_equal(n0.getblockcount(), START_HEIGHT)

        # Mock time for a deterministic chain, so the snapshot's base block
        # matches the hash pinned in the regtest assumeutxo table.
        for n in self.nodes:
            n.setmocktime(n.getblockheader(n.getbestblockhash())['time'])

        # Build to the snapshot height, ferrying headers (but not blocks) to n1
        # so it can recognise the snapshot's base block without being able to
        # read the blocks beneath it.
        for _ in range(SNAPSHOT_BASE_HEIGHT - START_HEIGHT):
            self.generate(n0, nblocks=1, sync_fun=self.no_op)
            n1.submitheader(n0.getblock(n0.getbestblockhash(), 0))

        assert_equal(n0.getblockcount(), SNAPSHOT_BASE_HEIGHT)
        assert_equal(n1.getblockcount(), START_HEIGHT)
        assert_equal(n1.getblockchaininfo()["headers"], SNAPSHOT_BASE_HEIGHT)

        self.log.info(f"Creating a UTXO snapshot at height {SNAPSHOT_BASE_HEIGHT}")
        dump_output = n0.dumptxoutset('utxos.dat')
        assert_equal(dump_output['base_height'], SNAPSHOT_BASE_HEIGHT)

        # Mine past the snapshot so the trailing rescan window is populated on n0.
        self.generate(n0, nblocks=FINAL_HEIGHT - SNAPSHOT_BASE_HEIGHT, sync_fun=self.no_op)
        assert_equal(n0.getblockcount(), FINAL_HEIGHT)

        self.log.info("Loading the snapshot into the second node")
        loaded = n1.loadtxoutset(dump_output['path'])
        assert_equal(loaded['base_height'], SNAPSHOT_BASE_HEIGHT)
        assert_equal(n1.getblockchaininfo()["blocks"], SNAPSHOT_BASE_HEIGHT)

        # n1 now has a snapshot chainstate at height 299 but no block data below
        # it. Blocks in [DD_ACTIVATION_HEIGHT, 299] are therefore both inside the
        # startup rescan window and at/above the floor.
        self.log.info("Restarting the snapshot node before background validation completes")
        self.stop_node(1)
        try:
            self.start_node(1, extra_args=self.extra_args[1])
        except Exception as e:
            raise AssertionError(
                "snapshot-bootstrapped node failed to restart while DigiDollar is "
                "active -- the startup reconstruction in init.cpp (LoadPricesFromChain "
                "/ ReconstructFromChain) cannot read blocks beneath the snapshot base "
                f"and fails closed: {e}") from e

        # The node must come back up with its snapshot chainstate intact.
        assert_equal(n1.getblockchaininfo()["blocks"], SNAPSHOT_BASE_HEIGHT)
        self.log.info("Snapshot node restarted successfully with DigiDollar active")


if __name__ == '__main__':
    DigiDollarSnapshotStartupTest().main()
