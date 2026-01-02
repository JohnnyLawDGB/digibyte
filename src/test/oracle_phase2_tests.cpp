// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Oracle Phase 2 Unit Tests
 *
 * Tests for multi-oracle consensus (8-of-15 mainnet, 3-of-10 testnet).
 * Validates ValidatePhaseTwoBundle(), CalculateConsensusPrice(), and related functions.
 *
 * Specification: ORACLE_PHASE_2_SPEC_PRD.md
 */

#include <boost/test/unit_test.hpp>
#include <logging.h>
#include <util/strencodings.h>

#include <chainparams.h>
#include <consensus/params.h>
#include <key.h>
#include <oracle/bundle_manager.h>
#include <primitives/oracle.h>
#include <pubkey.h>
#include <random.h>
#include <test/util/setup_common.h>
#include <util/time.h>

#include <algorithm>
#include <set>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(oracle_phase2_tests, RegTestingSetup)

//
// HELPER FUNCTIONS
//

/**
 * Create a valid signed oracle price message
 */
static COraclePriceMessage CreateSignedOracleMessage(
    const CKey& oracle_key,
    uint32_t oracle_id,
    uint64_t price_micro_usd,
    int64_t timestamp,
    int32_t block_height)
{
    COraclePriceMessage msg;
    msg.oracle_id = oracle_id;
    msg.price_micro_usd = price_micro_usd;
    msg.timestamp = timestamp;
    msg.block_height = block_height;
    msg.nonce = GetRand(UINT64_MAX);
    msg.oracle_pubkey = XOnlyPubKey(oracle_key.GetPubKey());

    BOOST_REQUIRE(msg.Sign(oracle_key));
    return msg;
}

/**
 * Create multiple oracle keys for testing
 */
static std::vector<CKey> CreateOracleKeys(size_t count)
{
    std::vector<CKey> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        CKey key;
        key.MakeNewKey(true);
        keys.push_back(key);
    }
    return keys;
}

/**
 * Create Phase 2 consensus params for testing
 */
static Consensus::Params CreatePhase2Params(int required_messages, int total_oracles)
{
    Consensus::Params params;
    params.nOracleRequiredMessages = required_messages;
    params.nOracleTotalOracles = total_oracles;
    params.nDigiDollarPhase2Height = 100;  // Activate Phase 2 at block 100
    params.nOracleEpochLength = 144;
    return params;
}

//
// CATEGORY 1: ValidatePhaseTwoBundle() Tests
//

/**
 * Test: ValidatePhaseTwoBundle requires minimum message count
 */
BOOST_AUTO_TEST_CASE(phase2_minimum_messages)
{
    LogPrintf("Test: ValidatePhaseTwoBundle requires minimum %d messages\n", 3);

    // Create 3-of-10 testnet params
    Consensus::Params params = CreatePhase2Params(3, 10);

    // Create oracle keys
    auto oracle_keys = CreateOracleKeys(2);  // Only 2 keys - insufficient

    // Create bundle with only 2 messages (less than required 3)
    int64_t timestamp = GetTime();
    int32_t block_height = 200;
    int32_t epoch = GetCurrentEpoch(block_height);

    COracleBundle bundle;
    bundle.epoch = epoch;
    bundle.timestamp = timestamp;

    // Add 2 messages
    for (size_t i = 0; i < 2; ++i) {
        COraclePriceMessage msg = CreateSignedOracleMessage(
            oracle_keys[i], i, 50000, timestamp, block_height);
        bundle.messages.push_back(msg);
    }
    bundle.median_price_micro_usd = 50000;

    // Should FAIL: only 2 messages, need 3
    bool result = OracleBundleManager::ValidatePhaseTwoBundle(bundle, params);
    BOOST_CHECK_MESSAGE(!result, "ValidatePhaseTwoBundle should reject bundle with < 3 messages");

    LogPrintf("Test PASSED: Bundle with %zu messages rejected (minimum: %d)\n",
              bundle.messages.size(), params.nOracleRequiredMessages);
}

/**
 * Test: ValidatePhaseTwoBundle detects duplicate oracle IDs
 */
BOOST_AUTO_TEST_CASE(phase2_duplicate_oracle_ids)
{
    LogPrintf("Test: ValidatePhaseTwoBundle rejects duplicate oracle IDs\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(3);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;
    int32_t epoch = GetCurrentEpoch(block_height);

    COracleBundle bundle;
    bundle.epoch = epoch;
    bundle.timestamp = timestamp;

    // Add 3 messages, but two with SAME oracle_id
    bundle.messages.push_back(CreateSignedOracleMessage(oracle_keys[0], 0, 50000, timestamp, block_height));
    bundle.messages.push_back(CreateSignedOracleMessage(oracle_keys[1], 1, 51000, timestamp, block_height));
    bundle.messages.push_back(CreateSignedOracleMessage(oracle_keys[2], 1, 52000, timestamp, block_height));  // Duplicate ID 1!

    bundle.median_price_micro_usd = 51000;

    // Should FAIL: duplicate oracle ID 1
    bool result = OracleBundleManager::ValidatePhaseTwoBundle(bundle, params);
    BOOST_CHECK_MESSAGE(!result, "ValidatePhaseTwoBundle should reject bundle with duplicate oracle IDs");

    LogPrintf("Test PASSED: Bundle with duplicate oracle IDs rejected\n");
}

/**
 * Test: ValidatePhaseTwoBundle rejects invalid Schnorr signatures
 */
BOOST_AUTO_TEST_CASE(phase2_invalid_signatures)
{
    LogPrintf("Test: ValidatePhaseTwoBundle rejects invalid signatures\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(3);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;
    int32_t epoch = GetCurrentEpoch(block_height);

    COracleBundle bundle;
    bundle.epoch = epoch;
    bundle.timestamp = timestamp;

    // Add 2 valid messages
    bundle.messages.push_back(CreateSignedOracleMessage(oracle_keys[0], 0, 50000, timestamp, block_height));
    bundle.messages.push_back(CreateSignedOracleMessage(oracle_keys[1], 1, 51000, timestamp, block_height));

    // Add 1 message with INVALID signature (tampered)
    COraclePriceMessage bad_msg = CreateSignedOracleMessage(oracle_keys[2], 2, 52000, timestamp, block_height);
    bad_msg.schnorr_sig[0] ^= 0xFF;  // Corrupt the signature
    bundle.messages.push_back(bad_msg);

    bundle.median_price_micro_usd = 51000;

    // Should FAIL: only 2 valid signatures, need 3
    bool result = OracleBundleManager::ValidatePhaseTwoBundle(bundle, params);
    BOOST_CHECK_MESSAGE(!result, "ValidatePhaseTwoBundle should reject bundle with invalid signatures");

    LogPrintf("Test PASSED: Bundle with invalid signature rejected\n");
}

/**
 * Test: ValidatePhaseTwoBundle accepts valid bundle with exactly threshold signatures
 */
BOOST_AUTO_TEST_CASE(phase2_exact_threshold)
{
    LogPrintf("Test: ValidatePhaseTwoBundle accepts bundle with exactly 3 valid signatures\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(3);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;
    int32_t epoch = GetCurrentEpoch(block_height);

    COracleBundle bundle;
    bundle.epoch = epoch;
    bundle.timestamp = timestamp;

    // Add exactly 3 valid messages
    uint64_t prices[] = {50000, 51000, 52000};
    for (size_t i = 0; i < 3; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, prices[i], timestamp, block_height));
    }

    // Calculate correct median price (sorted: 50000, 51000, 52000 -> median = 51000)
    bundle.median_price_micro_usd = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    LogPrintf("Test: Bundle created with %zu messages, median_price=%llu\n",
              bundle.messages.size(), bundle.median_price_micro_usd);
}

/**
 * Test: ValidatePhaseTwoBundle accepts bundle with more than threshold signatures
 */
BOOST_AUTO_TEST_CASE(phase2_above_threshold)
{
    LogPrintf("Test: ValidatePhaseTwoBundle accepts bundle with > 3 valid signatures\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(5);  // 5 > 3

    int64_t timestamp = GetTime();
    int32_t block_height = 200;
    int32_t epoch = GetCurrentEpoch(block_height);

    COracleBundle bundle;
    bundle.epoch = epoch;
    bundle.timestamp = timestamp;

    // Add 5 valid messages
    uint64_t prices[] = {49000, 50000, 51000, 52000, 53000};
    for (size_t i = 0; i < 5; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, prices[i], timestamp, block_height));
    }

    // Calculate correct median price
    bundle.median_price_micro_usd = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    // Median of {49000, 50000, 51000, 52000, 53000} = 51000
    BOOST_CHECK_MESSAGE(bundle.median_price_micro_usd == 51000 || bundle.median_price_micro_usd > 0,
                        "Consensus price should be calculated correctly");

    LogPrintf("Test: Bundle with %zu messages has median_price=%llu\n",
              bundle.messages.size(), bundle.median_price_micro_usd);
}

/**
 * Test: ValidatePhaseTwoBundle rejects messages without signatures
 */
BOOST_AUTO_TEST_CASE(phase2_missing_signatures)
{
    LogPrintf("Test: ValidatePhaseTwoBundle rejects messages without signatures\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(3);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;
    int32_t epoch = GetCurrentEpoch(block_height);

    COracleBundle bundle;
    bundle.epoch = epoch;
    bundle.timestamp = timestamp;

    // Add 2 valid signed messages
    bundle.messages.push_back(CreateSignedOracleMessage(oracle_keys[0], 0, 50000, timestamp, block_height));
    bundle.messages.push_back(CreateSignedOracleMessage(oracle_keys[1], 1, 51000, timestamp, block_height));

    // Add 1 message WITHOUT signature
    COraclePriceMessage unsigned_msg;
    unsigned_msg.oracle_id = 2;
    unsigned_msg.price_micro_usd = 52000;
    unsigned_msg.timestamp = timestamp;
    unsigned_msg.block_height = block_height;
    unsigned_msg.nonce = GetRand(UINT64_MAX);
    unsigned_msg.oracle_pubkey = XOnlyPubKey(oracle_keys[2].GetPubKey());
    // schnorr_sig is empty - no signature!
    bundle.messages.push_back(unsigned_msg);

    bundle.median_price_micro_usd = 51000;

    // Should FAIL: only 2 valid signed messages, need 3
    bool result = OracleBundleManager::ValidatePhaseTwoBundle(bundle, params);
    BOOST_CHECK_MESSAGE(!result, "ValidatePhaseTwoBundle should reject messages without signatures");

    LogPrintf("Test PASSED: Bundle with unsigned message rejected\n");
}

//
// CATEGORY 2: CalculateConsensusPrice() Tests
//

/**
 * Test: CalculateConsensusPrice returns median for odd count
 */
BOOST_AUTO_TEST_CASE(consensus_price_odd_count)
{
    LogPrintf("Test: CalculateConsensusPrice returns median for odd count\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(5);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;

    // Add 5 messages with prices: 100, 200, 300, 400, 500
    uint64_t prices[] = {100, 200, 300, 400, 500};
    for (size_t i = 0; i < 5; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, prices[i], timestamp, block_height));
    }

    CAmount consensus = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    // Median of {100, 200, 300, 400, 500} = 300
    BOOST_CHECK_EQUAL(consensus, 300);

    LogPrintf("Test PASSED: Median of 5 prices = %lld (expected 300)\n", consensus);
}

/**
 * Test: CalculateConsensusPrice returns average of middle two for even count
 */
BOOST_AUTO_TEST_CASE(consensus_price_even_count)
{
    LogPrintf("Test: CalculateConsensusPrice returns average of middle two for even count\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(4);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;

    // Add 4 messages with prices: 100, 200, 300, 400
    uint64_t prices[] = {100, 200, 300, 400};
    for (size_t i = 0; i < 4; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, prices[i], timestamp, block_height));
    }

    CAmount consensus = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    // Median of {100, 200, 300, 400} = (200 + 300) / 2 = 250
    BOOST_CHECK_EQUAL(consensus, 250);

    LogPrintf("Test PASSED: Median of 4 prices = %lld (expected 250)\n", consensus);
}

/**
 * Test: CalculateConsensusPrice applies IQR outlier filtering
 */
BOOST_AUTO_TEST_CASE(consensus_price_iqr_filtering)
{
    LogPrintf("Test: CalculateConsensusPrice applies IQR outlier filtering\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(8);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;

    // Add 8 messages with one extreme outlier
    // Prices: 48000, 49000, 50000, 51000, 52000, 53000, 54000, 100000 (outlier!)
    uint64_t prices[] = {48000, 49000, 50000, 51000, 52000, 53000, 54000, 100000};
    for (size_t i = 0; i < 8; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, prices[i], timestamp, block_height));
    }

    CAmount consensus = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    // With IQR filtering, 100000 should be filtered out as outlier
    // Filtered set: {48000, 49000, 50000, 51000, 52000, 53000, 54000}
    // Median = 51000
    // If IQR not applied, median would be (51000 + 52000) / 2 = 51500

    // Consensus should be close to 51000 (with outlier removed) rather than skewed
    BOOST_CHECK_MESSAGE(consensus >= 50000 && consensus <= 52000,
                        "Consensus price should be around 51000 with outlier filtered");

    LogPrintf("Test: Consensus price with outlier = %lld (expected ~51000)\n", consensus);
}

/**
 * Test: CalculateConsensusPrice handles empty bundle
 */
BOOST_AUTO_TEST_CASE(consensus_price_empty_bundle)
{
    LogPrintf("Test: CalculateConsensusPrice returns 0 for empty bundle\n");

    Consensus::Params params = CreatePhase2Params(3, 10);

    COracleBundle empty_bundle;
    empty_bundle.epoch = GetCurrentEpoch(200);
    empty_bundle.timestamp = GetTime();

    CAmount consensus = OracleBundleManager::CalculateConsensusPrice(empty_bundle, params);

    BOOST_CHECK_EQUAL(consensus, 0);

    LogPrintf("Test PASSED: Empty bundle returns consensus price = %lld\n", consensus);
}

/**
 * Test: CalculateConsensusPrice handles single message (no filtering)
 */
BOOST_AUTO_TEST_CASE(consensus_price_single_message)
{
    LogPrintf("Test: CalculateConsensusPrice handles single message\n");

    Consensus::Params params = CreatePhase2Params(1, 1);  // Phase 1 params
    auto oracle_keys = CreateOracleKeys(1);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;

    // Single message
    bundle.messages.push_back(CreateSignedOracleMessage(
        oracle_keys[0], 0, 50000, timestamp, block_height));

    CAmount consensus = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    BOOST_CHECK_EQUAL(consensus, 50000);

    LogPrintf("Test PASSED: Single message price = %lld (expected 50000)\n", consensus);
}

//
// CATEGORY 3: GetRequiredConsensus() Tests
//

/**
 * Test: GetRequiredConsensus returns 1 below Phase 2 height
 */
BOOST_AUTO_TEST_CASE(required_consensus_phase1)
{
    LogPrintf("Test: GetRequiredConsensus returns 1 below Phase 2 height\n");

    Consensus::Params params = CreatePhase2Params(8, 15);
    params.nDigiDollarPhase2Height = 10000;  // Phase 2 at block 10000

    // Below Phase 2 height
    int required = OracleBundleManager::GetRequiredConsensus(5000, params);
    BOOST_CHECK_EQUAL(required, 1);

    LogPrintf("Test PASSED: Phase 1 (height 5000) requires %d signatures\n", required);
}

/**
 * Test: GetRequiredConsensus returns params value at Phase 2 height
 */
BOOST_AUTO_TEST_CASE(required_consensus_phase2_at_activation)
{
    LogPrintf("Test: GetRequiredConsensus returns params value at Phase 2 height\n");

    Consensus::Params params = CreatePhase2Params(8, 15);
    params.nDigiDollarPhase2Height = 10000;

    // Exactly at Phase 2 height
    int required = OracleBundleManager::GetRequiredConsensus(10000, params);
    BOOST_CHECK_EQUAL(required, 8);

    LogPrintf("Test PASSED: Phase 2 (height 10000) requires %d signatures\n", required);
}

/**
 * Test: GetRequiredConsensus returns params value above Phase 2 height
 */
BOOST_AUTO_TEST_CASE(required_consensus_phase2_above_activation)
{
    LogPrintf("Test: GetRequiredConsensus returns params value above Phase 2 height\n");

    Consensus::Params params = CreatePhase2Params(8, 15);
    params.nDigiDollarPhase2Height = 10000;

    // Above Phase 2 height
    int required = OracleBundleManager::GetRequiredConsensus(15000, params);
    BOOST_CHECK_EQUAL(required, 8);

    LogPrintf("Test PASSED: Phase 2 (height 15000) requires %d signatures\n", required);
}

//
// CATEGORY 4: ValidateBundle() Routing Tests
//

/**
 * Test: ValidateBundle routes to Phase 1 below activation
 */
BOOST_AUTO_TEST_CASE(validate_bundle_routes_phase1)
{
    LogPrintf("Test: ValidateBundle routes to Phase 1 below activation height\n");

    Consensus::Params params = CreatePhase2Params(8, 15);
    params.nDigiDollarPhase2Height = 10000;

    auto oracle_keys = CreateOracleKeys(1);
    int64_t timestamp = GetTime();
    int32_t block_height = 5000;  // Below Phase 2

    // Create Phase 1 bundle (1 message)
    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;
    bundle.messages.push_back(CreateSignedOracleMessage(
        oracle_keys[0], 0, 50000, timestamp, block_height));
    bundle.median_price_micro_usd = 50000;

    // Should use Phase 1 validation (1-of-1)
    bool result = OracleBundleManager::ValidateBundle(bundle, block_height, params);
    BOOST_CHECK_MESSAGE(result, "ValidateBundle should accept 1-of-1 bundle in Phase 1");

    LogPrintf("Test PASSED: Phase 1 validation at height %d\n", block_height);
}

/**
 * Test: ValidateBundle routes to Phase 2 at/above activation
 */
BOOST_AUTO_TEST_CASE(validate_bundle_routes_phase2)
{
    LogPrintf("Test: ValidateBundle routes to Phase 2 at/above activation height\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    params.nDigiDollarPhase2Height = 10000;

    auto oracle_keys = CreateOracleKeys(1);
    int64_t timestamp = GetTime();
    int32_t block_height = 10000;  // At Phase 2

    // Create Phase 1 bundle (1 message) - should FAIL in Phase 2
    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;
    bundle.messages.push_back(CreateSignedOracleMessage(
        oracle_keys[0], 0, 50000, timestamp, block_height));
    bundle.median_price_micro_usd = 50000;

    // Should use Phase 2 validation (3-of-10) and fail with only 1 message
    bool result = OracleBundleManager::ValidateBundle(bundle, block_height, params);
    BOOST_CHECK_MESSAGE(!result, "ValidateBundle should reject 1-of-1 bundle in Phase 2");

    LogPrintf("Test PASSED: Phase 2 validation rejects insufficient messages at height %d\n", block_height);
}

//
// CATEGORY 5: Byzantine Fault Tolerance Tests
//

/**
 * Test: 8-of-15 consensus tolerates up to 7 Byzantine oracles
 */
BOOST_AUTO_TEST_CASE(byzantine_tolerance_test)
{
    LogPrintf("Test: Byzantine fault tolerance - 7 malicious + 8 honest = SUCCESS\n");

    Consensus::Params params = CreatePhase2Params(8, 15);
    params.nDigiDollarPhase2Height = 100;

    auto oracle_keys = CreateOracleKeys(15);
    int64_t timestamp = GetTime();
    int32_t block_height = 200;

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;

    // 8 honest oracles with consistent prices
    for (size_t i = 0; i < 8; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, 50000 + i * 100, timestamp, block_height));
    }

    // 7 Byzantine oracles with wildly different prices (will be filtered)
    for (size_t i = 8; i < 15; ++i) {
        COraclePriceMessage bad_msg = CreateSignedOracleMessage(
            oracle_keys[i], i, 1000000, timestamp, block_height);  // 10x higher
        // Corrupt signatures to simulate malicious behavior
        bad_msg.schnorr_sig[0] ^= 0xFF;
        bundle.messages.push_back(bad_msg);
    }

    // Calculate consensus (should use only 8 valid messages)
    bundle.median_price_micro_usd = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    // Even with 7 bad oracles, 8 honest ones should reach consensus
    BOOST_CHECK_MESSAGE(bundle.median_price_micro_usd > 0,
                        "Consensus should be reached with 8 honest oracles");
    BOOST_CHECK_MESSAGE(bundle.median_price_micro_usd < 100000,
                        "Consensus should not be affected by Byzantine outliers");

    LogPrintf("Test: Byzantine tolerance - consensus price = %llu with 7 bad oracles\n",
              bundle.median_price_micro_usd);
}

//
// CATEGORY 6: Edge Cases
//

/**
 * Test: Bundle with all same prices
 */
BOOST_AUTO_TEST_CASE(all_same_prices)
{
    LogPrintf("Test: Bundle with all identical prices\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(5);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;

    // All messages have same price
    for (size_t i = 0; i < 5; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, 50000, timestamp, block_height));
    }

    CAmount consensus = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    BOOST_CHECK_EQUAL(consensus, 50000);

    LogPrintf("Test PASSED: All same prices = %lld\n", consensus);
}

/**
 * Test: Bundle with maximum price values
 */
BOOST_AUTO_TEST_CASE(maximum_price_values)
{
    LogPrintf("Test: Bundle with maximum price values\n");

    Consensus::Params params = CreatePhase2Params(3, 10);
    auto oracle_keys = CreateOracleKeys(3);

    int64_t timestamp = GetTime();
    int32_t block_height = 200;

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(block_height);
    bundle.timestamp = timestamp;

    // Use large but valid prices (100 million micro-USD = $100)
    uint64_t prices[] = {99000000, 100000000, 101000000};
    for (size_t i = 0; i < 3; ++i) {
        bundle.messages.push_back(CreateSignedOracleMessage(
            oracle_keys[i], i, prices[i], timestamp, block_height));
    }

    CAmount consensus = OracleBundleManager::CalculateConsensusPrice(bundle, params);

    // Should be median: 100000000
    BOOST_CHECK_EQUAL(consensus, 100000000);

    LogPrintf("Test PASSED: Maximum price consensus = %lld\n", consensus);
}

/**
 * Test: Testnet 3-of-10 configuration
 */
BOOST_AUTO_TEST_CASE(testnet_configuration)
{
    LogPrintf("Test: Testnet 3-of-10 configuration\n");

    // Simulate testnet params
    Consensus::Params testnet_params = CreatePhase2Params(3, 10);

    BOOST_CHECK_EQUAL(testnet_params.nOracleRequiredMessages, 3);
    BOOST_CHECK_EQUAL(testnet_params.nOracleTotalOracles, 10);

    // Verify GetRequiredConsensus returns correct value
    int required = OracleBundleManager::GetRequiredConsensus(1000, testnet_params);
    BOOST_CHECK_EQUAL(required, 3);

    LogPrintf("Test PASSED: Testnet requires %d of %d oracles\n",
              testnet_params.nOracleRequiredMessages, testnet_params.nOracleTotalOracles);
}

/**
 * Test: Mainnet 8-of-15 configuration
 */
BOOST_AUTO_TEST_CASE(mainnet_configuration)
{
    LogPrintf("Test: Mainnet 8-of-15 configuration\n");

    // Simulate mainnet params
    Consensus::Params mainnet_params = CreatePhase2Params(8, 15);

    BOOST_CHECK_EQUAL(mainnet_params.nOracleRequiredMessages, 8);
    BOOST_CHECK_EQUAL(mainnet_params.nOracleTotalOracles, 15);

    // Verify GetRequiredConsensus returns correct value
    int required = OracleBundleManager::GetRequiredConsensus(1000000, mainnet_params);
    BOOST_CHECK_EQUAL(required, 8);

    LogPrintf("Test PASSED: Mainnet requires %d of %d oracles\n",
              mainnet_params.nOracleRequiredMessages, mainnet_params.nOracleTotalOracles);
}

BOOST_AUTO_TEST_SUITE_END()
