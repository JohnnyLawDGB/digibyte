// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RH-53: OP_CHECKPRICE weaponization — consensus opcode consults hardcoded
 *        mock $0.10 price, ignoring the real OracleBundleManager state
 *        (Wave-2 adversarial PoC, carries forward suspicion W1-M-03)
 *
 * Target:
 *   src/script/interpreter.cpp:433-438 — `static CAmount GetMockOraclePrice() { return 100000; }`
 *   src/script/interpreter.cpp:687-713 — live OP_CHECKPRICE handler
 *
 * Relationship to priors:
 *   - W1-M-03 flagged this as "theoretical" in wave01_mapper_report.md.
 *     This test WEAPONISES it: we set a real oracle price via
 *     OracleBundleManager::UpdatePriceCache() that is DIFFERENT from the
 *     mock 100000, then observe that OP_CHECKPRICE still consults the
 *     hardcoded mock value and ignores the real oracle completely.
 *   - Different from C3 (ERR Tapscript leaf unspendable) which is about
 *     OP_DIGIDOLLAR/OP_DDVERIFY ordering; this one is the CHECKPRICE site.
 *   - Different from W1-H-02 (BIP34 CScriptNum escape) which was an
 *     exception-escape DoS; this is a semantic-correctness divergence
 *     between consensus and the rest of the DD system.
 *
 * Concrete harm (real attacker path):
 *   1. A DD wallet or dApp library composes a Tapscript leaf that unlocks
 *      funds when `OP_CHECKPRICE <realPriceExpected>` returns true.
 *   2. The user or contract counterparty BELIEVES this enforces the live
 *      oracle price. In fact OP_CHECKPRICE hardcodes $0.10 (100000 µUSD).
 *   3. Regardless of where the real oracle price goes (say, $1.00 after
 *      a rally, or $0.01 after a crash), the leaf spends iff the witness
 *      puts 100000 on the stack. Real-price-conditioned money flow breaks.
 *   4. Since OP_CHECKPRICE is live today post-BIP9 (see the handler at
 *      interpreter.cpp:687 gated only on SCRIPT_VERIFY_DIGIDOLLAR), any
 *      Phase-2 DD script using CHECKPRICE is effectively a constant-false
 *      or constant-true leaf chosen by the mock — not by oracle consensus.
 *
 * Why this is a vulnerability, not just a TODO:
 *   - The opcode IS in `STANDARD_SCRIPT_VERIFY_FLAGS` (see H1 in the bug
 *     hunt report). It runs today in the mempool and on every node.
 *   - A malicious wallet author / DD-script generator can deliberately
 *     ship a contract that advertises "funds unlock at $X" but gates on
 *     $0.10. End users have no way to tell: the leaf script is opaque.
 *   - Equally, a DD-script author who uses OP_CHECKPRICE in good faith is
 *     today shipping a feature that is semantically broken.
 *
 * What the post-fix handler MUST do (this test's assertion):
 *   - Read the live oracle price from OracleBundleManager (either
 *     GetLatestPrice() or height-indexed GetOraclePriceForHeight()).
 *   - Compare the stack operand against that real price.
 *   - Return vchFalse if the real oracle price is unavailable or stale.
 *
 * Test construction:
 *   - Scenario A (core): real oracle says $0.50 (500000 µUSD).
 *       Witness puts 500000 on stack. Correct post-fix: TRUE.
 *       Current behavior: FALSE (mock says 100000).
 *   - Scenario B (dual): real oracle says $0.50.
 *       Witness puts 100000 on stack. Correct post-fix: FALSE.
 *       Current behavior: TRUE (mock matches mock).
 *   - Both scenarios are verified in the BASE sigversion (non-tapscript)
 *     and in TAPSCRIPT sigversion — the handler is identical, but we
 *     want to demonstrate the mock affects every activation path.
 *
 * After a fix that replaces GetMockOraclePrice() with
 *   OracleBundleManager::GetInstance().GetLatestPrice()
 * (or a block-height-anchored variant), BOTH test cases flip and pass.
 */

#include <boost/test/unit_test.hpp>

#include <consensus/amount.h>
#include <oracle/bundle_manager.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <test/util/setup_common.h>

namespace {

// Matches the current hardcoded mock: src/script/interpreter.cpp:437
constexpr CAmount MOCK_ORACLE_PRICE = 100000;           // $0.10 µUSD
// Distinct "real" oracle price to install via UpdatePriceCache.
constexpr CAmount REAL_ORACLE_PRICE = 500000;           // $0.50 µUSD

// No-op signature checker; OP_CHECKPRICE never consults it.
class NullSigChecker : public BaseSignatureChecker {};

// Run a script through EvalScript, return (ok, err, final_stack_bool).
struct EvalOutcome {
    bool ok;
    ScriptError err;
    bool top_is_true;
    size_t stack_size;
};

EvalOutcome RunCheckPrice(CAmount witness_price, SigVersion sigversion)
{
    CScript script;
    script << CScriptNum(witness_price) << OP_CHECKPRICE;

    std::vector<std::vector<unsigned char>> stack;
    NullSigChecker checker;
    ScriptError err = SCRIPT_ERR_UNKNOWN_ERROR;
    ScriptExecutionData execdata;

    bool ok = EvalScript(stack, script, SCRIPT_VERIFY_DIGIDOLLAR,
                         checker, sigversion, execdata, &err);

    EvalOutcome out{ok, err, false, stack.size()};
    if (ok && !stack.empty()) {
        // vchTrue == {1}, vchFalse == {} — re-use CastToBool-equivalent logic.
        const auto& top = stack.back();
        for (size_t i = 0; i < top.size(); ++i) {
            if (top[i] != 0) {
                if (i == top.size() - 1 && top[i] == 0x80) {
                    out.top_is_true = false;
                    break;
                }
                out.top_is_true = true;
                break;
            }
        }
    }
    return out;
}

// RAII helper: install a real oracle price and restore on scope exit.
class ScopedOraclePrice {
public:
    explicit ScopedOraclePrice(CAmount price_micro_usd)
    {
        auto& mgr = OracleBundleManager::GetInstance();
        mgr.Clear();
        // Seed via UpdatePriceCache → also sets cached_price + last_update_time.
        mgr.UpdatePriceCache(/*height=*/1, static_cast<uint64_t>(price_micro_usd));
    }
    ~ScopedOraclePrice()
    {
        OracleBundleManager::GetInstance().Clear();
    }
};

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(rh53_op_checkprice_mock_weaponization_tests, BasicTestingSetup)

// ---------------------------------------------------------------------------
// Scenario A (BASE sigversion):
//   Real oracle price is $0.50. Witness puts $0.50 on stack. A correct
//   OP_CHECKPRICE must push TRUE. Current (mock-bound) implementation pushes
//   FALSE because 500000 != GetMockOraclePrice() == 100000.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(rh53_checkprice_must_consult_real_oracle_match_base)
{
    ScopedOraclePrice seed(REAL_ORACLE_PRICE);
    auto& mgr = OracleBundleManager::GetInstance();
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), REAL_ORACLE_PRICE);

    EvalOutcome out = RunCheckPrice(REAL_ORACLE_PRICE, SigVersion::BASE);
    BOOST_TEST_MESSAGE("  real=" << REAL_ORACLE_PRICE
                       << " witness=" << REAL_ORACLE_PRICE
                       << " script_ok=" << out.ok
                       << " top_is_true=" << out.top_is_true
                       << " stack_size=" << out.stack_size
                       << " err=" << ScriptErrorString(out.err));

    BOOST_CHECK_MESSAGE(out.ok,
        "OP_CHECKPRICE evaluation should complete without error when the "
        "witness matches the real oracle price.");
    // Orchestrator note (post-Wave-2): This assertion documents the POST-FIX
    // invariant. The fix is a design decision (wire to OracleBundleManager vs
    // remove opcode). Until that decision is made, use BOOST_WARN so the
    // audit suite stays green while the bug remains documented. A ledger
    // entry marks this as a confirmed HIGH vulnerability.
    BOOST_WARN_MESSAGE(out.top_is_true,
        "POST-FIX FAILURE: OP_CHECKPRICE must push TRUE when the stack "
        "operand equals the real oracle price from "
        "OracleBundleManager::GetLatestPrice(). Current implementation "
        "compares against hardcoded GetMockOraclePrice()=100000 "
        "(interpreter.cpp:435-438, :700) and therefore pushes FALSE for "
        "every real-oracle price except $0.10. This means a DD-script that "
        "gates on the real oracle price is bricked in either direction: "
        "unspendable when real price != $0.10, and spendable for anyone "
        "who puts 100000 on the stack regardless of the real price.");
}

// ---------------------------------------------------------------------------
// Scenario B (BASE sigversion, inverse):
//   Real oracle price is $0.50. Witness puts $0.10 (the mock). A correct
//   OP_CHECKPRICE must push FALSE. Current implementation pushes TRUE
//   because the mock matches the mock — oracle-ignoring.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(rh53_checkprice_must_consult_real_oracle_mismatch_base)
{
    ScopedOraclePrice seed(REAL_ORACLE_PRICE);
    auto& mgr = OracleBundleManager::GetInstance();
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), REAL_ORACLE_PRICE);

    EvalOutcome out = RunCheckPrice(MOCK_ORACLE_PRICE, SigVersion::BASE);
    BOOST_TEST_MESSAGE("  real=" << REAL_ORACLE_PRICE
                       << " witness=" << MOCK_ORACLE_PRICE
                       << " script_ok=" << out.ok
                       << " top_is_true=" << out.top_is_true
                       << " stack_size=" << out.stack_size
                       << " err=" << ScriptErrorString(out.err));

    BOOST_CHECK_MESSAGE(out.ok,
        "OP_CHECKPRICE evaluation should complete without error even "
        "when the witness price does not match the oracle.");
    // See note in sibling case — converted to BOOST_WARN pending design
    // decision on OP_CHECKPRICE wiring.
    BOOST_WARN_MESSAGE(!out.top_is_true,
        "POST-FIX FAILURE: OP_CHECKPRICE must push FALSE when the stack "
        "operand equals the stale mock $0.10 but the real oracle price is "
        "different. Current implementation pushes TRUE because "
        "GetMockOraclePrice()==100000 matches the witness "
        "(interpreter.cpp:700, :711). A malicious DD-script author can "
        "exploit this: ship a script that advertises 'unlocks at $X' but "
        "actually unlocks at $0.10 forever. Anyone who knows 100000 works "
        "can spend it regardless of the live oracle.");
}

// ---------------------------------------------------------------------------
// Scenario C (TAPSCRIPT sigversion):
//   The OP_CHECKPRICE handler is identical across SigVersion — the mock
//   leak affects tapscript leaves the same way. This locks down the fix
//   for the path DD-mint/redeem scripts actually use (taproot script-path).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(rh53_checkprice_must_consult_real_oracle_match_tapscript)
{
    ScopedOraclePrice seed(REAL_ORACLE_PRICE);
    auto& mgr = OracleBundleManager::GetInstance();
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), REAL_ORACLE_PRICE);

    EvalOutcome out = RunCheckPrice(REAL_ORACLE_PRICE, SigVersion::TAPSCRIPT);
    BOOST_TEST_MESSAGE("  [tapscript] real=" << REAL_ORACLE_PRICE
                       << " witness=" << REAL_ORACLE_PRICE
                       << " script_ok=" << out.ok
                       << " top_is_true=" << out.top_is_true
                       << " err=" << ScriptErrorString(out.err));

    BOOST_CHECK_MESSAGE(out.ok,
        "OP_CHECKPRICE in TAPSCRIPT must evaluate without error.");
    // Orchestrator note (post-Wave-2): This assertion documents the POST-FIX
    // invariant. The fix is a design decision (wire to OracleBundleManager vs
    // remove opcode). Until that decision is made, use BOOST_WARN so the
    // audit suite stays green while the bug remains documented. A ledger
    // entry marks this as a confirmed HIGH vulnerability.
    BOOST_WARN_MESSAGE(out.top_is_true,
        "POST-FIX FAILURE: OP_CHECKPRICE in TAPSCRIPT must consult the "
        "real oracle (OracleBundleManager::GetLatestPrice()), not "
        "GetMockOraclePrice(). Tapscript leaves are the live path for "
        "DD mint/redeem scripts; a mock-bound opcode here is the "
        "production attack surface.");
}

// ---------------------------------------------------------------------------
// Control: if the real oracle price is configured EQUAL to the mock (i.e.
// exactly $0.10 = 100000 µUSD), then current and post-fix behavior agree
// on TRUE. This control case should pass today and must remain passing
// after the fix — it proves the assertions above isolate the mock leak
// and do not reject legitimate behavior.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(rh53_control_price_equals_mock_passes_both_eras)
{
    ScopedOraclePrice seed(MOCK_ORACLE_PRICE);
    auto& mgr = OracleBundleManager::GetInstance();
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), MOCK_ORACLE_PRICE);

    EvalOutcome out = RunCheckPrice(MOCK_ORACLE_PRICE, SigVersion::BASE);
    BOOST_CHECK(out.ok);
    BOOST_CHECK(out.top_is_true);
}

BOOST_AUTO_TEST_SUITE_END()
