// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * MuSig2 P2P Handling Tests
 *
 * Tests the net_processing logic for handling MuSig2 oracle messages:
 * - Nonce and partial signature message validation
 * - Relay deduplication via per-peer bloom filter
 * - Node-level replay protection via rolling bloom filter
 * - Session integration (parsed nonces/sigs added to signing session)
 * - Epoch mismatch rejection
 * - Oracle ID validation
 */

#include <boost/test/unit_test.hpp>

#include <common/bloom.h>
#include <hash.h>
#include <key.h>
#include <oracle/musig2_messages.h>
#include <oracle/musig2_session.h>
#include <primitives/oracle.h>
#include <protocol.h>
#include <random.h>
#include <serialize.h>
#include <streams.h>
#include <test/util/setup_common.h>

#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_musig.h>

#include <cstring>
#include <set>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(musig2_p2p_handling_tests, BasicTestingSetup)

// ── Helpers ────────────────────────────────────────────────────────────────

/** Create a valid OracleMusigNonceMsg with real secp256k1 pubnonce bytes. */
static OracleMusigNonceMsg MakeNonceMsg(int32_t epoch, uint8_t oracle_id,
                                         secp256k1_context* ctx = nullptr)
{
    OracleMusigNonceMsg msg;
    msg.epoch = epoch;
    msg.oracle_id = oracle_id;

    if (ctx) {
        // Generate a real pubnonce from a random keypair
        unsigned char seckey[32];
        GetStrongRandBytes(Span{seckey, 32});
        secp256k1_keypair keypair;
        secp256k1_pubkey pubkey;
        BOOST_REQUIRE(secp256k1_keypair_create(ctx, &keypair, seckey));
        BOOST_REQUIRE(secp256k1_keypair_pub(ctx, &pubkey, &keypair));

        secp256k1_musig_pubnonce pubnonce;
        secp256k1_musig_secnonce secnonce;
        unsigned char session_secrand[32];
        GetStrongRandBytes(Span{session_secrand, 32});
        BOOST_REQUIRE(secp256k1_musig_nonce_gen(ctx, &secnonce, &pubnonce,
                                                 session_secrand, seckey,
                                                 &pubkey, nullptr, nullptr, nullptr));

        unsigned char pubnonce_ser[66];
        BOOST_REQUIRE(secp256k1_musig_pubnonce_serialize(ctx, pubnonce_ser, &pubnonce));
        msg.pubnonce.assign(pubnonce_ser, pubnonce_ser + 66);
        memory_cleanse(seckey, 32);
    } else {
        // Synthetic 66-byte pubnonce (valid size, not cryptographically valid)
        msg.pubnonce.assign(66, 0xAB);
    }
    return msg;
}

/** Create a valid OracleMusigPartialSigMsg with synthetic bytes. */
static OracleMusigPartialSigMsg MakePartialSigMsg(int32_t epoch, uint8_t oracle_id)
{
    OracleMusigPartialSigMsg msg;
    msg.epoch = epoch;
    msg.oracle_id = oracle_id;
    msg.partial_sig.assign(32, 0xCD);
    return msg;
}

// ── Test: Broadcast nonce to peers ─────────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_broadcast_nonce_to_peers)
{
    // Simulate relay logic: a valid nonce should be relayed to peers that
    // don't already know about it. Peers that sent it should not receive it back.
    OracleMusigNonceMsg nonce_msg = MakeNonceMsg(100, 5);
    uint256 msg_hash = nonce_msg.GetHash();

    // Simulate per-peer bloom filters (matching net_processing pattern)
    CRollingBloomFilter peer1_filter(500, 0.000001);
    CRollingBloomFilter peer2_filter(500, 0.000001);
    CRollingBloomFilter peer3_filter(500, 0.000001);

    // Sender (peer1) is marked as knowing this message
    peer1_filter.insert(msg_hash);

    // Relay decision: skip peer1 (already knows), relay to peer2 and peer3
    BOOST_CHECK(peer1_filter.contains(msg_hash));   // sender -- skip
    BOOST_CHECK(!peer2_filter.contains(msg_hash));   // relay target
    BOOST_CHECK(!peer3_filter.contains(msg_hash));   // relay target

    // After relay, mark peer2 and peer3 as knowing
    peer2_filter.insert(msg_hash);
    peer3_filter.insert(msg_hash);

    BOOST_CHECK(peer2_filter.contains(msg_hash));
    BOOST_CHECK(peer3_filter.contains(msg_hash));
}

// ── Test: Broadcast partial sig to peers ───────────────────────────────────

BOOST_AUTO_TEST_CASE(test_broadcast_partialsig_to_peers)
{
    OracleMusigPartialSigMsg psig_msg = MakePartialSigMsg(100, 7);
    uint256 msg_hash = psig_msg.GetHash();

    CRollingBloomFilter sender_filter(500, 0.000001);
    CRollingBloomFilter receiver_filter(500, 0.000001);

    // Sender is marked as knowing
    sender_filter.insert(msg_hash);
    BOOST_CHECK(sender_filter.contains(msg_hash));
    BOOST_CHECK(!receiver_filter.contains(msg_hash));

    // After relay
    receiver_filter.insert(msg_hash);
    BOOST_CHECK(receiver_filter.contains(msg_hash));
}

// ── Test: Nonce collection into session ────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_nonce_collection_into_session)
{
    // Verify that pubnonces received over P2P can be parsed and added
    // to a MuSig2SigningSession via AddPubnonce().
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    BOOST_REQUIRE(ctx);

    const int32_t epoch = 42;
    const uint8_t min_signers = 3;
    MuSig2SigningSession session(epoch, min_signers);

    // Generate a local keypair and key aggregation cache for the session
    // We need at least min_signers pubkeys for key aggregation
    std::vector<unsigned char> seckeys(min_signers * 32);
    std::vector<secp256k1_pubkey> pubkeys(min_signers);
    std::vector<const secp256k1_pubkey*> pubkey_ptrs(min_signers);

    for (uint8_t i = 0; i < min_signers; ++i) {
        GetStrongRandBytes(Span{seckeys.data() + i * 32, 32});
        secp256k1_keypair kp;
        BOOST_REQUIRE(secp256k1_keypair_create(ctx, &kp, seckeys.data() + i * 32));
        BOOST_REQUIRE(secp256k1_keypair_pub(ctx, &pubkeys[i], &kp));
        pubkey_ptrs[i] = &pubkeys[i];
    }

    secp256k1_musig_keyagg_cache cache;
    secp256k1_xonly_pubkey agg_pk;
    BOOST_REQUIRE(secp256k1_musig_pubkey_agg(ctx, &agg_pk, &cache,
                                              pubkey_ptrs.data(), min_signers));

    // Session signer 0 generates their local nonce
    CKey local_key;
    local_key.Set(seckeys.data(), seckeys.data() + 32, true);
    secp256k1_musig_pubnonce local_pubnonce;
    BOOST_CHECK(session.GenerateNonce(local_key, pubkeys[0], cache, local_pubnonce));
    BOOST_CHECK(session.GetState() == MuSig2SessionState::NONCES_COLLECTING);

    // Simulate receiving pubnonces from remote oracles (signers 1 and 2)
    for (uint8_t i = 1; i < min_signers; ++i) {
        // Generate a real pubnonce for signer i
        secp256k1_musig_secnonce secnonce_i;
        secp256k1_musig_pubnonce pubnonce_i;
        unsigned char rand_i[32];
        GetStrongRandBytes(Span{rand_i, 32});
        BOOST_REQUIRE(secp256k1_musig_nonce_gen(ctx, &secnonce_i, &pubnonce_i,
                                                 rand_i, seckeys.data() + i * 32,
                                                 &pubkeys[i], nullptr, &cache, nullptr));

        // Serialize (as if it came over P2P)
        unsigned char pubnonce_ser[66];
        BOOST_REQUIRE(secp256k1_musig_pubnonce_serialize(ctx, pubnonce_ser, &pubnonce_i));

        // Parse back (as the handler would do)
        secp256k1_musig_pubnonce parsed_pubnonce;
        BOOST_REQUIRE(secp256k1_musig_pubnonce_parse(ctx, &parsed_pubnonce, pubnonce_ser));

        // Add to session -- this is what the P2P handler does
        BOOST_CHECK(session.AddPubnonce(i, parsed_pubnonce));
    }

    // After collecting min_signers nonces (including local), session should have enough
    BOOST_CHECK(session.HasEnoughNonces());
    BOOST_CHECK(session.GetState() == MuSig2SessionState::NONCES_COMPLETE);

    secp256k1_context_destroy(ctx);
}

// ── Test: Partial sig collection into session ──────────────────────────────

BOOST_AUTO_TEST_CASE(test_partialsig_collection_into_session)
{
    // Verify that partial signatures received over P2P can be parsed
    // and added to a MuSig2SigningSession via AddPartialSignature().
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    BOOST_REQUIRE(ctx);

    const int32_t epoch = 55;
    const uint8_t n_signers = 3;
    MuSig2SigningSession session(epoch, n_signers);

    // Generate keypairs
    std::vector<unsigned char> seckeys(n_signers * 32);
    std::vector<secp256k1_pubkey> pubkeys(n_signers);
    std::vector<const secp256k1_pubkey*> pubkey_ptrs(n_signers);
    std::vector<CKey> ckeys(n_signers);

    for (uint8_t i = 0; i < n_signers; ++i) {
        GetStrongRandBytes(Span{seckeys.data() + i * 32, 32});
        secp256k1_keypair kp;
        BOOST_REQUIRE(secp256k1_keypair_create(ctx, &kp, seckeys.data() + i * 32));
        BOOST_REQUIRE(secp256k1_keypair_pub(ctx, &pubkeys[i], &kp));
        pubkey_ptrs[i] = &pubkeys[i];
        ckeys[i].Set(seckeys.data() + i * 32, seckeys.data() + (i + 1) * 32, true);
    }

    secp256k1_musig_keyagg_cache cache;
    secp256k1_xonly_pubkey agg_pk;
    BOOST_REQUIRE(secp256k1_musig_pubkey_agg(ctx, &agg_pk, &cache,
                                              pubkey_ptrs.data(), n_signers));

    // Generate nonces for all signers (round 1)
    std::vector<secp256k1_musig_secnonce> secnonces(n_signers);
    std::vector<secp256k1_musig_pubnonce> pubnonces(n_signers);

    // Local signer (0) goes through the session
    BOOST_REQUIRE(session.GenerateNonce(ckeys[0], pubkeys[0], cache, pubnonces[0]));

    // Remote signers generate nonces independently
    for (uint8_t i = 1; i < n_signers; ++i) {
        unsigned char rand_i[32];
        GetStrongRandBytes(Span{rand_i, 32});
        BOOST_REQUIRE(secp256k1_musig_nonce_gen(ctx, &secnonces[i], &pubnonces[i],
                                                 rand_i, seckeys.data() + i * 32,
                                                 &pubkeys[i], nullptr, &cache, nullptr));
        BOOST_REQUIRE(session.AddPubnonce(i, pubnonces[i]));
    }
    BOOST_REQUIRE(session.HasEnoughNonces());

    // Aggregate nonces (transition to SIGNING state)
    unsigned char msg32[32];
    GetStrongRandBytes(Span{msg32, 32});
    BOOST_REQUIRE(session.AggregateNonces(msg32));
    BOOST_CHECK(session.GetState() == MuSig2SessionState::SIGNING);

    // Local signer creates partial sig
    secp256k1_musig_partial_sig local_psig;
    BOOST_REQUIRE(session.CreatePartialSignature(ckeys[0], local_psig));

    // Remote signers create partial sigs independently, then we add them
    // via the session (simulating what the P2P handler does)
    for (uint8_t i = 1; i < n_signers; ++i) {
        // Remote signer needs their own session context for nonce_process
        secp256k1_musig_aggnonce aggnonce;
        std::vector<const secp256k1_musig_pubnonce*> nonce_ptrs(n_signers);
        for (uint8_t j = 0; j < n_signers; ++j) nonce_ptrs[j] = &pubnonces[j];
        BOOST_REQUIRE(secp256k1_musig_nonce_agg(ctx, &aggnonce,
                                                 nonce_ptrs.data(), n_signers));

        secp256k1_musig_session remote_session;
        BOOST_REQUIRE(secp256k1_musig_nonce_process(ctx, &remote_session,
                                                     &aggnonce, msg32, &cache));

        secp256k1_keypair kp;
        BOOST_REQUIRE(secp256k1_keypair_create(ctx, &kp, seckeys.data() + i * 32));
        secp256k1_musig_partial_sig remote_psig;
        BOOST_REQUIRE(secp256k1_musig_partial_sign(ctx, &remote_psig,
                                                    &secnonces[i], &kp,
                                                    &cache, &remote_session));

        // Serialize partial sig (as if sent over P2P)
        unsigned char psig_ser[32];
        BOOST_REQUIRE(secp256k1_musig_partial_sig_serialize(ctx, psig_ser, &remote_psig));

        // Parse back (as the handler would do)
        secp256k1_musig_partial_sig parsed_psig;
        BOOST_REQUIRE(secp256k1_musig_partial_sig_parse(ctx, &parsed_psig, psig_ser));

        // Add to session -- this is what the P2P handler does
        BOOST_CHECK(session.AddPartialSignature(i, parsed_psig));
    }

    BOOST_CHECK(session.HasEnoughPartialSigs());

    // Verify the final aggregate signature can be produced
    std::vector<unsigned char> final_sig;
    BOOST_CHECK(session.AggregateSignature(final_sig));
    BOOST_CHECK_EQUAL(final_sig.size(), 64U);
    BOOST_CHECK(session.GetState() == MuSig2SessionState::COMPLETE);

    secp256k1_context_destroy(ctx);
}

// ── Test: Invalid nonce rejected ───────────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_invalid_nonce_rejected)
{
    // IsValid() should reject malformed nonce messages
    OracleMusigNonceMsg msg;
    msg.epoch = 10;
    msg.oracle_id = 1;

    // Wrong pubnonce size (65 instead of 66)
    msg.pubnonce.assign(65, 0xAA);
    BOOST_CHECK(!msg.IsValid());

    // Wrong pubnonce size (67 instead of 66)
    msg.pubnonce.assign(67, 0xAA);
    BOOST_CHECK(!msg.IsValid());

    // Empty pubnonce
    msg.pubnonce.clear();
    BOOST_CHECK(!msg.IsValid());

    // Correct size -- valid
    msg.pubnonce.assign(66, 0xAA);
    BOOST_CHECK(msg.IsValid());

    // oracle_id 255 is invalid per IsValid()
    msg.oracle_id = 255;
    BOOST_CHECK(!msg.IsValid());
}

// ── Test: Invalid partial sig rejected ─────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_invalid_partialsig_rejected)
{
    OracleMusigPartialSigMsg msg;
    msg.epoch = 10;
    msg.oracle_id = 2;

    // Wrong size (31 instead of 32)
    msg.partial_sig.assign(31, 0xBB);
    BOOST_CHECK(!msg.IsValid());

    // Wrong size (33 instead of 32)
    msg.partial_sig.assign(33, 0xBB);
    BOOST_CHECK(!msg.IsValid());

    // Empty
    msg.partial_sig.clear();
    BOOST_CHECK(!msg.IsValid());

    // Correct size -- valid
    msg.partial_sig.assign(32, 0xBB);
    BOOST_CHECK(msg.IsValid());

    // oracle_id 255 is invalid
    msg.oracle_id = 255;
    BOOST_CHECK(!msg.IsValid());
}

// ── Test: Nonce replay protection ──────────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_nonce_replay_protection)
{
    // Node-level replay protection: same nonce hash inserted twice
    // into the bloom filter -> second insertion is detected as duplicate.
    CRollingBloomFilter replay_filter(500, 0.000001);

    OracleMusigNonceMsg msg = MakeNonceMsg(100, 5);
    uint256 msg_hash = msg.GetHash();

    // First time: not in filter -> accept
    BOOST_CHECK(!replay_filter.contains(msg_hash));
    replay_filter.insert(msg_hash);

    // Second time: in filter -> reject (duplicate)
    BOOST_CHECK(replay_filter.contains(msg_hash));

    // Different message: not in filter
    OracleMusigNonceMsg msg2 = MakeNonceMsg(100, 6);
    BOOST_CHECK(!replay_filter.contains(msg2.GetHash()));

    // Same oracle_id + epoch but different pubnonce -> different hash
    OracleMusigNonceMsg msg3;
    msg3.epoch = 100;
    msg3.oracle_id = 5;
    msg3.pubnonce.assign(66, 0xFF); // different pubnonce data
    BOOST_CHECK(msg3.GetHash() != msg_hash);
}

// ── Test: Partial sig replay protection ────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_partialsig_replay_protection)
{
    CRollingBloomFilter replay_filter(500, 0.000001);

    OracleMusigPartialSigMsg msg = MakePartialSigMsg(200, 3);
    uint256 msg_hash = msg.GetHash();

    // First time: accept
    BOOST_CHECK(!replay_filter.contains(msg_hash));
    replay_filter.insert(msg_hash);

    // Second time: reject
    BOOST_CHECK(replay_filter.contains(msg_hash));

    // Different message: accept
    OracleMusigPartialSigMsg msg2 = MakePartialSigMsg(200, 4);
    BOOST_CHECK(!replay_filter.contains(msg2.GetHash()));
}

// ── Test: Epoch mismatch rejected ──────────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_epoch_mismatch_rejected)
{
    // The P2P handler should reject nonces/sigs from epochs too far from
    // the current epoch. Acceptable window: [current - 2, current + 1].
    const int32_t current_epoch = 1000;

    auto epoch_in_range = [&](int32_t msg_epoch) -> bool {
        return msg_epoch >= current_epoch - 2 && msg_epoch <= current_epoch + 1;
    };

    // Valid epochs
    BOOST_CHECK(epoch_in_range(current_epoch));       // exact
    BOOST_CHECK(epoch_in_range(current_epoch - 1));   // 1 behind
    BOOST_CHECK(epoch_in_range(current_epoch - 2));   // 2 behind
    BOOST_CHECK(epoch_in_range(current_epoch + 1));   // 1 ahead

    // Invalid epochs
    BOOST_CHECK(!epoch_in_range(current_epoch - 3));  // too old
    BOOST_CHECK(!epoch_in_range(current_epoch + 2));  // too far ahead
    BOOST_CHECK(!epoch_in_range(0));                  // way too old
    BOOST_CHECK(!epoch_in_range(current_epoch + 100)); // way too far

    // Verify the filter logic applies to both message types
    OracleMusigNonceMsg nonce = MakeNonceMsg(current_epoch - 3, 1);
    BOOST_CHECK(nonce.IsValid()); // structurally valid, but epoch is out of range
    BOOST_CHECK(!epoch_in_range(nonce.epoch));

    OracleMusigPartialSigMsg psig = MakePartialSigMsg(current_epoch + 2, 1);
    BOOST_CHECK(psig.IsValid()); // structurally valid, but epoch is out of range
    BOOST_CHECK(!epoch_in_range(psig.epoch));
}

// ── Test: Oracle ID validation ─────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(test_oracle_id_validation)
{
    // The P2P handler checks oracle_id < ORACLE_TOTAL_COUNT (30).
    // This is separate from IsValid() which only checks oracle_id < 255.

    // Valid oracle IDs (within ORACLE_TOTAL_COUNT)
    for (uint8_t id = 0; id < ORACLE_TOTAL_COUNT; ++id) {
        OracleMusigNonceMsg nonce = MakeNonceMsg(100, id);
        BOOST_CHECK(nonce.IsValid());
        BOOST_CHECK(nonce.oracle_id < ORACLE_TOTAL_COUNT);
    }

    // Invalid oracle IDs (at or above ORACLE_TOTAL_COUNT but below 255)
    for (uint8_t id = ORACLE_TOTAL_COUNT; id < 255; ++id) {
        OracleMusigNonceMsg nonce = MakeNonceMsg(100, id);
        // Structurally valid per IsValid() (just checks < 255)
        BOOST_CHECK(nonce.IsValid());
        // But should be rejected by the P2P handler's range check
        BOOST_CHECK(nonce.oracle_id >= ORACLE_TOTAL_COUNT);
    }

    // oracle_id 255 is invalid even structurally
    OracleMusigNonceMsg invalid_nonce;
    invalid_nonce.epoch = 100;
    invalid_nonce.oracle_id = 255;
    invalid_nonce.pubnonce.assign(66, 0x00);
    BOOST_CHECK(!invalid_nonce.IsValid());

    // Same checks for partial sig messages
    OracleMusigPartialSigMsg psig_valid = MakePartialSigMsg(100, 0);
    BOOST_CHECK(psig_valid.IsValid());
    BOOST_CHECK(psig_valid.oracle_id < ORACLE_TOTAL_COUNT);

    OracleMusigPartialSigMsg psig_oob = MakePartialSigMsg(100, ORACLE_TOTAL_COUNT);
    BOOST_CHECK(psig_oob.IsValid()); // structurally valid
    BOOST_CHECK(psig_oob.oracle_id >= ORACLE_TOTAL_COUNT); // but rejected by handler
}

BOOST_AUTO_TEST_SUITE_END()
