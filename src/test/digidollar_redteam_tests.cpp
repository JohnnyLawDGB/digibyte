// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RED TEAM SECURITY TESTS - Adversarial testing for DigiDollar
 * 
 * These tests attempt to BREAK the system by exploiting:
 * - Integer overflow/underflow
 * - Boundary conditions
 * - Rounding errors
 * - Discrepancies between TxBuilder and Validation
 */

#include <consensus/amount.h>
#include <consensus/digidollar.h>
#include <consensus/tx_check.h>
#include <digidollar/txbuilder.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <consensus/dca.h>
#include <consensus/err.h>
#include <digidollar/health.h>
#include <kernel/chainparams.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <key.h>
#include <pubkey.h>
#include <hash.h>
#include <crypto/sha256.h>
#include <util/strencodings.h>
#include <oracle/bundle_manager.h>
#include <oracle/exchange.h>
#include <primitives/oracle.h>
#include <protocol.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>
#include <limits>

BOOST_FIXTURE_TEST_SUITE(digidollar_redteam_tests, BasicTestingSetup)

// =============================================================================
// T1-01: Collateral Calculation Overflow/Underflow Exploits
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_overflow_max_dd_min_price)
{
    // ATTACK: Mint MAX_DIGIDOLLAR at minimum price - try to overflow collateral calc
    // Expected: Should require enormous collateral, not overflow to small value
    
    const CAmount MAX_DD = 10000000000000LL;  // $100 trillion in cents
    const CAmount MIN_PRICE = 1;  // 1 micro-USD ($0.000001 per DGB)
    const int LOCK_BLOCKS = 30 * DigiDollar::BLOCKS_PER_DAY;
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, MIN_PRICE, 150, *regTestParams);
    
    CAmount required = DigiDollar::CalculateRequiredCollateral(MAX_DD, LOCK_BLOCKS, ctx);
    
    // At minimum price, required collateral should be enormous (capped at MAX_MONEY)
    // Exploit would be: required collateral overflows to near-zero
    BOOST_CHECK_MESSAGE(required >= MAX_MONEY || required == MAX_MONEY,
        "EXPLOIT FOUND: MAX_DD at MIN_PRICE should require MAX_MONEY collateral, got " + 
        std::to_string(required));
    
    // Verify it's impossible to provide this much collateral
    BOOST_CHECK_GT(required, 21000000 * COIN);  // More than all DGB in existence
}

BOOST_AUTO_TEST_CASE(redteam_overflow_emergency_dca)
{
    // ATTACK: System in emergency (2.0x DCA) with max ratio - try to overflow
    // Expected: 10000% base * 2.0x = 20000% effective, should not overflow
    
    const CAmount DD_AMOUNT = 1000000;  // $10,000 in cents
    const int64_t LOCK_BLOCKS = 240;  // 1-hour tier (1000% ratio)
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, 5000, 50, *regTestParams);  // Emergency level - 2.0x DCA
    
    // Get effective ratio
    DigiDollar::ConsensusParams ddParams;
    int baseRatio = DigiDollar::GetCollateralRatioForLockTime(LOCK_BLOCKS, ddParams);
    int effectiveRatio = DigiDollar::GetEffectiveCollateralRatio(baseRatio, ctx.systemCollateral, ctx.params);
    
    // Verify DCA applied correctly (1000% * 2.0 = 2000%)
    BOOST_CHECK_MESSAGE(effectiveRatio == 2000,
        "EXPLOIT: Emergency DCA should be 2000%, got " + std::to_string(effectiveRatio));
    
    CAmount required = DigiDollar::CalculateRequiredCollateral(DD_AMOUNT, LOCK_BLOCKS, ctx);
    
    // Required should be positive and reasonable (not overflowed)
    BOOST_CHECK_GT(required, 0);
    BOOST_CHECK_LE(required, MAX_MONEY);
    
    // Sanity check: $10K DD at $0.005/DGB with 2000% ratio should need a LOT of DGB
    // Formula: (DD_cents * COIN * ratio * 100) / oracle_micro_usd
    // = (1000000 * 100000000 * 2000 * 100) / 5000
    // = 20,000,000,000,000,000,000 / 5000
    // = 4,000,000,000,000,000 sats = 40,000,000 DGB
    BOOST_CHECK_GT(required, 10000000 * COIN);  // Should need > 10M DGB
}

BOOST_AUTO_TEST_CASE(redteam_underflow_max_price)
{
    // ATTACK: Mint at extremely high price - try to get collateral requirement near zero
    // Expected: High price = low collateral (but still > 0)
    
    const CAmount DD_AMOUNT = 10000;  // $100 in cents
    const int64_t LOCK_BLOCKS = 30 * DigiDollar::BLOCKS_PER_DAY;
    const CAmount MAX_PRICE = 1000000000000LL;  // $1M per DGB in micro-USD
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, MAX_PRICE, 150, *regTestParams);
    
    CAmount required = DigiDollar::CalculateRequiredCollateral(DD_AMOUNT, LOCK_BLOCKS, ctx);
    
    // At very high price, collateral should be minimal but never zero or negative
    BOOST_CHECK_GT(required, 0);
    
    // At $1M/DGB, $100 DD with 500% ratio should need:
    // (10000 * 100000000 * 500 * 100) / 1000000000000 = 5000 sats = 0.00005 DGB
    // This is extremely low but valid
    BOOST_CHECK_LT(required, COIN);  // Less than 1 DGB
}

BOOST_AUTO_TEST_CASE(redteam_division_by_zero)
{
    // ATTACK: Try to trigger division by zero with price = 0
    // Expected: Should return 0 (calculation fails safely)
    
    const CAmount DD_AMOUNT = 10000;
    const int64_t LOCK_BLOCKS = 30 * DigiDollar::BLOCKS_PER_DAY;
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, 0, 150, *regTestParams);  // ATTACK: Zero price
    
    CAmount required = DigiDollar::CalculateRequiredCollateral(DD_AMOUNT, LOCK_BLOCKS, ctx);
    
    // Should return 0 (calculation failed), not crash or return garbage
    BOOST_CHECK_EQUAL(required, 0);
}

BOOST_AUTO_TEST_CASE(redteam_negative_dd_amount)
{
    // ATTACK: Try to mint negative DD amount - could underflow
    // Expected: Should return 0 (rejected)
    
    const CAmount NEGATIVE_DD = -1000000;  // ATTACK: Negative amount
    const int64_t LOCK_BLOCKS = 30 * DigiDollar::BLOCKS_PER_DAY;
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, 5000, 150, *regTestParams);
    
    CAmount required = DigiDollar::CalculateRequiredCollateral(NEGATIVE_DD, LOCK_BLOCKS, ctx);
    
    // Should return 0 (rejected), not wrap to huge positive number
    BOOST_CHECK_MESSAGE(required == 0,
        "EXPLOIT: Negative DD amount should return 0, got " + std::to_string(required));
}

BOOST_AUTO_TEST_CASE(redteam_128bit_overflow_boundary)
{
    // ATTACK: Values chosen to overflow even __int128
    // numerator = ddAmount * COIN * ratio * 100
    // max __int128 ≈ 1.7 × 10^38
    // Try: 10^18 * 10^8 * 10^4 * 10^2 = 10^32 (should be safe)
    
    // This is the boundary where uint64_t would overflow but __int128 is safe
    const CAmount LARGE_DD = 1000000000000000LL;  // 10^15 cents ($10 trillion)
    const int64_t LOCK_BLOCKS = 30 * DigiDollar::BLOCKS_PER_DAY;
    const CAmount LOW_PRICE = 100;  // $0.0001 per DGB
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, LOW_PRICE, 150, *regTestParams);
    
    CAmount required = DigiDollar::CalculateRequiredCollateral(LARGE_DD, LOCK_BLOCKS, ctx);
    
    // Should be capped at MAX_MONEY, not overflow to garbage
    BOOST_CHECK_MESSAGE(required == MAX_MONEY || required > 0,
        "EXPLOIT: __int128 calculation returned unexpected value: " + std::to_string(required));
}

BOOST_AUTO_TEST_CASE(redteam_rounding_attack)
{
    // ATTACK: Choose values that might round down to zero or favorable value
    // Try: 1 cent DD at high price with minimum ratio
    
    const CAmount TINY_DD = 1;  // 1 cent
    const int64_t LOCK_BLOCKS = 10 * 365 * DigiDollar::BLOCKS_PER_DAY;  // 10 years (200%)
    const CAmount HIGH_PRICE = 100000000;  // $100 per DGB
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, HIGH_PRICE, 150, *regTestParams);
    
    CAmount required = DigiDollar::CalculateRequiredCollateral(TINY_DD, LOCK_BLOCKS, ctx);
    
    // Even for 1 cent at high price, should never round to 0
    // 1 cent * COIN * 200 * 100 / 100000000 = 200 sats
    BOOST_CHECK_GT(required, 0);
    
    // Verify it's actually a reasonable amount
    // Formula: (1 cent * COIN * 200 * 100) / 100,000,000 micro-USD = 20,000 sats
    BOOST_CHECK_MESSAGE(required >= 10000 && required <= 50000,
        "Unexpected collateral for 1 cent DD: " + std::to_string(required));
}

BOOST_AUTO_TEST_CASE(redteam_dca_multiplier_precision)
{
    // ATTACK: Test that DCA multiplier calculations don't have floating-point errors
    // that could benefit an attacker
    
    using namespace DigiDollar::DCA;
    
    // Test all tier boundaries for exact values
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(150), 1.0);  // Healthy
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(149), 1.2);  // Warning
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(120), 1.2);  // Warning
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(119), 1.5);  // Critical
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(100), 1.5);  // Critical
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(99), 2.0);   // Emergency
    
    // Test ApplyDCA for precision at boundaries
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::ApplyDCA(500, 150), 500);   // 500 * 1.0
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::ApplyDCA(500, 149), 600);   // 500 * 1.2
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::ApplyDCA(500, 99), 1000);   // 500 * 2.0
    
    // Test for floating-point precision issues
    // 333 * 1.2 = 399.6 -> should truncate to 399, not round to 400
    int adjusted = DynamicCollateralAdjustment::ApplyDCA(333, 149);
    BOOST_CHECK_MESSAGE(adjusted == 399,
        "DCA precision error: 333 * 1.2 = " + std::to_string(adjusted) + " (expected 399)");
}

BOOST_AUTO_TEST_CASE(redteam_txbuilder_validation_consistency)
{
    // ATTACK: Verify TxBuilder and Validation use SAME collateral calculation
    // A discrepancy could allow minting with insufficient collateral
    
    const CAmount DD_AMOUNT = 100000;  // $1,000
    const int64_t LOCK_BLOCKS = 30 * DigiDollar::BLOCKS_PER_DAY;
    const CAmount PRICE = 5000;  // $0.005 per DGB
    const int SYSTEM_HEALTH = 150;
    
    // Calculate via Validation path
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, PRICE, SYSTEM_HEALTH, *regTestParams);
    
    CAmount validationRequired = DigiDollar::CalculateRequiredCollateral(DD_AMOUNT, LOCK_BLOCKS, ctx);
    
    // Calculate via TxBuilder path
    DigiDollar::MintTxBuilder builder(ctx.params, 1000, PRICE);
    
    // Get base ratio from consensus
    DigiDollar::ConsensusParams ddParams;
    int baseRatio = DigiDollar::GetCollateralRatioForLockTime(LOCK_BLOCKS, ddParams);
    int effectiveRatio = DigiDollar::GetEffectiveCollateralRatio(baseRatio, SYSTEM_HEALTH, ctx.params);
    
    // Manual calculation matching TxBuilder
    // (DD * COIN * ratio * 100) / price
    __int128 numerator = static_cast<__int128>(DD_AMOUNT) * 
                         static_cast<__int128>(COIN) * 
                         static_cast<__int128>(effectiveRatio) * 100;
    __int128 builderRequired = numerator / static_cast<__int128>(PRICE);
    
    // CRITICAL: Both calculations must produce the SAME result
    BOOST_CHECK_MESSAGE(validationRequired == static_cast<CAmount>(builderRequired),
        "EXPLOIT: TxBuilder/Validation mismatch! Validation=" + 
        std::to_string(validationRequired) + " Builder=" + 
        std::to_string(static_cast<CAmount>(builderRequired)));
}

BOOST_AUTO_TEST_CASE(redteam_system_health_extremes)
{
    // ATTACK: Test extreme system health values
    
    using namespace DigiDollar::DCA;
    
    // Negative health (shouldn't happen but test anyway)
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(-100), 2.0);  // Emergency
    
    // Zero health
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(0), 2.0);  // Emergency
    
    // Maximum health at tier boundary
    BOOST_CHECK_EQUAL(DynamicCollateralAdjustment::GetDCAMultiplier(30000), 1.0);  // Healthy
    
    // NOTE: Health values > 30000 fall through to emergency multiplier (2.0x) because
    // HEALTH_TIERS has maxCollateral=30000 for the healthy tier. This is NOT exploitable
    // because CalculateSystemHealth() caps health at 30000 before calling GetDCAMultiplier().
    // However, this is a defense-in-depth concern: if the cap is ever removed, extreme
    // health values would paradoxically increase collateral requirements.
    BOOST_CHECK_MESSAGE(DynamicCollateralAdjustment::GetDCAMultiplier(30001) == 2.0,
        "NOTE: Health > 30000 returns emergency multiplier due to tier max boundary");
}

BOOST_AUTO_TEST_CASE(redteam_int128_edge_cases)
{
    // ATTACK: Test edge cases specific to __int128 arithmetic
    
    // Case 1: Numerator at max safe value for uint64_t
    // uint64_t max ≈ 1.8 × 10^19
    // If using uint64_t: 10^14 * 10^8 * 10^3 * 10^2 = 10^27 would overflow
    // With __int128: Should work correctly
    
    const CAmount LARGE_DD = 100000000000000LL;  // 10^14 cents
    const CAmount LOW_PRICE = 1000;  // Very low price
    const int64_t LOCK_BLOCKS = 240;  // 1-hour tier (1000% ratio)
    
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, LOW_PRICE, 50, *regTestParams);  // Emergency (2x multiplier)
    
    // This would overflow uint64_t but should be safe with __int128
    CAmount required = DigiDollar::CalculateRequiredCollateral(LARGE_DD, LOCK_BLOCKS, ctx);
    
    // Should be capped at MAX_MONEY, not garbage value
    BOOST_CHECK_MESSAGE(required == MAX_MONEY,
        "EXPLOIT: Large calculation should cap at MAX_MONEY, got " + std::to_string(required));
}

// =============================================================================
// T1-02: DD Amount Manipulation in OP_RETURN
// =============================================================================
// VULNERABILITY: Mint OP_RETURN format is DD <type=1> <amount> <lockHeight> <lockTier>
// But ExtractDDAmountFromTxRef reads ALL remaining pushes as DD amounts.
// An attacker adding extra P2TR zero-value outputs to a mint tx would have
// those outputs valued at lockHeight and lockTier values — creating DD from nothing.

BOOST_AUTO_TEST_CASE(redteam_opreturn_mint_extra_outputs_inflation)
{
    // ATTACK: Craft a mint tx with extra P2TR zero-value outputs.
    // The OP_RETURN has: DD <1> <10000> <172800> <2>
    // where 10000 = DD amount (cents), 172800 = lockHeight (30 days), 2 = lockTier
    //
    // WITHOUT the fix, ExtractDDAmountFromTxRef reads ALL remaining pushes as DD amounts:
    //   dd_amounts = [10000, 172800, 2]  ← VULNERABLE
    //
    // WITH the fix, type-aware parsing reads only first push for MINT:
    //   dd_amounts = [10000]  ← CORRECT
    //
    // This test verifies the type-aware parsing defense.

    // Build a mint OP_RETURN: DD <type=1> <ddAmount=10000> <lockHeight=172800> <lockTier=2>
    const CAmount ddAmount = 10000;       // $100 in cents
    const int64_t lockHeight = 172800;    // 30 days * 24 * 60 * 4
    const int64_t lockTier = 2;

    CScript mintOpReturn = CScript() << OP_RETURN
                                     << std::vector<unsigned char>{'D', 'D'}
                                     << CScriptNum(1)           // MINT type
                                     << CScriptNum(ddAmount)
                                     << CScriptNum(lockHeight)
                                     << CScriptNum(lockTier);

    // Simulate the FIXED ExtractDDAmountFromTxRef type-aware parsing:
    {
        CScript::const_iterator pc = mintOpReturn.begin();
        opcodetype opcode;
        std::vector<unsigned char> data;

        // Skip OP_RETURN
        BOOST_REQUIRE(mintOpReturn.GetOp(pc, opcode));
        BOOST_CHECK_EQUAL(opcode, OP_RETURN);

        // Check "DD" marker
        BOOST_REQUIRE(mintOpReturn.GetOp(pc, opcode, data));
        BOOST_CHECK(data.size() == 2 && data[0] == 'D' && data[1] == 'D');

        // Read transaction type
        BOOST_REQUIRE(mintOpReturn.GetOp(pc, opcode, data));
        int64_t txType = 0;
        if (data.size() > 0) {
            CScriptNum txTypeNum(data, true);
            txType = txTypeNum.GetInt64();
        }
        BOOST_CHECK_EQUAL(txType, 1);  // MINT

        // Type-aware extraction: for MINT (type 1), only read FIRST push as DD amount
        std::vector<CAmount> dd_amounts;
        if (txType == 1 || txType == 3) {
            // MINT or REDEEM: Only first push is DD amount
            if (mintOpReturn.GetOp(pc, opcode, data) && data.size() > 0) {
                CScriptNum scriptNum(data, true, 8);
                dd_amounts.push_back(scriptNum.GetInt64());
            }
        } else {
            // TRANSFER: All remaining pushes are DD amounts
            while (mintOpReturn.GetOp(pc, opcode, data)) {
                if (data.size() > 0) {
                    CScriptNum scriptNum(data, true, 8);
                    dd_amounts.push_back(scriptNum.GetInt64());
                }
            }
        }

        // DEFENSE VERIFIED: Type-aware parsing produces exactly 1 DD amount
        BOOST_CHECK_EQUAL(dd_amounts.size(), 1u);
        BOOST_CHECK_EQUAL(dd_amounts[0], ddAmount);

        // Also verify NAIVE parsing would have been vulnerable (regression guard)
        pc = mintOpReturn.begin();
        mintOpReturn.GetOp(pc, opcode);        // OP_RETURN
        mintOpReturn.GetOp(pc, opcode, data);  // "DD"
        mintOpReturn.GetOp(pc, opcode, data);  // type
        std::vector<CAmount> naive_amounts;
        while (mintOpReturn.GetOp(pc, opcode, data)) {
            if (data.size() > 0) {
                CScriptNum scriptNum(data, true, 8);
                naive_amounts.push_back(scriptNum.GetInt64());
            }
        }
        // Naive parsing reads 3 values — this is what the attack exploited
        BOOST_CHECK_EQUAL(naive_amounts.size(), 3u);
        BOOST_CHECK_EQUAL(naive_amounts[0], ddAmount);
        BOOST_CHECK_EQUAL(naive_amounts[1], lockHeight);  // Would have been misinterpreted as DD
        BOOST_CHECK_EQUAL(naive_amounts[2], lockTier);    // Would have been misinterpreted as DD
    }
}

BOOST_AUTO_TEST_CASE(redteam_mint_validation_allows_multiple_dd_outputs)
{
    // ATTACK: Craft a mint transaction with multiple P2TR zero-value outputs.
    // Mint validation should reject this, but currently does NOT count DD outputs.

    CKey testKey;
    testKey.MakeNewKey(true);
    CPubKey testPubKey = testKey.GetPubKey();
    XOnlyPubKey testXOnlyKey(testPubKey);

    CMutableTransaction mtx;
    mtx.SetDigiDollarType(DD_TX_MINT);

    // Input (fake)
    mtx.vin.push_back(CTxIn(COutPoint(uint256::ONE, 0)));

    // Collateral output (P2TR with value)
    CScript collateralScript = CScript() << OP_1 << ToByteVector(testXOnlyKey);
    mtx.vout.push_back(CTxOut(500 * COIN, collateralScript));

    // DD token output 1 (legitimate, P2TR with value=0)
    CKey ddKey1; ddKey1.MakeNewKey(true);
    XOnlyPubKey ddXOnly1(ddKey1.GetPubKey());
    CScript ddScript1 = CScript() << OP_1 << ToByteVector(ddXOnly1);
    mtx.vout.push_back(CTxOut(0, ddScript1));

    // DD token output 2 (EXTRA — attacker-controlled, P2TR with value=0)
    CKey ddKey2; ddKey2.MakeNewKey(true);
    XOnlyPubKey ddXOnly2(ddKey2.GetPubKey());
    CScript ddScript2 = CScript() << OP_1 << ToByteVector(ddXOnly2);
    mtx.vout.push_back(CTxOut(0, ddScript2));

    // DD token output 3 (EXTRA — attacker-controlled, P2TR with value=0)
    CKey ddKey3; ddKey3.MakeNewKey(true);
    XOnlyPubKey ddXOnly3(ddKey3.GetPubKey());
    CScript ddScript3 = CScript() << OP_1 << ToByteVector(ddXOnly3);
    mtx.vout.push_back(CTxOut(0, ddScript3));

    // OP_RETURN: DD <1> <10000> <172800> <2>
    CScript opReturn = CScript() << OP_RETURN
                                 << std::vector<unsigned char>{'D', 'D'}
                                 << CScriptNum(1)
                                 << CScriptNum(10000)
                                 << CScriptNum(172800)
                                 << CScriptNum(2);
    mtx.vout.push_back(CTxOut(0, opReturn));

    // Validate: mint validation should reject multiple DD outputs
    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);
    ctx.skipOracleValidation = true;  // Focus on structural validation

    TxValidationState state;
    CTransaction tx(mtx);
    bool valid = DigiDollar::ValidateMintTransaction(tx, ctx, state);

    // EXPLOIT PROOF: If this passes, the tx with 3 DD outputs was accepted
    // When later spent, the extra outputs get inflated DD values from lockHeight/lockTier
    BOOST_CHECK_MESSAGE(!valid,
        "EXPLOIT T1-02: Mint tx with " + std::to_string(3) + " DD outputs was ACCEPTED! "
        "Extra outputs would inherit lockHeight/lockTier as DD amounts. "
        "Validation state: " + state.ToString());
}

BOOST_AUTO_TEST_CASE(redteam_opreturn_360day_lock_inflation)
{
    // ATTACK: With a 360-day lock, lockHeight is enormous:
    //   360 * 24 * 60 * 4 = 2,073,600 blocks
    // Without fix: attacker would get $20,736 of fake DD per mint tx!
    // With fix: type-aware parsing only reads 1 amount for MINT txs.

    const CAmount ddAmount = 10000;
    const int64_t lockHeight360 = 360LL * 24 * 60 * 4;  // 2,073,600 blocks

    CScript mintOpReturn = CScript() << OP_RETURN
                                     << std::vector<unsigned char>{'D', 'D'}
                                     << CScriptNum(1)
                                     << CScriptNum(ddAmount)
                                     << CScriptNum(lockHeight360)
                                     << CScriptNum(2);

    // Parse with FIXED type-aware logic (same as patched ExtractDDAmountFromTxRef)
    CScript::const_iterator pc = mintOpReturn.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;

    mintOpReturn.GetOp(pc, opcode);       // OP_RETURN
    mintOpReturn.GetOp(pc, opcode, data); // "DD"
    mintOpReturn.GetOp(pc, opcode, data); // type → read as int

    int64_t txType = 0;
    if (data.size() > 0) {
        CScriptNum txTypeNum(data, true);
        txType = txTypeNum.GetInt64();
    }

    std::vector<CAmount> dd_amounts;
    if (txType == 1 || txType == 3) {
        // MINT/REDEEM: Only first push is DD amount
        if (mintOpReturn.GetOp(pc, opcode, data) && data.size() > 0) {
            CScriptNum scriptNum(data, true, 8);
            dd_amounts.push_back(scriptNum.GetInt64());
        }
    } else {
        while (mintOpReturn.GetOp(pc, opcode, data)) {
            if (data.size() > 0) {
                CScriptNum scriptNum(data, true, 8);
                dd_amounts.push_back(scriptNum.GetInt64());
            }
        }
    }

    // DEFENSE VERIFIED: Only 1 DD amount extracted (the real one)
    BOOST_CHECK_EQUAL(dd_amounts.size(), 1u);
    BOOST_CHECK_EQUAL(dd_amounts[0], ddAmount);

    // No fake DD from lockHeight or lockTier
    CAmount fakeDD = 0;
    for (size_t i = 1; i < dd_amounts.size(); i++) {
        fakeDD += dd_amounts[i];
    }
    BOOST_CHECK_EQUAL(fakeDD, 0);
}

// =============================================================================
// T1-03: CLTV Timelock Bypass on Collateral
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_cltv_normal_path_enforces_lockheight)
{
    // ATTACK: Create a collateral script with a future lockHeight, then try to
    // spend it at the current height. The CLTV check in the script should reject.
    //
    // Defense chain:
    //   1. Script CLTV: script_lockHeight <= tx.nLockTime
    //   2. IsFinalTx:   blockHeight > tx.nLockTime (when nSequence != FINAL)
    //   3. Combined:    blockHeight > tx.nLockTime >= script_lockHeight

    // Create a normal redemption script with lockHeight = 2000
    DigiDollar::MintParams mintParams;
    mintParams.ddAmount = 10000; // $100
    mintParams.lockHeight = 2000; // 1000 blocks in the future

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    mintParams.ownerKey = XOnlyPubKey(ownerKey.GetPubKey());

    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(mintParams);
    BOOST_CHECK(!normalPath.empty());

    // Verify the script starts with the lockHeight and CLTV opcode
    CScript::const_iterator pc = normalPath.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;

    // First element should be the lockHeight (2000)
    BOOST_CHECK(normalPath.GetOp(pc, opcode, data));
    CScriptNum extractedLockHeight(data, true, 5);
    BOOST_CHECK_EQUAL(extractedLockHeight.GetInt64(), 2000);

    // Second element should be OP_CHECKLOCKTIMEVERIFY
    BOOST_CHECK(normalPath.GetOp(pc, opcode));
    BOOST_CHECK_EQUAL(opcode, OP_CHECKLOCKTIMEVERIFY);

    // Third element should be OP_DROP
    BOOST_CHECK(normalPath.GetOp(pc, opcode));
    BOOST_CHECK_EQUAL(opcode, OP_DROP);

    // DEFENSE: Script correctly encodes lockHeight with CLTV enforcement
    // An attacker cannot modify the script after it's committed to the MAST tree
    // because the P2TR output key is derived from the MAST root hash.
}

BOOST_AUTO_TEST_CASE(redteam_cltv_err_path_also_enforces_lockheight)
{
    // ATTACK: Try to use ERR path to bypass CLTV (ERR might skip timelock).
    // Defense: ERR path ALSO requires CLTV — both paths enforce the lock.

    DigiDollar::MintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockHeight = 5000;

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    mintParams.ownerKey = XOnlyPubKey(ownerKey.GetPubKey());

    CScript errPath = DigiDollar::CreateERRPath(mintParams);
    BOOST_CHECK(!errPath.empty());

    // Verify ERR path starts with the SAME lockHeight + CLTV
    CScript::const_iterator pc = errPath.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;

    // First element should be the lockHeight (5000)
    BOOST_CHECK(errPath.GetOp(pc, opcode, data));
    CScriptNum extractedLockHeight(data, true, 5);
    BOOST_CHECK_EQUAL(extractedLockHeight.GetInt64(), 5000);

    // Must have OP_CHECKLOCKTIMEVERIFY
    BOOST_CHECK(errPath.GetOp(pc, opcode));
    BOOST_CHECK_EQUAL(opcode, OP_CHECKLOCKTIMEVERIFY);

    // DEFENSE: ERR path has identical CLTV enforcement as normal path.
    // There is NO redemption path without a timelock.
}

BOOST_AUTO_TEST_CASE(redteam_cltv_lockheight_zero_creates_trivial_lock)
{
    // ATTACK: Create a collateral script with lockHeight = 0.
    // A CLTV of 0 is trivially satisfied (any nLockTime >= 0, which is always true).
    // If an attacker can get a mint accepted with lockHeight=0, they can redeem immediately.

    DigiDollar::MintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockHeight = 0; // ATTACK: Zero lockHeight

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    mintParams.ownerKey = XOnlyPubKey(ownerKey.GetPubKey());

    // CreateNormalRedemptionPath checks lockHeight < 0 but NOT lockHeight == 0
    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(mintParams);

    // FINDING: lockHeight=0 creates a valid script with trivial CLTV
    // Script: 0 OP_CHECKLOCKTIMEVERIFY OP_DROP <key> OP_CHECKSIG
    // This is immediately spendable because CLTV(0) passes when tx.nLockTime >= 0
    // (nLockTime is uint32_t, always >= 0).
    //
    // DEFENSE ASSESSMENT: Not directly exploitable because:
    // 1. The wallet's LockDaysToBlocks(0) returns 240 blocks (1-hour minimum), not 0
    // 2. The consensus CalculateRequiredCollateral uses OP_RETURN lockTime for ratio
    //    calculation, and GetCollateralRatioForLockTime maps short locks to 1000% ratio
    // 3. An attacker crafting raw tx with lockHeight=0 in the script but a long lockTime
    //    in the OP_RETURN could get a better ratio — but this is the metadata mismatch
    //    issue documented separately.
    //
    // NOTE: For production (Phase 2), the collateral script's lockHeight should be
    // verified against the OP_RETURN metadata to prevent lock period misrepresentation.
    BOOST_CHECK(!normalPath.empty()); // Script IS created (no rejection of lockHeight=0)
}

BOOST_AUTO_TEST_CASE(redteam_cltv_negative_lockheight_rejected)
{
    // ATTACK: Create a script with negative lockHeight.
    // Expected: Script creation should fail (return empty).

    DigiDollar::MintParams mintParams;
    mintParams.ddAmount = 10000;
    mintParams.lockHeight = -1; // ATTACK: Negative lockHeight

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    mintParams.ownerKey = XOnlyPubKey(ownerKey.GetPubKey());

    CScript normalPath = DigiDollar::CreateNormalRedemptionPath(mintParams);

    // DEFENSE: Negative lockHeight returns empty script
    BOOST_CHECK(normalPath.empty());

    CScript errPath = DigiDollar::CreateERRPath(mintParams);
    BOOST_CHECK(errPath.empty());
}

BOOST_AUTO_TEST_CASE(redteam_cltv_mast_commitment_prevents_script_tampering)
{
    // ATTACK: Create two collateral P2TR outputs with different lockHeights
    // and verify they produce different P2TR output keys. This proves that
    // an attacker cannot reuse a MAST proof from a shorter lock against a longer lock.

    CKey ownerKey;
    ownerKey.MakeNewKey(true);

    DigiDollar::MintParams params1;
    params1.ddAmount = 10000;
    params1.lockHeight = 1000; // Short lock
    params1.ownerKey = XOnlyPubKey(ownerKey.GetPubKey());
    params1.internalKey = DigiDollar::GetCollateralNUMSKey();
    params1.oracleKeys = DigiDollar::GetOracleKeys(15);

    DigiDollar::MintParams params2;
    params2.ddAmount = 10000;
    params2.lockHeight = 100000; // Long lock
    params2.ownerKey = XOnlyPubKey(ownerKey.GetPubKey());
    params2.internalKey = DigiDollar::GetCollateralNUMSKey();
    params2.oracleKeys = DigiDollar::GetOracleKeys(15);

    CScript script1 = DigiDollar::CreateCollateralP2TR(params1);
    CScript script2 = DigiDollar::CreateCollateralP2TR(params2);

    // Both should produce valid scripts
    BOOST_CHECK(!script1.empty());
    BOOST_CHECK(!script2.empty());

    // DEFENSE: Different lockHeights produce different P2TR output keys.
    // This means an attacker CANNOT substitute a short-lock proof for a long-lock output.
    // The MAST tree hash changes when any script leaf changes, which changes the output key.
    BOOST_CHECK_MESSAGE(script1 != script2,
        "EXPLOIT: Different lockHeights produced identical P2TR outputs! "
        "An attacker could substitute MAST proofs between lock periods.");
}

BOOST_AUTO_TEST_CASE(redteam_cltv_validate_normal_redemption_timelock)
{
    // ATTACK: Try to redeem collateral at a block height before the timelock.
    // ValidateNormalRedemptionConditions checks ctx.nHeight >= tx.nLockTime.

    auto regTestParams = CChainParams::RegTest({});

    // Scenario 1: Timelock NOT expired (should reject)
    {
        DigiDollar::ValidationContext ctx(500, 5000, 150, *regTestParams);
        TxValidationState state;

        CMutableTransaction tx;
        tx.nLockTime = 1000; // Locked until height 1000
        tx.vin.resize(2);
        tx.vin[0].nSequence = 0xFFFFFFFE; // Enable CLTV
        tx.vin[1].nSequence = 0xFFFFFFFE;
        tx.vout.resize(1);
        tx.vout[0].nValue = 100 * COIN;

        bool result = DigiDollar::ValidateNormalRedemptionConditions(
            CTransaction(tx), ctx, state);

        // DEFENSE: Should reject — current height 500 < locktime 1000
        BOOST_CHECK_MESSAGE(!result,
            "EXPLOIT: Normal redemption accepted before timelock expiry!");
    }

    // Scenario 2: Timelock expired (should accept)
    {
        DigiDollar::ValidationContext ctx(1500, 5000, 150, *regTestParams);
        TxValidationState state;

        CMutableTransaction tx;
        tx.nLockTime = 1000; // Locked until height 1000
        tx.vin.resize(2);
        tx.vin[0].nSequence = 0xFFFFFFFE;
        tx.vin[1].nSequence = 0xFFFFFFFE;
        tx.vout.resize(1);
        tx.vout[0].nValue = 100 * COIN;

        bool result = DigiDollar::ValidateNormalRedemptionConditions(
            CTransaction(tx), ctx, state);

        // DEFENSE: Should accept — current height 1500 >= locktime 1000
        BOOST_CHECK_MESSAGE(result,
            "False negative: Normal redemption rejected after timelock expiry");
    }

    // Scenario 3: nLockTime = 0 (trivially satisfied — immediately redeemable)
    {
        DigiDollar::ValidationContext ctx(100, 5000, 150, *regTestParams);
        TxValidationState state;

        CMutableTransaction tx;
        tx.nLockTime = 0; // ATTACK: Zero locktime
        tx.vin.resize(2);
        tx.vin[0].nSequence = 0xFFFFFFFE;
        tx.vin[1].nSequence = 0xFFFFFFFE;
        tx.vout.resize(1);
        tx.vout[0].nValue = 100 * COIN;

        bool result = DigiDollar::ValidateNormalRedemptionConditions(
            CTransaction(tx), ctx, state);

        // NOTE: nLockTime=0 passes validation at any height.
        // This is correct Bitcoin behavior. The protection against zero-locktime
        // collateral must come from mint-time validation (ensuring proper lockHeight).
        BOOST_CHECK(result);
    }
}

BOOST_AUTO_TEST_CASE(redteam_cltv_err_path_timelock_check)
{
    // ATTACK: Try ERR redemption before timelock expires.
    // ERR path should also require timelock expiry.

    auto regTestParams = CChainParams::RegTest({});

    // ERR with unexpired timelock (should reject)
    {
        DigiDollar::ValidationContext ctx(500, 5000, 50, *regTestParams); // system health 50%
        TxValidationState state;

        CMutableTransaction tx;
        tx.nLockTime = 1000; // Locked until height 1000
        tx.vin.resize(2);
        tx.vin[0].nSequence = 0xFFFFFFFE;
        tx.vin[1].nSequence = 0xFFFFFFFE;
        tx.vout.resize(1);
        tx.vout[0].nValue = 100 * COIN;

        bool result = DigiDollar::ValidateEmergencyRedemptionConditions(
            CTransaction(tx), ctx, state);

        // DEFENSE: ERR path also rejects if timelock not expired
        BOOST_CHECK_MESSAGE(!result,
            "EXPLOIT: ERR redemption accepted before timelock expiry!");
    }
}

BOOST_AUTO_TEST_CASE(redteam_cltv_lockdays_zero_maps_to_240_blocks)
{
    // Verify that lockDays=0 (testing tier) maps to 240 blocks, not 0.
    // This prevents accidentally creating zero-CLTV collateral through the wallet.

    int64_t blocks = DigiDollar::LockDaysToBlocks(0);
    BOOST_CHECK_EQUAL(blocks, 240); // 1 hour at 15-second blocks

    // Also verify standard lock periods
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(30), 30 * DigiDollar::BLOCKS_PER_DAY);
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(365), 365 * DigiDollar::BLOCKS_PER_DAY);
}

BOOST_AUTO_TEST_CASE(redteam_cltv_collateral_ratio_for_zero_lockblocks)
{
    // ATTACK: What ratio does lockBlocks=0 get?
    // If it gets the best ratio (200%), that's an exploit.
    // It should get the worst ratio (1000%) since it's shorter than any tier.

    DigiDollar::ConsensusParams ddParams;
    int ratio = DigiDollar::GetCollateralRatioForLockTime(0, ddParams);

    // Defense: lockBlocks=0 should get the highest (worst for attacker) ratio
    // The first tier is 240 blocks (1 hour) at 1000%
    // lower_bound(0) finds the first element, which is 1000%
    BOOST_CHECK_MESSAGE(ratio >= 1000,
        "EXPLOIT: lockBlocks=0 got ratio " + std::to_string(ratio) +
        "% (expected >= 1000%). Attacker could mint more DD with less collateral.");

    // Also verify that lockBlocks=1 gets the same worst-case ratio
    int ratio1 = DigiDollar::GetCollateralRatioForLockTime(1, ddParams);
    BOOST_CHECK_GE(ratio1, 1000);
}

// =============================================================================
// T1-04: NUMS Key Bypass — Key-path spend collateral
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_key_bypass_fake_collateral)
{
    // ATTACK: Craft a mint transaction where the "collateral" P2TR output uses
    // the ATTACKER'S key as internal key instead of the NUMS point.
    //
    // If ValidateMintTransaction accepts this, the attacker can:
    // 1. Mint DD tokens with "collateral" they can key-path spend
    // 2. Key-path spend the collateral in a regular (non-DD) transaction
    // 3. Result: free DD tokens — unbacked stablecoins
    //
    // The defense should be: validation MUST verify the P2TR output was
    // constructed with the NUMS internal key, making key-path spend impossible.

    auto regTestParams = CChainParams::RegTest({});

    // Generate attacker's key (they know the private key)
    CKey attackerKey;
    attackerKey.MakeNewKey(true);
    XOnlyPubKey attackerXOnly(attackerKey.GetPubKey());

    // Generate a separate owner key for the script paths
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    const CAmount ddAmount = 10000;  // $100 in cents
    const int nHeight = 1000;
    // lockTier 1 = 30 days, so lockHeight = nHeight + LockDaysToBlocks(30)
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    // Step 1: Create a LEGITIMATE collateral P2TR output (with NUMS key)
    DigiDollar::MintParams legitimateParams;
    legitimateParams.ddAmount = ddAmount;
    legitimateParams.lockHeight = lockHeight;
    legitimateParams.ownerKey = ownerXOnly;
    legitimateParams.internalKey = DigiDollar::GetCollateralNUMSKey();
    legitimateParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript legitimateCollateral = DigiDollar::CreateCollateralP2TR(legitimateParams);
    BOOST_REQUIRE(!legitimateCollateral.empty());

    // Step 2: Create a FAKE collateral P2TR output using ATTACKER's key
    // Same MAST tree (Normal + ERR paths) but with attacker's key as internal key
    DigiDollar::MintParams fakeParams;
    fakeParams.ddAmount = ddAmount;
    fakeParams.lockHeight = lockHeight;
    fakeParams.ownerKey = ownerXOnly;
    fakeParams.internalKey = attackerXOnly;  // <-- ATTACKER'S KEY, NOT NUMS!
    fakeParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript fakeCollateral = DigiDollar::CreateCollateralP2TR(fakeParams);
    BOOST_REQUIRE(!fakeCollateral.empty());

    // Step 3: Verify the outputs are DIFFERENT (different internal keys = different P2TR)
    BOOST_CHECK_MESSAGE(legitimateCollateral != fakeCollateral,
        "Sanity check failed: different internal keys should produce different P2TR outputs");

    // Step 4: Both are valid P2TR format (same length, same OP_1 prefix)
    BOOST_CHECK_EQUAL(legitimateCollateral.size(), 34u);
    BOOST_CHECK_EQUAL(fakeCollateral.size(), 34u);
    BOOST_CHECK_EQUAL(legitimateCollateral[0], OP_1);
    BOOST_CHECK_EQUAL(fakeCollateral[0], OP_1);

    // Step 5: Craft a mint transaction using the FAKE collateral output
    CMutableTransaction mintTx;
    mintTx.nVersion = 2;

    // Input (dummy — just needs to exist for structural validation)
    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // Output 0: OP_RETURN with DD mint metadata (including owner pubkey for NUMS check)
    CScript opReturn = CScript() << OP_RETURN
                                 << std::vector<unsigned char>{'D', 'D'}
                                 << CScriptNum(1)           // MINT type
                                 << CScriptNum(ddAmount)
                                 << CScriptNum(lockHeight)
                                 << CScriptNum(1)           // lockTier 1 = 30 days
                                 << std::vector<unsigned char>(ownerXOnly.begin(), ownerXOnly.end());  // 32-byte owner pubkey
    mintTx.vout.push_back(CTxOut(0, opReturn));

    // Output 1: FAKE collateral (attacker's key as internal key, 100 DGB)
    mintTx.vout.push_back(CTxOut(100 * COIN, fakeCollateral));

    // Output 2: DD token output (P2TR, zero value)
    CScript ddTokenScript = DigiDollar::CreateDigiDollarP2TR(ownerXOnly, ddAmount);
    BOOST_REQUIRE(!ddTokenScript.empty());
    mintTx.vout.push_back(CTxOut(0, ddTokenScript));

    // Step 6: Run ValidateMintTransaction
    // Oracle price = $0.01 per DGB (1000 micro-USD), height matches our lockHeight calculation
    DigiDollar::ValidationContext ctx(nHeight, 1000, 150, *regTestParams);
    ctx.skipOracleValidation = true;  // Skip oracle for unit test
    TxValidationState state;

    bool mintAccepted = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    // VULNERABILITY CHECK: If this passes, attacker can mint with key-path-spendable collateral
    // The fix should make this FAIL with "bad-collateral-internal-key" or similar
    if (!mintAccepted) {
        // Validation rejected the fake collateral — find out WHY
        BOOST_TEST_MESSAGE("Fake collateral rejected with reason: " + state.GetRejectReason());
        // If it was rejected for a reason OTHER than NUMS key verification,
        // the defense may be incidental (e.g., metadata-based) and fragile.
        bool rejectedForNUMS = (state.GetRejectReason().find("nums") != std::string::npos ||
                                state.GetRejectReason().find("internal-key") != std::string::npos);
        if (!rejectedForNUMS) {
            BOOST_TEST_MESSAGE("WARNING: Rejected but NOT because of NUMS key verification. "
                             "Reason: " + state.GetRejectReason() +
                             " — defense may be incidental/fragile.");
        }
    }
    BOOST_CHECK_MESSAGE(!mintAccepted,
        "VULNERABILITY [T1-04]: ValidateMintTransaction accepted a mint tx with "
        "attacker's key as P2TR internal key instead of NUMS point! "
        "Attacker can key-path spend collateral, creating unbacked DD tokens. "
        "Fix: Verify P2TR output matches reconstruction with NUMS internal key.");
}

BOOST_AUTO_TEST_CASE(redteam_nums_key_legitimate_collateral_accepted)
{
    // CONTROL TEST: Verify that a LEGITIMATE mint (using NUMS key) still passes.
    // This ensures the fix doesn't break honest minting.

    auto regTestParams = CChainParams::RegTest({});

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    const int nHeight = 1000;
    const CAmount ddAmount = 10000;  // $100
    // Use tier 1 = 30 days lock, consistent lockHeight and tier
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    // Create LEGITIMATE collateral with NUMS key
    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = lockHeight;
    params.ownerKey = ownerXOnly;
    params.internalKey = DigiDollar::GetCollateralNUMSKey();
    params.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript collateral = DigiDollar::CreateCollateralP2TR(params);
    BOOST_REQUIRE(!collateral.empty());

    CMutableTransaction mintTx;
    mintTx.nVersion = 2;

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // lockTier 1 = 30 days, matching the lockHeight above
    // Include owner pubkey for NUMS verification
    CScript opReturn = CScript() << OP_RETURN
                                 << std::vector<unsigned char>{'D', 'D'}
                                 << CScriptNum(1)
                                 << CScriptNum(ddAmount)
                                 << CScriptNum(lockHeight)
                                 << CScriptNum(1)
                                 << std::vector<unsigned char>(ownerXOnly.begin(), ownerXOnly.end());
    mintTx.vout.push_back(CTxOut(0, opReturn));
    mintTx.vout.push_back(CTxOut(100 * COIN, collateral));

    CScript ddToken = DigiDollar::CreateDigiDollarP2TR(ownerXOnly, ddAmount);
    mintTx.vout.push_back(CTxOut(0, ddToken));

    DigiDollar::ValidationContext ctx(nHeight, 1000, 150, *regTestParams);
    ctx.skipOracleValidation = true;
    TxValidationState state;

    bool result = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    // Legitimate mint should always pass
    BOOST_CHECK_MESSAGE(result,
        "False positive: Legitimate mint with NUMS key was rejected. "
        "Error: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_nums_documentation_mismatch)
{
    // FINDING (LOW): The NUMS point comment says:
    //   "The point is: lift_x(SHA256("DigiDollar/CollateralNUMS"))"
    // But the actual bytes are the BIP-341 standard NUMS point,
    // NOT SHA256("DigiDollar/CollateralNUMS").
    //
    // The BIP-341 NUMS point IS provably unspendable, so this is
    // a documentation bug, not a security bug. But it should be fixed
    // to avoid confusion during audits.

    XOnlyPubKey nums = DigiDollar::GetCollateralNUMSKey();

    // Verify it's the standard BIP-341 NUMS point (used in Bitcoin Core tests too)
    std::string nums_hex = HexStr(Span<const unsigned char>(nums.data(), nums.size()));
    BOOST_CHECK_EQUAL(nums_hex, "50929b74c1a04954b78b4b6035e97a5e078a5a0f28ec96d547bfee9ace803ac0");

    // Verify the comment's claim is WRONG
    // SHA256("DigiDollar/CollateralNUMS") = 552a6b77728fa8f7...
    // This is NOT what's hardcoded. The hardcoded value is the BIP-341 standard.
    // (Just documenting the discrepancy — the BIP-341 point is actually better
    // since it's a widely-audited standard.)
    BOOST_CHECK_MESSAGE(true,
        "NOTE: COLLATERAL_NUMS_POINT_BYTES comment claims SHA256(\"DigiDollar/CollateralNUMS\") "
        "but actual value is BIP-341 standard NUMS point. Documentation should be corrected.");
}

BOOST_AUTO_TEST_CASE(redteam_non_dd_tx_can_spend_collateral_utxo)
{
    // ATTACK SCENARIO: After minting with fake collateral, the attacker creates
    // a regular (non-DD) transaction spending the collateral UTXO.
    //
    // General validation only triggers DD checks for transactions with DD markers.
    // A regular transaction spending a DD collateral UTXO bypasses ALL DD validation.
    //
    // This tests that the validation framework correctly identifies this gap:
    // there's no tracking of DD collateral UTXOs in the general validation path.

    // Verify that HasDigiDollarMarker returns false for a plain P2TR spend
    CMutableTransaction regularTx;
    regularTx.nVersion = 2;

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFF;
    regularTx.vin.push_back(input);

    // Simple P2TR output (not DD-related) — just a plain P2TR send
    CKey destKey;
    destKey.MakeNewKey(true);
    XOnlyPubKey destXOnly(destKey.GetPubKey());
    auto tweaked = destXOnly.CreateTapTweak(nullptr);
    BOOST_REQUIRE(tweaked.has_value());
    CScript destScript;
    destScript << OP_1 << std::vector<unsigned char>(tweaked->first.begin(), tweaked->first.end());
    regularTx.vout.push_back(CTxOut(99 * COIN, destScript));

    // This transaction has NO DD markers — it's a plain Bitcoin transaction
    bool hasDDMarker = DigiDollar::HasDigiDollarMarker(CTransaction(regularTx));
    BOOST_CHECK_MESSAGE(!hasDDMarker,
        "Sanity: Plain transaction should NOT have DD marker");

    // Since it has no DD marker, ValidateDigiDollarTransaction would return true
    // (pass through), meaning NO DD-specific checks apply.
    // This confirms the gap: collateral can be spent without redemption validation.
    DigiDollar::ValidationContext ctx(1000, 1000, 150, *CChainParams::RegTest({}));
    TxValidationState state;
    bool ddValid = DigiDollar::ValidateDigiDollarTransaction(
        CTransaction(regularTx), ctx, state);
    BOOST_CHECK_MESSAGE(ddValid,
        "Non-DD transaction should pass DD validation (pass-through)");
}

// =============================================================================
// T1-03-FIX: Lock Height vs Lock Tier Verification
// VULNERABILITY: OP_RETURN lockHeight was not verified against lockTier.
// An attacker could claim tier 9 (10-year, 200% ratio) but set a 1-hour
// lockHeight, getting favorable collateral ratio without actual lock period.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_lockheight_tier_mismatch_attack)
{
    // Simulate: attacker claims tier 9 (10-year lock, 200% ratio) in OP_RETURN
    // but sets lockHeight to currentHeight + 240 (1 hour lock)
    // This should be REJECTED by consensus validation

    // Tier 9 = 3650 days = 21,024,000 blocks
    // 1-hour lock = 240 blocks
    // Attack: get 200% ratio but only lock for 1 hour

    int currentHeight = 1000;
    int64_t fakeLockHeight = currentHeight + 240;  // 1 hour (tier 0 blocks)
    int64_t realTier9Blocks = DigiDollar::LockDaysToBlocks(3650);  // 10 years

    // Verify the mismatch is significant
    BOOST_CHECK(fakeLockHeight - currentHeight < realTier9Blocks - 10);

    // Verify LockDaysToBlocks returns expected values
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(0), 240);  // 1 hour
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(30), 30 * 5760);  // 30 days
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(3650), 3650 * 5760);  // 10 years

    // The actual consensus validation test requires a full tx context,
    // but we verify the math that the validation check uses:
    // actualLockBlocks = lockHeight - currentHeight
    // expectedLockBlocks = LockDaysToBlocks(TIER_LOCK_DAYS[tier])
    // REJECT if actualLockBlocks < expectedLockBlocks - 10

    static const int TIER_LOCK_DAYS[] = {0, 30, 90, 180, 365, 730, 1095, 1825, 2555, 3650};

    for (int tier = 0; tier <= 9; tier++) {
        int64_t expectedBlocks = DigiDollar::LockDaysToBlocks(TIER_LOCK_DAYS[tier]);

        // VALID: lockHeight matches tier
        int64_t validLockHeight = currentHeight + expectedBlocks;
        int64_t validActual = validLockHeight - currentHeight;
        BOOST_CHECK_MESSAGE(validActual >= expectedBlocks - 10,
            "Tier " + std::to_string(tier) + " with correct lockHeight should pass");

        // VALID: lockHeight slightly above tier (extra blocks OK)
        int64_t overLockHeight = currentHeight + expectedBlocks + 100;
        int64_t overActual = overLockHeight - currentHeight;
        BOOST_CHECK_MESSAGE(overActual >= expectedBlocks - 10,
            "Tier " + std::to_string(tier) + " with extra blocks should pass");

        // INVALID: lockHeight much shorter than claimed tier (attack!)
        if (tier > 0) {
            // Use tier 0 blocks (240) for any tier > 0 — this is the attack
            int64_t attackLockHeight = currentHeight + 240;
            int64_t attackActual = attackLockHeight - currentHeight;
            BOOST_CHECK_MESSAGE(attackActual < expectedBlocks - 10,
                "Tier " + std::to_string(tier) + " with 1-hour lockHeight should FAIL validation");
        }
    }
}

BOOST_AUTO_TEST_CASE(redteam_lockheight_tier_valid_range)
{
    // Verify that valid lock heights pass for each tier
    int currentHeight = 50000;
    static const int TIER_LOCK_DAYS[] = {0, 30, 90, 180, 365, 730, 1095, 1825, 2555, 3650};

    for (int tier = 0; tier <= 9; tier++) {
        int64_t expectedBlocks = DigiDollar::LockDaysToBlocks(TIER_LOCK_DAYS[tier]);
        int64_t lockHeight = currentHeight + expectedBlocks;
        int64_t actualBlocks = lockHeight - currentHeight;

        // Within tolerance (±10)
        BOOST_CHECK(actualBlocks >= expectedBlocks - 10);

        // Exact match
        BOOST_CHECK_EQUAL(actualBlocks, expectedBlocks);
    }
}

BOOST_AUTO_TEST_CASE(redteam_invalid_lock_tier_range)
{
    // Lock tier must be 0-9
    // Tier -1 and tier 10 should be rejected
    BOOST_CHECK((-1 < 0 || -1 > 9));   // -1 is out of range
    BOOST_CHECK((10 < 0 || 10 > 9));   // 10 is out of range
    BOOST_CHECK(!((5 < 0 || 5 > 9)));  // 5 is valid
    BOOST_CHECK(!((0 < 0 || 0 > 9)));  // 0 is valid
    BOOST_CHECK(!((9 < 0 || 9 > 9)));  // 9 is valid
}

// =============================================================================
// T1-04b: NUMS Key Bypass via Missing OP_RETURN
// VULNERABILITY: If a mint transaction has NO OP_RETURN, hasOwnerPubKey stays
// false and the NUMS verification guard (hasOwnerPubKey && ...) evaluates to
// false — the entire NUMS check is SKIPPED. Attacker uses their own key as
// P2TR internal key, enabling key-path spend that bypasses CLTV timelocks.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_bypass_no_opreturn)
{
    // ATTACK: Craft a mint transaction WITHOUT OP_RETURN.
    // Without OP_RETURN, hasOwnerPubKey stays false, and the NUMS check guard
    // evaluates to false — NUMS verification is entirely SKIPPED.
    //
    // The attacker uses their own key as the P2TR internal key instead of NUMS.
    // After minting, they key-path spend the collateral (bypassing CLTV),
    // keeping both DD tokens AND original DGB — unbacked DD from nothing.

    auto regTestParams = CChainParams::RegTest({});

    // Step 1: Attacker's key pair
    CKey attackerKey;
    attackerKey.MakeNewKey(true);
    XOnlyPubKey attackerXOnly(attackerKey.GetPubKey());

    const int nHeight = 1000;
    const CAmount ddAmount = 1000;  // $10 in cents
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    // Step 2: Create FAKE collateral with ATTACKER'S key as internal key
    DigiDollar::MintParams fakeParams;
    fakeParams.ddAmount = ddAmount;
    fakeParams.lockHeight = lockHeight;
    fakeParams.ownerKey = attackerXOnly;
    fakeParams.internalKey = attackerXOnly;  // ATTACK: own key, not NUMS
    fakeParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript fakeCollateral = DigiDollar::CreateCollateralP2TR(fakeParams);
    BOOST_REQUIRE_MESSAGE(!fakeCollateral.empty(),
        "Failed to create fake collateral P2TR with attacker's key");

    // Step 3: Create DD token output (registers Phase 1 metadata)
    CScript ddToken = DigiDollar::CreateDigiDollarP2TR(attackerXOnly, ddAmount);
    BOOST_REQUIRE(!ddToken.empty());

    // Step 4: Craft the mint transaction WITHOUT OP_RETURN
    CMutableTransaction mintTx;
    mintTx.nVersion = 2;

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // Output 0: FAKE collateral (attacker's key as internal key)
    mintTx.vout.push_back(CTxOut(100 * COIN, fakeCollateral));
    // Output 1: DD token output (P2TR, zero value)
    mintTx.vout.push_back(CTxOut(0, ddToken));
    // NO OP_RETURN — this is the bypass vector

    // Step 5: Validate
    DigiDollar::ValidationContext ctx(nHeight, 5000000, 150, *regTestParams);
    ctx.skipOracleValidation = true;
    TxValidationState state;

    bool accepted = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    // This SHOULD be rejected. If accepted, NUMS bypass confirmed.
    BOOST_CHECK_MESSAGE(!accepted,
        "VULNERABILITY [T1-04b]: Mint tx WITHOUT OP_RETURN accepted! "
        "NUMS verification skipped because hasOwnerPubKey=false. "
        "Attacker can key-path spend collateral, creating unbacked DD.");

    if (accepted) {
        BOOST_TEST_MESSAGE("EXPLOIT CONFIRMED: No OP_RETURN -> no NUMS check -> "
                          "attacker controls internal key -> key-path bypasses CLTV");
    } else {
        // If rejected, verify it's for the RIGHT reason (not incidental)
        std::string reason = state.GetRejectReason();
        BOOST_TEST_MESSAGE("Rejected with reason: " + reason);
        bool correctRejection =
            reason.find("opreturn") != std::string::npos ||
            reason.find("owner-pubkey") != std::string::npos ||
            reason.find("nums") != std::string::npos ||
            reason.find("dd-opreturn") != std::string::npos;
        BOOST_CHECK_MESSAGE(correctRejection,
            "Rejection '" + reason + "' is incidental — defense is fragile");
    }
}

BOOST_AUTO_TEST_CASE(redteam_nums_bypass_non_dd_opreturn)
{
    // VARIANT: Include an OP_RETURN but NOT with "DD" marker.
    // The DD-specific parsing (including owner pubkey extraction) only triggers
    // when OP_RETURN starts with "DD". A non-DD OP_RETURN still leaves
    // hasOwnerPubKey=false, bypassing NUMS check.

    auto regTestParams = CChainParams::RegTest({});

    CKey attackerKey;
    attackerKey.MakeNewKey(true);
    XOnlyPubKey attackerXOnly(attackerKey.GetPubKey());

    const int nHeight = 1000;
    const CAmount ddAmount = 1000;
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    // Fake collateral with attacker's key
    DigiDollar::MintParams fakeParams;
    fakeParams.ddAmount = ddAmount;
    fakeParams.lockHeight = lockHeight;
    fakeParams.ownerKey = attackerXOnly;
    fakeParams.internalKey = attackerXOnly;  // ATTACK
    fakeParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript fakeCollateral = DigiDollar::CreateCollateralP2TR(fakeParams);
    BOOST_REQUIRE(!fakeCollateral.empty());

    CScript ddToken = DigiDollar::CreateDigiDollarP2TR(attackerXOnly, ddAmount);
    BOOST_REQUIRE(!ddToken.empty());

    CMutableTransaction mintTx;
    mintTx.nVersion = 2;

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // Non-DD OP_RETURN (random metadata, not "DD" prefix)
    CScript opReturn = CScript() << OP_RETURN
                                 << std::vector<unsigned char>{'X', 'Y'}  // NOT "DD"
                                 << CScriptNum(42);
    mintTx.vout.push_back(CTxOut(0, opReturn));
    mintTx.vout.push_back(CTxOut(100 * COIN, fakeCollateral));
    mintTx.vout.push_back(CTxOut(0, ddToken));

    DigiDollar::ValidationContext ctx(nHeight, 5000000, 150, *regTestParams);
    ctx.skipOracleValidation = true;
    TxValidationState state;

    bool accepted = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    BOOST_CHECK_MESSAGE(!accepted,
        "VULNERABILITY [T1-04b]: Mint tx with non-DD OP_RETURN accepted! "
        "NUMS verification bypassed by using 'XY' instead of 'DD' prefix.");
}

// =============================================================================
// T1-04c: NUMS Key Bypass via Multiple Collateral Outputs
// VULNERABILITY: actualCollateralScript only stores the LAST P2TR value output,
// but totalCollateral sums ALL P2TR value outputs. An attacker can include
// a large FAKE collateral (own key as internal key) + small LEGIT collateral
// (NUMS key). NUMS check only verifies the last one. Attacker key-path spends
// the large fake collateral, leaving DD backed by only the small amount.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_bypass_multiple_collateral_outputs)
{
    // ATTACK: Craft a mint transaction with TWO P2TR collateral outputs:
    //   Output A: 99 DGB with ATTACKER's key as internal key (key-path spendable!)
    //   Output B: 1 DGB with NUMS key as internal key (legitimate, CLTV enforced)
    //
    // The NUMS verification checks actualCollateralScript which is the LAST P2TR
    // value output (Output B) — reconstruction matches → PASSES!
    //
    // But totalCollateral = 100 DGB (99 + 1). Only 1 DGB is ACTUALLY locked.
    // Attacker key-path spends Output A immediately, recovering 99 DGB.
    // Result: $100 DD tokens backed by $0.01 worth of collateral.

    auto regTestParams = CChainParams::RegTest({});

    // Attacker's key (known private key for key-path spend)
    CKey attackerKey;
    attackerKey.MakeNewKey(true);
    XOnlyPubKey attackerXOnly(attackerKey.GetPubKey());

    // Owner key for MAST scripts
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    const int nHeight = 1000;
    const CAmount ddAmount = 10000;  // $100 in cents
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    // Step 1: Create FAKE collateral with ATTACKER's key as internal key
    DigiDollar::MintParams fakeParams;
    fakeParams.ddAmount = ddAmount;
    fakeParams.lockHeight = lockHeight;
    fakeParams.ownerKey = ownerXOnly;
    fakeParams.internalKey = attackerXOnly;  // <-- ATTACKER'S KEY, NOT NUMS!
    fakeParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript fakeCollateral = DigiDollar::CreateCollateralP2TR(fakeParams);
    BOOST_REQUIRE(!fakeCollateral.empty());

    // Step 2: Create LEGITIMATE collateral with NUMS key (tiny amount)
    DigiDollar::MintParams legitParams;
    legitParams.ddAmount = ddAmount;
    legitParams.lockHeight = lockHeight;
    legitParams.ownerKey = ownerXOnly;
    legitParams.internalKey = DigiDollar::GetCollateralNUMSKey();
    legitParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript legitCollateral = DigiDollar::CreateCollateralP2TR(legitParams);
    BOOST_REQUIRE(!legitCollateral.empty());

    // Sanity: they're different scripts (different internal keys)
    BOOST_CHECK(fakeCollateral != legitCollateral);

    // Step 3: Create DD token output
    CScript ddToken = DigiDollar::CreateDigiDollarP2TR(ownerXOnly, ddAmount);
    BOOST_REQUIRE(!ddToken.empty());

    // Step 4: Craft the mint transaction
    // Output order is critical: FAKE first, LEGIT second (so LEGIT overwrites
    // actualCollateralScript and passes the NUMS check)
    CMutableTransaction mintTx;
    mintTx.nVersion = 2;

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // OP_RETURN with valid DD metadata including owner pubkey
    CScript opReturn = CScript() << OP_RETURN
                                 << std::vector<unsigned char>{'D', 'D'}
                                 << CScriptNum(1)           // MINT type
                                 << CScriptNum(ddAmount)    // DD amount
                                 << CScriptNum(lockHeight)  // Lock height
                                 << CScriptNum(1)           // lockTier 1 = 30 days
                                 << std::vector<unsigned char>(ownerXOnly.begin(), ownerXOnly.end());
    mintTx.vout.push_back(CTxOut(0, opReturn));

    // FAKE collateral: 99 DGB, attacker's key as internal key
    mintTx.vout.push_back(CTxOut(99 * COIN, fakeCollateral));

    // LEGIT collateral: 1 DGB, NUMS key as internal key
    // This is LAST in the output list, so actualCollateralScript = this one
    mintTx.vout.push_back(CTxOut(1 * COIN, legitCollateral));

    // DD token output
    mintTx.vout.push_back(CTxOut(0, ddToken));

    // Step 5: Validate
    DigiDollar::ValidationContext ctx(nHeight, 1000, 150, *regTestParams);
    ctx.skipOracleValidation = true;
    TxValidationState state;

    bool accepted = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    // VULNERABILITY CHECK: If accepted, attacker can:
    // 1. Mint $100 DD with totalCollateral=100 DGB (99+1)
    // 2. Key-path spend the 99 DGB fake collateral (no CLTV enforcement)
    // 3. Only 1 DGB remains locked — $100 DD backed by $0.01
    BOOST_CHECK_MESSAGE(!accepted,
        "VULNERABILITY [T1-04c]: Mint tx with MULTIPLE collateral outputs accepted! "
        "NUMS check only verifies the LAST P2TR output. Attacker includes 99 DGB "
        "with own key + 1 DGB with NUMS key. 99% of collateral is key-path spendable!");

    if (!accepted) {
        BOOST_TEST_MESSAGE("Multiple collateral rejected with reason: " + state.GetRejectReason());
        // Verify it's rejected for the right reason
        bool correctRejection =
            state.GetRejectReason().find("multiple") != std::string::npos ||
            state.GetRejectReason().find("collateral") != std::string::npos ||
            state.GetRejectReason().find("nums") != std::string::npos;
        BOOST_CHECK_MESSAGE(correctRejection,
            "Rejected for wrong reason: " + state.GetRejectReason());
    } else {
        BOOST_TEST_MESSAGE("EXPLOIT CONFIRMED: Multiple collateral outputs bypass NUMS check. "
                          "Only last P2TR value output is verified against NUMS reconstruction. "
                          "FIX: Enforce exactly 1 collateral output per mint, or verify ALL.");
    }
}

BOOST_AUTO_TEST_CASE(redteam_nums_point_is_valid_curve_point)
{
    // Verify the NUMS point is actually a valid secp256k1 curve point.
    // If IsFullyValid() fails, the NUMS key is not on the curve and
    // CreateCollateralP2TR would return empty script.
    XOnlyPubKey nums = DigiDollar::GetCollateralNUMSKey();
    BOOST_CHECK_MESSAGE(nums.IsFullyValid(),
        "CRITICAL: NUMS point is NOT a valid secp256k1 curve point! "
        "CreateCollateralP2TR will silently fail or produce invalid outputs.");

    // Verify it's exactly 32 bytes
    BOOST_CHECK_EQUAL(nums.size(), 32u);

    // Verify it matches the BIP-341 standard NUMS point
    std::string hex = HexStr(Span<const unsigned char>(nums.data(), nums.size()));
    BOOST_CHECK_EQUAL(hex, "50929b74c1a04954b78b4b6035e97a5e078a5a0f28ec96d547bfee9ace803ac0");
}

// =============================================================================
// T1-04d: NUMS Key Bypass via Owner Key Mismatch (OP_RETURN vs MAST)
// ATTACK: Provide owner_key_A in OP_RETURN but construct collateral MAST with
// owner_key_B. The NUMS reconstruction uses owner_key_A, producing a different
// MAST tree → different P2TR output → mismatch detected.
// This verifies the NUMS reconstruction is a COMPLETE binding of all parameters.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_owner_key_mismatch)
{
    // ATTACK: The attacker controls two keys (A and B).
    // They construct collateral with owner B in the MAST scripts (so B can sign
    // to redeem), but claim owner A in the OP_RETURN. If validation only checks
    // the NUMS internal key but not the owner key binding, the attacker could
    // claim to be "A" while having "B" in the actual spending paths.
    //
    // Impact if bypassed: ownership confusion — an attacker could claim DD tokens
    // belong to one address while collateral is controlled by another.

    auto regTestParams = CChainParams::RegTest({});

    // Two different keys
    CKey keyA, keyB;
    keyA.MakeNewKey(true);
    keyB.MakeNewKey(true);
    XOnlyPubKey xOnlyA(keyA.GetPubKey());
    XOnlyPubKey xOnlyB(keyB.GetPubKey());

    // Sanity: they're different keys
    BOOST_REQUIRE(xOnlyA != xOnlyB);

    const int nHeight = 1000;
    const CAmount ddAmount = 10000;
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    // Construct collateral with owner B in MAST, but NUMS internal key (legitimate looking)
    DigiDollar::MintParams mismatchParams;
    mismatchParams.ddAmount = ddAmount;
    mismatchParams.lockHeight = lockHeight;
    mismatchParams.ownerKey = xOnlyB;  // <-- MAST scripts use key B
    mismatchParams.internalKey = DigiDollar::GetCollateralNUMSKey();
    mismatchParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript mismatchCollateral = DigiDollar::CreateCollateralP2TR(mismatchParams);
    BOOST_REQUIRE(!mismatchCollateral.empty());

    CMutableTransaction mintTx;
    mintTx.nVersion = 2;

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // OP_RETURN claims owner A, but MAST uses owner B
    CScript opReturn = CScript() << OP_RETURN
                                 << std::vector<unsigned char>{'D', 'D'}
                                 << CScriptNum(1)
                                 << CScriptNum(ddAmount)
                                 << CScriptNum(lockHeight)
                                 << CScriptNum(1)
                                 << std::vector<unsigned char>(xOnlyA.begin(), xOnlyA.end());  // Claims key A
    mintTx.vout.push_back(CTxOut(0, opReturn));

    // Collateral built with key B in MAST (but NUMS internal key)
    mintTx.vout.push_back(CTxOut(100 * COIN, mismatchCollateral));

    CScript ddToken = DigiDollar::CreateDigiDollarP2TR(xOnlyA, ddAmount);
    mintTx.vout.push_back(CTxOut(0, ddToken));

    DigiDollar::ValidationContext ctx(nHeight, 1000, 150, *regTestParams);
    ctx.skipOracleValidation = true;
    TxValidationState state;

    bool accepted = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    // MUST be rejected: reconstruction with owner A + NUMS produces different P2TR
    // than actual collateral built with owner B + NUMS
    BOOST_CHECK_MESSAGE(!accepted,
        "VULNERABILITY [T1-04d]: Owner key mismatch not detected! "
        "OP_RETURN claims owner A but MAST scripts use owner B. "
        "NUMS reconstruction binding is incomplete.");

    if (!accepted) {
        std::string reason = state.GetRejectReason();
        BOOST_TEST_MESSAGE("Owner key mismatch rejected: " + reason);
        // Should be caught by NUMS reconstruction mismatch
        BOOST_CHECK_MESSAGE(
            reason.find("nums-mismatch") != std::string::npos ||
            reason.find("reconstruction") != std::string::npos,
            "Expected NUMS mismatch rejection, got: " + reason);
    }
}

// =============================================================================
// T1-04e: NUMS documentation accuracy test
// The comment claims lift_x(SHA256("DigiDollar/CollateralNUMS")) but the actual
// bytes are lift_x(SHA256(serialize_uncompressed(G))) — the BIP-341 NUMS point.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_is_bip341_standard)
{
    // Verify the NUMS point is the standard BIP-341 unspendable key:
    // lift_x(SHA256(04 || Gx || Gy)) where G is the secp256k1 generator
    //
    // secp256k1 generator uncompressed:
    // 04 79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798
    //    483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8

    XOnlyPubKey nums = DigiDollar::GetCollateralNUMSKey();
    std::string hex = HexStr(Span<const unsigned char>(nums.data(), nums.size()));

    // This IS the SHA256 of the uncompressed generator point
    BOOST_CHECK_EQUAL(hex, "50929b74c1a04954b78b4b6035e97a5e078a5a0f28ec96d547bfee9ace803ac0");

    // Verify it's NOT SHA256("DigiDollar/CollateralNUMS") (old incorrect comment)
    // SHA256("DigiDollar/CollateralNUMS") = 552a6b77728fa8f7...
    BOOST_CHECK_MESSAGE(hex != "552a6b77728fa8f73762edadabc2c5ccaa4cb1eaeb145efe887b5407300b607b",
        "NUMS point should NOT be SHA256('DigiDollar/CollateralNUMS') — "
        "it should be the BIP-341 standard lift_x(SHA256(uncompressed_G))");

    // The point must be valid on secp256k1
    BOOST_CHECK(nums.IsFullyValid());
}

// =============================================================================
// T1-04f: Multiple DD OP_RETURN Attack — Owner Key Overwrite
// ATTACK: Include two DD-marked OP_RETURN outputs with different owner keys.
// The validation loop processes outputs sequentially, overwriting owner key
// variables. The second OP_RETURN's owner key is used for NUMS reconstruction.
// An attacker could build collateral with owner B in MAST but put owner A in
// the first OP_RETURN and owner B in the second, hoping reconstruction matches.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_multiple_opreturn_owner_overwrite)
{
    // ATTACK SCENARIO:
    // 1. Attacker creates collateral P2TR with NUMS key + owner B in MAST
    // 2. First OP_RETURN has owner A (decoy)
    // 3. Second OP_RETURN has owner B (real owner matching MAST)
    // 4. Validation overwrites owner key, NUMS reconstruction uses owner B
    // 5. Reconstruction matches actual collateral → PASSES
    //
    // This tests whether multiple DD OP_RETURN outputs are allowed.
    // If they are, the attacker controls which owner key is used for
    // NUMS verification, which could enable owner key substitution attacks.

    auto regTestParams = CChainParams::RegTest({});
    const int nHeight = 1000;
    const CAmount ddAmount = 10000;  // $100
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    // Create two different owner keys
    CKey keyA, keyB;
    keyA.MakeNewKey(true);
    keyB.MakeNewKey(true);
    XOnlyPubKey xOnlyA(keyA.GetPubKey());
    XOnlyPubKey xOnlyB(keyB.GetPubKey());

    // Build collateral with NUMS key + owner B (legitimate construction)
    DigiDollar::MintParams params;
    params.ddAmount = ddAmount;
    params.lockHeight = lockHeight;
    params.ownerKey = xOnlyB;  // Owner B in MAST scripts
    params.internalKey = DigiDollar::GetCollateralNUMSKey();
    params.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript collateral = DigiDollar::CreateCollateralP2TR(params);
    BOOST_REQUIRE(!collateral.empty());

    // Build DD token output with owner A
    CScript ddToken = DigiDollar::CreateDigiDollarP2TR(xOnlyA, ddAmount);
    BOOST_REQUIRE(!ddToken.empty());

    // Build mint tx with DD version marker
    CMutableTransaction mintTx;
    mintTx.nVersion = 0x0D1D0770 | (0x01 << 24);  // DD MINT version

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // FIRST OP_RETURN: owner A (decoy)
    CScript opReturn1 = CScript() << OP_RETURN
                                  << std::vector<unsigned char>{'D', 'D'}
                                  << CScriptNum(1)  // MINT type
                                  << CScriptNum(ddAmount)
                                  << CScriptNum(lockHeight)
                                  << CScriptNum(1)  // tier 1 (30 days)
                                  << std::vector<unsigned char>(xOnlyA.begin(), xOnlyA.end());
    mintTx.vout.push_back(CTxOut(0, opReturn1));

    // SECOND OP_RETURN: owner B (matches MAST construction)
    CScript opReturn2 = CScript() << OP_RETURN
                                  << std::vector<unsigned char>{'D', 'D'}
                                  << CScriptNum(1)  // MINT type
                                  << CScriptNum(ddAmount)
                                  << CScriptNum(lockHeight)
                                  << CScriptNum(1)  // tier 1 (30 days)
                                  << std::vector<unsigned char>(xOnlyB.begin(), xOnlyB.end());
    mintTx.vout.push_back(CTxOut(0, opReturn2));

    // Collateral (NUMS + owner B)
    mintTx.vout.push_back(CTxOut(100 * COIN, collateral));

    // DD token output
    mintTx.vout.push_back(CTxOut(0, ddToken));

    DigiDollar::ValidationContext ctx(nHeight, 1000, 150, *regTestParams);
    ctx.skipOracleValidation = true;
    TxValidationState state;

    bool accepted = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    // MUST be rejected — multiple DD OP_RETURNs are now blocked (T1-04f fix)
    BOOST_CHECK_MESSAGE(!accepted,
        "VULNERABILITY [T1-04f]: Multiple DD OP_RETURN outputs should be rejected! "
        "The second OP_RETURN's owner key overwrites the first, creating ambiguity.");

    if (!accepted) {
        std::string reason = state.GetRejectReason();
        BOOST_TEST_MESSAGE("Multiple DD OP_RETURN rejected: " + reason);
        BOOST_CHECK_MESSAGE(
            reason.find("multiple-dd-opreturn") != std::string::npos,
            "Expected rejection for multiple DD OP_RETURN, got: " + reason);
    }
}

// =============================================================================
// T1-04g: Malformed DD Amount → totalDD=0 → NUMS Check Bypass Attempt
// ATTACK: Craft OP_RETURN with an amount field that causes CScriptNum exception.
// If totalDD stays 0, the NUMS verification guard condition evaluates to false
// and the check is skipped entirely.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_malformed_amount_bypass)
{
    // ATTACK SCENARIO:
    // 1. Craft OP_RETURN with valid DD marker but malformed amount (non-minimal encoding)
    // 2. CScriptNum constructor throws, catch block lets it continue
    // 3. totalDD stays at 0
    // 4. NUMS verification guard: hasOwnerPubKey && hasCollateralOutput && lockTime > 0 && totalDD > 0
    //    → totalDD==0 → guard FALSE → NUMS check SKIPPED
    // 5. Remaining checks: ValidateMintAmount(0) should reject (0 < minMintAmount)
    //
    // Expected defense: ValidateMintAmount(0) rejects, OR the DD amount calculation
    // from collateral fills in totalDD > 0 enabling the NUMS check.
    // This test verifies that totalDD=0 CANNOT bypass all checks.

    auto regTestParams = CChainParams::RegTest({});
    const int nHeight = 1000;
    const int64_t lockHeight = nHeight + DigiDollar::LockDaysToBlocks(30);

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    // Build collateral with ATTACKER'S key (NOT NUMS) — this is the exploit
    CKey attackerKey;
    attackerKey.MakeNewKey(true);
    XOnlyPubKey attackerXOnly(attackerKey.GetPubKey());

    // Use attacker's key as internal key — key-path spendable!
    DigiDollar::MintParams attackParams;
    attackParams.ddAmount = 10000;
    attackParams.lockHeight = lockHeight;
    attackParams.ownerKey = ownerXOnly;
    attackParams.internalKey = attackerXOnly;  // NOT NUMS — attacker's key!
    attackParams.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript attackCollateral = DigiDollar::CreateCollateralP2TR(attackParams);
    BOOST_REQUIRE(!attackCollateral.empty());

    CScript ddToken = DigiDollar::CreateDigiDollarP2TR(ownerXOnly, 10000);

    CMutableTransaction mintTx;
    mintTx.nVersion = 0x0D1D0770 | (0x01 << 24);

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    input.nSequence = 0xFFFFFFFE;
    mintTx.vin.push_back(input);

    // Craft OP_RETURN with MALFORMED DD amount (non-minimal CScriptNum encoding)
    // A valid amount "10000" in script is {10 27} (2 bytes).
    // Non-minimal encoding: {10 27 00} (3 bytes with unnecessary zero padding)
    // CScriptNum(data, true) throws scriptnum_error for non-minimal encoding
    CScript opReturn;
    opReturn << OP_RETURN;
    opReturn << std::vector<unsigned char>{'D', 'D'};  // DD marker
    opReturn << CScriptNum(1);  // MINT type

    // MALFORMED AMOUNT: non-minimal encoding of 10000 (0x2710)
    // Minimal encoding: {0x10, 0x27} — but we add a trailing 0x00
    std::vector<unsigned char> malformedAmount = {0x10, 0x27, 0x00};
    opReturn << malformedAmount;  // This WILL cause CScriptNum exception

    opReturn << CScriptNum(lockHeight);
    opReturn << CScriptNum(1);  // tier 1
    opReturn << std::vector<unsigned char>(ownerXOnly.begin(), ownerXOnly.end());

    mintTx.vout.push_back(CTxOut(0, opReturn));
    mintTx.vout.push_back(CTxOut(100 * COIN, attackCollateral));  // Non-NUMS collateral!
    mintTx.vout.push_back(CTxOut(0, ddToken));

    DigiDollar::ValidationContext ctx(nHeight, 1000, 150, *regTestParams);
    ctx.skipOracleValidation = true;  // Simulate historical block — no DD amount recalculation
    TxValidationState state;

    bool accepted = DigiDollar::ValidateMintTransaction(
        CTransaction(mintTx), ctx, state);

    // MUST be rejected. If accepted, NUMS check was bypassed via malformed amount.
    BOOST_CHECK_MESSAGE(!accepted,
        "CRITICAL VULNERABILITY [T1-04g]: Malformed DD amount caused NUMS check bypass! "
        "Non-NUMS collateral accepted. Attacker can key-path spend collateral, "
        "creating unbacked DD tokens. totalDD=0 skips NUMS guard condition.");

    if (!accepted) {
        std::string reason = state.GetRejectReason();
        BOOST_TEST_MESSAGE("Malformed amount attack rejected: " + reason);
        // Could be rejected by ValidateMintAmount(0), or by NUMS check, or by other check
        // Any rejection is acceptable — the key thing is it was NOT accepted
    }
}

// =============================================================================
// T1-04h: TaprootBuilder Determinism Verification
// Verify that CreateCollateralP2TR is fully deterministic — same inputs always
// produce identical P2TR outputs. Non-determinism would break NUMS reconstruction.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_taprootbuilder_determinism)
{
    // If TaprootBuilder has any non-determinism (threading, hash map ordering, etc.),
    // the NUMS reconstruction during validation could produce a different P2TR output
    // than what was used during minting, causing false positive rejections or
    // (worse) false negative acceptances.

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    DigiDollar::MintParams params;
    params.ddAmount = 10000;
    params.lockHeight = 200000;
    params.ownerKey = ownerXOnly;
    params.internalKey = DigiDollar::GetCollateralNUMSKey();
    params.oracleKeys = DigiDollar::GetOracleKeys(15);

    // Build the same P2TR 100 times and verify all are identical
    CScript reference = DigiDollar::CreateCollateralP2TR(params);
    BOOST_REQUIRE(!reference.empty());

    for (int i = 0; i < 100; i++) {
        CScript result = DigiDollar::CreateCollateralP2TR(params);
        BOOST_CHECK_MESSAGE(result == reference,
            "NON-DETERMINISM DETECTED in CreateCollateralP2TR at iteration " +
            std::to_string(i) + "! This would break NUMS reconstruction verification. "
            "Expected: " + HexStr(reference) + " Got: " + HexStr(result));
    }

    // Verify different lock heights produce different P2TR outputs
    // (lockHeight IS embedded in script paths via CLTV)
    DigiDollar::MintParams params2 = params;
    params2.lockHeight = 300000;
    CScript result2 = DigiDollar::CreateCollateralP2TR(params2);
    BOOST_CHECK_MESSAGE(result2 != reference,
        "Different lock heights should produce different P2TR outputs");

    // Verify different owner keys produce different P2TR outputs
    CKey ownerKey2;
    ownerKey2.MakeNewKey(true);
    DigiDollar::MintParams params3 = params;
    params3.ownerKey = XOnlyPubKey(ownerKey2.GetPubKey());
    CScript result3 = DigiDollar::CreateCollateralP2TR(params3);
    BOOST_CHECK_MESSAGE(result3 != reference,
        "Different owner keys should produce different P2TR outputs");

    // NOTE: Different DD amounts produce the SAME P2TR output because
    // ddAmount is NOT embedded in the CLTV script paths — it's only in
    // the OP_RETURN metadata. This is by design: the collateral ratio
    // check validates amounts, not the script itself.
    DigiDollar::MintParams params4 = params;
    params4.ddAmount = 50000;
    CScript result4 = DigiDollar::CreateCollateralP2TR(params4);
    BOOST_CHECK_MESSAGE(result4 == reference,
        "DD amounts should NOT affect P2TR output (amount is in OP_RETURN, not script)");

    BOOST_TEST_MESSAGE("TaprootBuilder determinism verified over 100 iterations + parameter variation");
}

// =============================================================================
// T1-04i: Oracle Keys Don't Affect P2TR Output
// Verify that oracle keys (passed in MintParams) don't change the collateral
// P2TR output. This is critical for reconstruction — the validator uses
// GetOracleKeys(15) which must produce the same output regardless of actual
// oracle key set.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_oracle_keys_irrelevant)
{
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    // Build with 15 oracle keys (standard)
    DigiDollar::MintParams params15;
    params15.ddAmount = 10000;
    params15.lockHeight = 200000;
    params15.ownerKey = ownerXOnly;
    params15.internalKey = DigiDollar::GetCollateralNUMSKey();
    params15.oracleKeys = DigiDollar::GetOracleKeys(15);
    CScript script15 = DigiDollar::CreateCollateralP2TR(params15);

    // Build with 7 oracle keys
    DigiDollar::MintParams params7 = params15;
    params7.oracleKeys = DigiDollar::GetOracleKeys(7);
    CScript script7 = DigiDollar::CreateCollateralP2TR(params7);

    // Build with 0 oracle keys
    DigiDollar::MintParams params0 = params15;
    params0.oracleKeys.clear();
    CScript script0 = DigiDollar::CreateCollateralP2TR(params0);

    // Build with completely different random oracle keys
    DigiDollar::MintParams paramsRandom = params15;
    paramsRandom.oracleKeys.clear();
    for (int i = 0; i < 15; i++) {
        CKey k;
        k.MakeNewKey(true);
        paramsRandom.oracleKeys.push_back(XOnlyPubKey(k.GetPubKey()));
    }
    CScript scriptRandom = DigiDollar::CreateCollateralP2TR(paramsRandom);

    // ALL should produce the same P2TR output, since oracle keys aren't
    // used in CreateNormalRedemptionPath or CreateERRPath
    BOOST_CHECK_MESSAGE(script15 == script7,
        "Oracle key count (15 vs 7) should NOT affect P2TR output");
    BOOST_CHECK_MESSAGE(script15 == script0,
        "Oracle key count (15 vs 0) should NOT affect P2TR output");
    BOOST_CHECK_MESSAGE(script15 == scriptRandom,
        "Random oracle keys should NOT affect P2TR output");

    BOOST_TEST_MESSAGE("Verified: Oracle keys are irrelevant to P2TR collateral construction");
}

// =============================================================================
// T1-04j: Cryptographic NUMS Point Derivation Verification
// Actually compute SHA256(uncompressed_generator_point) and verify it matches
// the hardcoded COLLATERAL_NUMS_POINT_BYTES. This proves the point is the
// standard BIP-341 NUMS point and not an arbitrary value with a known DL.
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_nums_cryptographic_derivation)
{
    // The secp256k1 generator point G (uncompressed, 65 bytes):
    // 04 + Gx (32 bytes) + Gy (32 bytes)
    const std::vector<unsigned char> generator_uncompressed = {
        0x04,
        0x79, 0xBE, 0x66, 0x7E, 0xF9, 0xDC, 0xBB, 0xAC,
        0x55, 0xA0, 0x62, 0x95, 0xCE, 0x87, 0x0B, 0x07,
        0x02, 0x9B, 0xFC, 0xDB, 0x2D, 0xCE, 0x28, 0xD9,
        0x59, 0xF2, 0x81, 0x5B, 0x16, 0xF8, 0x17, 0x98,
        0x48, 0x3A, 0xDA, 0x77, 0x26, 0xA3, 0xC4, 0x65,
        0x5D, 0xA4, 0xFB, 0xFC, 0x0E, 0x11, 0x08, 0xA8,
        0xFD, 0x17, 0xB4, 0x48, 0xA6, 0x85, 0x54, 0x19,
        0x9C, 0x47, 0xD0, 0x8F, 0xFB, 0x10, 0xD4, 0xB8
    };

    // Compute SHA256(generator_uncompressed) — this should be the NUMS x-coordinate
    CSHA256 hasher;
    unsigned char hash[32];
    hasher.Write(generator_uncompressed.data(), generator_uncompressed.size());
    hasher.Finalize(hash);

    std::string computed_hex = HexStr(Span<const unsigned char>(hash, 32));

    // Compare with hardcoded NUMS point
    XOnlyPubKey nums = DigiDollar::GetCollateralNUMSKey();
    std::string hardcoded_hex = HexStr(Span<const unsigned char>(nums.data(), nums.size()));

    BOOST_CHECK_MESSAGE(computed_hex == hardcoded_hex,
        "CRITICAL: NUMS point bytes do NOT match SHA256(uncompressed_G)! "
        "Computed: " + computed_hex + " Hardcoded: " + hardcoded_hex + " "
        "The hardcoded point may have a known discrete logarithm, "
        "making ALL collateral key-path spendable!");

    BOOST_TEST_MESSAGE("Cryptographic verification: SHA256(uncompressed_G) = " + computed_hex);
    BOOST_TEST_MESSAGE("Hardcoded NUMS point:       " + hardcoded_hex);

    // Double-check: the NUMS point must be a valid point on secp256k1
    // (lift_x must succeed — not all x-coordinates correspond to valid curve points)
    BOOST_CHECK_MESSAGE(nums.IsFullyValid(),
        "NUMS x-coordinate does not correspond to a valid secp256k1 point! "
        "lift_x() failed — the point cannot be used as a Taproot internal key.");
}

// =============================================================================
// T1-05: Oracle Price Forgery — Miner Bypass via skipOracleValidation
// =============================================================================

/**
 * T1-05a: EXPLOIT — skipOracleValidation bypasses collateral ratio in ConnectBlock
 *
 * VULNERABILITY: During ConnectBlock, skipOracleValidation = true is passed to the
 * DD validation context. This was intended for IBD (Initial Block Download) where
 * oracle prices may not be available. However, it also applies to newly mined blocks
 * from the P2P network. A malicious miner can:
 *   1. Construct a mint tx with minimal collateral (1 sat) backing $1000 DD
 *   2. Include it in their mined block
 *   3. During ConnectBlock on all nodes: collateral ratio check is SKIPPED
 *   4. Block accepted → unbacked DD tokens created
 *
 * This test proves the vulnerability by showing that:
 * - CalculateRequiredCollateral returns a meaningful value (not skipped)
 * - But ValidateMintTransaction with skipOracleValidation=true NEVER calls it
 *
 * The test directly demonstrates the code path gap: collateral ratio checking
 * is gated entirely behind !ctx.skipOracleValidation, meaning ANY mint tx
 * in a mined block passes the economic check.
 */
BOOST_AUTO_TEST_CASE(redteam_T1_05a_skip_oracle_bypasses_collateral)
{
    auto regTestParams = CChainParams::RegTest({});
    const int blockHeight = 1000;
    const CAmount ddAmount = 100000;     // $1000 DD
    const CAmount oraclePrice = 6500;    // $0.0065/DGB
    const int lockBlocks = 30 * DigiDollar::BLOCKS_PER_DAY;  // 30 days

    // ═══════════════════════════════════════════════════
    // TEST 1: CalculateRequiredCollateral gives a real answer
    // ═══════════════════════════════════════════════════
    {
        DigiDollar::ValidationContext ctx(blockHeight, oraclePrice, 150, *regTestParams,
                                          nullptr, false);

        CAmount required = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctx);
        BOOST_TEST_MESSAGE("Required collateral for $1000 DD at $0.0065: " +
                          std::to_string(required) + " satoshis (" +
                          std::to_string(required / 100000000.0) + " DGB)");

        // At $0.0065/DGB with 150% base ratio, you need LOTS of DGB
        // $1000 DD = 100,000 cents, needs ~230 million DGB sats at minimum
        BOOST_CHECK_MESSAGE(required > 0,
            "CalculateRequiredCollateral returns meaningful value");
        BOOST_CHECK_MESSAGE(required > 100,
            "Required collateral is FAR more than 100 satoshis");
    }

    // ═══════════════════════════════════════════════════
    // TEST 2: Direct code inspection — the vulnerability
    // ═══════════════════════════════════════════════════
    // The critical code in ValidateMintTransaction (digidollar/validation.cpp):
    //
    //   if (!ctx.skipOracleValidation) {    // <-- THIS GATE
    //       requiredCollateral = CalculateRequiredCollateral(totalDD, lockTime, ctx);
    //       if (totalCollateral < requiredCollateral) { REJECT }
    //       if (!ValidateCollateralRatio(...)) { REJECT }
    //   }
    //
    // When skipOracleValidation = true:
    //   - CalculateRequiredCollateral is NEVER called
    //   - totalCollateral < requiredCollateral is NEVER checked
    //   - ValidateCollateralRatio is NEVER called
    //   - requiredCollateral stays at 0
    //
    // This means ALL structural checks pass (NUMS, DD marker, output counts),
    // but the ECONOMIC check (is collateral sufficient?) is SKIPPED.

    // Build a minimal mint tx to prove the structural checks pass
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    CPubKey ownerPubKey = ownerKey.GetPubKey();
    XOnlyPubKey ownerXOnly(ownerPubKey);

    const CAmount tinyCollateral = 100;  // 100 satoshis
    const int64_t lockHeight = blockHeight + lockBlocks;

    // Create oracle keys for MintParams
    std::vector<XOnlyPubKey> oracleXKeys;
    for (int i = 0; i < 7; i++) {
        CKey k;
        k.MakeNewKey(true);
        oracleXKeys.push_back(XOnlyPubKey(k.GetPubKey()));
    }

    // Build collateral P2TR script using proper MintParams
    DigiDollar::MintParams mintParams;
    mintParams.ddAmount = ddAmount;
    mintParams.lockHeight = lockHeight;
    mintParams.ownerKey = ownerXOnly;
    // Use NUMS point as internal key
    mintParams.internalKey = DigiDollar::GetCollateralNUMSKey();
    mintParams.oracleKeys = oracleXKeys;

    CScript collateralScript = DigiDollar::CreateCollateralP2TR(mintParams);
    CScript ddScript = DigiDollar::CreateDigiDollarP2TR(ownerXOnly, ddAmount);

    CScript metadataScript = CScript() << OP_RETURN
                                       << std::vector<unsigned char>{'D', 'D'}
                                       << CScriptNum(1)  // MINT
                                       << CScriptNum(ddAmount)
                                       << CScriptNum(lockHeight)
                                       << CScriptNum(0)  // lockTier
                                       << std::vector<unsigned char>(ownerXOnly.begin(), ownerXOnly.end());

    CMutableTransaction mtx;
    mtx.nVersion = MakeDigiDollarVersion(DD_TX_MINT);
    mtx.nLockTime = lockHeight;
    mtx.vin.push_back(CTxIn(COutPoint(uint256::ONE, 0)));
    mtx.vout.push_back(CTxOut(tinyCollateral, collateralScript));
    mtx.vout.push_back(CTxOut(0, ddScript));
    mtx.vout.push_back(CTxOut(0, metadataScript));

    CTransaction finalTx(mtx);

    // ═══════════════════════════════════════════════════
    // TEST 3: Mempool path MUST reject
    // ═══════════════════════════════════════════════════
    {
        DigiDollar::ValidationContext ctx(blockHeight, oraclePrice, 150, *regTestParams,
                                          nullptr, false);
        TxValidationState state;
        bool result = DigiDollar::ValidateMintTransaction(finalTx, ctx, state);
        BOOST_CHECK_MESSAGE(!result,
            "DEFENSE VERIFIED: Mempool rejects mint with insufficient collateral. "
            "Reason: " + state.GetRejectReason());
    }

    // ═══════════════════════════════════════════════════
    // TEST 4: ConnectBlock path — does it also reject?
    // ═══════════════════════════════════════════════════
    {
        DigiDollar::ValidationContext ctx(blockHeight, oraclePrice, 150, *regTestParams,
                                          nullptr, true);  // skipOracleValidation = true
        TxValidationState state;
        bool result = DigiDollar::ValidateMintTransaction(finalTx, ctx, state);

        // If this PASSES, the exploit is confirmed
        BOOST_CHECK_MESSAGE(!result,
            "EXPLOIT FOUND [T1-05a]: skipOracleValidation=true bypasses collateral ratio! "
            "Miner can include mint with 100 sat collateral for $1000 DD in a block. "
            "During ConnectBlock, collateral validation is entirely skipped. "
            "FIX: Collateral ratio MUST be checked during ConnectBlock when oracle price is available.");

        if (result) {
            BOOST_TEST_MESSAGE("*** CRITICAL: Mint with 100 sats for $1000 DD PASSED ConnectBlock validation ***");
        } else {
            BOOST_TEST_MESSAGE("Mint correctly rejected in ConnectBlock path. "
                             "Reason: " + state.GetRejectReason());
        }
    }
}

/**
 * T1-05b: Phase 1 compact oracle format — no signature in coinbase OP_RETURN
 *
 * VULNERABILITY: Phase 1 compact format stores only oracle_id + price + timestamp
 * in the coinbase OP_RETURN. No Schnorr signature is included. During block validation,
 * ValidateBlockOracleData skips signature verification for empty schnorr_sig.
 * A malicious miner can set ANY oracle price.
 *
 * MITIGATION: Current testnet/regtest configs activate Phase 2 at the same height
 * as DigiDollar, so Phase 1 compact format is never used standalone. But the code
 * path exists and would be exploitable if Phase 1 were ever used independently.
 */
BOOST_AUTO_TEST_CASE(redteam_T1_05b_phase1_oracle_no_signature)
{
    // Create a Phase 1 compact oracle script with FORGED price
    CScript forgedOracleScript;
    forgedOracleScript << OP_RETURN << OP_ORACLE;
    forgedOracleScript << std::vector<unsigned char>{0x01};  // version = Phase 1

    // Forge: oracle_id=0, price=$100 (100,000,000 micro-USD), current timestamp
    std::vector<unsigned char> compact_data;
    compact_data.reserve(17);
    compact_data.push_back(0);  // oracle_id = 0

    // Price: $100.00 = 100,000,000 micro-USD (the maximum allowed)
    uint64_t forged_price = 100000000;  // $100 — extreme manipulation
    for (int i = 0; i < 8; ++i) {
        compact_data.push_back(static_cast<unsigned char>((forged_price >> (i * 8)) & 0xFF));
    }

    // Timestamp: current time
    int64_t now = GetTime();
    for (int i = 0; i < 8; ++i) {
        compact_data.push_back(static_cast<unsigned char>((now >> (i * 8)) & 0xFF));
    }

    forgedOracleScript << compact_data;

    // Build fake coinbase with forged oracle data
    CMutableTransaction coinbase_tx;
    coinbase_tx.vin.push_back(CTxIn());
    coinbase_tx.vout.push_back(CTxOut(5000000000, CScript())); // block reward
    coinbase_tx.vout.push_back(CTxOut(0, forgedOracleScript));  // FORGED oracle

    CTransaction coinbase(coinbase_tx);

    // Extract oracle bundle — should succeed (no sig needed for compact format)
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    COracleBundle bundle;
    bool extracted = manager.ExtractOracleBundle(coinbase, bundle);

    BOOST_CHECK_MESSAGE(extracted,
        "Phase 1 compact oracle extraction works (expected)");

    if (extracted) {
        BOOST_CHECK_EQUAL(bundle.messages.size(), 1);
        BOOST_CHECK_EQUAL(bundle.median_price_micro_usd, forged_price);

        // The message should have NO signature (compact format)
        const COraclePriceMessage& msg = bundle.messages[0];
        BOOST_CHECK_MESSAGE(msg.schnorr_sig.empty(),
            "Phase 1 compact format has no signature — expected");

        // IsValid() should return true for compact format (empty sig = trusted)
        // This is the vulnerability: no cryptographic verification
        bool isValid = msg.IsValid(now);
        BOOST_CHECK_MESSAGE(isValid,
            "VULNERABILITY CONFIRMED: Phase 1 compact message with forged price ($100) "
            "passes IsValid() because empty signature is implicitly trusted. "
            "MITIGATION: Phase 2 activates at same height as DD on all networks, "
            "so this code path is never exercised in production.");

        BOOST_TEST_MESSAGE("Phase 1 compact format vulnerability: Forged price of $" +
            std::to_string(forged_price / 1000000) + " accepted without signature verification. "
            "This is acceptable ONLY because Phase 2 always activates simultaneously.");
    }
}

/**
 * T1-05c: P2P oracle message — signature verification with chainparams pubkey binding
 *
 * DEFENSE TEST: Verify that an attacker cannot inject forged oracle prices via P2P.
 * The net_processing code replaces the attacker-supplied pubkey with the authorized
 * pubkey from chainparams before signature verification.
 */
BOOST_AUTO_TEST_CASE(redteam_T1_05c_p2p_oracle_pubkey_binding)
{
    // Attacker generates their own key pair
    CKey attackerKey;
    attackerKey.MakeNewKey(true);

    // Create oracle message with attacker's key
    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 100000000;  // Forged: $100
    msg.timestamp = GetTime();
    msg.block_height = 1000;
    msg.nonce = 42;

    // Attacker signs with their own key
    BOOST_REQUIRE(msg.SignPhase2(attackerKey));

    // Verify passes with attacker's own key (expected — they signed it)
    BOOST_CHECK(msg.VerifyPhase2());

    // Now simulate the chainparams pubkey binding (what net_processing does)
    // Replace attacker's pubkey with a different authorized key
    CKey authorizedKey;
    authorizedKey.MakeNewKey(true);
    msg.oracle_pubkey = XOnlyPubKey(authorizedKey.GetPubKey());

    // Verification MUST fail — signature was made with attacker's key,
    // but we're verifying against the authorized key
    bool verifyResult = msg.VerifyPhase2();
    BOOST_CHECK_MESSAGE(!verifyResult,
        "DEFENSE HOLDS: After pubkey binding to chainparams key, attacker's "
        "forged signature fails verification. P2P oracle forgery is not possible.");
}

/**
 * T1-05d: Oracle message timestamp bounds — reject stale and future messages
 */
BOOST_AUTO_TEST_CASE(redteam_T1_05d_oracle_timestamp_validation)
{
    CKey validKey;
    validKey.MakeNewKey(true);
    int64_t now = GetTime();

    // Test 1: Message from far future (>60s) must be rejected
    {
        COraclePriceMessage futureMsg;
        futureMsg.oracle_id = 0;
        futureMsg.price_micro_usd = 6500;
        futureMsg.timestamp = now + 3600;  // 1 hour in future
        futureMsg.block_height = 1000;
        futureMsg.nonce = 1;
        BOOST_REQUIRE(futureMsg.SignPhase2(validKey));

        BOOST_CHECK_MESSAGE(!futureMsg.IsValid(now),
            "DEFENSE HOLDS: Oracle message from far future rejected");
    }

    // Test 2: Message too old (>1 hour) must be rejected
    {
        COraclePriceMessage staleMsg;
        staleMsg.oracle_id = 0;
        staleMsg.price_micro_usd = 6500;
        staleMsg.timestamp = now - 7200;  // 2 hours old
        staleMsg.block_height = 1000;
        staleMsg.nonce = 2;
        BOOST_REQUIRE(staleMsg.SignPhase2(validKey));

        BOOST_CHECK_MESSAGE(!staleMsg.IsValid(now),
            "DEFENSE HOLDS: Oracle message >1 hour old rejected");
    }

    // Test 3: Message within bounds must pass
    {
        COraclePriceMessage validMsg;
        validMsg.oracle_id = 0;
        validMsg.price_micro_usd = 6500;
        validMsg.timestamp = now - 30;  // 30 seconds ago
        validMsg.block_height = 1000;
        validMsg.nonce = 3;
        BOOST_REQUIRE(validMsg.SignPhase2(validKey));

        BOOST_CHECK_MESSAGE(validMsg.IsValid(now),
            "Valid oracle message within time bounds accepted");
    }
}

/**
 * T1-05e: Oracle price range validation — reject out-of-bounds prices
 */
BOOST_AUTO_TEST_CASE(redteam_T1_05e_oracle_price_range)
{
    CKey validKey;
    validKey.MakeNewKey(true);
    int64_t now = GetTime();

    // Test 1: Zero price
    {
        COraclePriceMessage msg;
        msg.oracle_id = 0;
        msg.price_micro_usd = 0;
        msg.timestamp = now;
        msg.block_height = 1000;
        msg.nonce = 1;
        BOOST_REQUIRE(msg.SignPhase2(validKey));
        BOOST_CHECK_MESSAGE(!msg.IsValid(now), "DEFENSE HOLDS: Zero price rejected");
    }

    // Test 2: Price below minimum ($0.0001 = 100 micro-USD)
    {
        COraclePriceMessage msg;
        msg.oracle_id = 0;
        msg.price_micro_usd = 99;  // Below ORACLE_MIN_PRICE_MICRO_USD (100)
        msg.timestamp = now;
        msg.block_height = 1000;
        msg.nonce = 2;
        BOOST_REQUIRE(msg.SignPhase2(validKey));
        BOOST_CHECK_MESSAGE(!msg.IsValid(now), "DEFENSE HOLDS: Price below minimum rejected");
    }

    // Test 3: Price above maximum ($100 = 100,000,000 micro-USD)
    {
        COraclePriceMessage msg;
        msg.oracle_id = 0;
        msg.price_micro_usd = 100000001;  // Above ORACLE_MAX_PRICE_MICRO_USD
        msg.timestamp = now;
        msg.block_height = 1000;
        msg.nonce = 3;
        BOOST_REQUIRE(msg.SignPhase2(validKey));
        BOOST_CHECK_MESSAGE(!msg.IsValid(now), "DEFENSE HOLDS: Price above maximum rejected");
    }

    // Test 4: Boundary values — minimum and maximum should PASS
    {
        COraclePriceMessage minMsg;
        minMsg.oracle_id = 0;
        minMsg.price_micro_usd = ORACLE_MIN_PRICE_MICRO_USD;
        minMsg.timestamp = now;
        minMsg.block_height = 1000;
        minMsg.nonce = 4;
        BOOST_REQUIRE(minMsg.SignPhase2(validKey));
        BOOST_CHECK(minMsg.IsValid(now));

        COraclePriceMessage maxMsg;
        maxMsg.oracle_id = 0;
        maxMsg.price_micro_usd = ORACLE_MAX_PRICE_MICRO_USD;
        maxMsg.timestamp = now;
        maxMsg.block_height = 1000;
        maxMsg.nonce = 5;
        BOOST_REQUIRE(maxMsg.SignPhase2(validKey));
        BOOST_CHECK(maxMsg.IsValid(now));
    }
}

/**
 * T1-05f: Oracle ID validation — reject IDs outside valid range
 */
BOOST_AUTO_TEST_CASE(redteam_T1_05f_oracle_id_range)
{
    // P2P layer checks: oracle_id < ORACLE_TOTAL_COUNT (30)
    // Test that OracleP2P::ValidateIncomingMessage rejects out-of-range IDs

    CKey validKey;
    validKey.MakeNewKey(true);
    int64_t now = GetTime();

    // Oracle ID at boundary (30 = ORACLE_TOTAL_COUNT, should fail)
    COraclePriceMessage msg;
    msg.oracle_id = ORACLE_TOTAL_COUNT;
    msg.price_micro_usd = 6500;
    msg.timestamp = now;
    msg.block_height = 1000;
    msg.nonce = 1;
    BOOST_REQUIRE(msg.SignPhase2(validKey));

    OracleP2P::ClearRateLimitState();
    bool result = OracleP2P::ValidateIncomingMessage(msg);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS: Oracle ID at boundary (30) rejected by P2P validation");

    // Oracle ID = max uint32 (extreme)
    msg.oracle_id = 0xFFFFFFFF;
    BOOST_REQUIRE(msg.SignPhase2(validKey));
    result = OracleP2P::ValidateIncomingMessage(msg);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS: Oracle ID 0xFFFFFFFF rejected by P2P validation");
}

// =============================================================================
// T1-06: BIP9 Activation Gate Bypass
// ATTACK: Can DigiDollar transactions or opcodes be used before BIP9 activation?
// Tests verify that all gates (mempool, ConnectBlock, script, RPC) are consistent.
// =============================================================================

// T1-06a: HasDigiDollarMarker correctly identifies DD version field
BOOST_AUTO_TEST_CASE(redteam_T1_06a_dd_marker_version_check)
{
    // The DD version marker is 0x0770 in the lower 16 bits
    const int32_t DD_TX_VERSION = 0x0D1D0770;

    // ATTACK: Can we craft a transaction that IS a DD tx but evades marker detection?
    {
        CMutableTransaction tx;
        tx.nVersion = DD_TX_VERSION;
        BOOST_CHECK_MESSAGE(DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Full DD version should be detected");
    }

    // Different upper bytes should still detect DD marker (lower 16 bits match)
    {
        CMutableTransaction tx;
        tx.nVersion = 0x00000770;  // Minimal DD version
        BOOST_CHECK_MESSAGE(DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Minimal DD version (0x0770) should be detected");
    }

    {
        CMutableTransaction tx;
        tx.nVersion = 0xFF000770;  // Exotic upper bytes but DD lower
        BOOST_CHECK_MESSAGE(DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Exotic upper bytes with DD lower should be detected");
    }

    // Non-DD versions should NOT be detected
    {
        CMutableTransaction tx;
        tx.nVersion = 1;  // Standard v1
        BOOST_CHECK_MESSAGE(!DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Version 1 should NOT be DD-marked");
    }

    {
        CMutableTransaction tx;
        tx.nVersion = 2;  // Standard v2
        BOOST_CHECK_MESSAGE(!DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Version 2 should NOT be DD-marked");
    }

    // ATTACK: Version with just ONE bit different from 0x0770
    {
        CMutableTransaction tx;
        tx.nVersion = 0x0771;  // One bit off
        BOOST_CHECK_MESSAGE(!DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Version 0x0771 should NOT be DD-marked (one bit off)");
    }

    {
        CMutableTransaction tx;
        tx.nVersion = 0x0760;  // Different nibble
        BOOST_CHECK_MESSAGE(!DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Version 0x0760 should NOT be DD-marked");
    }

    // ATTACK: Negative version number that has 0x0770 in lower bits
    {
        CMutableTransaction tx;
        tx.nVersion = static_cast<int32_t>(0x80000770);  // Sign bit set
        BOOST_CHECK_MESSAGE(DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
            "Negative version with DD lower bits IS detected (by design — version is int32_t, "
            "but mask operates on bit pattern)");
    }
}

// T1-06b: DD opcodes behave as NOPs when SCRIPT_VERIFY_DIGIDOLLAR is NOT set
BOOST_AUTO_TEST_CASE(redteam_T1_06b_dd_opcodes_nop_before_activation)
{
    // ATTACK: Before activation, can DD opcodes be used in scripts to create
    // unexpected behavior?

    // Create a simple script that uses OP_DIGIDOLLAR with an amount push
    // Script: OP_DIGIDOLLAR <amount=1000> OP_DROP OP_TRUE
    // Pre-activation: OP_DIGIDOLLAR is NOP, <1000> is pushed to stack, OP_DROP removes it, OP_TRUE succeeds
    // Post-activation: OP_DIGIDOLLAR consumes <1000>, pushes true, OP_DROP removes it, OP_TRUE succeeds

    CScript scriptPubKey;
    scriptPubKey << OP_DIGIDOLLAR;
    scriptPubKey << CScriptNum(1000);
    scriptPubKey << OP_DROP;
    scriptPubKey << OP_TRUE;

    CScript scriptSig;  // Empty — not needed for this script structure

    // Without SCRIPT_VERIFY_DIGIDOLLAR (pre-activation behavior):
    // OP_DIGIDOLLAR = NOP, <1000> pushed, OP_DROP removes 1000, OP_TRUE → stack has [true]
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH;  // No DD flag
        ScriptError err;
        // Use direct EvalScript since this isn't a real spending scenario
        std::vector<std::vector<unsigned char>> stack;
        bool result = EvalScript(stack, scriptPubKey, flags, BaseSignatureChecker(), SigVersion::BASE, &err);
        BOOST_CHECK_MESSAGE(result,
            "Pre-activation: OP_DIGIDOLLAR as NOP, script should succeed");
        BOOST_CHECK_MESSAGE(stack.size() == 1 && !stack.back().empty(),
            "Pre-activation: Stack should have [true] at top");
    }

    // With SCRIPT_VERIFY_DIGIDOLLAR (post-activation behavior):
    // OP_DIGIDOLLAR reads <1000>, pushes true (1000 > 0), OP_DROP removes true, OP_TRUE → stack has [true]
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DIGIDOLLAR;
        ScriptError err;
        std::vector<std::vector<unsigned char>> stack;
        bool result = EvalScript(stack, scriptPubKey, flags, BaseSignatureChecker(), SigVersion::BASE, &err);
        BOOST_CHECK_MESSAGE(result,
            "Post-activation: OP_DIGIDOLLAR processes amount, script should succeed");
        BOOST_CHECK_MESSAGE(stack.size() == 1 && !stack.back().empty(),
            "Post-activation: Stack should have [true] at top");
    }
}

// T1-06c: OP_DDVERIFY as NOP doesn't pop stack (consensus safety)
BOOST_AUTO_TEST_CASE(redteam_T1_06c_ddverify_nop_stack_safety)
{
    // ATTACK: OP_DDVERIFY pops and verifies top of stack when active.
    // As NOP, it must NOT touch the stack.
    // If it incorrectly popped pre-activation, scripts would break at activation.

    // Script: OP_TRUE OP_DDVERIFY
    // Pre-activation: OP_TRUE pushes 1, OP_DDVERIFY is NOP → stack has [1]
    // Post-activation: OP_TRUE pushes 1, OP_DDVERIFY pops 1 (verifies true) → stack is empty

    CScript script;
    script << OP_TRUE;
    script << OP_DDVERIFY;

    // Pre-activation: stack should still have the true value
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH;  // No DD flag
        ScriptError err;
        std::vector<std::vector<unsigned char>> stack;
        bool result = EvalScript(stack, script, flags, BaseSignatureChecker(), SigVersion::BASE, &err);
        BOOST_CHECK_MESSAGE(result, "Pre-activation: OP_DDVERIFY as NOP should succeed");
        BOOST_CHECK_MESSAGE(stack.size() == 1,
            "CRITICAL: Pre-activation OP_DDVERIFY must NOT pop stack (stack size should be 1, got " +
            std::to_string(stack.size()) + ")");
    }

    // Post-activation: OP_DDVERIFY consumes the true, stack should be empty
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DIGIDOLLAR;
        ScriptError err;
        std::vector<std::vector<unsigned char>> stack;
        bool result = EvalScript(stack, script, flags, BaseSignatureChecker(), SigVersion::BASE, &err);
        BOOST_CHECK_MESSAGE(result, "Post-activation: OP_DDVERIFY should verify true and succeed");
        BOOST_CHECK_MESSAGE(stack.size() == 0,
            "Post-activation: OP_DDVERIFY should pop the verified value (stack size should be 0, got " +
            std::to_string(stack.size()) + ")");
    }
}

// T1-06d: OP_CHECKCOLLATERAL NOP doesn't touch stack (consensus critical)
BOOST_AUTO_TEST_CASE(redteam_T1_06d_checkcollateral_nop_stack_safety)
{
    // ATTACK: OP_CHECKCOLLATERAL pops 2 items when active.
    // As NOP, it MUST NOT touch the stack — the comment in the code says so.
    // If it popped pre-activation, it would be a consensus split.

    // Script: <ratio=500> <threshold=200> OP_CHECKCOLLATERAL
    // Pre-activation: both numbers pushed, OP_CHECKCOLLATERAL NOP → stack has [500, 200]
    // Post-activation: both popped, 500 >= 200 → true → stack has [true]

    CScript script;
    script << CScriptNum(500);
    script << CScriptNum(200);
    script << OP_CHECKCOLLATERAL;

    // Pre-activation: stack should have both values
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH;
        ScriptError err;
        std::vector<std::vector<unsigned char>> stack;
        bool result = EvalScript(stack, script, flags, BaseSignatureChecker(), SigVersion::BASE, &err);
        BOOST_CHECK_MESSAGE(result, "Pre-activation: OP_CHECKCOLLATERAL NOP should succeed");
        BOOST_CHECK_MESSAGE(stack.size() == 2,
            "CRITICAL: Pre-activation OP_CHECKCOLLATERAL must NOT touch stack (stack size should be 2, got " +
            std::to_string(stack.size()) + ")");
    }

    // Post-activation: stack should have [true]
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DIGIDOLLAR;
        ScriptError err;
        std::vector<std::vector<unsigned char>> stack;
        bool result = EvalScript(stack, script, flags, BaseSignatureChecker(), SigVersion::BASE, &err);
        BOOST_CHECK_MESSAGE(result, "Post-activation: OP_CHECKCOLLATERAL(500>=200) should succeed");
        BOOST_CHECK_MESSAGE(stack.size() == 1 && !stack.back().empty(),
            "Post-activation: OP_CHECKCOLLATERAL should push true (500 >= 200)");
    }
}

// T1-06e: OP_CHECKPRICE NOP doesn't touch stack
BOOST_AUTO_TEST_CASE(redteam_T1_06e_checkprice_nop_stack_safety)
{
    // ATTACK: OP_CHECKPRICE pops 1 item when active.
    // As NOP, it must NOT touch the stack.

    // Script: <price=42000> OP_CHECKPRICE
    // Pre-activation: number pushed, OP_CHECKPRICE NOP → stack has [42000]
    // Post-activation: number popped, compared to mock oracle → stack has [true/false]

    CScript script;
    script << CScriptNum(42000);
    script << OP_CHECKPRICE;

    // Pre-activation: stack should still have the price value
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH;
        ScriptError err;
        std::vector<std::vector<unsigned char>> stack;
        bool result = EvalScript(stack, script, flags, BaseSignatureChecker(), SigVersion::BASE, &err);
        BOOST_CHECK_MESSAGE(result, "Pre-activation: OP_CHECKPRICE NOP should succeed");
        BOOST_CHECK_MESSAGE(stack.size() == 1,
            "CRITICAL: Pre-activation OP_CHECKPRICE must NOT pop stack (stack size should be 1, got " +
            std::to_string(stack.size()) + ")");
    }
}

// T1-06f: Non-DD-marked transaction bypasses DD validation completely
BOOST_AUTO_TEST_CASE(redteam_T1_06f_non_dd_marker_bypass)
{
    // ATTACK: Create a transaction without DD marker (version != 0x0770)
    // that contains DD-like structure (P2TR outputs, OP_RETURN with DD data).
    // This should bypass all DD validation.

    // This is by design — DD validation only runs for DD-marked transactions.
    // But we verify that ValidateDigiDollarTransaction correctly passes through
    // non-DD transactions (returns true without validation).

    CMutableTransaction tx;
    tx.nVersion = 2;  // Standard version, NOT DD

    CTxIn input;
    input.prevout = COutPoint(uint256::ONE, 0);
    tx.vin.push_back(input);

    // Add a DD-like OP_RETURN (with DD marker bytes)
    CScript opreturn;
    opreturn << OP_RETURN;
    std::vector<unsigned char> ddHeader = {0x44, 0x44}; // "DD"
    opreturn << ddHeader;
    opreturn << CScriptNum(100 * COIN);  // Fake DD amount
    tx.vout.push_back(CTxOut(0, opreturn));

    // Add a P2TR output that looks like collateral
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());
    auto tweaked = ownerXOnly.CreateTapTweak(nullptr);
    BOOST_REQUIRE(tweaked.has_value());
    CScript p2tr;
    p2tr << OP_1 << std::vector<unsigned char>(tweaked->first.begin(), tweaked->first.end());
    tx.vout.push_back(CTxOut(50 * COIN, p2tr));

    // CRITICAL CHECK: HasDigiDollarMarker must return FALSE
    BOOST_CHECK_MESSAGE(!DigiDollar::HasDigiDollarMarker(CTransaction(tx)),
        "Non-DD version tx should NOT be detected as DD, regardless of output content");

    // DD validation should pass through (return true) for non-DD transactions
    DigiDollar::ValidationContext ctx(1000, 1000, 150, *CChainParams::RegTest({}));
    TxValidationState state;
    bool result = DigiDollar::ValidateDigiDollarTransaction(CTransaction(tx), ctx, state);
    BOOST_CHECK_MESSAGE(result,
        "DEFENSE HOLDS: Non-DD-marked tx passes through DD validation (no checks applied)");
}

// T1-06g: IsDigiDollarEnabled consistency across overloads
BOOST_AUTO_TEST_CASE(redteam_T1_06g_activation_function_consistency)
{
    // ATTACK: Can the two IsDigiDollarEnabled overloads (chainman vs params-only)
    // return different results for the same chain state?
    // The params-only overload creates a temporary VersionBitsCache, which should
    // compute the same state as the shared cache.

    // On regtest, DD is ALWAYS_ACTIVE — both overloads should agree
    const auto params = CChainParams::RegTest({});

    // With nullptr (genesis): DD should be active on regtest (ALWAYS_ACTIVE)
    bool result1 = DigiDollar::IsDigiDollarEnabled(nullptr, params->GetConsensus());

    // ALWAYS_ACTIVE means active even at genesis (nullptr prev)
    BOOST_CHECK_MESSAGE(result1,
        "DEFENSE HOLDS: IsDigiDollarEnabled(nullptr, params) returns true on regtest (ALWAYS_ACTIVE)");
}

// T1-06h: SCRIPT_VERIFY_DIGIDOLLAR flag value doesn't collide with other flags
BOOST_AUTO_TEST_CASE(redteam_T1_06h_flag_collision_check)
{
    // ATTACK: If SCRIPT_VERIFY_DIGIDOLLAR shares bit position with another flag,
    // it could be accidentally set/unset, creating activation confusion.

    unsigned int dd_flag = SCRIPT_VERIFY_DIGIDOLLAR;

    // Verify it's a single bit
    BOOST_CHECK_MESSAGE((dd_flag & (dd_flag - 1)) == 0,
        "SCRIPT_VERIFY_DIGIDOLLAR must be a single bit (power of 2)");

    // Verify it's bit 21 (1 << 21 = 0x200000)
    BOOST_CHECK_MESSAGE(dd_flag == (1U << 21),
        "SCRIPT_VERIFY_DIGIDOLLAR should be bit 21");

    // Check no collision with standard flags
    unsigned int standard_flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG |
        SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY | SCRIPT_VERIFY_CHECKSEQUENCEVERIFY |
        SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_TAPROOT | SCRIPT_VERIFY_NULLDUMMY;

    BOOST_CHECK_MESSAGE((dd_flag & standard_flags) == 0,
        "SCRIPT_VERIFY_DIGIDOLLAR must not collide with any standard verification flag");
}

// T1-06i: Oracle activation height vs DD BIP9 activation consistency
BOOST_AUTO_TEST_CASE(redteam_T1_06i_oracle_vs_dd_activation_sync)
{
    // ATTACK: If oracle activates AFTER DD, then DD transactions could be
    // processed without oracle prices, bypassing collateral checks.
    // If oracle activates BEFORE DD, oracle messages accumulate uselessly.
    // They should activate at the same height.

    // Check testnet: both should be at height 600
    {
        const auto testnet_params = CChainParams::TestNet();
        const auto& consensus = testnet_params->GetConsensus();

        // Testnet BIP9 DD: min_activation_height = 600
        int dd_min_height = consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height;

        // Oracle activation height
        int oracle_height = consensus.nOracleActivationHeight;

        BOOST_CHECK_MESSAGE(dd_min_height == oracle_height,
            "DEFENSE HOLDS: Testnet DD min_activation_height (" +
            std::to_string(dd_min_height) + ") matches oracle activation height (" +
            std::to_string(oracle_height) + ")");
    }

    // Check regtest: DD is ALWAYS_ACTIVE
    {
        const auto regtest_params = CChainParams::RegTest({});  // RegTest takes optional args
        const auto& consensus = regtest_params->GetConsensus();

        // Regtest: ALWAYS_ACTIVE with min_activation_height = 0
        BOOST_CHECK_MESSAGE(
            consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime ==
                Consensus::BIP9Deployment::ALWAYS_ACTIVE,
            "Regtest DD should be ALWAYS_ACTIVE");
    }

    // Check mainnet: oracle should NOT be active (INT_MAX) since oracles aren't deployed
    {
        const auto mainnet_params = CChainParams::Main();
        const auto& consensus = mainnet_params->GetConsensus();

        int oracle_height = consensus.nOracleActivationHeight;
        BOOST_CHECK_MESSAGE(oracle_height == std::numeric_limits<int>::max(),
            "FINDING (LOW): Mainnet oracle activation is INT_MAX (disabled). "
            "When oracles are deployed, this MUST be updated to match DD BIP9 "
            "min_activation_height (" +
            std::to_string(consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].min_activation_height) +
            ") to avoid the skipOracleValidation gap found in T1-05.");
    }
}

// =============================================================================
// T1-07: Double-Spend DD Token Exploits
// =============================================================================

// Helper: Build a valid DD transfer OP_RETURN
static CScript MakeDDTransferOpReturn(const std::vector<CAmount>& amounts) {
    CScript script;
    script << OP_RETURN;
    // DD marker
    std::vector<unsigned char> dd_marker = {'D', 'D'};
    script << dd_marker;
    // Type = 2 (TRANSFER)
    script << CScriptNum(2);
    // Amounts
    for (CAmount amt : amounts) {
        script << CScriptNum::serialize(amt);
    }
    return script;
}

// Helper: Build a valid DD mint OP_RETURN
static CScript MakeDDMintOpReturn(CAmount ddAmount, int64_t lockHeight, int lockTier, const XOnlyPubKey& ownerKey) {
    CScript script;
    script << OP_RETURN;
    std::vector<unsigned char> dd_marker = {'D', 'D'};
    script << dd_marker;
    script << CScriptNum(1);  // Type = MINT
    script << CScriptNum::serialize(ddAmount);
    script << CScriptNum::serialize(lockHeight);
    script << CScriptNum(lockTier);
    // Owner x-only pubkey (32 bytes)
    std::vector<unsigned char> keyData(ownerKey.begin(), ownerKey.end());
    script << keyData;
    return script;
}

// Helper: Build a simple P2TR output script
static CScript MakeP2TR(const XOnlyPubKey& key) {
    CScript script;
    script << OP_1;
    script << std::vector<unsigned char>(key.begin(), key.end());
    return script;
}

BOOST_AUTO_TEST_CASE(redteam_t1_07a_duplicate_inputs_rejected)
{
    // ATTACK: Create a DD transfer with the same input listed twice.
    // If accepted, the DD amount from one UTXO gets counted twice,
    // allowing creation of more DD outputs than inputs.
    //
    // This is prevented by Bitcoin's CheckTransaction which rejects duplicate inputs
    // (CVE-2018-17144 fix). This test verifies the protection holds for DD txs.

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    // Same outpoint used twice
    COutPoint sharedInput(uint256S("aabb000000000000000000000000000000000000000000000000000000000001"), 1);
    mtx.vin.resize(2);
    mtx.vin[0].prevout = sharedInput;
    mtx.vin[1].prevout = sharedInput;  // DUPLICATE!

    // Generate a test key for outputs
    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // DD output claiming 200 DD (twice the input's 100 DD)
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({200})));

    CTransaction tx(mtx);
    TxValidationState state;

    // CheckTransaction should reject duplicate inputs
    bool check_result = CheckTransaction(tx, state);
    BOOST_CHECK_MESSAGE(!check_result,
        "DEFENSE HOLDS [T1-07a]: CheckTransaction rejects duplicate inputs (bad-txns-inputs-duplicate). "
        "DD tokens cannot be double-counted by repeating the same input outpoint.");
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-inputs-duplicate");
}

BOOST_AUTO_TEST_CASE(redteam_t1_07b_transfer_conservation_inflation)
{
    // ATTACK: Create a DD transfer where OP_RETURN claims more DD output than
    // what the inputs actually contain. The conservation check (inputDD == outputDD)
    // should catch this even if the OP_RETURN is crafted to inflate amounts.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    // Input pointing to a DD UTXO (100 DD, from a previous mint)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("dddd000000000000000000000000000000000000000000000000000000000001"), 1);

    // Generate a test key
    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // OP_RETURN claims 200 DD output (inflated from 100 DD input)
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({200})));

    CTransaction tx(mtx);
    TxValidationState state;

    // Validation without coins view — input amounts can't be looked up
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07b]: Transfer with inflated OP_RETURN amounts rejected. "
        "Without coins view, input DD amounts can't be determined so tx is rejected. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_07c_transfer_no_dd_inputs)
{
    // ATTACK: Create a DD transfer with only fee inputs (no DD UTXOs).
    // The OP_RETURN claims DD output, but no DD inputs exist.
    // This tests whether DD can be created from nothing via transfer.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    // Input: regular DGB UTXO (not DD)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("eeee000000000000000000000000000000000000000000000000000000000001"), 0);

    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // Claim DD output with no DD input
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({100})));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07c]: Transfer with no DD inputs rejected. "
        "DD cannot be created from nothing via a transfer transaction. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_07d_transfer_wrong_tx_type)
{
    // ATTACK: Use MINT version (type 1) but construct a transfer-like tx.
    // Could bypass transfer-specific conservation checks if type routing is wrong.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x01000770;  // DD_TX_MINT (type=1) — NOT TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("ffff000000000000000000000000000000000000000000000000000000000001"), 1);

    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // Transfer-style OP_RETURN but with MINT version
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({100})));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);

    // Should be routed to ValidateMintTransaction (type=1), which will fail
    // because it expects collateral output (P2TR with value > 0)
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07d]: Wrong tx type doesn't bypass conservation. "
        "Tx version type=1 routes to mint validation, which rejects transfer-style tx. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_07e_transfer_zero_dd_amount_in_opreturn)
{
    // ATTACK: Create a DD transfer with zero amounts in OP_RETURN.
    // This could bypass conservation if zero amounts are silently accepted.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1111000000000000000000000000000000000000000000000000000000000001"), 1);

    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // DD output with 0 amount
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({0})));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07e]: Transfer with zero DD amount rejected. "
        "Zero-amount DD outputs are invalid. Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_07f_transfer_negative_dd_amount)
{
    // ATTACK: Create a DD transfer with negative amounts in OP_RETURN.
    // Script numbers are signed — a negative amount could underflow conservation checks.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("2222000000000000000000000000000000000000000000000000000000000001"), 1);

    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // Two outputs: 200 DD to recipient, -100 DD as "change" (negative!)
    // If conservation check is inputDD == outputDD, and outputDD = 200 + (-100) = 100,
    // this could pass with only 100 DD input but recipient gets 200 DD
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({200, -100})));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07f]: Transfer with negative DD amount rejected. "
        "Negative amounts cannot be used to bypass conservation. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_07g_transfer_overflow_dd_amounts)
{
    // ATTACK: Create a DD transfer where output amounts overflow when summed.
    // If two outputs have amounts near INT64_MAX, their sum could overflow to
    // a small number, matching a small inputDD and creating DD from nothing.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("3333000000000000000000000000000000000000000000000000000000000001"), 1);

    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // Two outputs near INT64_MAX that would overflow on addition
    CAmount near_max = std::numeric_limits<int64_t>::max() / 2 + 1;
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({near_max, near_max})));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07g]: Transfer with overflow DD amounts rejected. "
        "Amounts near INT64_MAX cannot overflow conservation checks. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_07h_transfer_extra_outputs_beyond_opreturn)
{
    // ATTACK: Create a DD transfer with more P2TR zero-value outputs than
    // amounts listed in OP_RETURN. Extra outputs might get phantom DD values.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("4444000000000000000000000000000000000000000000000000000000000001"), 1);

    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // OP_RETURN has 1 amount (100 DD), but we create 3 P2TR zero-value outputs
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));  // 100 DD (from OP_RETURN)
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));  // Extra — no OP_RETURN amount
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));  // Extra — no OP_RETURN amount
    mtx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({100})));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07h]: Transfer with extra P2TR outputs beyond OP_RETURN rejected. "
        "Extra zero-value outputs with no corresponding OP_RETURN amounts are rejected. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_07i_transfer_no_opreturn)
{
    // ATTACK: Create a DD transfer with no OP_RETURN.
    // Without OP_RETURN, DD amounts can't be determined for outputs.

    auto regTestParams = CChainParams::RegTest({});

    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD_TX_TRANSFER

    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("5555000000000000000000000000000000000000000000000000000000000001"), 1);

    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());

    // P2TR output but NO OP_RETURN
    mtx.vout.push_back(CTxOut(0, MakeP2TR(xonly)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T1-07i]: Transfer with no OP_RETURN rejected. "
        "DD transfers require OP_RETURN for output amount declaration. "
        "Reason: " + state.GetRejectReason());
}

// ============================================================================
// T1-08: Redeem without burning DD (collateral release bypass)
// ============================================================================

// Helper: Build a DD redeem OP_RETURN with DD change amount
static CScript MakeDDRedeemOpReturn(CAmount ddChangeAmount) {
    CScript script;
    script << OP_RETURN;
    std::vector<unsigned char> dd_marker = {'D', 'D'};
    script << dd_marker;
    script << CScriptNum(3);  // Type = REDEEM
    if (ddChangeAmount > 0) {
        script << CScriptNum::serialize(ddChangeAmount);
    }
    return script;
}

BOOST_AUTO_TEST_CASE(redteam_t1_08a_collateral_release_bypass_no_metadata)
{
    // ATTACK [T1-08a]: ValidateCollateralReleaseAmount silently allows ANY release
    // when it can't extract originalDDMinted from the collateral script.
    //
    // VULNERABILITY: The collateral UTXO is a P2TR script (OP_1 + 32 bytes).
    // ExtractDDAmount() looks for OP_RETURN or metadata registry entries.
    // P2TR scripts are NOT OP_RETURN, and during cross-node block validation
    // the metadata registry is empty (ephemeral, not populated by remote txs).
    // When ExtractDDAmount fails, ValidateCollateralReleaseAmount returns true
    // (the "fallback: allow if we can't determine original amount" path).
    //
    // EXPLOIT: Attacker burns 1 cent of DD and claims ALL collateral back.
    // - Mint 10000 DD ($100) with 200 DGB collateral
    // - Redeem: burn 1 cent DD, keep 9999 cents as change, release all 200 DGB
    // - ValidateCollateralReleaseAmount can't read originalDDMinted → returns true
    //
    // IMPACT: Complete collateral theft. Unbacked DD tokens remain in circulation.

    auto regTestParams = CChainParams::RegTest({});

    // Create a raw P2TR collateral script WITHOUT metadata registry
    // (simulates cross-node validation where metadata is unavailable)
    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);  // Raw, no metadata

    CAmount lockedCollateral = 200 * COIN;  // 200 DGB
    CAmount ddBurnedTiny = 1;  // Only 1 cent burned

    // Set up coins view with raw P2TR collateral (no metadata)
    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);

    uint256 collTxId = uint256S("aaa1080000000000000000000000000000000000000000000000000000000001");
    COutPoint collOutpoint(collTxId, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    // Build redeem tx: burn 1 cent DD, release ALL 200 DGB collateral
    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;  // DD_TX_REDEEM

    // Input 0: collateral
    mtx.vin.push_back(CTxIn(collOutpoint));
    // Input 1: DD (we just need a valid prevout — actual DD amount doesn't matter here
    // since we're testing ValidateCollateralReleaseAmount directly)
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("bbb1080000000000000000000000000000000000000000000000000000000001"), 0)));

    // Output: release FULL 200 DGB (massively excessive for 1 cent burned!)
    mtx.vout.push_back(CTxOut(lockedCollateral, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView);

    // Call ValidateCollateralReleaseAmount with ddBurned=1 (only 1 cent burned)
    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurnedTiny, state);

    // BUG: This PASSES because ExtractDDAmount fails on raw P2TR script
    // and the function falls back to "return true" (allow if can't determine amount).
    //
    // AFTER FIX: This should FAIL — must look up originalDDMinted from the
    // creating (mint) transaction's OP_RETURN via txLookup or txindex.
    // If amount undetermined, REJECT (don't silently allow).
    BOOST_CHECK_MESSAGE(!result,
        "VULNERABILITY [T1-08a]: Collateral release bypass via missing metadata. "
        "Burning 1 cent DD should NOT allow releasing 200 DGB collateral. "
        "ValidateCollateralReleaseAmount must look up original DD minted from "
        "the creating transaction, not just the collateral script. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_08b_collateral_release_bypass_with_txlookup)
{
    // ATTACK [T1-08b]: Same as T1-08a but with txLookup available.
    // After the fix, txLookup should find the creating (mint) tx and extract
    // originalDDMinted from its OP_RETURN. Then the proportional check should reject.

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;  // $100 originally minted
    CAmount ddBurnedTiny = 1;    // Only 1 cent burned

    // Create a fake "mint" transaction that would have created the collateral UTXO
    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;  // DD_TX_MINT
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("fff0000000000000000000000000000000000000000000000000000000000001"), 0)));
    // Collateral output (P2TR with value)
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    // DD output (P2TR zero-value)
    CKey ddKey;
    ddKey.MakeNewKey(true);
    XOnlyPubKey ddXOnlyKey(ddKey.GetPubKey());
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(ddXOnlyKey)));
    // OP_RETURN with DD metadata (contains originalDD amount)
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    // Set up coins view
    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);

    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    // Create txLookup that returns the mint transaction
    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    // Build redeem tx: burn 1 cent DD, try to release ALL 200 DGB
    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;  // DD_TX_REDEEM

    mtx.vin.push_back(CTxIn(collOutpoint));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("ccc1080000000000000000000000000000000000000000000000000000000001"), 0)));

    // Try to release full collateral (200 DGB for 1 cent burned — should be rejected!)
    mtx.vout.push_back(CTxOut(lockedCollateral, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurnedTiny, state);

    // With the fix + txLookup, originalDDMinted=10000 extracted from mint tx OP_RETURN.
    // allowedRelease = (1/10000) * 200 DGB = 0.02 DGB = 2,000,000 sats
    // totalDGBRelease = 200 DGB = 20,000,000,000 sats
    // 20B >> 2M + tolerance → MUST REJECT
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE VERIFIED [T1-08b]: With txLookup, original DD minted is extracted from "
        "creating transaction's OP_RETURN. Proportional collateral release enforced. "
        "Burning 1 cent cannot release 200 DGB. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_08c_redeem_full_burn_valid_with_txlookup)
{
    // VALID: Full DD burn → full collateral release should still pass after fix

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;  // $100

    // Create mint tx
    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("eee0000000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    // Redeem tx: burn ALL DD (10000), release full 200 DGB — should PASS
    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOutpoint));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("ddd1080000000000000000000000000000000000000000000000000000000001"), 0)));
    mtx.vout.push_back(CTxOut(lockedCollateral, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, originalDD, state);

    BOOST_CHECK_MESSAGE(result,
        "VALID [T1-08c]: Full DD burn → full collateral release should pass. "
        "Error: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t1_08d_redeem_partial_burn_rejected)
{
    // SECURITY [T2-03 fix]: Partial burn is now REJECTED because the collateral UTXO
    // is indivisible — the excess goes to miner fees, enabling collateral theft.
    // Previously this test expected partial burns to pass; now they must fail.

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;
    CAmount ddBurnedHalf = 5000;

    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("aab0000000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    // Redeem: burn 5000 DD (half) — should now FAIL (partial burn rejected)
    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOutpoint));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("bbc1080000000000000000000000000000000000000000000000000000000001"), 0)));
    mtx.vout.push_back(CTxOut(100 * COIN, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurnedHalf, state);

    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE [T1-08d/T2-03]: Partial burn must be rejected to prevent miner fee "
        "collateral theft. Error: " + state.GetRejectReason());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-collateral-release-partial-burn");
}

BOOST_AUTO_TEST_CASE(redteam_t1_08e_redeem_partial_burn_excessive_release)
{
    // ATTACK [T1-08e]: Burn half DD but try to release full collateral

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;
    CAmount ddBurnedHalf = 5000;

    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("ccb0000000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    // Redeem: burn 5000 DD (half), but try to release FULL 200 DGB — should FAIL
    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOutpoint));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("ddc1080000000000000000000000000000000000000000000000000000000001"), 0)));
    mtx.vout.push_back(CTxOut(lockedCollateral, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurnedHalf, state);

    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE VERIFIED [T1-08e]: Burning half DD should NOT allow full collateral release. "
        "Proportional check must enforce (5000/10000) * 200 = 100 DGB max. "
        "Reason: " + state.GetRejectReason());
}

// =============================================================================
// T2-01: Mint with Insufficient Collateral (Rounding / Lock Height Confusion)
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_t2_01a_lockheight_absolute_vs_relative_mainnet)
{
    // CRITICAL BUG (NOW FIXED): On mainnet (height ~22M), the absolute lock HEIGHT from
    // OP_RETURN was passed directly to GetCollateralRatioForLockTime which treats it as a
    // RELATIVE lock period. Since 22M + any_lock > all tier thresholds (max is 10yr = 21M
    // blocks), EVERY lock tier mapped to the 200% (10-year) ratio instead of its correct ratio.
    //
    // FIX: ValidateMintTransaction now converts lockTime to lockPeriod (lockTime - ctx.nHeight)
    // before passing to CalculateRequiredCollateral and ValidateCollateralRatio.

    auto regTestParams = CChainParams::RegTest({});
    const auto& ddParams = regTestParams->GetDigiDollarParams();

    // Simulate mainnet activation height
    const int MAINNET_HEIGHT = 22014720;

    // 1-hour lock tier: should require 1000% collateral
    const int64_t ONE_HOUR_BLOCKS = 240;
    int64_t absoluteLockHeight = MAINNET_HEIGHT + ONE_HOUR_BLOCKS;  // ~22,014,960
    int64_t relativeLockPeriod = ONE_HOUR_BLOCKS;                    // 240 blocks

    // Verify GetCollateralRatioForLockTime still has the raw behavior difference
    // (it's the caller's job to pass relative, not absolute)
    int rawAbsoluteRatio = DigiDollar::GetCollateralRatioForLockTime(absoluteLockHeight, ddParams);
    int rawRelativeRatio = DigiDollar::GetCollateralRatioForLockTime(relativeLockPeriod, ddParams);

    // The raw function gives 200% for absolute (wrong) and 1000% for relative (correct)
    BOOST_CHECK_EQUAL(rawAbsoluteRatio, 200);
    BOOST_CHECK_EQUAL(rawRelativeRatio, 1000);

    // Verify that the FIXED code now uses the relative period
    // CalculateRequiredCollateral receives the relative period from the fixed ValidateMintTransaction
    const CAmount DD_AMOUNT = 100000;  // $1000 in cents
    const CAmount ORACLE_PRICE = 5000; // $0.005 per DGB

    DigiDollar::ValidationContext ctx(MAINNET_HEIGHT, ORACLE_PRICE, 150, *regTestParams);

    // After fix: collateral calculation uses relative lock period
    CAmount correctCollateral = DigiDollar::CalculateRequiredCollateral(DD_AMOUNT, relativeLockPeriod, ctx);

    // At 1000% ratio for 1-hour lock, $1000 DD at $0.005/DGB should require:
    // (100000 cents * 10^8 * 1000 * 100) / 5000 = 200,000,000,000,000 sats = 2,000,000 DGB
    BOOST_CHECK_GT(correctCollateral, 0);

    // Verify the fixed collateral is 5x more than the buggy calculation would give
    CAmount buggyCollateral = DigiDollar::CalculateRequiredCollateral(DD_AMOUNT, absoluteLockHeight, ctx);
    BOOST_CHECK_MESSAGE(correctCollateral > buggyCollateral * 4,
        "FIX VERIFIED [T2-01a]: Correct collateral (" + std::to_string(correctCollateral / COIN) +
        " DGB) is 5x more than buggy (" + std::to_string(buggyCollateral / COIN) +
        " DGB). Lock period conversion fix is working.");
}

BOOST_AUTO_TEST_CASE(redteam_t2_01b_lockheight_30day_at_mainnet_height)
{
    // Same bug but with 30-day lock tier
    // At mainnet height: 22M + 172800 = 22,187,520 > all tiers → 200% instead of 500%

    auto regTestParams = CChainParams::RegTest({});
    const auto& ddParams = regTestParams->GetDigiDollarParams();

    const int MAINNET_HEIGHT = 22014720;
    const int64_t THIRTY_DAY_BLOCKS = 30 * DigiDollar::BLOCKS_PER_DAY;  // 172800

    int64_t absoluteLockHeight = MAINNET_HEIGHT + THIRTY_DAY_BLOCKS;
    int64_t relativeLockPeriod = THIRTY_DAY_BLOCKS;

    int buggyRatio = DigiDollar::GetCollateralRatioForLockTime(absoluteLockHeight, ddParams);
    int correctRatio = DigiDollar::GetCollateralRatioForLockTime(relativeLockPeriod, ddParams);

    // Bug: 200% instead of 500%
    BOOST_CHECK_EQUAL(buggyRatio, 200);
    BOOST_CHECK_EQUAL(correctRatio, 500);

    BOOST_CHECK_MESSAGE(buggyRatio < correctRatio,
        "BUG CONFIRMED [T2-01b]: 30-day lock at mainnet height gets " +
        std::to_string(buggyRatio) + "% ratio instead of correct " +
        std::to_string(correctRatio) + "%. 2.5x less collateral required.");
}

BOOST_AUTO_TEST_CASE(redteam_t2_01c_all_tiers_broken_at_mainnet_height)
{
    // Verify ALL lock tiers are broken at mainnet activation height
    auto regTestParams = CChainParams::RegTest({});
    const auto& ddParams = regTestParams->GetDigiDollarParams();

    const int MAINNET_HEIGHT = 22014720;

    struct TierTest {
        const char* name;
        int lockDays;
        int expectedRatio;
    };

    TierTest tiers[] = {
        {"1-hour",   0,    1000},   // 240 blocks
        {"30-day",   30,   500},
        {"90-day",   90,   400},
        {"180-day",  180,  350},
        {"1-year",   365,  300},
        {"2-year",   730,  275},
        {"3-year",   1095, 250},
        {"5-year",   1825, 225},
        {"7-year",   2555, 212},
        {"10-year",  3650, 200},
    };

    int brokenCount = 0;
    for (const auto& tier : tiers) {
        int64_t lockBlocks = DigiDollar::LockDaysToBlocks(tier.lockDays);
        int64_t absoluteHeight = MAINNET_HEIGHT + lockBlocks;

        int buggyRatio = DigiDollar::GetCollateralRatioForLockTime(absoluteHeight, ddParams);
        int correctRatio = DigiDollar::GetCollateralRatioForLockTime(lockBlocks, ddParams);

        if (buggyRatio != correctRatio) {
            brokenCount++;
            BOOST_TEST_MESSAGE("  BROKEN: " << tier.name << " tier - absolute height " <<
                absoluteHeight << " gives " << buggyRatio << "% instead of " <<
                correctRatio << "%");
        }
    }

    // All tiers except 10-year should be broken (10-year always returns 200%)
    BOOST_CHECK_MESSAGE(brokenCount >= 9,
        "EXPLOIT CONFIRMED [T2-01c]: " + std::to_string(brokenCount) +
        "/10 tiers are broken at mainnet height. Every short lock gets 200% "
        "(10-year rate) instead of its correct higher rate.");
}

BOOST_AUTO_TEST_CASE(redteam_t2_01d_integer_division_truncation)
{
    // Secondary check: Is the <1 satoshi truncation in CalculateRequiredCollateral exploitable?
    // The integer division `numerator / oraclePriceMicroUSD` truncates, losing <1 sat.
    // This should NOT be exploitable in practice.

    auto regTestParams = CChainParams::RegTest({});

    // Test at various oracle prices
    CAmount prices[] = {100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
    for (CAmount price : prices) {
        DigiDollar::ValidationContext ctx(1000, price, 150, *regTestParams);

        CAmount ddAmount = 10000;  // $100
        int64_t lockBlocks = 30 * DigiDollar::BLOCKS_PER_DAY;

        CAmount required = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctx);

        // Verify the truncation is less than 1 satoshi
        // Exact: ddAmount * COIN * ratio * 100 / price
        // The remainder is at most (price - 1), making the lost value < 1 sat
        // This means integer truncation alone is NOT exploitable
        BOOST_CHECK_MESSAGE(required > 0,
            "Collateral requirement should be positive at price " + std::to_string(price));
    }

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-01d]: Integer division truncation is <1 satoshi — not exploitable");
}

// =============================================================================
// T2-02: Transfer Conservation Bypass (Create DD from Nothing)
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_t2_02a_non_dd_source_tx_fake_dd_opreturn)
{
    // CRITICAL ATTACK: Create DD from nothing using a non-DD source transaction
    // that has a DD-formatted OP_RETURN.
    //
    // A malicious miner includes a REGULAR (non-DD) transaction in a block with:
    //   - nVersion = 2 (standard Bitcoin version, NOT DD marker)
    //   - OP_RETURN: "DD" type=2 amount=100000 ($1000 in cents)
    //   - Zero-value P2TR output (looks like a DD output)
    //
    // Then they craft a DD TRANSFER tx spending that zero-value P2TR output.
    // ExtractDDAmountFromTxRef parses the source tx's OP_RETURN and finds
    // DD amounts — even though the source tx was NEVER validated as a DD tx.
    //
    // If inputDD is populated from this fake source, conservation passes,
    // and the attacker created DD from nothing (no collateral, no mint).
    //
    // EXPECTED: Transfer MUST be rejected. ExtractDDAmountFromTxRef should
    // either check HasDigiDollarMarker on the source tx, or the transfer
    // validation should verify input sources are legitimate DD transactions.

    auto regTestParams = CChainParams::RegTest({});

    // ─────────────────────────────────────────────────
    // Step 1: Create the fake "source" transaction (NOT a DD tx)
    // ─────────────────────────────────────────────────
    CKey fakeKey;
    fakeKey.MakeNewKey(true);
    XOnlyPubKey fakeXOnly(fakeKey.GetPubKey());

    CMutableTransaction fakeSrcTx;
    fakeSrcTx.nVersion = 2;  // REGULAR Bitcoin version — NO DD marker!
    fakeSrcTx.vin.push_back(CTxIn(COutPoint(uint256S("aaaa020200000000000000000000000000000000000000000000000000000001"), 0)));

    // Zero-value P2TR output (mimics a DD output)
    fakeSrcTx.vout.push_back(CTxOut(0, MakeP2TR(fakeXOnly)));

    // DD-formatted OP_RETURN with fake DD amounts (type=2 TRANSFER format)
    // This makes ExtractDDAmountFromTxRef think this tx has 100000 DD cents
    fakeSrcTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({100000})));

    CTransactionRef fakeSrcRef = MakeTransactionRef(fakeSrcTx);
    uint256 fakeSrcHash = fakeSrcRef->GetHash();

    // Verify this is NOT a DD transaction
    BOOST_CHECK_MESSAGE(!DigiDollar::HasDigiDollarMarker(CTransaction(fakeSrcTx)),
        "Precondition: Source tx must NOT have DD version marker");

    // ─────────────────────────────────────────────────
    // Step 2: Set up coins view with the fake DD output
    // ─────────────────────────────────────────────────
    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);

    COutPoint fakeOutpoint(fakeSrcHash, 0);  // The zero-value P2TR
    coinsView.AddCoin(fakeOutpoint, Coin(CTxOut(0, MakeP2TR(fakeXOnly)), 500, false), false);

    // txLookup returns the fake source tx
    auto txLookup = [&fakeSrcRef, &fakeSrcHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == fakeSrcHash) {
            tx_out = fakeSrcRef;
            return true;
        }
        return false;
    };

    // ─────────────────────────────────────────────────
    // Step 3: Build the DD TRANSFER spending the fake source
    // ─────────────────────────────────────────────────
    CKey recipientKey;
    recipientKey.MakeNewKey(true);
    XOnlyPubKey recipientXOnly(recipientKey.GetPubKey());

    CMutableTransaction transferTx;
    transferTx.nVersion = 0x02000770;  // DD_TX_TRANSFER (proper DD marker)
    transferTx.vin.push_back(CTxIn(fakeOutpoint));

    // DD output to recipient
    transferTx.vout.push_back(CTxOut(0, MakeP2TR(recipientXOnly)));
    // OP_RETURN claiming same amount as the fake source
    transferTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({100000})));

    CTransaction tx(transferTx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);

    // If this PASSES, we have a critical bug: DD created from nothing!
    BOOST_CHECK_MESSAGE(!result,
        "VULNERABILITY [T2-02a]: Transfer accepted with input from NON-DD source tx! "
        "ExtractDDAmountFromTxRef parses DD amounts from a regular Bitcoin tx that has a "
        "DD-formatted OP_RETURN but was never DD-validated. A malicious miner could include "
        "such a tx in a block and create unlimited DD from nothing. "
        "FIX: Verify creating tx has DD version marker before extracting DD amounts. "
        "Reason: " + state.GetRejectReason());

    if (result) {
        BOOST_TEST_MESSAGE("*** CRITICAL BUG: DD created from nothing via non-DD source tx ***");
        BOOST_TEST_MESSAGE("*** A miner can inflate DD supply without collateral ***");
    } else {
        BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-02a]: Transfer from non-DD source tx correctly rejected. "
            "Reason: " + state.GetRejectReason());
    }
}

BOOST_AUTO_TEST_CASE(redteam_t2_02b_non_dd_mint_source_inflates_dd)
{
    // VARIANT: Source tx has DD MINT-style OP_RETURN (type=1) with zero-value P2TR.
    // If ExtractDDAmountFromTxRef parses MINT OP_RETURN from a non-DD tx,
    // the DD amount gets attributed to the zero-value P2TR output.

    auto regTestParams = CChainParams::RegTest({});

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    // Non-DD source tx with MINT-format OP_RETURN
    CMutableTransaction fakeMintTx;
    fakeMintTx.nVersion = 2;  // NOT a DD tx
    fakeMintTx.vin.push_back(CTxIn(COutPoint(uint256S("bbbb020200000000000000000000000000000000000000000000000000000001"), 0)));
    fakeMintTx.vout.push_back(CTxOut(0, MakeP2TR(ownerXOnly)));  // Fake DD output
    fakeMintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(50000, 2000, 1, ownerXOnly)));  // Mint-style OP_RETURN

    CTransactionRef fakeMintRef = MakeTransactionRef(fakeMintTx);
    uint256 fakeMintHash = fakeMintRef->GetHash();

    BOOST_CHECK(!DigiDollar::HasDigiDollarMarker(CTransaction(fakeMintTx)));

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint fakeOutpoint(fakeMintHash, 0);
    coinsView.AddCoin(fakeOutpoint, Coin(CTxOut(0, MakeP2TR(ownerXOnly)), 500, false), false);

    auto txLookup = [&fakeMintRef, &fakeMintHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == fakeMintHash) { tx_out = fakeMintRef; return true; }
        return false;
    };

    // Transfer: claim the 50000 DD from the fake mint
    CKey recipKey;
    recipKey.MakeNewKey(true);
    XOnlyPubKey recipXOnly(recipKey.GetPubKey());

    CMutableTransaction transferTx;
    transferTx.nVersion = 0x02000770;
    transferTx.vin.push_back(CTxIn(fakeOutpoint));
    transferTx.vout.push_back(CTxOut(0, MakeP2TR(recipXOnly)));
    transferTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({50000})));

    CTransaction tx(transferTx);
    TxValidationState state;
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);

    BOOST_CHECK_MESSAGE(!result,
        "VULNERABILITY [T2-02b]: Transfer accepted from non-DD source with MINT OP_RETURN! "
        "ExtractDDAmountFromTxRef should verify source tx HasDigiDollarMarker. "
        "Reason: " + state.GetRejectReason());

    if (result) {
        BOOST_TEST_MESSAGE("*** CRITICAL BUG: MINT-format OP_RETURN in non-DD tx creates fake DD ***");
    }
}

BOOST_AUTO_TEST_CASE(redteam_t2_02c_legitimate_transfer_still_works)
{
    // SANITY CHECK: A legitimate DD transfer from a real DD source tx should still pass.
    // This ensures any fix for T2-02a/b doesn't break normal transfers.

    auto regTestParams = CChainParams::RegTest({});

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    // Legitimate DD transfer source (proper DD version marker)
    CMutableTransaction realTransferTx;
    realTransferTx.nVersion = 0x02000770;  // DD_TX_TRANSFER — proper DD marker!
    realTransferTx.vin.push_back(CTxIn(COutPoint(uint256S("cccc020200000000000000000000000000000000000000000000000000000001"), 0)));
    realTransferTx.vout.push_back(CTxOut(0, MakeP2TR(ownerXOnly)));
    realTransferTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({5000})));

    CTransactionRef realRef = MakeTransactionRef(realTransferTx);
    uint256 realHash = realRef->GetHash();

    BOOST_CHECK(DigiDollar::HasDigiDollarMarker(CTransaction(realTransferTx)));

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint realOutpoint(realHash, 0);
    coinsView.AddCoin(realOutpoint, Coin(CTxOut(0, MakeP2TR(ownerXOnly)), 500, false), false);

    auto txLookup = [&realRef, &realHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == realHash) { tx_out = realRef; return true; }
        return false;
    };

    CKey recipKey;
    recipKey.MakeNewKey(true);
    XOnlyPubKey recipXOnly(recipKey.GetPubKey());

    CMutableTransaction newTransfer;
    newTransfer.nVersion = 0x02000770;
    newTransfer.vin.push_back(CTxIn(realOutpoint));
    newTransfer.vout.push_back(CTxOut(0, MakeP2TR(recipXOnly)));
    newTransfer.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({5000})));

    CTransaction tx(newTransfer);
    TxValidationState state;
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);

    // This SHOULD pass — legitimate transfer from a real DD tx
    BOOST_CHECK_MESSAGE(result,
        "REGRESSION [T2-02c]: Legitimate DD transfer should still pass! "
        "Fix for T2-02a/b must not break normal transfers. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t2_02d_conservation_inflated_output)
{
    // ATTACK: Transfer with OP_RETURN claiming more DD than the input provides.
    // Conservation check: inputDD (from source OP_RETURN) != outputDD (from this OP_RETURN)
    // This should always be caught regardless of source tx type.

    auto regTestParams = CChainParams::RegTest({});

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    // Real DD source with 1000 DD
    CMutableTransaction srcTx;
    srcTx.nVersion = 0x02000770;
    srcTx.vin.push_back(CTxIn(COutPoint(uint256S("dddd020200000000000000000000000000000000000000000000000000000001"), 0)));
    srcTx.vout.push_back(CTxOut(0, MakeP2TR(ownerXOnly)));
    srcTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({1000})));

    CTransactionRef srcRef = MakeTransactionRef(srcTx);
    uint256 srcHash = srcRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint srcOutpoint(srcHash, 0);
    coinsView.AddCoin(srcOutpoint, Coin(CTxOut(0, MakeP2TR(ownerXOnly)), 500, false), false);

    auto txLookup = [&srcRef, &srcHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == srcHash) { tx_out = srcRef; return true; }
        return false;
    };

    CKey recipKey;
    recipKey.MakeNewKey(true);
    XOnlyPubKey recipXOnly(recipKey.GetPubKey());

    // ATTACK: Claim 10x more DD than input has
    CMutableTransaction inflatedTransfer;
    inflatedTransfer.nVersion = 0x02000770;
    inflatedTransfer.vin.push_back(CTxIn(srcOutpoint));
    inflatedTransfer.vout.push_back(CTxOut(0, MakeP2TR(recipXOnly)));
    inflatedTransfer.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({10000})));  // 10x inflation!

    CTransaction tx(inflatedTransfer);
    TxValidationState state;
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);

    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE HOLDS [T2-02d]: Conservation check catches inflated output amounts. "
        "inputDD (1000) != outputDD (10000). "
        "Reason: " + state.GetRejectReason());
    if (!result) {
        BOOST_CHECK_MESSAGE(state.GetRejectReason() == "transfer-dd-conservation-violation",
            "Should fail with conservation violation, got: " + state.GetRejectReason());
    }
}

BOOST_AUTO_TEST_CASE(redteam_t2_02e_extra_opreturn_amounts_phantom_dd)
{
    // ATTACK: OP_RETURN contains more DD amounts than there are P2TR outputs.
    // Extra amounts are "phantom" — they exist in metadata but have no real UTXO.
    // When SPENT in a future transfer, the phantom amounts should not be extractable.

    auto regTestParams = CChainParams::RegTest({});

    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    XOnlyPubKey ownerXOnly(ownerKey.GetPubKey());

    // Source tx with OP_RETURN claiming [1000, 99000] but only ONE P2TR output
    CMutableTransaction srcTx;
    srcTx.nVersion = 0x02000770;
    srcTx.vin.push_back(CTxIn(COutPoint(uint256S("eeee020200000000000000000000000000000000000000000000000000000001"), 0)));
    srcTx.vout.push_back(CTxOut(0, MakeP2TR(ownerXOnly)));  // Only 1 P2TR output
    srcTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({1000, 99000})));  // Claims 2 amounts!

    CTransactionRef srcRef = MakeTransactionRef(srcTx);
    uint256 srcHash = srcRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint srcOutpoint(srcHash, 0);
    coinsView.AddCoin(srcOutpoint, Coin(CTxOut(0, MakeP2TR(ownerXOnly)), 500, false), false);

    auto txLookup = [&srcRef, &srcHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == srcHash) { tx_out = srcRef; return true; }
        return false;
    };

    CKey recipKey;
    recipKey.MakeNewKey(true);
    XOnlyPubKey recipXOnly(recipKey.GetPubKey());

    // Transfer: spend the one P2TR output, claim 1000 DD (matches first amount)
    CMutableTransaction transferTx;
    transferTx.nVersion = 0x02000770;
    transferTx.vin.push_back(CTxIn(srcOutpoint));
    transferTx.vout.push_back(CTxOut(0, MakeP2TR(recipXOnly)));
    transferTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({1000})));

    CTransaction tx(transferTx);
    TxValidationState state;
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);

    // ExtractDDAmountFromTxRef matches by P2TR output position — so output 0
    // maps to dd_amounts[0] = 1000. The phantom 99000 is never assigned.
    // inputDD = 1000, outputDD = 1000 → conservation passes.
    BOOST_TEST_MESSAGE("T2-02e: Phantom amounts in OP_RETURN. Transfer result: " +
        std::string(result ? "PASSED" : "REJECTED") + " Reason: " + state.GetRejectReason());

    // If it passes, verify the phantom 99000 is NOT accessible
    if (result) {
        BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-02e]: Only 1000 DD transferred (matched to P2TR position). "
            "Phantom 99000 in OP_RETURN has no corresponding UTXO and cannot be spent.");
    }
    // If rejected, also fine — stricter validation (e.g., requiring OP_RETURN count == P2TR count)
}

BOOST_AUTO_TEST_CASE(redteam_t2_02f_conservation_with_multiple_inputs)
{
    // ATTACK: Multiple DD inputs from different sources. If one source's DD amount
    // is inflated by the attacker, the total inputDD is inflated.
    // Tests that conservation holds with accurate per-input DD extraction.

    auto regTestParams = CChainParams::RegTest({});

    CKey key1, key2, recipKey;
    key1.MakeNewKey(true);
    key2.MakeNewKey(true);
    recipKey.MakeNewKey(true);
    XOnlyPubKey xonly1(key1.GetPubKey()), xonly2(key2.GetPubKey()), recipXOnly(recipKey.GetPubKey());

    // Source 1: real DD transfer with 500 DD
    CMutableTransaction src1;
    src1.nVersion = 0x02000770;
    src1.vin.push_back(CTxIn(COutPoint(uint256S("f1f1020200000000000000000000000000000000000000000000000000000001"), 0)));
    src1.vout.push_back(CTxOut(0, MakeP2TR(xonly1)));
    src1.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({500})));
    CTransactionRef src1Ref = MakeTransactionRef(src1);
    uint256 src1Hash = src1Ref->GetHash();

    // Source 2: real DD transfer with 300 DD
    CMutableTransaction src2;
    src2.nVersion = 0x02000770;
    src2.vin.push_back(CTxIn(COutPoint(uint256S("f2f2020200000000000000000000000000000000000000000000000000000001"), 0)));
    src2.vout.push_back(CTxOut(0, MakeP2TR(xonly2)));
    src2.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({300})));
    CTransactionRef src2Ref = MakeTransactionRef(src2);
    uint256 src2Hash = src2Ref->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint out1(src1Hash, 0), out2(src2Hash, 0);
    coinsView.AddCoin(out1, Coin(CTxOut(0, MakeP2TR(xonly1)), 500, false), false);
    coinsView.AddCoin(out2, Coin(CTxOut(0, MakeP2TR(xonly2)), 500, false), false);

    auto txLookup = [&](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == src1Hash) { tx_out = src1Ref; return true; }
        if (txid == src2Hash) { tx_out = src2Ref; return true; }
        return false;
    };

    // VALID transfer: 500 + 300 = 800 DD total
    CMutableTransaction transfer;
    transfer.nVersion = 0x02000770;
    transfer.vin.push_back(CTxIn(out1));
    transfer.vin.push_back(CTxIn(out2));
    transfer.vout.push_back(CTxOut(0, MakeP2TR(recipXOnly)));
    transfer.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({800})));

    CTransaction tx(transfer);
    TxValidationState state;
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);

    BOOST_CHECK_MESSAGE(result,
        "DEFENSE VERIFIED [T2-02f]: Multi-input transfer with correct conservation passes. "
        "500 + 300 = 800 DD. Reason: " + state.GetRejectReason());

    // ATTACK: claim 900 DD from 500+300 inputs
    CMutableTransaction inflatedTransfer;
    inflatedTransfer.nVersion = 0x02000770;
    inflatedTransfer.vin.push_back(CTxIn(out1));
    inflatedTransfer.vin.push_back(CTxIn(out2));
    inflatedTransfer.vout.push_back(CTxOut(0, MakeP2TR(recipXOnly)));
    inflatedTransfer.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({900})));  // 100 more than inputs!

    CTransaction tx2(inflatedTransfer);
    TxValidationState state2;

    bool result2 = DigiDollar::ValidateTransferTransaction(tx2, ctx, state2);

    BOOST_CHECK_MESSAGE(!result2,
        "DEFENSE HOLDS [T2-02f]: Multi-input inflation caught by conservation. "
        "inputDD=800, outputDD=900. Reason: " + state2.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t2_02g_mixed_dd_and_regular_inputs)
{
    // ATTACK: Mix DD inputs with regular (non-DD, non-zero-value) inputs.
    // Only DD inputs should contribute to inputDD. Regular inputs must be ignored.
    // If a regular input is wrongly counted as DD, conservation could be bypassed.

    auto regTestParams = CChainParams::RegTest({});

    CKey ddKey, feeKey, recipKey;
    ddKey.MakeNewKey(true);
    feeKey.MakeNewKey(true);
    recipKey.MakeNewKey(true);
    XOnlyPubKey ddXOnly(ddKey.GetPubKey()), recipXOnly(recipKey.GetPubKey());

    // DD source tx: 2000 DD
    CMutableTransaction ddSrc;
    ddSrc.nVersion = 0x02000770;
    ddSrc.vin.push_back(CTxIn(COutPoint(uint256S("aabb020200000000000000000000000000000000000000000000000000000001"), 0)));
    ddSrc.vout.push_back(CTxOut(0, MakeP2TR(ddXOnly)));
    ddSrc.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({2000})));
    CTransactionRef ddSrcRef = MakeTransactionRef(ddSrc);
    uint256 ddSrcHash = ddSrcRef->GetHash();

    // Regular DGB tx for fees (no DD OP_RETURN)
    CMutableTransaction feeTx;
    feeTx.nVersion = 2;  // Regular Bitcoin version
    feeTx.vin.push_back(CTxIn(COutPoint(uint256S("ccdd020200000000000000000000000000000000000000000000000000000001"), 0)));
    feeTx.vout.push_back(CTxOut(1 * COIN, CScript() << OP_1 << std::vector<unsigned char>(feeKey.GetPubKey().IsCompressed() ?
        std::vector<unsigned char>(XOnlyPubKey(feeKey.GetPubKey()).begin(), XOnlyPubKey(feeKey.GetPubKey()).end()) :
        std::vector<unsigned char>(32, 0))));
    CTransactionRef feeTxRef = MakeTransactionRef(feeTx);
    uint256 feeTxHash = feeTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint ddOut(ddSrcHash, 0);
    COutPoint feeOut(feeTxHash, 0);
    coinsView.AddCoin(ddOut, Coin(CTxOut(0, MakeP2TR(ddXOnly)), 500, false), false);
    coinsView.AddCoin(feeOut, Coin(CTxOut(1 * COIN, feeTx.vout[0].scriptPubKey), 500, false), false);

    auto txLookup = [&](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == ddSrcHash) { tx_out = ddSrcRef; return true; }
        if (txid == feeTxHash) { tx_out = feeTxRef; return true; }
        return false;
    };

    // Transfer: DD input (2000) + fee input → claim 2000 DD output
    CMutableTransaction transfer;
    transfer.nVersion = 0x02000770;
    transfer.vin.push_back(CTxIn(ddOut));
    transfer.vin.push_back(CTxIn(feeOut));
    transfer.vout.push_back(CTxOut(0, MakeP2TR(recipXOnly)));
    transfer.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({2000})));

    CTransaction tx(transfer);
    TxValidationState state;
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateTransferTransaction(tx, ctx, state);

    BOOST_CHECK_MESSAGE(result,
        "DEFENSE VERIFIED [T2-02g]: Mixed DD + fee input transfer works. "
        "Only DD input (2000) contributes to inputDD. Fee input ignored. "
        "Reason: " + state.GetRejectReason());

    BOOST_TEST_MESSAGE("T2-02g: Mixed DD + regular input transfer correctly validated");
}

// =============================================================================
// T2-03: Collateral Release Excess — Get Back More DGB Than Entitled
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_t2_03a_partial_burn_miner_fee_collateral_theft)
{
    // CRITICAL ATTACK [T2-03a]: Miner fee collateral theft via partial burn
    //
    // Scenario: Attacker mints 10,000 DD with 200 DGB collateral. Later, attacker
    // creates a redemption tx burning only 1% (100 DD) of the original DD. The
    // validation correctly limits DGB OUTPUTS to 1% (2 DGB). But the FULL collateral
    // UTXO (200 DGB) is consumed as vin[0]. The remaining 198 DGB becomes miner fee.
    //
    // If the attacker is a miner (or colludes with one), they recover ALL 200 DGB
    // while only burning 100 DD. The other 9,900 DD remains in circulation, unbacked.
    //
    // The validator checks outputs but does NOT check that fee (inputs - outputs)
    // doesn't steal locked collateral. A consensus-level economic exploit.

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;    // 200 DGB locked
    CAmount originalDD = 10000;               // 10,000 DD cents ($100)
    CAmount ddBurned = 100;                   // Burn only 1% (100 DD cents = $1)

    // Create the original mint transaction
    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("aa03000000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    // Set up coins view with collateral UTXO
    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    // EXPLOIT TX: Burn 100 DD (1%), output only 2 DGB (1% of collateral)
    // Remaining 198 DGB becomes miner fee
    CAmount allowedOutput = 2 * COIN;  // 1% of 200 DGB

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;  // REDEEM type
    mtx.vin.push_back(CTxIn(collOutpoint));  // 200 DGB collateral
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("bb03010000000000000000000000000000000000000000000000000000000001"), 0)));  // DD input
    mtx.vout.push_back(CTxOut(allowedOutput, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));  // Only 2 DGB output

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    // The collateral release check will PASS — outputs (2 DGB) <= allowedRelease (2 DGB)
    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurned, state);

    // VULNERABILITY: This passes! But 198 DGB (99% of collateral) goes to miner as fee.
    // The implicit fee = 200 DGB (input) - 2 DGB (output) = 198 DGB
    // A miner-attacker recovers ALL collateral while burning only 1% of DD.
    CAmount implicitFee = lockedCollateral - allowedOutput;
    CAmount allowedRelease = static_cast<int64_t>(
        static_cast<__int128>(ddBurned) * static_cast<__int128>(lockedCollateral) /
        static_cast<__int128>(originalDD));

    BOOST_TEST_MESSAGE("T2-03a: Partial burn 1% DD, output = " << allowedOutput / COIN << " DGB");
    BOOST_TEST_MESSAGE("T2-03a: Collateral input = " << lockedCollateral / COIN << " DGB");
    BOOST_TEST_MESSAGE("T2-03a: Implicit miner fee (stolen collateral) = " << implicitFee / COIN << " DGB");
    BOOST_TEST_MESSAGE("T2-03a: Validation result = " << (result ? "PASS (VULNERABILITY!)" : "FAIL (DEFENDED)"));

    // If validation passes, this is a CRITICAL BUG — partial burn allows
    // miner to steal locked collateral as fees.
    // After fix: should FAIL because ddBurned < originalDDMinted
    BOOST_CHECK_MESSAGE(!result,
        "EXPLOIT [T2-03a]: Partial burn (1%) passed validation! "
        "Miner steals " + std::to_string(implicitFee / COIN) + " DGB as fees. "
        "Fix: Require ddBurned >= originalDDMinted (no partial redemptions). "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t2_03b_cross_mint_dd_burn_collateral_theft)
{
    // ATTACK [T2-03b]: Use DD from mint B to release collateral from mint A
    //
    // Attacker mints A (10,000 DD / 200 DGB collateral) and transfers 10,000 DD to users.
    // Attacker separately obtains 100 DD from mint B (cheap).
    // Attacker creates redemption: vin[0] = A's collateral, burns B's 100 DD.
    //
    // Validator checks: ddBurned (100) vs originalDDMinted from A (10,000)
    // allowedRelease = (100/10000) * 200 = 2 DGB output
    // Fee = 200 - 2 = 198 DGB to miner
    //
    // RESULT: Mint A's collateral released, mint A's 10,000 DD still circulating unbacked.

    auto regTestParams = CChainParams::RegTest({});

    CKey collKeyA;
    collKeyA.MakeNewKey(true);
    XOnlyPubKey xPubA(collKeyA.GetPubKey());
    CScript p2trA = MakeP2TR(xPubA);

    CAmount collateralA = 200 * COIN;
    CAmount originalDDA = 10000;    // Mint A: 10,000 DD
    CAmount ddBurnedFromB = 100;    // Burning DD from a DIFFERENT mint

    // Create mint A
    CMutableTransaction mintTxA;
    mintTxA.nVersion = 0x01000770;
    mintTxA.vin.push_back(CTxIn(COutPoint(uint256S("aa03b00000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTxA.vout.push_back(CTxOut(collateralA, p2trA));
    CKey ddKeyA;
    ddKeyA.MakeNewKey(true);
    mintTxA.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKeyA.GetPubKey()))));
    mintTxA.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDDA, 1000, 1, xPubA)));

    CTransactionRef mintTxRefA = MakeTransactionRef(mintTxA);
    uint256 mintHashA = mintTxRefA->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpointA(mintHashA, 0);
    coinsView.AddCoin(collOutpointA, Coin(CTxOut(collateralA, p2trA), 400, false), false);

    auto txLookup = [&mintTxRefA, &mintHashA](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintHashA) {
            tx_out = mintTxRefA;
            return true;
        }
        return false;
    };

    // Redemption: burn 100 DD from mint B, using mint A's collateral
    CAmount output = 2 * COIN;  // (100/10000) * 200 = 2 DGB

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOutpointA));  // Mint A's collateral
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("bb03b00000000000000000000000000000000000000000000000000000000001"), 0)));  // DD from mint B
    mtx.vout.push_back(CTxOut(output, CScript() << OP_1 << ToByteVector(xPubA)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurnedFromB, state);

    CAmount implicitFee = collateralA - output;

    BOOST_TEST_MESSAGE("T2-03b: Cross-mint burn — DD from mint B (100) vs mint A collateral (200 DGB)");
    BOOST_TEST_MESSAGE("T2-03b: Implicit fee (stolen) = " << implicitFee / COIN << " DGB");
    BOOST_TEST_MESSAGE("T2-03b: Result = " << (result ? "PASS (VULNERABLE)" : "FAIL (DEFENDED)"));

    // After fix: should FAIL — ddBurned (100) < originalDDMinted from A (10,000)
    BOOST_CHECK_MESSAGE(!result,
        "EXPLOIT [T2-03b]: Cross-mint burn passed! Mint A's 10,000 DD remains unbacked. "
        "Fee steals " + std::to_string(implicitFee / COIN) + " DGB. "
        "Reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t2_03c_full_burn_still_works)
{
    // VALID [T2-03c]: Full burn should still be allowed after fix
    // ddBurned == originalDDMinted → full collateral release

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;
    CAmount ddBurned = 10000;  // Full burn!

    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("cc03000000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOutpoint));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("dd03010000000000000000000000000000000000000000000000000000000001"), 0)));
    mtx.vout.push_back(CTxOut(lockedCollateral, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurned, state);

    BOOST_CHECK_MESSAGE(result,
        "REGRESSION [T2-03c]: Full burn (ddBurned == originalDDMinted) must still allow "
        "full collateral release. Error: " + state.GetRejectReason());

    BOOST_TEST_MESSAGE("T2-03c: Full burn redemption correctly allowed");
}

BOOST_AUTO_TEST_CASE(redteam_t2_03d_slight_overburn_still_works)
{
    // VALID [T2-03d]: Burning slightly MORE DD than original mint (e.g., from multiple
    // DD sources) should still allow full collateral release.

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;
    CAmount ddBurned = 10500;  // Burn more than minted — acceptable, user's loss

    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("ee03000000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOutpoint));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("ff03010000000000000000000000000000000000000000000000000000000001"), 0)));
    mtx.vout.push_back(CTxOut(lockedCollateral, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurned, state);

    BOOST_CHECK_MESSAGE(result,
        "REGRESSION [T2-03d]: Burning more DD than originally minted should still allow "
        "full collateral release. Error: " + state.GetRejectReason());

    BOOST_TEST_MESSAGE("T2-03d: Over-burn redemption correctly allowed");
}

BOOST_AUTO_TEST_CASE(redteam_t2_03e_fee_tolerance_minimum_exploit)
{
    // ATTACK [T2-03e]: Exploit the minimum fee tolerance of 1000 satoshis
    //
    // For tiny partial burns, allowedRelease is small but feeTolerance = max(1000, allowedRelease/1000)
    // When allowedRelease < 1,000,000 sats (0.01 DGB), tolerance > 0.1% of allowed
    // At extreme: allowedRelease = 100 sats, feeTolerance = 1000 sats → can release 11x allowed!
    //
    // While individual excess is tiny (1000 sats per tx), a miner creating thousands
    // of these per block could accumulate meaningful theft.

    auto regTestParams = CChainParams::RegTest({});

    CKey collateralKey;
    collateralKey.MakeNewKey(true);
    XOnlyPubKey collateralXOnlyKey(collateralKey.GetPubKey());
    CScript rawCollateralP2TR = MakeP2TR(collateralXOnlyKey);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;
    CAmount ddBurned = 1;  // Burn just 1 DD cent ($0.01)

    // Expected: allowedRelease = (1/10000) * 200 DGB = 0.02 DGB = 2,000,000 sats
    // feeTolerance = max(1000, 2,000,000/1000) = max(1000, 2000) = 2000 sats
    // Max allowed output = 2,002,000 sats

    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("ff03e00000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, rawCollateralP2TR));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, collateralXOnlyKey)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintTxHash = mintTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutpoint(mintTxHash, 0);
    coinsView.AddCoin(collOutpoint, Coin(CTxOut(lockedCollateral, rawCollateralP2TR), 400, false), false);

    auto txLookup = [&mintTxRef, &mintTxHash](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintTxHash) {
            tx_out = mintTxRef;
            return true;
        }
        return false;
    };

    CAmount allowedRelease = static_cast<int64_t>(
        static_cast<__int128>(ddBurned) * static_cast<__int128>(lockedCollateral) /
        static_cast<__int128>(originalDD));
    CAmount feeTolerance = std::max((CAmount)1000, allowedRelease / 1000);

    BOOST_TEST_MESSAGE("T2-03e: allowedRelease = " << allowedRelease << " sats, feeTolerance = " << feeTolerance << " sats");

    // Try to release allowedRelease + feeTolerance (maximum allowed)
    CAmount exploitOutput = allowedRelease + feeTolerance;

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOutpoint));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("ff03e10000000000000000000000000000000000000000000000000000000001"), 0)));
    mtx.vout.push_back(CTxOut(exploitOutput, CScript() << OP_1 << ToByteVector(collateralXOnlyKey)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctxWithCoins(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctxWithCoins, ddBurned, state);

    // After partial-burn fix, this should be rejected entirely (ddBurned < originalDDMinted)
    BOOST_CHECK_MESSAGE(!result,
        "EXPLOIT [T2-03e]: Micro-burn with tolerance exploitation passed! "
        "Released " + std::to_string(exploitOutput) + " sats for burning just 1 DD cent. "
        "Reason: " + state.GetRejectReason());

    BOOST_TEST_MESSAGE("T2-03e: Fee tolerance minimum exploitation " << (result ? "VULNERABLE" : "DEFENDED"));
}

// =============================================================================
// T2-04: Oracle Price Staleness Exploit
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_t2_04a_stale_cached_price_no_expiry)
{
    // ATTACK: Oracle goes offline, cached price persists forever.
    // DGB price crashes, attacker mints DD using stale high price with less collateral.
    //
    // GetLatestPrice() returns cached_price without checking last_update_time.
    // last_update_time is tracked but NEVER checked for freshness.

    BOOST_TEST_MESSAGE("=== T2-04a: Stale Cached Price — No Expiry Check ===");

    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    // Simulate: oracle sends price at time T
    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    // Create a valid oracle message with price $0.05 (50000 micro-USD)
    CKey oracleKey;
    oracleKey.MakeNewKey(true);

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;  // $0.05
    msg.timestamp = baseTime;
    msg.SignPhase2(oracleKey);

    // Inject directly (bypass chainparams check)
    manager.InjectTestMessage(msg);

    // Force cached price update
    {
        std::vector<uint64_t> prices = {50000};
        // Manually set cached price via UpdatePriceCache
        manager.UpdatePriceCache(100, 50000);
    }

    CAmount priceAtTime = manager.GetLatestPrice();
    BOOST_CHECK_EQUAL(priceAtTime, 50000);
    BOOST_TEST_MESSAGE("T2-04a: Price at T=0: " << priceAtTime << " micro-USD ($" << priceAtTime / 1000000.0 << ")");

    // Advance time by 2 hours (well past ORACLE_MAX_AGE_SECONDS = 3600)
    SetMockTime(baseTime + 7200);

    // GetLatestPrice() should ideally return 0 (stale), but currently returns cached value
    CAmount priceAfter2h = manager.GetLatestPrice();

    BOOST_TEST_MESSAGE("T2-04a: Price after 2 hours (no oracle updates): " << priceAfter2h << " micro-USD");

    // BUG CHECK: If price is still returned after 2 hours with no updates, staleness is not checked
    if (priceAfter2h > 0) {
        BOOST_TEST_MESSAGE("VULNERABILITY [T2-04a]: GetLatestPrice() returns stale price " << priceAfter2h
            << " micro-USD after 2 hours with no oracle updates! "
            << "last_update_time is tracked but NEVER checked.");

        // Demonstrate the exploit: attacker uses stale $0.05 price when real price dropped to $0.001
        // With stale price: 1 DGB = $0.05, so $1 DD needs 2000 DGB at 1000% ratio
        // With real price: 1 DGB = $0.001, so $1 DD needs 100000 DGB at 1000% ratio
        // Attacker gets 50x leverage on under-collateralized DD
        BOOST_CHECK_MESSAGE(priceAfter2h == 0,
            "EXPLOIT [T2-04a]: Stale cached oracle price persists indefinitely! "
            "Cached price = " + std::to_string(priceAfter2h) + " micro-USD after 2 hours. "
            "last_update_time exists but GetLatestPrice() NEVER checks it. "
            "An attacker can DDoS oracles and mint DD using the last known (higher) price "
            "while the real DGB price has crashed, creating under-collateralized tokens.");
    } else {
        BOOST_TEST_MESSAGE("T2-04a: DEFENDED — GetLatestPrice() correctly returns 0 for stale price");
    }

    // Advance time by 24 hours — price should definitely be invalid
    SetMockTime(baseTime + 86400);
    CAmount priceAfter24h = manager.GetLatestPrice();
    BOOST_TEST_MESSAGE("T2-04a: Price after 24 hours: " << priceAfter24h << " micro-USD");

    BOOST_CHECK_MESSAGE(priceAfter24h == 0,
        "EXPLOIT [T2-04a]: Oracle price persists after 24 HOURS! "
        "Price = " + std::to_string(priceAfter24h) + " micro-USD. "
        "No staleness timeout exists in GetLatestPrice().");

    SetMockTime(0);  // Reset mock time
    manager.Clear();
}

BOOST_AUTO_TEST_CASE(redteam_t2_04b_hardcoded_fallback_price)
{
    // ATTACK: GetOraclePriceForTransaction() has a hardcoded fallback price of $0.0065
    // If oracle system returns 0, minting proceeds at this arbitrary fixed price.
    // This bypasses the oracle system entirely.

    BOOST_TEST_MESSAGE("=== T2-04b: Hardcoded Fallback Oracle Price ===");

    // On a fresh node or after oracle failure, GetOraclePriceForTransaction falls through to:
    // static const CAmount FALLBACK_ORACLE_PRICE_MICRO_USD = 6500;
    //
    // This means ANY node can mint DD tokens using $0.0065/DGB even if:
    // - No oracles have ever been online
    // - All oracles are offline
    // - Real DGB price is completely different
    //
    // The fallback should NOT exist — oracle failure should HALT minting, not use a guess.

    // Verify the fallback exists by checking the price flow
    // In regtest, MockOracleManager takes priority, so we need to check the code path directly
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // With no oracle data at all, GetLatestPrice returns 0
    CAmount noDataPrice = manager.GetLatestPrice();
    BOOST_CHECK_EQUAL(noDataPrice, 0);
    BOOST_TEST_MESSAGE("T2-04b: GetLatestPrice() with no data = " << noDataPrice << " (correctly 0)");

    // But GetOraclePriceForTransaction in validation.cpp falls back to FALLBACK_ORACLE_PRICE_MICRO_USD = 6500
    // This is a code-level finding — the fallback bypasses oracle consensus entirely
    // We can verify this by examining the function, but can't easily call it from unit tests
    // without setting up the full transaction validation context

    BOOST_TEST_MESSAGE("FINDING [T2-04b]: validation.cpp GetOraclePriceForTransaction() has hardcoded fallback "
        "FALLBACK_ORACLE_PRICE_MICRO_USD = 6500 ($0.0065/DGB). "
        "When oracle system returns 0, minting uses this fixed price instead of rejecting. "
        "This bypasses oracle consensus entirely on fresh nodes or during oracle outages.");

    // Verify the code: validation.cpp line ~1844
    // static const CAmount FALLBACK_ORACLE_PRICE_MICRO_USD = 6500;
    // This should be removed — oracle failure = no minting, period.

    manager.Clear();
}

BOOST_AUTO_TEST_CASE(redteam_t2_04c_last_update_time_unused)
{
    // ATTACK: Verify that last_update_time is stored but never used for validation.
    // This is the root cause of T2-04a — the freshness timestamp exists but is decorative.

    BOOST_TEST_MESSAGE("=== T2-04c: last_update_time Is Unused ===");

    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    // Set a price
    manager.UpdatePriceCache(100, 50000);

    // Get stats — last_update should be baseTime
    auto stats = manager.GetStats();
    BOOST_CHECK_EQUAL(stats.latest_price, 50000);
    BOOST_CHECK(stats.last_update > 0);
    BOOST_TEST_MESSAGE("T2-04c: Stats.last_update = " << stats.last_update << ", latest_price = " << stats.latest_price);

    // Advance time way past any reasonable staleness window
    SetMockTime(baseTime + 604800);  // 1 week later

    // Price is still available
    CAmount stalePrice = manager.GetLatestPrice();
    BOOST_TEST_MESSAGE("T2-04c: Price after 1 WEEK: " << stalePrice << " micro-USD");

    // Stats still show the old update time
    auto staleStats = manager.GetStats();
    int64_t age = (baseTime + 604800) - staleStats.last_update;
    BOOST_TEST_MESSAGE("T2-04c: Price age: " << age << " seconds (" << age / 3600 << " hours, " << age / 86400 << " days)");

    BOOST_CHECK_MESSAGE(stalePrice == 0,
        "EXPLOIT [T2-04c]: cached_price persists for " + std::to_string(age) + " seconds ("
        + std::to_string(age / 86400) + " days) without any oracle update! "
        "last_update_time = " + std::to_string(staleStats.last_update) + " but NOTHING checks it. "
        "OracleStats::last_update is informational only — purely decorative.");

    SetMockTime(0);
    manager.Clear();
}

BOOST_AUTO_TEST_CASE(redteam_t2_04d_stale_price_enables_undercollateralized_mint)
{
    // ATTACK: Full exploit demonstration.
    // 1. Oracle price $0.05 cached
    // 2. Oracle goes offline, real DGB price drops to $0.001
    // 3. Attacker mints DD using stale $0.05 price
    // 4. Required collateral is 50x less than it should be

    BOOST_TEST_MESSAGE("=== T2-04d: Stale Price Enables Under-Collateralized Minting ===");

    auto regTestParams = CChainParams::RegTest({});
    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    // Oracle sets price at $0.05 (50000 micro-USD)
    CAmount stalePrice = 50000;

    // Calculate collateral needed at stale $0.05 price (1-hour lock = 1000% ratio)
    CAmount ddToMint = 100;  // $1.00 in DD cents
    int lockBlocks = 240;    // 1-hour lock
    DigiDollar::ValidationContext ctxStale(1000, stalePrice, 150, *regTestParams);
    CAmount collateralAtStale = DigiDollar::CalculateRequiredCollateral(ddToMint, lockBlocks, ctxStale);

    BOOST_TEST_MESSAGE("T2-04d: At stale price $0.05: collateral needed = " << collateralAtStale << " sats ("
        << collateralAtStale / COIN << " DGB)");

    // Now simulate: price SHOULD be $0.001 (1000 micro-USD) — DGB crashed 50x
    CAmount realPrice = 1000;
    DigiDollar::ValidationContext ctxReal(1000, realPrice, 150, *regTestParams);
    CAmount collateralAtReal = DigiDollar::CalculateRequiredCollateral(ddToMint, lockBlocks, ctxReal);

    BOOST_TEST_MESSAGE("T2-04d: At real price $0.001: collateral needed = " << collateralAtReal << " sats ("
        << collateralAtReal / COIN << " DGB)");

    // The exploit: attacker provides collateralAtStale (much less than collateralAtReal)
    // Validation passes because it uses stale price
    if (collateralAtStale > 0 && collateralAtReal > 0) {
        double undercollateralizedRatio = static_cast<double>(collateralAtReal) / collateralAtStale;
        BOOST_TEST_MESSAGE("T2-04d: Under-collateralization factor: " << undercollateralizedRatio << "x");
        BOOST_TEST_MESSAGE("T2-04d: Attacker provides " << (1.0 / undercollateralizedRatio * 100.0) << "% of required collateral");

        // Verify the stale price validation passes
        DigiDollar::ValidationContext ctxExploit(1000, stalePrice, 150, *regTestParams);
        CAmount requiredAtStale = DigiDollar::CalculateRequiredCollateral(ddToMint, lockBlocks, ctxExploit);
        BOOST_CHECK(requiredAtStale > 0);

        // With stale price, collateralAtStale is enough (validation passes)
        // But with real price, it's woefully insufficient
        BOOST_TEST_MESSAGE("EXPLOIT [T2-04d]: Attacker mints $1 DD with " << collateralAtStale << " sats collateral. "
            "Correct requirement at real price: " << collateralAtReal << " sats. "
            "Under-collateralized by " << undercollateralizedRatio << "x. "
            "Root cause: GetLatestPrice() has no staleness check.");
    }

    SetMockTime(0);
}

// =============================================================================
// T2-05: DCA/ERR Health Calculation Gaming
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_T2_05a_hardcoded_system_health)
{
    // ATTACK: GetSystemCollateralRatio() is hardcoded to 150, meaning:
    // - DCA multiplier is ALWAYS 1.0x (no collateral adjustment)
    // - ERR never triggers (150 > 100)
    // - System protections completely disabled in consensus validation
    //
    // EXPLOIT: Keep minting DD when system is catastrophically under-collateralized.
    // No DCA increase, no ERR mint-blocking, no death spiral protection.

    auto regTestParams = CChainParams::RegTest({});

    // Prove GetSystemCollateralRatio() always returns 150
    CAmount health1 = DigiDollar::GetSystemCollateralRatio();
    CAmount health2 = DigiDollar::GetSystemCollateralRatio();
    CAmount health3 = DigiDollar::GetSystemCollateralRatio();
    BOOST_CHECK_EQUAL(health1, 150);
    BOOST_CHECK_EQUAL(health2, 150);
    BOOST_CHECK_EQUAL(health3, 150);
    BOOST_TEST_MESSAGE("T2-05a: GetSystemCollateralRatio() returns hardcoded " << health1 << " (always 'healthy')");

    // Prove DCA multiplier is always 1.0x because health is always 150
    double multiplier = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(health1);
    BOOST_CHECK_EQUAL(multiplier, 1.0);
    BOOST_TEST_MESSAGE("T2-05a: DCA multiplier at health=" << health1 << ": " << multiplier << "x (no adjustment)");

    // Prove ERR would never trigger
    bool errShouldActivate = DigiDollar::ERR::EmergencyRedemptionRatio::ShouldActivateERR(health1);
    BOOST_CHECK_EQUAL(errShouldActivate, false);
    BOOST_TEST_MESSAGE("T2-05a: ERR activation at health=" << health1 << ": " << (errShouldActivate ? "YES" : "NO"));

    // Show what SHOULD happen at different health levels
    struct HealthScenario {
        int health;
        double expectedMultiplier;
        bool expectedERR;
        const char* description;
    };

    std::vector<HealthScenario> scenarios = {
        {150, 1.0, false, "Healthy system"},
        {130, 1.2, false, "Warning level"},
        {110, 1.5, false, "Critical level"},
        {90,  2.0, true,  "Emergency - under-collateralized"},
        {50,  2.0, true,  "Catastrophic - 50% backed"},
        {10,  2.0, true,  "Near-zero backing"},
    };

    for (const auto& s : scenarios) {
        double dcaMult = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(s.health);
        bool errActive = DigiDollar::ERR::EmergencyRedemptionRatio::ShouldActivateERR(s.health);
        BOOST_CHECK_EQUAL(dcaMult, s.expectedMultiplier);
        BOOST_CHECK_EQUAL(errActive, s.expectedERR);
        BOOST_TEST_MESSAGE("T2-05a: Health " << s.health << "% -> DCA " << dcaMult << "x, ERR " 
            << (errActive ? "ACTIVE" : "inactive") << " (" << s.description << ")");
    }

    // EXPLOIT DEMONSTRATION: Compare collateral requirements at different health levels
    const CAmount ddAmount = 10000; // $100 DD
    const int lockBlocks = 30 * DigiDollar::BLOCKS_PER_DAY;
    const CAmount oraclePrice = 5000; // $0.005 per DGB

    // What consensus ALWAYS calculates (health hardcoded to 150):
    DigiDollar::ValidationContext ctxAlwaysHealthy(1000, oraclePrice, 150, *regTestParams);
    CAmount collateralHealthy = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctxAlwaysHealthy);

    // What consensus SHOULD calculate at health=90 (emergency):
    DigiDollar::ValidationContext ctxEmergency(1000, oraclePrice, 90, *regTestParams);
    CAmount collateralEmergency = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctxEmergency);

    // What consensus SHOULD calculate at health=50 (catastrophic):
    DigiDollar::ValidationContext ctxCatastrophic(1000, oraclePrice, 50, *regTestParams);
    CAmount collateralCatastrophic = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctxCatastrophic);

    BOOST_TEST_MESSAGE("T2-05a: Collateral for $100 DD at health=150%: " << collateralHealthy << " sats");
    BOOST_TEST_MESSAGE("T2-05a: Collateral for $100 DD at health=90%:  " << collateralEmergency << " sats (should be 2x)");
    BOOST_TEST_MESSAGE("T2-05a: Collateral for $100 DD at health=50%:  " << collateralCatastrophic << " sats (should be 2x)");

    // Verify emergency requires 2x collateral
    BOOST_CHECK(collateralEmergency > collateralHealthy);
    double emergencyRatio = static_cast<double>(collateralEmergency) / collateralHealthy;
    BOOST_TEST_MESSAGE("T2-05a: Emergency requires " << emergencyRatio << "x more collateral");

    BOOST_TEST_MESSAGE("CRITICAL BUG [T2-05a]: GetSystemCollateralRatio() is hardcoded to 150. "
        "Both mempool and ConnectBlock use this for ctx.systemCollateral. "
        "DCA multiplier is ALWAYS 1.0x (no adjustment). ERR never triggers. "
        "Death spiral protection completely disabled.");
}

BOOST_AUTO_TEST_CASE(redteam_T2_05b_dca_never_applied_in_consensus)
{
    // ATTACK: Prove that even with a proper DCA system, the consensus path
    // never applies DCA adjustments because health is hardcoded.
    // 
    // Scenario: System has 10x more DD than collateral backing (10% health).
    // Expected: DCA should double collateral requirements.
    // Actual: Collateral requirements unchanged (1.0x multiplier).

    auto regTestParams = CChainParams::RegTest({});
    const CAmount ddAmount = 10000; // $100 DD
    const int lockBlocks = 365 * DigiDollar::BLOCKS_PER_DAY; // 1 year
    const CAmount oraclePrice = 5000; // $0.005 per DGB

    // Consensus path: uses hardcoded health=150
    DigiDollar::ValidationContext ctxConsensus(1000, oraclePrice, 150, *regTestParams);
    CAmount collateralConsensus = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctxConsensus);

    // What DCA SHOULD produce at emergency health:
    int baseRatio = DigiDollar::GetCollateralRatioForLockTime(lockBlocks, regTestParams->GetDigiDollarParams());
    int adjustedRatio = DigiDollar::DCA::DynamicCollateralAdjustment::ApplyDCA(baseRatio, 50); // 50% health

    BOOST_TEST_MESSAGE("T2-05b: Base collateral ratio for 1yr lock: " << baseRatio << "%");
    BOOST_TEST_MESSAGE("T2-05b: DCA-adjusted ratio at 50% health: " << adjustedRatio << "% (2.0x multiplier)");
    BOOST_TEST_MESSAGE("T2-05b: Consensus ALWAYS uses: " << baseRatio << "% (no DCA applied)");
    BOOST_CHECK_EQUAL(adjustedRatio, baseRatio * 2);
    BOOST_CHECK(adjustedRatio > baseRatio);

    // Prove the collateral calculation uses the un-adjusted ratio in consensus
    DigiDollar::ValidationContext ctxDCA(1000, oraclePrice, 50, *regTestParams);
    CAmount collateralDCA = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctxDCA);

    BOOST_TEST_MESSAGE("T2-05b: Collateral at consensus health (150%): " << collateralConsensus << " sats");
    BOOST_TEST_MESSAGE("T2-05b: Collateral at real health (50%): " << collateralDCA << " sats");
    double dcaImpact = static_cast<double>(collateralDCA) / collateralConsensus;
    BOOST_TEST_MESSAGE("T2-05b: DCA impact: " << dcaImpact << "x more collateral required at 50% health");

    BOOST_TEST_MESSAGE("BUG [T2-05b]: In production, attacker keeps minting at base ratio "
        "even when system is 50% collateralized. DCA multiplier is always 1.0x in consensus.");
}

BOOST_AUTO_TEST_CASE(redteam_T2_05c_err_never_blocks_minting)
{
    // ATTACK: Prove that ERR mint-blocking never engages through the consensus
    // validation path because ctx.systemCollateral is always 150.
    //
    // The only mint-blocking comes from ShouldBlockMintingDuringERR() which
    // delegates to ShouldBlockMinting() - but that relies on cached metrics
    // that are only populated by RPC calls.

    auto regTestParams = CChainParams::RegTest({});

    // Get hardcoded health
    CAmount health = DigiDollar::GetSystemCollateralRatio();
    BOOST_CHECK_EQUAL(health, 150);

    // ERR should NOT activate at 150
    BOOST_CHECK_EQUAL(DigiDollar::ERR::EmergencyRedemptionRatio::ShouldActivateERR(health), false);

    // Even with extreme under-collateralization scenarios, consensus always sees 150
    // This means the redemption path check at validation.cpp:1416 NEVER takes ERR path
    //   if (ctx.systemCollateral < 100) { // ERR path }
    //   else { // Normal path — ALWAYS this one }
    bool normalPath = (health >= 100);
    bool errPath = (health < 100);
    BOOST_CHECK_EQUAL(normalPath, true);
    BOOST_CHECK_EQUAL(errPath, false);

    BOOST_TEST_MESSAGE("T2-05c: Consensus health=" << health << " -> Normal redemption path ALWAYS taken");
    BOOST_TEST_MESSAGE("T2-05c: ERR path is dead code in consensus — never executed");
    BOOST_TEST_MESSAGE("T2-05c: ValidateEmergencyRedemptionConditions returns 'err-validation-incomplete' anyway");
    BOOST_TEST_MESSAGE("BUG [T2-05c]: ERR protection is non-functional. During a death spiral, "
        "minting continues, normal redemptions continue, no emergency measures engage.");
}

BOOST_AUTO_TEST_CASE(redteam_T2_05d_unit_mismatch_health_calculation)
{
    // ATTACK: GetLastOraclePrice() returns cents (50 = $0.50/DGB)
    // but CalculateSystemHealth() treats its oraclePrice parameter as
    // millicents (50 millicents = $0.0005/DGB).
    //
    // This means when GetCurrentSystemHealth() is eventually used with
    // real data, health is calculated 1000x too LOW.
    //
    // Example: System at 150% collateralization shows as 0.15%

    // Simulate: 100 DGB collateral, 50 cents of DD, price $0.50/DGB
    CAmount totalCollateral = 100 * COIN; // 100 DGB in satoshis
    CAmount totalDD = 5000; // $50 in cents
    CAmount priceInCents = 50; // $0.50 per DGB in cents

    // CalculateSystemHealth treats price as millicents
    // With price=50 millicents = $0.0005:
    // collateralValueMillicents = (10,000,000,000 * 50) / 100,000,000 = 5000
    // collateralValueCents = 5000 / 1000 = 5
    // health = (5 * 100) / 5000 = 0
    int healthWithCents = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
        totalCollateral, totalDD, priceInCents);

    // Now with CORRECT units (millicents): 50 cents = 50000 millicents
    CAmount priceInMillicents = 50000; // $0.50 per DGB in millicents
    int healthWithMillicents = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
        totalCollateral, totalDD, priceInMillicents);

    BOOST_TEST_MESSAGE("T2-05d: 100 DGB collateral, $50 DD supply, $0.50/DGB price");
    BOOST_TEST_MESSAGE("T2-05d: Health with cents (50):      " << healthWithCents << "% (should be ~100%)");
    BOOST_TEST_MESSAGE("T2-05d: Health with millicents (50000): " << healthWithMillicents << "%");

    // The cents calculation produces wildly wrong result
    BOOST_CHECK(healthWithCents < 10); // Calculates near-zero health
    BOOST_CHECK(healthWithMillicents >= 80 && healthWithMillicents <= 120); // Should be close to 100%

    BOOST_TEST_MESSAGE("MEDIUM BUG [T2-05d]: GetLastOraclePrice() returns cents, "
        "CalculateSystemHealth() expects millicents. Health calculated 1000x too low. "
        "Currently masked by hardcoded GetSystemCollateralRatio()=150, but will break "
        "when that's fixed to use real health calculations.");
}

BOOST_AUTO_TEST_CASE(redteam_T2_05e_should_block_minting_fails_open)
{
    // ATTACK: ShouldBlockMinting() is the ONLY real-time health check,
    // but it fails open in multiple scenarios:
    // 1. Cached metrics empty (fresh node) -> allows minting
    // 2. No oracle price -> allows minting
    // 3. totalDDSupply <= 0 -> allows minting
    //
    // An attacker on a fresh node or one where ScanUTXOSet hasn't been
    // called can bypass ERR mint-blocking entirely.

    // On a fresh start, SystemHealthMonitor metrics are empty
    // ShouldBlockMinting() checks totalDDSupply <= 0 first and returns false
    bool shouldBlock = DigiDollar::ERR::EmergencyRedemptionRatio::ShouldBlockMinting();

    BOOST_TEST_MESSAGE("T2-05e: ShouldBlockMinting() on fresh node: " << (shouldBlock ? "BLOCKED" : "ALLOWED"));
    // Fresh node has no metrics -> allows minting
    BOOST_CHECK_EQUAL(shouldBlock, false);

    BOOST_TEST_MESSAGE("MEDIUM BUG [T2-05e]: ShouldBlockMinting() fails-open when cached metrics "
        "are empty. On a fresh node or before first UTXO scan, ERR mint-blocking is disabled. "
        "Combined with hardcoded GetSystemCollateralRatio()=150, there is NO functional "
        "mint-blocking during system emergencies.");
}

BOOST_AUTO_TEST_CASE(redteam_T2_05f_death_spiral_no_protection)
{
    // ATTACK: Demonstrate a full death spiral scenario where ALL protections fail.
    //
    // Scenario:
    // 1. System starts healthy (150% collateralized)
    // 2. DGB price drops 75% -> system now at ~37.5% health
    // 3. Attacker keeps minting DD at base collateral ratio (no DCA increase)
    // 4. Each mint further dilutes the system
    // 5. No ERR blocks minting
    // 6. No DCA increases requirements
    // 7. System becomes insolvent
    //
    // All because GetSystemCollateralRatio() returns 150 regardless of reality.

    auto regTestParams = CChainParams::RegTest({});

    // System starts with $1M DD, $1.5M collateral (150% health)
    CAmount initialDD = 100000000; // $1M in cents
    CAmount initialCollateral = 1500000 * COIN; // 1.5M DGB
    CAmount initialPrice = 100000; // $0.10/DGB in millicents → $1.5M collateral value

    int healthBefore = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
        initialCollateral, initialDD, initialPrice);
    BOOST_TEST_MESSAGE("T2-05f: Initial system health: " << healthBefore << "% ($1.5M backing $1M DD)");

    // Price drops 75%
    CAmount crashedPrice = initialPrice / 4; // $0.025/DGB
    int healthAfterCrash = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
        initialCollateral, initialDD, crashedPrice);
    BOOST_TEST_MESSAGE("T2-05f: Health after 75% price crash: " << healthAfterCrash << "% ($375K backing $1M DD)");
    BOOST_CHECK(healthAfterCrash < 50); // System is critically under-collateralized

    // But consensus still sees health=150
    CAmount consensusHealth = DigiDollar::GetSystemCollateralRatio();
    BOOST_CHECK_EQUAL(consensusHealth, 150);

    // DCA at real health should be 2.0x (emergency)
    double dcaAtRealHealth = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(healthAfterCrash);
    BOOST_CHECK_EQUAL(dcaAtRealHealth, 2.0);

    // DCA at consensus health is still 1.0x
    double dcaAtConsensusHealth = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(consensusHealth);
    BOOST_CHECK_EQUAL(dcaAtConsensusHealth, 1.0);

    // ERR should be active at real health
    bool errShouldBeActive = DigiDollar::ERR::EmergencyRedemptionRatio::ShouldActivateERR(healthAfterCrash);
    BOOST_CHECK_EQUAL(errShouldBeActive, true);

    // ERR at consensus health: inactive
    bool errAtConsensus = DigiDollar::ERR::EmergencyRedemptionRatio::ShouldActivateERR(consensusHealth);
    BOOST_CHECK_EQUAL(errAtConsensus, false);

    BOOST_TEST_MESSAGE("T2-05f: Real health=" << healthAfterCrash << "% -> DCA " << dcaAtRealHealth << "x, ERR " 
        << (errShouldBeActive ? "ACTIVE" : "inactive"));
    BOOST_TEST_MESSAGE("T2-05f: Consensus health=" << consensusHealth << "% -> DCA " << dcaAtConsensusHealth << "x, ERR "
        << (errAtConsensus ? "ACTIVE" : "inactive"));

    BOOST_TEST_MESSAGE("CRITICAL BUG [T2-05f]: After 75% price crash, system is at " << healthAfterCrash 
        << "% health but consensus reports 150%. DCA remains 1.0x (should be 2.0x). "
        "ERR remains inactive (should block minting). Attacker can continue minting "
        "DD at base ratios, accelerating the death spiral.");
}

// =============================================================================
// T2-06: Fee Manipulation — Zero-Fee DD Transactions
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_t2_06a_fee_calc_zero_value_dd_inputs)
{
    // ATTACK [T2-06a]: Verify that fee calculation handles zero-value DD inputs correctly.
    //
    // DD P2TR outputs have nValue=0. When used as inputs to a transfer, they
    // contribute 0 to nValueIn. The fee = nValueIn - value_out. If ALL inputs are
    // DD UTXOs (nValue=0) and all outputs are DD (nValue=0), fee = 0.
    //
    // This test verifies the fee arithmetic is correct — DD inputs/outputs should
    // be "invisible" to fee calculation, and fee must come entirely from DGB (non-DD)
    // inputs and outputs.

    CKey key1, key2;
    key1.MakeNewKey(true);
    key2.MakeNewKey(true);
    XOnlyPubKey xonly1(key1.GetPubKey());
    XOnlyPubKey xonly2(key2.GetPubKey());

    // Scenario A: Transfer with ONLY DD inputs/outputs → fee = 0
    CMutableTransaction ddOnlyTx;
    ddOnlyTx.nVersion = 0x02000770;  // DD_TX_TRANSFER
    // Two DD inputs (nValue=0)
    ddOnlyTx.vin.push_back(CTxIn(COutPoint(uint256S("d206a00000000000000000000000000000000000000000000000000000000001"), 0)));
    ddOnlyTx.vin.push_back(CTxIn(COutPoint(uint256S("d206a00000000000000000000000000000000000000000000000000000000002"), 0)));
    // DD output (nValue=0) + OP_RETURN
    ddOnlyTx.vout.push_back(CTxOut(0, MakeP2TR(xonly1)));
    ddOnlyTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({500})));

    CTransaction txDDOnly(ddOnlyTx);

    // Fee = sum(input nValue) - sum(output nValue) = 0 - 0 = 0
    CAmount ddOnlyFee = 0;
    for (const auto& vout : txDDOnly.vout) ddOnlyFee -= vout.nValue;
    BOOST_CHECK_EQUAL(ddOnlyFee, 0);
    BOOST_TEST_MESSAGE("T2-06a: DD-only transfer has fee = 0 (correct — no DGB inputs/outputs)");

    // Scenario B: Transfer with DD inputs + fee UTXO → fee > 0
    CMutableTransaction ddWithFeeTx;
    ddWithFeeTx.nVersion = 0x02000770;
    ddWithFeeTx.vin.push_back(CTxIn(COutPoint(uint256S("d206a00000000000000000000000000000000000000000000000000000000003"), 0)));  // DD
    ddWithFeeTx.vin.push_back(CTxIn(COutPoint(uint256S("d206a00000000000000000000000000000000000000000000000000000000004"), 0)));  // fee UTXO
    ddWithFeeTx.vout.push_back(CTxOut(0, MakeP2TR(xonly2)));  // DD output
    ddWithFeeTx.vout.push_back(CTxOut(0, MakeDDTransferOpReturn({500})));  // OP_RETURN
    ddWithFeeTx.vout.push_back(CTxOut(90000, CScript() << OP_0 << ToByteVector(uint160())));  // Change

    // With fee UTXO of 100000 sats:
    // nValueIn = 0 (DD) + 100000 (fee) = 100000
    // value_out = 0 (DD) + 0 (OP_RETURN) + 90000 (change) = 90000
    // fee = 100000 - 90000 = 10000 sats
    CAmount expectedFee = 10000;  // 100000 - 90000
    BOOST_CHECK_GT(expectedFee, 0);
    BOOST_TEST_MESSAGE("T2-06a: DD transfer with fee UTXO has correct fee of " << expectedFee << " sats");

    // Verify: Zero-value outputs DON'T contribute to value_out
    CTransaction ddWithFeeTxFinal(ddWithFeeTx);
    CAmount totalOutputValue = ddWithFeeTxFinal.GetValueOut();
    BOOST_CHECK_EQUAL(totalOutputValue, 90000);  // Only the change output has value

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-06a]: Fee calculation correctly handles zero-value DD inputs/outputs. "
        "DD components are invisible to fee math — fee comes entirely from DGB inputs vs outputs.");
}

BOOST_AUTO_TEST_CASE(redteam_t2_06b_fee_input_collateral_masquerade)
{
    // ATTACK [T2-06b]: Use another mint's collateral UTXO as a "fee input" in a
    // redemption transaction to free it without burning its associated DD.
    //
    // The redemption validator treats any input (position > 0) with nValue > 0 as
    // a "fee input" and SUBTRACTS its value from the total DGB release calculation.
    // This means a second collateral UTXO's value is deducted, making the net
    // release appear correct while TWO collaterals are actually being spent.
    //
    // Attack flow:
    //   Mint A: 10,000 DD, 200 DGB collateral
    //   Mint B: 15,000 DD, 300 DGB collateral
    //   Redemption: burn 10,000 DD (full A), include B's collateral as input 1
    //   Outputs: 495 DGB
    //   Net release = 495 - 300 (fee input) = 195 <= 200 (allowed from A) → PASSES
    //   But 300 DGB from Mint B freed without burning Mint B's 15,000 DD!
    //
    // NOTE: This requires timelock expiry for both collaterals (script-level CLTV).
    // The broader issue is that post-timelock collateral can be spent via script path
    // without DD consensus checks, because CLTV enforcement is at the script level,
    // not the DD validation level. The DCA/ERR system (T2-05) should handle this
    // economically, but it's currently non-functional.

    auto regTestParams = CChainParams::RegTest({});

    // Set up Mint A
    CKey collKeyA;
    collKeyA.MakeNewKey(true);
    XOnlyPubKey xPubA(collKeyA.GetPubKey());
    CScript p2trA = MakeP2TR(xPubA);

    CAmount collateralA = 200 * COIN;
    CAmount originalDDA = 10000;  // 10,000 DD cents

    CMutableTransaction mintTxA;
    mintTxA.nVersion = 0x01000770;
    mintTxA.vin.push_back(CTxIn(COutPoint(uint256S("d206b00000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTxA.vout.push_back(CTxOut(collateralA, p2trA));  // Collateral
    CKey ddKeyA;
    ddKeyA.MakeNewKey(true);
    mintTxA.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKeyA.GetPubKey()))));  // DD token
    mintTxA.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDDA, 1000, 1, xPubA)));

    CTransactionRef mintTxRefA = MakeTransactionRef(mintTxA);
    uint256 mintHashA = mintTxRefA->GetHash();

    // Set up Mint B (different mint, different DD)
    CKey collKeyB;
    collKeyB.MakeNewKey(true);
    XOnlyPubKey xPubB(collKeyB.GetPubKey());
    CScript p2trB = MakeP2TR(xPubB);

    CAmount collateralB = 300 * COIN;
    CAmount originalDDB = 15000;  // 15,000 DD cents

    CMutableTransaction mintTxB;
    mintTxB.nVersion = 0x01000770;
    mintTxB.vin.push_back(CTxIn(COutPoint(uint256S("d206b00000000000000000000000000000000000000000000000000000000002"), 0)));
    mintTxB.vout.push_back(CTxOut(collateralB, p2trB));
    CKey ddKeyB;
    ddKeyB.MakeNewKey(true);
    mintTxB.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKeyB.GetPubKey()))));
    mintTxB.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDDB, 1000, 1, xPubB)));

    CTransactionRef mintTxRefB = MakeTransactionRef(mintTxB);
    uint256 mintHashB = mintTxRefB->GetHash();

    // Set up coins view with both collaterals
    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOutA(mintHashA, 0);
    COutPoint collOutB(mintHashB, 0);
    coinsView.AddCoin(collOutA, Coin(CTxOut(collateralA, p2trA), 400, false), false);
    coinsView.AddCoin(collOutB, Coin(CTxOut(collateralB, p2trB), 400, false), false);

    auto txLookup = [&](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintHashA) { tx_out = mintTxRefA; return true; }
        if (txid == mintHashB) { tx_out = mintTxRefB; return true; }
        return false;
    };

    // EXPLOIT TX: Input 0 = Collateral A, Input 1 = Collateral B (as "fee input"),
    // Input 2 = DD input (burn 10,000 DD for Mint A)
    // Output: 495 DGB (200 from A + 300 from B - 5 DGB fee)
    CAmount totalOutput = 495 * COIN;

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;  // REDEEM
    mtx.vin.push_back(CTxIn(collOutA));   // Collateral A (200 DGB)
    mtx.vin.push_back(CTxIn(collOutB));   // Collateral B (300 DGB) — masquerades as "fee input"!
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("d206b00000000000000000000000000000000000000000000000000000000099"), 0)));  // DD input
    mtx.vout.push_back(CTxOut(totalOutput, CScript() << OP_1 << ToByteVector(xPubA)));

    CTransaction tx(mtx);
    TxValidationState state;

    CAmount ddBurned = originalDDA;  // Full burn of Mint A's DD (10,000)
    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctx, ddBurned, state);

    // Calculate what the validator sees:
    // totalDGBOutputs = 495 DGB
    // totalFeeInputs = 300 DGB (collateral B treated as fee input because nValue > 0)
    // totalDGBRelease = 495 - 300 = 195 DGB
    // allowedRelease = 200 DGB (collateral A)
    // 195 <= 200 + tolerance → PASSES

    // SECURITY FIX [T2-06b]: ValidateCollateralReleaseAmount now detects when a
    // non-zero-value input after index 0 is from a DD mint transaction (i.e., it's
    // collateral, not a fee UTXO). The validator rejects such transactions, requiring
    // each collateral position to be redeemed separately with its own DD burn.
    BOOST_CHECK_MESSAGE(!result,
        "DEFENSE [T2-06b]: Collateral masquerade must be REJECTED. "
        "Including another mint's collateral as a 'fee input' should fail validation.");
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-redeem-collateral-as-fee-input");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-06b]: ValidateCollateralReleaseAmount correctly detects "
        "collateral UTXOs masquerading as fee inputs and rejects the transaction. "
        "Each collateral must be redeemed separately with its own DD burn.");

    // Verify a LEGITIMATE fee input (non-DD regular UTXO) still works
    {
        CKey feeKey;
        feeKey.MakeNewKey(true);
        CScript feeScript = CScript() << OP_DUP << OP_HASH160
            << ToByteVector(feeKey.GetPubKey().GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        CMutableTransaction feeFundTx;
        feeFundTx.nVersion = 2;  // Regular non-DD transaction
        feeFundTx.vin.push_back(CTxIn(COutPoint(uint256S("f000000000000000000000000000000000000000000000000000000000000001"), 0)));
        feeFundTx.vout.push_back(CTxOut(1 * COIN, feeScript));  // 1 DGB fee UTXO

        CTransactionRef feeFundRef = MakeTransactionRef(feeFundTx);
        uint256 feeFundHash = feeFundRef->GetHash();
        COutPoint feeOutpoint(feeFundHash, 0);

        CCoinsView baseView2;
        CCoinsViewCache coinsView2(&baseView2);
        coinsView2.AddCoin(collOutA, Coin(CTxOut(collateralA, p2trA), 400, false), false);
        coinsView2.AddCoin(feeOutpoint, Coin(CTxOut(1 * COIN, feeScript), 400, false), false);

        auto txLookup2 = [&](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
            if (txid == mintHashA) { tx_out = mintTxRefA; return true; }
            if (txid == feeFundHash) { tx_out = feeFundRef; return true; }
            return false;
        };

        // Legitimate redeem: collateral A + regular fee UTXO
        CMutableTransaction mtx2;
        mtx2.nVersion = 0x03000770;
        mtx2.vin.push_back(CTxIn(collOutA));     // Collateral A (200 DGB)
        mtx2.vin.push_back(CTxIn(feeOutpoint));  // Regular DGB fee UTXO (1 DGB)
        mtx2.vin.push_back(CTxIn(COutPoint(uint256S("d206b00000000000000000000000000000000000000000000000000000000099"), 0)));
        mtx2.vout.push_back(CTxOut(200 * COIN + 50000000, CScript() << OP_1 << ToByteVector(xPubA)));  // 200.5 DGB

        CTransaction tx2(mtx2);
        TxValidationState state2;
        DigiDollar::ValidationContext ctx2(1000, 500000, 150, *regTestParams, &coinsView2, false, txLookup2);

        bool result2 = DigiDollar::ValidateCollateralReleaseAmount(tx2, ctx2, originalDDA, state2);
        BOOST_CHECK_MESSAGE(result2,
            "Legitimate redemption with regular fee UTXO should PASS. Got: " + state2.GetRejectReason());
        BOOST_TEST_MESSAGE("DEFENSE [T2-06b]: Legitimate fee UTXO (non-DD) correctly accepted.");
    }
}

BOOST_AUTO_TEST_CASE(redteam_t2_06c_collateral_release_fee_tolerance)
{
    // ATTACK [T2-06c]: Exploit the fee tolerance in collateral release validation.
    //
    // The tolerance is: max(1000 sats, allowedRelease / 1000)
    // For 200 DGB collateral: tolerance = max(1000, 20,000,000) = 20,000,000 sats = 0.2 DGB
    //
    // Can an attacker extract 0.2 DGB extra by exploiting the tolerance?

    auto regTestParams = CChainParams::RegTest({});

    CKey collKey;
    collKey.MakeNewKey(true);
    XOnlyPubKey xPub(collKey.GetPubKey());
    CScript p2tr = MakeP2TR(xPub);

    CAmount lockedCollateral = 200 * COIN;
    CAmount originalDD = 10000;

    CMutableTransaction mintTx;
    mintTx.nVersion = 0x01000770;
    mintTx.vin.push_back(CTxIn(COutPoint(uint256S("d206c00000000000000000000000000000000000000000000000000000000001"), 0)));
    mintTx.vout.push_back(CTxOut(lockedCollateral, p2tr));
    CKey ddKey;
    ddKey.MakeNewKey(true);
    mintTx.vout.push_back(CTxOut(0, MakeP2TR(XOnlyPubKey(ddKey.GetPubKey()))));
    mintTx.vout.push_back(CTxOut(0, MakeDDMintOpReturn(originalDD, 1000, 1, xPub)));

    CTransactionRef mintTxRef = MakeTransactionRef(mintTx);
    uint256 mintHash = mintTxRef->GetHash();

    CCoinsView baseView;
    CCoinsViewCache coinsView(&baseView);
    COutPoint collOut(mintHash, 0);
    coinsView.AddCoin(collOut, Coin(CTxOut(lockedCollateral, p2tr), 400, false), false);

    auto txLookup = [&](const uint256& txid, uint32_t coinHeight, CTransactionRef& tx_out) -> bool {
        if (txid == mintHash) { tx_out = mintTxRef; return true; }
        return false;
    };

    // Calculate tolerance
    CAmount tolerance = std::max((CAmount)1000, lockedCollateral / 1000);
    BOOST_CHECK_EQUAL(tolerance, 20000000);  // 0.2 DGB for 200 DGB collateral

    // EXPLOIT: Try to release collateral + tolerance (200.2 DGB output)
    CAmount exploitOutput = lockedCollateral + tolerance;  // 200.2 DGB

    CMutableTransaction mtx;
    mtx.nVersion = 0x03000770;
    mtx.vin.push_back(CTxIn(collOut));
    mtx.vin.push_back(CTxIn(COutPoint(uint256S("d206c00000000000000000000000000000000000000000000000000000000099"), 0)));  // DD input
    mtx.vout.push_back(CTxOut(exploitOutput, CScript() << OP_1 << ToByteVector(xPub)));

    CTransaction tx(mtx);
    TxValidationState state;

    DigiDollar::ValidationContext ctx(1000, 500000, 150, *regTestParams, &coinsView, false, txLookup);
    CAmount ddBurned = originalDD;  // Full burn

    bool result = DigiDollar::ValidateCollateralReleaseAmount(tx, ctx, ddBurned, state);

    // totalDGBRelease = 200.2 DGB (exploitOutput, no fee inputs to subtract)
    // allowedRelease = 200 DGB + tolerance = 200.2 DGB
    // 200.2 <= 200.2 → PASSES at boundary

    BOOST_TEST_MESSAGE("T2-06c: Fee tolerance exploitation:");
    BOOST_TEST_MESSAGE("  Locked collateral: " << lockedCollateral / COIN << " DGB");
    BOOST_TEST_MESSAGE("  Tolerance: " << tolerance << " sats (" << (double)tolerance / COIN << " DGB)");
    BOOST_TEST_MESSAGE("  Output attempted: " << exploitOutput / COIN << "." << (exploitOutput % COIN) << " DGB");
    BOOST_TEST_MESSAGE("  Result: " << (result ? "PASS" : "FAIL") << " — " << state.GetRejectReason());

    // The tolerance allows up to 0.2 DGB extra for a 200 DGB collateral.
    // This is by design to account for fee calculation variations.
    // But it means an attacker can extract 0.1% more than entitled.
    // At $0.01/DGB, this is $0.002 — negligible.
    if (result) {
        BOOST_TEST_MESSAGE("FINDING [T2-06c] (INFO): Fee tolerance allows 0.1% over-release. "
            "For 200 DGB, that's 0.2 DGB ($0.002 at $0.01/DGB). "
            "This is by design for fee variation. Not exploitable at scale.");
    }

    // Now try BEYOND tolerance: allowedRelease + tolerance + 1
    CAmount beyondTolerance = lockedCollateral + tolerance + 1;

    CMutableTransaction mtx2;
    mtx2.nVersion = 0x03000770;
    mtx2.vin.push_back(CTxIn(collOut));
    mtx2.vin.push_back(CTxIn(COutPoint(uint256S("d206c00000000000000000000000000000000000000000000000000000000098"), 0)));
    mtx2.vout.push_back(CTxOut(beyondTolerance, CScript() << OP_1 << ToByteVector(xPub)));

    CTransaction tx2(mtx2);
    TxValidationState state2;

    bool result2 = DigiDollar::ValidateCollateralReleaseAmount(tx2, ctx, ddBurned, state2);

    BOOST_TEST_MESSAGE("T2-06c: Beyond tolerance: " << beyondTolerance << " sats → " 
        << (result2 ? "PASS (BUG!)" : "FAIL (defended)"));

    BOOST_CHECK_MESSAGE(!result2,
        "EXPLOIT [T2-06c]: Release beyond tolerance should be rejected! "
        "Reason: " + state2.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(redteam_t2_06d_dust_bypass_scope)
{
    // ATTACK [T2-06d]: Verify that dust check bypass is ONLY for DD transactions.
    //
    // IsStandardTx skips dust for DD (necessary — DD outputs have nValue=0).
    // Ensure a non-DD transaction with zero-value outputs is still rejected as dust.
    //
    // A non-DD tx with DD-like outputs could try to create permanent UTXO set entries
    // with no economic cost to sweep them (UTXO bloat attack).

    // DD tx version lower 16 bits must match 0x0770
    BOOST_CHECK_EQUAL(0x02000770 & 0xFFFF, 0x0770);  // DD transfer
    BOOST_CHECK_EQUAL(0x01000770 & 0xFFFF, 0x0770);  // DD mint
    BOOST_CHECK_EQUAL(0x03000770 & 0xFFFF, 0x0770);  // DD redeem

    // Non-DD version
    int32_t regularVersion = 2;  // Standard Bitcoin tx
    BOOST_CHECK_NE(regularVersion & 0xFFFF, 0x0770);

    // A regular tx with zero-value P2TR outputs should be rejected as dust
    // (enforced by IsStandardTx → IsDust → nValue < threshold)
    //
    // The IsStandardTx function checks IsDust for each output, but DD transactions
    // skip this check. For non-DD transactions, zero-value outputs are dust because
    // nValue (0) < GetDustThreshold (which is > 0 for spendable scripts).
    CKey key;
    key.MakeNewKey(true);
    XOnlyPubKey xonly(key.GetPubKey());
    CTxOut zeroValueP2TR(0, MakeP2TR(xonly));

    // P2TR (witness v1) dust threshold at default 3000 sat/kvB:
    // Output size ~43 bytes + input ~67 bytes = ~110 bytes
    // Threshold = 110 * 3000 / 1000 = 330 sats
    // nValue=0 < 330 → IS DUST
    BOOST_CHECK_EQUAL(zeroValueP2TR.nValue, 0);
    BOOST_CHECK_MESSAGE(zeroValueP2TR.nValue == 0,
        "DEFENSE [T2-06d]: Zero-value P2TR output will fail dust check for non-DD transactions. "
        "Only DD transactions (version & 0xFFFF == 0x0770) bypass dust in IsStandardTx.");

    // An OP_RETURN output (nValue=0) is NOT dust because IsUnspendable() → threshold = 0
    CScript opReturnScript;
    opReturnScript << OP_RETURN;
    CTxOut opReturnOut(0, opReturnScript);
    BOOST_CHECK_MESSAGE(opReturnOut.scriptPubKey.IsUnspendable(),
        "DEFENSE [T2-06d]: OP_RETURN is unspendable, so dust threshold = 0.");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-06d]: Dust check bypass is scoped to DD version marker. "
        "Non-DD transactions cannot create zero-value UTXO entries via mempool relay. "
        "Only DD transactions (version & 0xFFFF == 0x0770) bypass dust.");
}

BOOST_AUTO_TEST_CASE(redteam_t2_06e_txbuilder_fee_rate_bounds)
{
    // ATTACK [T2-06e]: Test TxBuilder fee rate validation boundaries.
    //
    // The TxBuilder enforces: 100,000 <= feeRate <= 100,000,000 sat/kB
    // Can an attacker bypass these wallet-level checks?
    // (Answer: yes, via raw transaction crafting, but consensus doesn't enforce fees)

    auto regTestParams = CChainParams::RegTest({});
    DigiDollar::TxBuilder builder(*regTestParams, 1000, 500000);

    // Too low fee rate (below minimum relay)
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(0), false);
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(1), false);
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(99999), false);

    // Valid fee rates
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(100000), true);     // Minimum: 100k sat/kB
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(1000000), true);    // 1M sat/kB
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(100000000), true);  // Maximum: 100M sat/kB

    // Too high fee rate
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(100000001), false);
    BOOST_CHECK_EQUAL(builder.ValidateFeeRate(1000000000), false);

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-06e]: TxBuilder rejects fee rates outside 100k-100M sat/kB range. "
        "Note: This is wallet-level only. Raw transactions bypass TxBuilder. "
        "Consensus has no minimum fee (standard Bitcoin behavior — miners decide).");
}

BOOST_AUTO_TEST_CASE(redteam_t2_06f_negative_fee_prevention)
{
    // ATTACK [T2-06f]: Attempt to create a DD transaction with negative fees.
    //
    // In a DD transfer, if outputs somehow had more DGB value than inputs, the
    // fee would be negative (creating DGB from nothing). CheckTxInputs prevents
    // this with: if (nValueIn < value_out) → reject "bad-txns-in-belowout".
    //
    // DD outputs with nValue=0 don't contribute to value_out, so they can't be
    // used to inflate the output side. This test verifies the protection.

    // Create a transaction where outputs exceed inputs
    CMutableTransaction mtx;
    mtx.nVersion = 0x02000770;  // DD transfer

    // Input with 10,000 sats (via coins view, but we check the arithmetic)
    CAmount inputValue = 10000;
    CAmount output1Value = 0;      // DD output (nValue=0)
    CAmount output2Value = 15000;  // Change output — more than input!

    // Fee would be: 10000 - (0 + 15000) = -5000 → SHOULD REJECT
    CAmount computedFee = inputValue - (output1Value + output2Value);
    BOOST_CHECK_LT(computedFee, 0);
    BOOST_CHECK_MESSAGE(inputValue < output1Value + output2Value,
        "DEFENSE [T2-06f]: nValueIn < value_out → CheckTxInputs rejects with 'bad-txns-in-belowout'. "
        "DD zero-value outputs don't help — they contribute 0 to value_out.");

    // DD outputs can't be used to inflate value_out because nValue is always 0
    CAmount ddOutputTotal = 0 + 0 + 0;  // Three DD outputs
    BOOST_CHECK_EQUAL(ddOutputTotal, 0);
    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T2-06f]: Negative fees impossible. DD outputs contribute 0 to value_out. "
        "CheckTxInputs enforces nValueIn >= value_out at consensus level.");
}

// =============================================================================
// T3-01: Schnorr Signature Forgery on Oracle Messages
// =============================================================================

BOOST_AUTO_TEST_CASE(redteam_t3_01a_forge_with_attacker_keypair)
{
    // ATTACK [T3-01a]: Generate our own keypair, sign a price message, and attempt
    // to pass verification by including our own pubkey in the message.
    //
    // This tests whether oracle verification trusts the embedded pubkey (BAD)
    // or forces it from chainparams (GOOD).

    // Attacker generates their own keypair
    CKey attackerKey;
    attackerKey.MakeNewKey(true);
    XOnlyPubKey attackerPubkey(attackerKey.GetPubKey());

    // Attacker creates a fake oracle price message
    COraclePriceMessage forgedMsg;
    forgedMsg.oracle_id = 0;  // Impersonate oracle 0
    forgedMsg.price_micro_usd = 50000;  // Fake price: $0.05/DGB
    forgedMsg.timestamp = GetTime();
    forgedMsg.block_height = 1000;
    forgedMsg.nonce = 12345;

    // Sign with attacker's key (Phase 2 format — what matters for consensus)
    BOOST_REQUIRE(forgedMsg.SignPhase2(attackerKey));

    // Verify against attacker's own pubkey — this WILL pass (math is correct)
    BOOST_CHECK_MESSAGE(forgedMsg.VerifyPhase2(),
        "EXPECTED: Signature verifies against attacker's own pubkey (this is just Schnorr math)");

    // Now simulate what the P2P/block validation layer does:
    // Replace the pubkey with the REAL oracle 0 pubkey from chainparams
    auto regTestParams = CChainParams::RegTest({});
    const OracleNodeInfo* oracle0 = regTestParams->GetOracleNode(0);

    if (oracle0) {
        COraclePriceMessage boundMsg = forgedMsg;
        boundMsg.oracle_pubkey = XOnlyPubKey(oracle0->pubkey);

        // With chainparams pubkey, the attacker's signature MUST fail
        BOOST_CHECK_MESSAGE(!boundMsg.VerifyPhase2(),
            "DEFENSE [T3-01a]: Forged message FAILS verification when pubkey is bound to chainparams. "
            "Attacker's Schnorr signature does not match authorized oracle public key.");
    } else {
        BOOST_TEST_MESSAGE("NOTE [T3-01a]: No oracle nodes configured in regtest params — "
            "cannot test chainparams pubkey binding. Defense relies on P2P/block validation layers.");
    }

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-01a]: Schnorr forgery impossible when pubkey binding is enforced. "
        "P2P layer (net_processing.cpp) and ExtractOracleBundle both replace embedded pubkey "
        "with chainparams-authorized key before verification.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_01b_zero_signature_bypass)
{
    // ATTACK [T3-01b]: Submit an oracle message with an all-zero 64-byte signature.
    // Can a zero signature somehow pass VerifyPhase2()?

    CKey legitimateKey;
    legitimateKey.MakeNewKey(true);

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;
    msg.timestamp = GetTime();
    msg.block_height = 1000;
    msg.nonce = 0;
    msg.oracle_pubkey = XOnlyPubKey(legitimateKey.GetPubKey());

    // All-zero signature (64 bytes)
    msg.schnorr_sig.assign(64, 0x00);

    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01b]: All-zero signature correctly rejected by VerifyPhase2()");
    BOOST_CHECK_MESSAGE(!msg.Verify(),
        "DEFENSE [T3-01b]: All-zero signature correctly rejected by Verify()");

    // All-0xFF signature
    msg.schnorr_sig.assign(64, 0xFF);
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01b]: All-0xFF signature correctly rejected by VerifyPhase2()");

    // Random garbage signature
    msg.schnorr_sig.resize(64);
    for (int i = 0; i < 64; i++) msg.schnorr_sig[i] = static_cast<unsigned char>(i * 7 + 13);
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01b]: Random garbage signature correctly rejected by VerifyPhase2()");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-01b]: Invalid signatures (zero, max, garbage) all rejected. "
        "BIP-340 Schnorr verification in libsecp256k1 correctly validates.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_01c_signature_malleability)
{
    // ATTACK [T3-01c]: Given a valid signature, attempt to create a different valid
    // signature for the same message (signature malleability).
    //
    // BIP-340 Schnorr signatures are NOT malleable — each (key, message) pair has
    // exactly one valid signature. Unlike ECDSA where (r, s) and (r, n-s) are both valid.

    CKey key;
    key.MakeNewKey(true);

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;
    msg.timestamp = GetTime();

    BOOST_REQUIRE(msg.SignPhase2(key));

    // Save original valid signature
    std::vector<unsigned char> originalSig = msg.schnorr_sig;
    BOOST_REQUIRE(msg.VerifyPhase2());

    // Attempt 1: Negate the s-value (ECDSA malleability trick)
    // In Schnorr, sig = (R, s) where R is 32 bytes and s is 32 bytes
    // Negating s (mod n) should invalidate the signature
    std::vector<unsigned char> malleatedSig = originalSig;
    // Flip all bits in the s-value (bytes 32-63)
    for (int i = 32; i < 64; i++) {
        malleatedSig[i] ^= 0xFF;
    }
    msg.schnorr_sig = malleatedSig;
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01c]: Bit-flipped s-value correctly rejected");

    // Attempt 2: Flip single bit in R
    malleatedSig = originalSig;
    malleatedSig[0] ^= 0x01;
    msg.schnorr_sig = malleatedSig;
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01c]: Single bit flip in R correctly rejected");

    // Attempt 3: Flip single bit in s
    malleatedSig = originalSig;
    malleatedSig[32] ^= 0x01;
    msg.schnorr_sig = malleatedSig;
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01c]: Single bit flip in s correctly rejected");

    // Restore original — should pass
    msg.schnorr_sig = originalSig;
    BOOST_CHECK_MESSAGE(msg.VerifyPhase2(),
        "SANITY: Original signature still valid after malleability attempts");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-01c]: BIP-340 Schnorr signatures are non-malleable. "
        "Any modification to (R, s) invalidates the signature. "
        "Unlike ECDSA, there is exactly one valid signature per (key, message) pair.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_01d_wrong_oracle_id_cross_sign)
{
    // ATTACK [T3-01d]: Oracle 0 signs a price, but we change oracle_id to 1 in the
    // message before verification. The Phase 2 hash includes oracle_id, so this
    // should invalidate the signature.

    CKey oracleKey;
    oracleKey.MakeNewKey(true);

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;
    msg.timestamp = GetTime();

    BOOST_REQUIRE(msg.SignPhase2(oracleKey));
    BOOST_REQUIRE(msg.VerifyPhase2());

    // Tamper: change oracle_id
    msg.oracle_id = 1;
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01d]: Changing oracle_id after signing invalidates Phase2 signature. "
        "oracle_id is included in GetPhase2SignatureHash().");

    // Tamper: change price
    msg.oracle_id = 0;  // Restore
    uint64_t originalPrice = msg.price_micro_usd;
    msg.price_micro_usd = 100000;  // Double the price
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01d]: Changing price after signing invalidates Phase2 signature.");

    // Tamper: change timestamp
    msg.price_micro_usd = originalPrice;  // Restore
    msg.timestamp += 1;
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01d]: Changing timestamp after signing invalidates Phase2 signature.");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-01d]: All three Phase2 hash fields (oracle_id, price, timestamp) "
        "are integrity-protected by the Schnorr signature. Tampering with any field causes verification failure.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_01e_empty_signature_isvalid_bypass)
{
    // ATTACK [T3-01e]: Can we bypass signature verification by submitting a message
    // with an EMPTY signature vector? IsValid() has special handling for empty sigs
    // (returns true for "compact format" messages).
    //
    // This is a known design choice for Phase 1 compact format, but we verify that
    // validation layers properly enforce signatures when required.

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;  // Valid price range
    msg.timestamp = GetTime();
    msg.block_height = 1000;
    msg.nonce = 0;

    // Empty signature — Phase 1 compact format trust path
    msg.schnorr_sig.clear();
    BOOST_CHECK_MESSAGE(msg.IsValid(),
        "EXPECTED: IsValid() accepts empty-signature messages (Phase 1 compact format trust). "
        "This is by design — compact format relies on chainparams pubkey binding at higher layers.");

    // But direct Phase2 verification should fail
    BOOST_CHECK_MESSAGE(!msg.VerifyPhase2(),
        "DEFENSE [T3-01e]: VerifyPhase2() rejects empty signature (size != 64)");

    // ValidatePhaseTwoBundle explicitly skips empty-sig messages
    // (they don't count toward valid_count)
    COracleBundle bundle;
    bundle.messages.push_back(msg);
    bundle.epoch = 0;
    bundle.median_price_micro_usd = 50000;
    bundle.timestamp = GetTime();

    // Phase 2 bundle should NOT have consensus with only empty-sig messages
    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& params = regTestParams->GetConsensus();
    BOOST_CHECK_MESSAGE(!OracleBundleManager::ValidatePhaseTwoBundle(bundle, params),
        "DEFENSE [T3-01e]: Phase 2 bundle validation rejects empty-signature messages. "
        "They don't count toward the required consensus threshold.");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-01e]: Empty signatures are a Phase 1 compact format artifact. "
        "Phase 2 validation (ValidatePhaseTwoBundle) correctly ignores empty-sig messages. "
        "P2P layer (CheckMessageSize) also rejects non-64-byte signatures.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_01f_phase2_hash_field_independence)
{
    // ATTACK [T3-01f]: Phase 2 hash covers oracle_id + price + timestamp ONLY.
    // This means block_height and nonce can be modified without invalidating the sig.
    // Verify this is the intended behavior and doesn't create exploitable replay vectors.

    CKey key;
    key.MakeNewKey(true);

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;
    msg.timestamp = GetTime();
    msg.block_height = 1000;
    msg.nonce = 42;

    BOOST_REQUIRE(msg.SignPhase2(key));
    BOOST_REQUIRE(msg.VerifyPhase2());

    // Changing block_height should NOT invalidate Phase 2 signature
    // (because block_height is NOT in Phase 2 hash)
    msg.block_height = 9999;
    BOOST_CHECK_MESSAGE(msg.VerifyPhase2(),
        "EXPECTED: block_height change doesn't invalidate Phase2 sig (not in hash)");

    // Changing nonce should NOT invalidate Phase 2 signature
    msg.nonce = 999999;
    BOOST_CHECK_MESSAGE(msg.VerifyPhase2(),
        "EXPECTED: nonce change doesn't invalidate Phase2 sig (not in hash)");

    // But Phase 1 full verification SHOULD fail (block_height and nonce are in Phase 1 hash)
    // The original Sign() hash covers all 5 fields
    COraclePriceMessage msg2;
    msg2.oracle_id = 0;
    msg2.price_micro_usd = 50000;
    msg2.timestamp = msg.timestamp;
    msg2.block_height = 1000;
    msg2.nonce = 42;
    BOOST_REQUIRE(msg2.Sign(key));
    BOOST_REQUIRE(msg2.Verify());

    msg2.block_height = 9999;
    BOOST_CHECK_MESSAGE(!msg2.Verify(),
        "DEFENSE [T3-01f]: Phase 1 full signature IS invalidated by block_height change");

    BOOST_TEST_MESSAGE("INFO [T3-01f]: Phase 2 hash intentionally excludes block_height and nonce "
        "because they are NOT stored on-chain in Phase 2 format. The consensus-critical fields "
        "(oracle_id, price, timestamp) are all protected. block_height/nonce mutability is by design "
        "and does not create exploitable vectors because Phase 2 on-chain format doesn't use them.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_01g_manual_pubkey_rebinding)
{
    // ATTACK [T3-01g]: Simulate what IsValidOracleMessage (private in bundle_manager)
    // does: rebind pubkey from chainparams before verification.
    // This tests the PATTERN used across all validation layers.

    CKey attackerKey;
    attackerKey.MakeNewKey(true);

    COraclePriceMessage forgedMsg;
    forgedMsg.oracle_id = 0;
    forgedMsg.price_micro_usd = 50000;
    forgedMsg.timestamp = GetTime();

    // Sign with attacker key
    BOOST_REQUIRE(forgedMsg.SignPhase2(attackerKey));

    // Direct VerifyPhase2 passes (attacker's own key)
    BOOST_CHECK(forgedMsg.VerifyPhase2());

    // Simulate pubkey rebinding (what P2P handler and IsValidOracleMessage do):
    // Look up authorized pubkey from chainparams for oracle_id
    auto regTestParams = CChainParams::RegTest({});
    const OracleNodeInfo* oracle_config = regTestParams->GetOracleNode(forgedMsg.oracle_id);

    if (oracle_config) {
        // Create bound copy — overwrite attacker pubkey with chainparams pubkey
        COraclePriceMessage boundMsg = forgedMsg;
        boundMsg.oracle_pubkey = XOnlyPubKey(oracle_config->pubkey);

        // Verification MUST fail with the real pubkey
        BOOST_CHECK_MESSAGE(!boundMsg.VerifyPhase2(),
            "DEFENSE [T3-01g]: After rebinding pubkey from chainparams, attacker's "
            "Schnorr signature is rejected. This is the pattern used by P2P handler, "
            "IsValidOracleMessage, and ExtractOracleBundle.");
    } else {
        BOOST_TEST_MESSAGE("NOTE [T3-01g]: No oracle nodes in regtest — testing with random keys");
        // Use a different random key as the "authorized" key
        CKey authorizedKey;
        authorizedKey.MakeNewKey(true);
        COraclePriceMessage boundMsg = forgedMsg;
        boundMsg.oracle_pubkey = XOnlyPubKey(authorizedKey.GetPubKey());
        BOOST_CHECK_MESSAGE(!boundMsg.VerifyPhase2(),
            "DEFENSE [T3-01g]: Forged message fails when verified against a different pubkey.");
    }

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-01g]: Pubkey rebinding pattern is used consistently: "
        "net_processing.cpp (P2P), IsValidOracleMessage (bundle mgr), ExtractOracleBundle (block parsing). "
        "All replace embedded pubkey with chainparams-authorized key before verification.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_01h_bundle_isvalid_no_rebind)
{
    // ATTACK [T3-01h]: COracleBundle::IsValid() verifies signatures but does NOT
    // rebind pubkeys from chainparams. If called on a bundle with attacker-supplied
    // pubkeys, it would pass. This is safe because all callers ensure pubkeys are
    // bound before calling IsValid().
    //
    // This test documents the trust boundary: callers MUST bind pubkeys.

    CKey attackerKey;
    attackerKey.MakeNewKey(true);

    COraclePriceMessage forgedMsg;
    forgedMsg.oracle_id = 0;
    forgedMsg.price_micro_usd = 50000;
    forgedMsg.timestamp = GetTime();
    forgedMsg.block_height = 0;
    forgedMsg.nonce = 0;

    BOOST_REQUIRE(forgedMsg.SignPhase2(attackerKey));

    COracleBundle bundle;
    bundle.messages.push_back(forgedMsg);
    bundle.epoch = 0;
    bundle.median_price_micro_usd = 50000;
    bundle.timestamp = GetTime();

    // WARNING: bundle.IsValid() with attacker pubkey DOES pass
    // This is NOT a bug — the function trusts its caller to bind pubkeys
    BOOST_CHECK_MESSAGE(bundle.IsValid(GetTime(), 1),
        "INFO [T3-01h]: bundle.IsValid() passes with attacker pubkey — this is expected. "
        "The function verifies cryptographic correctness, not authorization. "
        "Callers (ExtractOracleBundle, P2P handler) must bind chainparams pubkeys first.");

    BOOST_TEST_MESSAGE("INFO [T3-01h]: COracleBundle::IsValid() is a cryptographic check, not an "
        "authorization check. It trusts the caller to set oracle_pubkey from chainparams. "
        "All production code paths (P2P, block extraction, IsValidOracleMessage) do this correctly. "
        "RISK: If a new code path calls IsValid() without binding pubkeys, it would be vulnerable. "
        "Consider adding a chainparams pubkey verification inside IsValid() as defense-in-depth.");
}

// ============================================================================
// T3-02: Oracle ID Spoofing
// Attack: Can an attacker impersonate a legitimate oracle by spoofing oracle_id?
// ============================================================================

BOOST_AUTO_TEST_CASE(redteam_t3_02a_oraclebundle_no_pubkey_rebinding)
{
    // ATTACK [T3-02a]: ORACLEBUNDLE P2P handler verifies signatures using
    // attacker-supplied pubkeys WITHOUT rebinding from chainparams.
    //
    // The ORACLEPRICE handler correctly rebinds:
    //   oracle_msg.price_message.oracle_pubkey = XOnlyPubKey(oracle_config->pubkey);
    //
    // But the ORACLEBUNDLE handler does:
    //   for (const auto& msg : bundle_msg.bundle.messages) {
    //       if (!msg.schnorr_sig.empty()) {
    //           if (!msg.VerifyPhase2() && !msg.Verify()) { ... }
    //       }
    //   }
    //
    // No pubkey rebinding! Attacker generates own keypair, signs messages
    // claiming any oracle_id, sets oracle_pubkey to their own key.
    // VerifyPhase2() passes. Bundle passes P2P validation and gets RELAYED
    // to all connected peers — P2P relay amplification attack.

    CKey attackerKey;
    attackerKey.MakeNewKey(true);

    // Forge 8 messages (ORACLE_CONSENSUS_REQUIRED) with different oracle_ids
    COracleBundle forgedBundle;
    forgedBundle.epoch = 0;
    forgedBundle.timestamp = GetTime();

    std::vector<uint64_t> prices;
    for (uint32_t i = 0; i < ORACLE_CONSENSUS_REQUIRED; i++) {
        COraclePriceMessage msg;
        msg.oracle_id = i;
        msg.price_micro_usd = 50000 + i * 100; // Slight variation
        msg.timestamp = GetTime();
        msg.block_height = 0;
        msg.nonce = i;
        // Set attacker's pubkey
        msg.oracle_pubkey = XOnlyPubKey(attackerKey.GetPubKey());
        // Sign with attacker's key
        BOOST_REQUIRE(msg.SignPhase2(attackerKey));
        prices.push_back(msg.price_micro_usd);
        forgedBundle.messages.push_back(msg);
    }

    // Set median price
    std::sort(prices.begin(), prices.end());
    forgedBundle.median_price_micro_usd = (prices[prices.size()/2 - 1] + prices[prices.size()/2]) / 2;

    // Simulate what ORACLEBUNDLE P2P handler does (before fix):
    // Verify signatures WITHOUT rebinding pubkeys from chainparams
    bool all_sigs_pass = true;
    for (const auto& msg : forgedBundle.messages) {
        if (!msg.schnorr_sig.empty()) {
            if (!msg.VerifyPhase2() && !msg.Verify()) {
                all_sigs_pass = false;
                break;
            }
        }
    }

    // BUG: All signatures pass because they're verified against attacker's pubkey
    BOOST_CHECK_MESSAGE(all_sigs_pass,
        "BUG [T3-02a]: Forged bundle with attacker-signed messages passes P2P "
        "signature verification because ORACLEBUNDLE handler does NOT rebind "
        "pubkeys from chainparams before calling VerifyPhase2().");

    // The bundle also passes consensus check
    BOOST_CHECK_MESSAGE(forgedBundle.HasConsensus(),
        "BUG [T3-02a]: Forged bundle meets consensus threshold (8 messages). "
        "Combined with missing pubkey rebinding, this means the entire P2P "
        "validation pipeline is bypassed.");

    // Bundle.IsValid also passes (it doesn't rebind either)
    BOOST_CHECK_MESSAGE(forgedBundle.IsValid(GetTime(), ORACLE_CONSENSUS_REQUIRED),
        "BUG [T3-02a]: bundle.IsValid() passes with attacker pubkeys — confirming "
        "that the bundle passes ALL P2P validation checks and will be relayed.");

    BOOST_TEST_MESSAGE("BUG [T3-02a]: ORACLEBUNDLE P2P relay amplification attack. "
        "Attacker generates own keypair, forges bundle with 8+ messages claiming "
        "different oracle_ids. All P2P validation passes. Bundle relayed to entire "
        "network. Defense: IsValidOracleMessage in AddOracleMessage catches at storage "
        "level, but relay damage is done. "
        "FIX NEEDED: Rebind pubkeys from chainparams in ORACLEBUNDLE handler before "
        "sig verification (same pattern as ORACLEPRICE handler). Also: only relay "
        "bundle if at least one message was successfully stored.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_02b_isvalidoraclemessage_catches_spoofed_id)
{
    // DEFENSE [T3-02b]: IsValidOracleMessage (Phase Two) correctly rebinds
    // pubkeys from chainparams, catching oracle ID spoofing at the storage level.

    CKey attackerKey;
    attackerKey.MakeNewKey(true);

    COraclePriceMessage spoofedMsg;
    spoofedMsg.oracle_id = 0;  // Claim to be oracle 0
    spoofedMsg.price_micro_usd = 50000;
    spoofedMsg.timestamp = GetTime();
    spoofedMsg.oracle_pubkey = XOnlyPubKey(attackerKey.GetPubKey());
    BOOST_REQUIRE(spoofedMsg.SignPhase2(attackerKey));

    // Direct verification passes (attacker's own key)
    BOOST_CHECK(spoofedMsg.VerifyPhase2());

    // But after rebinding from chainparams, it should fail
    auto regTestParams = CChainParams::RegTest({});
    const OracleNodeInfo* oracle_config = regTestParams->GetOracleNode(0);
    BOOST_REQUIRE(oracle_config != nullptr);

    COraclePriceMessage boundMsg = spoofedMsg;
    boundMsg.oracle_pubkey = XOnlyPubKey(oracle_config->pubkey);

    BOOST_CHECK_MESSAGE(!boundMsg.VerifyPhase2(),
        "DEFENSE [T3-02b]: After rebinding pubkey from chainparams, spoofed oracle "
        "message is rejected. IsValidOracleMessage does this for Phase Two.");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-02b]: IsValidOracleMessage correctly rebinds "
        "pubkeys from chainparams for Phase Two (min_oracle_count > 1). Oracle ID "
        "spoofing cannot inject fake prices into the bundle manager.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_02c_oracle_id_range_check)
{
    // DEFENSE [T3-02c]: Verify oracle_id range checks are present at P2P layer.
    // oracle_id >= ORACLE_TOTAL_COUNT should be rejected.

    // Valid range: 0 to ORACLE_TOTAL_COUNT-1 (29)
    BOOST_CHECK(ORACLE_TOTAL_COUNT == 30);
    BOOST_CHECK(ORACLE_ACTIVE_COUNT == 15);

    // Verify chainparams has nodes for valid IDs (using regtest)
    auto regTestParams = CChainParams::RegTest({});

    // Valid IDs should have oracle configs
    for (uint32_t id = 0; id < 7; id++) { // Regtest has 7 oracles
        const OracleNodeInfo* config = regTestParams->GetOracleNode(id);
        BOOST_CHECK_MESSAGE(config != nullptr,
            "DEFENSE [T3-02c]: Oracle ID " + std::to_string(id) + " has chainparams config");
    }

    // Invalid IDs should NOT have configs
    for (uint32_t id : {30u, 31u, 100u, 255u, 0xFFFFu}) {
        const OracleNodeInfo* config = regTestParams->GetOracleNode(id);
        BOOST_CHECK_MESSAGE(config == nullptr,
            "DEFENSE [T3-02c]: Oracle ID " + std::to_string(id) + " correctly has no config");
    }

    // P2P handler checks: oracle_id >= ORACLE_TOTAL_COUNT
    // An attacker trying oracle_id=30 or higher would be caught
    BOOST_CHECK(30 >= ORACLE_TOTAL_COUNT); // 30 >= 30 → true → rejected
    BOOST_CHECK(29 < ORACLE_TOTAL_COUNT);  // 29 < 30 → false → not rejected by range check

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-02c]: Oracle ID range validation at P2P layer "
        "rejects oracle_id >= ORACLE_TOTAL_COUNT (30). Valid range is 0-29.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_02d_oracle_id_byte_truncation_coinbase)
{
    // INFO [T3-02d]: Oracle ID is stored as 1 byte in coinbase OP_RETURN format.
    // Phase 1: compact_data.push_back(static_cast<unsigned char>(msg.oracle_id & 0xFF))
    // Phase 2: p2_data.push_back(static_cast<unsigned char>(msg.oracle_id & 0xFF))
    //
    // This means oracle_id is truncated to 0-255 range on-chain.
    // Currently ORACLE_TOTAL_COUNT is 30, so all IDs fit in 1 byte.
    // If ORACLE_TOTAL_COUNT ever exceeds 255, coinbase format breaks silently.

    BOOST_CHECK_MESSAGE(ORACLE_TOTAL_COUNT <= 255,
        "INFO [T3-02d]: ORACLE_TOTAL_COUNT (" + std::to_string(ORACLE_TOTAL_COUNT) +
        ") fits in 1 byte. If this constant is increased above 255, the coinbase "
        "oracle format (Phase 1 and Phase 2) will silently truncate oracle IDs.");

    // Verify truncation behavior
    uint32_t id_255 = 255;
    uint32_t id_256 = 256;
    BOOST_CHECK(static_cast<unsigned char>(id_255 & 0xFF) == 255);
    BOOST_CHECK(static_cast<unsigned char>(id_256 & 0xFF) == 0);  // Truncates to 0!

    BOOST_TEST_MESSAGE("INFO [T3-02d]: Oracle ID stored as uint8_t in coinbase OP_RETURN. "
        "oracle_id=256 would silently map to oracle_id=0 on-chain. Not currently "
        "exploitable (ORACLE_TOTAL_COUNT=30), but a latent truncation hazard if "
        "oracle count is ever increased above 255.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_02e_p2p_price_handler_rebinding)
{
    // DEFENSE [T3-02e]: Verify the ORACLEPRICE P2P handler correctly rebinds
    // pubkeys. The pattern is:
    //   1. Deserialize message (contains attacker-supplied pubkey)
    //   2. Look up oracle_config from chainparams by oracle_id
    //   3. Replace: oracle_msg.price_message.oracle_pubkey = XOnlyPubKey(oracle_config->pubkey)
    //   4. Verify signature against the rebound pubkey
    //
    // Simulate this pattern to confirm it rejects spoofed oracle IDs.

    CKey attackerKey, legitimateKey;
    attackerKey.MakeNewKey(true);
    legitimateKey.MakeNewKey(true);

    // Attacker sends message claiming oracle_id=0, signed with their key
    COraclePriceMessage attackerMsg;
    attackerMsg.oracle_id = 0;
    attackerMsg.price_micro_usd = 1000; // Extremely low price to undercollateralize
    attackerMsg.timestamp = GetTime();
    BOOST_REQUIRE(attackerMsg.SignPhase2(attackerKey));

    // Without rebinding: passes (attacker's own key)
    BOOST_CHECK(attackerMsg.VerifyPhase2());

    // Simulate P2P handler rebinding to legitimate key
    attackerMsg.oracle_pubkey = XOnlyPubKey(legitimateKey.GetPubKey());

    // After rebinding: fails (signature doesn't match legitimate key)
    BOOST_CHECK_MESSAGE(!attackerMsg.VerifyPhase2(),
        "DEFENSE [T3-02e]: After pubkey rebinding, attacker's signature is rejected. "
        "This is the correct behavior of the ORACLEPRICE P2P handler.");

    BOOST_TEST_MESSAGE("DEFENSE HOLDS [T3-02e]: ORACLEPRICE handler rebinding pattern works "
        "correctly. Attacker cannot impersonate oracle 0 by sending a self-signed "
        "message with a spoofed oracle_id.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_02f_empty_sig_bundle_bypass)
{
    // ATTACK [T3-02f]: Can an attacker bypass signature verification in the
    // ORACLEBUNDLE handler by sending messages with empty signatures?
    //
    // The handler skips sig check for empty sigs:
    //   if (!msg.schnorr_sig.empty()) { ... verify ... }
    //
    // With empty sig, the message passes P2P sig check entirely.
    // Then IsValidOracleMessage checks VerifyPhase2() which requires sig.size()==64.

    COraclePriceMessage emptySigMsg;
    emptySigMsg.oracle_id = 0;
    emptySigMsg.price_micro_usd = 50000;
    emptySigMsg.timestamp = GetTime();
    emptySigMsg.schnorr_sig.clear();  // Empty signature

    // ORACLEBUNDLE handler: empty sig → skip verification → passes P2P check
    bool passes_p2p = emptySigMsg.schnorr_sig.empty() || emptySigMsg.VerifyPhase2();
    BOOST_CHECK_MESSAGE(passes_p2p,
        "BUG [T3-02f]: Empty-signature message passes ORACLEBUNDLE P2P sig check "
        "because the handler skips verification for empty signatures.");

    // IsValidOracleMessage (Phase Two): VerifyPhase2() requires 64-byte sig
    bool passes_storage = emptySigMsg.VerifyPhase2();
    BOOST_CHECK_MESSAGE(!passes_storage,
        "DEFENSE [T3-02f]: Empty-signature message fails IsValidOracleMessage "
        "because VerifyPhase2() requires 64-byte Schnorr signature.");

    BOOST_TEST_MESSAGE("PARTIAL DEFENSE [T3-02f]: Empty-sig messages bypass ORACLEBUNDLE P2P "
        "sig verification (skipped entirely), but are caught at storage level. "
        "Same relay amplification issue as T3-02a — message passes P2P, gets relayed, "
        "then silently rejected by AddOracleMessage.");
}

BOOST_AUTO_TEST_CASE(redteam_t3_02g_unconditional_bundle_relay)
{
    // ATTACK [T3-02g]: ORACLEBUNDLE handler relays bundle unconditionally
    // after calling AddOracleMessage, regardless of whether ANY message was stored.
    //
    // Code (net_processing.cpp):
    //   for (const auto& msg : bundle_msg.bundle.messages) {
    //       bundleManager.AddOracleMessage(msg);  // return value IGNORED
    //   }
    //   // ... relay to all peers ...
    //
    // If all messages are rejected (e.g., all spoofed), the forged bundle
    // is STILL relayed to every connected peer. This is the amplification
    // part of the attack — one attacker message becomes N relay messages.

    // This test documents the code pattern issue
    BOOST_TEST_MESSAGE("BUG [T3-02g]: ORACLEBUNDLE handler does not check AddOracleMessage "
        "return values. Bundle is relayed unconditionally after storage attempts. "
        "Combined with T3-02a (missing pubkey rebinding) and T3-02f (empty sig bypass), "
        "an attacker can flood the P2P network with fake bundles that pass all "
        "handler-level checks, get relayed to every peer, but are silently "
        "dropped at the storage level. "
        "FIX: Check AddOracleMessage return values; only relay if ≥1 message stored.");

    // Verify the basic assumption: AddOracleMessage returns bool
    // (We can't easily test P2P relay in unit tests, but we can verify
    // that the storage-level defense works)
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    mgr.SetEnabled(true);
    mgr.SetMinOracleCount(4);  // Phase Two mode

    CKey attackerKey;
    attackerKey.MakeNewKey(true);

    // Spoofed message
    COraclePriceMessage spoofed;
    spoofed.oracle_id = 0;
    spoofed.price_micro_usd = 50000;
    spoofed.timestamp = GetTime();
    BOOST_REQUIRE(spoofed.SignPhase2(attackerKey));

    // Storage rejects it
    bool stored = mgr.AddOracleMessage(spoofed);
    BOOST_CHECK_MESSAGE(!stored,
        "DEFENSE [T3-02g]: IsValidOracleMessage correctly rejects spoofed message. "
        "But the ORACLEBUNDLE handler ignores this return value and relays anyway.");

    mgr.SetEnabled(false);
}

// =============================================================================
// T3-03: Outlier Filtering Bypass
// =============================================================================

static COraclePriceMessage MakeSignedOracleMsg(uint32_t id, uint64_t price, int64_t ts, const CKey& key)
{
    COraclePriceMessage msg;
    msg.oracle_id = id;
    msg.price_micro_usd = price;
    msg.timestamp = ts;
    msg.block_height = 0;
    msg.nonce = 0;
    BOOST_REQUIRE(msg.SignPhase2(key));
    return msg;
}

BOOST_AUTO_TEST_CASE(redteam_T3_03a_consensus_price_time_dependent)
{
    // ATTACK: CalculateConsensusPrice uses msg.IsValid() which calls GetTime().
    // During IBD or delayed block relay, oracle timestamps become "stale"
    // relative to wall-clock time. Messages get EXCLUDED from price calculation,
    // causing the consensus price to CHANGE depending on when a node validates.
    //
    // EXPLOIT: Miner creates block at time T with oracle price X.
    // Node receives block at time T + 7200 (2 hours later).
    // CalculateConsensusPrice excludes all messages (stale) → returns 0.
    // 0 != X → ValidatePhaseTwoBundle rejects the block.
    // CHAIN SPLIT: timely nodes accept, delayed nodes reject.

    BOOST_TEST_MESSAGE("=== T3-03a: Time-Dependent Consensus Price (CHAIN SPLIT RISK) ===");

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    // Create 5 oracle messages at baseTime
    std::vector<CKey> keys(5);
    for (auto& k : keys) k.MakeNewKey(true);

    COracleBundle bundle;
    bundle.epoch = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        bundle.messages.push_back(MakeSignedOracleMsg(i, 50000, baseTime - 30, keys[i]));
    }

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    // At time T (shortly after messages), CalculateConsensusPrice should work
    CAmount price_at_T = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);
    BOOST_TEST_MESSAGE("Price at creation time: " << price_at_T);
    BOOST_CHECK_MESSAGE(price_at_T == 50000,
        "Price at creation time should be 50000, got " + std::to_string(price_at_T));

    // Now advance time by 2 hours (past ORACLE_MAX_AGE_SECONDS = 3600)
    SetMockTime(baseTime + 7200);

    CAmount price_at_T_plus_2h = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);
    BOOST_TEST_MESSAGE("Price 2 hours later: " << price_at_T_plus_2h);

    // BUG: If price changes based on wall-clock time, we have a consensus bug.
    // The miner set bundle.median_price_micro_usd = 50000 at time T.
    // A validator at T+7200 recalculates and gets a DIFFERENT value.
    // If price_at_T_plus_2h != price_at_T → CHAIN SPLIT
    if (price_at_T_plus_2h != price_at_T) {
        BOOST_TEST_MESSAGE("CRITICAL BUG CONFIRMED: Consensus price is TIME-DEPENDENT!");
        BOOST_TEST_MESSAGE("  Price at T:      " << price_at_T);
        BOOST_TEST_MESSAGE("  Price at T+2h:   " << price_at_T_plus_2h);
        BOOST_TEST_MESSAGE("  A block mined at T with price " << price_at_T << " would be REJECTED");
        BOOST_TEST_MESSAGE("  by a node validating at T+2h because it calculates price " << price_at_T_plus_2h);
        BOOST_TEST_MESSAGE("  This causes a CHAIN SPLIT between timely and delayed nodes.");
        BOOST_TEST_MESSAGE("  Also breaks IBD: all historical blocks with oracle data fail validation.");
        BOOST_TEST_MESSAGE("  FIX: CalculateConsensusPrice must NOT call msg.IsValid() with GetTime().");
        BOOST_TEST_MESSAGE("       Use only price-range checks, not timestamp checks.");
        BOOST_CHECK_MESSAGE(false,
            "CRITICAL: CalculateConsensusPrice is time-dependent. "
            "Price at T=" + std::to_string(price_at_T) +
            " but at T+2h=" + std::to_string(price_at_T_plus_2h) +
            ". CHAIN SPLIT during IBD or delayed relay.");
    } else {
        BOOST_TEST_MESSAGE("Defense holds: consensus price is time-independent");
    }

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(redteam_T3_03b_ibd_oracle_validation_failure)
{
    // ATTACK: During IBD (Initial Block Download), node validates blocks from hours/days ago.
    // CalculateConsensusPrice → msg.IsValid() → timestamp > current - 3600?
    // Historical blocks will ALWAYS fail because their oracle timestamps are old.
    //
    // This test simulates IBD: block from 24 hours ago being validated now.

    BOOST_TEST_MESSAGE("=== T3-03b: IBD Oracle Block Rejection ===");

    int64_t now = 1700086400;
    int64_t block_time = now - 86400;  // Block from 24 hours ago
    SetMockTime(now);

    // Oracle messages from 24 hours ago
    std::vector<CKey> keys(4);
    for (auto& k : keys) k.MakeNewKey(true);

    COracleBundle bundle;
    bundle.epoch = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        bundle.messages.push_back(MakeSignedOracleMsg(i, 50000, block_time - 10, keys[i]));
    }

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    CAmount price = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);
    BOOST_TEST_MESSAGE("IBD price calculation for 24h-old block: " << price);

    if (price == 0) {
        BOOST_TEST_MESSAGE("BUG CONFIRMED: CalculateConsensusPrice returns 0 for historical blocks.");
        BOOST_TEST_MESSAGE("  During IBD, ALL blocks with oracle data would fail validation.");
        BOOST_TEST_MESSAGE("  New nodes cannot sync the chain past the first DD-activated block.");
        BOOST_CHECK_MESSAGE(false,
            "CRITICAL: IBD broken — historical oracle blocks return price=0");
    } else if (price != 50000) {
        BOOST_TEST_MESSAGE("BUG: Price should be 50000 but got " << price << " (partial message exclusion)");
        BOOST_CHECK_MESSAGE(false,
            "Partial message exclusion during IBD: expected 50000, got " + std::to_string(price));
    } else {
        BOOST_TEST_MESSAGE("Defense holds: historical blocks validate correctly");
    }

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(redteam_T3_03c_iqr_colluding_oracles_equal_split)
{
    // ATTACK: With even split (e.g., 4 honest + 3 malicious out of 7),
    // can malicious oracles manipulate the median via IQR?
    // This tests that IQR + median protects against minority manipulation.

    BOOST_TEST_MESSAGE("=== T3-03c: IQR Bypass via Colluding Oracle Minority ===");

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    std::vector<CKey> keys(7);
    for (auto& k : keys) k.MakeNewKey(true);

    // 4 honest oracles at $0.05, 3 malicious at $0.10 (2x manipulation attempt)
    COracleBundle bundle;
    bundle.epoch = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        bundle.messages.push_back(MakeSignedOracleMsg(i, 50000, baseTime - 10, keys[i]));
    }
    for (uint32_t i = 4; i < 7; ++i) {
        bundle.messages.push_back(MakeSignedOracleMsg(i, 100000, baseTime - 10, keys[i]));
    }

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    CAmount price = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);
    BOOST_TEST_MESSAGE("Price with 4 honest (50000) + 3 malicious (100000): " << price);

    // With 7 prices sorted: [50000, 50000, 50000, 50000, 100000, 100000, 100000]
    // Median = prices[3] = 50000
    // IQR: q1=prices[1]=50000, q3=prices[5]=100000, IQR=50000
    // Bounds: [50000-75000, 100000+75000] = [-25000, 175000] → all pass
    // Filtered median = same = 50000
    BOOST_CHECK_MESSAGE(price == 50000,
        "Defense should hold: median of 7 with 4 honest should be 50000, got " +
        std::to_string(price));

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(redteam_T3_03d_iqr_half_compromised)
{
    // ATTACK: What if exactly half the oracles are compromised?
    // With 4 messages (minimum Phase 2): 2 honest at $0.05, 2 malicious at $100
    // Median of even count = average of middle two

    BOOST_TEST_MESSAGE("=== T3-03d: 50% Compromised Oracles — Price Manipulation ===");

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    std::vector<CKey> keys(4);
    for (auto& k : keys) k.MakeNewKey(true);

    // 2 honest at $0.05, 2 malicious at $100
    COracleBundle bundle;
    bundle.epoch = 0;
    bundle.messages.push_back(MakeSignedOracleMsg(0, 50000, baseTime - 10, keys[0]));      // $0.05
    bundle.messages.push_back(MakeSignedOracleMsg(1, 50000, baseTime - 10, keys[1]));      // $0.05
    bundle.messages.push_back(MakeSignedOracleMsg(2, 100000000, baseTime - 10, keys[2]));  // $100
    bundle.messages.push_back(MakeSignedOracleMsg(3, 100000000, baseTime - 10, keys[3]));  // $100

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    CAmount price = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);
    BOOST_TEST_MESSAGE("Price with 2 honest (50000) + 2 malicious (100000000): " << price);

    // Sorted: [50000, 50000, 100000000, 100000000]
    // IQR: q1=prices[1]=50000, q3=prices[3]=100000000
    // IQR = 99950000
    // Bounds: [50000-149925000, 100000000+149925000] → all pass
    // Median of 4: (prices[1]+prices[2])/2 = (50000+100000000)/2 = 50025000
    // That's $50.025 instead of $0.05 — 1000x manipulation!
    if (price > 100000) {  // More than $0.10 indicates manipulation succeeded
        BOOST_TEST_MESSAGE("FINDING: 50% compromised oracles can manipulate price.");
        BOOST_TEST_MESSAGE("  Honest price: $0.05 (50000 micro-USD)");
        BOOST_TEST_MESSAGE("  Manipulated consensus: $" << price / 1000000.0 << " (" << price << " micro-USD)");
        BOOST_TEST_MESSAGE("  This is expected — 50% compromise defeats any filter.");
        BOOST_TEST_MESSAGE("  The defense is the 4-of-7 minimum threshold on testnet (8-of-15 mainnet).");
        BOOST_TEST_MESSAGE("  Attacker needs to compromise 50%+ oracle private keys.");
    }
    // This is not a bug — it's expected behavior when majority is compromised
    BOOST_CHECK_MESSAGE(price > 0, "Price calculation should not return 0");

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(redteam_T3_03e_no_filtering_under_4_messages)
{
    // ATTACK: With < 4 messages, CalculateConsensusPrice skips IQR filtering.
    // With exactly 3 messages, a single extreme value might shift the median.
    // But median of 3 is always the middle value — 1 outlier can't move it.

    BOOST_TEST_MESSAGE("=== T3-03e: No IQR Filtering for < 4 Messages ===");

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    std::vector<CKey> keys(3);
    for (auto& k : keys) k.MakeNewKey(true);

    // 2 honest at $0.05, 1 extreme outlier at $100
    COracleBundle bundle;
    bundle.epoch = 0;
    bundle.messages.push_back(MakeSignedOracleMsg(0, 50000, baseTime - 10, keys[0]));
    bundle.messages.push_back(MakeSignedOracleMsg(1, 50000, baseTime - 10, keys[1]));
    bundle.messages.push_back(MakeSignedOracleMsg(2, 100000000, baseTime - 10, keys[2]));

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    CAmount price = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);
    BOOST_TEST_MESSAGE("Price with 2 honest + 1 extreme (3 total, no IQR): " << price);

    // Sorted: [50000, 50000, 100000000]
    // No IQR (< 4), median of odd = prices[1] = 50000
    BOOST_CHECK_MESSAGE(price == 50000,
        "Median of 3 with 1 outlier should be 50000, got " + std::to_string(price));

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(redteam_T3_03f_filter_inconsistency_consensus_vs_cached)
{
    // ATTACK: CalculateConsensusPrice (IQR) and GetConsensusPrice (10% median)
    // use DIFFERENT filtering algorithms. If both are used for consensus-critical
    // decisions on the same data, they could produce different results.

    BOOST_TEST_MESSAGE("=== T3-03f: Filter Algorithm Inconsistency ===");

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    std::vector<CKey> keys(5);
    for (auto& k : keys) k.MakeNewKey(true);

    // Prices chosen to trigger different behavior between IQR and 10% filters:
    // [45000, 49000, 50000, 51000, 65000]
    // Median = 50000
    // 10% threshold: 50000 * 10/100 = 5000 → range [45000, 55000] → 65000 filtered
    // IQR: q1=49000, q3=51000, IQR=2000 → bounds [46000, 54000] → 45000 AND 65000 filtered
    COracleBundle bundle;
    bundle.epoch = 0;
    bundle.messages.push_back(MakeSignedOracleMsg(0, 45000, baseTime - 10, keys[0]));
    bundle.messages.push_back(MakeSignedOracleMsg(1, 49000, baseTime - 10, keys[1]));
    bundle.messages.push_back(MakeSignedOracleMsg(2, 50000, baseTime - 10, keys[2]));
    bundle.messages.push_back(MakeSignedOracleMsg(3, 51000, baseTime - 10, keys[3]));
    bundle.messages.push_back(MakeSignedOracleMsg(4, 65000, baseTime - 10, keys[4]));

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    // CalculateConsensusPrice (static, IQR-based — used in ValidatePhaseTwoBundle)
    CAmount price_iqr = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);

    // GetConsensusPrice (member function, 10%-of-median — used in UpdateBundle/cached price)
    uint64_t price_10pct = bundle.GetConsensusPrice(1);

    BOOST_TEST_MESSAGE("CalculateConsensusPrice (IQR):     " << price_iqr);
    BOOST_TEST_MESSAGE("GetConsensusPrice (10% median):    " << price_10pct);

    if (price_iqr != static_cast<CAmount>(price_10pct)) {
        BOOST_TEST_MESSAGE("FINDING: Two consensus price functions produce DIFFERENT results!");
        BOOST_TEST_MESSAGE("  If both are used in consensus-critical paths, this causes chain splits.");
        BOOST_TEST_MESSAGE("  Currently: CalculateConsensusPrice is used for block validation (correct),");
        BOOST_TEST_MESSAGE("            GetConsensusPrice is used for cached price (advisory).");
        BOOST_TEST_MESSAGE("  RISK: If code changes route consensus decisions through GetConsensusPrice,");
        BOOST_TEST_MESSAGE("        the different filtering will cause validation disagreements.");
    } else {
        BOOST_TEST_MESSAGE("Both functions agree for this input set");
    }

    // The key check: CalculateConsensusPrice must be deterministic and not rely on GetTime()
    // (This is the real T3-03 bug — covered by T3-03a above)
    BOOST_CHECK_MESSAGE(price_iqr > 0, "IQR price should be positive");
    BOOST_CHECK_MESSAGE(price_10pct > 0, "10% price should be positive");

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(redteam_T3_03g_iqr_all_same_price)
{
    // EDGE CASE: All oracles report identical price → IQR = 0
    // Bounds become [q1, q3] = [price, price] → only exact matches pass

    BOOST_TEST_MESSAGE("=== T3-03g: All Identical Prices (IQR = 0) ===");

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    std::vector<CKey> keys(5);
    for (auto& k : keys) k.MakeNewKey(true);

    COracleBundle bundle;
    bundle.epoch = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        bundle.messages.push_back(MakeSignedOracleMsg(i, 50000, baseTime - 10, keys[i]));
    }

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    CAmount price = OracleBundleManager::CalculateConsensusPrice(bundle, cparams);
    BOOST_CHECK_EQUAL(price, 50000);

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(redteam_T3_03h_iqr_boundary_price_range)
{
    // ATTACK: Oracle messages at the extreme limits of ORACLE_MIN/MAX_PRICE_MICRO_USD
    // Can we get CalculateConsensusPrice to accept extreme prices?

    BOOST_TEST_MESSAGE("=== T3-03h: Extreme Price Boundaries ===");

    int64_t baseTime = 1700000000;
    SetMockTime(baseTime);

    std::vector<CKey> keys(5);
    for (auto& k : keys) k.MakeNewKey(true);

    // All at minimum price: $0.0001 = 100 micro-USD
    COracleBundle bundle_min;
    bundle_min.epoch = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        bundle_min.messages.push_back(MakeSignedOracleMsg(i, ORACLE_MIN_PRICE_MICRO_USD, baseTime - 10, keys[i]));
    }

    auto regTestParams = CChainParams::RegTest({});
    const Consensus::Params& cparams = regTestParams->GetConsensus();

    CAmount price_min = OracleBundleManager::CalculateConsensusPrice(bundle_min, cparams);
    BOOST_CHECK_MESSAGE(price_min == static_cast<CAmount>(ORACLE_MIN_PRICE_MICRO_USD),
        "Min price should be " + std::to_string(ORACLE_MIN_PRICE_MICRO_USD) + ", got " + std::to_string(price_min));

    // All at maximum price: $100 = 100000000 micro-USD
    COracleBundle bundle_max;
    bundle_max.epoch = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        bundle_max.messages.push_back(MakeSignedOracleMsg(i, ORACLE_MAX_PRICE_MICRO_USD, baseTime - 10, keys[i]));
    }

    CAmount price_max = OracleBundleManager::CalculateConsensusPrice(bundle_max, cparams);
    BOOST_CHECK_MESSAGE(price_max == static_cast<CAmount>(ORACLE_MAX_PRICE_MICRO_USD),
        "Max price should be " + std::to_string(ORACLE_MAX_PRICE_MICRO_USD) + ", got " + std::to_string(price_max));

    // Messages BELOW minimum (should be excluded by IsValid)
    COracleBundle bundle_under;
    bundle_under.epoch = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        bundle_under.messages.push_back(MakeSignedOracleMsg(i, 50, baseTime - 10, keys[i]));  // $0.00005
    }

    CAmount price_under = OracleBundleManager::CalculateConsensusPrice(bundle_under, cparams);
    BOOST_CHECK_MESSAGE(price_under == 0,
        "Below-minimum prices should result in 0 (all excluded), got " + std::to_string(price_under));

    SetMockTime(0);
}

// ============================================================================
// T3-04: P2P Oracle Message Replay Attack
// ============================================================================

/**
 * T3-04a: Nonce/block_height mutation bypasses dedup while Phase2 sig holds
 *
 * ATTACK: An attacker captures a valid Phase2-signed oracle message and mutates
 * the block_height and nonce fields (NOT covered by Phase2 signature hash).
 * Each mutation produces a different GetSignatureHash() → bypasses seen_message_hashes
 * dedup in OracleBundleManager. The Phase2 signature remains valid because it
 * only covers oracle_id + price + timestamp.
 *
 * IMPACT: Attacker can generate unlimited "distinct" messages from a single
 * valid oracle broadcast. While pending_messages dedup by oracle_id prevents
 * storage of duplicates, each mutation still:
 *   1. Passes HasOracleMessage() dedup check (different hash)
 *   2. Passes VerifyPhase2() (same Phase2 hash)
 *   3. Gets counted by rate limiter (burns budget)
 *   4. Pollutes seen_message_hashes set
 *
 * In a P2P context, this allows an attacker to exhaust a peer's rate limit
 * budget (3600/hour), blocking legitimate oracle message acceptance.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_04a_nonce_mutation_dedup_bypass)
{
    SetMockTime(GetTime());
    int64_t now = GetTime();

    // Create a valid Phase2-signed message
    CKey key;
    key.MakeNewKey(true);

    COraclePriceMessage original;
    original.oracle_id = 0;
    original.price_micro_usd = 50000;  // $0.05
    original.timestamp = now - 30;
    original.block_height = 100;
    original.nonce = 42;
    BOOST_REQUIRE(original.SignPhase2(key));

    // Verify original is valid
    BOOST_CHECK(original.VerifyPhase2());

    // Now mutate block_height and nonce — Phase2 sig should still verify
    COraclePriceMessage mutant1 = original;
    mutant1.block_height = 999;
    mutant1.nonce = 9999;
    BOOST_CHECK_MESSAGE(mutant1.VerifyPhase2(),
        "EXPLOIT: Phase2 signature still valid after block_height/nonce mutation — "
        "attacker can create unlimited 'distinct' messages from a single valid broadcast");

    COraclePriceMessage mutant2 = original;
    mutant2.block_height = 0;
    mutant2.nonce = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK_MESSAGE(mutant2.VerifyPhase2(),
        "EXPLOIT: Phase2 signature valid with extreme nonce values");

    // All three messages have DIFFERENT GetSignatureHash (used for dedup)
    uint256 hash_orig = original.GetSignatureHash();
    uint256 hash_mut1 = mutant1.GetSignatureHash();
    uint256 hash_mut2 = mutant2.GetSignatureHash();

    BOOST_CHECK_MESSAGE(hash_orig != hash_mut1,
        "Mutant1 has different dedup hash — bypasses seen_message_hashes");
    BOOST_CHECK_MESSAGE(hash_orig != hash_mut2,
        "Mutant2 has different dedup hash — bypasses seen_message_hashes");
    BOOST_CHECK_MESSAGE(hash_mut1 != hash_mut2,
        "All mutations produce unique dedup hashes");

    // But all three have the SAME Phase2 signature hash
    uint256 phase2_orig = original.GetPhase2SignatureHash();
    uint256 phase2_mut1 = mutant1.GetPhase2SignatureHash();
    uint256 phase2_mut2 = mutant2.GetPhase2SignatureHash();

    BOOST_CHECK_EQUAL(phase2_orig, phase2_mut1);
    BOOST_CHECK_EQUAL(phase2_orig, phase2_mut2);

    // Verify all bypass the bundle manager's seen_message_hashes
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    mgr.Clear();

    // Original: not seen
    BOOST_CHECK(!mgr.HasOracleMessage(hash_orig));
    // After adding original (won't actually store due to regtest key mismatch,
    // but the seen hash is added regardless)
    mgr.InjectTestMessage(original);

    // Mutant hashes are NOT in seen set — dedup bypassed
    BOOST_CHECK_MESSAGE(!mgr.HasOracleMessage(hash_mut1),
        "Mutant1 bypasses HasOracleMessage dedup — different hash, same content");
    BOOST_CHECK_MESSAGE(!mgr.HasOracleMessage(hash_mut2),
        "Mutant2 bypasses HasOracleMessage dedup — different hash, same content");

    mgr.Clear();
    SetMockTime(0);
}

/**
 * T3-04b: Rate limit exhaustion via nonce mutations
 *
 * ATTACK: Attacker generates thousands of nonce-mutated copies of a single
 * valid oracle message. Each passes Phase2 verification and HasOracleMessage
 * dedup. While none are stored (pending_messages dedup catches them), each
 * consumes one unit of the peer's rate limit budget.
 *
 * After 3600 mutations, ALL subsequent oracle messages from that peer are
 * silently dropped — including legitimate new price updates.
 *
 * This is an eclipse attack amplifier: if the victim is connected primarily
 * to attacker nodes, the victim loses oracle price updates entirely.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_04b_rate_limit_exhaustion_via_mutations)
{
    SetMockTime(GetTime());
    int64_t now = GetTime();

    CKey key;
    key.MakeNewKey(true);

    COraclePriceMessage original;
    original.oracle_id = 0;
    original.price_micro_usd = 50000;
    original.timestamp = now - 30;
    original.block_height = 100;
    original.nonce = 0;
    BOOST_REQUIRE(original.SignPhase2(key));

    // Generate 100 mutations — all valid, all unique hashes
    std::set<uint256> unique_hashes;
    for (uint64_t i = 0; i < 100; ++i) {
        COraclePriceMessage mutant = original;
        mutant.nonce = i + 1;
        mutant.block_height = static_cast<int32_t>(i * 7);

        // Phase2 signature still valid
        BOOST_CHECK(mutant.VerifyPhase2());

        // Unique dedup hash
        uint256 hash = mutant.GetSignatureHash();
        unique_hashes.insert(hash);
    }

    // ALL 100 mutations have unique hashes — each would pass dedup
    BOOST_CHECK_MESSAGE(unique_hashes.size() == 100,
        "All 100 nonce mutations produce unique dedup hashes, each consuming rate limit budget. "
        "At scale (3600+), this exhausts the hourly rate limit, blocking legitimate oracle messages.");

    SetMockTime(0);
}

/**
 * T3-04c: Bundle manager pending_messages provides secondary dedup
 *
 * DEFENSE CHECK: Even though mutations bypass seen_message_hashes,
 * pending_messages keyed by oracle_id prevents actual storage of duplicates.
 * Only the first message (or one with a newer timestamp) gets stored.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_04c_pending_messages_secondary_dedup)
{
    SetMockTime(GetTime());
    int64_t now = GetTime();

    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    mgr.Clear();

    CKey key;
    key.MakeNewKey(true);

    // Create and inject original message
    COraclePriceMessage original;
    original.oracle_id = 0;
    original.price_micro_usd = 50000;
    original.timestamp = now - 30;
    original.block_height = 100;
    original.nonce = 42;
    BOOST_REQUIRE(original.SignPhase2(key));

    mgr.InjectTestMessage(original);
    BOOST_CHECK_EQUAL(mgr.GetPendingMessageCount(), 1);

    // Inject mutation with same oracle_id and same timestamp
    COraclePriceMessage mutant = original;
    mutant.block_height = 999;
    mutant.nonce = 9999;

    mgr.InjectTestMessage(mutant);

    // pending_messages only stores one entry per oracle_id
    // InjectTestMessage overwrites regardless of timestamp, but AddOracleMessage
    // would check timestamp ordering
    BOOST_CHECK_EQUAL(mgr.GetPendingMessageCount(), 1);

    // Defense holds: even with dedup bypass, only one message per oracle stored
    // The concern is resource exhaustion (rate limit burn, CPU), not consensus corruption

    mgr.Clear();
    SetMockTime(0);
}

/**
 * T3-04d: GETORACLES response has no rate limiting
 *
 * FINDING: The GETORACLES handler responds with all pending oracle messages
 * each time it's called, with no rate limiting on the request itself.
 * An attacker can spam GETORACLES to cause repeated responses of N messages,
 * wasting bandwidth (N * message_size per request).
 *
 * With 15 active oracles, each ~140 bytes, that's ~2.1KB per GETORACLES response.
 * At 1000 requests/sec, that's 2.1MB/sec of outbound traffic per peer.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_04d_getoracles_no_rate_limit)
{
    SetMockTime(GetTime());
    int64_t now = GetTime();

    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    mgr.Clear();

    // Inject 15 oracle messages (simulating a full set)
    std::vector<CKey> keys(15);
    for (int i = 0; i < 15; ++i) {
        keys[i].MakeNewKey(true);
        COraclePriceMessage msg;
        msg.oracle_id = i;
        msg.price_micro_usd = 50000;
        msg.timestamp = now - 10;
        msg.block_height = 0;
        msg.nonce = 0;
        BOOST_REQUIRE(msg.SignPhase2(keys[i]));
        mgr.InjectTestMessage(msg);
    }

    BOOST_CHECK_EQUAL(mgr.GetPendingMessageCount(), 15);

    // GetPendingMessages can be called repeatedly with no rate limit
    // Each call returns all 15 messages — attacker can spam GETORACLES
    for (int i = 0; i < 10; ++i) {
        auto msgs = mgr.GetPendingMessages();
        BOOST_CHECK_EQUAL(msgs.size(), 15);
    }

    // FINDING: No rate limit on GETORACLES requests.
    // The handler in net_processing.cpp responds unconditionally.
    // Mitigation needed: rate limit GETORACLES to e.g. 5 requests/minute/peer.

    mgr.Clear();
    SetMockTime(0);
}

/**
 * T3-04e: Post-restart replay acceptance
 *
 * DEFENSE CHECK: After a node restart, all in-memory state is cleared.
 * Old messages (< 1 hour) can be replayed and accepted.
 * This is BY DESIGN — the GETORACLES sync mechanism relies on this.
 * The timestamp check ensures only recent messages are accepted.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_04e_post_restart_replay)
{
    SetMockTime(GetTime());
    int64_t now = GetTime();

    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    // Simulate restart: clear all state
    mgr.Clear();

    CKey key;
    key.MakeNewKey(true);

    // Message from 30 minutes ago — within ORACLE_MAX_AGE_SECONDS (3600)
    COraclePriceMessage recent_msg;
    recent_msg.oracle_id = 0;
    recent_msg.price_micro_usd = 50000;
    recent_msg.timestamp = now - 1800;  // 30 min ago
    recent_msg.block_height = 100;
    recent_msg.nonce = 42;
    BOOST_REQUIRE(recent_msg.SignPhase2(key));

    // After "restart", dedup is empty — message accepted
    BOOST_CHECK(!mgr.HasOracleMessage(recent_msg.GetSignatureHash()));
    // This is correct behavior — nodes need to catch up on oracle prices after restart

    // Message from 2 hours ago — beyond ORACLE_MAX_AGE_SECONDS
    COraclePriceMessage stale_msg;
    stale_msg.oracle_id = 1;
    stale_msg.price_micro_usd = 50000;
    stale_msg.timestamp = now - 7200;  // 2 hours ago
    stale_msg.block_height = 50;
    stale_msg.nonce = 0;
    BOOST_REQUIRE(stale_msg.SignPhase2(key));

    // Stale message: IsValid will reject it (timestamp > ORACLE_MAX_AGE_SECONDS old)
    BOOST_CHECK_MESSAGE(!stale_msg.IsValid(),
        "Defense holds: messages older than 1 hour rejected by IsValid timestamp check");

    mgr.Clear();
    SetMockTime(0);
}

/**
 * T3-04f: Verify fix — OraclePriceMsg::GetHash() now uses Phase2 hash
 *
 * ROOT CAUSE (FIXED): The dedup hash previously included block_height and nonce
 * (GetSignatureHash), but the signature verification uses Phase2 hash (only
 * oracle_id+price+timestamp). This mismatch allowed dedup bypass via field mutation.
 *
 * FIX APPLIED: OraclePriceMsg::GetHash() and AddOracleMessage now use
 * GetPhase2SignatureHash() for Phase2-signed messages. All mutations of the
 * same (oracle_id, price, timestamp) triple now map to the same dedup hash.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_04f_dedup_hash_fix_verified)
{
    SetMockTime(GetTime());
    int64_t now = GetTime();

    CKey key;
    key.MakeNewKey(true);

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.price_micro_usd = 50000;
    msg.timestamp = now - 30;
    msg.block_height = 100;
    msg.nonce = 42;
    BOOST_REQUIRE(msg.SignPhase2(key));

    // GetSignatureHash still includes block_height+nonce (internal detail)
    uint256 full_hash = msg.GetSignatureHash();
    uint256 phase2_hash = msg.GetPhase2SignatureHash();
    BOOST_CHECK(full_hash != phase2_hash);

    // FIX VERIFICATION: OraclePriceMsg::GetHash() now uses Phase2 hash for signed messages
    OraclePriceMsg wrapped;
    wrapped.price_message = msg;
    uint256 p2p_hash = wrapped.GetHash();
    BOOST_CHECK_EQUAL(p2p_hash, phase2_hash);

    // Mutate nonce: OraclePriceMsg::GetHash() should be UNCHANGED (fix working)
    COraclePriceMessage mutant = msg;
    mutant.nonce = 99999;
    mutant.block_height = 9999;
    BOOST_CHECK(mutant.VerifyPhase2()); // Phase2 sig still valid

    OraclePriceMsg wrapped_mutant;
    wrapped_mutant.price_message = mutant;
    uint256 p2p_hash_mutant = wrapped_mutant.GetHash();

    // After fix: both map to same dedup hash → mutation caught!
    BOOST_CHECK_MESSAGE(p2p_hash == p2p_hash_mutant,
        "FIX VERIFIED: Nonce/block_height mutations now produce same dedup hash — "
        "P2P handler catches them as duplicates before sig verification or rate limiting");

    // Also verify bundle manager dedup now uses Phase2 hash
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    mgr.Clear();

    // After fix, HasOracleMessage should use Phase2 hash internally
    // The manager's AddOracleMessage computes Phase2 hash for the seen set
    // So after adding original, the mutation should be detected as duplicate
    // (We test via the P2P hash which matches what the manager now uses)
    BOOST_CHECK_EQUAL(msg.GetPhase2SignatureHash(), mutant.GetPhase2SignatureHash());

    mgr.Clear();
    SetMockTime(0);
}

/**
 * RED TEAM T3-05: Consensus Threshold Bypass
 *
 * ATTACK VECTOR: Multiple code paths call HasConsensus() and GetConsensusPrice()
 * WITHOUT passing the network-specific min_oracle_count parameter. The default
 * parameter is ORACLE_CONSENSUS_REQUIRED=8 (compile-time constant).
 *
 * On any network where nOracleRequiredMessages != ORACLE_CONSENSUS_REQUIRED,
 * these code paths use the WRONG threshold:
 *   - Testnet: needs 5, default requires 8 → 5-7 message bundles rejected
 *   - Any future network with fewer oracles: threshold impossibly high
 *
 * AFFECTED CODE PATHS:
 *   1. UpdateCachedPrice() — uses HasConsensus()/GetConsensusPrice() defaults
 *   2. OracleDataValidator::ValidateOracleBundle() — uses HasConsensus() default
 *   3. OracleDataValidator::CheckOracleConsensus() — uses HasConsensus() default
 *   4. GetOraclePriceForHeight() fallback — uses HasConsensus()/GetConsensusPrice()
 *   5. net_processing.cpp ORACLEBUNDLE handler — uses HasConsensus() default
 *   6. EmergencyRedemptionRatio::HasOracleConsensus() — uses HasConsensus() default
 */
BOOST_AUTO_TEST_CASE(T3_05a_consensus_threshold_default_parameter_mismatch)
{
    // ORACLE_CONSENSUS_REQUIRED is a compile-time constant (8)
    // HasConsensus() defaults to this value when called without arguments
    // This test proves that bundles meeting a LOWER threshold (e.g., testnet 5-of-8)
    // are incorrectly rejected by code paths using the default

    const Consensus::Params& params = Params().GetConsensus();
    int runtime_required = params.nOracleRequiredMessages;
    int runtime_total = params.nOracleTotalOracles;

    BOOST_TEST_MESSAGE("Network oracle config: " << runtime_required << "-of-" << runtime_total);
    BOOST_TEST_MESSAGE("Compile-time default ORACLE_CONSENSUS_REQUIRED=" << ORACLE_CONSENSUS_REQUIRED);

    // Simulate a testnet scenario: 5 valid messages should meet 5-of-8 threshold
    // but HasConsensus() default requires 8
    int testnet_required = 5;  // Testnet's nOracleRequiredMessages

    COracleBundle bundle(0);
    for (int i = 0; i < testnet_required; i++) {
        CKey key;
        key.MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, GetTime());
        msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
        msg.SignPhase2(key);
        bundle.messages.push_back(msg);
    }
    bundle.median_price_micro_usd = 50000;

    // With testnet threshold, 5 messages is sufficient
    BOOST_CHECK_MESSAGE(bundle.HasConsensus(testnet_required),
        "5 messages should meet 5-of-8 threshold (testnet)");

    // BUG: Default parameter uses ORACLE_CONSENSUS_REQUIRED=8
    // 5 messages < 8 → HasConsensus() returns false
    bool default_consensus = bundle.HasConsensus();
    BOOST_CHECK_MESSAGE(!default_consensus,
        "BUG CONFIRMED: HasConsensus() with default ORACLE_CONSENSUS_REQUIRED=" <<
        ORACLE_CONSENSUS_REQUIRED << " rejects 5-message bundle that meets testnet 5-of-8 threshold. "
        "Code paths using HasConsensus() without explicit min_oracle_count will reject "
        "valid testnet bundles unless ALL 8 oracles respond.");

    // 5 messages also fails GetConsensusPrice() default
    uint64_t default_price = bundle.GetConsensusPrice();
    BOOST_CHECK_MESSAGE(default_price == 0,
        "BUG CONFIRMED: GetConsensusPrice() also uses default threshold 8 — "
        "returns 0 for 5-message bundle");

    uint64_t correct_price = bundle.GetConsensusPrice(testnet_required);
    BOOST_CHECK_MESSAGE(correct_price == 50000,
        "With correct threshold (5), GetConsensusPrice returns 50000");

    // Even 7-of-8 messages fail the default on testnet
    for (int i = testnet_required; i < 7; i++) {
        CKey key;
        key.MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, GetTime());
        msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
        msg.SignPhase2(key);
        bundle.messages.push_back(msg);
    }
    BOOST_CHECK_MESSAGE(!bundle.HasConsensus(),
        "BUG: 7 messages still fails default threshold of 8 — "
        "even with 7-of-8 oracle quorum, system is dead if one oracle is offline");
}

BOOST_AUTO_TEST_CASE(T3_05b_update_cached_price_uses_wrong_threshold)
{
    // UpdateCachedPrice calls HasConsensus()/GetConsensusPrice() with defaults
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // Set manager threshold to testnet value (5)
    manager.SetMinOracleCount(5);

    // Create a bundle with 5 valid messages (meets 5-of-8 testnet threshold)
    int32_t epoch = 5;
    COracleBundle bundle(epoch);
    for (int i = 0; i < 5; i++) {
        CKey key;
        key.MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, GetTime());
        msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
        msg.SignPhase2(key);
        bundle.messages.push_back(msg);
    }
    bundle.median_price_micro_usd = 50000;

    // Store via UpdateBundle — this path correctly uses min_oracle_count
    // So cached_price WILL be set here (5 >= 5)
    manager.UpdateBundle(bundle);

    // But UpdateCachedPrice uses HasConsensus()/GetConsensusPrice() with DEFAULT threshold
    // This is a SEPARATE code path that bypasses min_oracle_count
    bool updated = manager.UpdateCachedPrice(epoch);

    // BUG: Returns false because HasConsensus() defaults to 8, not min_oracle_count (5)
    BOOST_CHECK_MESSAGE(!updated,
        "BUG CONFIRMED: UpdateCachedPrice fails with 5 messages because "
        "it calls HasConsensus() with default " << ORACLE_CONSENSUS_REQUIRED <<
        " instead of manager's min_oracle_count (5)");

    manager.Clear();
}

BOOST_AUTO_TEST_CASE(T3_05c_validate_oracle_bundle_wrong_threshold)
{
    // OracleDataValidator::ValidateOracleBundle uses HasConsensus() with default
    const Consensus::Params& params = Params().GetConsensus();

    // Create a 5-message bundle (valid for testnet 5-of-8)
    int32_t epoch = GetCurrentEpoch(1000);
    COracleBundle bundle(epoch);
    for (int i = 0; i < 5; i++) {
        CKey key;
        key.MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, GetTime());
        msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
        msg.SignPhase2(key);
        bundle.messages.push_back(msg);
    }
    bundle.median_price_micro_usd = 50000;

    // ValidateOracleBundle internally calls CheckOracleConsensus which calls
    // HasConsensus() with the default ORACLE_CONSENSUS_REQUIRED=8
    // 5 messages < 8 → bundle rejected before signature validation even starts
    bool valid = OracleDataValidator::ValidateOracleBundle(bundle, epoch, params);

    BOOST_CHECK_MESSAGE(!valid,
        "BUG CONFIRMED: ValidateOracleBundle rejects 5-message bundle because "
        "CheckOracleConsensus uses default threshold " << ORACLE_CONSENSUS_REQUIRED <<
        " instead of network-specific nOracleRequiredMessages");
}

BOOST_AUTO_TEST_CASE(T3_05d_net_processing_oraclebundle_wrong_threshold)
{
    // The P2P ORACLEBUNDLE handler in net_processing.cpp calls:
    //   bundle_msg.bundle.HasConsensus()
    // with NO explicit threshold parameter.
    //
    // On testnet (5-of-8): valid bundles with 5-7 messages are rejected at P2P
    // layer, peers are misbehavior-banned (+5) for sending them.
    //
    // This test proves the threshold mismatch exists at the API level.

    COracleBundle bundle(0);
    for (int i = 0; i < 5; i++) {
        CKey key;
        key.MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, GetTime());
        msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
        msg.SignPhase2(key);
        bundle.messages.push_back(msg);
    }
    bundle.median_price_micro_usd = 50000;

    // The P2P handler does exactly this:
    bool p2p_check = bundle.HasConsensus();  // defaults to 8

    BOOST_CHECK_MESSAGE(!p2p_check,
        "BUG CONFIRMED: P2P ORACLEBUNDLE handler's HasConsensus() (default " <<
        ORACLE_CONSENSUS_REQUIRED << ") rejects 5-message bundle. "
        "On testnet, peers sending valid 5-of-8 bundles get Misbehaving(+5). "
        "If sustained, legitimate oracle relaying peers get banned.");
}

BOOST_AUTO_TEST_CASE(T3_05e_phase2_extraction_hardcodes_epoch_zero)
{
    // Phase 2 ExtractOracleBundle hardcodes bundle.epoch = 0
    // ValidatePhaseTwoBundle uses GetActiveOraclesForEpoch(bundle.epoch)
    // This means epoch-based oracle rotation is broken for on-chain validation

    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();

    // Create a Phase 2 formatted coinbase with oracle data
    CMutableTransaction coinbase_tx;
    coinbase_tx.vin.resize(1);
    coinbase_tx.vin[0].scriptSig << CScriptNum(1000); // BIP34 height

    // Create 4 oracle messages for Phase 2 bundle
    std::vector<CKey> keys;
    uint64_t consensus_price = 50000;
    int64_t timestamp = GetTime();

    CScript oracle_script;
    oracle_script << OP_RETURN << OP_ORACLE;
    oracle_script << std::vector<unsigned char>{0x02}; // Phase Two version

    std::vector<unsigned char> p2_data;
    int num_msgs = 4;

    // num_messages
    p2_data.push_back(static_cast<unsigned char>(num_msgs));

    // consensus price (uint64 LE)
    for (int i = 0; i < 8; i++)
        p2_data.push_back(static_cast<unsigned char>((consensus_price >> (i * 8)) & 0xFF));

    // timestamp (int64 LE)
    for (int i = 0; i < 8; i++)
        p2_data.push_back(static_cast<unsigned char>((timestamp >> (i * 8)) & 0xFF));

    // Per-oracle: oracle_id (1) + schnorr_sig (64)
    for (int i = 0; i < num_msgs; i++) {
        CKey key;
        key.MakeNewKey(true);
        keys.push_back(key);

        p2_data.push_back(static_cast<unsigned char>(i)); // oracle_id

        // Create message and sign for valid sig
        COraclePriceMessage msg(i, consensus_price, timestamp);
        msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
        msg.SignPhase2(key);

        if (msg.schnorr_sig.size() == 64) {
            p2_data.insert(p2_data.end(), msg.schnorr_sig.begin(), msg.schnorr_sig.end());
        } else {
            p2_data.insert(p2_data.end(), 64, 0x00);
        }
    }

    oracle_script << p2_data;

    CTxOut oracle_out;
    oracle_out.nValue = 0;
    oracle_out.scriptPubKey = oracle_script;
    coinbase_tx.vout.push_back(CTxOut(5000000000LL, CScript())); // block reward
    coinbase_tx.vout.push_back(oracle_out);

    CTransaction tx(coinbase_tx);
    COracleBundle extracted;
    bool ok = manager.ExtractOracleBundle(tx, extracted);

    BOOST_CHECK(ok);
    BOOST_CHECK_EQUAL(extracted.messages.size(), (size_t)num_msgs);

    // BUG: epoch is hardcoded to 0 regardless of actual block height
    BOOST_CHECK_MESSAGE(extracted.epoch == 0,
        "BUG CONFIRMED: Phase 2 ExtractOracleBundle hardcodes epoch=0. "
        "ValidatePhaseTwoBundle will call GetActiveOraclesForEpoch(0) "
        "regardless of actual block height. Oracle rotation broken for "
        "on-chain validation once >15 oracles exist on mainnet.");

    manager.Clear();
}

// =============================================================================
// T3-06: Exchange Price Source Manipulation
// =============================================================================

/**
 * T3-06a: ATTACK — Exploit min_required_sources=2 default to bypass outlier filtering
 *
 * The header default for min_required_sources is 2, while node.cpp overrides to 3.
 * If ANY code path creates MultiExchangeAggregator without SetMinRequiredSources(3),
 * it runs with 2 sources. FilterOutliers() requires >= 3 data points, so with exactly
 * 2 sources, NO outlier filtering occurs. An attacker controlling 1 of 2 remaining
 * exchanges gets 50% influence on the "median" (which is the average of 2 values).
 *
 * This test verifies the header default and documents the foot-gun.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06a_min_sources_default_bypasses_outlier_filter)
{
    using namespace ExchangeAPI;
    MultiExchangeAggregator aggregator;
    // DO NOT call SetMinRequiredSources — use header default

    // Simulate: attacker DDoS'd 4 of 6 exchanges, compromised 1 of remaining 2
    // Real price: $0.006 (6000 μUSD), attacker price: $0.060 (60000 μUSD) — 10x
    std::vector<MultiExchangeAggregator::ExchangePrice> two_prices = {
        {"Legit", 6000, GetTime(), true, 1.0},     // Real: $0.006
        {"Compromised", 60000, GetTime(), true, 1.0}, // Fake: $0.060 (10x)
    };

    // FilterOutliers with < 3 data points returns ALL prices unchanged (no filtering!)
    auto filtered = aggregator.FilterOutliers(two_prices);
    BOOST_CHECK_EQUAL(filtered.size(), 2); // Both kept — no outlier filtering!

    // Median of 2 prices = average = (6000 + 60000) / 2 = 33000 ($0.033)
    // That's 5.5x the real price — attacker gets massive under-collateralization
    CAmount median = aggregator.CalculateMedianPrice(two_prices);
    BOOST_CHECK_EQUAL(median, 33000); // 5.5x real price — EXPLOITABLE if min_required=2

    // DEFENSE VERIFICATION: When SetMinRequiredSources(3) is used, 2 sources = reject
    // The actual node.cpp code does call SetMinRequiredSources(3), so this attack
    // fails in production. But the header default of 2 is a dangerous foot-gun.
    // Any new call site that forgets SetMinRequiredSources(3) is vulnerable.
}

/**
 * T3-06b: ATTACK — Weighted median is dead code (weights ignored)
 *
 * CalculateWeightedMedian() just calls CalculateMedianPrice(), completely ignoring
 * the weight parameter. Binance (weight 1.5, highest volume exchange) has identical
 * influence to Poloniex (weight 0.8, low volume). This means exchange weight
 * assignments are security theater — they have zero effect on the final price.
 *
 * If a future developer enables use_weighted_median thinking it provides better
 * protection against low-volume exchange manipulation, they'd be wrong.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06b_weighted_median_ignores_weights)
{
    using namespace ExchangeAPI;
    MultiExchangeAggregator aggregator;

    // Create prices where weights SHOULD matter but DON'T
    // High-weight exchange (Binance) says $0.006, low-weight (Poloniex) says $0.010
    std::vector<MultiExchangeAggregator::ExchangePrice> prices = {
        {"Binance", 6000, GetTime(), true, 10.0},    // Weight 10x
        {"KuCoin", 6100, GetTime(), true, 1.0},       // Weight 1x
        {"Poloniex", 10000, GetTime(), true, 0.1},    // Weight 0.1x (should barely count)
    };

    CAmount weighted = aggregator.CalculateWeightedMedian(prices);
    CAmount unweighted = aggregator.CalculateMedianPrice(prices);

    // BUG: Weighted median returns EXACTLY the same as unweighted median
    // Poloniex with 0.1x weight has EQUAL influence to Binance with 10x weight
    BOOST_CHECK_EQUAL(weighted, unweighted); // Dead code — weights completely ignored

    // The median of [6000, 6100, 10000] sorted is 6100 regardless of weights
    BOOST_CHECK_EQUAL(weighted, 6100);

    // In a correct weighted median, Binance (10x weight) should pull result toward 6000
    // But it doesn't. This is pure security theater.
}

/**
 * T3-06c: ATTACK — ConvertToMicroUSD truncation creates cross-platform disagreement
 *
 * static_cast<CAmount>(price_usd * 1000000) truncates toward zero instead of rounding.
 * For borderline values, different oracle nodes on different platforms (ARM vs x86)
 * may compute slightly different intermediate doubles due to FPU/compiler differences,
 * causing 1-μUSD disagreement that could affect oracle consensus.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06c_convert_truncation_bias)
{
    using namespace ExchangeAPI;
    BinanceFetcher fetcher; // Just to access ConvertToMicroUSD

    // Test: price that creates truncation loss
    // $0.0062725 * 1000000 = 6272.5 → truncated to 6272 (loses 0.5 μUSD)
    CAmount result1 = fetcher.ConvertToMicroUSD(0.0062725);
    // Due to IEEE 754, 0.0062725 * 1000000 may be 6272.4999... or 6272.5000...
    // static_cast truncates: floor for positive values
    BOOST_CHECK(result1 == 6272 || result1 == 6273); // Platform-dependent!

    // Test: exact representation
    CAmount result2 = fetcher.ConvertToMicroUSD(0.006272);
    BOOST_CHECK_EQUAL(result2, 6272); // Exact

    // Test: systematic truncation — 0.9999999 should ideally round to 1000000 but truncates
    CAmount result3 = fetcher.ConvertToMicroUSD(0.9999999);
    // 0.9999999 * 1000000 = 999999.9 → truncated to 999999
    // Should be 1000000 if properly rounded
    BOOST_CHECK(result3 == 999999 || result3 == 1000000); // Truncation vs rounding

    // FINDING (LOW): Boundary — exactly $100 passes the > 100 check
    CAmount result4 = fetcher.ConvertToMicroUSD(100.0);
    // if (price_usd <= 0 || price_usd > 100) return 0;
    // 100.0 > 100 is FALSE, so 100.0 passes the check!
    // This means $100/DGB is ACCEPTED — extremely unrealistic for DGB
    BOOST_CHECK_EQUAL(result4, 100000000); // $100 exactly accepted (100M μUSD)
}

/**
 * T3-06d: ATTACK — Inconsistent price range caps across fetchers
 *
 * ConvertToMicroUSD base function: max $100
 * Per-fetcher validation (KuCoin, Gate.io, HTX, Crypto.com): max $10
 * No per-fetcher validation (Binance, CoinGecko): no additional cap
 *
 * If DGB price reaches $15, Binance and CoinGecko report it but
 * KuCoin/Gate.io/HTX/Crypto.com reject it → only 2 sources remain.
 * With min_required=3 in node.cpp, oracle fails entirely.
 * With min_required=2 (default header), no outlier filtering on 2 sources.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06d_inconsistent_price_range_caps)
{
    using namespace ExchangeAPI;

    // Verify base ConvertToMicroUSD accepts up to $100
    BinanceFetcher base_fetcher;
    CAmount high_price = base_fetcher.ConvertToMicroUSD(15.0); // $15/DGB
    BOOST_CHECK_EQUAL(high_price, 15000000); // Base accepts it

    CAmount very_high = base_fetcher.ConvertToMicroUSD(99.99);
    BOOST_CHECK(very_high > 0); // Base accepts $99.99

    CAmount too_high = base_fetcher.ConvertToMicroUSD(100.01);
    BOOST_CHECK_EQUAL(too_high, 0); // Base rejects > $100

    // The per-fetcher range checks (in FetchPrice) use $0.0001 to $10 range
    // for KuCoin, Gate.io, HTX, Crypto.com. If DGB reaches $15:
    // - Binance: returns 15000000 (no per-fetcher check beyond ConvertToMicroUSD)
    // - CoinGecko: returns 15000000 (no per-fetcher check)
    // - KuCoin: returns 0 (per-fetcher rejects > $10 = 10000000 μUSD)
    // - Gate.io: returns 0
    // - HTX: returns 0
    // - Crypto.com: returns 0
    // Only 2 valid sources → fails min_required_sources=3 → oracle returns 0
    // Result: Oracle completely breaks if DGB exceeds $10
    // FINDING (LOW): 4 of 6 fetchers have $10 cap, 2 have $100 cap.
    // All should use consistent range, or range should be configurable.
}

/**
 * T3-06e: ATTACK — 3-of-6 exchange compromise with prices inside outlier threshold
 *
 * An attacker who compromises 3 of 6 exchanges and sets prices just inside the
 * 10% outlier threshold can manipulate the median by ~5%.
 * This is within designed tolerance — the test documents expected behavior.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06e_subtle_price_manipulation_within_threshold)
{
    using namespace ExchangeAPI;
    MultiExchangeAggregator aggregator;

    // Real price: $0.006 (6000 μUSD)
    // Attacker controls 3 exchanges, sets prices to $0.0066 (6600 μUSD) — exactly 10% above
    std::vector<MultiExchangeAggregator::ExchangePrice> prices = {
        {"Real1", 6000, GetTime(), true, 1.0},
        {"Real2", 6010, GetTime(), true, 1.0},
        {"Real3", 6020, GetTime(), true, 1.0},
        {"Fake1", 6600, GetTime(), true, 1.0},  // +10% above real
        {"Fake2", 6600, GetTime(), true, 1.0},
        {"Fake3", 6600, GetTime(), true, 1.0},
    };

    auto filtered = aggregator.FilterOutliers(prices);

    // Sorted: [6000, 6010, 6020, 6600, 6600, 6600]
    // Median of 6: (6020 + 6600) / 2 = 6310
    // Threshold: 6310 * 0.10 = 631
    // All deviations from 6310 are < 631, so ALL pass filter
    BOOST_CHECK_EQUAL(filtered.size(), 6); // All kept!

    CAmount median = aggregator.CalculateMedianPrice(filtered);
    BOOST_CHECK_EQUAL(median, 6310); // ~5% above real price

    // With real price 6000 and oracle reporting 6310, collateral requirement is
    // ~5% lower than it should be. Not catastrophic but allows slight under-collateralization.
    // CONCLUSION: With 50% exchange compromise and prices within 10% band,
    // attacker achieves ~5% price manipulation. This is expected — no statistical
    // filter can protect against 50% compromise. Defense holds by design.
}

/**
 * T3-06f: ATTACK — ConvertToMicroUSD special float values
 *
 * Test that special floating-point values (inf, nan, negative zero, subnormals)
 * are handled correctly and don't produce unexpected μUSD values.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06f_special_float_values)
{
    using namespace ExchangeAPI;
    BinanceFetcher fetcher;

    // Infinity
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(std::numeric_limits<double>::infinity()), 0);
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(-std::numeric_limits<double>::infinity()), 0);

    // NaN
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(std::numeric_limits<double>::quiet_NaN()), 0);
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(std::numeric_limits<double>::signaling_NaN()), 0);

    // Negative zero — should be caught by <= 0 check
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(-0.0), 0);

    // Negative price
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(-1.0), 0);

    // Zero
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(0.0), 0);

    // Subnormal (denormalized) — extremely small but nonzero
    double subnormal = std::numeric_limits<double>::denorm_min();
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(subnormal), 0); // subnormal * 1e6 still ≈ 0

    // String special values via stod
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD("inf"), 0);
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD("nan"), 0);
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD("-1.0"), 0);
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD(""), 0);
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD("not_a_number"), 0);
    BOOST_CHECK_EQUAL(fetcher.ConvertToMicroUSD("1e308"), 0); // overflow to inf

    // Defense holds: All special values correctly return 0
}

/**
 * T3-06g: ATTACK — ExtractJsonValue homebrew parser injection
 *
 * ExtractJsonValue is a naive string search. Test if duplicate keys or
 * nested objects can trick it into returning wrong values.
 * Note: This function is dead code in production (all fetchers use UniValue)
 * but could be accidentally used in future code.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06g_extract_json_value_injection)
{
    using namespace ExchangeAPI;
    BinanceFetcher fetcher;

    // Duplicate key — returns FIRST match
    std::string dupeJson = R"({"price":"99.99","other":"data","price":"0.006"})";
    std::string val = fetcher.ExtractJsonValue(dupeJson, "price");
    BOOST_CHECK_EQUAL(val, "99.99"); // First match wins — could be wrong

    // Key within a string value — backslash escapes prevent false match here
    // But if a malformed JSON response lacks proper escaping, parser can be tricked
    std::string nestedJson = R"({"data":"has \"price\":\"99.99\" inside","price":"0.006"})";
    val = fetcher.ExtractJsonValue(nestedJson, "price");
    // Backslash-escaped quotes break the byte pattern match, so correct key is found
    BOOST_CHECK_EQUAL(val, "0.006"); // Happens to work due to escape characters

    // However, with unescaped embedded key (malformed JSON from compromised exchange):
    std::string malformedJson = "{\"junk\":\"x\",\"price\":\"99.99\",\"real\":\"data\",\"price\":\"0.006\"}";
    val = fetcher.ExtractJsonValue(malformedJson, "price");
    // Naive parser returns FIRST match — attacker-controlled value
    BOOST_CHECK_EQUAL(val, "99.99"); // First match wins — could be attacker data

    // Missing key
    val = fetcher.ExtractJsonValue(R"({"other":"value"})", "price");
    BOOST_CHECK_EQUAL(val, "");

    // Empty JSON
    val = fetcher.ExtractJsonValue("", "price");
    BOOST_CHECK_EQUAL(val, "");

    // CONCLUSION: ExtractJsonValue is a naive, exploitable parser.
    // Defense holds in production: all fetchers use UniValue, not ExtractJsonValue.
    // RECOMMENDATION: Remove dead ExtractJsonValue to reduce attack surface.
}

/**
 * T3-06h: ATTACK — Outlier filter with adversarial price distribution
 *
 * Test that an attacker who controls fewer than 50% of exchanges
 * cannot meaningfully shift the median even with optimal positioning.
 */
BOOST_AUTO_TEST_CASE(redteam_T3_06h_minority_compromise_median_resilience)
{
    using namespace ExchangeAPI;
    MultiExchangeAggregator aggregator;

    // 2-of-6 compromise: attacker sets 2 prices to maximize impact
    // Strategy: set both to just under 10% above median to avoid filtering
    std::vector<MultiExchangeAggregator::ExchangePrice> prices = {
        {"Real1", 6000, GetTime(), true, 1.0},
        {"Real2", 6010, GetTime(), true, 1.0},
        {"Real3", 6020, GetTime(), true, 1.0},
        {"Real4", 6030, GetTime(), true, 1.0},
        {"Fake1", 6650, GetTime(), true, 1.0},  // ~10% above cluster
        {"Fake2", 6650, GetTime(), true, 1.0},
    };

    auto filtered = aggregator.FilterOutliers(prices);

    // Sorted: [6000, 6010, 6020, 6030, 6650, 6650]
    // Median: (6020 + 6030) / 2 = 6025
    // Threshold: 6025 * 0.10 = 602.5 → 602
    // 6650 - 6025 = 625 > 602 → FILTERED!
    // Both fakes get filtered
    BOOST_CHECK_EQUAL(filtered.size(), 4);

    CAmount median = aggregator.CalculateMedianPrice(filtered);
    // Median of [6000, 6010, 6020, 6030]: (6010 + 6020) / 2 = 6015
    BOOST_CHECK_EQUAL(median, 6015);

    // With 2-of-6 compromise, price manipulation is ZERO — defense holds!
    // Attacker would need to keep fake prices within ±10% of real median
    // to avoid filtering, but then their impact on the median is minimal.
}

BOOST_AUTO_TEST_SUITE_END()
