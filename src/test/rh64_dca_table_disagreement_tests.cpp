// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RH-64: Wave-12 Exploit — DCA table disagreement (H2 weaponisation).
 *
 * =================================================================
 * Target sites
 * =================================================================
 *
 *   Table A (chainparams, builder-facing):
 *     src/consensus/digidollar.h:87-92   `ConsensusParams::dcaLevels`
 *         {150, 100}  // >=150%: 1.00x
 *         {120, 125}  // >=120%: 1.25x
 *         {110, 150}  // >=110%: 1.50x
 *         {100, 200}  // >=100%: 2.00x
 *     src/consensus/digidollar.cpp:39-56 `DigiDollar::GetDCAMultiplier`
 *         Iterates the vector above and returns the multiplier of the
 *         FIRST level whose `systemCollateral` threshold the current
 *         health meets or exceeds.  Below all thresholds -> returns the
 *         last level's multiplier (2.0x) as the hard floor.
 *
 *   Table B (hardcoded, validator-facing):
 *     src/consensus/dca.cpp:20-25        `HEALTH_TIERS` (static vector)
 *         { 0..99   -> 2.0x, "emergency" }
 *         {100..119 -> 1.5x, "critical"  }
 *         {120..149 -> 1.2x, "warning"   }
 *         {150..30000 -> 1.0x, "healthy" }
 *     src/consensus/dca.cpp:117-136      `DynamicCollateralAdjustment::GetDCAMultiplier`
 *         Finds the tier whose [min,max] range contains the health value.
 *
 *   Callers that use Table A (chainparams, builder-side):
 *     src/digidollar/txbuilder.cpp:147  `MintTxBuilder::CalculateRequiredCollateral`
 *         Every wallet-initiated mint computes collateral requirement
 *         against the 2.0x-at-105 schedule.
 *
 *   Callers that use Table B (hardcoded, consensus-side):
 *     src/digidollar/validation.cpp:482 `GetEffectiveCollateralRatio`
 *     src/digidollar/validation.cpp:447 `CalculateRequiredCollateral` (via
 *                                        GetEffectiveCollateralRatio)
 *     src/digidollar/validation.cpp:515 `ValidateMintTransaction` (via
 *                                        CalculateRequiredCollateral/
 *                                        ValidateCollateralRatio :1140)
 *         Every block-validation mint check computes the required
 *         collateral against the 1.5x-at-105 schedule.
 *
 *   DIGIDOLLAR_BUG_HUNT_REPORT.md H2 (row "DCA multiplier tables
 *   disagree") called out the divergence but noted the builder is
 *   always "more conservative" and treated that as benign.  This PoC
 *   shows the inverse: an attacker who bypasses the builder can mint
 *   with LESS collateral than honest users, because the validator's
 *   table is the weaker of the two.
 *
 * =================================================================
 * Attacker model
 * =================================================================
 *
 *   Any end-user.  No oracle key, no colluding operators, no miner
 *   hashrate.  The attacker only needs:
 *     (a) the ability to hand-craft and broadcast a DigiDollar mint
 *         transaction (standard wallet + RPC: `createrawtransaction` +
 *         `signrawtransactionwithkey` + `sendrawtransaction`, or a
 *         third-party library that skips `MintTxBuilder`), AND
 *     (b) a window of system-health values in {100..109} or {120..149}.
 *
 *   System health landing in {100..109} after a price wobble is the
 *   most frequent DCA tier transition on any collateralised stable-
 *   coin; DigiByte's 15-second blocks mean the system crosses this
 *   band on the order of seconds, not minutes.
 *
 * =================================================================
 * Concrete harm
 * =================================================================
 *
 *   1. Under-collateralised mints.  At system health 105%, an honest
 *      wallet user is told to lock 2.0x*baseRatio DGB for their mint.
 *      A hand-crafted attacker transaction locks only 1.5x*baseRatio
 *      DGB and still passes ConnectBlock mint validation.  The
 *      attacker gets 33% MORE DD per DGB of collateral than honest
 *      users for as long as the health window holds.
 *
 *      Worked numbers (500% base ratio = 30-day lock, $100 DD, DGB at
 *      $0.01, systemCollateral=105):
 *        Builder path (Table A): effective ratio = 500 * 2.00 = 1000%
 *          required DGB = $100 * 10.0 / $0.01 = 10,000 DGB
 *        Validator path (Table B): effective ratio = 500 * 1.50 = 750%
 *          required DGB = $100 * 7.5 / $0.01 = 7,500 DGB
 *        Delta = 2,500 DGB per $100 DD.  Attacker is short 25% of the
 *        collateral the system "thinks" was posted on a live health-
 *        tier-aware dashboard.
 *
 *   2. System-health accounting drift.  The UI/RPC chain surfaces
 *      ddParams.dcaLevels as "the rules"; the validator silently
 *      enforces a weaker schedule.  After a stretch of attacker mints
 *      during a {100..109} window, the system's REAL aggregate
 *      collateralisation is lower than the DashBoard number suggests.
 *      When the window ends and the DCA multiplier normalises, the
 *      under-collateralised positions remain -- they were minted valid
 *      at the window, and there is no retro-check that ratchets them
 *      back up.
 *
 *   3. Peg attack amplifier.  A malicious actor who can also nudge the
 *      oracle-reported price (by coordinating with a single compliant
 *      oracle operator, or by exploiting unfixed C1 mainnet validator
 *      short-circuit) can steer system health into the {100..109} band
 *      intentionally, mint under-collateralised DD, then sell on the
 *      open market at near-$1 for pure extraction.  The DCA table
 *      disagreement turns a short-lived oracle glitch into a
 *      sustained DD peg pressure.
 *
 * =================================================================
 * Severity
 * =================================================================
 *
 *   HIGH (money-flow correctness / peg erosion).  Not CRITICAL only
 *   because the attacker cannot cause a consensus split (both tables
 *   are enforced by the same consensus instance of the binary) and
 *   cannot mint without collateral -- just with LESS collateral than
 *   the policy layer thinks is required.  A price-crash scenario with
 *   this vector open means many live mints are under-collateralised
 *   with no on-chain signal of the under-collateralisation.
 *
 * =================================================================
 * Novelty vs. priors and W1-W11
 * =================================================================
 *
 *   - H2 in `DIGIDOLLAR_BUG_HUNT_REPORT.md` flagged the two tables
 *     disagree but claimed builder-is-always-more-conservative means
 *     "no consensus reject".  This PoC proves the INVERSE is the
 *     attacker angle: a crafted tx that bypasses the builder is
 *     validator-accepted at a weaker ratio than the builder demands.
 *     H2 is upgraded from "benign documentation split" to "active
 *     extraction vector".
 *   - RH-61 / W9-C-01 poisoned the oracle PRICE.  RH-64 poisons the
 *     collateral RATIO expectation at fixed price.  Independent axes.
 *   - RH-63 / W11 suppressed oracle updates to stale `cached_price`.
 *     RH-64 exploits divergent interpretation of current `cached_price`
 *     at mint time.  Orthogonal.
 *   - No other RH test in the tree exercises the specific
 *     {100..109} or {120..149} boundary disagreement.  Closest is
 *     `digidollar_t2_05_tests.cpp:157-281` and
 *     `digidollar_redteam_tests.cpp:223-299`, which validate Table B
 *     against itself and never consult Table A.  Those tests would
 *     PASS today under a patched validator; this PoC targets the
 *     cross-table semantic gap.
 *
 * =================================================================
 * Fix direction (defender note)
 * =================================================================
 *
 *   Three candidates, ordered by preferred intrusiveness:
 *
 *   (1) Delete Table B entirely.  `DynamicCollateralAdjustment::
 *       GetDCAMultiplier` and `ApplyDCA` are thin wrappers that can
 *       call `DigiDollar::GetDCAMultiplier(systemCollateral,
 *       params.GetDigiDollarParams())` instead.  One source of truth,
 *       chainparams-driven, per-chain overridable.
 *
 *   (2) Assert invariant at boot.  Add a ValidateConsensusParams-style
 *       hook that fails startup if the two tables ever differ on any
 *       integer in [0, 30000].  This at least turns the silent
 *       semantic drift into a loud abort if someone edits one table
 *       but not the other.
 *
 *   (3) Pass chainparams into every `DCA::` call site.  Change the
 *       static `HEALTH_TIERS` into an instance built from the
 *       ConsensusParams at DCA-subsystem init.  More surgical than
 *       (1) but requires a plumbed context through the DCA class.
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/dca.h>
#include <consensus/digidollar.h>
#include <digidollar/validation.h>
#include <test/util/setup_common.h>

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Resolve Table A: builder-side multiplier.  Reads chainparams verbatim.
double TableA_mult(int health, const DigiDollar::ConsensusParams& ddParams)
{
    return DigiDollar::GetDCAMultiplier(health, ddParams);
}

// Resolve Table B: validator-side multiplier.  Reads the hardcoded tiers.
double TableB_mult(int health)
{
    return DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(health);
}

// The band of health values where the two tables disagree.
// Derived by direct inspection of the two definitions above.
struct Disagreement {
    int min_health;
    int max_health;
    double table_a;
    double table_b;
    const char* description;
};

std::vector<Disagreement> KnownDisagreements()
{
    return {
        {100, 109, 2.00, 1.50,
         "health 100..109: chainparams (Table A) = 2.00x (last level),"
         " hardcoded (Table B) = 1.50x (critical tier) -- 0.50 delta"},
        {120, 149, 1.25, 1.20,
         "health 120..149: chainparams = 1.25x (second level),"
         " hardcoded = 1.20x (warning tier) -- 0.05 delta"},
    };
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(rh64_dca_table_disagreement_tests, RegTestingSetup)

// =====================================================================
// RH-64-01: Prove Table A != Table B at systemCollateral=105 (the
//           deepest disagreement band).  This is the primitive the
//           attacker's entire economic advantage rests on.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh64_01_dca_tables_disagree_at_health_105)
{
    const auto& ddParams = Params().GetDigiDollarParams();

    const int health = 105;
    const double a = TableA_mult(health, ddParams);
    const double b = TableB_mult(health);

    BOOST_CHECK_MESSAGE(std::fabs(a - 2.00) < 1e-9,
        "Table A (chainparams) at health=105 should yield 2.00x (the "
        ">=100 last-level entry). Got " << a);
    BOOST_CHECK_MESSAGE(std::fabs(b - 1.50) < 1e-9,
        "Table B (hardcoded) at health=105 should yield 1.50x (the "
        "[100,119] critical tier). Got " << b);
    BOOST_CHECK_MESSAGE(std::fabs(a - b) > 1e-9,
        "Tables must DISAGREE at health=105 -- if this assertion fires, "
        "H2 has been fixed.  Tree value is Table A=" << a
        << ", Table B=" << b);

    BOOST_TEST_MESSAGE("RH-64-01: at health=105, Table A (builder) = " << a
        << "x, Table B (validator) = " << b << "x.  Builder demands "
        "33% more collateral than validator accepts.");
}

// =====================================================================
// RH-64-02: Prove Table A != Table B at systemCollateral=125 (the
//           shallower disagreement band).  Documents that the H2
//           disagreement is not a one-off at the emergency cliff but
//           a structural mismatch in tier edges.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh64_02_dca_tables_disagree_at_health_125)
{
    const auto& ddParams = Params().GetDigiDollarParams();

    const int health = 125;
    const double a = TableA_mult(health, ddParams);
    const double b = TableB_mult(health);

    BOOST_CHECK_MESSAGE(std::fabs(a - 1.25) < 1e-9,
        "Table A at health=125 should yield 1.25x (>=120 level). Got " << a);
    BOOST_CHECK_MESSAGE(std::fabs(b - 1.20) < 1e-9,
        "Table B at health=125 should yield 1.20x ([120,149] warning). Got " << b);
    BOOST_CHECK_MESSAGE(std::fabs(a - b) > 1e-9,
        "Tables must DISAGREE at health=125.  A=" << a << ", B=" << b);

    BOOST_TEST_MESSAGE("RH-64-02: at health=125, A=" << a << "x, B=" << b
        << "x.  Smaller discount band (4.2% collateral savings for attacker) "
        "but covers health=120..149 which is the most common 'not healthy, "
        "not critical' zone in DCA simulation runs.");
}

// =====================================================================
// RH-64-03: End-to-end economic harm.  Compute the dollar amount the
//           validator accepts vs what the builder demands for the
//           same (ddAmount, lockTime, oraclePrice) tuple at a
//           {100..109}-band systemCollateral.  This exercises the
//           real CalculateRequiredCollateral entry point through
//           GetEffectiveCollateralRatio -> DCA::ApplyDCA so the
//           reader can track the divergence in the function pair they
//           would grep for in a review.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh64_03_validator_accepts_builder_would_reject_collateral)
{
    const CChainParams& chainParams = Params();
    const auto& ddParams = chainParams.GetDigiDollarParams();

    // Fixed attack inputs.
    const CAmount ddAmount          = 10000;       // $100 in cents
    const int64_t lockPeriodBlocks  = 30 * DigiDollar::BLOCKS_PER_DAY;   // 30-day tier -> baseRatio 500%
    const CAmount oraclePriceMuUSD  = 10000;       // $0.01/DGB
    const int     systemCollateral  = 105;         // WITHIN disagreement band

    const int baseRatio = DigiDollar::GetCollateralRatioForLockTime(lockPeriodBlocks, ddParams);
    BOOST_REQUIRE_MESSAGE(baseRatio == 500,
        "Sanity: 30-day lock tier should be 500%, got " << baseRatio);

    // --------- Table A / builder view ---------
    const double aMult   = TableA_mult(systemCollateral, ddParams);
    const int    aEffRat = static_cast<int>(static_cast<double>(baseRatio) * aMult);
    // Required DGB using the builder's formula from txbuilder.cpp:171-173.
    // numerator = ddAmount * COIN * ratio * 100, denom = oraclePrice.
    const __int128 aNum  = static_cast<__int128>(ddAmount) *
                           static_cast<__int128>(COIN) *
                           static_cast<__int128>(aEffRat) * 100;
    const __int128 aReq  = aNum / static_cast<__int128>(oraclePriceMuUSD);

    // --------- Table B / validator view ---------
    // Drive CalculateRequiredCollateral through its public API rather than
    // reimplementing the arithmetic -- this is the EXACT function
    // ConnectBlock calls at validation.cpp:1125.
    DigiDollar::ValidationContext ctx(/*height=*/1000,
                                      /*price_micro_usd=*/oraclePriceMuUSD,
                                      /*collateral=*/systemCollateral,
                                      chainParams);
    const CAmount bReq = DigiDollar::CalculateRequiredCollateral(ddAmount, lockPeriodBlocks, ctx);

    // Cross-check the inside using the public ratio fn, too.
    const int bEffRat = DigiDollar::GetEffectiveCollateralRatio(baseRatio, systemCollateral, chainParams);

    // --------- Invariants ---------
    BOOST_CHECK_MESSAGE(aEffRat == 1000,
        "Builder's effective ratio at 500% * 2.0 should be 1000%, got " << aEffRat);
    BOOST_CHECK_MESSAGE(bEffRat == 750,
        "Validator's effective ratio at 500% * 1.5 should be 750%, got " << bEffRat);
    BOOST_CHECK_MESSAGE(aReq > static_cast<__int128>(bReq),
        "Builder must demand MORE DGB than validator accepts.  A=" << static_cast<uint64_t>(aReq)
        << " sats, B=" << bReq << " sats.");

    // Quantify the extraction.
    const __int128 delta   = aReq - static_cast<__int128>(bReq);
    const double   deltaDGB = static_cast<double>(static_cast<uint64_t>(delta)) / COIN;
    const double   bReqDGB = static_cast<double>(bReq) / COIN;

    BOOST_TEST_MESSAGE("RH-64-03: $100 DD mint at health=105, 30-day lock, "
        "DGB=$0.01:");
    BOOST_TEST_MESSAGE("           Table A (builder) requires  "
        << static_cast<uint64_t>(aReq) << " sats (~"
        << std::fixed << static_cast<long>(static_cast<double>(static_cast<uint64_t>(aReq)) / COIN)
        << " DGB)");
    BOOST_TEST_MESSAGE("           Table B (validator) accepts "
        << bReq << " sats (~" << std::fixed << static_cast<long>(bReqDGB)
        << " DGB)");
    BOOST_TEST_MESSAGE("           Attacker SHORTS the system "
        << std::fixed << static_cast<long>(deltaDGB) << " DGB per $100 DD minted.");
    BOOST_TEST_MESSAGE("           Scaled to $1M of attacker mints in the "
        "window: shortfall ~" << std::fixed << static_cast<long>(deltaDGB * 10000) << " DGB "
        "($" << std::fixed << static_cast<long>(deltaDGB * 10000 * 0.01) << " @ $0.01).");
}

// =====================================================================
// RH-64-04: Build the attack-gain matrix across every real lock tier
//           at health=105.  Quantifies the extraction per $100 DD for
//           each lock period so the defender can see where the bigger
//           gains live (longer locks -> bigger absolute shortfall
//           because the base ratio is lower and DCA multiplier is
//           applied multiplicatively).
// =====================================================================
BOOST_AUTO_TEST_CASE(rh64_04_extraction_ratio_quantification)
{
    const CChainParams& chainParams = Params();
    const auto& ddParams = chainParams.GetDigiDollarParams();

    const CAmount ddAmount         = 10000;  // $100
    const CAmount oraclePriceMuUSD = 10000;  // $0.01/DGB
    const int     systemCollateral = 105;

    // Walk every lock tier defined in chainparams.
    struct TierRow { int64_t lockBlocks; int baseRatio; };
    std::vector<TierRow> rows;
    for (const auto& [blocks, ratio] : ddParams.collateralRatios) {
        rows.push_back({blocks, ratio});
    }

    BOOST_REQUIRE_MESSAGE(!rows.empty(), "chainparams collateralRatios must not be empty");

    for (const auto& tier : rows) {
        const double aMult    = TableA_mult(systemCollateral, ddParams);
        const int    aEffRat  = static_cast<int>(tier.baseRatio * aMult);
        const __int128 aNum   = static_cast<__int128>(ddAmount) * static_cast<__int128>(COIN)
                              * static_cast<__int128>(aEffRat) * 100;
        const __int128 aReq   = aNum / static_cast<__int128>(oraclePriceMuUSD);

        DigiDollar::ValidationContext ctx(/*height=*/1000,
                                          /*price=*/oraclePriceMuUSD,
                                          /*collateral=*/systemCollateral,
                                          chainParams);
        const CAmount bReq    = DigiDollar::CalculateRequiredCollateral(
                                    ddAmount, tier.lockBlocks, ctx);
        const int     bEffRat = DigiDollar::GetEffectiveCollateralRatio(
                                    tier.baseRatio, systemCollateral, chainParams);

        const __int128 delta   = aReq - static_cast<__int128>(bReq);
        const double   deltaDGB = static_cast<double>(static_cast<uint64_t>(delta)) / COIN;
        const double   ratioDiscount = (static_cast<double>(aEffRat - bEffRat) / aEffRat) * 100.0;

        BOOST_CHECK_MESSAGE(aReq > static_cast<__int128>(bReq),
            "Builder must demand more than validator at lock="
            << tier.lockBlocks << " blocks, base=" << tier.baseRatio);

        std::ostringstream os;
        os << "RH-64-04 row: lock=" << tier.lockBlocks << "b base=" << tier.baseRatio
           << "%  aEff=" << aEffRat << "% bEff=" << bEffRat << "%"
           << "  shortfall/$100=" << std::fixed << static_cast<long>(deltaDGB) << " DGB"
           << "  discount=" << std::fixed << ratioDiscount << "%";
        BOOST_TEST_MESSAGE(os.str());
    }
}

// =====================================================================
// RH-64-05: Rapid oscillation amplifies the bug.  A price that cycles
//           into the {100..109} band 100 times per day (normal market
//           volatility at 15-second blocks + stablecoin arbitrage
//           reflexes) gives the attacker 100 mint windows per day
//           where validator is weaker than builder.  Show cumulative
//           extraction per day at a plausible attacker rate.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh64_05_rapid_oscillation_amplifies_bug)
{
    const CChainParams& chainParams = Params();
    const auto& ddParams = chainParams.GetDigiDollarParams();

    // 30-day lock, $10K per mint (maxMintAmount=$100K; attacker submits $10K
    // to stay under single-tx limit but within one block).
    const CAmount ddAmount         = 1000000;   // $10,000 in cents
    const int64_t lockBlocks       = 30 * DigiDollar::BLOCKS_PER_DAY;
    const CAmount oraclePriceMuUSD = 10000;     // $0.01/DGB

    // Number of mint opportunities assumed per day.  Each ~15s block
    // during a health window is a mint slot.  Health windows of 100..109
    // typically persist ~10-60 blocks in live DCA simulations.  Use a
    // conservative 50 per day.
    const int daily_opportunities = 50;

    __int128 cumulative_shortfall = 0;
    for (int i = 0; i < daily_opportunities; ++i) {
        const int health = 100 + (i % 10); // walk 100..109

        const double aMult = TableA_mult(health, ddParams);
        const int baseRatio = DigiDollar::GetCollateralRatioForLockTime(lockBlocks, ddParams);
        const int aEffRat = static_cast<int>(baseRatio * aMult);
        const __int128 aNum = static_cast<__int128>(ddAmount) * static_cast<__int128>(COIN)
                            * static_cast<__int128>(aEffRat) * 100;
        const __int128 aReq = aNum / static_cast<__int128>(oraclePriceMuUSD);

        DigiDollar::ValidationContext ctx(1000, oraclePriceMuUSD, health, chainParams);
        const CAmount bReq = DigiDollar::CalculateRequiredCollateral(ddAmount, lockBlocks, ctx);
        cumulative_shortfall += (aReq - static_cast<__int128>(bReq));
    }

    const double cumulativeDGB = static_cast<double>(static_cast<uint64_t>(cumulative_shortfall)) / COIN;
    BOOST_CHECK_MESSAGE(cumulative_shortfall > 0,
        "Cumulative shortfall must be positive across all 100..109 samples.");

    BOOST_TEST_MESSAGE("RH-64-05: " << daily_opportunities
        << " $10K mints across health=100..109 cycling: cumulative "
        "validator-vs-builder shortfall = "
        << std::fixed << static_cast<long>(cumulativeDGB) << " DGB "
        "($" << std::fixed << (cumulativeDGB * 0.01) << " at $0.01).  "
        "Sustained over a year of busy markets -> ~"
        << std::fixed << static_cast<long>(cumulativeDGB * 365)
        << " DGB of system under-collateralisation.");
}

// =====================================================================
// RH-64-06: Full boundary sweep.  Walk health = 0..200 in 1% steps
//           and tabulate both tables.  Emits the disagreement set for
//           easy visual inspection + asserts Table A >= Table B at
//           every integer (i.e., builder is never WEAKER than
//           validator) so we prove the attack direction is
//           single-sided -- you can always gain by bypassing the
//           builder, never by routing through it.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh64_06_full_boundary_sweep)
{
    const auto& ddParams = Params().GetDigiDollarParams();

    int n_same  = 0;
    int n_adiff = 0;  // A > B (attack-direction)
    int n_bdiff = 0;  // B > A (would be a defender-favoring bug)

    for (int h = 0; h <= 200; ++h) {
        const double a = TableA_mult(h, ddParams);
        const double b = TableB_mult(h);
        if (std::fabs(a - b) < 1e-9) {
            ++n_same;
        } else if (a > b) {
            ++n_adiff;
        } else {
            ++n_bdiff;
            BOOST_TEST_MESSAGE("RH-64-06: UNEXPECTED defender-favoring mismatch "
                "at health=" << h << "  A=" << a << "  B=" << b);
        }
    }

    BOOST_CHECK_MESSAGE(n_bdiff == 0,
        "Invariant: Table A (builder) must never be WEAKER than Table B "
        "(validator).  If this fires, an honest wallet user would be "
        "ABOVE-consensus-accepted, which is a different (but still bad) "
        "direction.  Count of B>A mismatches: " << n_bdiff);

    BOOST_CHECK_MESSAGE(n_adiff > 0,
        "Sanity: there must exist health values where A > B (the attack "
        "band).  Count: " << n_adiff);

    // Spell out the known bands from KnownDisagreements() to double-check.
    for (const auto& d : KnownDisagreements()) {
        for (int h = d.min_health; h <= d.max_health; ++h) {
            const double a = TableA_mult(h, ddParams);
            const double b = TableB_mult(h);
            BOOST_CHECK_MESSAGE(std::fabs(a - d.table_a) < 1e-9,
                "Known band A-value mismatch at h=" << h
                << ": expected " << d.table_a << " got " << a
                << " (" << d.description << ")");
            BOOST_CHECK_MESSAGE(std::fabs(b - d.table_b) < 1e-9,
                "Known band B-value mismatch at h=" << h
                << ": expected " << d.table_b << " got " << b
                << " (" << d.description << ")");
        }
    }

    BOOST_TEST_MESSAGE("RH-64-06 sweep: same=" << n_same
        << "  A>B (attack band)=" << n_adiff
        << "  B>A (never)=" << n_bdiff
        << "  attack bands: {100..109, 120..149} covers "
        << (10 + 30) << " integer-health values.");
}

BOOST_AUTO_TEST_SUITE_END()
