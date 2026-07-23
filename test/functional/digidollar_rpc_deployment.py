#!/usr/bin/env python3
# Copyright (c) 2025-2026 The DigiByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test DigiDollar deployment info RPC (getdigidollardeploymentinfo).

DigiDollar is a buried deployment (BIP90): the RPC reports the hardcoded
per-network activation height instead of BIP9 signaling state. On default
regtest the buried deployment height is 0, so the deployment is active from
genesis while the static DD/oracle height gates stay at 650.
"""

from test_framework.test_framework import DigiByteTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal


class DigiDollarRPCDeploymentTest(DigiByteTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-digidollar=1", "-txindex=1", "-mocktime=0", "-dandelion=0"]]

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Testing DigiDollar deployment info RPC...")
        node = self.nodes[0]

        self.log.info("Generating initial blocks for test setup...")
        self.generate(node, 110)

        self.test_deployment_info_basic()
        self.test_deployment_info_fields()
        self.test_deployment_status_values()
        self.test_deployment_after_activation()
        self.test_deployment_info_oracle_activation_fields()

        self.log.info("All deployment info tests passed!")

    def test_deployment_info_basic(self):
        self.log.info("Testing basic deployment info response...")
        node = self.nodes[0]

        result = node.getdigidollardeploymentinfo()

        assert 'enabled' in result, "Missing 'enabled' field"
        assert 'type' in result, "Missing 'type' field"
        assert 'status' in result, "Missing 'status' field"
        # BIP9 signaling fields are gone with the burial.
        for removed in ('bit', 'start_time', 'timeout', 'min_activation_height',
                        'blocks_until_timeout', 'signaling_blocks', 'threshold',
                        'period_blocks', 'progress_percent'):
            assert removed not in result, f"Stale BIP9 field '{removed}' present"

        self.log.info(f"Deployment enabled: {result['enabled']}")
        self.log.info(f"Deployment status: {result['status']}")

    def test_deployment_info_fields(self):
        self.log.info("Testing deployment info field types...")
        node = self.nodes[0]

        result = node.getdigidollardeploymentinfo()

        assert isinstance(result['enabled'], bool), "enabled should be boolean"
        assert isinstance(result['type'], str), "type should be string"
        assert isinstance(result['status'], str), "status should be string"
        assert_equal(result['type'], 'buried')
        # The deployment is enabled on regtest, so the buried activation
        # height must be reported.
        assert 'activation_height' in result, "Missing 'activation_height' field"
        assert isinstance(result['activation_height'], int), "activation_height should be integer"
        assert_greater_than_or_equal(result['activation_height'], 0)

        self.log.info("All field types verified correctly")

    def test_deployment_status_values(self):
        self.log.info("Testing deployment status is valid...")
        node = self.nodes[0]

        result = node.getdigidollardeploymentinfo()

        # Buried deployments only ever report defined (below the activation
        # height) or active (at/after it); the BIP9 started/locked_in/failed
        # states no longer exist for DigiDollar.
        valid_statuses = ['defined', 'active']
        assert result['status'] in valid_statuses, f"Invalid status: {result['status']}"

        self.log.info(f"Status '{result['status']}' is valid")

        if result['status'] == 'active':
            assert result['enabled'] == True, "If active, enabled should be True"
        else:
            assert result['enabled'] == False, "If defined, enabled should be False"

        # Default regtest buries DigiDollar at height 0: active from genesis.
        assert_equal(result['status'], 'active')
        assert_equal(result['activation_height'], 0)
        self.log.info(f"Activation height: {result['activation_height']}")

    def test_deployment_info_oracle_activation_fields(self):
        # Wave 9 (Agent C): operators need a single RPC that exposes the
        # MuSig2 oracle activation height and the active roster shape so
        # they can correlate `nDigiDollarMuSig2Height`, `nOracleConsensusRequired`,
        # and `nOraclePubkeyCount` with the deployment status. Before
        # this fix the RPC only exposed deployment fields; operators had to
        # read chainparams source to discover the oracle quorum.
        self.log.info("Testing deployment info exposes oracle activation fields...")
        node = self.nodes[0]
        result = node.getdigidollardeploymentinfo()

        for field in (
                "oracle_activation_height",
                "musig2_format_activation_height",
                "oracle_pubkey_count",
                "oracle_consensus_required",
                "oracle_total_slots"):
            assert field in result, f"Missing '{field}' field — operator cannot see oracle roster shape"
            assert isinstance(result[field], int), f"'{field}' should be integer"

        # Default regtest keeps oracle/DD height gates at 650, but the buried
        # DigiDollar deployment height is 0. MuSig2 follows that effective
        # DigiDollar boundary so v0x03 quotes are valid whenever DD is active.
        assert_equal(result["oracle_activation_height"], 650)
        assert_equal(result["musig2_format_activation_height"], 0)
        # Regtest 4-of-7 quorum.
        assert_equal(result["oracle_pubkey_count"], 7)
        assert_equal(result["oracle_consensus_required"], 4)
        # Regtest configures 7 oracle slots.
        assert_equal(result["oracle_total_slots"], 7)

        self.log.info(
            "Oracle activation fields verified: oracle_height=%d, musig2_height=%d, consensus=%d-of-%d, slots=%d" % (
                result["oracle_activation_height"],
                result["musig2_format_activation_height"],
                result["oracle_consensus_required"],
                result["oracle_pubkey_count"],
                result["oracle_total_slots"]))

    def test_deployment_after_activation(self):
        self.log.info("Testing deployment info consistency...")
        node = self.nodes[0]

        result1 = node.getdigidollardeploymentinfo()

        self.generate(node, 10)

        result2 = node.getdigidollardeploymentinfo()

        # The buried deployment parameters are constants: they must not
        # change as the chain advances.
        assert_equal(result1['type'], result2['type'])
        assert_equal(result1['activation_height'], result2['activation_height'])

        if result1['status'] == 'active':
            assert_equal(result1['status'], result2['status'])
            assert_equal(result1['enabled'], result2['enabled'])

        self.log.info("Deployment info is consistent across blocks")


if __name__ == '__main__':
    DigiDollarRPCDeploymentTest().main()
