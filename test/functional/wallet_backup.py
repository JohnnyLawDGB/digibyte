#!/usr/bin/env python3
# Copyright (c) 2009-2020 The Bitcoin Core developers
# Copyright (c) 2014-2022 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet backup features.

Test case is:
4 nodes. 1 2 and 3 send transactions between each other,
fourth node is a miner.
1 2 3 each mine a block to start, then
Miner creates 8 blocks so 1 2 3 each have 50 mature
coins to spend.
Then 5 iterations of 1/2/3 sending coins amongst
themselves to get transactions in the wallets,
and the miner mining one block.

Wallets are backed up using dumpwallet/backupwallet.
Then 5 more iterations of transactions and mining a block.

Miner then generates 101 more blocks, so any
transaction fees paid mature.

Sanity check:
  Sum(1,2,3,4 balances) == 22*72000

1/2/3 are shutdown, and their wallets erased.
Then restore using wallet.dat backup. And
confirm 1/2/3/4 balances are same as before.

Shutdown again, restore using importwallet,
and confirm again balances are correct.
"""
from decimal import Decimal
import os
from random import randint
import shutil

from test_framework.blocktools import COINBASE_MATURITY, COINBASE_MATURITY_2
from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class WalletBackupTest(DigiByteTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        # nodes 1, 2,3 are spenders, let's give them a keypool=100
        # whitelist all peers to speed up tx relay / mempool sync
        self.extra_args = [
            ["-whitelist=noban@127.0.0.1", "-keypool=100"],
            ["-whitelist=noban@127.0.0.1", "-keypool=100"],
            ["-whitelist=noban@127.0.0.1", "-keypool=100"],
            ["-whitelist=noban@127.0.0.1"],
        ]
        self.rpc_timeout = 120

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(0, 3)
        self.connect_nodes(1, 3)
        self.connect_nodes(2, 3)
        self.connect_nodes(2, 0)
        self.sync_all()

    def one_send(self, from_node, to_address):
        if (randint(1,2) == 1):
            amount = Decimal(randint(1,10)) / Decimal(10)
            self.nodes[from_node].sendtoaddress(to_address, amount)

    def do_one_round(self):
        a0 = self.nodes[0].getnewaddress()
        a1 = self.nodes[1].getnewaddress()
        a2 = self.nodes[2].getnewaddress()

        self.one_send(0, a1)
        self.one_send(0, a2)
        self.one_send(1, a0)
        self.one_send(1, a2)
        self.one_send(2, a0)
        self.one_send(2, a1)

        # Have the miner (node3) mine a block.
        # Must sync mempools before mining.
        self.sync_mempools()
        self.generate(self.nodes[3], 1)

    def run_test(self):
        self.log.info("Generating initial blockchain")
        self.generate(self.nodes[0], 1)
        self.generate(self.nodes[1], 1)
        self.generate(self.nodes[2], 1)
        # DigiByte: Need enough blocks for all coinbases to mature
        # Node 0 mined at height 0, needs height 8 (COINBASE_MATURITY)
        # Node 1 mined at height 1, needs height 9
        # Node 2 mined at height 2, needs height 10
        # So we need to mine to at least height 10
        self.generate(self.nodes[3], COINBASE_MATURITY + 2)

        # Debug: Check block count and balances
        self.log.info(f"Block count: {self.nodes[0].getblockcount()}")
        self.log.info(f"Node 0 balance: {self.nodes[0].getbalance()}, listunspent: {len(self.nodes[0].listunspent())}")
        self.log.info(f"Node 1 balance: {self.nodes[1].getbalance()}, listunspent: {len(self.nodes[1].listunspent())}")
        self.log.info(f"Node 2 balance: {self.nodes[2].getbalance()}, listunspent: {len(self.nodes[2].listunspent())}")
        self.log.info(f"Node 3 balance: {self.nodes[3].getbalance()}, listunspent: {len(self.nodes[3].listunspent())}")
        
        assert_equal(self.nodes[0].getbalance(), 72000)  # DigiByte: Block reward is 72000
        assert_equal(self.nodes[1].getbalance(), 72000)
        assert_equal(self.nodes[2].getbalance(), 72000)
        assert_equal(self.nodes[3].getbalance(), 0)

        self.log.info("Creating transactions")
        # Five rounds of sending each other transactions.
        for _ in range(5):
            self.do_one_round()

        self.log.info("Backing up")

        self.nodes[0].backupwallet(os.path.join(self.nodes[0].datadir, 'wallet.bak'))
        self.nodes[0].dumpwallet(os.path.join(self.nodes[0].datadir, 'wallet.dump'))
        self.nodes[1].backupwallet(os.path.join(self.nodes[1].datadir, 'wallet.bak'))
        self.nodes[1].dumpwallet(os.path.join(self.nodes[1].datadir, 'wallet.dump'))
        self.nodes[2].backupwallet(os.path.join(self.nodes[2].datadir, 'wallet.bak'))
        self.nodes[2].dumpwallet(os.path.join(self.nodes[2].datadir, 'wallet.dump'))

        self.log.info("More transactions")
        for _ in range(5):
            self.do_one_round()

        # Generate 101 more blocks, so any fees paid mature
        # DigiByte: Use COINBASE_MATURITY_2 instead of hardcoded 101
        self.generate(self.nodes[3], COINBASE_MATURITY_2 + 1)

        balance0 = self.nodes[0].getbalance()
        balance1 = self.nodes[1].getbalance()
        balance2 = self.nodes[2].getbalance()
        balance3 = self.nodes[3].getbalance()
        total = balance0 + balance1 + balance2 + balance3

        # At this point, there are 121 blocks (3 for setup, then 10 rounds, then 101.)
        # 22 are mature, so the sum of all wallets should be 22 * 72000 = 1584000.
        # DigiByte: Adjusted for 72000 DGB block reward
        assert_equal(total, 22 * 72000)

        ##
        # Test restoring spender wallets from backups
        ##
        self.log.info("Restoring using wallet.dat")
        self.stop_node(0)
        self.stop_node(1)
        self.stop_node(2)
        self.stop_node(3)

        # Start node2 with no chain
        shutil.rmtree(os.path.join(self.nodes[2].datadir, self.chain, 'blocks'))
        shutil.rmtree(os.path.join(self.nodes[2].datadir, self.chain, 'chainstate'))

        # Restore wallets from backup
        shutil.copyfile(
            os.path.join(self.nodes[0].datadir, 'wallet.bak'),
            os.path.join(self.nodes[0].datadir, self.chain, 'wallets', self.default_wallet_name, self.wallet_data_filename)
        )
        shutil.copyfile(
            os.path.join(self.nodes[1].datadir, 'wallet.bak'),
            os.path.join(self.nodes[1].datadir, self.chain, 'wallets', self.default_wallet_name, self.wallet_data_filename)
        )
        shutil.copyfile(
            os.path.join(self.nodes[2].datadir, 'wallet.bak'),
            os.path.join(self.nodes[2].datadir, self.chain, 'wallets', self.default_wallet_name, self.wallet_data_filename)
        )

        self.log.info("Re-starting nodes")
        self.start_node(0)
        self.start_node(1)
        self.start_node(2)
        self.start_node(3)
        self.connect_nodes(0, 3)
        self.connect_nodes(1, 3)
        self.connect_nodes(2, 3)
        self.connect_nodes(2, 0)

        assert_equal(self.nodes[0].getbalance(), balance0)
        assert_equal(self.nodes[1].getbalance(), balance1)
        assert_equal(self.nodes[2].getbalance(), balance2)

        self.log.info("Restoring using dumped wallet")
        self.stop_node(0)
        self.stop_node(1)
        self.stop_node(2)

        # Get wallet file paths
        wallet_file_0 = os.path.join(self.nodes[0].datadir, self.chain, 'wallets', self.default_wallet_name, self.wallet_data_filename)
        wallet_file_1 = os.path.join(self.nodes[1].datadir, self.chain, 'wallets', self.default_wallet_name, self.wallet_data_filename)
        wallet_file_2 = os.path.join(self.nodes[2].datadir, self.chain, 'wallets', self.default_wallet_name, self.wallet_data_filename)
        
        # Remove the wallet files
        os.remove(wallet_file_0)
        os.remove(wallet_file_1)
        os.remove(wallet_file_2)

        self.start_node(0)
        self.start_node(1)
        self.start_node(2)

        assert_equal(self.nodes[0].getbalance(), 0)
        assert_equal(self.nodes[1].getbalance(), 0)
        assert_equal(self.nodes[2].getbalance(), 0)

        self.nodes[0].importwallet(os.path.join(self.nodes[0].datadir, 'wallet.dump'))
        self.nodes[1].importwallet(os.path.join(self.nodes[1].datadir, 'wallet.dump'))
        self.nodes[2].importwallet(os.path.join(self.nodes[2].datadir, 'wallet.dump'))

        self.sync_blocks()

        assert_equal(self.nodes[0].getbalance(), balance0)
        assert_equal(self.nodes[1].getbalance(), balance1)
        assert_equal(self.nodes[2].getbalance(), balance2)

        # Backup to a different file
        self.nodes[2].backupwallet(os.path.join(self.nodes[2].datadir, 'wallet.bak2'))

        self.stop_node(2)
        
        # Remove the wallet file  
        wallet_file_2 = os.path.join(self.nodes[2].datadir, self.chain, 'wallets', self.default_wallet_name, self.wallet_data_filename)
        os.remove(wallet_file_2)

        # Restore from the second backup
        shutil.copyfile(
            os.path.join(self.nodes[2].datadir, 'wallet.bak2'),
            wallet_file_2
        )

        self.start_node(2)
        self.connect_nodes(0, 2)

        assert_equal(self.nodes[2].getbalance(), balance2)

        # Test backup to invalid path
        target_dir = os.path.join(self.nodes[0].datadir, "invalid_backup_path")
        assert_raises_rpc_error(-4, "backup failed", self.nodes[0].backupwallet, target_dir)

        self.log.info("Backup and restore tests completed successfully!")


if __name__ == '__main__':
    WalletBackupTest().main()