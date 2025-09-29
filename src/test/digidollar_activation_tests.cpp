// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/digidollar.h>
#include <consensus/params.h>
#include <kernel/chainparams.h>
#include <chainparams.h>
#include <test/util/setup_common.h>
#include <versionbits.h>
#include <chain.h>
#include <util/time.h>

#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

// Test constants for better readability and maintainability
namespace {
    constexpr int64_t TEST_START_TIME = 1000000000;     // Past time for activation
    constexpr int64_t TEST_TIMEOUT = 3000000000;        // Far future timeout
    constexpr int64_t TEST_BLOCK_TIME = 1500000000;     // Test block timestamp
    constexpr uint32_t TEST_THRESHOLD = 3;              // Simplified threshold for testing
    constexpr uint32_t TEST_WINDOW = 4;                 // Simplified window for testing
}

// Forward declarations for helper functions we'll implement
static CBlockIndex* CreateTestBlock(CBlockIndex* pprev, int32_t nVersion, int64_t nTime);
static void SetMockTimeForTesting(int64_t nMockTime);
static Consensus::Params CreateTestParams(int64_t startTime, int64_t timeout);

BOOST_FIXTURE_TEST_SUITE(digidollar_activation_tests, RegTestingSetup)

/**
 * Test BIP9 state transitions for DigiDollar deployment.
 * REFACTOR Phase: Clean implementation with helper functions.
 */
BOOST_AUTO_TEST_CASE(test_bip9_state_transitions)
{
    // Create standardized test parameters
    Consensus::Params testParams = CreateTestParams(TEST_START_TIME, TEST_TIMEOUT);

    VersionBitsCache cache;
    CBlockIndex* tip = nullptr;

    // Create genesis block - should be STARTED since we're past starttime
    tip = CreateTestBlock(nullptr, VERSIONBITS_TOP_BITS, TEST_BLOCK_TIME);
    ThresholdState state = cache.State(tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::STARTED);

    // Generate blocks without signaling - should remain STARTED
    for (int i = 0; i < TEST_WINDOW; i++) {
        tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, TEST_BLOCK_TIME + i);
    }
    state = cache.State(tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::STARTED);

    // Generate blocks WITH signaling to trigger LOCKED_IN
    uint32_t signal_bit = 1 << testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit;
    int32_t signaling_version = VERSIONBITS_TOP_BITS | signal_bit;

    // Generate 3 out of 4 blocks with signaling (>= 75% threshold)
    for (int i = 0; i < 3; i++) {
        tip = CreateTestBlock(tip, signaling_version, 1500000100 + i);
    }
    tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1500000103); // One non-signaling

    state = cache.State(tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::LOCKED_IN);

    // Generate another window to move to ACTIVE
    for (int i = 0; i < 4; i++) {
        tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1500000200 + i);
    }

    state = cache.State(tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::ACTIVE);
}

/**
 * Test activation threshold mechanics.
 * GREEN Phase: Simplified test with smaller numbers.
 */
BOOST_AUTO_TEST_CASE(test_activation_threshold)
{
    const auto& consensusParams = Params().GetConsensus();

    // Create test params with smaller window for easier testing
    Consensus::Params testParams = consensusParams;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = 1000000000;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = 3000000000;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 0;
    testParams.nRuleChangeActivationThreshold = 3; // 3 out of 4 = 75%
    testParams.nMinerConfirmationWindow = 4;

    VersionBitsCache cache;
    CBlockIndex* tip = nullptr;

    tip = CreateTestBlock(nullptr, VERSIONBITS_TOP_BITS, 1500000000);

    uint32_t signal_bit = 1 << testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit;
    int32_t signaling_version = VERSIONBITS_TOP_BITS | signal_bit;

    // Test just below threshold (2 out of 4 = 50% < 75%)
    tip = CreateTestBlock(tip, signaling_version, 1500000001);
    tip = CreateTestBlock(tip, signaling_version, 1500000002);
    tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1500000003);
    tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1500000004);

    ThresholdState state = cache.State(tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::STARTED);

    // Test at threshold (3 out of 4 = 75%)
    tip = CreateTestBlock(tip, signaling_version, 1500000005);
    tip = CreateTestBlock(tip, signaling_version, 1500000006);
    tip = CreateTestBlock(tip, signaling_version, 1500000007);
    tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1500000008);

    state = cache.State(tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::LOCKED_IN);
}

/**
 * Test pre/post activation behavior for DigiDollar functionality.
 * GREEN Phase: Simplified test for basic enable/disable behavior.
 */
BOOST_AUTO_TEST_CASE(test_pre_post_activation_behavior)
{
    const auto& consensusParams = Params().GetConsensus();

    // Create simple test params
    Consensus::Params testParams = consensusParams;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = 1000000000;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = 3000000000;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 0;
    testParams.nRuleChangeActivationThreshold = 3;
    testParams.nMinerConfirmationWindow = 4;

    CBlockIndex* tip = nullptr;

    // Test before activation - create a block in STARTED state
    tip = CreateTestBlock(nullptr, VERSIONBITS_TOP_BITS, 1500000000);

    // Should not be enabled in STARTED state
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(tip, testParams));

    // Simulate activation by creating blocks that reach ACTIVE state
    uint32_t signal_bit = 1 << testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit;
    int32_t signaling_version = VERSIONBITS_TOP_BITS | signal_bit;

    // Create signaling blocks
    for (int i = 0; i < 3; i++) {
        tip = CreateTestBlock(tip, signaling_version, 1500000001 + i);
    }
    tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1500000004);

    // Still in LOCKED_IN, should not be enabled yet
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(tip, testParams));

    // Complete activation
    for (int i = 0; i < 4; i++) {
        tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1500000010 + i);
    }

    // Now should be ACTIVE and enabled
    BOOST_CHECK(DigiDollar::IsDigiDollarEnabled(tip, testParams));
}

/**
 * Test activation rollback scenarios during chain reorganization.
 * GREEN Phase: Basic test with simplified chain handling.
 */
BOOST_AUTO_TEST_CASE(test_activation_rollback)
{
    const auto& consensusParams = Params().GetConsensus();

    // Create test params
    Consensus::Params testParams = consensusParams;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = 1000000000;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = 3000000000;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 0;
    testParams.nRuleChangeActivationThreshold = 3;
    testParams.nMinerConfirmationWindow = 4;

    VersionBitsCache cache;

    // Create main chain with activation
    CBlockIndex* main_tip = CreateTestBlock(nullptr, VERSIONBITS_TOP_BITS, 1500000000);

    uint32_t signal_bit = 1 << testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].bit;
    int32_t signaling_version = VERSIONBITS_TOP_BITS | signal_bit;

    // Build chain to LOCKED_IN
    for (int i = 0; i < 3; i++) {
        main_tip = CreateTestBlock(main_tip, signaling_version, 1500000001 + i);
    }
    main_tip = CreateTestBlock(main_tip, VERSIONBITS_TOP_BITS, 1500000004);

    ThresholdState state = cache.State(main_tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::LOCKED_IN);

    // Create alternative chain without activation
    CBlockIndex* alt_tip = CreateTestBlock(nullptr, VERSIONBITS_TOP_BITS, 1500000000);

    for (int i = 0; i < 4; i++) {
        alt_tip = CreateTestBlock(alt_tip, VERSIONBITS_TOP_BITS, 1500000001 + i);
    }

    // Alternative chain should still be in STARTED state
    state = cache.State(alt_tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::STARTED);

    // Test that cache properly handles different chain tips
    state = cache.State(main_tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::LOCKED_IN);
}

/**
 * Test timeout scenarios where deployment fails to activate.
 * GREEN Phase: Simple timeout test.
 */
BOOST_AUTO_TEST_CASE(test_activation_timeout)
{
    const auto& consensusParams = Params().GetConsensus();

    // Create test params with short timeout
    Consensus::Params testParams = consensusParams;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = 1000000000;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = 1500000000; // Short timeout
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 0;
    testParams.nRuleChangeActivationThreshold = 3;
    testParams.nMinerConfirmationWindow = 4;

    VersionBitsCache cache;
    CBlockIndex* tip = nullptr;

    // Generate blocks past timeout without sufficient signaling
    tip = CreateTestBlock(nullptr, VERSIONBITS_TOP_BITS, 1600000000); // Past timeout

    // Generate blocks past timeout
    for (int i = 0; i < 4; i++) {
        tip = CreateTestBlock(tip, VERSIONBITS_TOP_BITS, 1600000000 + i);
    }

    ThresholdState state = cache.State(tip, testParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(state, ThresholdState::FAILED);

    // Verify DigiDollar is not enabled in FAILED state
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(tip, testParams));
}

BOOST_AUTO_TEST_SUITE_END()

// REFACTOR Phase - Improved helper functions with better design

namespace {
    // Static storage for test blocks - ensures memory management
    std::vector<std::unique_ptr<CBlockIndex>> g_test_blocks;

    // Constants for better readability and maintainability
    constexpr uint32_t DEFAULT_REGTEST_BITS = 0x207fffff;
}

/**
 * Create a test block for BIP9 activation testing.
 * REFACTOR Phase - Improved with better documentation and validation.
 *
 * @param pprev Previous block index (nullptr for genesis)
 * @param nVersion Block version (should include version bits)
 * @param nTime Block timestamp
 * @return Pointer to created block index (managed by static storage)
 */
static CBlockIndex* CreateTestBlock(CBlockIndex* pprev, int32_t nVersion, int64_t nTime)
{
    auto pindex = std::make_unique<CBlockIndex>();

    // Initialize block index fields
    pindex->pprev = pprev;
    pindex->nVersion = nVersion;
    pindex->nTime = nTime;
    pindex->nHeight = pprev ? pprev->nHeight + 1 : 0;
    pindex->nBits = DEFAULT_REGTEST_BITS;

    // Set median time past - for BIP9 evaluation
    // In real implementation this would be calculated from previous blocks
    pindex->nTimeMax = nTime;

    // Store the raw pointer before moving the unique_ptr
    CBlockIndex* result = pindex.get();
    g_test_blocks.push_back(std::move(pindex));

    return result;
}

/**
 * Set mock time for testing (placeholder).
 * REFACTOR Phase - Better documentation of future implementation.
 *
 * In a full implementation, this would integrate with the node's
 * mock time infrastructure for deterministic testing.
 *
 * @param nMockTime Unix timestamp to set as mock time
 */
static void SetMockTimeForTesting(int64_t nMockTime)
{
    // Future implementation would call SetMockTime(nMockTime)
    // For now, this is a placeholder for the interface
    (void)nMockTime;
}

/**
 * Create standardized test parameters for DigiDollar activation testing.
 * REFACTOR Phase - Centralized parameter creation for consistency.
 *
 * @param startTime Deployment start time (default: TEST_START_TIME)
 * @param timeout Deployment timeout (default: TEST_TIMEOUT)
 * @return Configured consensus parameters for testing
 */
static Consensus::Params CreateTestParams(int64_t startTime = TEST_START_TIME,
                                         int64_t timeout = TEST_TIMEOUT)
{
    const auto& consensusParams = Params().GetConsensus();
    Consensus::Params testParams = consensusParams;

    // Configure DigiDollar deployment
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime = startTime;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nTimeout = timeout;
    testParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height = 0;

    // Use simplified thresholds for testing
    testParams.nRuleChangeActivationThreshold = TEST_THRESHOLD;
    testParams.nMinerConfirmationWindow = TEST_WINDOW;

    return testParams;
}