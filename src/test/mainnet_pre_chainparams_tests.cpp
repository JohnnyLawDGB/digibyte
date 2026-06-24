// Copyright (c) 2014-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <consensus/params.h>
#include <test/util/setup_common.h>
#include <versionbits.h>

#include <boost/test/unit_test.hpp>

#include <memory>
#include <vector>

namespace {
constexpr int MAINNET_PRE_PORT{12046};
constexpr int MAINNET_PRE_RPC_PORT{14046};
constexpr int MAINNET_PRE_ONION_PORT{14146};
constexpr int MAINNET_PRE_BIP9_WINDOW{100};
constexpr int MAINNET_PRE_BIP9_THRESHOLD{70};
constexpr int MAINNET_PRE_DIGIDOLLAR_HEIGHT{600};
constexpr int64_t MAINNET_PRE_DIGIDOLLAR_START{1389388394};
constexpr int64_t MAINNET_PRE_DIGIDOLLAR_TIMEOUT{1830297600};
constexpr int32_t MAINNET_PRE_SIGNAL_BIT{int32_t{1} << 23};
constexpr int32_t MAINNET_PRE_SIGNAL_VERSION{VERSIONBITS_TOP_BITS | MAINNET_PRE_SIGNAL_BIT};

const CBlockIndex* MineBlock(std::vector<std::unique_ptr<CBlockIndex>>& chain, int32_t time, int32_t version)
{
    auto index{std::make_unique<CBlockIndex>()};
    index->nHeight = static_cast<int>(chain.size());
    index->pprev = chain.empty() ? nullptr : chain.back().get();
    index->nTime = time;
    index->nVersion = version;
    index->BuildSkip();
    chain.push_back(std::move(index));
    return chain.back().get();
}

const CBlockIndex* MineToTipHeight(
    std::vector<std::unique_ptr<CBlockIndex>>& chain,
    int height,
    int32_t time,
    int32_t version)
{
    const CBlockIndex* tip{chain.empty() ? nullptr : chain.back().get()};
    while (chain.empty() || chain.back()->nHeight < height) {
        tip = MineBlock(chain, time, version);
    }
    return tip;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(mainnet_pre_chainparams_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(mainnet_pre_keeps_mainnet_identity_but_uses_pre_storage_and_ports)
{
    const auto main_params{CChainParams::Main()};
    const auto base_params{CreateBaseChainParams(ChainType::MAIN)};

    BOOST_CHECK_EQUAL(main_params->GenesisBlock().GetHash().ToString(),
                      "7497ea1b465eb39f1c8f507bc877078fe016d6fcb6dfad3a64c98dcc6e1e8496");
    BOOST_CHECK_EQUAL(main_params->GetConsensus().hashGenesisBlock, main_params->GenesisBlock().GetHash());
    BOOST_CHECK_EQUAL(main_params->MessageStart()[0], 0xfa);
    BOOST_CHECK_EQUAL(main_params->MessageStart()[1], 0xc3);
    BOOST_CHECK_EQUAL(main_params->MessageStart()[2], 0xb6);
    BOOST_CHECK_EQUAL(main_params->MessageStart()[3], 0xda);

    BOOST_CHECK_EQUAL(main_params->GetDefaultPort(), MAINNET_PRE_PORT);
    BOOST_CHECK_EQUAL(base_params->RPCPort(), MAINNET_PRE_RPC_PORT);
    BOOST_CHECK_EQUAL(base_params->OnionServiceTargetPort(), MAINNET_PRE_ONION_PORT);
    BOOST_CHECK_EQUAL(base_params->DataDir(), "mainnet-pre");
}

BOOST_AUTO_TEST_CASE(mainnet_pre_public_bootstrap_sources_are_disabled)
{
    const auto main_params{CChainParams::Main()};

    BOOST_CHECK(main_params->DNSSeeds().empty());
    BOOST_CHECK(main_params->FixedSeeds().empty());
    BOOST_REQUIRE_EQUAL(main_params->Checkpoints().mapCheckpoints.size(), 1U);
    BOOST_CHECK_EQUAL(main_params->Checkpoints().mapCheckpoints.begin()->first, 0);
    BOOST_CHECK_EQUAL(main_params->Checkpoints().mapCheckpoints.begin()->second,
                      main_params->GenesisBlock().GetHash());
    BOOST_CHECK(!main_params->AssumeutxoForHeight(21700000).has_value());
    BOOST_CHECK_EQUAL(main_params->GetConsensus().defaultAssumeValid, uint256{});
    BOOST_CHECK_EQUAL(main_params->TxData().nTxCount, 1);
    BOOST_CHECK_EQUAL(main_params->TxData().dTxRate, 0.0);
}

BOOST_AUTO_TEST_CASE(mainnet_pre_compresses_historical_fork_schedule_before_digidollar)
{
    const auto main_params{CChainParams::Main()};
    const auto& consensus{main_params->GetConsensus()};

    BOOST_CHECK_EQUAL(consensus.BIP34Height, 1);
    BOOST_CHECK_EQUAL(consensus.BIP65Height, 1);
    BOOST_CHECK_EQUAL(consensus.BIP66Height, 1);
    BOOST_CHECK_EQUAL(consensus.CSVHeight, 1);
    BOOST_CHECK_EQUAL(consensus.SegwitHeight, 0);

    BOOST_CHECK_EQUAL(consensus.nDiffChangeTarget, 67);
    BOOST_CHECK_EQUAL(consensus.patchBlockRewardDuration, 10);
    BOOST_CHECK_EQUAL(consensus.patchBlockRewardDuration2, 80);
    BOOST_CHECK_EQUAL(consensus.multiAlgoDiffChangeTarget, 100);
    BOOST_CHECK_EQUAL(consensus.alwaysUpdateDiffChangeTarget, 200);
    BOOST_CHECK_EQUAL(consensus.workComputationChangeTarget, 400);
    BOOST_CHECK_EQUAL(consensus.ReserveAlgoBitsHeight, 450);
    BOOST_CHECK_EQUAL(consensus.algoSwapChangeTarget, 490);
    BOOST_CHECK_EQUAL(consensus.OdoHeight, 500);
    BOOST_CHECK_EQUAL(consensus.nOdoShapechangeInterval, 24 * 60 * 60);

    const auto& taproot{consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT]};
    BOOST_CHECK_EQUAL(taproot.nStartTime, Consensus::BIP9Deployment::ALWAYS_ACTIVE);
    BOOST_CHECK_EQUAL(taproot.nTimeout, Consensus::BIP9Deployment::NO_TIMEOUT);
    BOOST_CHECK_EQUAL(taproot.min_activation_height, 0);
}

BOOST_AUTO_TEST_CASE(mainnet_pre_digidollar_oracle_and_musig2_activate_together_at_600)
{
    const auto main_params{CChainParams::Main()};
    const auto& consensus{main_params->GetConsensus()};
    const auto& deployment{consensus.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR]};

    BOOST_CHECK_EQUAL(consensus.nMinerConfirmationWindow, MAINNET_PRE_BIP9_WINDOW);
    BOOST_CHECK_EQUAL(consensus.nRuleChangeActivationThreshold, MAINNET_PRE_BIP9_THRESHOLD);
    BOOST_CHECK_EQUAL(deployment.bit, 23);
    BOOST_CHECK_EQUAL(deployment.nStartTime, MAINNET_PRE_DIGIDOLLAR_START);
    BOOST_CHECK_EQUAL(deployment.nTimeout, MAINNET_PRE_DIGIDOLLAR_TIMEOUT);
    BOOST_CHECK_EQUAL(deployment.min_activation_height, MAINNET_PRE_DIGIDOLLAR_HEIGHT);
    BOOST_CHECK_EQUAL(consensus.nDDActivationHeight, MAINNET_PRE_DIGIDOLLAR_HEIGHT);
    BOOST_CHECK_EQUAL(consensus.nOracleActivationHeight, MAINNET_PRE_DIGIDOLLAR_HEIGHT);
    BOOST_CHECK_EQUAL(consensus.nDigiDollarMuSig2Height, MAINNET_PRE_DIGIDOLLAR_HEIGHT);
    BOOST_CHECK_EQUAL(main_params->GetDigiDollarParams().minMintAmountActivationHeight,
                      MAINNET_PRE_DIGIDOLLAR_HEIGHT);
}

BOOST_AUTO_TEST_CASE(mainnet_pre_oracle_roster_uses_pre_port)
{
    const auto main_params{CChainParams::Main()};
    const auto& consensus{main_params->GetConsensus()};
    const auto& oracle_nodes{main_params->GetOracleNodes()};

    BOOST_REQUIRE(Consensus::ValidateOracleConfiguration(consensus));
    BOOST_REQUIRE(main_params->ValidateOracleNodeAlignment());
    BOOST_CHECK_EQUAL(consensus.nOraclePubkeyCount, 35);
    BOOST_CHECK_EQUAL(consensus.nOracleConsensusRequired, 7);
    BOOST_REQUIRE_EQUAL(oracle_nodes.size(), 35U);

    for (const auto& oracle : oracle_nodes) {
        BOOST_CHECK_MESSAGE(oracle.endpoint.size() >= 6 &&
                                oracle.endpoint.rfind(":12046") == oracle.endpoint.size() - 6,
                            "mainnet PRE oracle slot " << oracle.id
                            << " must use port 12046, got " << oracle.endpoint);
        BOOST_CHECK_MESSAGE(oracle.endpoint.find(":12024") == std::string::npos,
                            "mainnet PRE oracle slot " << oracle.id
                            << " still references public mainnet port: " << oracle.endpoint);
    }
}

BOOST_AUTO_TEST_CASE(mainnet_pre_digidollar_bip9_uses_100_block_window_until_height_600)
{
    const auto main_params{CChainParams::Main()};
    const auto& consensus{main_params->GetConsensus()};
    VersionBitsCache cache;
    std::vector<std::unique_ptr<CBlockIndex>> chain;
    const int32_t time{static_cast<int32_t>(MAINNET_PRE_DIGIDOLLAR_START + 600)};

    BOOST_CHECK_EQUAL(cache.State(nullptr, consensus, Consensus::DEPLOYMENT_DIGIDOLLAR),
                      ThresholdState::DEFINED);

    const CBlockIndex* tip{MineToTipHeight(chain, 99, time, VERSIONBITS_LAST_OLD_BLOCK_VERSION)};
    BOOST_CHECK_EQUAL(cache.State(tip, consensus, Consensus::DEPLOYMENT_DIGIDOLLAR),
                      ThresholdState::STARTED);
    BOOST_CHECK((cache.ComputeBlockVersion(tip, consensus, ALGO_SCRYPT) & MAINNET_PRE_SIGNAL_BIT) != 0);

    tip = MineToTipHeight(chain, 199, time, MAINNET_PRE_SIGNAL_VERSION);
    BOOST_CHECK_EQUAL(cache.State(tip, consensus, Consensus::DEPLOYMENT_DIGIDOLLAR),
                      ThresholdState::LOCKED_IN);
    BOOST_CHECK((cache.ComputeBlockVersion(tip, consensus, ALGO_SCRYPT) & MAINNET_PRE_SIGNAL_BIT) != 0);

    tip = MineToTipHeight(chain, 598, time, MAINNET_PRE_SIGNAL_VERSION);
    BOOST_CHECK_EQUAL(cache.State(tip, consensus, Consensus::DEPLOYMENT_DIGIDOLLAR),
                      ThresholdState::LOCKED_IN);

    tip = MineToTipHeight(chain, 599, time, MAINNET_PRE_SIGNAL_VERSION);
    BOOST_CHECK_EQUAL(cache.State(tip, consensus, Consensus::DEPLOYMENT_DIGIDOLLAR),
                      ThresholdState::ACTIVE);
    BOOST_CHECK_EQUAL(cache.ComputeBlockVersion(tip, consensus, ALGO_SCRYPT) & MAINNET_PRE_SIGNAL_BIT, 0);
}

BOOST_AUTO_TEST_CASE(mainnet_pre_does_not_change_testnet26_or_regtest_activation)
{
    const auto test_params{CChainParams::TestNet()};
    const auto regtest_params{CChainParams::RegTest({})};

    BOOST_CHECK_EQUAL(test_params->GetDefaultPort(), 12033);
    BOOST_CHECK_EQUAL(test_params->GetConsensus().nMinerConfirmationWindow, 200);
    BOOST_CHECK_EQUAL(test_params->GetConsensus().nRuleChangeActivationThreshold, 140);
    BOOST_CHECK_EQUAL(test_params->GetConsensus().nDDActivationHeight, 600);
    BOOST_CHECK_EQUAL(test_params->GetConsensus().nDigiDollarMuSig2Height, 600);

    BOOST_CHECK_EQUAL(regtest_params->GetConsensus().nDDActivationHeight, 650);
    BOOST_CHECK_EQUAL(regtest_params->GetConsensus().nDigiDollarMuSig2Height, 0);
    BOOST_CHECK_EQUAL(
        regtest_params->GetConsensus().vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR].nStartTime,
        Consensus::BIP9Deployment::ALWAYS_ACTIVE);
}

BOOST_AUTO_TEST_SUITE_END()
