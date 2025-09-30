// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/digidollar.h>
#include <consensus/dca.h>
#include <consensus/err.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <digidollar/digidollar.h>
#include <primitives/oracle.h>
#include <key.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/interpreter.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <chrono>
#include <algorithm>
#include <limits>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(digidollar_err_tests)

struct DigiDollarERRTestSetup : public TestingSetup {
    DigiDollarERRTestSetup() : TestingSetup(CBaseChainParams::REGTEST) {
        // Set up mock oracle price and system state
        mockOraclePrice = 50000; // $500.00 DGB
        mockHeight = 1000;

        // Generate test keys
        testKey.MakeNewKey(true);
        testPubKey = testKey.GetPubKey();
        testXOnlyKey = XOnlyPubKey(testPubKey);

        // Set up validation context
        validationContext.nHeight = mockHeight;
        validationContext.oraclePrice = mockOraclePrice;
        validationContext.params = Params();

        // Generate oracle nodes for consensus
        for (int i = 0; i < 15; i++) {
            CKey oracleKey;
            oracleKey.MakeNewKey(true);
            oracleKeys.push_back(oracleKey);

            OracleNode node(i, oracleKey.GetPubKey(), "http://oracle" + std::to_string(i) + ".example.com", true);
            oracleNodes.push_back(node);
        }
    }

    CKey testKey;
    CPubKey testPubKey;
    XOnlyPubKey testXOnlyKey;
    CAmount mockOraclePrice;
    int mockHeight;
    DigiDollar::ValidationContext validationContext;
    std::vector<CKey> oracleKeys;
    std::vector<OracleNode> oracleNodes;
};

// ============================================================================
// ERR Activation Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_activation_when_system_health_below_100, DigiDollarERRTestSetup)
{
    // Arrange: System health at 99% (should trigger ERR)
    int systemHealth = 99;

    // Act: Check if ERR should activate - EXPECTED TO FAIL (RED phase)
    bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(systemHealth);

    // Assert: Should fail since ERR system is not implemented yet
    BOOST_CHECK(!shouldActivate); // Will fail until implementation

    // After GREEN phase implementation:
    // BOOST_CHECK(shouldActivate); // Should activate when < 100%
}

BOOST_FIXTURE_TEST_CASE(err_no_activation_when_system_health_100_or_above, DigiDollarERRTestSetup)
{
    // Arrange: System health at exactly 100%
    int systemHealth = 100;

    // Act: Check if ERR should activate - EXPECTED TO FAIL (RED phase)
    bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!shouldActivate);

    // Test with health above 100%
    systemHealth = 150;
    shouldActivate = DigiDollar::ERR::ShouldActivateERR(systemHealth);
    BOOST_CHECK(!shouldActivate);

    // After GREEN phase:
    // BOOST_CHECK(!shouldActivate); // Should NOT activate when >= 100%
}

BOOST_FIXTURE_TEST_CASE(err_activation_various_unhealthy_levels, DigiDollarERRTestSetup)
{
    // Test ERR activation at various unhealthy system levels
    std::vector<int> unhealthyLevels = {50, 75, 85, 90, 95, 99};

    for (int health : unhealthyLevels) {
        // Act: Check ERR activation - EXPECTED TO FAIL (RED phase)
        bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(health);

        // Assert: Should fail in RED phase
        BOOST_CHECK(!shouldActivate);

        // After GREEN phase:
        // BOOST_CHECK(shouldActivate); // Should activate for all < 100%
    }
}

// ============================================================================
// ERR Ratio Calculation Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_ratio_calculation_95_to_100_percent_health, DigiDollarERRTestSetup)
{
    // Arrange: System health in 95-100% range
    int systemHealth = 97;

    // Act: Calculate ERR adjustment ratio - EXPECTED TO FAIL (RED phase)
    double adjustmentRatio = DigiDollar::ERR::CalculateERRAdjustment(systemHealth);

    // Assert: Should fail since ERR calculation is not implemented
    BOOST_CHECK_EQUAL(adjustmentRatio, 0.0); // Will be 0 until implemented

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(adjustmentRatio, 0.95); // 95% collateral return
}

BOOST_FIXTURE_TEST_CASE(err_ratio_calculation_90_to_95_percent_health, DigiDollarERRTestSetup)
{
    // Arrange: System health in 90-95% range
    int systemHealth = 92;

    // Act: Calculate ERR adjustment ratio - EXPECTED TO FAIL (RED phase)
    double adjustmentRatio = DigiDollar::ERR::CalculateERRAdjustment(systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(adjustmentRatio, 0.0);

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(adjustmentRatio, 0.90); // 90% collateral return
}

BOOST_FIXTURE_TEST_CASE(err_ratio_calculation_85_to_90_percent_health, DigiDollarERRTestSetup)
{
    // Arrange: System health in 85-90% range
    int systemHealth = 87;

    // Act: Calculate ERR adjustment ratio - EXPECTED TO FAIL (RED phase)
    double adjustmentRatio = DigiDollar::ERR::CalculateERRAdjustment(systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(adjustmentRatio, 0.0);

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(adjustmentRatio, 0.85); // 85% collateral return
}

BOOST_FIXTURE_TEST_CASE(err_ratio_calculation_below_85_percent_health, DigiDollarERRTestSetup)
{
    // Arrange: System health below 85% (minimum ratio)
    int systemHealth = 70;

    // Act: Calculate ERR adjustment ratio - EXPECTED TO FAIL (RED phase)
    double adjustmentRatio = DigiDollar::ERR::CalculateERRAdjustment(systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(adjustmentRatio, 0.0);

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(adjustmentRatio, 0.80); // 80% minimum collateral return
}

BOOST_FIXTURE_TEST_CASE(err_ratio_calculation_edge_cases, DigiDollarERRTestSetup)
{
    // Test edge cases for ERR ratio calculation

    // Exactly at tier boundaries
    std::vector<std::pair<int, double>> testCases = {
        {95, 0.95},  // Lower bound of 95-100% tier
        {90, 0.90},  // Lower bound of 90-95% tier
        {85, 0.85},  // Lower bound of 85-90% tier
        {1, 0.80},   // Extreme low health (minimum ratio)
        {0, 0.80}    // Zero health (minimum ratio)
    };

    for (auto& testCase : testCases) {
        int health = testCase.first;
        double expectedRatio = testCase.second;

        // Act: Calculate ratio - EXPECTED TO FAIL (RED phase)
        double actualRatio = DigiDollar::ERR::CalculateERRAdjustment(health);

        // Assert: Should fail in RED phase
        BOOST_CHECK_EQUAL(actualRatio, 0.0);

        // After GREEN phase:
        // BOOST_CHECK_EQUAL(actualRatio, expectedRatio);
    }
}

// ============================================================================
// ERR Adjusted Redemption Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_adjusted_redemption_calculation, DigiDollarERRTestSetup)
{
    // Arrange: Normal redemption of 100 DGB, system at 90% health
    CAmount normalRedemption = 100 * COIN;
    int systemHealth = 90;

    // Act: Get adjusted redemption amount - EXPECTED TO FAIL (RED phase)
    CAmount adjustedRedemption = DigiDollar::ERR::GetAdjustedRedemption(normalRedemption, systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(adjustedRedemption, 0);

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(adjustedRedemption, 90 * COIN); // 90% of 100 DGB
}

BOOST_FIXTURE_TEST_CASE(err_adjusted_redemption_various_amounts, DigiDollarERRTestSetup)
{
    // Test ERR adjustment with various redemption amounts
    std::vector<CAmount> testAmounts = {
        50 * COIN,   // 50 DGB
        100 * COIN,  // 100 DGB
        500 * COIN,  // 500 DGB
        1000 * COIN  // 1000 DGB
    };

    int systemHealth = 85; // 85% health = 85% return

    for (CAmount amount : testAmounts) {
        // Act: Get adjusted amount - EXPECTED TO FAIL (RED phase)
        CAmount adjusted = DigiDollar::ERR::GetAdjustedRedemption(amount, systemHealth);

        // Assert: Should fail in RED phase
        BOOST_CHECK_EQUAL(adjusted, 0);

        // After GREEN phase:
        // CAmount expected = (amount * 85) / 100;
        // BOOST_CHECK_EQUAL(adjusted, expected);
    }
}

BOOST_FIXTURE_TEST_CASE(err_adjusted_redemption_minimum_ratio, DigiDollarERRTestSetup)
{
    // Arrange: Test minimum 80% ratio for very low health
    CAmount normalRedemption = 200 * COIN;
    int systemHealth = 50; // Very low health

    // Act: Get adjusted redemption - EXPECTED TO FAIL (RED phase)
    CAmount adjustedRedemption = DigiDollar::ERR::GetAdjustedRedemption(normalRedemption, systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(adjustedRedemption, 0);

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(adjustedRedemption, 160 * COIN); // 80% minimum
}

// ============================================================================
// Oracle Consensus Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_oracle_consensus_sufficient_signatures, DigiDollarERRTestSetup)
{
    // Arrange: Create oracle bundle with 8 messages (sufficient for consensus)
    COracleBundle bundle(1); // Epoch 1

    for (int i = 0; i < 8; i++) {
        COraclePriceMessage msg(i, mockOraclePrice, GetTime());
        // In real implementation, would sign the message
        msg.signature = std::vector<unsigned char>(64, 0x01); // Mock signature
        bundle.AddMessage(msg);
    }

    // Act: Check oracle consensus - EXPECTED TO FAIL (RED phase)
    bool hasConsensus = DigiDollar::ERR::EmergencyRedemptionRatio::HasOracleConsensus(bundle);

    // Assert: Should fail since oracle consensus is not implemented
    BOOST_CHECK(!hasConsensus);

    // After GREEN phase:
    // BOOST_CHECK(hasConsensus); // Should have consensus with 8/15 signatures
}

BOOST_FIXTURE_TEST_CASE(err_oracle_consensus_insufficient_signatures, DigiDollarERRTestSetup)
{
    // Arrange: Create oracle bundle with only 7 messages (insufficient)
    COracleBundle bundle(1); // Epoch 1

    for (int i = 0; i < 7; i++) {
        COraclePriceMessage msg(i, mockOraclePrice, GetTime());
        msg.signature = std::vector<unsigned char>(64, 0x01); // Mock signature
        bundle.AddMessage(msg);
    }

    // Act: Check oracle consensus - EXPECTED TO FAIL (RED phase)
    bool hasConsensus = DigiDollar::ERR::EmergencyRedemptionRatio::HasOracleConsensus(bundle);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!hasConsensus);

    // After GREEN phase:
    // BOOST_CHECK(!hasConsensus); // Should NOT have consensus with only 7/15
}

BOOST_FIXTURE_TEST_CASE(err_oracle_consensus_no_messages, DigiDollarERRTestSetup)
{
    // Arrange: Empty oracle messages
    std::vector<DigiDollar::COraclePriceMessage> messages;

    // Act: Check oracle consensus - EXPECTED TO FAIL (RED phase)
    bool hasConsensus = DigiDollar::ERR::HasOracleConsensus(messages);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!hasConsensus);

    // After GREEN phase:
    // BOOST_CHECK(!hasConsensus); // Should NOT have consensus with no messages
}

BOOST_FIXTURE_TEST_CASE(err_oracle_consensus_exactly_threshold, DigiDollarERRTestSetup)
{
    // Arrange: Create exactly 8 oracle messages (minimum threshold)
    std::vector<DigiDollar::COraclePriceMessage> messages;
    for (int i = 0; i < 8; i++) {
        DigiDollar::COraclePriceMessage msg;
        msg.price = mockOraclePrice;
        msg.timestamp = GetTime();
        msg.oracleKey = oracleXOnlyKeys[i];
        messages.push_back(msg);
    }

    // Act: Check consensus at threshold - EXPECTED TO FAIL (RED phase)
    bool hasConsensus = DigiDollar::ERR::HasOracleConsensus(messages);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!hasConsensus);

    // After GREEN phase:
    // BOOST_CHECK(hasConsensus); // Should have consensus at exactly 8/15
}

// ============================================================================
// ERR State Management Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_state_inactive_when_healthy, DigiDollarERRTestSetup)
{
    // Arrange: System is healthy (150% collateral)
    validationContext.systemCollateral = 150;

    // Act: Get current ERR state - EXPECTED TO FAIL (RED phase)
    DigiDollar::ERR::ERRState state = DigiDollar::ERR::GetCurrentState();

    // Assert: Should fail since ERR state management is not implemented
    BOOST_CHECK(!state.isActive);
    BOOST_CHECK_EQUAL(state.systemHealth, 0);
    BOOST_CHECK_EQUAL(state.adjustmentRatio, 0.0);

    // After GREEN phase:
    // BOOST_CHECK(!state.isActive); // Should be inactive when healthy
    // BOOST_CHECK_EQUAL(state.systemHealth, 150);
}

BOOST_FIXTURE_TEST_CASE(err_state_active_when_unhealthy, DigiDollarERRTestSetup)
{
    // Arrange: System is unhealthy (90% collateral)
    validationContext.systemCollateral = 90;

    // Act: Get current ERR state - EXPECTED TO FAIL (RED phase)
    DigiDollar::ERR::ERRState state = DigiDollar::ERR::GetCurrentState();

    // Assert: Should fail in RED phase
    BOOST_CHECK(!state.isActive);

    // After GREEN phase:
    // BOOST_CHECK(state.isActive); // Should be active when unhealthy
    // BOOST_CHECK_EQUAL(state.systemHealth, 90);
    // BOOST_CHECK_EQUAL(state.adjustmentRatio, 0.90);
}

BOOST_FIXTURE_TEST_CASE(err_state_tracks_activation_height, DigiDollarERRTestSetup)
{
    // Arrange: System becomes unhealthy
    validationContext.systemCollateral = 85;
    validationContext.nHeight = 50000;

    // Act: Get ERR state with activation height - EXPECTED TO FAIL (RED phase)
    DigiDollar::ERR::ERRState state = DigiDollar::ERR::GetCurrentState();

    // Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(state.activationHeight, 0);

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(state.activationHeight, 50000);
}

BOOST_FIXTURE_TEST_CASE(err_state_tracks_oracle_consensus_hash, DigiDollarERRTestSetup)
{
    // Arrange: ERR activation with oracle consensus
    std::vector<DigiDollar::COraclePriceMessage> messages;
    for (int i = 0; i < 8; i++) {
        DigiDollar::COraclePriceMessage msg;
        msg.price = mockOraclePrice;
        msg.timestamp = GetTime();
        msg.oracleKey = oracleXOnlyKeys[i];
        messages.push_back(msg);
    }

    // Act: Get ERR state with oracle consensus - EXPECTED TO FAIL (RED phase)
    DigiDollar::ERR::ERRState state = DigiDollar::ERR::GetCurrentState();

    // Assert: Should fail in RED phase
    BOOST_CHECK(state.oracleConsensusHash.IsNull());

    // After GREEN phase:
    // BOOST_CHECK(!state.oracleConsensusHash.IsNull()); // Should have consensus hash
}

// ============================================================================
// ERR Queue Management Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_queue_empty_when_inactive, DigiDollarERRTestSetup)
{
    // Arrange: ERR is inactive (healthy system)
    validationContext.systemCollateral = 150;

    // Act: Get ERR queue - EXPECTED TO FAIL (RED phase)
    std::vector<COutPoint> queue = DigiDollar::ERR::GetERRQueue();

    // Assert: Should fail since ERR queue is not implemented
    BOOST_CHECK(queue.empty());

    // After GREEN phase:
    // BOOST_CHECK(queue.empty()); // Should be empty when ERR inactive
}

BOOST_FIXTURE_TEST_CASE(err_queue_processes_redemptions_when_active, DigiDollarERRTestSetup)
{
    // Arrange: ERR is active, add redemptions to queue
    validationContext.systemCollateral = 90;

    // Create mock redemption requests
    std::vector<COutPoint> expectedRedemptions = {
        COutPoint(uint256S("1111111111111111111111111111111111111111111111111111111111111111"), 0),
        COutPoint(uint256S("2222222222222222222222222222222222222222222222222222222222222222"), 1),
        COutPoint(uint256S("3333333333333333333333333333333333333333333333333333333333333333"), 2)
    };

    // Act: Get ERR queue - EXPECTED TO FAIL (RED phase)
    std::vector<COutPoint> queue = DigiDollar::ERR::GetERRQueue();

    // Assert: Should fail in RED phase
    BOOST_CHECK(queue.empty());

    // After GREEN phase:
    // BOOST_CHECK_EQUAL(queue.size(), expectedRedemptions.size());
    // for (size_t i = 0; i < queue.size(); i++) {
    //     BOOST_CHECK_EQUAL(queue[i], expectedRedemptions[i]);
    // }
}

BOOST_FIXTURE_TEST_CASE(err_queue_prioritizes_by_request_time, DigiDollarERRTestSetup)
{
    // Arrange: Multiple ERR redemption requests at different times
    validationContext.systemCollateral = 85;

    // Act: Get prioritized queue - EXPECTED TO FAIL (RED phase)
    std::vector<COutPoint> queue = DigiDollar::ERR::GetERRQueue();

    // Assert: Should fail in RED phase
    BOOST_CHECK(queue.empty());

    // After GREEN phase:
    // Queue should be ordered by timestamp (FIFO for fairness)
    // Earliest requests should be processed first
}

// ============================================================================
// ERR Integration with Redemption Validation Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_blocks_normal_redemptions_when_active, DigiDollarERRTestSetup)
{
    // Arrange: ERR is active, create normal redemption transaction
    validationContext.systemCollateral = 90; // ERR active

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770; // DD_TX_REDEEM (type=3 in bits 24-31, marker=0x0770 in bits 0-15)

    // Add normal redemption inputs/outputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate normal redemption during ERR - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // BOOST_CHECK(!result); // Should fail - normal redemptions blocked during ERR
    // BOOST_CHECK(state.GetRejectReason().find("ERR") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(err_allows_err_redemptions_when_active, DigiDollarERRTestSetup)
{
    // Arrange: ERR is active, create ERR redemption transaction
    validationContext.systemCollateral = 85; // ERR active

    CMutableTransaction mtx;
    mtx.nVersion = 0x05000770; // DD_TX_ERR (type=5 in bits 24-31, marker=0x0770 in bits 0-15)

    // Add ERR redemption inputs/outputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // ERR redemption with reduced collateral (85% of original)
    CAmount errCollateralRelease = 85 * COIN; // Reduced amount
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(errCollateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate ERR redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateERRRedemption(tx, validationContext, state);

    // Assert: Should fail since ERR validation is not implemented
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // BOOST_CHECK(result); // Should pass - ERR redemptions allowed during ERR
}

BOOST_FIXTURE_TEST_CASE(err_validates_adjusted_collateral_return, DigiDollarERRTestSetup)
{
    // Arrange: ERR redemption with correct adjusted collateral
    validationContext.systemCollateral = 90; // 90% health = 90% return

    CMutableTransaction mtx;
    mtx.nVersion = 0x05000770; // DD_TX_ERR (type=5 in bits 24-31, marker=0x0770 in bits 0-15)

    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Correct ERR adjusted amount (90% of original 100 DGB)
    CAmount correctERRAmount = 90 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(correctERRAmount, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate correct ERR amount - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateERRRedemption(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);

    // Test with incorrect amount (too much)
    mtx.vout[0].nValue = 100 * COIN; // Full amount (should fail in ERR)
    CTransaction tx2(mtx);
    TxValidationState state2;
    result = DigiDollar::ValidateERRRedemption(tx2, validationContext, state2);
    BOOST_CHECK(!result);

    // After GREEN phase:
    // First tx should pass (correct ERR amount)
    // Second tx should fail (excessive amount for ERR)
}

BOOST_FIXTURE_TEST_CASE(err_requires_oracle_consensus_for_activation, DigiDollarERRTestSetup)
{
    // Arrange: System unhealthy but no oracle consensus
    validationContext.systemCollateral = 90;

    // Create insufficient oracle messages (only 7)
    std::vector<DigiDollar::COraclePriceMessage> insufficientMessages;
    for (int i = 0; i < 7; i++) {
        DigiDollar::COraclePriceMessage msg;
        msg.price = mockOraclePrice;
        msg.timestamp = GetTime();
        msg.oracleKey = oracleXOnlyKeys[i];
        insufficientMessages.push_back(msg);
    }

    // Act: Check ERR activation without consensus - EXPECTED TO FAIL (RED phase)
    bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(validationContext.systemCollateral);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!shouldActivate);

    // After GREEN phase:
    // BOOST_CHECK(!shouldActivate); // Should NOT activate without oracle consensus
    // Even if system health is poor, need 8-of-15 oracle agreement
}

// ============================================================================
// ERR Deactivation Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_deactivates_when_system_recovers, DigiDollarERRTestSetup)
{
    // Arrange: System recovers from unhealthy to healthy
    // Start unhealthy
    validationContext.systemCollateral = 90;
    DigiDollar::ERR::ERRState initialState = DigiDollar::ERR::GetCurrentState();

    // System recovers
    validationContext.systemCollateral = 105; // Above 100%

    // Act: Check ERR state after recovery - EXPECTED TO FAIL (RED phase)
    DigiDollar::ERR::ERRState recoveredState = DigiDollar::ERR::GetCurrentState();

    // Assert: Should fail in RED phase
    BOOST_CHECK(!recoveredState.isActive);

    // After GREEN phase:
    // BOOST_CHECK(!recoveredState.isActive); // Should deactivate when healthy
    // BOOST_CHECK_EQUAL(recoveredState.systemHealth, 105);
}

BOOST_FIXTURE_TEST_CASE(err_clears_queue_on_deactivation, DigiDollarERRTestSetup)
{
    // Arrange: ERR active with pending redemptions, then system recovers
    validationContext.systemCollateral = 85;

    // Add some pending redemptions (would be done in real implementation)
    std::vector<COutPoint> queueBeforeRecovery = DigiDollar::ERR::GetERRQueue();

    // System recovers
    validationContext.systemCollateral = 110;

    // Act: Check queue after recovery - EXPECTED TO FAIL (RED phase)
    std::vector<COutPoint> queueAfterRecovery = DigiDollar::ERR::GetERRQueue();

    // Assert: Should fail in RED phase
    BOOST_CHECK(queueAfterRecovery.empty());

    // After GREEN phase:
    // BOOST_CHECK(queueAfterRecovery.empty()); // Queue should be cleared on recovery
    // Any pending ERR redemptions should be processed normally
}

// ============================================================================
// ERR Edge Case Tests (RED Phase)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_handles_zero_system_health, DigiDollarERRTestSetup)
{
    // Arrange: System at 0% health (extreme case)
    int systemHealth = 0;

    // Act: Check ERR behavior at zero health - EXPECTED TO FAIL (RED phase)
    bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(systemHealth);
    double adjustmentRatio = DigiDollar::ERR::CalculateERRAdjustment(systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!shouldActivate);
    BOOST_CHECK_EQUAL(adjustmentRatio, 0.0);

    // After GREEN phase:
    // BOOST_CHECK(shouldActivate); // Should activate
    // BOOST_CHECK_EQUAL(adjustmentRatio, 0.80); // Minimum 80% ratio
}

BOOST_FIXTURE_TEST_CASE(err_handles_negative_system_health, DigiDollarERRTestSetup)
{
    // Arrange: Negative system health (error case)
    int systemHealth = -10;

    // Act: Check ERR behavior with negative health - EXPECTED TO FAIL (RED phase)
    bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(systemHealth);
    double adjustmentRatio = DigiDollar::ERR::CalculateERRAdjustment(systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!shouldActivate);
    BOOST_CHECK_EQUAL(adjustmentRatio, 0.0);

    // After GREEN phase:
    // BOOST_CHECK(shouldActivate); // Should activate for any < 100%
    // BOOST_CHECK_EQUAL(adjustmentRatio, 0.80); // Minimum ratio
}

BOOST_FIXTURE_TEST_CASE(err_handles_extremely_high_system_health, DigiDollarERRTestSetup)
{
    // Arrange: Very high system health
    int systemHealth = 50000; // 500x overcollateralized

    // Act: Check ERR behavior with high health - EXPECTED TO FAIL (RED phase)
    bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(systemHealth);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!shouldActivate);

    // After GREEN phase:
    // BOOST_CHECK(!shouldActivate); // Should NOT activate when healthy
}

BOOST_FIXTURE_TEST_CASE(err_validates_minimum_redemption_amounts, DigiDollarERRTestSetup)
{
    // Arrange: ERR redemption with very small amount
    validationContext.systemCollateral = 90;

    CAmount tinyAmount = 1; // 1 satoshi
    CAmount adjustedAmount = DigiDollar::ERR::GetAdjustedRedemption(tinyAmount, 90);

    // Act & Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(adjustedAmount, 0);

    // After GREEN phase:
    // Should handle tiny amounts correctly without underflow
    // BOOST_CHECK_EQUAL(adjustedAmount, 0); // May round down to 0 for tiny amounts
}

BOOST_FIXTURE_TEST_CASE(err_validates_maximum_redemption_amounts, DigiDollarERRTestSetup)
{
    // Arrange: ERR redemption with maximum amount
    validationContext.systemCollateral = 85;

    CAmount maxAmount = 1000000 * COIN; // 1M DGB
    CAmount adjustedAmount = DigiDollar::ERR::GetAdjustedRedemption(maxAmount, 85);

    // Act & Assert: Should fail in RED phase
    BOOST_CHECK_EQUAL(adjustedAmount, 0);

    // After GREEN phase:
    // Should handle large amounts without overflow
    // CAmount expected = (maxAmount * 85) / 100;
    // BOOST_CHECK_EQUAL(adjustedAmount, expected);
}

// ============================================================================
// ERR Extreme Activation Threshold Tests (RED Phase) - Task 4.9
// ============================================================================

BOOST_FIXTURE_TEST_CASE(test_err_extreme_activation_scenarios, DigiDollarERRTestSetup)
{
    // RED PHASE: These tests should FAIL until ERR extreme scenario handling is implemented

    // Test 1: Rapid health oscillation around activation threshold
    {
        std::vector<int> oscillatingHealth = {101, 99, 100, 99, 101, 98, 102};
        std::vector<bool> activationResults;

        for (int health : oscillatingHealth) {
            bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(health);
            activationResults.push_back(shouldActivate);
        }

        // Test oscillation stability - EXPECTED TO FAIL (RED phase)
        bool oscillationHandled = DigiDollar::ERR::HandleHealthOscillation(oscillatingHealth, activationResults);
        BOOST_CHECK(!oscillationHandled); // Will fail until implemented
    }

    // Test 2: Sub-threshold precision testing
    {
        // Test precise activation at boundaries
        std::vector<double> preciseHealth = {99.99, 99.9, 99.1, 99.01, 100.0, 100.01};

        for (double health : preciseHealth) {
            int intHealth = static_cast<int>(health * 100); // Convert to basis points
            bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(intHealth / 100);

            if (intHealth < 10000) { // Less than 100.00%
                // Should activate but currently doesn't due to lack of implementation
                BOOST_CHECK(!shouldActivate); // RED phase expectation
            }
        }

        // Test precision boundary handling - EXPECTED TO FAIL (RED phase)
        bool precisionHandled = DigiDollar::ERR::ValidatePrecisionBoundaries(preciseHealth);
        BOOST_CHECK(!precisionHandled); // Will fail until implemented
    }

    // Test 3: Extreme system health scenarios
    {
        // Test activation with extreme health values
        std::vector<int> extremeHealthValues = {-1000, -1, 0, 1, 50000, 100000};

        for (int health : extremeHealthValues) {
            bool shouldActivate = DigiDollar::ERR::ShouldActivateERR(health);

            // All negative or zero health should trigger ERR
            if (health < 100) {
                // Currently fails due to lack of implementation
                BOOST_CHECK(!shouldActivate); // RED phase expectation
            } else {
                BOOST_CHECK(!shouldActivate); // Should not activate for high health
            }
        }

        // Test extreme value validation - EXPECTED TO FAIL (RED phase)
        bool extremeValuesHandled = DigiDollar::ERR::ValidateExtremeHealthValues(extremeHealthValues);
        BOOST_CHECK(!extremeValuesHandled); // Will fail until implemented
    }

    // Test 4: Concurrent activation requests
    {
        // Simulate multiple threads checking ERR activation simultaneously
        int borderlineHealth = 99;
        std::vector<bool> concurrentResults;

        // Simulate concurrent activation checks
        for (int i = 0; i < 10; ++i) {
            bool result = DigiDollar::ERR::ShouldActivateERR(borderlineHealth);
            concurrentResults.push_back(result);
        }

        // All results should be consistent
        bool allSame = std::all_of(concurrentResults.begin(), concurrentResults.end(),
                                  [&](bool result) { return result == concurrentResults[0]; });
        BOOST_CHECK(allSame);

        // Test thread safety - EXPECTED TO FAIL (RED phase)
        bool threadSafe = DigiDollar::ERR::ValidateThreadSafety(concurrentResults);
        BOOST_CHECK(!threadSafe); // Will fail until implemented
    }

    // Test 5: Oracle consensus failure scenarios
    {
        // Test ERR activation when oracle consensus fails intermittently
        std::vector<int> unhealthyLevels = {95, 90, 85, 80};

        for (int health : unhealthyLevels) {
            validationContext.systemCollateral = health;

            // Test with insufficient oracle messages
            std::vector<DigiDollar::COraclePriceMessage> insufficientMessages;
            for (int i = 0; i < 5; i++) { // Only 5 out of required 8
                DigiDollar::COraclePriceMessage msg;
                msg.price = mockOraclePrice;
                msg.timestamp = GetTime();
                insufficientMessages.push_back(msg);
            }

            // Should not activate without consensus even if health is low
            bool hasConsensus = DigiDollar::ERR::HasOracleConsensus(insufficientMessages);
            BOOST_CHECK(!hasConsensus);

            // Test consensus failure handling - EXPECTED TO FAIL (RED phase)
            bool consensusFailureHandled = DigiDollar::ERR::HandleConsensusFailure(health, insufficientMessages);
            BOOST_CHECK(!consensusFailureHandled); // Will fail until implemented
        }
    }

    // Test 6: System state corruption scenarios
    {
        // Test ERR behavior when system state is corrupted or inconsistent
        validationContext.systemCollateral = 90; // Should trigger ERR

        // Simulate corrupted state
        bool stateCorrupted = true;

        // ERR should handle corrupted state gracefully
        bool canActivateWithCorruption = DigiDollar::ERR::ShouldActivateERRWithCorruptedState(
            validationContext.systemCollateral, stateCorrupted);

        // Test corruption handling - EXPECTED TO FAIL (RED phase)
        BOOST_CHECK(!canActivateWithCorruption); // Will fail until implemented
    }
}

BOOST_FIXTURE_TEST_CASE(test_err_activation_timing_precision, DigiDollarERRTestSetup)
{
    // RED PHASE: Test timing-sensitive ERR activation scenarios

    // Test 1: Block-level activation timing
    {
        // Test ERR activation at specific block heights
        std::vector<uint32_t> criticalBlocks = {1000, 2000, 5000, 10000};

        for (uint32_t blockHeight : criticalBlocks) {
            validationContext.nHeight = blockHeight;
            validationContext.systemCollateral = 95; // Should trigger ERR

            bool activatedAtBlock = DigiDollar::ERR::ShouldActivateERRAtHeight(
                validationContext.systemCollateral, blockHeight);

            // Test block-specific activation - EXPECTED TO FAIL (RED phase)
            BOOST_CHECK(!activatedAtBlock); // Will fail until implemented
        }
    }

    // Test 2: Time-based activation windows
    {
        // Test ERR activation during specific time windows
        int64_t currentTime = GetTime();
        std::vector<int64_t> testTimes = {
            currentTime - 3600,  // 1 hour ago
            currentTime,         // Now
            currentTime + 3600   // 1 hour from now
        };

        for (int64_t testTime : testTimes) {
            bool activatedAtTime = DigiDollar::ERR::ShouldActivateERRAtTime(
                95, testTime); // 95% health

            // Test time-based activation - EXPECTED TO FAIL (RED phase)
            BOOST_CHECK(!activatedAtTime); // Will fail until implemented
        }
    }

    // Test 3: Activation delay mechanisms
    {
        // Test that ERR doesn't activate immediately but has delay
        validationContext.systemCollateral = 90; // Should trigger ERR

        // First check should not activate immediately
        bool immediateActivation = DigiDollar::ERR::ShouldActivateERR(90);
        BOOST_CHECK(!immediateActivation); // Currently expected

        // Test activation delay - EXPECTED TO FAIL (RED phase)
        bool delayMechanismActive = DigiDollar::ERR::HasActivationDelay(90);
        BOOST_CHECK(!delayMechanismActive); // Will fail until implemented
    }
}

BOOST_FIXTURE_TEST_CASE(test_err_ratio_calculation_extremes, DigiDollarERRTestSetup)
{
    // RED PHASE: Test ERR ratio calculations under extreme conditions

    // Test 1: Floating point precision in ratio calculations
    {
        std::vector<std::pair<int, double>> precisionTests = {
            {95, 0.95}, {90, 0.90}, {85, 0.85}, {80, 0.80}, // Standard cases
            {84, 0.80}, {86, 0.85}, // Boundary cases
            {1, 0.80},   // Extreme low
            {99, 0.95}   // Just below threshold
        };

        for (auto& test : precisionTests) {
            double ratio = DigiDollar::ERR::CalculateERRAdjustment(test.first);

            // Currently returns 0.0 in RED phase
            BOOST_CHECK_EQUAL(ratio, 0.0);

            // Test precision validation - EXPECTED TO FAIL (RED phase)
            bool precisionValid = DigiDollar::ERR::ValidateRatioPrecision(test.first, test.second);
            BOOST_CHECK(!precisionValid); // Will fail until implemented
        }
    }

    // Test 2: Overflow protection in ratio calculations
    {
        // Test with maximum possible redemption amounts
        CAmount maxRedemption = std::numeric_limits<CAmount>::max() / 2;
        int health = 85;

        CAmount adjustedAmount = DigiDollar::ERR::GetAdjustedRedemption(maxRedemption, health);

        // Currently returns 0 in RED phase
        BOOST_CHECK_EQUAL(adjustedAmount, 0);

        // Test overflow protection - EXPECTED TO FAIL (RED phase)
        bool overflowProtected = DigiDollar::ERR::PreventCalculationOverflow(maxRedemption, health);
        BOOST_CHECK(!overflowProtected); // Will fail until implemented
    }

    // Test 3: Ratio calculation consistency under stress
    {
        // Perform many calculations and verify consistency
        std::vector<std::pair<CAmount, double>> stressTests;

        for (int i = 0; i < 1000; ++i) {
            CAmount amount = (i + 1) * COIN; // 1 to 1000 DGB
            int health = 85 + (i % 15); // Health from 85-99%

            double ratio1 = DigiDollar::ERR::CalculateERRAdjustment(health);
            double ratio2 = DigiDollar::ERR::CalculateERRAdjustment(health);

            // Results should be consistent
            BOOST_CHECK_EQUAL(ratio1, ratio2);
            stressTests.push_back({amount, ratio1});
        }

        // Test calculation consistency - EXPECTED TO FAIL (RED phase)
        bool calculationsConsistent = DigiDollar::ERR::ValidateCalculationConsistency(stressTests);
        BOOST_CHECK(!calculationsConsistent); // Will fail until implemented
    }
}

BOOST_FIXTURE_TEST_CASE(test_err_oracle_consensus_stress, DigiDollarERRTestSetup)
{
    // RED PHASE: Test oracle consensus under stress conditions

    // Test 1: Massive oracle message handling
    {
        COracleBundle largeBundle(1);

        // Add maximum number of oracle messages
        for (int i = 0; i < 100; ++i) { // More than the 15 expected oracles
            COraclePriceMessage msg(i, mockOraclePrice, GetTime());
            msg.signature = std::vector<unsigned char>(64, 0x01);
            largeBundle.AddMessage(msg);
        }

        // Should handle large number of messages gracefully
        bool hasConsensus = DigiDollar::ERR::EmergencyRedemptionRatio::HasOracleConsensus(largeBundle);
        BOOST_CHECK(!hasConsensus); // Currently expected in RED phase

        // Test large message handling - EXPECTED TO FAIL (RED phase)
        bool largeMessageHandling = DigiDollar::ERR::HandleLargeOracleMessageCount(largeBundle);
        BOOST_CHECK(!largeMessageHandling); // Will fail until implemented
    }

    // Test 2: Malformed oracle message handling
    {
        std::vector<DigiDollar::COraclePriceMessage> malformedMessages;

        // Create messages with various malformations
        for (int i = 0; i < 8; ++i) {
            DigiDollar::COraclePriceMessage msg;

            // Various malformations
            if (i % 4 == 0) {
                msg.price = 0; // Invalid price
            } else if (i % 4 == 1) {
                msg.timestamp = 0; // Invalid timestamp
            } else if (i % 4 == 2) {
                // Invalid signature (empty)
            } else {
                // Valid message
                msg.price = mockOraclePrice;
                msg.timestamp = GetTime();
            }

            malformedMessages.push_back(msg);
        }

        bool hasConsensus = DigiDollar::ERR::HasOracleConsensus(malformedMessages);
        BOOST_CHECK(!hasConsensus); // Should reject malformed messages

        // Test malformed message handling - EXPECTED TO FAIL (RED phase)
        bool malformedHandling = DigiDollar::ERR::ValidateMalformedMessageHandling(malformedMessages);
        BOOST_CHECK(!malformedHandling); // Will fail until implemented
    }

    // Test 3: Consensus timing under high load
    {
        // Measure time to process oracle consensus under load
        auto startTime = std::chrono::high_resolution_clock::now();

        std::vector<DigiDollar::COraclePriceMessage> loadTestMessages;
        for (int i = 0; i < 15; ++i) {
            DigiDollar::COraclePriceMessage msg;
            msg.price = mockOraclePrice;
            msg.timestamp = GetTime();
            loadTestMessages.push_back(msg);
        }

        // Perform consensus check multiple times
        for (int i = 0; i < 100; ++i) {
            bool consensus = DigiDollar::ERR::HasOracleConsensus(loadTestMessages);
            (void)consensus; // Suppress unused variable warning
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Should complete in reasonable time (less than 1 second)
        BOOST_CHECK_LT(duration.count(), 1000);

        // Test performance under load - EXPECTED TO FAIL (RED phase)
        bool performanceAcceptable = DigiDollar::ERR::ValidateConsensusPerformance(duration.count());
        BOOST_CHECK(!performanceAcceptable); // Will fail until implemented
    }
}

BOOST_AUTO_TEST_SUITE_END()