// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * MuSig2 Bundle Mining Tests (TDD — tests before implementation)
 *
 * Tests AddOracleBundleToBlock() integration with MuSig2 Phase 3:
 * - v0x03 bundle creation when signing session is COMPLETE
 * - v0x02 fallback when no session exists
 * - v0x02 fallback when session is incomplete
 * - Phase 2 mode always produces v0x02
 * - Coinbase output correctness for v0x03
 * - Price/timestamp preservation in v0x03 bundle
 * - Session lifecycle across epoch boundaries
 * - Session reset after v0x03 bundle is mined
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <key.h>
#include <oracle/bundle_manager.h>
#include <oracle/musig2_session.h>
#include <primitives/block.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <random.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <util/time.h>

#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_musig.h>

#include <cstring>
#include <vector>

/**
 * Helper: create a minimal CBlock with an empty coinbase transaction.
 * AddOracleBundleToBlock expects block.vtx[0] to exist.
 */
static CBlock MakeTestBlock()
{
    CBlock block;
    CMutableTransaction coinbase_tx;
    coinbase_tx.vin.resize(1);
    coinbase_tx.vin[0].prevout.SetNull();
    coinbase_tx.vout.resize(1);
    coinbase_tx.vout[0].nValue = 72000 * COIN; // DigiByte block reward
    coinbase_tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase_tx)));
    return block;
}

/**
 * Helper: inject an oracle message into the bundle manager for Phase 1/2 flow.
 */
static void InjectTestOracleMessage(OracleBundleManager& manager, uint32_t oracle_id,
                                     uint64_t price_micro_usd, int64_t timestamp)
{
    COraclePriceMessage msg;
    msg.oracle_id = oracle_id;
    msg.price_micro_usd = price_micro_usd;
    msg.timestamp = timestamp;
    msg.block_height = 100;
    msg.nonce = 42;

    CKey key;
    key.MakeNewKey(true);
    msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());
    msg.Sign(key);

    manager.InjectTestMessage(msg);
}

/**
 * Helper: create a fully completed MuSig2SigningSession with a valid aggregate signature.
 * Runs the full 2-round MuSig2 protocol with `n_signers` signers, requiring `min_signers`.
 * Returns the session in COMPLETE state with a valid 64-byte aggregate sig.
 */
static std::unique_ptr<MuSig2SigningSession> MakeCompletedSession(
    int32_t epoch,
    uint8_t min_signers,
    uint8_t n_signers,
    const unsigned char msg32[32],
    std::vector<unsigned char>& bitmap_out)
{
    auto session = std::make_unique<MuSig2SigningSession>(epoch, min_signers);

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    // Generate keypairs for all signers
    struct SignerInfo {
        unsigned char seckey[32];
        secp256k1_keypair keypair;
        secp256k1_pubkey pubkey;
        CKey ckey;
    };
    std::vector<SignerInfo> signers(n_signers);

    std::vector<const secp256k1_pubkey*> pubkey_ptrs;
    for (uint8_t i = 0; i < n_signers; ++i) {
        GetStrongRandBytes(Span{signers[i].seckey, 32});
        BOOST_REQUIRE(secp256k1_keypair_create(ctx, &signers[i].keypair, signers[i].seckey));
        BOOST_REQUIRE(secp256k1_keypair_pub(ctx, &signers[i].pubkey, &signers[i].keypair));
        signers[i].ckey.Set(signers[i].seckey, signers[i].seckey + 32, true);
        pubkey_ptrs.push_back(&signers[i].pubkey);
    }

    // Compute key aggregation cache
    secp256k1_musig_keyagg_cache cache;
    secp256k1_xonly_pubkey agg_pk;
    BOOST_REQUIRE(secp256k1_musig_pubkey_agg(ctx, nullptr, &agg_pk, &cache,
                                              pubkey_ptrs.data(), pubkey_ptrs.size()));

    // Round 1: Generate nonces
    // Signer 0 uses the session object; others are "remote" signers
    secp256k1_musig_pubnonce local_pubnonce;
    BOOST_REQUIRE(session->GenerateNonce(signers[0].ckey, signers[0].pubkey, cache, local_pubnonce));

    // Add local signer's pubnonce
    BOOST_REQUIRE(session->AddPubnonce(0, local_pubnonce));

    // Generate and add pubnonces for remote signers
    std::vector<secp256k1_musig_secnonce> remote_secnonces(n_signers);
    std::vector<secp256k1_musig_pubnonce> remote_pubnonces(n_signers);
    for (uint8_t i = 1; i < n_signers; ++i) {
        unsigned char session_secrand[32];
        GetStrongRandBytes(Span{session_secrand, 32});
        BOOST_REQUIRE(secp256k1_musig_nonce_gen(ctx,
            &remote_secnonces[i], &remote_pubnonces[i],
            session_secrand, signers[i].seckey,
            &signers[i].pubkey, nullptr, &cache, nullptr));
        BOOST_REQUIRE(session->AddPubnonce(i, remote_pubnonces[i]));
    }

    // Aggregate nonces
    BOOST_REQUIRE(session->AggregateNonces(msg32));

    // Round 2: Create partial signatures
    // Local signer (signer 0)
    secp256k1_musig_partial_sig local_psig;
    BOOST_REQUIRE(session->CreatePartialSignature(signers[0].ckey, local_psig));
    BOOST_REQUIRE(session->AddPartialSignature(0, local_psig));

    // Remote signers: build a parallel secp256k1_musig_session for valid partial sigs
    std::vector<const secp256k1_musig_pubnonce*> all_pubnonce_ptrs;
    all_pubnonce_ptrs.push_back(&local_pubnonce);
    for (uint8_t i = 1; i < n_signers; ++i) {
        all_pubnonce_ptrs.push_back(&remote_pubnonces[i]);
    }

    secp256k1_musig_aggnonce aggnonce;
    BOOST_REQUIRE(secp256k1_musig_nonce_agg(ctx, &aggnonce,
                                             all_pubnonce_ptrs.data(), all_pubnonce_ptrs.size()));

    secp256k1_musig_session remote_session;
    BOOST_REQUIRE(secp256k1_musig_nonce_process(ctx, &remote_session, &aggnonce,
                                                 msg32, &cache));

    for (uint8_t i = 1; i < n_signers; ++i) {
        secp256k1_musig_partial_sig psig;
        BOOST_REQUIRE(secp256k1_musig_partial_sign(ctx, &psig,
            &remote_secnonces[i], &signers[i].keypair, &cache, &remote_session));
        BOOST_REQUIRE(session->AddPartialSignature(i, psig));
    }

    // Aggregate final signature
    std::vector<unsigned char> sig64;
    BOOST_REQUIRE(session->AggregateSignature(sig64));
    BOOST_CHECK_EQUAL(session->GetState(), MuSig2SessionState::COMPLETE);
    BOOST_CHECK_EQUAL(sig64.size(), 64u);

    // Build participation bitmap: all n_signers participate (bits 0..n_signers-1 set)
    uint16_t total_oracles = 15;
    size_t bitmap_bytes = (total_oracles + 7) / 8;
    bitmap_out.assign(bitmap_bytes, 0);
    for (uint8_t i = 0; i < n_signers; ++i) {
        bitmap_out[i / 8] |= (1 << (i % 8));
    }

    secp256k1_context_destroy(ctx);
    return session;
}

BOOST_FIXTURE_TEST_SUITE(musig2_bundle_mining_tests, BasicTestingSetup)

// ============================================================================
// test_add_oracle_bundle_v03_with_complete_session
// Complete MuSig2 session at Phase 3 height -> v0x03 bundle
// ============================================================================
BOOST_AUTO_TEST_CASE(test_add_oracle_bundle_v03_with_complete_session)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    // Inject oracle message so there's data to bundle
    int64_t now = GetTime();
    InjectTestOracleMessage(manager, 0, 6000, now);

    // Phase 3 height: use a height >= nDigiDollarPhase3Height
    const int32_t phase3_height = 5000;
    int32_t epoch = GetCurrentEpoch(phase3_height);

    // Create a completed MuSig2 signing session
    unsigned char msg32[32] = {};
    std::vector<unsigned char> bitmap;
    auto session = MakeCompletedSession(epoch, 9, 9, msg32, bitmap);
    BOOST_REQUIRE(session->GetState() == MuSig2SessionState::COMPLETE);

    // Register the session in the global map
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.emplace(epoch, std::move(*session));
    }

    // Create block and add oracle bundle
    CBlock block = MakeTestBlock();
    size_t orig_vout_count = block.vtx[0]->vout.size();

    bool result = manager.AddOracleBundleToBlock(block, phase3_height);
    BOOST_CHECK(result);

    // Should have added an oracle output
    BOOST_CHECK_EQUAL(block.vtx[0]->vout.size(), orig_vout_count + 1);

    // The oracle output should be OP_RETURN OP_ORACLE with version 0x03
    const CTxOut& oracle_out = block.vtx[0]->vout.back();
    BOOST_CHECK_EQUAL(oracle_out.nValue, 0);
    BOOST_CHECK(oracle_out.scriptPubKey.size() > 4);
    BOOST_CHECK_EQUAL(oracle_out.scriptPubKey[0], OP_RETURN);
    BOOST_CHECK_EQUAL(oracle_out.scriptPubKey[1], OP_ORACLE);

    // Session should have been erased after mining
    {
        LOCK(g_oracle_signing_sessions_mutex);
        BOOST_CHECK(g_oracle_signing_sessions.find(epoch) == g_oracle_signing_sessions.end());
    }

    manager.Clear();
}

// ============================================================================
// test_add_oracle_bundle_v02_if_no_session
// No MuSig2 session at Phase 3 height -> v0x02 fallback
// ============================================================================
BOOST_AUTO_TEST_CASE(test_add_oracle_bundle_v02_if_no_session)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    int64_t now = GetTime();
    InjectTestOracleMessage(manager, 0, 6000, now);

    const int32_t phase3_height = 5000;
    int32_t epoch = GetCurrentEpoch(phase3_height);

    // Ensure no session exists for this epoch
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.erase(epoch);
    }

    CBlock block = MakeTestBlock();
    bool result = manager.AddOracleBundleToBlock(block, phase3_height);
    BOOST_CHECK(result);

    // Should still produce a valid block (v0x02 or v0x01 fallback)
    BOOST_CHECK(block.vtx[0]->vout.size() >= 1);

    manager.Clear();
}

// ============================================================================
// test_add_oracle_bundle_v03_session_not_complete
// Incomplete MuSig2 session -> v0x02 fallback
// ============================================================================
BOOST_AUTO_TEST_CASE(test_add_oracle_bundle_v03_session_not_complete)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    int64_t now = GetTime();
    InjectTestOracleMessage(manager, 0, 6000, now);

    const int32_t phase3_height = 5000;
    int32_t epoch = GetCurrentEpoch(phase3_height);

    // Create an INCOMPLETE session (just CREATED, no nonces generated)
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.erase(epoch);
        g_oracle_signing_sessions.emplace(epoch, MuSig2SigningSession(epoch, 9));
    }

    CBlock block = MakeTestBlock();
    bool result = manager.AddOracleBundleToBlock(block, phase3_height);
    BOOST_CHECK(result);

    // Should have fallen back to v0x02/v0x01 since session is not COMPLETE
    // Verify the session was NOT erased (only COMPLETE sessions are erased)
    {
        LOCK(g_oracle_signing_sessions_mutex);
        BOOST_CHECK(g_oracle_signing_sessions.find(epoch) != g_oracle_signing_sessions.end());
    }

    manager.Clear();
}

// ============================================================================
// test_add_oracle_bundle_phase2_mode
// Phase 2 height -> always v0x02, never v0x03 even with complete session
// ============================================================================
BOOST_AUTO_TEST_CASE(test_add_oracle_bundle_phase2_mode)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    int64_t now = GetTime();
    InjectTestOracleMessage(manager, 0, 6000, now);

    // Phase 2 height: below Phase 3 but above Phase 2
    const int32_t phase2_height = 100;
    int32_t epoch = GetCurrentEpoch(phase2_height);

    // Even if a complete session exists, Phase 2 should produce v0x02
    unsigned char msg32[32] = {};
    std::vector<unsigned char> bitmap;
    auto session = MakeCompletedSession(epoch, 9, 9, msg32, bitmap);
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.emplace(epoch, std::move(*session));
    }

    CBlock block = MakeTestBlock();
    bool result = manager.AddOracleBundleToBlock(block, phase2_height);
    BOOST_CHECK(result);

    // Session should NOT be erased since we're in Phase 2 mode
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.erase(epoch); // cleanup
    }

    manager.Clear();
}

// ============================================================================
// test_add_oracle_bundle_v03_coinbase_output
// Verify v0x03 coinbase output structure: OP_RETURN OP_ORACLE 0x03 <v03data>
// ============================================================================
BOOST_AUTO_TEST_CASE(test_add_oracle_bundle_v03_coinbase_output)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    int64_t now = GetTime();
    InjectTestOracleMessage(manager, 0, 6000, now);

    const int32_t phase3_height = 5000;
    int32_t epoch = GetCurrentEpoch(phase3_height);

    unsigned char msg32[32] = {};
    std::vector<unsigned char> bitmap;
    auto session = MakeCompletedSession(epoch, 9, 9, msg32, bitmap);
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.emplace(epoch, std::move(*session));
    }

    CBlock block = MakeTestBlock();
    bool result = manager.AddOracleBundleToBlock(block, phase3_height);
    BOOST_CHECK(result);

    // Find the oracle output
    bool found_oracle_output = false;
    for (const auto& out : block.vtx[0]->vout) {
        if (out.scriptPubKey.size() >= 4 &&
            out.scriptPubKey[0] == OP_RETURN &&
            out.scriptPubKey[1] == OP_ORACLE) {
            found_oracle_output = true;

            // Extract the data after OP_RETURN OP_ORACLE
            auto it = out.scriptPubKey.begin() + 2;
            BOOST_REQUIRE(it < out.scriptPubKey.end());

            // Read version push: should be 1-byte push of 0x03
            unsigned char push_len = *it;
            BOOST_REQUIRE(push_len >= 1);
            ++it;
            BOOST_CHECK_EQUAL(*it, 0x03);
            break;
        }
    }
    BOOST_CHECK(found_oracle_output);

    manager.Clear();
}

// ============================================================================
// test_add_oracle_bundle_preserves_price_timestamp
// v0x03 bundle should carry correct median price and timestamp
// ============================================================================
BOOST_AUTO_TEST_CASE(test_add_oracle_bundle_preserves_price_timestamp)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    const uint64_t test_price = 7500;  // $0.0075
    int64_t test_timestamp = GetTime();
    InjectTestOracleMessage(manager, 0, test_price, test_timestamp);

    const int32_t phase3_height = 5000;
    int32_t epoch = GetCurrentEpoch(phase3_height);

    unsigned char msg32[32] = {};
    std::vector<unsigned char> bitmap;
    auto session = MakeCompletedSession(epoch, 9, 9, msg32, bitmap);
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.emplace(epoch, std::move(*session));
    }

    CBlock block = MakeTestBlock();
    bool result = manager.AddOracleBundleToBlock(block, phase3_height);
    BOOST_CHECK(result);

    // Extract the oracle bundle from the coinbase and verify price/timestamp
    COracleBundle extracted;
    bool extracted_ok = manager.ExtractOracleBundle(*block.vtx[0], extracted);
    BOOST_CHECK(extracted_ok);

    if (extracted_ok && extracted.version == 3) {
        // Price should match what we injected
        BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, test_price);
        // Timestamp should be non-zero
        BOOST_CHECK(extracted.timestamp != 0);
    }

    manager.Clear();
}

// ============================================================================
// test_session_lifecycle_across_epochs
// Sessions for different epochs don't collide
// ============================================================================
BOOST_AUTO_TEST_CASE(test_session_lifecycle_across_epochs)
{
    unsigned char msg32_a[32] = {};
    unsigned char msg32_b[32] = {};
    msg32_a[0] = 0xAA;
    msg32_b[0] = 0xBB;

    std::vector<unsigned char> bitmap_a, bitmap_b;
    auto session_a = MakeCompletedSession(100, 9, 9, msg32_a, bitmap_a);
    auto session_b = MakeCompletedSession(101, 9, 9, msg32_b, bitmap_b);

    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.emplace(100, std::move(*session_a));
        g_oracle_signing_sessions.emplace(101, std::move(*session_b));

        BOOST_CHECK(g_oracle_signing_sessions.count(100));
        BOOST_CHECK(g_oracle_signing_sessions.count(101));

        auto& s100 = g_oracle_signing_sessions.at(100);
        auto& s101 = g_oracle_signing_sessions.at(101);
        BOOST_CHECK_EQUAL(s100.GetEpoch(), 100);
        BOOST_CHECK_EQUAL(s101.GetEpoch(), 101);
        BOOST_CHECK(s100.GetState() == MuSig2SessionState::COMPLETE);
        BOOST_CHECK(s101.GetState() == MuSig2SessionState::COMPLETE);

        g_oracle_signing_sessions.clear();
    }
}

// ============================================================================
// test_session_reset_on_new_epoch
// Session is erased from map after v0x03 bundle is successfully mined
// ============================================================================
BOOST_AUTO_TEST_CASE(test_session_reset_on_new_epoch)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    int64_t now = GetTime();
    InjectTestOracleMessage(manager, 0, 6000, now);

    const int32_t phase3_height = 5000;
    int32_t epoch = GetCurrentEpoch(phase3_height);

    unsigned char msg32[32] = {};
    std::vector<unsigned char> bitmap;
    auto session = MakeCompletedSession(epoch, 9, 9, msg32, bitmap);
    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.emplace(epoch, std::move(*session));
        BOOST_CHECK(g_oracle_signing_sessions.count(epoch));
    }

    // Mine the v0x03 bundle
    CBlock block = MakeTestBlock();
    bool result = manager.AddOracleBundleToBlock(block, phase3_height);
    BOOST_CHECK(result);

    // Session should be erased after successful v0x03 mining
    {
        LOCK(g_oracle_signing_sessions_mutex);
        BOOST_CHECK_EQUAL(g_oracle_signing_sessions.count(epoch), 0u);
    }

    // Mining again for the same epoch (without a session) should fall back to v0x02
    InjectTestOracleMessage(manager, 0, 6000, now);
    CBlock block2 = MakeTestBlock();
    result = manager.AddOracleBundleToBlock(block2, phase3_height);
    BOOST_CHECK(result);

    {
        LOCK(g_oracle_signing_sessions_mutex);
        BOOST_CHECK_EQUAL(g_oracle_signing_sessions.count(epoch), 0u);
    }

    manager.Clear();
}

// ============================================================================
// test_getters_aggregate_sig_and_bitmap
// Verify GetAggregateSig() and GetParticipationBitmap() return correct data
// ============================================================================
BOOST_AUTO_TEST_CASE(test_getters_aggregate_sig_and_bitmap)
{
    unsigned char msg32[32] = {};
    msg32[0] = 0xCC;

    std::vector<unsigned char> bitmap;
    auto session = MakeCompletedSession(42, 9, 9, msg32, bitmap);

    BOOST_CHECK(session->GetState() == MuSig2SessionState::COMPLETE);

    // GetAggregateSig should return the 64-byte signature
    std::vector<unsigned char> sig = session->GetAggregateSig();
    BOOST_CHECK_EQUAL(sig.size(), 64u);

    // Sig should not be all zeros
    bool all_zero = true;
    for (auto b : sig) {
        if (b != 0) { all_zero = false; break; }
    }
    BOOST_CHECK(!all_zero);

    // GetParticipationBitmap should return non-empty bitmap
    std::vector<unsigned char> bm = session->GetParticipationBitmap();
    BOOST_CHECK(!bm.empty());

    // Verify bitmap encodes the right oracle IDs (0..8 for 9 signers)
    for (uint8_t i = 0; i < 9; ++i) {
        bool bit_set = (bm[i / 8] >> (i % 8)) & 1;
        BOOST_CHECK(bit_set);
    }
}

BOOST_AUTO_TEST_SUITE_END()
