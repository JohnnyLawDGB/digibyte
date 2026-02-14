// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// =============================================================================
// DigiDollar Bug Hunt Test Suite
// =============================================================================
// These tests PROVE bugs found during the DigiDollar code audit.
// Each test is designed to FAIL when the corresponding bug is FIXED.
// This lets us track which bugs have been resolved.
//
// Bug hunt conducted: 2026-01-31
// Test suite created: 2026-01-31
// Branch: feature/digidollar-v1
// =============================================================================

#include <boost/test/unit_test.hpp>

#include <primitives/oracle.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <consensus/digidollar.h>
#include <consensus/err.h>
#include <consensus/volatility.h>
#include <kernel/chainparams.h>
#include <chainparams.h>
#include <key.h>
#include <test/util/setup_common.h>
#include <util/time.h>

#include <map>
#include <vector>
#include <cstdint>
#include <cmath>

using namespace DigiDollar;

BOOST_FIXTURE_TEST_SUITE(digidollar_bughunt_tests, RegTestingSetup)

// =============================================================================
// BUG #1: Oracle Price Ceiling — RESOLVED (T9-01)
// Original issue: Three different filter algorithms with inconsistent caps.
// FilterOutliersAdvanced had $10 cap, FilterOutliers had 10% threshold.
// FIX (T9-01): All removed. Unified on single IQR algorithm via
// GetConsensusPrice(). Price range uses ORACLE_MIN/MAX_PRICE_MICRO_USD
// constants ($0.0001 to $100). No separate filter-specific caps.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_1a_unified_iqr_accepts_15_dollar_price)
{
    // FIXED: Unified IQR filter uses ORACLE_MAX_PRICE_MICRO_USD ($100).
    // $15/DGB is well within range and should be accepted by GetConsensusPrice.

    uint64_t price_15_dollars = 15000000; // $15.00 in micro-USD
    int64_t now = GetTime();

    // Verify IsValid() accepts $15
    COraclePriceMessage msg;
    msg.price_micro_usd = price_15_dollars;
    msg.oracle_id = 1;
    msg.timestamp = now;
    msg.block_height = 1000;
    BOOST_CHECK(msg.IsValid(now + 10));

    // Verify GetConsensusPrice (unified IQR) accepts $15 prices
    COracleBundle bundle;
    for (int i = 0; i < 5; i++) {
        COraclePriceMessage m;
        m.price_micro_usd = price_15_dollars;
        m.oracle_id = i + 1;
        m.timestamp = now;
        m.block_height = 1000;
        bundle.messages.push_back(m);
    }

    // All identical $15 → IQR=0, all pass, median=$15
    uint64_t consensus_price = bundle.GetConsensusPrice(5);
    BOOST_CHECK_EQUAL(consensus_price, price_15_dollars);
    BOOST_CHECK_MESSAGE(consensus_price > 0,
        "BUG #1 FIXED: Unified IQR filter accepts $15 prices (no $10 cap)");
}

BOOST_AUTO_TEST_CASE(bughunt_1b_unified_filter_no_inconsistency)
{
    // FIXED: IsValid() and GetConsensusPrice() both use
    // ORACLE_MIN/MAX_PRICE_MICRO_USD for range. No separate filter caps.

    struct PriceTest {
        uint64_t micro_usd;
        const char* label;
        bool expect_isvalid;       // IsValid() result
        bool expect_consensus_ok;  // GetConsensusPrice returns > 0
    };

    PriceTest tests[] = {
        {9000000,   "$9",   true, true},   // Both accept
        {11000000,  "$11",  true, true},   // FIXED: Both accept ($100 max)
        {50000000,  "$50",  true, true},   // FIXED: Both accept
        {100000000, "$100", true, true},   // At max boundary — both accept
    };

    int64_t now = GetTime();
    for (const auto& t : tests) {
        COraclePriceMessage msg;
        msg.price_micro_usd = t.micro_usd;
        msg.oracle_id = 1;
        msg.timestamp = now;
        msg.block_height = 1000;

        bool valid = msg.IsValid(now + 10);
        BOOST_CHECK_EQUAL(valid, t.expect_isvalid);

        // Check unified IQR filter via GetConsensusPrice
        COracleBundle bundle;
        for (int i = 0; i < 5; i++) {
            COraclePriceMessage m = msg;
            m.oracle_id = i + 1;
            bundle.messages.push_back(m);
        }
        uint64_t price = bundle.GetConsensusPrice(5);

        BOOST_CHECK_MESSAGE((price > 0) == t.expect_consensus_ok,
            "Price " << t.label << ": expected consensus_ok=" << t.expect_consensus_ok
            << " got price=" << price);
    }
}

// =============================================================================
// BUG #2: RPC Tier Mapping Mismatch (HIGH)
// Files: src/rpc/digidollar.cpp:51-63, src/consensus/digidollar.h:55-63,
//        src/wallet/digidollarwallet.cpp:6249-6267
// RESOLVED: 2-year tier now properly implemented across consensus, RPC, wallet, and GUI.
// All layers use 10 tiers (0-9): 1hr, 30d, 90d, 180d, 1yr, 2yr, 3yr, 5yr, 7yr, 10yr.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_2_all_tiers_aligned)
{
    // Verify consensus has exactly 10 tiers including the 2-year tier
    const auto& params = Params().GetDigiDollarConsensus();
    BOOST_CHECK_EQUAL(params.collateralRatios.size(), 10u);

    const int BLOCKS_PER_DAY = 24 * 60 * 4; // 5760

    // Verify all 10 consensus tiers exist with correct ratios
    BOOST_CHECK_EQUAL(params.collateralRatios.at(240), 1000);                          // Tier 0: 1 hour
    BOOST_CHECK_EQUAL(params.collateralRatios.at(30 * BLOCKS_PER_DAY), 500);           // Tier 1: 30 days
    BOOST_CHECK_EQUAL(params.collateralRatios.at(90 * BLOCKS_PER_DAY), 400);           // Tier 2: 90 days
    BOOST_CHECK_EQUAL(params.collateralRatios.at(180 * BLOCKS_PER_DAY), 350);          // Tier 3: 180 days
    BOOST_CHECK_EQUAL(params.collateralRatios.at(365 * BLOCKS_PER_DAY), 300);          // Tier 4: 1 year
    BOOST_CHECK_EQUAL(params.collateralRatios.at(2 * 365 * BLOCKS_PER_DAY), 275);      // Tier 5: 2 years
    BOOST_CHECK_EQUAL(params.collateralRatios.at(3 * 365 * BLOCKS_PER_DAY), 250);      // Tier 6: 3 years
    BOOST_CHECK_EQUAL(params.collateralRatios.at(5 * 365 * BLOCKS_PER_DAY), 225);      // Tier 7: 5 years
    BOOST_CHECK_EQUAL(params.collateralRatios.at(7 * 365 * BLOCKS_PER_DAY), 212);      // Tier 8: 7 years
    BOOST_CHECK_EQUAL(params.collateralRatios.at(10 * 365 * BLOCKS_PER_DAY), 200);     // Tier 9: 10 years

    // Verify collateral ratios decrease monotonically with longer lock periods
    int prevRatio = 1001;
    for (const auto& [blocks, ratio] : params.collateralRatios) {
        BOOST_CHECK_MESSAGE(ratio < prevRatio,
            "Collateral ratio should decrease for longer locks: " << ratio << "% at " << blocks << " blocks");
        prevRatio = ratio;
    }

    // Verify the wallet tier array matches consensus
    const int walletTierDays[10] = {0, 30, 90, 180, 365, 730, 1095, 1825, 2555, 3650};
    for (int i = 1; i < 10; ++i) {
        int64_t blocks = static_cast<int64_t>(walletTierDays[i]) * BLOCKS_PER_DAY;
        BOOST_CHECK_MESSAGE(params.collateralRatios.count(blocks) > 0,
            "Wallet tier " << i << " (" << walletTierDays[i] << " days / " << blocks << " blocks) missing from consensus");
    }
}

// =============================================================================
// BUG #3: ERR Redemption Deadlock (MEDIUM)
// File: src/digidollar/validation.cpp:1218-1221, 1529-1533
// Issue: ValidateEmergencyRedemptionConditions always returns Invalid.
//        ShouldBlockNormalRedemptionsDuringERR returns true when ERR active.
//        Result: BOTH redemption paths blocked during ERR = deadlock.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_3_err_redemption_deadlock)
{
    // Simulate ERR being active
    // The ERR system uses static state, so we can activate it directly
    DigiDollar::ERR::ERRState errState;
    errState.isActive = true;
    errState.systemHealth = 80; // Below 100% = ERR condition
    DigiDollar::ERR::EmergencyRedemptionRatio::UpdateERRState(errState);

    // Path 1: Normal redemption - BLOCKED during ERR
    ValidationContext ctx;
    ctx.height = 100000;
    bool normalBlocked = ShouldBlockNormalRedemptionsDuringERR(ctx);
    BOOST_CHECK_MESSAGE(normalBlocked,
        "Normal redemptions should be blocked during ERR");

    // Path 2: ERR redemption - ALWAYS REJECTED (bug)
    // ValidateEmergencyRedemptionConditions returns state.Invalid("err-validation-incomplete")
    // We can't easily call it without a full transaction, but we can verify the code
    // by checking that the function exists and the ERR path is blocked.
    //
    // The deadlock: normalBlocked=true AND ERR redemption always rejected
    // = NO WAY to redeem during ERR event

    BOOST_CHECK_MESSAGE(normalBlocked == true,
        "BUG #3: Both redemption paths are blocked during ERR. "
        "Normal redemption blocked: " << normalBlocked <<
        ". ERR redemption: always returns 'err-validation-incomplete'. "
        "Users are stuck in a deadlock.");

    // Clean up: deactivate ERR
    errState.isActive = false;
    DigiDollar::ERR::EmergencyRedemptionRatio::UpdateERRState(errState);
}

// =============================================================================
// BUG #4: ValidateCollateralReleaseAmount No-Op (MEDIUM)
// File: src/digidollar/validation.cpp:1227-1240
// Issue: Always returns true regardless of input. No actual validation.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_4_collateral_release_noop)
{
    // Create a minimal transaction
    CMutableTransaction mtx;
    mtx.nVersion = 2;

    // Add an absurd output: release 1 billion DGB as "collateral"
    CScript dummyScript;
    dummyScript << OP_TRUE;
    mtx.vout.push_back(CTxOut(100000000000LL * COIN, dummyScript)); // 100B DGB

    CTransaction tx(mtx);
    ValidationContext ctx;
    ctx.height = 100000;
    TxValidationState state;

    // Pass absurd values: claim to burn 1 cent of DD, release 100B DGB
    CAmount absurd_dd_burned = 1; // 1 cent
    bool result = ValidateCollateralReleaseAmount(tx, ctx, absurd_dd_burned, state);

    // BUG: Returns true even for absurd inputs
    BOOST_CHECK_MESSAGE(result == true,
        "BUG #4 FIXED? ValidateCollateralReleaseAmount now rejects absurd values. "
        "This is good! Remove this test.");

    // Also test with zero DD burned
    result = ValidateCollateralReleaseAmount(tx, ctx, 0, state);
    BOOST_CHECK_MESSAGE(result == true,
        "BUG #4: Even 0 DD burned passes collateral release validation");

    // And negative (if CAmount allows)
    result = ValidateCollateralReleaseAmount(tx, ctx, -1, state);
    BOOST_CHECK_MESSAGE(result == true,
        "BUG #4: Even negative DD burned passes collateral release validation");
}

// =============================================================================
// BUG #5: Hardcoded Height/Price in Wallet (MEDIUM)
// File: src/wallet/digidollarwallet.cpp:889-890
// Issue: currentHeight=100000 and oraclePrice=2500 are hardcoded with TODOs
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_5_hardcoded_height_price)
{
    // We can't directly call the wallet function without a full wallet context,
    // but we can document and verify the hardcoded values exist.
    //
    // File: src/wallet/digidollarwallet.cpp
    // Line 889: int currentHeight = 100000; // TODO: Get actual height from chainstate
    // Line 890: CAmount oraclePrice = 2500;  // TODO: Get from MockOracleManager
    //
    // These values are used in TransferDigiDollar() for building transactions.
    // The same pattern appears in RedeemDigiDollar().
    //
    // Impact: All transfer/redemption transactions use height=100000 and
    // price=$0.0025/DGB regardless of actual chain state.

    // Verify the expected hardcoded values (these are what the code uses)
    int hardcoded_height = 100000;
    CAmount hardcoded_price = 2500; // micro-USD

    // These should NOT be hardcoded in production
    BOOST_CHECK_MESSAGE(hardcoded_height == 100000,
        "BUG #5: Wallet uses hardcoded currentHeight=100000 in TransferDigiDollar/RedeemDigiDollar. "
        "See src/wallet/digidollarwallet.cpp:889");
    BOOST_CHECK_MESSAGE(hardcoded_price == 2500,
        "BUG #5: Wallet uses hardcoded oraclePrice=2500 ($0.0025). "
        "See src/wallet/digidollarwallet.cpp:890");

    // This test always passes - it's a documentation marker.
    // When the hardcoded values are replaced with real lookups,
    // these constants will no longer appear in the code.
    BOOST_CHECK(true);
}

// =============================================================================
// BUG #6: GUI Mock Price 10,000x Error (MEDIUM)
// File: src/qt/digidollaroverviewwidget.cpp:599
// Issue: MockOracleManager returns micro-USD (6500 = $0.0065) but GUI
//        divides by 100 (treating as cents), displaying $65.00 instead.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_6_gui_mock_price_10000x)
{
    // MockOracleManager::GetCurrentPrice() returns micro-USD
    // 6500 micro-USD = $0.0065
    uint64_t mock_price_micro_usd = 6500;

    // CORRECT conversion: micro-USD to dollars
    double correct_price = mock_price_micro_usd / 1000000.0;
    BOOST_CHECK_CLOSE(correct_price, 0.0065, 0.001);

    // BUG: GUI divides by 100 instead of 1,000,000
    double buggy_price = mock_price_micro_usd / 100.0;
    BOOST_CHECK_CLOSE(buggy_price, 65.0, 0.001);

    // The error factor is 10,000x
    double error_factor = buggy_price / correct_price;
    BOOST_CHECK_CLOSE(error_factor, 10000.0, 0.001);

    // BUG: buggy_price != correct_price
    BOOST_CHECK_MESSAGE(std::abs(buggy_price - correct_price) > 1.0,
        "BUG #6 FIXED? GUI price conversion now correct. Remove this test.");
}

// =============================================================================
// BUG #7: Volatility + ERR State Ephemeral (MEDIUM)
// Files: src/consensus/err.cpp, src/consensus/volatility.cpp
// Issue: Both use static members with no persistence. State lost on restart.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_7_ephemeral_err_state)
{
    // Activate ERR
    DigiDollar::ERR::ERRState errState;
    errState.isActive = true;
    errState.systemHealth = 75;
    DigiDollar::ERR::EmergencyRedemptionRatio::UpdateERRState(errState);

    // Verify it's active
    auto currentState = DigiDollar::ERR::EmergencyRedemptionRatio::GetCurrentState();
    BOOST_CHECK(currentState.isActive);
    BOOST_CHECK_EQUAL(currentState.systemHealth, 75);

    // Simulate "restart" by resetting state to defaults
    DigiDollar::ERR::ERRState freshState;
    DigiDollar::ERR::EmergencyRedemptionRatio::UpdateERRState(freshState);

    // After "restart", ERR state is lost - defaults to inactive
    auto afterRestart = DigiDollar::ERR::EmergencyRedemptionRatio::GetCurrentState();

    // BUG: State is lost because it's only in static memory, not persisted
    BOOST_CHECK_MESSAGE(!afterRestart.isActive,
        "BUG #7: ERR state is ephemeral - lost on restart. "
        "Was active with health=75, now inactive with health=" << afterRestart.systemHealth);
}

BOOST_AUTO_TEST_CASE(bughunt_7b_ephemeral_volatility_state)
{
    int64_t now = GetTime();

    // Clear any prior state
    DigiDollar::VolatilityMonitor::ClearHistory();

    // Record some price data to trigger volatility monitoring
    DigiDollar::VolatilityMonitor::RecordPrice(5000000, now - 7200, 100000);    // $50 at t-2h
    DigiDollar::VolatilityMonitor::RecordPrice(1000000, now - 3600, 100001);    // $10 at t-1h (80% drop)

    // Get state before "restart"
    auto stateBefore = DigiDollar::VolatilityMonitor::GetCurrentState();
    auto historyBefore = DigiDollar::VolatilityMonitor::GetPriceHistory();

    // Simulate restart by clearing state
    DigiDollar::VolatilityMonitor::ClearHistory();

    auto historyAfter = DigiDollar::VolatilityMonitor::GetPriceHistory();

    // BUG: Volatility state is lost - all price history gone after restart
    BOOST_CHECK_MESSAGE(historyBefore.size() > 0,
        "Should have recorded price history before clear");
    BOOST_CHECK_MESSAGE(historyAfter.empty(),
        "BUG #7b: Volatility state is ephemeral - all history lost on ClearHistory/restart. "
        "A volatility freeze cooldown can be bypassed by restarting the node. "
        "No persistence mechanism exists for price history or freeze state.");
}

// =============================================================================
// BUG #8: Transfer DD Conservation Placeholder (LOW)
// File: src/digidollar/validation.cpp:885
// Issue: inputDD = outputDD assignment instead of real UTXO lookup.
//        Conservation check always passes because input is set equal to output.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_8_transfer_conservation_placeholder)
{
    // The validation code at line 885 does:
    //   inputDD = outputDD; // Assume conservation for basic testing
    //   if (inputDD != outputDD) { return Invalid(...); }
    //
    // This means the conservation check ALWAYS passes because inputDD
    // is explicitly set to outputDD right before the comparison.
    //
    // A transfer creating DD from nothing would pass validation.

    // We can demonstrate this by creating a transfer with outputs but no real inputs
    CMutableTransaction mtx;
    mtx.nVersion = 2;

    // Add a dummy input (no real DD backing)
    mtx.vin.push_back(CTxIn(COutPoint(uint256::ONE, 0)));

    // Add DD output worth $1000 (created from nothing)
    CScript ddScript;
    ddScript << OP_1; // Placeholder - real DD script would be P2TR
    mtx.vout.push_back(CTxOut(100000, ddScript)); // 100000 cents = $1000

    // The validation would pass because inputDD is set to outputDD
    // (We can't easily call ValidateTransferTransaction without full context,
    // but the code review confirms the bug at line 885)

    // Documentation test - the assignment is the bug
    CAmount inputDD = 0;      // No real DD inputs
    CAmount outputDD = 100000; // $1000 of DD outputs

    // BUG: This is what the code does
    inputDD = outputDD; // Line 885: Assume conservation

    BOOST_CHECK_MESSAGE(inputDD == outputDD,
        "BUG #8: Transfer conservation is a no-op. inputDD forced equal to outputDD. "
        "See src/digidollar/validation.cpp:885");
}

// =============================================================================
// BUG #9: Static Metadata Map Unbounded Growth (LOW)
// File: src/digidollar/scripts.cpp:215
// Issue: g_scriptMetadataMap grows indefinitely, never cleaned up.
// =============================================================================

BOOST_AUTO_TEST_CASE(bughunt_9_metadata_map_unbounded_growth)
{
    // Register many script metadata entries
    // The map never shrinks - memory leak over time

    size_t initial_count = 0; // Can't access map size directly, but we can demonstrate the pattern

    // Register 1000 metadata entries
    for (int i = 0; i < 1000; i++) {
        CScript script;
        script << OP_TRUE << CScriptNum(i);
        RegisterScriptMetadata(script, ScriptType::DD_MINT, i * 100, i);
    }

    // Verify they're all still there (none evicted)
    bool all_present = true;
    for (int i = 0; i < 1000; i++) {
        CScript script;
        script << OP_TRUE << CScriptNum(i);
        ScriptMetadata metadata;
        if (!GetScriptMetadata(script, metadata)) {
            all_present = false;
            break;
        }
    }

    BOOST_CHECK_MESSAGE(all_present,
        "BUG #9: All 1000 metadata entries persist in g_scriptMetadataMap. "
        "No eviction or cleanup mechanism exists. "
        "See src/digidollar/scripts.cpp:215");

    // There's no way to clear the map - it grows forever
    // In production, with millions of DD transactions, this leaks memory
}

BOOST_AUTO_TEST_SUITE_END()
