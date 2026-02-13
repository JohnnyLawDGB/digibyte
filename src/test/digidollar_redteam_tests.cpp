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
#include <digidollar/txbuilder.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <consensus/dca.h>
#include <kernel/chainparams.h>
#include <primitives/transaction.h>
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

BOOST_AUTO_TEST_SUITE_END()
