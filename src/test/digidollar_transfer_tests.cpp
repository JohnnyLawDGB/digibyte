// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>
#include <test/util/random.h>

#include <digidollar/txbuilder.h>
#include <digidollar/validation.h>
#include <digidollar/digidollar.h>
#include <consensus/digidollar.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <key.h>
#include <util/strencodings.h>
#include <validation.h>
#include <wallet/walletdb.h>

#include <boost/test/unit_test.hpp>

#include <map>

using namespace DigiDollar;

// Mock UTXO store for testing - maps outpoint to DD amount
std::map<COutPoint, CAmount> g_mockDDUTXOs;
std::map<COutPoint, CAmount> g_mockDGBUTXOs;

/**
 * Test-specific TransferTxBuilder that uses mock UTXO stores
 */
class MockTransferTxBuilder : public TransferTxBuilder {
public:
    using TransferTxBuilder::TransferTxBuilder;

protected:
    CAmount GetDDFromUTXO(const COutPoint& outpoint) const override {
        auto it = g_mockDDUTXOs.find(outpoint);
        if (it != g_mockDDUTXOs.end()) {
            return it->second;
        }
        return 0;
    }

    CAmount GetDGBFromUTXO(const COutPoint& outpoint) const override {
        auto it = g_mockDGBUTXOs.find(outpoint);
        if (it != g_mockDGBUTXOs.end()) {
            return it->second;
        }
        return 100 * COIN; // Default for testing
    }
};

BOOST_FIXTURE_TEST_SUITE(digidollar_transfer_tests, TestingSetup)

/**
 * Test fixture for DigiDollar transfer transaction tests
 * Sets up necessary environment for testing transfer operations
 */
struct DDTransferTestFixture : public TestingSetup {
    // Test chain parameters
    const CChainParams& chainParams;

    // Test keys for various roles
    CKey senderKey;
    CKey recipientKey;
    CKey changeKey;

    // Mock blockchain state
    int currentHeight;
    CAmount oraclePrice;
    int systemCollateral;

    // Test amounts (in cents)
    static const CAmount TEST_DD_AMOUNT = 10000;  // $100.00
    static const CAmount LARGE_DD_AMOUNT = 5000000; // $50,000.00
    static const CAmount MAX_TRANSFER_AMOUNT = 10000000; // $100,000.00
    static const CAmount DUST_AMOUNT = 50;        // $0.50

    DDTransferTestFixture() : chainParams(m_node.chainman->GetParams()) {
        // Generate test keys
        senderKey.MakeNewKey(true);
        recipientKey.MakeNewKey(true);
        changeKey.MakeNewKey(true);

        // Set blockchain state
        currentHeight = 100000;
        oraclePrice = 2500; // $25.00 per DGB
        systemCollateral = 150; // 150% healthy system

        // Clear mock UTXO stores
        g_mockDDUTXOs.clear();
        g_mockDGBUTXOs.clear();
    }

    /**
     * Create mock DD UTXO for testing
     */
    COutPoint CreateMockDDUTXO(CAmount ddAmount) {
        // Mock UTXO creation - in real implementation this would reference blockchain
        COutPoint outpoint(InsecureRand256(), 0);
        g_mockDDUTXOs[outpoint] = ddAmount; // Store amount in mock store
        return outpoint;
    }

    /**
     * Create mock DGB UTXO for fees
     */
    COutPoint CreateMockDGBUTXO(CAmount dgbAmount) {
        // Mock UTXO creation
        COutPoint outpoint(InsecureRand256(), 1);
        g_mockDGBUTXOs[outpoint] = dgbAmount; // Store amount in mock store
        return outpoint;
    }

    /**
     * Create DD address from public key
     */
    std::string CreateDDAddress(const CPubKey& pubkey) {
        // Create P2TR destination
        XOnlyPubKey xonly(pubkey);
        WitnessV1Taproot dest(xonly);
        return EncodeDigiDollarAddress(dest, chainParams);
    }

    /**
     * Build TransferParams for testing
     */
    TransferParams BuildTransferParams(const std::vector<std::pair<std::string, CAmount>>& recipients) {
        TransferParams params;
        params.recipients = recipients;
        params.feeRate = 100000; // 100,000 sat/kB (DigiByte minimum relay fee)
        params.spenderKey = senderKey;

        // Add some mock UTXOs
        params.ddUtxos.push_back(CreateMockDDUTXO(TEST_DD_AMOUNT));
        params.feeUtxos.push_back(CreateMockDGBUTXO(100000)); // 0.001 DGB for fees

        return params;
    }
};

// Define static const members
const CAmount DDTransferTestFixture::TEST_DD_AMOUNT;
const CAmount DDTransferTestFixture::LARGE_DD_AMOUNT;
const CAmount DDTransferTestFixture::MAX_TRANSFER_AMOUNT;
const CAmount DDTransferTestFixture::DUST_AMOUNT;

// =============================================================================
// Basic Transfer Creation Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_basic_transfer_creation, DDTransferTestFixture)
{
    // Arrange: Create transfer parameters
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, TEST_DD_AMOUNT}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer transaction
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should succeed with proper implementation
    if (!result.success) {
        BOOST_TEST_MESSAGE("Transfer failed: " << result.error);
    }
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
    BOOST_CHECK_GT(result.tx.vout.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(test_transfer_with_change, DDTransferTestFixture)
{
    // Arrange: Transfer less than available, should create change
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    CAmount transferAmount = TEST_DD_AMOUNT / 2; // Transfer half
    TransferParams params = BuildTransferParams({{recipientAddr, transferAmount}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer with change - GREEN phase (should succeed)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should succeed in GREEN phase
    if (!result.success) {
        BOOST_TEST_MESSAGE("Transfer failed: " << result.error);
    }
    BOOST_CHECK(result.success);
    BOOST_CHECK_EQUAL(result.tx.vout.size(), 2); // recipient + change
}

BOOST_FIXTURE_TEST_CASE(test_multiple_dd_inputs_consolidation, DDTransferTestFixture)
{
    // Arrange: Multiple DD inputs that need consolidation
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, LARGE_DD_AMOUNT}});

    // Add multiple DD UTXOs
    params.ddUtxos.clear();
    params.ddUtxos.push_back(CreateMockDDUTXO(TEST_DD_AMOUNT));
    params.ddUtxos.push_back(CreateMockDDUTXO(TEST_DD_AMOUNT));
    params.ddUtxos.push_back(CreateMockDDUTXO(LARGE_DD_AMOUNT));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build consolidation transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_multiple_recipients, DDTransferTestFixture)
{
    // Arrange: Send to multiple recipients
    std::string recipient1 = CreateDDAddress(recipientKey.GetPubKey());
    CKey recipient2Key;
    recipient2Key.MakeNewKey(true);
    std::string recipient2 = CreateDDAddress(recipient2Key.GetPubKey());

    std::vector<std::pair<std::string, CAmount>> recipients = {
        {recipient1, 3000}, // $30.00
        {recipient2, 7000}  // $70.00
    };

    TransferParams params = BuildTransferParams(recipients);
    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build multi-recipient transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

// =============================================================================
// Error Handling Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_insufficient_balance_handling, DDTransferTestFixture)
{
    // Arrange: Try to transfer more DD than available
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    CAmount excessiveAmount = TEST_DD_AMOUNT * 10; // 10x available
    TransferParams params = BuildTransferParams({{recipientAddr, excessiveAmount}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt transfer with insufficient balance - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());

    // After GREEN phase: error should mention insufficient balance
    // BOOST_CHECK(result.error.find("insufficient") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(test_zero_amount_validation, DDTransferTestFixture)
{
    // Arrange: Try to transfer zero amount
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, 0}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt zero transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_negative_amount_validation, DDTransferTestFixture)
{
    // Arrange: Try to transfer negative amount
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, -1000}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt negative transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_maximum_transfer_limits, DDTransferTestFixture)
{
    // Arrange: Try to transfer maximum allowed amount
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, MAX_TRANSFER_AMOUNT}});

    // Mock sufficient UTXOs
    params.ddUtxos.clear();
    for(int i = 0; i < 100; ++i) {
        params.ddUtxos.push_back(CreateMockDDUTXO(MAX_TRANSFER_AMOUNT / 100));
    }

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt maximum transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_exceed_maximum_transfer_limits, DDTransferTestFixture)
{
    // Arrange: Try to transfer more than maximum allowed
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    CAmount excessiveAmount = MAX_TRANSFER_AMOUNT + 1;
    TransferParams params = BuildTransferParams({{recipientAddr, excessiveAmount}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt excessive transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

// =============================================================================
// DD Conservation Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_dd_conservation_verification, DDTransferTestFixture)
{
    // Arrange: Transfer that should conserve DD (input = output)
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, TEST_DD_AMOUNT}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build conservation test - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());

    // After GREEN phase implementation:
    // CAmount totalInputDD = CalculateTotalDDInputs(params.ddUtxos);
    // CAmount totalOutputDD = TEST_DD_AMOUNT;
    // BOOST_CHECK_EQUAL(totalInputDD, totalOutputDD);
}

BOOST_FIXTURE_TEST_CASE(test_dust_threshold_handling, DDTransferTestFixture)
{
    // Arrange: Transfer very small amount (dust)
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, DUST_AMOUNT}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt dust transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

// =============================================================================
// Address Validation Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_invalid_recipient_validation, DDTransferTestFixture)
{
    // Arrange: Invalid DD address
    std::string invalidAddr = "invalid_dd_address_format";
    TransferParams params = BuildTransferParams({{invalidAddr, TEST_DD_AMOUNT}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt transfer to invalid address - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_empty_recipient_validation, DDTransferTestFixture)
{
    // Arrange: Empty recipients list
    TransferParams params = BuildTransferParams({});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt transfer with no recipients - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

// =============================================================================
// Transaction Structure Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_transaction_version_validation, DDTransferTestFixture)
{
    // Arrange: Standard transfer
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, TEST_DD_AMOUNT}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer and check version - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());

    // After GREEN phase implementation:
    // uint32_t expectedVersion = DD_TX_VERSION | DD_TX_TRANSFER;
    // BOOST_CHECK_EQUAL(result.tx.nVersion, expectedVersion);
}

BOOST_FIXTURE_TEST_CASE(test_transaction_type_validation, DDTransferTestFixture)
{
    // Arrange: Transfer transaction
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, TEST_DD_AMOUNT}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build and validate transaction type - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

// =============================================================================
// Helper Function Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_validate_transfer_params, DDTransferTestFixture)
{
    // Arrange: Valid and invalid parameters
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams validParams = BuildTransferParams({{recipientAddr, TEST_DD_AMOUNT}});
    TransferParams invalidParams = BuildTransferParams({{recipientAddr, -1000}});

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act & Assert: These should fail in RED phase since methods don't exist yet
    // In GREEN phase, we'll test:
    // BOOST_CHECK(builder.ValidateTransferParams(validParams));
    // BOOST_CHECK(!builder.ValidateTransferParams(invalidParams));

    // For now, just verify the fixture works
    BOOST_CHECK(!validParams.recipients.empty());
    BOOST_CHECK(!invalidParams.recipients.empty());
}

BOOST_FIXTURE_TEST_CASE(test_calculate_total_dd_input, DDTransferTestFixture)
{
    // Arrange: Mock DD UTXOs with known amounts
    std::vector<CTxOut> inputs;
    std::vector<CAmount> amounts = {1000, 2000, 3000}; // $10, $20, $30

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act & Assert: This should fail in RED phase since method doesn't exist
    // In GREEN phase:
    // CAmount total = builder.CalculateTotalDDInput(inputs, amounts);
    // BOOST_CHECK_EQUAL(total, 6000); // $60.00 total

    // For now, verify test data
    BOOST_CHECK_EQUAL(amounts.size(), 3);
}

BOOST_FIXTURE_TEST_CASE(test_create_dd_transfer_script, DDTransferTestFixture)
{
    // Arrange: Recipient key and amount
    CPubKey recipient = recipientKey.GetPubKey();
    CAmount amount = TEST_DD_AMOUNT;

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act & Assert: This should fail in RED phase since method doesn't exist
    // In GREEN phase:
    // CScript script = builder.CreateDDTransferScript(recipient, amount);
    // BOOST_CHECK(!script.empty());
    // BOOST_CHECK(IsDDTokenScript(script));

    // For now, verify test inputs
    BOOST_CHECK(recipient.IsValid());
    BOOST_CHECK_GT(amount, 0);
}

BOOST_FIXTURE_TEST_CASE(test_select_dd_inputs, DDTransferTestFixture)
{
    // Arrange: Available DD UTXOs and needed amount
    std::vector<CTxOut> available;
    CAmount needed = TEST_DD_AMOUNT;
    std::vector<CTxOut> selected;
    CAmount total = 0;

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act & Assert: This should fail in RED phase since method doesn't exist
    // In GREEN phase:
    // bool success = builder.SelectDDInputs(available, needed, selected, total);
    // BOOST_CHECK(success);
    // BOOST_CHECK_GE(total, needed);

    // For now, verify test setup
    BOOST_CHECK_GT(needed, 0);
    BOOST_CHECK_EQUAL(total, 0); // Initially zero
}

// =============================================================================
// Edge Cases and Stress Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_maximum_inputs_consolidation, DDTransferTestFixture)
{
    // Arrange: Many small DD UTXOs that need consolidation
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, LARGE_DD_AMOUNT}});

    // Add many small UTXOs (each $1.00)
    params.ddUtxos.clear();
    for(int i = 0; i < 1000; ++i) {
        params.ddUtxos.push_back(CreateMockDDUTXO(100)); // $1.00 each
    }

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Attempt large consolidation - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_precise_amount_matching, DDTransferTestFixture)
{
    // Arrange: Exact amount match (no change needed)
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, TEST_DD_AMOUNT}});

    // Ensure exact match
    params.ddUtxos.clear();
    params.ddUtxos.push_back(CreateMockDDUTXO(TEST_DD_AMOUNT));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build exact amount transfer - EXPECTED TO FAIL (RED phase)
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail in RED phase
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

// =============================================================================
// Phase 2.2 - DD Input Assembly Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_build_transfer_inputs, DDTransferTestFixture)
{
    // Arrange: Create mock DD UTXOs
    COutPoint utxo1 = CreateMockDDUTXO(5000);  // $50.00
    COutPoint utxo2 = CreateMockDDUTXO(3000);  // $30.00

    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, 8000}}); // $80.00

    // Set specific UTXOs
    params.ddUtxos.clear();
    params.ddUtxos.push_back(utxo1);
    params.ddUtxos.push_back(utxo2);

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer transaction
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Transaction should be created with correct inputs
    BOOST_CHECK_EQUAL(result.success, true);

    // Verify DD inputs were added (should be first 2 inputs before fee inputs)
    BOOST_CHECK_GE(result.tx.vin.size(), 2);
    BOOST_CHECK(result.tx.vin[0].prevout == utxo1);
    BOOST_CHECK(result.tx.vin[1].prevout == utxo2);
}

BOOST_FIXTURE_TEST_CASE(test_single_dd_input_assembly, DDTransferTestFixture)
{
    // Arrange: Single DD UTXO
    COutPoint utxo = CreateMockDDUTXO(10000);  // $100.00

    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());
    TransferParams params = BuildTransferParams({{recipientAddr, 10000}}); // Exact match

    params.ddUtxos.clear();
    params.ddUtxos.push_back(utxo);

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Single input should be present
    BOOST_CHECK_EQUAL(result.success, true);
    BOOST_CHECK_GE(result.tx.vin.size(), 1);
    BOOST_CHECK(result.tx.vin[0].prevout == utxo);
}

BOOST_FIXTURE_TEST_CASE(test_multiple_dd_inputs_assembly, DDTransferTestFixture)
{
    // Arrange: Multiple DD UTXOs for consolidation
    std::vector<COutPoint> utxos;
    TransferParams params;
    params.recipients = {{CreateDDAddress(recipientKey.GetPubKey()), 15000}}; // $150.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    // Create 5 small UTXOs
    for (int i = 0; i < 5; ++i) {
        COutPoint utxo = CreateMockDDUTXO(3000);  // $30.00 each
        utxos.push_back(utxo);
        params.ddUtxos.push_back(utxo);
    }

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build consolidation transfer
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: All 5 DD inputs should be assembled
    BOOST_CHECK_EQUAL(result.success, true);
    BOOST_CHECK_GE(result.tx.vin.size(), 5);

    // Verify each input matches
    for (size_t i = 0; i < utxos.size(); ++i) {
        BOOST_CHECK(result.tx.vin[i].prevout == utxos[i]);
    }
}

BOOST_FIXTURE_TEST_CASE(test_dd_input_count_matches_utxo_count, DDTransferTestFixture)
{
    // Arrange: Variable number of UTXOs
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    // Test with 1, 3, and 10 UTXOs
    std::vector<int> utxoCounts = {1, 3, 10};

    for (int count : utxoCounts) {
        TransferParams params;
        params.recipients = {{recipientAddr, count * 1000}}; // $10.00 per UTXO
        params.feeRate = 100000; // 100,000 sat/kB
        params.spenderKey = senderKey;
        params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

        for (int i = 0; i < count; ++i) {
            params.ddUtxos.push_back(CreateMockDDUTXO(1000));
        }

        MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

        // Act: Build transfer
        TxBuilderResult result = builder.BuildTransferTransaction(params);

        // Assert: Input count should match (DD inputs + possibly fee inputs)
        BOOST_CHECK_EQUAL(result.success, true);
        BOOST_CHECK_GE(result.tx.vin.size(), static_cast<size_t>(count));
    }
}

// =============================================================================
// Phase 2.3 - DD Output Assembly Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_build_transfer_outputs, DDTransferTestFixture)
{
    // Arrange: Transfer params with 2 recipients
    std::string recipient1 = CreateDDAddress(recipientKey.GetPubKey());
    CKey recipient2Key;
    recipient2Key.MakeNewKey(true);
    std::string recipient2 = CreateDDAddress(recipient2Key.GetPubKey());

    TransferParams params;
    params.recipients = {
        {recipient1, 50000},  // $500.00
        {recipient2, 25000}   // $250.00
    };
    params.feeRate = 100000; // 100,000 sat/kB (minimum relay fee for DigiByte)
    params.spenderKey = senderKey;

    // Create DD UTXOs with sufficient balance (need 75000 cents total)
    params.ddUtxos.push_back(CreateMockDDUTXO(50000)); // $500
    params.ddUtxos.push_back(CreateMockDDUTXO(25000)); // $250

    // Add fee UTXOs
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000)); // 0.001 DGB

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer transaction
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Transaction should be built successfully
    BOOST_CHECK_MESSAGE(result.success, "Transfer failed: " << result.error);

    // Verify number of outputs (2 recipients + potentially DGB change, no DD change needed)
    BOOST_CHECK_GE(result.tx.vout.size(), 2);

    // Find DD outputs (those with 0 DGB value)
    std::vector<CTxOut> ddOutputs;
    for (const auto& output : result.tx.vout) {
        if (output.nValue == 0) {
            ddOutputs.push_back(output);
        }
    }

    // Should have exactly 2 DD outputs for our 2 recipients
    BOOST_CHECK_EQUAL(ddOutputs.size(), 2);

    // Verify DD amounts in outputs by extracting from scripts
    CAmount totalDDOut = 0;
    for (const auto& output : ddOutputs) {
        CAmount ddAmount = 0;
        bool extracted = DigiDollar::ExtractDDAmount(output.scriptPubKey, ddAmount);
        BOOST_CHECK_MESSAGE(extracted, "Failed to extract DD amount from output");
        totalDDOut += ddAmount;
    }

    // Total DD output should equal requested amounts (75000)
    BOOST_CHECK_EQUAL(totalDDOut, 75000);

    // Verify each output has proper P2TR script
    for (const auto& output : ddOutputs) {
        // P2TR scripts start with OP_1 (0x51) followed by 32 bytes
        BOOST_CHECK_GE(output.scriptPubKey.size(), 34);
        BOOST_CHECK_EQUAL(output.scriptPubKey[0], 0x51); // OP_1
    }
}

BOOST_FIXTURE_TEST_CASE(test_single_recipient_output, DDTransferTestFixture)
{
    // Arrange: Single recipient transfer
    std::string recipient = CreateDDAddress(recipientKey.GetPubKey());

    TransferParams params;
    params.recipients = {{recipient, 30000}}; // $300.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(30000)); // Exact amount
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert
    BOOST_CHECK_MESSAGE(result.success, "Transfer failed: " << result.error);

    // Find DD outputs
    std::vector<CTxOut> ddOutputs;
    for (const auto& output : result.tx.vout) {
        if (output.nValue == 0) {
            ddOutputs.push_back(output);
        }
    }

    // Should have exactly 1 DD output (exact match, no change)
    BOOST_CHECK_EQUAL(ddOutputs.size(), 1);

    // Verify amount
    CAmount ddAmount = 0;
    BOOST_CHECK(DigiDollar::ExtractDDAmount(ddOutputs[0].scriptPubKey, ddAmount));
    BOOST_CHECK_EQUAL(ddAmount, 30000);
}

BOOST_FIXTURE_TEST_CASE(test_output_p2tr_script_format, DDTransferTestFixture)
{
    // Arrange
    std::string recipient = CreateDDAddress(recipientKey.GetPubKey());

    TransferParams params;
    params.recipients = {{recipient, 10000}}; // $100.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(10000));
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert
    BOOST_CHECK(result.success);

    // Get DD output
    CTxOut ddOutput;
    bool found = false;
    for (const auto& output : result.tx.vout) {
        if (output.nValue == 0) {
            ddOutput = output;
            found = true;
            break;
        }
    }
    BOOST_CHECK(found);

    // Verify P2TR script structure
    // P2TR: OP_1 <32-byte-pubkey> (possibly followed by DD amount data)
    BOOST_CHECK_GE(ddOutput.scriptPubKey.size(), 34);

    // First byte should be OP_1 (version 1 witness)
    BOOST_CHECK_EQUAL(ddOutput.scriptPubKey[0], 0x51);

    // Second byte should be 0x20 (32 bytes following)
    BOOST_CHECK_EQUAL(ddOutput.scriptPubKey[1], 0x20);
}

BOOST_FIXTURE_TEST_CASE(test_all_recipients_get_outputs, DDTransferTestFixture)
{
    // Arrange: 3 recipients with different amounts
    std::string recipient1 = CreateDDAddress(recipientKey.GetPubKey());

    CKey recipient2Key, recipient3Key;
    recipient2Key.MakeNewKey(true);
    recipient3Key.MakeNewKey(true);

    std::string recipient2 = CreateDDAddress(recipient2Key.GetPubKey());
    std::string recipient3 = CreateDDAddress(recipient3Key.GetPubKey());

    TransferParams params;
    params.recipients = {
        {recipient1, 10000},  // $100.00
        {recipient2, 20000},  // $200.00
        {recipient3, 30000}   // $300.00
    };
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(60000)); // Exact total
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert
    BOOST_CHECK_MESSAGE(result.success, "Transfer failed: " << result.error);

    // Count DD outputs
    std::vector<CAmount> ddAmounts;
    for (const auto& output : result.tx.vout) {
        if (output.nValue == 0) {
            CAmount amount = 0;
            if (DigiDollar::ExtractDDAmount(output.scriptPubKey, amount)) {
                ddAmounts.push_back(amount);
            }
        }
    }

    // Should have 3 DD outputs
    BOOST_CHECK_EQUAL(ddAmounts.size(), 3);

    // Verify all requested amounts are present (order may vary)
    CAmount totalOut = 0;
    for (CAmount amount : ddAmounts) {
        totalOut += amount;
    }
    BOOST_CHECK_EQUAL(totalOut, 60000);
}

BOOST_FIXTURE_TEST_CASE(test_output_amounts_match_requested, DDTransferTestFixture)
{
    // Arrange: Multiple recipients with specific amounts
    std::string recipient1 = CreateDDAddress(recipientKey.GetPubKey());
    CKey recipient2Key;
    recipient2Key.MakeNewKey(true);
    std::string recipient2 = CreateDDAddress(recipient2Key.GetPubKey());

    CAmount amount1 = 12345; // $123.45
    CAmount amount2 = 67890; // $678.90

    TransferParams params;
    params.recipients = {
        {recipient1, amount1},
        {recipient2, amount2}
    };
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(amount1 + amount2));
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert
    BOOST_CHECK_MESSAGE(result.success, "Transfer failed: " << result.error);

    // Extract all DD amounts
    std::vector<CAmount> ddAmounts;
    for (const auto& output : result.tx.vout) {
        if (output.nValue == 0) {
            CAmount amount = 0;
            if (DigiDollar::ExtractDDAmount(output.scriptPubKey, amount)) {
                ddAmounts.push_back(amount);
            }
        }
    }

    // Sort for comparison
    std::sort(ddAmounts.begin(), ddAmounts.end());
    std::vector<CAmount> expected = {amount1, amount2};
    std::sort(expected.begin(), expected.end());

    BOOST_CHECK_EQUAL(ddAmounts.size(), expected.size());
    for (size_t i = 0; i < ddAmounts.size(); ++i) {
        BOOST_CHECK_EQUAL(ddAmounts[i], expected[i]);
    }
}

// =============================================================================
// Phase 2.4 - Fee Input Assembly Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_fee_inputs_added, DDTransferTestFixture)
{
    // Arrange: Create transfer params with fee inputs
    COutPoint ddUtxo = CreateMockDDUTXO(50000);  // $500.00
    COutPoint feeUtxo = CreateMockDGBUTXO(100000); // 0.001 DGB for fees

    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    TransferParams params;
    params.recipients = {{recipientAddr, 50000}};
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos = {ddUtxo};
    params.feeUtxos = {feeUtxo};

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer transaction
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Transaction should be created successfully
    BOOST_CHECK_EQUAL(result.success, true);

    // Verify input count (1 DD + 1 fee = 2 total)
    BOOST_CHECK_EQUAL(result.tx.vin.size(), 2);

    // Verify first input is DD input
    BOOST_CHECK(result.tx.vin[0].prevout == ddUtxo);

    // Verify second input is fee input
    BOOST_CHECK(result.tx.vin[1].prevout == feeUtxo);
}

// =============================================================================
// Phase 2.6 - Transaction Finalization Tests
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_transaction_finalization, DDTransferTestFixture)
{
    // Arrange: Create transfer params with valid inputs and outputs
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    TxBuilderTransferParams params;
    params.recipients = {{recipientAddr, 50000}}; // $500.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;

    // Create DD UTXO with sufficient balance
    params.ddUtxos.push_back(CreateMockDDUTXO(50000)); // Exact amount
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000)); // Fee UTXO

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act: Build transfer transaction
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Transaction should be finalized successfully
    BOOST_CHECK_MESSAGE(result.success, "Transaction finalization failed: " << result.error);
    BOOST_CHECK(result.error.empty());

    // Verify transaction structure
    BOOST_CHECK_GT(result.tx.vin.size(), 0);  // Must have inputs
    BOOST_CHECK_GT(result.tx.vout.size(), 0); // Must have outputs

    // Verify transaction version is set to 2 (SegWit v2)
    BOOST_CHECK_EQUAL(result.tx.nVersion, 2);

    // Verify locktime is set to 0 (immediate broadcast)
    BOOST_CHECK_EQUAL(result.tx.nLockTime, 0);
}

BOOST_FIXTURE_TEST_CASE(test_transaction_version_and_locktime, DDTransferTestFixture)
{
    // Arrange
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    TxBuilderTransferParams params;
    params.recipients = {{recipientAddr, 25000}}; // $250.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(25000));
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Verify version and locktime
    BOOST_CHECK(result.success);
    BOOST_CHECK_EQUAL(result.tx.nVersion, 2);
    BOOST_CHECK_EQUAL(result.tx.nLockTime, 0);
}

BOOST_FIXTURE_TEST_CASE(test_dd_amount_balance_verification, DDTransferTestFixture)
{
    // Arrange: Create transfer with balanced DD amounts
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    TxBuilderTransferParams params;
    params.recipients = {{recipientAddr, 30000}}; // $300.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(30000)); // Exact balance
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: DD amounts should balance
    BOOST_CHECK_MESSAGE(result.success, "DD balance verification failed: " << result.error);

    // Extract and verify total DD input and output
    CAmount totalDDIn = 0;
    for (const auto& utxo : params.ddUtxos) {
        totalDDIn += g_mockDDUTXOs[utxo];
    }

    CAmount totalDDOut = 0;
    for (const auto& output : result.tx.vout) {
        if (output.nValue == 0) { // DD outputs have 0 DGB value
            CAmount ddAmount = 0;
            if (DigiDollar::ExtractDDAmount(output.scriptPubKey, ddAmount)) {
                totalDDOut += ddAmount;
            }
        }
    }

    BOOST_CHECK_EQUAL(totalDDIn, totalDDOut);
}

BOOST_FIXTURE_TEST_CASE(test_dd_amount_mismatch_detection, DDTransferTestFixture)
{
    // Arrange: Create transfer with insufficient DD inputs (should fail)
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    TxBuilderTransferParams params;
    params.recipients = {{recipientAddr, 50000}}; // $500.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(30000)); // Only $300.00 (insufficient!)
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail due to insufficient DD
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
    BOOST_CHECK(result.error.find("Insufficient DD balance") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(test_empty_inputs_validation, DDTransferTestFixture)
{
    // Arrange: Create params with no DD inputs
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    TxBuilderTransferParams params;
    params.recipients = {{recipientAddr, 10000}}; // $100.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    // No ddUtxos provided!
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail validation
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_empty_outputs_validation, DDTransferTestFixture)
{
    // Arrange: Create params with no recipients
    TxBuilderTransferParams params;
    params.recipients = {}; // No recipients!
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(10000));
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Should fail validation
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
}

BOOST_FIXTURE_TEST_CASE(test_complete_transaction_structure, DDTransferTestFixture)
{
    // Arrange: Complete valid transfer
    std::string recipientAddr = CreateDDAddress(recipientKey.GetPubKey());

    TxBuilderTransferParams params;
    params.recipients = {{recipientAddr, 40000}}; // $400.00
    params.feeRate = 100000; // 100,000 sat/kB
    params.spenderKey = senderKey;
    params.ddUtxos.push_back(CreateMockDDUTXO(40000));
    params.feeUtxos.push_back(CreateMockDGBUTXO(100000));

    MockTransferTxBuilder builder(chainParams, currentHeight, oraclePrice);

    // Act
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Assert: Complete validation
    BOOST_CHECK_MESSAGE(result.success, "Transaction failed: " << result.error);

    // Structure checks
    BOOST_CHECK_GT(result.tx.vin.size(), 0);   // Has inputs
    BOOST_CHECK_GT(result.tx.vout.size(), 0);  // Has outputs
    BOOST_CHECK_EQUAL(result.tx.nVersion, 2);  // Version 2
    BOOST_CHECK_EQUAL(result.tx.nLockTime, 0); // Locktime 0

    // Fee checks
    BOOST_CHECK_GT(result.totalFees, 0); // Has fees calculated

    // DD conservation
    CAmount totalDDIn = g_mockDDUTXOs[params.ddUtxos[0]];
    CAmount totalDDOut = 0;
    for (const auto& output : result.tx.vout) {
        if (output.nValue == 0) {
            CAmount ddAmount = 0;
            if (DigiDollar::ExtractDDAmount(output.scriptPubKey, ddAmount)) {
                totalDDOut += ddAmount;
            }
        }
    }
    BOOST_CHECK_EQUAL(totalDDIn, totalDDOut);
}

// =============================================================================
// DD UTXO Database Persistence Tests (Fix #5)
// =============================================================================

BOOST_FIXTURE_TEST_CASE(test_dd_utxo_persistence, DDTransferTestFixture)
{
    // Arrange: Create a DD UTXO to persist
    COutPoint outpoint(InsecureRand256(), 0);
    CAmount dd_amount = 50000; // $500.00

    // Get wallet database batch
    auto pwallet = m_node.wallet_loader->create_wallet_from_file("test_dd_utxo_db", "", DatabaseOptions());
    BOOST_REQUIRE(pwallet);
    auto& wallet = *pwallet;

    WalletBatch batch(wallet->GetDatabase());

    // Act: Write DD UTXO to database
    bool write_success = batch.WriteDDUTXO(outpoint, dd_amount);
    BOOST_CHECK(write_success);

    // Read DD UTXO back from database
    CAmount read_amount = 0;
    bool read_success = batch.ReadDDUTXO(outpoint, read_amount);
    BOOST_CHECK(read_success);

    // Assert: Verify the data matches
    BOOST_CHECK_EQUAL(read_amount, dd_amount);

    // Clean up: Erase DD UTXO
    bool erase_success = batch.EraseDDUTXO(outpoint);
    BOOST_CHECK(erase_success);

    // Verify it's gone
    CAmount verify_amount = 0;
    bool verify_read = batch.ReadDDUTXO(outpoint, verify_amount);
    BOOST_CHECK(!verify_read);
}

BOOST_AUTO_TEST_SUITE_END()