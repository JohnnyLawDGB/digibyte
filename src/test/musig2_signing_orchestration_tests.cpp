// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <crypto/sha256.h>
#include <oracle/musig2_aggregator.h>
#include <oracle/musig2_messages.h>
#include <oracle/signing_orchestrator.h>
#include <primitives/block.h>
#include <primitives/oracle.h>
#include <test/util/setup_common.h>

#include <secp256k1.h>
#include <secp256k1_musig.h>

BOOST_FIXTURE_TEST_SUITE(musig2_signing_orchestration_tests, RegTestingSetup)

static CKey GetRegtestMusigOracleKey(uint32_t oracle_id)
{
    const std::string seed = "digibyte_regtest_oracle_" + std::to_string(oracle_id);
    uint256 hash;
    CSHA256().Write(reinterpret_cast<const unsigned char*>(seed.data()), seed.size()).Finalize(hash.begin());

    CKey key;
    key.Set(hash.begin(), hash.end(), true);
    return key;
}

static std::vector<uint8_t> GetActiveOracleIdsForMusigTest()
{
    std::vector<uint8_t> ids;
    const uint32_t total = static_cast<uint32_t>(Params().GetConsensus().nOracleTotalOracles);
    for (const auto& node : Params().GetOracleNodes()) {
        if (node.is_active && node.id < total) {
            ids.push_back(static_cast<uint8_t>(node.id));
        }
    }
    return ids;
}

static OracleMusigNonceMsg MakeSignedMusigNonceMsg(int32_t epoch, uint8_t oracle_id)
{
    CKey key = GetRegtestMusigOracleKey(oracle_id);
    CPubKey pubkey = key.GetPubKey();

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    BOOST_REQUIRE(ctx);

    secp256k1_pubkey secp_pubkey;
    BOOST_REQUIRE(secp256k1_ec_pubkey_parse(ctx, &secp_pubkey, pubkey.data(), pubkey.size()));

    MuSig2OracleAggregator aggregator;
    secp256k1_xonly_pubkey agg_pk;
    secp256k1_musig_keyagg_cache keyagg_cache;
    BOOST_REQUIRE(aggregator.ComputeAggregatePubkey(GetActiveOracleIdsForMusigTest(), agg_pk, keyagg_cache));

    MuSig2SigningSession temp_session(epoch, static_cast<uint8_t>(Params().GetConsensus().nOracleConsensusRequired));
    secp256k1_musig_pubnonce pubnonce;
    BOOST_REQUIRE(temp_session.GenerateNonce(oracle_id, key, secp_pubkey, keyagg_cache, pubnonce));

    OracleMusigNonceMsg msg;
    msg.epoch = epoch;
    msg.oracle_id = oracle_id;
    msg.pubnonce.resize(66);
    BOOST_REQUIRE(secp256k1_musig_pubnonce_serialize(ctx, msg.pubnonce.data(), &pubnonce));
    secp256k1_context_destroy(ctx);

    BOOST_REQUIRE(msg.Sign(key));
    return msg;
}

// Regression test for the "MuSig2 v0x02 fallback every block" bug.
//
// Before this fix, OracleSigningOrchestrator only created a session
// for epoch N when the first block of that epoch connected. Block
// template assembly (bundle_manager.cpp AddOracleBundleToBlock) runs
// BEFORE the block it is building connects, so when the miner
// templated block N*epoch_length and queried the orchestrator for
// epoch N's completed session, no session existed — and the miner
// fell through to the v0x02 individual-signature fallback. Even once
// the session was eventually created, the MuSig2 ceremony needs ~3
// block ticks (nonce, partial-sig, aggregate) to reach COMPLETE, so
// the first few blocks of every epoch also fell back.
//
// Fix: in the tail of epoch N (last K blocks), the orchestrator also
// creates/ticks a session for epoch N+1 so the ceremony has time to
// reach COMPLETE before block (N+1)*epoch_length is templated.
BOOST_AUTO_TEST_CASE(prestart_next_epoch_session_at_boundary)
{
    OracleSigningOrchestrator orch;

    const int32_t epoch_length = Params().GetConsensus().nDDOracleEpochBlocks;
    BOOST_REQUIRE_GT(epoch_length, 0);

    // Pick a height 3 blocks before the next-epoch boundary. With
    // regtest epoch_length=10 that's height 7, current_epoch=0,
    // next_epoch=1.
    const int32_t tail_height = epoch_length - 3;
    const int32_t current_epoch = tail_height / epoch_length;
    const int32_t next_epoch = current_epoch + 1;

    BOOST_REQUIRE(!orch.HasSession(current_epoch));
    BOOST_REQUIRE(!orch.HasSession(next_epoch));

    std::shared_ptr<const CBlock> empty_block;
    orch.OnBlockConnected(empty_block, tail_height);

    // Current epoch's session always exists (pre-existing behavior).
    BOOST_CHECK(orch.HasSession(current_epoch));
    // Next epoch's session must also exist — this is what the fix
    // introduces. Without pre-start this would be false.
    BOOST_CHECK(orch.HasSession(next_epoch));
}

// Outside the pre-start window we should NOT create the next-epoch
// session — premature creation during the middle of an epoch wastes
// memory and noise.
BOOST_AUTO_TEST_CASE(no_prestart_mid_epoch)
{
    OracleSigningOrchestrator orch;

    const int32_t epoch_length = Params().GetConsensus().nDDOracleEpochBlocks;
    BOOST_REQUIRE_GT(epoch_length, 0);

    // Mid-epoch: far from the boundary, no pre-start expected.
    // Use height 1 (position 1 within epoch 0) to be unambiguously
    // outside the pre-start window on any epoch_length (regtest=10,
    // testnet=50, mainnet=100+).
    const int32_t mid_height = 1;
    BOOST_REQUIRE_GT(epoch_length, mid_height + 5);
    const int32_t current_epoch = mid_height / epoch_length;
    const int32_t next_epoch = current_epoch + 1;

    std::shared_ptr<const CBlock> empty_block;
    orch.OnBlockConnected(empty_block, mid_height);

    BOOST_CHECK(orch.HasSession(current_epoch));
    BOOST_CHECK(!orch.HasSession(next_epoch));
}

BOOST_AUTO_TEST_CASE(remote_nonce_lazy_session_accepts_first_nonce)
{
    OracleSigningOrchestrator orch;
    const int32_t epoch = 42;
    const uint8_t oracle_id = 1;

    BOOST_REQUIRE(!orch.HasSession(epoch));

    OracleMusigNonceMsg msg = MakeSignedMusigNonceMsg(epoch, oracle_id);
    orch.IngestRemoteNonce(msg);

    BOOST_REQUIRE(orch.HasSession(epoch));
    MuSig2SigningSession* session = orch.GetOrCreateSigningSession(epoch, epoch * 10);
    BOOST_REQUIRE(session != nullptr);
    BOOST_CHECK_EQUAL(session->GetNonceCount(), 1U);
    BOOST_CHECK(session->GetState() == MuSig2SessionState::NONCES_COLLECTING ||
                session->GetState() == MuSig2SessionState::NONCES_COMPLETE);
}

BOOST_AUTO_TEST_SUITE_END()

struct MainParamsMuSig2Setup : public BasicTestingSetup {
    MainParamsMuSig2Setup() : BasicTestingSetup(ChainType::MAIN) {}
};

BOOST_FIXTURE_TEST_SUITE(musig2_signing_orchestration_mainnet_tests, MainParamsMuSig2Setup)

BOOST_AUTO_TEST_CASE(remote_nonce_lazy_session_timeout_uses_chain_epoch_length)
{
    const int32_t epoch_length = Params().GetConsensus().nDDOracleEpochBlocks;
    BOOST_REQUIRE_GT(epoch_length, 0);
    BOOST_REQUIRE_NE(epoch_length, 50);

    OracleSigningOrchestrator orch;
    const int32_t epoch = 3;
    const int32_t epoch_start_height = epoch * epoch_length;

    OracleMusigNonceMsg msg = musig2_signing_orchestration_tests::MakeSignedMusigNonceMsg(epoch, 1);
    orch.IngestRemoteNonce(msg);

    MuSig2SigningSession* session = orch.GetOrCreateSigningSession(epoch, epoch_start_height);
    BOOST_REQUIRE(session != nullptr);
    BOOST_REQUIRE_EQUAL(session->GetNonceCount(), 1U);

    std::shared_ptr<const CBlock> empty_block;
    orch.OnBlockConnected(empty_block, epoch_start_height);

    session = orch.GetOrCreateSigningSession(epoch, epoch_start_height);
    BOOST_REQUIRE(session != nullptr);
    BOOST_CHECK_MESSAGE(session->GetState() != MuSig2SessionState::FAILED,
        "lazy-created MuSig2 session must use the active chain epoch length for timeout binding");
}

BOOST_AUTO_TEST_SUITE_END()
