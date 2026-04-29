#!/usr/bin/env python3
"""Test DigiDollar oracle key generation in descriptor wallets.

Tests the createoraclekey RPC command which generates oracle keypairs
and stores them inside a descriptor wallet for use with startoracle.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
import re


class DigiDollarOracleKeygenTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-digidollar=1", "-txindex=1", "-dandelion=0"]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]

        # Generate blocks past coinbase maturity
        self.log.info("Generating initial blocks...")
        node.generate(110)

        self.log.info("Creating descriptor wallet for oracle keys...")
        node.createwallet("oracle_test")
        wallet = node.get_wallet_rpc("oracle_test")

        # --- DD-RH-028: listoracle no-running path must satisfy RPC schema ---
        self.log.info("Test: listoracle should return a clean no-oracle-running result")
        list_result = node.listoracle()
        assert_equal(list_result["running"], False)
        assert "No oracle is running" in list_result["message"]

        # --- DD-RH-026: createoraclekey must reject disabled-private-key wallets ---
        self.log.info("Test: createoraclekey should reject disabled-private-key wallets")
        node.createwallet(wallet_name="oracle_watchonly", disable_private_keys=True)
        watch_wallet = node.get_wallet_rpc("oracle_watchonly")
        assert_equal(watch_wallet.getwalletinfo()["private_keys_enabled"], False)
        assert_raises_rpc_error(-4, "Private keys are disabled", watch_wallet.createoraclekey, 0)

        # --- Test 1: createoraclekey 0 succeeds ---
        self.log.info("Test: createoraclekey 0 should succeed")
        result = wallet.createoraclekey(0)
        assert_equal(result["oracle_id"], 0)
        assert_equal(result["stored_in_wallet"], True)

        pubkey = result["pubkey"]
        pubkey_xonly = result["pubkey_xonly"]

        # --- Test 2: pubkey format (66 hex chars, starts with 02 or 03) ---
        self.log.info("Test: pubkey format validation")
        assert_equal(len(pubkey), 66)
        assert pubkey[:2] in ("02", "03"), f"pubkey must start with 02 or 03, got {pubkey[:2]}"
        assert re.fullmatch(r'[0-9a-f]{66}', pubkey), "pubkey must be lowercase hex"

        # --- Test 3: pubkey_xonly format (64 hex chars) ---
        self.log.info("Test: pubkey_xonly format validation")
        assert_equal(len(pubkey_xonly), 64)
        assert re.fullmatch(r'[0-9a-f]{64}', pubkey_xonly), "pubkey_xonly must be lowercase hex"

        # --- Test 4: pubkey_xonly == pubkey without prefix ---
        self.log.info("Test: pubkey_xonly equals pubkey without 02/03 prefix")
        assert_equal(pubkey_xonly, pubkey[2:])

        # --- Test 5: duplicate createoraclekey 0 should fail ---
        self.log.info("Test: createoraclekey 0 again should fail (key exists)")
        assert_raises_rpc_error(None, "already exists", wallet.createoraclekey, 0)

        # --- Test 6: createoraclekey 1 succeeds with different key ---
        self.log.info("Test: createoraclekey 1 should succeed with different pubkey")
        result1 = wallet.createoraclekey(1)
        assert_equal(result1["oracle_id"], 1)
        assert_equal(result1["stored_in_wallet"], True)
        assert result1["pubkey"] != pubkey, "oracle 1 should have different pubkey than oracle 0"

        # --- Test 7: createoraclekey 30 should fail (max oracle_id is 29) ---
        self.log.info("Test: createoraclekey 30 should fail (invalid oracle_id)")
        assert_raises_rpc_error(None, None, wallet.createoraclekey, 30)

        # --- Test 8: startoracle 0 without private_key loads from wallet ---
        # On regtest, chainparams oracle keys are test keys, so the wallet-generated
        # key won't match. We expect a pubkey mismatch error.
        self.log.info("Test: startoracle 0 from wallet should fail with pubkey mismatch on regtest")
        try:
            start_result = wallet.startoracle(0)
            self.log.info(f"startoracle 0 returned: {start_result}")
            assert_equal(start_result["oracle_id"], 0)
            assert_equal(start_result["success"], False)
            assert_equal(start_result["status"], "stopped")
            assert_equal(start_result["initialized"], True)
            assert "price thread not active" in start_result["message"]
        except Exception as e:
            err_msg = str(e)
            self.log.info(f"startoracle 0 failed as expected: {err_msg}")
            assert "internal bug detected" not in err_msg.lower(), err_msg
            # Should mention pubkey mismatch or similar
            assert "pubkey" in err_msg.lower() or "mismatch" in err_msg.lower() or "key" in err_msg.lower(), \
                f"Expected pubkey-related error, got: {err_msg}"

        # --- Test 9: Key persistence across wallet unload/reload ---
        self.log.info("Test: oracle key persists across wallet unload/reload")
        wallet.unloadwallet()
        node.loadwallet("oracle_test")
        wallet = node.get_wallet_rpc("oracle_test")

        # Creating key for oracle 0 again should fail (proves key persisted)
        assert_raises_rpc_error(None, "already exists", wallet.createoraclekey, 0)

        # startoracle 0 should still attempt to load the key from wallet
        # (same pubkey mismatch error proves key was loaded from wallet)
        try:
            start_result = wallet.startoracle(0)
            self.log.info(f"startoracle 0 after reload returned: {start_result}")
            assert_equal(start_result["oracle_id"], 0)
            assert_equal(start_result["success"], False)
            assert_equal(start_result["status"], "stopped")
            assert_equal(start_result["initialized"], True)
            assert "price thread not active" in start_result["message"]
        except Exception as e:
            err_msg = str(e)
            self.log.info(f"startoracle 0 after reload failed as expected: {err_msg}")
            assert "internal bug detected" not in err_msg.lower(), err_msg
            assert "pubkey" in err_msg.lower() or "mismatch" in err_msg.lower() or "key" in err_msg.lower(), \
                f"Expected pubkey-related error after reload, got: {err_msg}"

        self.log.info("All oracle keygen tests passed!")


if __name__ == '__main__':
    DigiDollarOracleKeygenTest().main()
