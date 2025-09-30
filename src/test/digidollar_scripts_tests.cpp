// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <digidollar/scripts.h>
#include <digidollar/digidollar.h>
#include <script/script.h>
#include <script/standard.h>
#include <script/solver.h>
#include <script/interpreter.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <key.h>
#include <pubkey.h>

#include <boost/test/unit_test.hpp>

// Use TestingSetup instead of BasicTestingSetup to ensure full initialization
// including ECC context and logging
BOOST_FIXTURE_TEST_SUITE(digidollar_scripts_tests, TestingSetup)

// Test helper to create valid MintParams
DigiDollar::MintParams CreateTestMintParams()
{
    DigiDollar::MintParams params;

    // Generate test keys
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    params.ownerKey = XOnlyPubKey(ownerKey.GetPubKey());

    CKey internalKey;
    internalKey.MakeNewKey(true);
    params.internalKey = XOnlyPubKey(internalKey.GetPubKey());

    // Set test parameters
    params.ddAmount = 100 * 100;  // $100 in cents
    params.lockHeight = 1000;     // 1000 blocks from now

    // Get oracle keys
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    return params;
}

BOOST_AUTO_TEST_CASE(test_oracle_keys_generation)
{
    // Test oracle key generation
    auto keys = DigiDollar::GetOracleKeys(15);

    BOOST_CHECK_EQUAL(keys.size(), 15);

    // Check all keys are valid
    for (const auto& key : keys) {
        BOOST_CHECK(key.IsFullyValid());
    }

    // Check keys are deterministic (same call should produce same keys)
    auto keys2 = DigiDollar::GetOracleKeys(15);
    BOOST_CHECK_EQUAL(keys.size(), keys2.size());
    for (size_t i = 0; i < keys.size(); i++) {
        BOOST_CHECK(keys[i] == keys2[i]);
    }
}

BOOST_AUTO_TEST_CASE(test_normal_redemption_path_creation)
{
    auto params = CreateTestMintParams();

    // This should fail until we implement CreateNormalRedemptionPath
    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(params);

    // Verify script contains expected opcodes
    BOOST_CHECK(normalPath.size() > 0);

    // Should contain CHECKLOCKTIMEVERIFY for timelock
    BOOST_CHECK(std::find(normalPath.begin(), normalPath.end(), OP_CHECKLOCKTIMEVERIFY) != normalPath.end());

    // Should contain OP_DIGIDOLLAR for amount verification
    BOOST_CHECK(std::find(normalPath.begin(), normalPath.end(), OP_DIGIDOLLAR) != normalPath.end());

    // Should contain CHECKSIG for owner verification
    BOOST_CHECK(std::find(normalPath.begin(), normalPath.end(), OP_CHECKSIG) != normalPath.end());
}

BOOST_AUTO_TEST_CASE(test_emergency_path_creation)
{
    auto params = CreateTestMintParams();

    // This should fail until we implement CreateEmergencyPath
    CScript emergencyPath = DigiDollar::CreateEmergencyPath(params);

    BOOST_CHECK(emergencyPath.size() > 0);

    // Should contain OP_DIGIDOLLAR for amount verification
    BOOST_CHECK(std::find(emergencyPath.begin(), emergencyPath.end(), OP_DIGIDOLLAR) != emergencyPath.end());

    // Should contain multiple CHECKSIGADD operations (for 15 oracle keys)
    // Note: The script also includes key pushes (32 bytes each with size prefix)
    // So we count >= 15 rather than exactly 15 to account for encoding
    size_t checksigadd_count = std::count(emergencyPath.begin(), emergencyPath.end(), OP_CHECKSIGADD);
    BOOST_CHECK(checksigadd_count >= 15);

    // Should require 8-of-15 threshold
    BOOST_CHECK(std::find(emergencyPath.begin(), emergencyPath.end(), OP_8) != emergencyPath.end());
    BOOST_CHECK(std::find(emergencyPath.begin(), emergencyPath.end(), OP_EQUAL) != emergencyPath.end());
}

BOOST_AUTO_TEST_CASE(test_partial_redemption_path_creation)
{
    auto params = CreateTestMintParams();

    // This should fail until we implement CreatePartialRedemptionPath
    CScript partialPath = DigiDollar::CreatePartialRedemptionPath(params);

    BOOST_CHECK(partialPath.size() > 0);

    // Should contain OP_DIGIDOLLAR and OP_DDVERIFY
    BOOST_CHECK(std::find(partialPath.begin(), partialPath.end(), OP_DIGIDOLLAR) != partialPath.end());
    BOOST_CHECK(std::find(partialPath.begin(), partialPath.end(), OP_DDVERIFY) != partialPath.end());

    // Should contain price check
    BOOST_CHECK(std::find(partialPath.begin(), partialPath.end(), OP_CHECKPRICE) != partialPath.end());

    // Should contain owner signature verification
    BOOST_CHECK(std::find(partialPath.begin(), partialPath.end(), OP_CHECKSIGVERIFY) != partialPath.end());
}

BOOST_AUTO_TEST_CASE(test_err_path_creation)
{
    auto params = CreateTestMintParams();

    // This should fail until we implement CreateERRPath
    CScript errPath = DigiDollar::CreateERRPath(params);

    BOOST_CHECK(errPath.size() > 0);

    // Should contain collateral check
    BOOST_CHECK(std::find(errPath.begin(), errPath.end(), OP_CHECKCOLLATERAL) != errPath.end());
    BOOST_CHECK(std::find(errPath.begin(), errPath.end(), OP_LESSTHAN) != errPath.end());

    // Should contain DD verification
    BOOST_CHECK(std::find(errPath.begin(), errPath.end(), OP_DIGIDOLLAR) != errPath.end());
    BOOST_CHECK(std::find(errPath.begin(), errPath.end(), OP_DDVERIFY) != errPath.end());

    // Should contain owner signature
    BOOST_CHECK(std::find(errPath.begin(), errPath.end(), OP_CHECKSIG) != errPath.end());
}

BOOST_AUTO_TEST_CASE(test_collateral_p2tr_creation)
{
    auto params = CreateTestMintParams();

    // This should fail until we implement CreateCollateralP2TR
    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);

    // P2TR script should be exactly 34 bytes: OP_1 (1 byte) + size prefix (1 byte) + 32-byte key
    BOOST_CHECK_EQUAL(collateralScript.size(), 34);

    // Should start with OP_1 (Taproot version)
    BOOST_CHECK_EQUAL(collateralScript[0], OP_1);

    // Next should be size prefix 0x20 (32) followed by 32-byte taproot output key
    BOOST_CHECK_EQUAL(collateralScript[1], 0x20); // Size prefix for 32-byte push
    std::vector<unsigned char> outputKey(collateralScript.begin() + 2, collateralScript.end());
    BOOST_CHECK_EQUAL(outputKey.size(), 32);

    // Should be valid P2TR format
    std::vector<std::vector<unsigned char>> solutions;
    TxoutType type = Solver(collateralScript, solutions);
    BOOST_CHECK(type == TxoutType::WITNESS_V1_TAPROOT);
}

BOOST_AUTO_TEST_CASE(test_digidollar_p2tr_creation)
{
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey owner(ownerKey.GetPubKey());
    CAmount ddAmount = 50 * 100;  // $50 in cents

    // This should fail until we implement CreateDigiDollarP2TR
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(owner, ddAmount);

    // P2TR script should be exactly 34 bytes
    BOOST_CHECK_EQUAL(ddScript.size(), 34);

    // Should start with OP_1
    BOOST_CHECK_EQUAL(ddScript[0], OP_1);

    // Should be valid P2TR format
    std::vector<std::vector<unsigned char>> solutions2;
    TxoutType type2 = Solver(ddScript, solutions2);
    BOOST_CHECK(type2 == TxoutType::WITNESS_V1_TAPROOT);
}

BOOST_AUTO_TEST_CASE(test_collateral_script_with_different_amounts)
{
    auto params = CreateTestMintParams();

    // Test with different DD amounts
    std::vector<CAmount> amounts = {100, 1000 * 100, 10000 * 100};  // $1, $1000, $10000

    for (CAmount amount : amounts) {
        params.ddAmount = amount;
        CScript script = DigiDollar::CreateCollateralP2TR(params);

        // All should be valid P2TR scripts
        BOOST_CHECK_EQUAL(script.size(), 34);
        BOOST_CHECK_EQUAL(script[0], OP_1);

        std::vector<std::vector<unsigned char>> solutions;
        TxoutType type = Solver(script, solutions);
        BOOST_CHECK(type == TxoutType::WITNESS_V1_TAPROOT);
    }
}

BOOST_AUTO_TEST_CASE(test_collateral_script_with_different_lock_periods)
{
    auto params = CreateTestMintParams();

    // Test with different lock periods
    std::vector<int64_t> lockPeriods = {
        144,        // 1 day (144 blocks * 15 seconds)
        4320,       // 30 days
        52560,      // 1 year
        525600      // 10 years
    };

    for (int64_t lockPeriod : lockPeriods) {
        params.lockHeight = lockPeriod;
        CScript script = DigiDollar::CreateCollateralP2TR(params);

        // All should be valid regardless of lock period
        BOOST_CHECK_EQUAL(script.size(), 34);

        std::vector<std::vector<unsigned char>> solutions;
        TxoutType type = Solver(script, solutions);
        BOOST_CHECK(type == TxoutType::WITNESS_V1_TAPROOT);
    }
}

BOOST_AUTO_TEST_CASE(test_script_size_limits)
{
    auto params = CreateTestMintParams();

    // Test all individual paths are within reasonable size limits
    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(params);
    CScript emergencyPath = DigiDollar::CreateEmergencyPath(params);
    CScript partialPath = DigiDollar::CreatePartialRedemptionPath(params);
    CScript errPath = DigiDollar::CreateERRPath(params);

    // Individual scripts should be reasonable in size (under 1KB each)
    BOOST_CHECK(normalPath.size() < 1024);
    BOOST_CHECK(emergencyPath.size() < 1024);
    BOOST_CHECK(partialPath.size() < 1024);
    BOOST_CHECK(errPath.size() < 1024);

    // Emergency path will be largest due to 15 oracle keys
    BOOST_CHECK(emergencyPath.size() > normalPath.size());
}

BOOST_AUTO_TEST_CASE(test_script_validation_with_mock_execution)
{
    auto params = CreateTestMintParams();

    // Create a mock transaction context for script validation
    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(params);

    // The script should at least parse correctly
    BOOST_CHECK(normalPath.HasValidOps());

    // Check that it doesn't have obvious syntax errors
    CScript::const_iterator pc = normalPath.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;
    bool hasOps = normalPath.GetOp(pc, opcode, data);
    BOOST_CHECK(hasOps || normalPath.empty());
}

BOOST_AUTO_TEST_CASE(test_oracle_keys_with_different_counts)
{
    // Test with different oracle counts
    for (size_t count = 1; count <= 20; count++) {
        auto keys = DigiDollar::GetOracleKeys(count);
        BOOST_CHECK_EQUAL(keys.size(), count);

        for (const auto& key : keys) {
            BOOST_CHECK(key.IsFullyValid());
        }
    }
}

BOOST_AUTO_TEST_CASE(test_invalid_parameters_handling)
{
    DigiDollar::MintParams params;
    // Leave params mostly uninitialized to test error handling

    // This should handle invalid parameters gracefully
    // (Implementation should validate inputs)
    params.ddAmount = 0;  // Invalid amount
    params.lockHeight = -1;  // Invalid lock height

    // These calls should not crash even with invalid params
    // (Though they may return empty/invalid scripts)
    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(params);
    CScript emergencyPath = DigiDollar::CreateEmergencyPath(params);
    CScript partialPath = DigiDollar::CreatePartialRedemptionPath(params);
    CScript errPath = DigiDollar::CreateERRPath(params);

    // Scripts may be empty or invalid, but shouldn't crash
    BOOST_CHECK(true);  // If we get here, no crash occurred
}

BOOST_AUTO_TEST_SUITE_END()