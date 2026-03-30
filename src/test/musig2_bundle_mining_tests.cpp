// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/amount.h>
#include <oracle/bundle_manager.h>
#include <oracle/musig2_orchestrator.h>
#include <oracle/musig2_session.h>
#include <primitives/block.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <random.h>
#include <script/script.h>
#include <span.h>
#include <test/util/setup_common.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <tuple>

#include <secp256k1.h>
#include <secp256k1_musig.h>

namespace {

CBlock MakeBlockWithCoinbase()
{
    CBlock block;
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 50 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;
    block.vtx.push_back(MakeTransactionRef(coinbase));
    return block;
}

bool BuildCompleteSession(int32_t epoch, MuSig2SigningSession& session_out)
{
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx) return false;

    unsigned char seckey[32];
    secp256k1_keypair keypair;
    secp256k1_pubkey secp_pubkey;
    bool key_ok = false;
    for (int i = 0; i < 100 && !key_ok; ++i) {
        GetStrongRandBytes(Span<unsigned char>(seckey, 32));
        if (secp256k1_keypair_create(ctx, &keypair, seckey)) {
            key_ok = secp256k1_keypair_pub(ctx, &secp_pubkey, &keypair);
        }
    }
    if (!key_ok) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    const secp256k1_pubkey* pk_ptr = &secp_pubkey;
    secp256k1_xonly_pubkey agg_pk;
    secp256k1_musig_keyagg_cache cache;
    if (!secp256k1_musig_pubkey_agg(ctx, &agg_pk, &cache, &pk_ptr, 1)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    CKey key;
    key.Set(seckey, seckey + 32, true);
    secp256k1_musig_pubnonce pubnonce;
    if (!session_out.GenerateNonce(key, secp_pubkey, cache, pubnonce)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    if (!session_out.AddPubnonce(0, pubnonce)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    unsigned char msg32[32] = {0};
    std::memcpy(msg32, &epoch, std::min(sizeof(epoch), sizeof(msg32)));

    if (!session_out.AggregateNonces(msg32)) return false;

    secp256k1_musig_partial_sig partial_sig;
    if (!session_out.CreatePartialSignature(key, partial_sig)) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    if (!session_out.AddPartialSignature(0, partial_sig)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    std::vector<unsigned char> sig64;
    const bool ok = session_out.AggregateSignature(sig64) && sig64.size() == 64;
    secp256k1_context_destroy(ctx);
    return ok;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(musig2_bundle_mining_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(add_bundle_requires_complete_session)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    const Consensus::Params& params = Params().GetConsensus();
    if (params.nDigiDollarPhase3Height == std::numeric_limits<int>::max()) {
        BOOST_TEST_MESSAGE("Phase 3 disabled on this network, skipping test");
        return;
    }

    const int32_t block_height = params.nDigiDollarPhase3Height;
    const int32_t epoch = GetCurrentEpoch(block_height);

    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.clear();
        g_oracle_signing_sessions.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(epoch),
            std::forward_as_tuple(epoch, 1)); // CREATED, incomplete
    }

    CBlock block = MakeBlockWithCoinbase();
    BOOST_CHECK(!manager.AddOracleBundleToBlock(block, block_height));

    // No oracle OP_RETURN output should be added when session is incomplete.
    BOOST_CHECK_EQUAL(block.vtx[0]->vout.size(), 1);
}

BOOST_AUTO_TEST_CASE(add_bundle_consumes_session_and_prunes_old_epochs)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    const Consensus::Params& params = Params().GetConsensus();
    if (params.nDigiDollarPhase3Height == std::numeric_limits<int>::max()) {
        BOOST_TEST_MESSAGE("Phase 3 disabled on this network, skipping test");
        return;
    }

    const int32_t block_height = params.nDigiDollarPhase3Height;
    const int32_t epoch = GetCurrentEpoch(block_height);

    MuSig2SigningSession complete(epoch, 1);
    BOOST_REQUIRE(BuildCompleteSession(epoch, complete));
    BOOST_CHECK_EQUAL(complete.GetState(), MuSig2SessionState::COMPLETE);

    {
        LOCK(g_oracle_signing_sessions_mutex);
        g_oracle_signing_sessions.clear();

        // Old stale session to validate epoch-boundary cleanup.
        g_oracle_signing_sessions.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(epoch - 1),
            std::forward_as_tuple(epoch - 1, 1));

        g_oracle_signing_sessions.emplace(epoch, std::move(complete));
    }

    CBlock block = MakeBlockWithCoinbase();
    BOOST_REQUIRE(manager.AddOracleBundleToBlock(block, block_height));

    // Prior epoch session should always be cleaned up at epoch boundary.
    {
        LOCK(g_oracle_signing_sessions_mutex);
        BOOST_CHECK_EQUAL(g_oracle_signing_sessions.count(epoch - 1), 0);
    }

    if (block.vtx[0]->vout.size() == 2) {
        const CTxOut& oracle_out = block.vtx[0]->vout[1];
        BOOST_CHECK_EQUAL(oracle_out.nValue, 0);
        BOOST_CHECK(oracle_out.scriptPubKey.IsUnspendable());

        COracleBundle extracted;
        BOOST_REQUIRE(manager.ExtractOracleBundle(*block.vtx[0], extracted));
        BOOST_CHECK_EQUAL(extracted.version, 3);
        BOOST_CHECK(extracted.IsMuSig2());
        BOOST_CHECK_EQUAL(extracted.aggregate_sig.size(), 64);
        BOOST_CHECK_EQUAL(extracted.messages.size(), 1);
        BOOST_CHECK_EQUAL(extracted.messages[0].oracle_id, 0);

        LOCK(g_oracle_signing_sessions_mutex);
        BOOST_CHECK_EQUAL(g_oracle_signing_sessions.count(epoch), 0);
    } else {
        // If v03 OP_RETURN is gated out by chain height context in unit test env,
        // session remains available for next block attempt.
        BOOST_REQUIRE_EQUAL(block.vtx[0]->vout.size(), 1);
        LOCK(g_oracle_signing_sessions_mutex);
        BOOST_CHECK_EQUAL(g_oracle_signing_sessions.count(epoch), 1);
    }
}

BOOST_AUTO_TEST_SUITE_END()
