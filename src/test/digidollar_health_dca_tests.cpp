// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/dca.h>
#include <consensus/digidollar.h>
#include <consensus/err.h>
#include <consensus/validation.h>
#include <consensus/volatility.h>
#include <digidollar/digidollar.h>
#include <digidollar/health.h>
#include <digidollar/txbuilder.h>
#include <digidollar/validation.h>
#include <key.h>
#include <rpc/client.h>
#include <rpc/server.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <util/string.h>

#include <utility>
#include <vector>

BOOST_AUTO_TEST_SUITE(digidollar_health_dca_tests)

namespace {

static constexpr int WAVE1_HEIGHT{1000};
static constexpr CAmount WAVE1_ORACLE_PRICE_MICRO_USD{500000};
static constexpr CAmount WAVE1_DD_AMOUNT{10000};
static constexpr int WAVE1_TEN_YEAR_DAYS{3650};
static constexpr uint32_t WAVE1_TEN_YEAR_TIER{9};

int ConsensusRatioForLockDays(int lock_days)
{
    return DigiDollar::GetCollateralRatioForLockTime(
        DigiDollar::LockDaysToBlocks(lock_days), Params().GetDigiDollarParams());
}

struct DigiDollarHealthDCASetup : public TestingSetup {
    DigiDollarHealthDCASetup() : TestingSetup(ChainType::REGTEST)
    {
        DigiDollar::SystemHealthMonitor::Initialize();
        ResetSharedState();
    }

    ~DigiDollarHealthDCASetup()
    {
        ResetSharedState();
        DigiDollar::SystemHealthMonitor::Shutdown();
    }

    void ResetSharedState()
    {
        DigiDollar::SystemHealthMonitor::ResetMetrics();
        DigiDollar::Volatility::VolatilityMonitor::ClearHistory();
        DigiDollar::ERR::EmergencyRedemptionRatio::ResetForTesting();
    }

    DigiDollar::SystemMetrics SeedCachedHealth120()
    {
        ResetSharedState();
        DigiDollar::SystemHealthMonitor::OnMintConnected(WAVE1_DD_AMOUNT, 240 * COIN);
        DigiDollar::SystemMetrics metrics = DigiDollar::SystemHealthMonitor::GetSystemMetrics();
        BOOST_REQUIRE_EQUAL(metrics.systemHealth, 120);
        return metrics;
    }

    DigiDollar::TxBuilderResult BuildBaseCollateralMint()
    {
        DigiDollar::MintTxBuilder builder(Params(), WAVE1_HEIGHT, WAVE1_ORACLE_PRICE_MICRO_USD);

        CKey owner_key;
        owner_key.MakeNewKey(true);

        DigiDollar::TxBuilderMintParams params;
        params.ddAmount = WAVE1_DD_AMOUNT;
        params.lockDays = WAVE1_TEN_YEAR_DAYS;
        params.lockTier = WAVE1_TEN_YEAR_TIER;
        params.ownerKey = owner_key;
        params.feeRate = 100000;
        for (uint32_t i = 0; i < 8; ++i) {
            params.utxos.emplace_back(uint256::ONE, i);
        }

        DigiDollar::TxBuilderResult result = builder.BuildMintTransaction(params);
        if (result.success) {
            bool kept_collateral_output = false;
            std::vector<CTxOut> filtered_outputs;
            for (const CTxOut& output : result.tx.vout) {
                if (output.nValue > 0) {
                    if (kept_collateral_output) continue;
                    kept_collateral_output = true;
                }
                filtered_outputs.push_back(output);
            }
            result.tx.vout = std::move(filtered_outputs);
        }
        return result;
    }

    UniValue CallRPC(const std::string& args)
    {
        std::vector<std::string> v_args{SplitString(args, ' ')};
        const std::string method = v_args.front();
        v_args.erase(v_args.begin());

        JSONRPCRequest request;
        request.context = &m_node;
        request.strMethod = method;
        request.params = RPCConvertValues(method, v_args);
        if (RPCIsInWarmup(nullptr)) SetRPCWarmupFinished();
        return tableRPC.execute(request);
    }
};

} // namespace

BOOST_FIXTURE_TEST_CASE(wave1_stale_cached_health_cannot_allow_base_collateral_mint, DigiDollarHealthDCASetup)
{
    const DigiDollar::SystemMetrics current_metrics = SeedCachedHealth120();

    DigiDollar::TxBuilderResult build = BuildBaseCollateralMint();
    BOOST_REQUIRE_MESSAGE(build.success, build.error);

    DigiDollar::ValidationContext stale_healthy_ctx(
        WAVE1_HEIGHT, WAVE1_ORACLE_PRICE_MICRO_USD, 30000, Params());
    TxValidationState state;
    const bool accepted = DigiDollar::ValidateMintTransaction(CTransaction(build.tx), stale_healthy_ctx, state);

    BOOST_CHECK_MESSAGE(!accepted,
        "base-collateral mint accepted using stale healthy context while current cached health is "
        << current_metrics.systemHealth << "%; reject_reason=" << state.GetRejectReason());
    if (!accepted) {
        const std::string reason = state.GetRejectReason();
        BOOST_CHECK_MESSAGE(reason == "insufficient-collateral" || reason == "bad-collateral-ratio",
            "expected health-source collateral rejection, observed reason=" << reason);
    }
}

BOOST_FIXTURE_TEST_CASE(wave1_missing_post_activation_health_rejects_not_default_healthy, DigiDollarHealthDCASetup)
{
    ResetSharedState();
    DigiDollar::SystemHealthMonitor::OnMintConnected(WAVE1_DD_AMOUNT, 240 * COIN);

    const int health = DigiDollar::DCA::DynamicCollateralAdjustment::GetCurrentSystemHealth();
    BOOST_CHECK_MESSAGE(health < 0,
        "missing oracle/health data should fail closed after DD supply exists, observed health="
        << health);
}

BOOST_FIXTURE_TEST_CASE(wave1_dca_err_rpc_quote_and_txbuilder_share_health_source, DigiDollarHealthDCASetup)
{
    const DigiDollar::SystemMetrics metrics = SeedCachedHealth120();
    const int dca_health = DigiDollar::DCA::DynamicCollateralAdjustment::GetCurrentSystemHealth();
    BOOST_REQUIRE_EQUAL(dca_health, metrics.systemHealth);

    const int base_ratio = ConsensusRatioForLockDays(WAVE1_TEN_YEAR_DAYS);
    const int expected_ratio = DigiDollar::DCA::DynamicCollateralAdjustment::ApplyDCA(base_ratio, dca_health);

    BOOST_CHECK(!DigiDollar::ERR::EmergencyRedemptionRatio::ShouldBlockMinting(WAVE1_ORACLE_PRICE_MICRO_USD));

    UniValue rpc_quote = CallRPC("calculatecollateralrequirement 10000 3650 500000");
    BOOST_REQUIRE(rpc_quote.isObject());
    BOOST_CHECK_EQUAL(rpc_quote["system_health"].getInt<int>(), dca_health);
    BOOST_CHECK_EQUAL(rpc_quote["effective_ratio"].getInt<int>(), expected_ratio);

    DigiDollar::ValidationContext current_ctx(WAVE1_HEIGHT, WAVE1_ORACLE_PRICE_MICRO_USD, dca_health, Params());
    const CAmount consensus_required = DigiDollar::CalculateRequiredCollateral(
        WAVE1_DD_AMOUNT, DigiDollar::LockDaysToBlocks(WAVE1_TEN_YEAR_DAYS), current_ctx);

    DigiDollar::MintTxBuilder builder(Params(), WAVE1_HEIGHT, WAVE1_ORACLE_PRICE_MICRO_USD);
    const CAmount txbuilder_required = builder.CalculateRequiredCollateral(WAVE1_DD_AMOUNT, WAVE1_TEN_YEAR_DAYS);

    BOOST_CHECK_MESSAGE(txbuilder_required >= consensus_required,
        "txbuilder collateral quote used a different health source: consensus_required="
        << consensus_required << " txbuilder_required=" << txbuilder_required
        << " health=" << dca_health);
}

BOOST_AUTO_TEST_SUITE_END()
