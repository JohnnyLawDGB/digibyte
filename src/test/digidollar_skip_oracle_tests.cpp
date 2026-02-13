// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * T1-05a: Tests for skipOracleValidation bypass vulnerability
 *
 * BUG: ConnectBlock unconditionally sets skipOracleValidation=true, which
 * disables collateral ratio checking for ALL block connections — not just IBD.
 * A malicious miner could include a mint tx with minimal collateral for $100 DD.
 *
 * These tests verify:
 * 1. The vulnerability exists (mint with insufficient collateral passes when skipOracle=true)
 * 2. The fix correctly rejects such transactions when skipOracle=false (non-IBD)
 * 3. IBD blocks still pass with skipOracle=true (no regression)
 */

#include <consensus/amount.h>
#include <consensus/digidollar.h>
#include <consensus/volatility.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <digidollar/digidollar.h>
#include <key.h>
#include <pubkey.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(digidollar_skip_oracle_tests)

/**
 * Helper: Build a mint transaction with specified collateral amount.
 * The DD amount is always $100 (10000 cents), lock period 30 days.
 * collateralAmount MUST be > 0 (otherwise the output structure changes
 * and the collateral output gets misidentified as a DD token output).
 */
static CMutableTransaction BuildMintTx(const XOnlyPubKey& ownerKey, CAmount collateralAmount, int currentHeight)
{
    assert(collateralAmount > 0);

    CMutableTransaction mtx;
    mtx.nVersion = 0x01000770; // DD_TX_MINT

    // Input (simplified)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);

    CAmount ddAmount = 10000; // $100.00
    int64_t lockBlocks = 30 * 24 * 60 * 4; // 30 days in blocks
    int64_t lockHeight = currentHeight + lockBlocks;

    // Create proper collateral script with NUMS internal key
    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = lockHeight;
    params.ownerKey = ownerKey;
    params.internalKey = DigiDollar::GetCollateralNUMSKey();
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);

    // OP_RETURN with DD metadata (matching existing test patterns)
    CScript opReturn = CScript() << OP_RETURN
                                 << std::vector<unsigned char>{'D', 'D'}
                                 << CScriptNum(1) // DD_TX_MINT
                                 << CScriptNum(ddAmount)
                                 << CScriptNum(static_cast<int64_t>(lockHeight))
                                 << CScriptNum(1) // lockTier 1 = 30 days
                                 << std::vector<unsigned char>(ownerKey.begin(), ownerKey.end());

    // DD token output
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(ownerKey, ddAmount);

    // Output order: [0] OP_RETURN, [1] collateral (value > 0), [2] DD token (value = 0)
    mtx.vout.resize(3);
    mtx.vout[0] = CTxOut(0, opReturn);
    mtx.vout[1] = CTxOut(collateralAmount, collateralScript);
    mtx.vout[2] = CTxOut(0, ddScript);

    return mtx;
}

// =============================================================================
// T1-05a: skipOracleValidation bypass allows insufficient collateral
// =============================================================================

BOOST_FIXTURE_TEST_CASE(skip_oracle_allows_insufficient_collateral_exploit, BasicTestingSetup)
{
    // ATTACK SCENARIO: Malicious miner includes a mint tx with 1000 satoshis
    // (dust+) collateral for $100 DD. At $0.50/DGB with 500% ratio, the
    // real requirement is ~10 DGB (1,000,000,000 sats).
    //
    // With skipOracleValidation=true (the current bug in ConnectBlock),
    // collateral ratio validation is skipped entirely, so this passes.

    CKey testKey;
    testKey.MakeNewKey(true);
    XOnlyPubKey ownerKey(testKey.GetPubKey());

    // Clear volatility state
    DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();

    int currentHeight = 1000;
    CAmount oraclePrice = 500000; // $0.50/DGB in micro-USD

    // 1000 satoshis collateral for $100 DD — massively undercollateralized
    // Real requirement at 500% ratio: ~10 DGB = 1,000,000,000 sats
    CAmount trivialCollateral = 1000; // 0.00001 DGB
    CMutableTransaction mtx = BuildMintTx(ownerKey, trivialCollateral, currentHeight);
    CTransaction tx(mtx);

    // With skipOracleValidation=true (the bug): collateral check is SKIPPED
    // This passes because no economic validation occurs
    {
        DigiDollar::ValidationContext ctx(currentHeight, oraclePrice, 150, Params(),
                                          nullptr, true /* skipOracleValidation */);
        TxValidationState state;
        bool result = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
        BOOST_TEST_MESSAGE("skipOracle=true, trivial collateral result: " +
                          std::to_string(result) + " reason: " + state.GetRejectReason());
        // This documents the bug: with skipOracle=true, insufficient collateral passes
        // After the fix, this STILL passes in IBD mode (skipOracle=true) because
        // historical blocks already passed consensus when they were first validated.
        BOOST_CHECK_MESSAGE(result,
            "IBD mode should allow through historical blocks (skipOracle=true)");
    }

    // With skipOracleValidation=false (the fix for non-IBD blocks): this MUST FAIL
    {
        DigiDollar::ValidationContext ctx(currentHeight, oraclePrice, 150, Params(),
                                          nullptr, false /* NOT skipping oracle */);
        TxValidationState state;
        bool result = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
        BOOST_TEST_MESSAGE("skipOracle=false, trivial collateral result: " +
                          std::to_string(result) + " reason: " + state.GetRejectReason());
        BOOST_CHECK_MESSAGE(!result,
            "SECURITY BUG: Mint with 1000 sats collateral for $100 DD accepted in non-IBD!");
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "insufficient-collateral");
    }
}

BOOST_FIXTURE_TEST_CASE(non_ibd_rejects_zero_oracle_price, BasicTestingSetup)
{
    // When NOT in IBD, oracle price must be available and positive.
    // Zero oracle price should cause rejection (fail-closed).

    CKey testKey;
    testKey.MakeNewKey(true);
    XOnlyPubKey ownerKey(testKey.GetPubKey());

    DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();

    int currentHeight = 1000;

    // Use reasonable collateral amount (won't matter — zero price causes early rejection)
    CAmount properCollateral = 100 * COIN; // 100 DGB
    CMutableTransaction mtx = BuildMintTx(ownerKey, properCollateral, currentHeight);
    CTransaction tx(mtx);

    // Non-IBD with oracle price = 0: MUST reject (fail-closed)
    {
        DigiDollar::ValidationContext ctx(currentHeight, 0 /* no oracle price */, 150, Params(),
                                          nullptr, false /* NOT skipping oracle */);
        TxValidationState state;
        bool result = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
        BOOST_TEST_MESSAGE("non-IBD, zero oracle result: " +
                          std::to_string(result) + " reason: " + state.GetRejectReason());
        BOOST_CHECK_MESSAGE(!result,
            "SECURITY BUG: Mint accepted with zero oracle price in non-IBD mode!");
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-oracle-price");
    }

    // IBD with oracle price = 0: allowed (oracle may not be available during sync)
    {
        DigiDollar::ValidationContext ctx(currentHeight, 0, 150, Params(),
                                          nullptr, true /* IBD mode */);
        TxValidationState state;
        bool result = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
        BOOST_TEST_MESSAGE("IBD, zero oracle result: " +
                          std::to_string(result) + " reason: " + state.GetRejectReason());
        BOOST_CHECK(result);
    }
}

BOOST_FIXTURE_TEST_CASE(valid_mint_passes_non_ibd, BasicTestingSetup)
{
    // A properly collateralized mint should pass in both IBD and non-IBD modes.
    // This ensures the fix doesn't break valid transactions.

    CKey testKey;
    testKey.MakeNewKey(true);
    XOnlyPubKey ownerKey(testKey.GetPubKey());

    DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();

    int currentHeight = 1000;
    CAmount oraclePrice = 500000; // $0.50/DGB

    // Calculate required collateral manually to match what validation expects
    // $100 DD, $0.50/DGB, 500% ratio for 30 days
    // Formula: (ddAmount * COIN * ratio * 100) / oraclePrice
    // = (10000 * 100000000 * 500 * 100) / 500000
    // = 100,000,000,000 = 1000 DGB
    // Use 2x that (2000 DGB) to be safe against any rounding/DCA issues
    CAmount veryGenerousCollateral = 200000000000LL; // 2000 DGB — way more than needed

    CMutableTransaction mtx = BuildMintTx(ownerKey, veryGenerousCollateral, currentHeight);
    CTransaction tx(mtx);

    // Non-IBD with proper oracle price and generous collateral: MUST pass
    {
        DigiDollar::ValidationContext ctx(currentHeight, oraclePrice, 150, Params(),
                                          nullptr, false /* NOT skipping oracle */);
        TxValidationState state;
        bool result = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
        BOOST_TEST_MESSAGE("non-IBD, generous collateral (2000 DGB) result: " +
                          std::to_string(result) + " reason: " + state.GetRejectReason());
        BOOST_CHECK_MESSAGE(result,
            "REGRESSION: Valid mint rejected in non-IBD mode! reason: " + state.GetRejectReason());
    }

    // IBD mode: also passes
    {
        DigiDollar::ValidationContext ctx(currentHeight, oraclePrice, 150, Params(),
                                          nullptr, true /* IBD mode */);
        TxValidationState state;
        bool result = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
        BOOST_CHECK(result);
    }
}

BOOST_AUTO_TEST_SUITE_END()
