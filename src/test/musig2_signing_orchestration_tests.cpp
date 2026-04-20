// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <oracle/signing_orchestrator.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(musig2_signing_orchestration_tests, RegTestingSetup)

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

BOOST_AUTO_TEST_SUITE_END()
