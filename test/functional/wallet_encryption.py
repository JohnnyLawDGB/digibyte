#!/usr/bin/env python3
<<<<<<< HEAD
# Copyright (c) 2016-2020 The DigiByte Core developers
=======
# Copyright (c) 2016-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Wallet encryption"""

import time

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_raises_rpc_error,
    assert_equal,
)
from test_framework.wallet_util import WalletUnlock


class WalletEncryptionTest(DigiByteTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

<<<<<<< HEAD

class WalletEncryptionTest(DigiByteTestFramework):
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        passphrase = "WalletPassphrase"
        passphrase2 = "SecondWalletPassphrase"

        # Make sure the wallet isn't encrypted first
        msg = "test message"
        address = self.nodes[0].getnewaddress(address_type='legacy')
        sig = self.nodes[0].signmessage(address, msg)
        assert self.nodes[0].verifymessage(address, sig, msg)
        assert_raises_rpc_error(-15, "Error: running with an unencrypted wallet, but walletpassphrase was called", self.nodes[0].walletpassphrase, 'ff', 1)
        assert_raises_rpc_error(-15, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.", self.nodes[0].walletpassphrasechange, 'ff', 'ff')

        # Encrypt the wallet
<<<<<<< HEAD
        assert_raises_rpc_error(-8, "passphrase can not be empty", self.nodes[0].encryptwallet, '')
=======
        assert_raises_rpc_error(-8, "passphrase cannot be empty", self.nodes[0].encryptwallet, '')
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        self.nodes[0].encryptwallet(passphrase)

        # Test that the wallet is encrypted
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase with walletpassphrase first", self.nodes[0].signmessage, address, msg)
        assert_raises_rpc_error(-15, "Error: running with an encrypted wallet, but encryptwallet was called.", self.nodes[0].encryptwallet, 'ff')
<<<<<<< HEAD
        assert_raises_rpc_error(-8, "passphrase can not be empty", self.nodes[0].walletpassphrase, '', 1)
        assert_raises_rpc_error(-8, "passphrase can not be empty", self.nodes[0].walletpassphrasechange, '', 'ff')
=======
        assert_raises_rpc_error(-8, "passphrase cannot be empty", self.nodes[0].walletpassphrase, '', 1)
        assert_raises_rpc_error(-8, "passphrase cannot be empty", self.nodes[0].walletpassphrasechange, '', 'ff')
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

        # Check that walletpassphrase works
        self.nodes[0].walletpassphrase(passphrase, 2)
        sig = self.nodes[0].signmessage(address, msg)
        assert self.nodes[0].verifymessage(address, sig, msg)

        # Check that the timeout is right
        time.sleep(3)
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase with walletpassphrase first", self.nodes[0].signmessage, address, msg)

        # Test wrong passphrase
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect", self.nodes[0].walletpassphrase, passphrase + "wrong", 10)

        # Test walletlock
<<<<<<< HEAD
        self.nodes[0].walletpassphrase(passphrase, 84600)
        sig = self.nodes[0].signmessage(address, msg)
        assert self.nodes[0].verifymessage(address, sig, msg)
        self.nodes[0].walletlock()
=======
        with WalletUnlock(self.nodes[0], passphrase):
            sig = self.nodes[0].signmessage(address, msg)
            assert self.nodes[0].verifymessage(address, sig, msg)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase with walletpassphrase first", self.nodes[0].signmessage, address, msg)

        # Test passphrase changes
        self.nodes[0].walletpassphrasechange(passphrase, passphrase2)
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect", self.nodes[0].walletpassphrase, passphrase, 10)
<<<<<<< HEAD
        self.nodes[0].walletpassphrase(passphrase2, 10)
        sig = self.nodes[0].signmessage(address, msg)
        assert self.nodes[0].verifymessage(address, sig, msg)
        self.nodes[0].walletlock()
=======
        with WalletUnlock(self.nodes[0], passphrase2):
            sig = self.nodes[0].signmessage(address, msg)
            assert self.nodes[0].verifymessage(address, sig, msg)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

        # Test timeout bounds
        assert_raises_rpc_error(-8, "Timeout cannot be negative.", self.nodes[0].walletpassphrase, passphrase2, -10)

        self.log.info('Check a timeout less than the limit')
        MAX_VALUE = 100000000
        now = int(time.time())
        self.nodes[0].setmocktime(now)
        expected_time = now + MAX_VALUE - 600
        self.nodes[0].walletpassphrase(passphrase2, MAX_VALUE - 600)
        # give buffer for walletpassphrase, since it iterates over all encrypted keys
        expected_time_with_buffer = time.time() + MAX_VALUE - 600
        actual_time = self.nodes[0].getwalletinfo()['unlocked_until']
<<<<<<< HEAD
        assert_greater_than_or_equal(actual_time, expected_time)
        assert_greater_than(expected_time_with_buffer, actual_time)

        self.log.info('Check a timeout greater than the limit')
        expected_time = int(time.time()) + MAX_VALUE - 1
=======
        assert_equal(actual_time, expected_time)

        self.log.info('Check a timeout greater than the limit')
        expected_time = now + MAX_VALUE
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        self.nodes[0].walletpassphrase(passphrase2, MAX_VALUE + 1000)
        expected_time_with_buffer = time.time() + MAX_VALUE
        actual_time = self.nodes[0].getwalletinfo()['unlocked_until']
<<<<<<< HEAD
        assert_greater_than_or_equal(actual_time, expected_time)
        assert_greater_than(expected_time_with_buffer, actual_time)
=======
        assert_equal(actual_time, expected_time)
        self.nodes[0].walletlock()

        # Test passphrase with null characters
        passphrase_with_nulls = "Phrase\0With\0Nulls"
        self.nodes[0].walletpassphrasechange(passphrase2, passphrase_with_nulls)
        # walletpassphrasechange should not stop at null characters
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect", self.nodes[0].walletpassphrase, passphrase_with_nulls.partition("\0")[0], 10)
        with WalletUnlock(self.nodes[0], passphrase_with_nulls):
            sig = self.nodes[0].signmessage(address, msg)
            assert self.nodes[0].verifymessage(address, sig, msg)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion


if __name__ == '__main__':
    WalletEncryptionTest().main()
