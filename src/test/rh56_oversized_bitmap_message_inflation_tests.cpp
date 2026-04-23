// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RH-56: Oversized participation-bitmap inflates bundle.messages in
 *        OracleBundleManager::ExtractOracleBundle before consensus validation
 *        (Wave-4 adversarial PoC — participation-bitmap attack)
 *
 * Target: src/oracle/bundle_manager.cpp:1127-1131 (ExtractOracleBundle v0x03 path)
 *         src/oracle/musig2_aggregator.cpp:63-80  (DecodeBitmap)
 *         src/oracle/bundle_manager.cpp:2735-2742 (ValidatePhaseThreeBundle)
 *
 * Angle: (D) Out-of-range IDs + (B) ExtractOracleBundle cache asymmetry.
 *        Carries W1-L-08 from the Wave-1 mapper report into a concrete PoC.
 *
 * Justification paragraph:
 *   The v0x03 on-chain serialization is `bitmap_len(1) + bitmap(variable) + …`
 *   with `bitmap_len` a single byte, so any bitmap length 1..255 passes
 *   `COracleBundle::DeserializeV03Data` as long as the outer payload size
 *   matches. `ExtractOracleBundle` then asks `DecodeBitmap` to iterate the
 *   bitmap using a LOCALLY RECOMPUTED total:
 *       total = max(bitmap.size()*8, nOracleTotalOracles)
 *   rather than the consensus-fixed `params.nOracleTotalOracles` (17 on mainnet,
 *   7 on regtest). For a 32-byte attacker-supplied bitmap this computes
 *   total=256, DecodeBitmap iterates bits 0..255, and every set bit becomes
 *   a synthetic `COraclePriceMessage` pushed into `bundle.messages` with the
 *   attacker-chosen `oracle_id`. IDs ≥ nOracleTotalOracles have no chainparams
 *   entry, so `GetOracleNode(id)` returns null and the synthetic message
 *   carries an UNINITIALIZED `oracle_pubkey`. The consensus validator later
 *   rejects the bundle at `ValidatePhaseThreeBundle` because that path
 *   decodes the bitmap with the exact consensus total (17 → expected 3 bytes,
 *   the 32-byte bitmap mismatches → empty `oracle_ids` → "bitmap decoding
 *   failed"), so this is not a direct consensus break. However the
 *   side-effectful `bundle.messages` is populated BEFORE the validator runs
 *   and any intermediate caller that trusts `bundle.messages` — today mining
 *   log paths (`bundle_manager.cpp:2096-2104, 2177`), and any future
 *   telemetry/RPC consumer threading the raw-extracted bundle — will see up
 *   to 256 bogus oracle messages per crafted block. The asymmetry also gives
 *   a relayer-sized amplification: 1-byte bitmap_len change → 256x memory
 *   inflation in the extract path.
 *
 * Why this is novel (not C1-C4, H1-H8, M1-M5, W1-W3):
 *   - rh05 attack_v03_bitmap_max_participants only checks Serialize/Deserialize
 *     round-trip, never calls ExtractOracleBundle from a coinbase.
 *   - rh29 attack3_v03_bitmap_len_{zero,mismatch} exercise DeserializeV03Data
 *     directly; neither covers the ExtractOracleBundle→DecodeBitmap asymmetry
 *     where total_oracles expands from nOracleTotalOracles to bitmap.size()*8.
 *   - W1-L-08 named the asymmetry; W4 weaponizes it into a concrete 256-message
 *     inflation, including out-of-range oracle IDs (17..255) carrying null
 *     pubkeys.
 *   - W3 (rh55) targeted MuSig2 partial-sig aggregation, not bitmap parsing.
 *
 * Attack model (local DoS + caller-trust trap):
 *   1. Attacker prepares a v0x03 coinbase OP_RETURN where participation_bitmap
 *      is 32 bytes of 0xFF (256 participants claimed).
 *   2. Block candidate is passed to any node that calls ExtractOracleBundle
 *      (validation.cpp:142 and :2811 today).
 *   3. Extract succeeds. bundle.messages contains 256 synthetic entries with
 *      oracle_ids 0..255. IDs >= nOracleTotalOracles (17 mainnet / 7 regtest)
 *      have default-constructed `oracle_pubkey`.
 *   4. ValidatePhaseThreeBundle later rejects the block (bitmap size mismatch),
 *      so the block does NOT enter the chain. Defense holds at consensus
 *      layer.
 *   5. HARM: any caller reading bundle.messages before validation (debug logs,
 *      RPC paths, mining template inspection) sees 256 bogus messages and
 *      allocates memory proportionally. In a P2P block-spam scenario this is
 *      a 256x amplification over the legitimate 9-of-17 path.
 *
 * Impact: LOW-MEDIUM (hardening gap + caller-trust trap)
 *   No consensus split, no forgery, no fund theft. A memory-amplification
 *   DoS amplifier for block-spam; a trap for any future caller threading
 *   the pre-validation bundle downstream. The fix is one line:
 *       const uint16_t total_oracles = static_cast<uint16_t>(
 *           std::max(1, params.nOracleTotalOracles));
 *   — forcing the extract path to use the consensus-fixed total and matching
 *   ValidatePhaseThreeBundle's expectations. DecodeBitmap with the correct
 *   total would then reject the oversized bitmap at line 70 (size mismatch),
 *   bundle.messages stays empty, and the caller sees a clean "extract failed"
 *   instead of a polluted bundle.
 *
 * This test asserts the PRE-FIX invariant:
 *   - ExtractOracleBundle returns true on a 32-byte bitmap.
 *   - bundle.messages.size() == 256 (BUG — should be bounded by
 *     nOracleTotalOracles after the fix).
 *   - Out-of-range IDs (>= nOracleTotalOracles) present in bundle.messages.
 *   - ValidatePhaseThreeBundle still rejects (defense-in-depth holds today).
 *   - Smaller bitmaps produce the same bounded set of IDs as the validator,
 *     but the larger one produces a superset.
 *
 * When the fix lands (tie total_oracles to params.nOracleTotalOracles), the
 * primary BOOST_CHECK_MESSAGE lines flip: ExtractOracleBundle should return
 * false or produce an empty messages list — this test must then be rewritten
 * against the fixed API.
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <oracle/bundle_manager.h>
#include <oracle/musig2_aggregator.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <util/time.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

// Build a v0x03 payload with an arbitrary bitmap and fixed sig/price/ts.
std::vector<unsigned char> BuildV03Payload(const std::vector<unsigned char>& bitmap,
                                           uint64_t price_micro_usd,
                                           int64_t timestamp,
                                           int32_t epoch,
                                           const std::vector<unsigned char>& sig64)
{
    assert(sig64.size() == 64);
    std::vector<unsigned char> data;
    data.push_back(0x03); // version
    data.push_back(static_cast<unsigned char>(bitmap.size())); // bitmap_len
    data.insert(data.end(), bitmap.begin(), bitmap.end());

    uint32_t ep = static_cast<uint32_t>(epoch);
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<unsigned char>(ep & 0xFF));
        ep >>= 8;
    }
    uint64_t p = price_micro_usd;
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<unsigned char>(p & 0xFF));
        p >>= 8;
    }
    uint64_t ts = static_cast<uint64_t>(timestamp);
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<unsigned char>(ts & 0xFF));
        ts >>= 8;
    }
    data.insert(data.end(), sig64.begin(), sig64.end());
    return data;
}

// Wrap payload in an OP_RETURN OP_ORACLE script using OP_PUSHDATA1/2 as needed.
CScript MakeOracleScript(const std::vector<unsigned char>& payload)
{
    CScript script;
    script << OP_RETURN << OP_ORACLE;
    script << payload; // CScript << vector selects the correct PUSHDATA opcode.
    return script;
}

// Build a minimal coinbase carrying the oracle OP_RETURN in vout[1].
CMutableTransaction MakeOracleCoinbase(const CScript& oracle_script, int32_t height)
{
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << height;
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 50 * 100000000LL;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;
    coinbase.vout.push_back(CTxOut(0, oracle_script));
    return coinbase;
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(rh56_oversized_bitmap_message_inflation_tests, RegTestingSetup)

// ============================================================================
// rh56_extract_inflates_messages_with_oversized_bitmap
//
// Demonstrates the ExtractOracleBundle side-effect:
//   - bitmap_len = 32, every byte 0xFF → 256 claimed participants
//   - ExtractOracleBundle returns true
//   - bundle.messages.size() == 256 (BUG: should be <= nOracleTotalOracles)
//   - Messages with oracle_id >= nOracleTotalOracles carry null pubkeys
// ============================================================================
BOOST_AUTO_TEST_CASE(rh56_extract_inflates_messages_with_oversized_bitmap)
{
    BOOST_TEST_MESSAGE("=== RH-56: ExtractOracleBundle 256-message inflation via 32-byte bitmap ===");

    const Consensus::Params& params = Params().GetConsensus();
    const int total_on_consensus = params.nOracleTotalOracles;
    BOOST_TEST_MESSAGE("  regtest nOracleTotalOracles = " << total_on_consensus);

    // 32-byte bitmap, every bit set → claims oracles 0..255.
    std::vector<unsigned char> bitmap(32, 0xFF);
    std::vector<unsigned char> sig(64, 0x11);
    const uint64_t price = 100000ULL; // $0.10
    const int64_t ts = 1700000000;
    const int32_t epoch = 0;

    auto payload = BuildV03Payload(bitmap, price, ts, epoch, sig);
    // Payload is 1 (version) + 1 (bitmap_len) + 32 (bitmap) + 4 (epoch) + 8 (price) + 8 (ts) + 64 (sig) = 118
    BOOST_REQUIRE_EQUAL(payload.size(), 118u);

    CScript script = MakeOracleScript(payload);
    CMutableTransaction cb = MakeOracleCoinbase(script, 101);

    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    COracleBundle bundle;
    bool extracted = manager.ExtractOracleBundle(CTransaction(cb), bundle);

    // PRIMARY ASSERTION: ExtractOracleBundle succeeds despite the oversized bitmap.
    // Pre-fix: true. Post-fix (clamp total_oracles to params.nOracleTotalOracles):
    // DecodeBitmap size-check fails → extract returns false OR messages.empty().
    BOOST_CHECK_MESSAGE(extracted,
        "URGENT-STOP-CONDITION rh56: ExtractOracleBundle unexpectedly rejected "
        "the 32-byte oversized bitmap. If this flips to reject, the clamp-to-"
        "params.nOracleTotalOracles fix may already be in place — update this "
        "test against the new API.");

    if (extracted) {
        BOOST_TEST_MESSAGE("  bundle.version = " << static_cast<int>(bundle.version));
        BOOST_TEST_MESSAGE("  bundle.messages.size() = " << bundle.messages.size());
        BOOST_TEST_MESSAGE("  bundle.participation_bitmap.size() = " << bundle.participation_bitmap.size());

        BOOST_REQUIRE_EQUAL(bundle.version, 3);
        BOOST_REQUIRE_EQUAL(bundle.participation_bitmap.size(), 32u);

        // 256 bits were set → 256 synthetic oracle messages pushed. This is
        // the inflation symptom — the validator's consensus total is 7 on
        // regtest, 17 on mainnet; an honest bundle would have at most that
        // many messages.
        BOOST_CHECK_MESSAGE(bundle.messages.size() == 256,
            "Pre-fix bug: bundle.messages should contain 256 inflated entries "
            "(size=" << bundle.messages.size() << ")");

        // Count messages whose oracle_id >= params.nOracleTotalOracles.
        // These cannot correspond to any real chainparams oracle and must
        // carry a default-constructed (invalid) pubkey.
        size_t out_of_range = 0;
        size_t null_pubkey_count = 0;
        for (const auto& m : bundle.messages) {
            if (m.oracle_id >= static_cast<uint32_t>(total_on_consensus)) {
                ++out_of_range;
                if (!m.oracle_pubkey.IsFullyValid()) ++null_pubkey_count;
            }
        }
        BOOST_TEST_MESSAGE("  out-of-range IDs (>= " << total_on_consensus << "): " << out_of_range);
        BOOST_TEST_MESSAGE("  of those, null/invalid pubkey: " << null_pubkey_count);

        // Expect ALL out-of-range IDs to have null pubkeys (GetOracleNode → nullptr).
        BOOST_CHECK_MESSAGE(out_of_range > 0,
            "Pre-fix bug: out-of-range oracle IDs should be present in "
            "inflated bundle.messages");
        BOOST_CHECK_MESSAGE(null_pubkey_count == out_of_range,
            "Pre-fix bug: all out-of-range IDs should carry default-constructed "
            "(invalid) pubkeys (null=" << null_pubkey_count << ", out_of_range="
            << out_of_range << ")");
    }
}

// ============================================================================
// rh56_validator_rejects_oversized_bitmap_defense
//
// Confirms the consensus defender still rejects: ValidatePhaseThreeBundle
// calls DecodeBitmap with params.nOracleTotalOracles (NOT the inflated total),
// catching the bitmap-size mismatch and refusing the bundle. This is the
// defense-in-depth layer that keeps the extract-time inflation from being a
// consensus split today.
// ============================================================================
BOOST_AUTO_TEST_CASE(rh56_validator_rejects_oversized_bitmap_defense)
{
    BOOST_TEST_MESSAGE("=== RH-56 defender sanity: ValidatePhaseThreeBundle rejects oversized bitmap ===");

    const Consensus::Params& params = Params().GetConsensus();

    COracleBundle bundle;
    bundle.version = 3;
    bundle.participation_bitmap.assign(32, 0xFF);
    bundle.aggregate_sig.assign(64, 0x22);
    bundle.median_price_micro_usd = 100000ULL;
    bundle.timestamp = 1700000000;
    bundle.epoch = 0;

    std::string error;
    bool ok = OracleBundleManager::ValidatePhaseThreeBundle(bundle, /*block_height=*/0, params, error);

    BOOST_CHECK_MESSAGE(!ok,
        "Defender-path sanity: consensus MUST reject the 32-byte oversized "
        "bitmap. If this flips to accepted, the bitmap inflation becomes a "
        "CRITICAL consensus-bypass — promote to URGENT-STOP-CONDITION.");
    BOOST_TEST_MESSAGE("  rejection reason: " << error);

    // The error text varies slightly with code version; we just confirm the
    // validator refused and produced SOME error message.
    BOOST_CHECK(!error.empty());
}

// ============================================================================
// rh56_decode_bitmap_total_mismatch_surface
//
// Pure DecodeBitmap PoC that documents the core asymmetry:
//   DecodeBitmap(32-byte bitmap, total=256)  → 256 IDs
//   DecodeBitmap(32-byte bitmap, total=17)   → empty (size mismatch)
//   DecodeBitmap(32-byte bitmap, total=7)    → empty (size mismatch)
// The existence of the first call — and only the first — inside
// ExtractOracleBundle is the bug surface.
// ============================================================================
BOOST_AUTO_TEST_CASE(rh56_decode_bitmap_total_mismatch_surface)
{
    BOOST_TEST_MESSAGE("=== RH-56 surface: DecodeBitmap total_oracles asymmetry ===");

    std::vector<unsigned char> bitmap(32, 0xFF);

    auto ids_256 = MuSig2OracleAggregator::DecodeBitmap(bitmap, 256);
    auto ids_17  = MuSig2OracleAggregator::DecodeBitmap(bitmap, 17);
    auto ids_7   = MuSig2OracleAggregator::DecodeBitmap(bitmap, 7);

    BOOST_TEST_MESSAGE("  DecodeBitmap(bitmap_32, total=256) → " << ids_256.size() << " IDs");
    BOOST_TEST_MESSAGE("  DecodeBitmap(bitmap_32, total=17)  → " << ids_17.size()  << " IDs");
    BOOST_TEST_MESSAGE("  DecodeBitmap(bitmap_32, total=7)   → " << ids_7.size()   << " IDs");

    BOOST_CHECK_EQUAL(ids_256.size(), 256u);
    BOOST_CHECK(ids_17.empty());
    BOOST_CHECK(ids_7.empty());

    // The fix makes the extract path use the consensus total, matching the
    // validator path. Both would then reject the 32-byte bitmap identically.
}

BOOST_AUTO_TEST_SUITE_END()
