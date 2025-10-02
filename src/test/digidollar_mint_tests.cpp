// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <digidollar/txbuilder.h>
#include <digidollar/scripts.h>
#include <digidollar/validation.h>
#include <consensus/digidollar.h>
#include <kernel/chainparams.h>
#include <chainparams.h>
#include <key.h>
#include <random.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <base58.h>

using namespace DigiDollar;

BOOST_FIXTURE_TEST_SUITE(digidollar_mint_tests, RegTestingSetup)

// Helper function to create a test key
CKey CreateTestKey() {
    CKey key;
    key.MakeNewKey(true);
    return key;
}

// Helper function to create test UTXOs with specific values
std::vector<COutPoint> CreateTestUTXOsWithValues(const std::vector<CAmount>& values) {
    std::vector<COutPoint> utxos;
    for (size_t i = 0; i < values.size(); ++i) {
        uint256 hash;
        hash.SetHex("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcd" + std::to_string(i).substr(0,2));
        utxos.emplace_back(hash, i);
    }
    return utxos;
}

// Helper function to create test oracle price (DGB/USD in cents)
CAmount CreateTestOraclePrice() {
    return 5000; // $0.05 per DGB = 5 cents per DGB
}

// Helper function to create mock mint transaction builder
class MockMintTxBuilder : public MintTxBuilder {
private:
    std::map<COutPoint, CAmount> m_utxo_values;

public:
    MockMintTxBuilder(const CChainParams& params, int height, CAmount price)
        : MintTxBuilder(params, height, price) {}

    // Override UTXO value lookup for testing
    void SetUTXOValue(const COutPoint& outpoint, CAmount value) {
        m_utxo_values[outpoint] = value;
    }

    // Override GetUTXOValue for testing
    CAmount GetUTXOValue(const COutPoint& outpoint) const {
        auto it = m_utxo_values.find(outpoint);
        return (it != m_utxo_values.end()) ? it->second : 100 * COIN; // Default for testing
    }

    // Override GetUTXOValueVirtual to make SelectCoins work with mocked values
    CAmount GetUTXOValueVirtual(const COutPoint& outpoint) const override {
        return GetUTXOValue(outpoint);
    }

    // Expose public methods for testing
    using MintTxBuilder::CalculateRequiredCollateral;
};

// ============================================================================
// Basic Mint Creation Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(mint_minimum_amount)
{
    // Test minting minimum allowed amount (1 DD = $1.00)
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Set up UTXOs with sufficient value
    auto utxos = CreateTestUTXOsWithValues({100 * COIN, 200 * COIN});
    for (size_t i = 0; i < utxos.size(); ++i) {
        builder.SetUTXOValue(utxos[i], (i + 1) * 100 * COIN);
    }

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 100; // $1.00 in cents
    mintParams.lockDays = 365; // 1 year
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    if (!result.success) {
        std::cout << "ERROR: BuildMintTransaction FAILED: " << result.error << std::endl;
    }
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());
    BOOST_CHECK(result.tx.vin.size() > 0);
    BOOST_CHECK(result.tx.vout.size() >= 2); // Collateral + DD outputs
    BOOST_CHECK(result.collateralRequired > 0);
    BOOST_CHECK(result.totalFees > 0);

    // Check transaction version format
    BOOST_CHECK(result.tx.IsDigiDollar());
    BOOST_CHECK(::GetDigiDollarTxType(CTransaction(result.tx)) == ::DD_TX_MINT);
}

BOOST_AUTO_TEST_CASE(mint_standard_amounts)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Test standard amounts: $100, $1000, $10000
    std::vector<CAmount> amounts = {10000, 100000, 1000000}; // $100, $1000, $10000 in cents

    for (CAmount amount : amounts) {
        auto utxos = CreateTestUTXOsWithValues({1000 * COIN, 2000 * COIN, 3000 * COIN});
        for (size_t i = 0; i < utxos.size(); ++i) {
            builder.SetUTXOValue(utxos[i], (i + 1) * 1000 * COIN);
        }

        TxBuilderMintParams mintParams;
        mintParams.ddAmount = amount;
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);

        BOOST_CHECK_MESSAGE(result.success, "Failed for amount: " + std::to_string(amount));
        BOOST_CHECK(result.collateralRequired > 0);

        // Collateral should scale roughly with DD amount
        CAmount expectedCollateral = builder.CalculateRequiredCollateral(amount, 365);
        BOOST_CHECK_MESSAGE(result.collateralRequired == expectedCollateral,
                          "Collateral mismatch for amount: " + std::to_string(amount));
    }
}

BOOST_AUTO_TEST_CASE(mint_maximum_amount)
{
    // Test minting maximum allowed amount per transaction
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Create large UTXOs for maximum mint
    auto utxos = CreateTestUTXOsWithValues({
        50000 * COIN, 100000 * COIN, 150000 * COIN, 200000 * COIN
    });
    for (size_t i = 0; i < utxos.size(); ++i) {
        builder.SetUTXOValue(utxos[i], (i + 1) * 50000 * COIN);
    }

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000000; // $100,000 in cents (max per transaction)
    mintParams.lockDays = 3650; // 10 years (lowest collateral ratio)
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    BOOST_CHECK(result.collateralRequired > 0);
    // With 10-year lock and $100k mint at 200% ratio and $0.05 DGB:
    // Collateral = $100k * 2.0 / $0.05 = 4,000,000 DGB
    CAmount expected = 4000000 * COIN;
    BOOST_CHECK(std::abs(result.collateralRequired - expected) < 1000 * COIN); // Allow small variance
}

BOOST_AUTO_TEST_CASE(mint_invalid_amounts)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({100 * COIN});
    builder.SetUTXOValue(utxos[0], 100 * COIN);

    // Test zero amount
    {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = 0;
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);
        BOOST_CHECK(!result.success);
        BOOST_CHECK(!result.error.empty());
    }

    // Test negative amount
    {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = -1000;
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);
        BOOST_CHECK(!result.success);
        BOOST_CHECK(!result.error.empty());
    }

    // Test amount below minimum ($100)
    {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = 5000; // $50 in cents
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);
        BOOST_CHECK(!result.success);
        BOOST_CHECK(result.error.find("Invalid mint parameters") != std::string::npos);
    }

    // Test amount above maximum ($100k)
    {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = 15000000; // $150k in cents
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);
        BOOST_CHECK(!result.success);
        BOOST_CHECK(!result.error.empty());
    }
}

// ============================================================================
// Collateral Calculation Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(collateral_all_lock_tiers)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice(); // $0.05 per DGB

    MockMintTxBuilder builder(params, height, price);

    CAmount ddAmount = 10000; // $100 in cents

    // Test all 8 lock tiers with expected collateral ratios
    struct LockTier {
        int days;
        int expectedRatio; // percentage
    };

    std::vector<LockTier> tiers = {
        {30, 500},    // 30 days: 500%
        {90, 400},    // 3 months: 400%
        {180, 350},   // 6 months: 350%
        {365, 300},   // 1 year: 300%
        {1095, 250},  // 3 years: 250%
        {1825, 225},  // 5 years: 225%
        {2555, 212},  // 7 years: 212%
        {3650, 200}   // 10 years: 200%
    };

    for (const auto& tier : tiers) {
        CAmount collateral = builder.CalculateRequiredCollateral(ddAmount, tier.days);

        // Expected: $100 * ratio% / $0.05 per DGB
        CAmount expected = (ddAmount * tier.expectedRatio * COIN) / (100 * price);

        BOOST_CHECK_MESSAGE(std::abs(collateral - expected) < COIN,
                          "Tier " + std::to_string(tier.days) + " days: got " +
                          std::to_string(collateral / COIN) + " DGB, expected " +
                          std::to_string(expected / COIN) + " DGB");
    }
}

BOOST_AUTO_TEST_CASE(collateral_calculation_consistency)
{
    // Test that collateral calculation is consistent across multiple calls
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    CAmount ddAmount = 10000; // $100

    // Multiple calls should return the same result
    CAmount collateral1 = builder.CalculateRequiredCollateral(ddAmount, 365);
    CAmount collateral2 = builder.CalculateRequiredCollateral(ddAmount, 365);
    CAmount collateral3 = builder.CalculateRequiredCollateral(ddAmount, 365);

    BOOST_CHECK_EQUAL(collateral1, collateral2);
    BOOST_CHECK_EQUAL(collateral2, collateral3);
    BOOST_CHECK(collateral1 > 0);
}

BOOST_AUTO_TEST_CASE(collateral_price_dependency)
{
    // Test that collateral requirements change with different oracle prices
    const CChainParams& params = Params();
    int height = 1000;

    CAmount ddAmount = 10000; // $100

    // Test with different prices
    CAmount lowPrice = 2500;   // $0.025 per DGB
    CAmount highPrice = 10000; // $0.10 per DGB

    MockMintTxBuilder builderLow(params, height, lowPrice);
    MockMintTxBuilder builderHigh(params, height, highPrice);

    CAmount collateralLow = builderLow.CalculateRequiredCollateral(ddAmount, 365);
    CAmount collateralHigh = builderHigh.CalculateRequiredCollateral(ddAmount, 365);

    // Higher DGB price should require less DGB collateral for same USD amount
    BOOST_CHECK(collateralLow > collateralHigh);
    BOOST_CHECK(collateralLow > 0);
    BOOST_CHECK(collateralHigh > 0);
}

BOOST_AUTO_TEST_CASE(collateral_insufficient_rejection)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Create UTXOs with insufficient value
    auto utxos = CreateTestUTXOsWithValues({10 * COIN}); // Only 10 DGB
    builder.SetUTXOValue(utxos[0], 10 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100 (needs ~600 DGB collateral)
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(!result.success);
    BOOST_CHECK(result.error.find("Insufficient funds") != std::string::npos);
}

// ============================================================================
// P2TR Output Creation Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(p2tr_script_creation_through_transaction)
{
    // Test P2TR script creation through successful transaction building
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    BOOST_CHECK(result.tx.vout.size() >= 2);

    // First output should be collateral P2TR script (OP_1 + 32 bytes)
    BOOST_CHECK(result.tx.vout[0].scriptPubKey.size() == 34);
    BOOST_CHECK(result.tx.vout[0].scriptPubKey[0] == OP_1);
    BOOST_CHECK(result.tx.vout[0].scriptPubKey[1] == 32);

    // Second output should be DigiDollar P2TR script (OP_1 + 32 bytes)
    BOOST_CHECK(result.tx.vout[1].scriptPubKey.size() == 34);
    BOOST_CHECK(result.tx.vout[1].scriptPubKey[0] == OP_1);
    BOOST_CHECK(result.tx.vout[1].scriptPubKey[1] == 32);
}

BOOST_AUTO_TEST_CASE(p2tr_redemption_paths_verification)
{
    // Test that mint transactions create proper scripts through the full transaction
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    // Verify that scripts are created (specific MAST verification would be in scripts_tests.cpp)
    BOOST_CHECK(!result.tx.vout[0].scriptPubKey.empty());
    BOOST_CHECK(!result.tx.vout[1].scriptPubKey.empty());
}

// ============================================================================
// Transaction Structure Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(transaction_version_field)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);

    // Check DigiDollar version format: 0x4444XXYY
    BOOST_CHECK(result.tx.IsDigiDollar());
    BOOST_CHECK((result.tx.nVersion & 0xFFFF0000) == 0x44440000);

    // Check mint transaction type
    BOOST_CHECK(::GetDigiDollarTxType(CTransaction(result.tx)) == ::DD_TX_MINT);
}

BOOST_AUTO_TEST_CASE(transaction_input_consumption)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Provide multiple UTXOs
    auto utxos = CreateTestUTXOsWithValues({100 * COIN, 200 * COIN, 300 * COIN});
    for (size_t i = 0; i < utxos.size(); ++i) {
        builder.SetUTXOValue(utxos[i], (i + 1) * 100 * COIN);
    }

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    BOOST_CHECK(result.tx.vin.size() > 0);
    BOOST_CHECK(result.tx.vin.size() <= utxos.size());

    // Verify inputs reference provided UTXOs
    for (const auto& input : result.tx.vin) {
        bool found = false;
        for (const auto& utxo : utxos) {
            if (input.prevout == utxo) {
                found = true;
                break;
            }
        }
        BOOST_CHECK(found);
    }
}

BOOST_AUTO_TEST_CASE(transaction_output_creation)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    BOOST_CHECK(result.tx.vout.size() >= 2); // Collateral + DD + possible change

    // First output should be collateral (positive DGB value)
    BOOST_CHECK(result.tx.vout[0].nValue > 0);
    BOOST_CHECK(result.tx.vout[0].nValue == result.collateralRequired);
    const auto& collateralScript = result.tx.vout[0].scriptPubKey;
    BOOST_CHECK(collateralScript.size() == 34 && collateralScript[0] == OP_1 && collateralScript[1] == 32);

    // Second output should be DigiDollar (zero DGB value)
    BOOST_CHECK(result.tx.vout[1].nValue == 0);
    const auto& ddScript = result.tx.vout[1].scriptPubKey;
    BOOST_CHECK(ddScript.size() == 34 && ddScript[0] == OP_1 && ddScript[1] == 32);

    // If change exists, it should be in a third output
    if (result.tx.vout.size() > 2) {
        BOOST_CHECK(result.tx.vout[2].nValue > 0); // Change has positive value
    }
}

BOOST_AUTO_TEST_CASE(transaction_fee_calculation)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    // Test different fee rates
    std::vector<CAmount> feeRates = {1000, 2000, 5000}; // sat/vB

    CAmount prevFee = 0;
    for (CAmount feeRate : feeRates) {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = 10000;
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = feeRate;
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);

        BOOST_CHECK(result.success);
        BOOST_CHECK(result.totalFees > 0);
        BOOST_CHECK(result.totalFees > prevFee); // Higher rate = higher fee

        prevFee = result.totalFees;
    }
}

BOOST_AUTO_TEST_CASE(transaction_signing_preparation)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);

    // Transaction should be ready for signing
    BOOST_CHECK(!result.tx.vin.empty());
    BOOST_CHECK(!result.tx.vout.empty());

    // All inputs should have empty signatures (unsigned)
    for (const auto& input : result.tx.vin) {
        BOOST_CHECK(input.scriptSig.empty());
        BOOST_CHECK(input.scriptWitness.IsNull());
    }
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(edge_case_exact_collateral_no_change)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Calculate exact collateral needed
    CAmount ddAmount = 10000;
    int lockDays = 365;
    CAmount requiredCollateral = builder.CalculateRequiredCollateral(ddAmount, lockDays);
    CAmount estimatedFees = 250 * 1000 / 1000; // Rough fee estimate

    // Provide exact amount needed (collateral + fees)
    auto utxos = CreateTestUTXOsWithValues({requiredCollateral + estimatedFees});
    builder.SetUTXOValue(utxos[0], requiredCollateral + estimatedFees);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = ddAmount;
    mintParams.lockDays = lockDays;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    // Should have exactly 2 outputs (collateral + DD, no change)
    BOOST_CHECK(result.tx.vout.size() == 2);
}

BOOST_AUTO_TEST_CASE(edge_case_multiple_inputs)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Provide many small UTXOs that need to be combined
    std::vector<CAmount> values;
    for (int i = 0; i < 10; ++i) {
        values.push_back(50 * COIN); // 50 DGB each
    }
    auto utxos = CreateTestUTXOsWithValues(values);
    for (size_t i = 0; i < utxos.size(); ++i) {
        builder.SetUTXOValue(utxos[i], values[i]);
    }

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100 (needs ~600 DGB)
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    BOOST_CHECK(result.tx.vin.size() > 1); // Multiple inputs used
    BOOST_CHECK(result.tx.vin.size() <= 10); // Not more than provided
}

BOOST_AUTO_TEST_CASE(edge_case_invalid_lock_times)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    // Test various invalid lock times
    std::vector<int> invalidLockTimes = {0, 10, 29, 3651, 5000}; // 0, 10d, 29d, >10y, way too long

    for (int lockDays : invalidLockTimes) {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = 10000;
        mintParams.lockDays = lockDays;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);

        BOOST_CHECK_MESSAGE(!result.success, "Lock time " + std::to_string(lockDays) + " should be invalid");
        BOOST_CHECK(!result.error.empty());
    }
}

BOOST_AUTO_TEST_CASE(edge_case_oracle_price_unavailable)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = 0; // No oracle price available

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    // Should fail gracefully when oracle price is zero/unavailable
    BOOST_CHECK(!result.success);
    BOOST_CHECK(!result.error.empty());
}

BOOST_AUTO_TEST_CASE(edge_case_extreme_fee_rates)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({10000 * COIN});
    builder.SetUTXOValue(utxos[0], 10000 * COIN);

    // Test extremely high fee rate
    {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = 10000;
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 1000000; // 1M sat/vB (way too high)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);

        BOOST_CHECK(!result.success);
        BOOST_CHECK(!result.error.empty());
    }

    // Test zero fee rate
    {
        TxBuilderMintParams mintParams;
        mintParams.ddAmount = 10000;
        mintParams.lockDays = 365;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 0;
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);

        BOOST_CHECK(!result.success);
        BOOST_CHECK(!result.error.empty());
    }
}

BOOST_AUTO_TEST_CASE(edge_case_invalid_keys)
{
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    // mintParams.ownerKey not set (invalid)
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(!result.success);
    BOOST_CHECK(!result.error.empty());
}

// ============================================================================
// Integration Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(integration_complete_mint_flow)
{
    // Test complete mint transaction flow with realistic parameters
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = 5000; // $0.05 per DGB

    MockMintTxBuilder builder(params, height, price);

    // Set up realistic UTXOs
    auto utxos = CreateTestUTXOsWithValues({
        1000 * COIN,  // 1000 DGB
        2000 * COIN,  // 2000 DGB
        500 * COIN    // 500 DGB
    });
    for (size_t i = 0; i < utxos.size(); ++i) {
        builder.SetUTXOValue(utxos[i], (i == 0 ? 1000 : (i == 1 ? 2000 : 500)) * COIN);
    }

    // Mint $500 worth of DigiDollars with 1-year lock
    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 50000; // $500 in cents
    mintParams.lockDays = 365;   // 1 year = 300% collateral ratio
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 2000;   // 2000 sat/vB
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    BOOST_CHECK(result.error.empty());

    // Verify transaction structure
    BOOST_CHECK(result.tx.IsDigiDollar());
    BOOST_CHECK(::GetDigiDollarTxType(CTransaction(result.tx)) == ::DD_TX_MINT);
    BOOST_CHECK(result.tx.vin.size() > 0);
    BOOST_CHECK(result.tx.vout.size() >= 2);

    // Verify collateral calculation
    // $500 * 300% / $0.05 = 30,000 DGB
    CAmount expectedCollateral = 30000 * COIN;
    BOOST_CHECK(std::abs(result.collateralRequired - expectedCollateral) < 100 * COIN);

    // Verify outputs
    BOOST_CHECK(result.tx.vout[0].nValue == result.collateralRequired); // Collateral
    BOOST_CHECK(result.tx.vout[1].nValue == 0); // DigiDollar output
    const auto& script0 = result.tx.vout[0].scriptPubKey;
    const auto& script1 = result.tx.vout[1].scriptPubKey;
    BOOST_CHECK(script0.size() == 34 && script0[0] == OP_1 && script0[1] == 32);
    BOOST_CHECK(script1.size() == 34 && script1[0] == OP_1 && script1[1] == 32);

    // Verify fees are reasonable
    BOOST_CHECK(result.totalFees > 0);
    BOOST_CHECK(result.totalFees < 1000000); // Less than 0.01 DGB in fees
}

// ============================================================================
// DCA Integration Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(mint_with_dca_healthy_system)
{
    // Test minting with healthy system (no DCA adjustment)
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Create mock system state for healthy system (200% collateralization)
    CAmount totalCollateral = 100000000 * COIN;  // 100M DGB
    CAmount totalDD = 10000000;                  // 10M DD ($100k)
    // Health = (100M * $0.05) / $100k * 100 = $5M / $100k * 100 = 500%

    auto utxos = CreateTestUTXOsWithValues({1000 * COIN});
    builder.SetUTXOValue(utxos[0], 1000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100
    mintParams.lockDays = 365;   // 1 year (300% base ratio)
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);

    // With healthy system, DCA multiplier should be 1.0x (no adjustment)
    // Expected: $100 * 300% / $0.05 = 60,000 DGB (no DCA adjustment)
    CAmount expectedBaseCollateral = 60000 * COIN;
    BOOST_CHECK(std::abs(result.collateralRequired - expectedBaseCollateral) < 1000 * COIN);
}

BOOST_AUTO_TEST_CASE(mint_with_dca_warning_system)
{
    // Test minting with warning system (1.2x DCA multiplier)
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Create mock system state for warning system (130% collateralization)
    // This should trigger 1.2x DCA multiplier

    auto utxos = CreateTestUTXOsWithValues({2000 * COIN});
    builder.SetUTXOValue(utxos[0], 2000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100
    mintParams.lockDays = 365;   // 1 year (300% base ratio)
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    // TODO: Mock system health to return 130% for warning tier
    // For now, test basic structure without DCA adjustment

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);

    // With warning system, DCA multiplier should be 1.2x
    // Expected: $100 * 300% * 1.2 / $0.05 = 72,000 DGB
    // For now, test without DCA integration until validation is updated
    CAmount baseCollateral = 60000 * COIN;
    BOOST_CHECK(result.collateralRequired >= baseCollateral);
}

BOOST_AUTO_TEST_CASE(mint_with_dca_critical_system)
{
    // Test minting with critical system (1.5x DCA multiplier)
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Create sufficient UTXOs for higher collateral requirement
    auto utxos = CreateTestUTXOsWithValues({3000 * COIN});
    builder.SetUTXOValue(utxos[0], 3000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100
    mintParams.lockDays = 365;   // 1 year (300% base ratio)
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    // TODO: Mock system health to return 110% for critical tier

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);

    // With critical system, DCA multiplier should be 1.5x
    // Expected: $100 * 300% * 1.5 / $0.05 = 90,000 DGB
    // For now, test basic structure
    CAmount baseCollateral = 60000 * COIN;
    BOOST_CHECK(result.collateralRequired >= baseCollateral);
}

BOOST_AUTO_TEST_CASE(mint_with_dca_emergency_system)
{
    // Test minting with emergency system (2.0x DCA multiplier)
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Create sufficient UTXOs for doubled collateral requirement
    auto utxos = CreateTestUTXOsWithValues({5000 * COIN});
    builder.SetUTXOValue(utxos[0], 5000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100
    mintParams.lockDays = 365;   // 1 year (300% base ratio)
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    // TODO: Mock system health to return 90% for emergency tier

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);

    // With emergency system, DCA multiplier should be 2.0x
    // Expected: $100 * 300% * 2.0 / $0.05 = 120,000 DGB
    // For now, test basic structure
    CAmount baseCollateral = 60000 * COIN;
    BOOST_CHECK(result.collateralRequired >= baseCollateral);
}

BOOST_AUTO_TEST_CASE(mint_dca_applies_to_all_lock_tiers)
{
    // Test that DCA adjustment applies correctly to all lock tiers
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Test DCA with different lock tiers
    struct LockTier {
        int days;
        int baseRatio;
        const char* description;
    };

    std::vector<LockTier> tiers = {
        {30, 500, "30 days"},
        {90, 400, "3 months"},
        {180, 350, "6 months"},
        {365, 300, "1 year"},
        {1095, 250, "3 years"},
        {1825, 225, "5 years"},
        {2555, 212, "7 years"},
        {3650, 200, "10 years"}
    };

    CAmount ddAmount = 10000; // $100

    for (const auto& tier : tiers) {
        // Create sufficient UTXOs (conservative estimate)
        auto utxos = CreateTestUTXOsWithValues({5000 * COIN});
        builder.SetUTXOValue(utxos[0], 5000 * COIN);

        TxBuilderMintParams mintParams;
        mintParams.ddAmount = ddAmount;
        mintParams.lockDays = tier.days;
        mintParams.ownerKey = CreateTestKey();
        mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
        mintParams.utxos = utxos;

        TxBuilderResult result = builder.BuildMintTransaction(mintParams);

        BOOST_CHECK_MESSAGE(result.success,
                          "Failed for tier: " + std::string(tier.description));

        // Calculate expected base collateral
        CAmount expectedBase = (ddAmount * tier.baseRatio * COIN) / (100 * price);

        // For now, verify base collateral is calculated correctly
        // DCA integration will be tested after validation is updated
        BOOST_CHECK_MESSAGE(result.collateralRequired >= expectedBase,
                          "Insufficient collateral for tier: " + std::string(tier.description));
    }
}

BOOST_AUTO_TEST_CASE(mint_insufficient_funds_with_dca)
{
    // Test insufficient funds rejection when DCA increases requirements
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    // Provide just enough for base collateral but not DCA adjustment
    auto utxos = CreateTestUTXOsWithValues({65000 * COIN}); // Just above base requirement
    builder.SetUTXOValue(utxos[0], 65000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000; // $100
    mintParams.lockDays = 365;   // 1 year (300% base = 60k DGB)
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    // TODO: Mock system health to trigger emergency DCA (2.0x = 120k DGB required)

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    // Should succeed with current implementation (DCA not integrated yet)
    // Will fail after DCA integration when requirement becomes 120k DGB
    BOOST_CHECK(result.success); // For now
}

BOOST_AUTO_TEST_CASE(mint_dca_real_time_adjustment)
{
    // Test that DCA adjusts in real-time during minting process
    const CChainParams& params = Params();
    int height = 1000;
    CAmount price = CreateTestOraclePrice();

    MockMintTxBuilder builder(params, height, price);

    auto utxos = CreateTestUTXOsWithValues({5000 * COIN});
    builder.SetUTXOValue(utxos[0], 5000 * COIN);

    TxBuilderMintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockDays = 365;
    mintParams.ownerKey = CreateTestKey();
    mintParams.feeRate = 100000; // 100,000 sat/kB (minimum for DigiByte)
    mintParams.utxos = utxos;

    // TODO: Test that system health is calculated during mint validation
    // TODO: Test that DCA multiplier is applied to collateral requirement
    // TODO: Test that transaction fails if system health changes during building

    TxBuilderResult result = builder.BuildMintTransaction(mintParams);

    BOOST_CHECK(result.success);
    // Additional real-time tests will be added after DCA integration
}

BOOST_AUTO_TEST_SUITE_END()