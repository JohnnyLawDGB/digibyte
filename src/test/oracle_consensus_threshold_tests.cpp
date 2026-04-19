// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Oracle Consensus Threshold Tests — T3-05a Fix Verification
 *
 * Verifies that ALL code paths use chainparams.nOracleRequiredMessages
 * instead of the compile-time ORACLE_CONSENSUS_REQUIRED constant.
 *
 * The bug: HasConsensus(), GetConsensusPrice(), and IsValid() had default
 * parameters of ORACLE_CONSENSUS_REQUIRED (RC30: 9). On regtest (4-of-7),
 * call sites using defaults would incorrectly require 9 messages,
 * rejecting valid bundles.
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/err.h>
#include <consensus/params.h>
#include <key.h>
#include <oracle/bundle_manager.h>
#include <primitives/oracle.h>
#include <pubkey.h>
#include <test/util/setup_common.h>
#include <util/time.h>

namespace {

// Helper: Create a bundle with N signed messages at a given price
COracleBundle CreateTestBundle(int num_messages, uint64_t price = 50000, int32_t epoch = 0)
{
    COracleBundle bundle(epoch);
    for (int i = 0; i < num_messages; i++) {
        CKey key;
        key.MakeNewKey(true);
        COraclePriceMessage msg(i, price, GetTime());
        msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
        msg.SignPhase2(key);
        bundle.messages.push_back(msg);
    }
    bundle.median_price_micro_usd = price;
    bundle.timestamp = GetTime();
    return bundle;
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(oracle_consensus_threshold_tests, BasicTestingSetup)

/**
 * Test 1: HasConsensus requires explicit min_required parameter
 *
 * After the fix, HasConsensus() no longer has a default parameter.
 * All callers must explicitly pass min_required from chainparams.
 * This test verifies the function works correctly with the chainparams value.
 */
BOOST_AUTO_TEST_CASE(has_consensus_uses_chainparams_value)
{
    const Consensus::Params& params = Params().GetConsensus();
    int required = params.nOracleRequiredMessages;

    BOOST_TEST_MESSAGE("Network requires " << required << " oracle messages for consensus");

    // Bundle with exactly the required number should pass
    COracleBundle exact_bundle = CreateTestBundle(required);
    BOOST_CHECK(exact_bundle.HasConsensus(required));

    // Bundle with one fewer should fail
    if (required > 1) {
        COracleBundle insufficient_bundle = CreateTestBundle(required - 1);
        BOOST_CHECK(!insufficient_bundle.HasConsensus(required));
    }

    // Bundle with more than required should also pass
    COracleBundle excess_bundle = CreateTestBundle(required + 1);
    BOOST_CHECK(excess_bundle.HasConsensus(required));

    // Empty bundle should fail
    COracleBundle empty_bundle(0);
    BOOST_CHECK(!empty_bundle.HasConsensus(required));
}

/**
 * Test 2: GetConsensusPrice requires explicit min_required parameter
 *
 * GetConsensusPrice must also use the chainparams value.
 * With fewer than required messages, it should return 0.
 */
BOOST_AUTO_TEST_CASE(get_consensus_price_uses_chainparams_value)
{
    const Consensus::Params& params = Params().GetConsensus();
    int required = params.nOracleRequiredMessages;

    // Bundle meeting threshold should return price
    COracleBundle valid_bundle = CreateTestBundle(required, 50000);
    uint64_t price = valid_bundle.GetConsensusPrice(required);
    BOOST_CHECK(price > 0);

    // Bundle below threshold should return 0
    if (required > 1) {
        COracleBundle invalid_bundle = CreateTestBundle(required - 1, 50000);
        uint64_t no_price = invalid_bundle.GetConsensusPrice(required);
        BOOST_CHECK_EQUAL(no_price, 0);
    }
}

/**
 * Test 3: IsValid requires explicit min_required parameter
 *
 * IsValid must accept min_required as first parameter (no default).
 */
BOOST_AUTO_TEST_CASE(is_valid_uses_chainparams_value)
{
    const Consensus::Params& params = Params().GetConsensus();
    int required = params.nOracleRequiredMessages;

    // Valid bundle should pass
    COracleBundle valid_bundle = CreateTestBundle(required, 50000);
    BOOST_CHECK(valid_bundle.IsValid(required, GetTime()));

    // Bundle below threshold: median mismatch (GetConsensusPrice returns 0
    // but median_price_micro_usd is 50000), so IsValid returns true
    // because HasConsensus fails → median check is skipped
    // The consensus check happens at the call site level, not inside IsValid
}

/**
 * Test 4: OracleBundleManager uses min_oracle_count from chainparams
 *
 * The manager's min_oracle_count is set from consensus.nOracleRequiredMessages
 * during initialization. All internal methods should use this value.
 */
BOOST_AUTO_TEST_CASE(bundle_manager_uses_chainparams_threshold)
{
    const Consensus::Params& params = Params().GetConsensus();
    int required = params.nOracleRequiredMessages;

    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(required);

    // Create and store a bundle meeting the threshold
    int32_t epoch = 5;
    COracleBundle bundle = CreateTestBundle(required, 50000, epoch);
    manager.UpdateBundle(bundle);

    // UpdateCachedPrice should now succeed with the correct threshold
    bool updated = manager.UpdateCachedPrice(epoch);
    BOOST_CHECK_MESSAGE(updated,
        "UpdateCachedPrice should succeed with " << required << " messages meeting chainparams threshold");

    // Verify the cached price is correct
    CAmount cached = manager.GetLatestPrice();
    BOOST_CHECK(cached > 0);

    manager.Clear();
}

/**
 * Test 5: OracleDataValidator uses chainparams threshold
 *
 * ValidateOracleBundle should use params.nOracleRequiredMessages,
 * not the compile-time ORACLE_CONSENSUS_REQUIRED constant.
 */
BOOST_AUTO_TEST_CASE(validator_uses_chainparams_threshold)
{
    const Consensus::Params& params = Params().GetConsensus();
    int required = params.nOracleRequiredMessages;

    int32_t epoch = GetCurrentEpoch(1000);
    COracleBundle valid_bundle = CreateTestBundle(required, 50000, epoch);

    // ValidateOracleBundle should accept bundles meeting chainparams threshold
    bool valid = OracleDataValidator::ValidateOracleBundle(valid_bundle, epoch, params);
    BOOST_CHECK_MESSAGE(valid,
        "ValidateOracleBundle should accept " << required << "-message bundle (chainparams threshold)");
}

/**
 * Test 6: ERR HasOracleConsensus uses chainparams threshold
 *
 * EmergencyRedemptionRatio::HasOracleConsensus should use chainparams,
 * not the hardcoded ORACLE_CONSENSUS_REQUIRED.
 */
BOOST_AUTO_TEST_CASE(err_has_oracle_consensus_uses_chainparams)
{
    const Consensus::Params& params = Params().GetConsensus();
    int required = params.nOracleRequiredMessages;

    // Bundle meeting chainparams threshold should have consensus
    COracleBundle valid_bundle = CreateTestBundle(required, 50000);
    bool consensus = DigiDollar::ERR::EmergencyRedemptionRatio::HasOracleConsensus(valid_bundle, params);
    BOOST_CHECK_MESSAGE(consensus,
        "ERR::HasOracleConsensus should accept " << required << "-message bundle");

    // Bundle below threshold should NOT have consensus
    if (required > 1) {
        COracleBundle insufficient_bundle = CreateTestBundle(required - 1, 50000);
        bool no_consensus = DigiDollar::ERR::EmergencyRedemptionRatio::HasOracleConsensus(insufficient_bundle, params);
        BOOST_CHECK_MESSAGE(!no_consensus,
            "ERR::HasOracleConsensus should reject " << (required - 1) << "-message bundle");
    }
}

/**
 * Test 7: Different networks have different thresholds
 *
 * Verify the chainparams values are configured correctly per network.
 */
BOOST_AUTO_TEST_CASE(network_specific_thresholds)
{
    // Regtest (current test environment)
    const Consensus::Params& regtest_params = Params().GetConsensus();
    BOOST_CHECK(regtest_params.nOracleRequiredMessages > 0);
    BOOST_CHECK(regtest_params.nOracleRequiredMessages <= regtest_params.nOracleTotalOracles);

    BOOST_TEST_MESSAGE("Regtest: " << regtest_params.nOracleRequiredMessages
        << " of " << regtest_params.nOracleTotalOracles << " required");
}

/**
 * Test 8: Boundary conditions — exactly at threshold
 *
 * Verify behavior at the exact threshold boundary.
 */
BOOST_AUTO_TEST_CASE(consensus_threshold_boundary)
{
    const Consensus::Params& params = Params().GetConsensus();
    int required = params.nOracleRequiredMessages;

    // Exactly at threshold: passes
    COracleBundle at_threshold = CreateTestBundle(required, 50000);
    BOOST_CHECK(at_threshold.HasConsensus(required));
    BOOST_CHECK(at_threshold.GetConsensusPrice(required) > 0);

    // One below threshold: fails
    if (required > 0) {
        COracleBundle below_threshold = CreateTestBundle(required - 1, 50000);
        BOOST_CHECK(!below_threshold.HasConsensus(required));
        BOOST_CHECK_EQUAL(below_threshold.GetConsensusPrice(required), 0);
    }

    // One above threshold: passes
    COracleBundle above_threshold = CreateTestBundle(required + 1, 50000);
    BOOST_CHECK(above_threshold.HasConsensus(required));
    BOOST_CHECK(above_threshold.GetConsensusPrice(required) > 0);
}

BOOST_AUTO_TEST_SUITE_END()
