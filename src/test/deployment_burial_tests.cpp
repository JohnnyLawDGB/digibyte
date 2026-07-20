// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Pins for the BIP90 burial of the TAPROOT, DIGIDOLLAR, and ALGOLOCK
// deployments (v9.26.5). The burial heights are the actual on-chain BIP9
// 'since' heights, verified against live getdeploymentinfo output on mainnet
// (tip 23,882,878) and a fully-synced testnet26 node:
//   mainnet:  taproot 21,168,000; digidollar 23,869,440; algolock 23,869,440
//   testnet:  taproot 0 (ALWAYS_ACTIVE); digidollar 600; algolock 0
//   regtest/signet: all 0 (ALWAYS_ACTIVE)
// The static nDDActivationHeight/nOracleActivationHeight floor gates are
// deliberately NOT moved by the burial; EarliestActivationFloor must keep its
// pre-burial value on every network.

#include <chain.h>
#include <chainparams.h>
#include <consensus/digidollar.h>
#include <consensus/params.h>
#include <deploymentinfo.h>
#include <deploymentstatus.h>
#include <digidollar/digidollar.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>

#include <boost/test/unit_test.hpp>

#include <limits>

BOOST_FIXTURE_TEST_SUITE(deployment_burial_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(mainnet_burial_heights)
{
    SelectParams(ChainType::MAIN);
    const auto& params = Params().GetConsensus();

    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_TAPROOT), 21168000);
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR), 23869440);
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_ALGOLOCK), 23869440);

    // Burial heights sit on 40,320-block confirmation-window boundaries
    // (BIP9 ACTIVE always begins at a period boundary).
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_TAPROOT) % params.nMinerConfirmationWindow, 0U);
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR) % params.nMinerConfirmationWindow, 0U);

    // The static DD/oracle floor gates keep their pre-burial values: they are
    // floors *below* the actual activation, and moving them would change
    // historical oracle-P2P / prune-floor behavior.
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 23627520);
    BOOST_CHECK_EQUAL(params.nOracleActivationHeight, 23627520);
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 23627520);
    BOOST_CHECK(params.nDDActivationHeight <= params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR));

    // EarliestActivationFloor is value-preserving: min(floor, burial) == floor.
    BOOST_CHECK_EQUAL(DigiDollar::EarliestActivationFloor(params), 23627520);

    // Warning floor covers the historical bit-2/23/0 signaling periods:
    // burial height + one confirmation window.
    BOOST_CHECK_EQUAL(params.MinBIP9WarningHeight,
                      params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR) + static_cast<int>(params.nMinerConfirmationWindow));

    // AlgoLock: the static Groestl backstop precedes the buried activation, so
    // the OR'd enforcement predicate is unchanged at every mainnet height.
    BOOST_CHECK(params.nGroestlDeactivationHeight <= params.DeploymentHeight(Consensus::DEPLOYMENT_ALGOLOCK));
    BOOST_CHECK_EQUAL(params.nGroestlDeactivationHeight, 23808000);
}

BOOST_AUTO_TEST_CASE(testnet_burial_heights)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();

    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_TAPROOT), 0);
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR), 600);
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_ALGOLOCK), 0);

    // On testnet26 the floor equals the burial height (lock-in happened in the
    // earliest possible window), so everything stays collapsed onto 600.
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 600);
    BOOST_CHECK_EQUAL(params.nOracleActivationHeight, 600);
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 600);
    BOOST_CHECK_EQUAL(DigiDollar::EarliestActivationFloor(params), 600);
    BOOST_CHECK_EQUAL(params.MinBIP9WarningHeight, 600 + static_cast<int>(params.nMinerConfirmationWindow));
}

BOOST_AUTO_TEST_CASE(regtest_burial_defaults)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();

    // Buried at 0 == the old BIP9 ALWAYS_ACTIVE: active from genesis.
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_TAPROOT), 0);
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR), 0);
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_ALGOLOCK), 0);

    // The intentional regtest asymmetry survives burial: deployment active
    // from genesis while the static P2P/oracle height gates stay at 650.
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 650);
    BOOST_CHECK_EQUAL(params.nOracleActivationHeight, 650);
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 0);
    BOOST_CHECK_EQUAL(DigiDollar::EarliestActivationFloor(params), 0);

    // Genesis edge: null pprev means "next block is height 0", which is
    // active for a buried height of 0 — matching old ALWAYS_ACTIVE behavior.
    BOOST_CHECK(DigiDollar::IsDigiDollarEnabled(nullptr, params));
}

BOOST_AUTO_TEST_CASE(regtest_digidollaractivationheight_knob_moves_everything)
{
    CChainParams::RegTestOptions opts;
    opts.digidollar_activation_height = 432;

    const auto chainparams = CChainParams::RegTest(opts);
    const auto& params = chainparams->GetConsensus();

    // The knob retargets the buried deployment height AND the static gates,
    // preserving the pre-burial "everything activates together" contract —
    // except activation now lands at exactly N instead of the next 144-block
    // BIP9 window boundary >= max(432, N).
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR), 432);
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 432);
    BOOST_CHECK_EQUAL(params.nOracleActivationHeight, 432);
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 432);
    BOOST_CHECK_EQUAL(DigiDollar::EarliestActivationFloor(params), 432);
}

BOOST_AUTO_TEST_CASE(regtest_testactivationheight_moves_only_deployment)
{
    CChainParams::RegTestOptions opts;
    opts.activation_heights[Consensus::BuriedDeployment::DEPLOYMENT_DIGIDOLLAR] = 999;

    const auto chainparams = CChainParams::RegTest(opts);
    const auto& params = chainparams->GetConsensus();

    // -testactivationheight=digidollar@H moves ONLY the buried deployment
    // height; the static gates keep their 650 defaults. This preserves the
    // "BIP9-inactive while height gates open" split that wave20 exercises.
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR), 999);
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 650);
    BOOST_CHECK_EQUAL(params.nOracleActivationHeight, 650);
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 650); // min(650, 999)
    BOOST_CHECK_EQUAL(DigiDollar::EarliestActivationFloor(params), 650); // min(650, 999)
}

BOOST_AUTO_TEST_CASE(regtest_knob_takes_precedence_over_testactivationheight)
{
    CChainParams::RegTestOptions opts;
    opts.activation_heights[Consensus::BuriedDeployment::DEPLOYMENT_DIGIDOLLAR] = 999;
    opts.digidollar_activation_height = 432;

    const auto chainparams = CChainParams::RegTest(opts);
    const auto& params = chainparams->GetConsensus();

    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR), 432);
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 432);
}

BOOST_AUTO_TEST_CASE(buried_gate_flips_exactly_at_height)
{
    CChainParams::RegTestOptions opts;
    opts.digidollar_activation_height = 432;
    const auto chainparams = CChainParams::RegTest(opts);
    const auto& params = chainparams->GetConsensus();

    // IsDigiDollarEnabled(pindexPrev) judges the block AFTER pindexPrev.
    CBlockIndex prev_431; // next block is 432 -> first active block
    prev_431.nHeight = 431;
    CBlockIndex prev_430; // next block is 431 -> last inactive block
    prev_430.nHeight = 430;

    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(&prev_430, params));
    BOOST_CHECK(DigiDollar::IsDigiDollarEnabled(&prev_431, params));

    // Buried DeploymentActiveAt agrees: block 431 inactive, block 432 active.
    // (The VersionBitsCache parameter is [[maybe_unused]] for buried
    // deployments — no cache is ever consulted.)
    VersionBitsCache dummy_cache;
    CBlockIndex block_432;
    block_432.nHeight = 432;
    CBlockIndex block_431;
    block_431.nHeight = 431;
    BOOST_CHECK(!DeploymentActiveAt(block_431, params, Consensus::DEPLOYMENT_DIGIDOLLAR, dummy_cache));
    BOOST_CHECK(DeploymentActiveAt(block_432, params, Consensus::DEPLOYMENT_DIGIDOLLAR, dummy_cache));
}

BOOST_AUTO_TEST_CASE(disabled_deployment_semantics)
{
    CChainParams::RegTestOptions opts;
    opts.activation_heights[Consensus::BuriedDeployment::DEPLOYMENT_DIGIDOLLAR] =
        std::numeric_limits<int>::max();

    const auto chainparams = CChainParams::RegTest(opts);
    const auto& params = chainparams->GetConsensus();

    // Height == int max is the buried representation of "disabled": the
    // deployment reports disabled and the activation floor collapses to 0
    // (no prune lock), matching the old NEVER_ACTIVE contract.
    BOOST_CHECK(!DeploymentEnabled(params, Consensus::DEPLOYMENT_DIGIDOLLAR));
    BOOST_CHECK_EQUAL(DigiDollar::EarliestActivationFloor(params), 0);

    CBlockIndex prev;
    prev.nHeight = 1000000;
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(&prev, params));
}

BOOST_AUTO_TEST_CASE(deployment_names_round_trip)
{
    BOOST_CHECK(Consensus::ValidDeployment(Consensus::DEPLOYMENT_ALGOLOCK));
    BOOST_CHECK_EQUAL(DeploymentName(Consensus::DEPLOYMENT_TAPROOT), "taproot");
    BOOST_CHECK_EQUAL(DeploymentName(Consensus::DEPLOYMENT_DIGIDOLLAR), "digidollar");
    BOOST_CHECK_EQUAL(DeploymentName(Consensus::DEPLOYMENT_ALGOLOCK), "algolock");

    BOOST_CHECK(GetBuriedDeployment("taproot") == Consensus::BuriedDeployment::DEPLOYMENT_TAPROOT);
    BOOST_CHECK(GetBuriedDeployment("digidollar") == Consensus::BuriedDeployment::DEPLOYMENT_DIGIDOLLAR);
    BOOST_CHECK(GetBuriedDeployment("algolock") == Consensus::BuriedDeployment::DEPLOYMENT_ALGOLOCK);
    BOOST_CHECK(!GetBuriedDeployment("digidollarx").has_value());
}

BOOST_AUTO_TEST_SUITE_END()
