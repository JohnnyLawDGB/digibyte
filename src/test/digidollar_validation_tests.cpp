// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/digidollar.h>
#include <consensus/volatility.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <digidollar/digidollar.h>
#include <key.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/interpreter.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <util/time.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(digidollar_validation_tests)

struct DigiDollarValidationTestSetup : public TestingSetup {
    DigiDollarValidationTestSetup() : TestingSetup(ChainType::REGTEST),
        validationContext(1000, 50000, 150, Params()) {
        // Set up mock oracle price and system state
        mockOraclePrice = 50000; // $500.00 DGB
        mockSystemCollateral = 150; // 150% system-wide collateral
        mockHeight = 1000;

        // Generate test keys
        testKey.MakeNewKey(true);
        testPubKey = testKey.GetPubKey();
        testXOnlyKey = XOnlyPubKey(testPubKey);

        // Validation context is initialized in member initializer list
    }

    CKey testKey;
    CPubKey testPubKey;
    XOnlyPubKey testXOnlyKey;
    CAmount mockOraclePrice;
    int mockSystemCollateral;
    int mockHeight;
    DigiDollar::ValidationContext validationContext;
};

// ============================================================================
// Script Type Detection Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(script_type_detection_dd_token, DigiDollarValidationTestSetup)
{
    // Test identification of DD token scripts
    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);

    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(ddScript), DigiDollar::ScriptType::DD_TOKEN_OUTPUT);
    BOOST_CHECK(DigiDollar::IsDDTokenScript(ddScript));
    BOOST_CHECK(!DigiDollar::IsCollateralScript(ddScript));

    // Test amount extraction
    CAmount extractedAmount;
    BOOST_CHECK(DigiDollar::ExtractDDAmount(ddScript, extractedAmount));
    BOOST_CHECK_EQUAL(extractedAmount, ddAmount);
}

BOOST_FIXTURE_TEST_CASE(script_type_detection_collateral, DigiDollarValidationTestSetup)
{
    // Test identification of collateral scripts
    DigiDollar::MintParams params;
    params.ddAmount = 10000; // $100.00
    params.lockHeight = mockHeight + 30 * 24 * 60 * 4; // 30 days
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);

    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(collateralScript), DigiDollar::ScriptType::COLLATERAL_LOCK);
    BOOST_CHECK(DigiDollar::IsCollateralScript(collateralScript));
    BOOST_CHECK(!DigiDollar::IsDDTokenScript(collateralScript));
}

BOOST_FIXTURE_TEST_CASE(script_type_detection_non_dd, DigiDollarValidationTestSetup)
{
    // Test rejection of non-DD scripts
    CScript p2pkhScript = GetScriptForDestination(PKHash(testPubKey));
    CScript p2shScript = GetScriptForDestination(ScriptHash(CScript()));
    CScript opReturnScript = CScript() << OP_RETURN << ParseHex("deadbeef");

    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(p2pkhScript), DigiDollar::ScriptType::NOT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(p2shScript), DigiDollar::ScriptType::NOT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(opReturnScript), DigiDollar::ScriptType::NOT_DIGIDOLLAR);

    BOOST_CHECK(!DigiDollar::IsDDTokenScript(p2pkhScript));
    BOOST_CHECK(!DigiDollar::IsCollateralScript(p2pkhScript));
}

BOOST_FIXTURE_TEST_CASE(script_type_detection_malformed, DigiDollarValidationTestSetup)
{
    // Test malformed scripts
    CScript emptyScript;
    CScript invalidP2TR = CScript() << OP_1 << ParseHex("deadbeef"); // Wrong witness program size

    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(emptyScript), DigiDollar::ScriptType::NOT_DIGIDOLLAR);
    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(invalidP2TR), DigiDollar::ScriptType::NOT_DIGIDOLLAR);

    // Test amount extraction failure
    CAmount amount;
    BOOST_CHECK(!DigiDollar::ExtractDDAmount(emptyScript, amount));
    BOOST_CHECK(!DigiDollar::ExtractDDAmount(invalidP2TR, amount));
}

// ============================================================================
// Amount Validation Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(amount_validation_mint_amounts, DigiDollarValidationTestSetup)
{
    const auto& params = Params();

    // Test minimum mint amount ($100)
    BOOST_CHECK(DigiDollar::ValidateMintAmount(10000, params)); // $100.00
    BOOST_CHECK(!DigiDollar::ValidateMintAmount(9999, params)); // $99.99 - too small
    BOOST_CHECK(!DigiDollar::ValidateMintAmount(0, params)); // Zero
    BOOST_CHECK(!DigiDollar::ValidateMintAmount(-1, params)); // Negative

    // Test maximum mint amount ($100k)
    BOOST_CHECK(DigiDollar::ValidateMintAmount(10000000, params)); // $100k
    BOOST_CHECK(!DigiDollar::ValidateMintAmount(10000001, params)); // $100k + 1 cent - too large

    // Test edge cases
    BOOST_CHECK(DigiDollar::ValidateMintAmount(5000000, params)); // $50k - middle range
}

BOOST_FIXTURE_TEST_CASE(amount_validation_output_amounts, DigiDollarValidationTestSetup)
{
    const auto& params = Params();

    // Test minimum output amount ($1)
    BOOST_CHECK(DigiDollar::ValidateOutputAmount(100, params)); // $1.00
    BOOST_CHECK(!DigiDollar::ValidateOutputAmount(99, params)); // $0.99 - too small
    BOOST_CHECK(!DigiDollar::ValidateOutputAmount(0, params)); // Zero
    BOOST_CHECK(!DigiDollar::ValidateOutputAmount(-1, params)); // Negative

    // Test large amounts (up to MAX_MONEY)
    BOOST_CHECK(DigiDollar::ValidateOutputAmount(MAX_DIGIDOLLAR, params));
    BOOST_CHECK(!DigiDollar::ValidateOutputAmount(MAX_DIGIDOLLAR + 1, params));
}

BOOST_FIXTURE_TEST_CASE(amount_validation_overflow_protection, DigiDollarValidationTestSetup)
{
    const auto& params = Params();

    // Test overflow protection
    CAmount maxInt64 = std::numeric_limits<int64_t>::max();
    BOOST_CHECK(!DigiDollar::ValidateMintAmount(maxInt64, params));
    BOOST_CHECK(!DigiDollar::ValidateOutputAmount(maxInt64, params));

    // Test near-overflow values
    CAmount nearMax = MAX_DIGIDOLLAR - 1;
    BOOST_CHECK(DigiDollar::ValidateOutputAmount(nearMax, params));
}

// ============================================================================
// Collateral Ratio Validation Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(collateral_ratio_validation_basic, DigiDollarValidationTestSetup)
{
    // Test 30-day lock requiring 500% collateral
    int64_t lockTime = 30 * 24 * 60 * 4; // 30 days in blocks
    CAmount ddMinted = 10000; // $100.00
    CAmount dgbRequired = (ddMinted * 500 * COIN) / (mockOraclePrice / 100); // 500% ratio

    BOOST_CHECK(DigiDollar::ValidateCollateralRatio(dgbRequired, ddMinted, lockTime, validationContext));
    BOOST_CHECK(!DigiDollar::ValidateCollateralRatio(dgbRequired - 1, ddMinted, lockTime, validationContext));
}

BOOST_FIXTURE_TEST_CASE(collateral_ratio_validation_dca_adjustment, DigiDollarValidationTestSetup)
{
    // Test DCA (Dynamic Collateral Adjustment) when system health is poor
    validationContext.systemCollateral = 110; // Between 110-120% - requires +50% collateral

    int64_t lockTime = 30 * 24 * 60 * 4; // 30 days - normally 500%
    CAmount ddMinted = 10000; // $100.00

    // With DCA multiplier of 1.5, effective ratio is 500% * 1.5 = 750%
    CAmount dgbRequired = (ddMinted * 750 * COIN) / (mockOraclePrice / 100);

    BOOST_CHECK(DigiDollar::ValidateCollateralRatio(dgbRequired, ddMinted, lockTime, validationContext));

    // Original 500% should now fail due to DCA
    CAmount dgbOriginal = (ddMinted * 500 * COIN) / (mockOraclePrice / 100);
    BOOST_CHECK(!DigiDollar::ValidateCollateralRatio(dgbOriginal, ddMinted, lockTime, validationContext));
}

BOOST_FIXTURE_TEST_CASE(collateral_ratio_validation_edge_cases, DigiDollarValidationTestSetup)
{
    // Test zero amounts
    BOOST_CHECK(!DigiDollar::ValidateCollateralRatio(0, 0, 30 * 24 * 60 * 4, validationContext));
    BOOST_CHECK(!DigiDollar::ValidateCollateralRatio(1000 * COIN, 0, 30 * 24 * 60 * 4, validationContext));

    // Test zero oracle price
    validationContext.oraclePrice = 0;
    BOOST_CHECK(!DigiDollar::ValidateCollateralRatio(1000 * COIN, 10000, 30 * 24 * 60 * 4, validationContext));
}

// ============================================================================
// Path-Specific Validation Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(path_validation_normal_redemption, DigiDollarValidationTestSetup)
{
    DigiDollar::MintParams params;
    params.lockHeight = mockHeight + 100; // Lock expires in 100 blocks
    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(params);

    // Should pass when timelock has expired
    BOOST_CHECK(DigiDollar::ValidateNormalRedemption(normalPath, mockHeight + 101));

    // Should fail when timelock hasn't expired
    BOOST_CHECK(!DigiDollar::ValidateNormalRedemption(normalPath, mockHeight + 99));
    BOOST_CHECK(!DigiDollar::ValidateNormalRedemption(normalPath, mockHeight + 100)); // Exact match should fail (not strictly greater)
}

BOOST_FIXTURE_TEST_CASE(path_validation_emergency_redemption, DigiDollarValidationTestSetup)
{
    DigiDollar::MintParams params;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript emergencyPath = DigiDollar::CreateEmergencyPath(params);

    // Test with sufficient signatures (8 of 15)
    std::vector<std::vector<unsigned char>> sufficientSigs(8);
    BOOST_CHECK(DigiDollar::ValidateEmergencyRedemption(emergencyPath, sufficientSigs));

    // Test with insufficient signatures (7 of 15)
    std::vector<std::vector<unsigned char>> insufficientSigs(7);
    BOOST_CHECK(!DigiDollar::ValidateEmergencyRedemption(emergencyPath, insufficientSigs));

    // Test with no signatures
    std::vector<std::vector<unsigned char>> noSigs;
    BOOST_CHECK(!DigiDollar::ValidateEmergencyRedemption(emergencyPath, noSigs));
}

BOOST_FIXTURE_TEST_CASE(path_validation_partial_redemption, DigiDollarValidationTestSetup)
{
    DigiDollar::MintParams params;
    CScript partialPath = DigiDollar::CreatePartialRedemptionPath(params);

    // Should pass with valid oracle price
    BOOST_CHECK(DigiDollar::ValidatePartialRedemption(partialPath, mockOraclePrice));

    // Should fail with stale/invalid price
    BOOST_CHECK(!DigiDollar::ValidatePartialRedemption(partialPath, 0));
    BOOST_CHECK(!DigiDollar::ValidatePartialRedemption(partialPath, -1));
}

BOOST_FIXTURE_TEST_CASE(path_validation_err_redemption, DigiDollarValidationTestSetup)
{
    DigiDollar::MintParams params;
    CScript errPath = DigiDollar::CreateERRPath(params);

    // ERR should activate when system < 100% collateralized
    BOOST_CHECK(DigiDollar::ValidateERRRedemption(errPath, 99));
    BOOST_CHECK(DigiDollar::ValidateERRRedemption(errPath, 50));

    // ERR should not activate when system >= 100% collateralized
    BOOST_CHECK(!DigiDollar::ValidateERRRedemption(errPath, 100));
    BOOST_CHECK(!DigiDollar::ValidateERRRedemption(errPath, 150));
}

// ============================================================================
// Script Validation Integration Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(script_validation_dd_token_script, DigiDollarValidationTestSetup)
{
    // Test valid DD token script validation
    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);

    ScriptError serror = SCRIPT_ERR_OK;
    BOOST_CHECK(DigiDollar::ValidateDigiDollarScript(ddScript, validationContext, &serror));
    BOOST_CHECK_EQUAL(serror, SCRIPT_ERR_OK);
}

BOOST_FIXTURE_TEST_CASE(script_validation_invalid_amount, DigiDollarValidationTestSetup)
{
    // Test script with invalid DD amount
    CScript invalidScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, 50); // $0.50 - too small

    ScriptError serror = SCRIPT_ERR_OK;
    BOOST_CHECK(!DigiDollar::ValidateDigiDollarScript(invalidScript, validationContext, &serror));
    BOOST_CHECK_EQUAL(serror, SCRIPT_ERR_INVALID_DD_AMOUNT);
}

BOOST_FIXTURE_TEST_CASE(script_validation_non_dd_script, DigiDollarValidationTestSetup)
{
    // Test that non-DD scripts pass validation (not our concern)
    CScript p2pkhScript = GetScriptForDestination(PKHash(testPubKey));

    ScriptError serror = SCRIPT_ERR_OK;
    BOOST_CHECK(DigiDollar::ValidateDigiDollarScript(p2pkhScript, validationContext, &serror));
    BOOST_CHECK_EQUAL(serror, SCRIPT_ERR_OK);
}

// ============================================================================
// Transaction Validation Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(transaction_validation_mint_tx, DigiDollarValidationTestSetup)
{
    // Create a mock mint transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    // Add collateral input (simplified for test)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("0x1234"), 0);

    // Add collateral output
    DigiDollar::MintParams params;
    params.ddAmount = 10000; // $100.00
    params.lockHeight = mockHeight + 30 * 24 * 60 * 4;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    CAmount requiredCollateral = (params.ddAmount * 500 * COIN) / (mockOraclePrice / 100);

    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(requiredCollateral, collateralScript);

    // Add DD token output
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, params.ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript); // DD tokens have no DGB value

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(transaction_validation_invalid_mint_amount, DigiDollarValidationTestSetup)
{
    // Create mint transaction with invalid amount
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("0x1234"), 0);

    // Add DD output with invalid amount
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, 50); // $0.50 - too small
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-dd-mint-amount");
}

BOOST_FIXTURE_TEST_CASE(transaction_validation_non_dd_tx, DigiDollarValidationTestSetup)
{
    // Test that non-DD transactions pass validation
    CMutableTransaction mtx;
    mtx.nVersion = 1; // Regular transaction version

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("0x1234"), 0);

    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(1000 * COIN, GetScriptForDestination(PKHash(testPubKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(transaction_validation_unknown_tx_type, DigiDollarValidationTestSetup)
{
    // Test unknown DD transaction type
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440004; // DD_TX_VERSION | unknown type (4)

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("0x1234"), 0);

    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(1000 * COIN, GetScriptForDestination(PKHash(testPubKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-dd-tx-type");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(error_handling_script_errors, DigiDollarValidationTestSetup)
{
    // Test various script error conditions
    ScriptError serror;

    // Test malformed DD script
    CScript malformedScript = CScript() << OP_1 << ParseHex("deadbeef"); // Wrong size
    BOOST_CHECK_EQUAL(DigiDollar::IdentifyScriptType(malformedScript), DigiDollar::ScriptType::NOT_DIGIDOLLAR);

    // Test script with DD marker but invalid amount encoding
    CScript invalidAmountScript = CScript() << OP_DIGIDOLLAR << ParseHex("ff"); // Invalid number
    serror = SCRIPT_ERR_OK;
    BOOST_CHECK(!DigiDollar::ValidateDigiDollarScript(invalidAmountScript, validationContext, &serror));
    // Note: In implementation, this would likely be SCRIPT_ERR_INVALID_DD_AMOUNT
}

// ============================================================================
// MINT TRANSACTION VALIDATION TESTS (TDD - RED PHASE)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(mint_validation_valid_basic_mint, DigiDollarValidationTestSetup)
{
    // Test a valid basic mint transaction with correct collateral
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    // Add collateral input
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Create mint parameters
    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4; // 30 days
    CAmount requiredCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100); // 500% for 30 days

    // Add collateral output (P2TR with proper lock script)
    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(requiredCollateral, collateralScript);

    // Add DD token output (0 DGB value)
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // This should pass when implementation is complete
    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(mint_validation_insufficient_collateral, DigiDollarValidationTestSetup)
{
    // Test mint with insufficient collateral (should fail)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4; // 30 days
    CAmount requiredCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);
    CAmount insufficientCollateral = requiredCollateral - COIN; // 1 DGB short

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(insufficientCollateral, collateralScript);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "insufficient-collateral");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_invalid_dd_amount, DigiDollarValidationTestSetup)
{
    // Test mint with amount below minimum ($100)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 9999; // $99.99 - below minimum
    int64_t lockBlocks = 30 * 24 * 60 * 4;
    CAmount requiredCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(requiredCollateral, collateralScript);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-dd-mint-amount");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_excessive_dd_amount, DigiDollarValidationTestSetup)
{
    // Test mint with amount above maximum ($100k)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000001; // $100k + 1 cent - above maximum
    int64_t lockBlocks = 30 * 24 * 60 * 4;

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-dd-mint-amount");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_no_inputs, DigiDollarValidationTestSetup)
{
    // Test mint transaction with no inputs (should fail)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    // No inputs
    mtx.vin.clear();

    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-mint-no-inputs");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_insufficient_outputs, DigiDollarValidationTestSetup)
{
    // Test mint transaction with insufficient outputs (needs collateral + DD)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Only one output - missing either collateral or DD
    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-mint-outputs");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_invalid_collateral_script, DigiDollarValidationTestSetup)
{
    // Test mint with non-P2TR collateral script
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    CAmount requiredCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);

    // Invalid collateral script (P2PKH instead of P2TR)
    CScript invalidCollateralScript = GetScriptForDestination(PKHash(testPubKey));
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(requiredCollateral, invalidCollateralScript);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-collateral-script");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_dd_output_nonzero_value, DigiDollarValidationTestSetup)
{
    // Test mint with DD output having non-zero DGB value (should be 0)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4;
    CAmount requiredCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(requiredCollateral, collateralScript);

    // DD output with non-zero value (invalid)
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(1000, ddScript); // Should be 0

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "dd-output-value");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_dca_multiplier_adjustment, DigiDollarValidationTestSetup)
{
    // Test mint validation with DCA multiplier when system health is poor
    validationContext.systemCollateral = 110; // Triggers +50% collateral requirement

    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4; // 30 days

    // With DCA, need 500% * 1.5 = 750% collateral
    CAmount adjustedCollateral = (ddAmount * 750 * COIN) / (mockOraclePrice / 100);

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(adjustedCollateral, collateralScript);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Should pass with adjusted collateral
    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(state.IsValid());

    // Test with original 500% collateral - should fail
    CAmount originalCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);
    mtx.vout[0].nValue = originalCollateral;
    CTransaction tx2(mtx);
    TxValidationState state2;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx2, validationContext, state2));
    BOOST_CHECK(!state2.IsValid());
    BOOST_CHECK_EQUAL(state2.GetRejectReason(), "insufficient-collateral");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_multiple_dd_outputs, DigiDollarValidationTestSetup)
{
    // Test mint with multiple DD outputs (should handle correctly)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount1 = 5000; // $50.00
    CAmount ddAmount2 = 5000; // $50.00
    CAmount totalDD = ddAmount1 + ddAmount2; // $100.00 total
    int64_t lockBlocks = 30 * 24 * 60 * 4;
    CAmount requiredCollateral = (totalDD * 500 * COIN) / (mockOraclePrice / 100);

    DigiDollar::MintParams params;
    params.ddAmount = totalDD;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(3);
    mtx.vout[0] = CTxOut(requiredCollateral, collateralScript);

    // Two DD outputs
    CScript ddScript1 = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount1);
    CScript ddScript2 = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount2);
    mtx.vout[1] = CTxOut(0, ddScript1);
    mtx.vout[2] = CTxOut(0, ddScript2);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(mint_validation_invalid_oracle_price, DigiDollarValidationTestSetup)
{
    // Test mint validation when oracle price is invalid/missing
    validationContext.oraclePrice = 0; // Invalid price

    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4;

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(1000 * COIN, collateralScript); // Arbitrary amount

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-oracle-price");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_dust_collateral, DigiDollarValidationTestSetup)
{
    // Test mint with collateral below dust threshold
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4;

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(100, collateralScript); // Below dust threshold

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "collateral-dust");
}

BOOST_FIXTURE_TEST_CASE(mint_validation_edge_case_exact_minimum, DigiDollarValidationTestSetup)
{
    // Test mint with exact minimum amount and collateral
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // Exactly $100.00 (minimum)
    int64_t lockBlocks = 30 * 24 * 60 * 4;
    CAmount exactCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(exactCollateral, collateralScript);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(mint_validation_edge_case_exact_maximum, DigiDollarValidationTestSetup)
{
    // Test mint with exact maximum amount
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000000; // Exactly $100k (maximum)
    int64_t lockBlocks = 30 * 24 * 60 * 4;
    CAmount requiredCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(requiredCollateral, collateralScript);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state));
    BOOST_CHECK(state.IsValid());
}

// =============================================================================
// Transfer Transaction Validation Tests (RED Phase - Task 3.5)
// =============================================================================

/**
 * Test suite for DigiDollar transfer transaction validation
 * These tests verify DD conservation, script validation, and transfer rules
 */

BOOST_FIXTURE_TEST_CASE(test_validate_transfer_transaction_basic, DigiDollarValidationTestSetup)
{
    // Arrange: Create a basic DD transfer transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    // Add DD input
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Add DD output (same amount to preserve conservation)
    CAmount ddAmount = 10000; // $100.00
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript); // DD outputs have 0 DGB value

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate transfer transaction - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail since ValidateTransferTransaction is not implemented yet
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_dd_conservation_check, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer that violates DD conservation (input != output)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    // Add DD input
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DD output with different amount (violates conservation)
    CAmount inputAmount = 10000; // $100.00 input
    CAmount outputAmount = 5000;  // $50.00 output (violates conservation)
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, outputAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate transfer with conservation violation - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase (not implemented)
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase: Should fail due to conservation violation
    // BOOST_CHECK(!result);
    // BOOST_CHECK(state.GetRejectReason().find("conservation") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(test_transfer_invalid_dd_amounts, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer with invalid DD amounts
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321"), 0);

    // Invalid DD amount (zero)
    CAmount invalidAmount = 0;
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, invalidAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate transfer with invalid amount - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_script_validation, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer with valid P2TR scripts
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("0987654321fedcba0987654321fedcba0987654321fedcba0987654321fedcba"), 0);

    // Valid DD P2TR output
    CAmount ddAmount = 25000; // $250.00
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate script format - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_p2tr_spending_validation, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer spending valid P2TR outputs
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    // Add multiple DD inputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1111111111111111111111111111111111111111111111111111111111111111"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("2222222222222222222222222222222222222222222222222222222222222222"), 1);

    // Add corresponding DD outputs
    CAmount input1Amount = 15000; // $150.00
    CAmount input2Amount = 35000; // $350.00
    CAmount totalAmount = input1Amount + input2Amount; // $500.00

    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, totalAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate P2TR spending - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_multiple_recipients, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer to multiple recipients
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("3333333333333333333333333333333333333333333333333333333333333333"), 0);

    // Multiple DD outputs
    CKey recipient1Key, recipient2Key;
    recipient1Key.MakeNewKey(true);
    recipient2Key.MakeNewKey(true);
    XOnlyPubKey recipient1XOnly(recipient1Key.GetPubKey());
    XOnlyPubKey recipient2XOnly(recipient2Key.GetPubKey());

    CAmount amount1 = 30000; // $300.00
    CAmount amount2 = 20000; // $200.00
    CScript output1Script = DigiDollar::CreateDigiDollarP2TR(recipient1XOnly, amount1);
    CScript output2Script = DigiDollar::CreateDigiDollarP2TR(recipient2XOnly, amount2);

    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(0, output1Script);
    mtx.vout[1] = CTxOut(0, output2Script);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate multi-recipient transfer - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_utxo_set_update, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer that should update UTXO tracking
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("4444444444444444444444444444444444444444444444444444444444444444"), 0);

    CAmount ddAmount = 12500; // $125.00
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate and check UTXO updates - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase: Should update UTXO set correctly
    // BOOST_CHECK(result);
    // Verify input UTXO is spent
    // Verify new output UTXO is created
}

BOOST_FIXTURE_TEST_CASE(test_transfer_with_change_output, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer with change back to sender
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("5555555555555555555555555555555555555555555555555555555555555555"), 0);

    // Input: $100, Output: $60 to recipient + $40 change
    CAmount inputAmount = 10000;
    CAmount transferAmount = 6000;
    CAmount changeAmount = 4000;

    CKey recipientKey;
    recipientKey.MakeNewKey(true);
    XOnlyPubKey recipientXOnly(recipientKey.GetPubKey());

    CScript recipientScript = DigiDollar::CreateDigiDollarP2TR(recipientXOnly, transferAmount);
    CScript changeScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, changeAmount);

    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(0, recipientScript);
    mtx.vout[1] = CTxOut(0, changeScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate transfer with change - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_non_zero_dgb_value_rejection, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer with non-zero DGB value (should be rejected)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("6666666666666666666666666666666666666666666666666666666666666666"), 0);

    CAmount ddAmount = 15000; // $150.00
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);

    // Invalid: DD output with non-zero DGB value
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(1000, ddOutputScript); // Should be 0, not 1000

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate transfer with DGB value - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_maximum_amount_limits, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer at maximum allowed amount
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("7777777777777777777777777777777777777777777777777777777777777777"), 0);

    CAmount maxAmount = 10000000; // $100,000.00 (maximum single transfer)
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, maxAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate maximum transfer - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase: Should succeed at exactly max amount
    // BOOST_CHECK(result);
}

BOOST_FIXTURE_TEST_CASE(test_transfer_exceed_maximum_amount, DigiDollarValidationTestSetup)
{
    // Arrange: Transfer exceeding maximum allowed amount
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("8888888888888888888888888888888888888888888888888888888888888888"), 0);

    CAmount excessiveAmount = 10000001; // $100,000.01 (exceeds maximum)
    CScript ddOutputScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, excessiveAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddOutputScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate excessive transfer - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_transfer_input_output_consistency, DigiDollarValidationTestSetup)
{
    // Arrange: Comprehensive transfer validation
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER

    // Multiple inputs
    mtx.vin.resize(3);
    mtx.vin[0].prevout = COutPoint(uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"), 1);
    mtx.vin[2].prevout = COutPoint(uint256S("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"), 2);

    // Multiple outputs that should sum to same as inputs
    CAmount amount1 = 12000; // $120.00
    CAmount amount2 = 18000; // $180.00
    CAmount amount3 = 20000; // $200.00
    // Total: $500.00

    CKey key1, key2, key3;
    key1.MakeNewKey(true);
    key2.MakeNewKey(true);
    key3.MakeNewKey(true);

    CScript script1 = DigiDollar::CreateDigiDollarP2TR(XOnlyPubKey(key1.GetPubKey()), amount1);
    CScript script2 = DigiDollar::CreateDigiDollarP2TR(XOnlyPubKey(key2.GetPubKey()), amount2);
    CScript script3 = DigiDollar::CreateDigiDollarP2TR(XOnlyPubKey(key3.GetPubKey()), amount3);

    mtx.vout.resize(3);
    mtx.vout[0] = CTxOut(0, script1);
    mtx.vout[1] = CTxOut(0, script2);
    mtx.vout[2] = CTxOut(0, script3);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate complex transfer - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateTransferTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());
}

// =============================================================================
// Redemption Transaction Validation Tests (RED Phase - Task 3.8)
// =============================================================================

/**
 * Test suite for DigiDollar redemption transaction validation
 * These tests verify redemption paths, timelock validation, DD burning, and collateral release
 */

BOOST_FIXTURE_TEST_CASE(test_validate_redemption_transaction_normal_after_timelock, DigiDollarValidationTestSetup)
{
    // Arrange: Create normal redemption after timelock expiry
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add collateral input (timelock expired)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Add DD input to burn
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0)));

    // Add DGB output (collateral release)
    CAmount collateralRelease = 100 * COIN; // 100 DGB
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate redemption transaction - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail since ValidateRedemptionTransaction is not implemented yet
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase implementation:
    // BOOST_CHECK(result);
    // BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(test_validate_redemption_transaction_before_timelock, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption before timelock expiry (should fail)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add collateral input (timelock NOT expired)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Add DD input to burn
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0)));

    // Add DGB output (collateral release)
    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate early redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase: Should fail due to timelock
    // BOOST_CHECK(!result);
    // BOOST_CHECK(state.GetRejectReason().find("timelock") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(test_validate_err_redemption, DigiDollarValidationTestSetup)
{
    // Arrange: Create ERR redemption when system is unhealthy
    validationContext.systemCollateral = 80; // 80% system collateral (triggers ERR)

    CMutableTransaction mtx;
    mtx.nVersion = 0x44440005; // DD_TX_VERSION | DD_TX_EMERGENCY (ERR)

    // Add collateral and DD inputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output (reduced collateral for ERR)
    CAmount errCollateralRelease = 90 * COIN; // 90% of original (ERR penalty)
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(errCollateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate ERR redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // BOOST_CHECK(result); // Should pass when system unhealthy
}

BOOST_FIXTURE_TEST_CASE(test_validate_dd_burning_verification, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption with DD burning (DD inputs > DD outputs)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add collateral input
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Add DD inputs to burn (multiple DD UTXOs)
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("dddd1111111111111111111111111111111111111111111111111111111111"), 0)));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("dddd2222222222222222222222222222222222222222222222222222222222"), 1)));

    // Add DGB output only (no DD outputs = burning)
    CAmount collateralRelease = 150 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate DD burning - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // BOOST_CHECK(result); // Should pass - DD is properly burned
}

BOOST_FIXTURE_TEST_CASE(test_validate_collateral_release, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption with proper collateral release calculation
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add inputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output with correct collateral calculation
    CAmount ddAmount = 10000; // $100.00 being redeemed
    CAmount expectedCollateral = (ddAmount * COIN) / (mockOraclePrice / 100); // Based on current price
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(expectedCollateral, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate collateral release - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // BOOST_CHECK(result); // Should pass with correct collateral calculation
}

BOOST_FIXTURE_TEST_CASE(test_validate_partial_redemption_rules, DigiDollarValidationTestSetup)
{
    // Arrange: Create partial redemption transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440004; // DD_TX_VERSION | DD_TX_PARTIAL

    // Add collateral input
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Add DD input to burn (partial amount)
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0)));

    // Add partial DGB output to user
    CAmount partialCollateral = 50 * COIN; // Half the collateral
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(partialCollateral, GetScriptForDestination(dest));

    // Add new collateral output for remainder
    CAmount remainderCollateral = 50 * COIN;
    // Would use proper collateral script creation here
    CScript remainderScript = GetScriptForDestination(dest); // Simplified
    mtx.vout[1] = CTxOut(remainderCollateral, remainderScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate partial redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // BOOST_CHECK(result); // Should pass for valid partial redemption
}

BOOST_FIXTURE_TEST_CASE(test_validate_script_path_validation, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption with script path spending validation
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add collateral input with proper witness stack (would contain script path)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    // In real implementation, would set proper witness stack for script path spending

    // Add DD input
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0)));

    // Add DGB output
    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate script path - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should validate proper script path spending in witness
}

BOOST_FIXTURE_TEST_CASE(test_validate_utxo_update_verification, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption that should update UTXO set correctly
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add inputs that will be spent
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add output that will be created
    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate UTXO updates - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should properly track UTXO set changes
    // Collateral UTXO spent, DD UTXO spent, new DGB UTXO created
}

BOOST_FIXTURE_TEST_CASE(test_validate_invalid_redemption_no_collateral_input, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption without collateral input (should fail)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add only DD input, no collateral input
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output
    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate invalid redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase: Should still fail due to missing collateral input
    // BOOST_CHECK(!result);
    // BOOST_CHECK(state.GetRejectReason().find("collateral") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(test_validate_invalid_redemption_no_dd_inputs, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption without DD inputs to burn (should fail)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add only collateral input, no DD inputs
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    // Add DGB output
    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate invalid redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase: Should still fail due to missing DD inputs
    // BOOST_CHECK(!result);
    // BOOST_CHECK(state.GetRejectReason().find("DD") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(test_validate_invalid_collateral_amount, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption with incorrect collateral release amount
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add inputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output with INCORRECT amount (too much)
    CAmount excessiveCollateral = 1000 * COIN; // Way more than should be released
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(excessiveCollateral, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate excessive collateral - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase: Should fail due to excessive collateral release
    // BOOST_CHECK(!result);
    // BOOST_CHECK(state.GetRejectReason().find("collateral") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(test_validate_emergency_redemption_conditions, DigiDollarValidationTestSetup)
{
    // Arrange: Create emergency redemption transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440005; // DD_TX_VERSION | DD_TX_EMERGENCY

    // Add collateral and DD inputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output
    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate emergency redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should validate oracle signatures for emergency path
}

BOOST_FIXTURE_TEST_CASE(test_validate_redemption_fee_handling, DigiDollarValidationTestSetup)
{
    // Arrange: Create redemption with fee inputs and change
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add collateral, DD, and fee inputs
    mtx.vin.resize(3);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);
    mtx.vin[2].prevout = COutPoint(uint256S("fee1111111111111111111111111111111111111111111111111111111111111"), 0);

    // Add DGB output for collateral
    CAmount collateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(collateralRelease, GetScriptForDestination(dest));

    // Add DGB change output
    CAmount changeAmount = 1 * COIN; // Fee change
    mtx.vout[1] = CTxOut(changeAmount, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate fee handling - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should properly handle fees and change
}

// ============================================================================
// ERR Validation Integration Tests (RED Phase - Task 4.4)
// ============================================================================

BOOST_FIXTURE_TEST_CASE(err_validation_blocks_normal_redemptions_during_err, DigiDollarValidationTestSetup)
{
    // Arrange: System is unhealthy triggering ERR
    validationContext.systemCollateral = 90; // 90% health triggers ERR

    // Create normal redemption transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM

    // Add collateral and DD inputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output (full collateral return - not ERR adjusted)
    CAmount fullCollateralRelease = 100 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(fullCollateralRelease, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate normal redemption during ERR - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase (not implemented)
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase implementation:
    // Normal redemptions should be blocked during ERR
    // BOOST_CHECK(!result);
    // BOOST_CHECK_EQUAL(state.GetRejectReason(), "normal-redemption-blocked-during-err");
}

BOOST_FIXTURE_TEST_CASE(err_validation_allows_err_redemptions_during_err, DigiDollarValidationTestSetup)
{
    // Arrange: System is unhealthy with ERR active
    validationContext.systemCollateral = 85; // 85% health = 85% ERR return

    // Create ERR redemption transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440005; // DD_TX_VERSION | DD_TX_ERR (new ERR transaction type)

    // Add collateral and DD inputs
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output with ERR adjustment (85% of original 100 DGB = 85 DGB)
    CAmount errAdjustedCollateral = 85 * COIN;
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(errAdjustedCollateral, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate ERR redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail since ERR validation is not implemented
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase implementation:
    // ERR redemptions should be allowed with proper adjustment
    // BOOST_CHECK(result);
    // BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(err_validation_rejects_incorrect_err_adjustment, DigiDollarValidationTestSetup)
{
    // Arrange: System at 90% health (should return 90% collateral)
    validationContext.systemCollateral = 90;

    // Create ERR redemption with INCORRECT adjustment
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440005; // DD_TX_VERSION | DD_TX_ERR

    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // Add DGB output with WRONG adjustment (95% instead of 90%)
    CAmount incorrectAdjustment = 95 * COIN; // Should be 90 DGB, not 95
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(incorrectAdjustment, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate incorrect ERR redemption - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should fail due to incorrect ERR adjustment
    // BOOST_CHECK(!result);
    // BOOST_CHECK_EQUAL(state.GetRejectReason(), "invalid-err-adjustment");
}

BOOST_FIXTURE_TEST_CASE(err_validation_blocks_minting_during_err, DigiDollarValidationTestSetup)
{
    // Arrange: System is unhealthy with ERR active
    validationContext.systemCollateral = 85;

    // Create mint transaction during ERR
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4;
    CAmount requiredCollateral = (ddAmount * 500 * COIN) / (mockOraclePrice / 100);

    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = mockHeight + lockBlocks;
    params.ownerKey = testXOnlyKey;
    params.internalKey = testXOnlyKey;
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
    mtx.vout.resize(2);
    mtx.vout[0] = CTxOut(requiredCollateral, collateralScript);

    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout[1] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate mint during ERR - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateDigiDollarTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Minting should be blocked during ERR
    // BOOST_CHECK(!result);
    // BOOST_CHECK_EQUAL(state.GetRejectReason(), "minting-blocked-during-err");
}

BOOST_FIXTURE_TEST_CASE(err_validation_minimum_protection_floor, DigiDollarValidationTestSetup)
{
    // Arrange: Extremely low system health
    validationContext.systemCollateral = 30; // 30% health (severe crisis)

    // Create ERR redemption with minimum protection (80%)
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440005; // DD_TX_VERSION | DD_TX_ERR

    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);
    mtx.vin[1].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    // ERR adjustment should be minimum 80% even for extremely low health
    CAmount minimumProtection = 80 * COIN; // 80% of 100 DGB
    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(minimumProtection, GetScriptForDestination(dest));

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate minimum protection - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateRedemptionTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should pass - minimum 80% protection guaranteed
    // BOOST_CHECK(result);
    // BOOST_CHECK(state.IsValid());

    // Test with amount below minimum (should fail)
    mtx.vout[0].nValue = 75 * COIN; // Below 80% minimum
    CTransaction txBelowMin(mtx);
    TxValidationState stateBelowMin;
    result = DigiDollar::ValidateRedemptionTransaction(txBelowMin, validationContext, stateBelowMin);
    BOOST_CHECK(!result);

    // After GREEN phase:
    // BOOST_CHECK(!result); // Should fail - below minimum protection
}

// ============================================================================
// Volatility Protection Validation Tests
// ============================================================================

BOOST_FIXTURE_TEST_CASE(volatility_validation_mint_allowed_stable, DigiDollarValidationTestSetup)
{
    using namespace DigiDollar::Volatility;

    // Arrange: Set up stable price history
    VolatilityMonitor::ClearHistory();
    int64_t baseTime = GetTime();

    // Record stable prices for 24 hours
    for (int hour = 0; hour < 24; hour++) {
        VolatilityMonitor::RecordPrice(mockOraclePrice, baseTime + hour * 3600, mockHeight + hour);
    }

    // Create mint transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate mint with stable volatility - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateMintTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase (not implemented yet)
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should pass - no volatility restrictions
    // BOOST_CHECK(result);
    // BOOST_CHECK(state.IsValid());
    // BOOST_CHECK(!VolatilityMonitor::ShouldFreezeMinting());
}

BOOST_FIXTURE_TEST_CASE(volatility_validation_mint_blocked_high_volatility, DigiDollarValidationTestSetup)
{
    using namespace DigiDollar::Volatility;

    // Arrange: Set up high volatility scenario
    VolatilityMonitor::ClearHistory();
    int64_t baseTime = GetTime();

    // Record high volatility - 25% swing in 1 hour
    VolatilityMonitor::RecordPrice(mockOraclePrice, baseTime, mockHeight);
    VolatilityMonitor::RecordPrice(mockOraclePrice * 125 / 100, baseTime + 3600, mockHeight + 1);

    // Trigger freeze check
    VolatilityMonitor::UpdateState(mockHeight + 1);

    // Create mint transaction
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"), 0);

    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate mint with high volatility - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateMintTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);
    BOOST_CHECK(!state.IsValid());

    // After GREEN phase:
    // Should fail - minting frozen due to volatility
    // BOOST_CHECK(!result);
    // BOOST_CHECK(!state.IsValid());
    // BOOST_CHECK_EQUAL(state.GetRejectReason(), "minting-frozen-volatility");
    // BOOST_CHECK(VolatilityMonitor::ShouldFreezeMinting());
}

BOOST_FIXTURE_TEST_CASE(volatility_validation_all_operations_blocked, DigiDollarValidationTestSetup)
{
    using namespace DigiDollar::Volatility;

    // Arrange: Set up extreme volatility scenario
    VolatilityMonitor::ClearHistory();
    int64_t baseTime = GetTime();

    // Record extreme 24h volatility (35%)
    for (int hour = 0; hour < 24; hour++) {
        CAmount swingPrice;
        if (hour % 4 == 0) {
            swingPrice = mockOraclePrice * 135 / 100; // +35%
        } else if (hour % 4 == 2) {
            swingPrice = mockOraclePrice * 65 / 100;  // -35%
        } else {
            swingPrice = mockOraclePrice; // baseline
        }
        VolatilityMonitor::RecordPrice(swingPrice, baseTime + hour * 3600, mockHeight + hour);
    }

    // Trigger freeze check
    VolatilityMonitor::UpdateState(mockHeight + 24);

    // Test 1: Mint transaction
    CMutableTransaction mintTx;
    mintTx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT
    mintTx.vin.resize(1);
    mintTx.vin[0].prevout = COutPoint(uint256S("1111111111111111111111111111111111111111111111111111111111111111"), 0);

    CAmount ddAmount = 5000; // $50.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mintTx.vout.resize(1);
    mintTx.vout[0] = CTxOut(0, ddScript);

    CTransaction mint(mintTx);
    TxValidationState mintState;

    // Test 2: Transfer transaction
    CMutableTransaction transferTx;
    transferTx.nVersion = 0x44440002; // DD_TX_VERSION | DD_TX_TRANSFER
    transferTx.vin.resize(1);
    transferTx.vin[0].prevout = COutPoint(uint256S("2222222222222222222222222222222222222222222222222222222222222222"), 0);
    transferTx.vout.resize(1);
    transferTx.vout[0] = CTxOut(0, ddScript);

    CTransaction transfer(transferTx);
    TxValidationState transferState;

    // Test 3: Redeem transaction
    CMutableTransaction redeemTx;
    redeemTx.nVersion = 0x44440003; // DD_TX_VERSION | DD_TX_REDEEM
    redeemTx.vin.resize(1);
    redeemTx.vin[0].prevout = COutPoint(uint256S("3333333333333333333333333333333333333333333333333333333333333333"), 0);

    CPubKey ownerPubkey = testKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(ownerPubkey))};
    redeemTx.vout.resize(1);
    redeemTx.vout[0] = CTxOut(10000, GetScriptForDestination(dest)); // $100.00 in DGB

    CTransaction redeem(redeemTx);
    TxValidationState redeemState;

    // Act: Validate all transaction types - EXPECTED TO FAIL (RED phase)
    bool mintResult = DigiDollar::ValidateMintTransaction(mint, validationContext, mintState);
    bool transferResult = DigiDollar::ValidateTransferTransaction(transfer, validationContext, transferState);
    bool redeemResult = DigiDollar::ValidateRedemptionTransaction(redeem, validationContext, redeemState);

    // Assert: Should all fail in RED phase
    BOOST_CHECK(!mintResult);
    BOOST_CHECK(!transferResult);
    BOOST_CHECK(!redeemResult);

    // After GREEN phase:
    // All should fail - all operations frozen
    // BOOST_CHECK(!mintResult);
    // BOOST_CHECK(!transferResult);
    // BOOST_CHECK(!redeemResult);
    // BOOST_CHECK_EQUAL(mintState.GetRejectReason(), "all-operations-frozen");
    // BOOST_CHECK_EQUAL(transferState.GetRejectReason(), "all-operations-frozen");
    // BOOST_CHECK_EQUAL(redeemState.GetRejectReason(), "all-operations-frozen");
    // BOOST_CHECK(VolatilityMonitor::ShouldFreezeAll());
}

BOOST_FIXTURE_TEST_CASE(volatility_validation_cooldown_enforcement, DigiDollarValidationTestSetup)
{
    using namespace DigiDollar::Volatility;

    // Arrange: Trigger freeze and then stabilize
    VolatilityMonitor::ClearHistory();
    int64_t baseTime = GetTime();

    // High volatility to trigger freeze
    VolatilityMonitor::RecordPrice(mockOraclePrice, baseTime, mockHeight);
    VolatilityMonitor::RecordPrice(mockOraclePrice * 125 / 100, baseTime + 3600, mockHeight + 1);
    VolatilityMonitor::UpdateState(mockHeight + 1);

    // Verify freeze is active
    BOOST_CHECK(VolatilityMonitor::ShouldFreezeMinting());
    BOOST_CHECK(VolatilityMonitor::InCooldownPeriod());

    // Stabilize prices
    for (int hour = 2; hour < 26; hour++) {
        VolatilityMonitor::RecordPrice(mockOraclePrice, baseTime + hour * 3600, mockHeight + hour);
    }

    // Create mint transaction during cooldown
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("4444444444444444444444444444444444444444444444444444444444444444"), 0);

    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);

    // Test during cooldown period
    validationContext.nHeight = mockHeight + 50; // Still in cooldown
    TxValidationState stateCooldown;

    // Act: Validate during cooldown - EXPECTED TO FAIL (RED phase)
    bool resultCooldown = DigiDollar::ValidateMintTransaction(tx, validationContext, stateCooldown);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!resultCooldown);

    // After GREEN phase:
    // Should fail - still in cooldown despite stable prices
    // BOOST_CHECK(!resultCooldown);
    // BOOST_CHECK_EQUAL(stateCooldown.GetRejectReason(), "minting-frozen-volatility");

    // Test after cooldown period
    validationContext.nHeight = mockHeight + 200; // After cooldown
    VolatilityMonitor::UpdateState(validationContext.nHeight);
    TxValidationState stateAfterCooldown;

    // Act: Validate after cooldown - EXPECTED TO FAIL (RED phase)
    bool resultAfterCooldown = DigiDollar::ValidateMintTransaction(tx, validationContext, stateAfterCooldown);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!resultAfterCooldown);

    // After GREEN phase:
    // Should pass - cooldown expired and prices stable
    // BOOST_CHECK(resultAfterCooldown);
    // BOOST_CHECK(stateAfterCooldown.IsValid());
    // BOOST_CHECK(!VolatilityMonitor::InCooldownPeriod());
}

BOOST_FIXTURE_TEST_CASE(volatility_validation_override_mechanism, DigiDollarValidationTestSetup)
{
    using namespace DigiDollar::Volatility;

    // Arrange: Trigger freeze
    VolatilityMonitor::ClearHistory();
    int64_t baseTime = GetTime();

    VolatilityMonitor::RecordPrice(mockOraclePrice, baseTime, mockHeight);
    VolatilityMonitor::RecordPrice(mockOraclePrice * 125 / 100, baseTime + 3600, mockHeight + 1);
    VolatilityMonitor::UpdateState(mockHeight + 1);

    BOOST_CHECK(VolatilityMonitor::ShouldFreezeMinting());

    // Create sufficient oracle approvals for override
    std::vector<COraclePriceMessage> approvals;
    for (int i = 0; i < 8; i++) {
        CKey oracleKey;
        oracleKey.MakeNewKey(true);

        COraclePriceMessage msg;
        msg.price_satoshis = mockOraclePrice;
        msg.timestamp = baseTime + 3600;
        msg.oracle_id = i; // Use loop index as oracle ID

        // TODO: Fix SerializeHash call - may need proper serialization
        // uint256 hash = SerializeHash(msg);
        uint256 hash = uint256S("0000000000000000000000000000000000000000000000000000000000000000"); // Mock hash
        // TODO: Fix signature call - may need proper signing method
        // oracleKey.SignSchnorr(hash, msg.signature);
        msg.signature = std::vector<unsigned char>(64, 0); // Mock signature

        approvals.push_back(msg);
    }

    // Apply override
    BOOST_CHECK(VolatilityMonitor::OverrideFreeze(approvals));
    BOOST_CHECK(!VolatilityMonitor::ShouldFreezeMinting());

    // Create mint transaction after override
    CMutableTransaction mtx;
    mtx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("5555555555555555555555555555555555555555555555555555555555555555"), 0);

    CAmount ddAmount = 10000; // $100.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mtx.vout.resize(1);
    mtx.vout[0] = CTxOut(0, ddScript);

    CTransaction tx(mtx);
    TxValidationState state;

    // Act: Validate after override - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateMintTransaction(tx, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);

    // After GREEN phase:
    // Should pass - freeze was overridden by oracle consensus
    // BOOST_CHECK(result);
    // BOOST_CHECK(state.IsValid());
    // BOOST_CHECK(!VolatilityMonitor::ShouldFreezeMinting());
}

BOOST_FIXTURE_TEST_CASE(volatility_validation_gradual_unfreezing, DigiDollarValidationTestSetup)
{
    using namespace DigiDollar::Volatility;

    // Arrange: Trigger all operations freeze
    VolatilityMonitor::ClearHistory();
    int64_t baseTime = GetTime();

    // Create extreme volatility scenario
    for (int hour = 0; hour < 24; hour++) {
        CAmount swingPrice = (hour % 4 < 2) ? mockOraclePrice * 135 / 100 : mockOraclePrice * 65 / 100;
        VolatilityMonitor::RecordPrice(swingPrice, baseTime + hour * 3600, mockHeight + hour);
    }

    VolatilityMonitor::UpdateState(mockHeight + 24);
    BOOST_CHECK(VolatilityMonitor::ShouldFreezeAll());

    // Stabilize prices for extended period
    for (int hour = 24; hour < 48; hour++) {
        VolatilityMonitor::RecordPrice(mockOraclePrice, baseTime + hour * 3600, mockHeight + hour);
    }

    // Advance beyond cooldown period
    uint32_t testHeight = mockHeight + 200; // Well beyond cooldown
    VolatilityMonitor::UpdateState(testHeight);

    // Create test transactions
    CMutableTransaction mintTx;
    mintTx.nVersion = 0x44440001; // DD_TX_VERSION | DD_TX_MINT
    mintTx.vin.resize(1);
    mintTx.vin[0].prevout = COutPoint(uint256S("6666666666666666666666666666666666666666666666666666666666666666"), 0);

    CAmount ddAmount = 5000; // $50.00
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, ddAmount);
    mintTx.vout.resize(1);
    mintTx.vout[0] = CTxOut(0, ddScript);

    CTransaction mint(mintTx);
    validationContext.nHeight = testHeight;
    TxValidationState state;

    // Act: Validate after stabilization period - EXPECTED TO FAIL (RED phase)
    bool result = DigiDollar::ValidateMintTransaction(mint, validationContext, state);

    // Assert: Should fail in RED phase
    BOOST_CHECK(!result);

    // After GREEN phase:
    // Should pass - volatility stabilized and cooldown expired
    // BOOST_CHECK(result);
    // BOOST_CHECK(state.IsValid());
    // BOOST_CHECK(!VolatilityMonitor::ShouldFreezeAll());
    // BOOST_CHECK(!VolatilityMonitor::ShouldFreezeMinting());

    // Verify volatility is low
    auto volatilityState = VolatilityMonitor::GetCurrentState();
    // BOOST_CHECK(volatilityState.dailyVolatility < 10.0);
    // BOOST_CHECK(volatilityState.weeklyVolatility < 20.0);
}

BOOST_AUTO_TEST_SUITE_END()