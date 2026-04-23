// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RH-54: OP_ORACLE incorrectly classified as OP_SUCCESS in Tapscript
 *        (Wave-2 adversarial PoC)
 *
 * Target: src/script/script.cpp::IsOpSuccess
 *
 * Pre-patch behavior:
 *   IsOpSuccess() excluded opcodes 0xbb..0xbe (OP_DIGIDOLLAR..OP_CHECKCOLLATERAL)
 *   from the generic Tapscript OP_SUCCESS range [187..254], but NOT OP_ORACLE
 *   (0xbf = 191). Because OP_ORACLE falls inside 187..254 without exclusion,
 *   `IsOpSuccess(OP_ORACLE) == true` — which means any Tapscript leaf containing
 *   OP_ORACLE becomes **unconditionally spendable** per BIP-342 OP_SUCCESSx
 *   semantics (ExecuteWitnessScript short-circuits the script as a success).
 *
 * Why this is novel (not in bug-hunt report priors):
 *   - C1-C4, H1-H8, M1-M5 and the W1 ledger entries do not cover IsOpSuccess.
 *   - Existing `digidollar_opcodes_tests.cpp` and `digidollar_script_attacks_tests.cpp`
 *     assert opcode semantics via EvalScript but never exercise the OP_SUCCESS
 *     branch of Tapscript execution.
 *
 * Attack model (fund-theft trap):
 *   - DD wallets and third-party DD-aware tooling that treat OP_ORACLE as a
 *     marker opcode (docs describe it as "Oracle price data marker") may
 *     embed it in a Tapscript leaf as a compact tag — e.g., for on-chain
 *     indexing of oracle-related transactions.
 *   - Any P2TR output whose script-path MAST contains OP_ORACLE is spendable
 *     by anyone because the Tapscript execution path short-circuits as success.
 *   - The attacker scans the chain for P2TR outputs whose revealed leaf
 *     includes OP_ORACLE, then takes them with an empty witness / a
 *     well-formed control block.
 *
 * Fix (applied in this commit):
 *   Extend the IsOpSuccess exclusion to cover OP_ORACLE:
 *     if (opcode >= OP_DIGIDOLLAR && opcode <= OP_ORACLE) return false;
 *
 * This test asserts the POST-FIX invariant. Pre-fix it fails; post-fix it
 * passes. The unit-test binary was re-built twice during audit (pre-patch
 * and post-patch) to verify both directions.
 */

#include <boost/test/unit_test.hpp>

#include <script/script.h>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(rh54_op_oracle_opsuccess_tests, BasicTestingSetup)

// All five DD opcodes (0xbb..0xbf) must be excluded from OP_SUCCESSx.
BOOST_AUTO_TEST_CASE(rh54_all_dd_opcodes_excluded_from_opsuccess)
{
    BOOST_CHECK_MESSAGE(!IsOpSuccess(OP_DIGIDOLLAR),
        "OP_DIGIDOLLAR must be executable, not OP_SUCCESS");
    BOOST_CHECK_MESSAGE(!IsOpSuccess(OP_DDVERIFY),
        "OP_DDVERIFY must be executable, not OP_SUCCESS");
    BOOST_CHECK_MESSAGE(!IsOpSuccess(OP_CHECKPRICE),
        "OP_CHECKPRICE must be executable, not OP_SUCCESS");
    BOOST_CHECK_MESSAGE(!IsOpSuccess(OP_CHECKCOLLATERAL),
        "OP_CHECKCOLLATERAL must be executable, not OP_SUCCESS");
    BOOST_CHECK_MESSAGE(!IsOpSuccess(OP_ORACLE),
        "OP_ORACLE must NOT be OP_SUCCESS — a Tapscript leaf containing "
        "OP_ORACLE would otherwise be unconditionally spendable by any "
        "user presenting a valid control block. Fund-theft trap for any "
        "wallet that treats OP_ORACLE as a marker opcode.");
}

// Spot-check the opcode numerics that drive the fix so a future refactor
// doesn't silently reopen the hole.
BOOST_AUTO_TEST_CASE(rh54_opcode_numeric_invariants)
{
    BOOST_CHECK_EQUAL(static_cast<int>(OP_DIGIDOLLAR),      0xbb);
    BOOST_CHECK_EQUAL(static_cast<int>(OP_DDVERIFY),        0xbc);
    BOOST_CHECK_EQUAL(static_cast<int>(OP_CHECKPRICE),      0xbd);
    BOOST_CHECK_EQUAL(static_cast<int>(OP_CHECKCOLLATERAL), 0xbe);
    BOOST_CHECK_EQUAL(static_cast<int>(OP_ORACLE),          0xbf);

    // Sanity: 0xbf == 191, and the generic OP_SUCCESS range starts at 187.
    BOOST_CHECK_GE(static_cast<int>(OP_ORACLE), 187);
    BOOST_CHECK_LE(static_cast<int>(OP_ORACLE), 254);
}

// Neighbors outside the DD range remain OP_SUCCESS to preserve the existing
// BIP-342 semantics.
BOOST_AUTO_TEST_CASE(rh54_neighbors_still_opsuccess)
{
    // 0xba == OP_CHECKSIGADD  (NOT OP_SUCCESS — it's a real opcode)
    BOOST_CHECK(!IsOpSuccess(static_cast<opcodetype>(0xba)));
    // 0xc0 == first byte after OP_ORACLE; must remain OP_SUCCESS (has no
    // assigned opcode today).
    BOOST_CHECK(IsOpSuccess(static_cast<opcodetype>(0xc0)));
    // 0xfe remains OP_SUCCESS (end of range).
    BOOST_CHECK(IsOpSuccess(static_cast<opcodetype>(0xfe)));
}

BOOST_AUTO_TEST_SUITE_END()
