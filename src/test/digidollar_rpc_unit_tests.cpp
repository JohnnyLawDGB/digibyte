// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/digidollar.h>
#include <digidollar/digidollar.h>
#include <rpc/client.h>
#include <rpc/digidollar.h>
#include <rpc/server.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <wallet/context.h>
#include <wallet/digidollarwallet.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tinyformat.h>
#include <vector>

BOOST_AUTO_TEST_SUITE(digidollar_rpc_unit_tests)

namespace {

struct DigiDollarRPCUnitSetup : public TestingSetup {
    DigiDollarRPCUnitSetup() : TestingSetup(ChainType::REGTEST) {}

    UniValue CallNodeRPC(const std::string& args)
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

    std::shared_ptr<wallet::CWallet> CreateWalletWithPositions()
    {
        auto wallet = std::make_shared<wallet::CWallet>(
            m_node.chain.get(), "wave1-rpc-wallet", wallet::CreateMockableWalletDatabase());
        wallet->LoadWallet();
        wallet->EnsureDDWallet();
        WITH_LOCK(wallet->cs_wallet, wallet->SetLastBlockProcessed(1000, uint256::ONE));

        DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
        BOOST_REQUIRE(dd_wallet != nullptr);
        dd_wallet->AddCollateralPosition(WalletCollateralPosition(
            uint256S("00000000000000000000000000000000000000000000000000000000dd100001"),
            50, COIN, 1, 1100));
        dd_wallet->AddCollateralPosition(WalletCollateralPosition(
            uint256S("00000000000000000000000000000000000000000000000000000000dd100002"),
            150, 2 * COIN, 1, 1100));
        dd_wallet->AddCollateralPosition(WalletCollateralPosition(
            uint256S("00000000000000000000000000000000000000000000000000000000dd100003"),
            10000, 300 * COIN, 9, 2000));

        return wallet;
    }
};

int LockDaysForTier(int tier)
{
    switch (tier) {
    case 5: return 730;
    case 6: return 1095;
    case 7: return 1825;
    case 8: return 2555;
    default: return 3650;
    }
}

int ConsensusRatioForTier(int tier)
{
    return DigiDollar::GetCollateralRatioForLockTime(
        DigiDollar::LockDaysToBlocks(LockDaysForTier(tier)), Params().GetDigiDollarParams());
}

} // namespace

BOOST_FIXTURE_TEST_CASE(wave1_list_positions_min_amount_filters_dd_cents_not_dgb_sats, DigiDollarRPCUnitSetup)
{
    wallet::WalletContext context;
    context.args = m_node.args;
    context.chain = m_node.chain.get();

    std::shared_ptr<wallet::CWallet> wallet = CreateWalletWithPositions();
    wallet::AddWallet(context, wallet);

    JSONRPCRequest request;
    request.context = &context;
    request.strMethod = "listdigidollarpositions";
    request.params = UniValue(UniValue::VARR);
    request.params.push_back(true);
    request.params.push_back(-1);
    request.params.push_back("1.00");

    UniValue result = listdigidollarpositions().HandleRequest(request);
    wallet::RemoveWallet(context, wallet, std::nullopt);

    BOOST_REQUIRE(result.isArray());
    BOOST_CHECK_EQUAL(result.size(), 2U);

    std::vector<CAmount> amounts;
    for (const UniValue& position : result.getValues()) {
        amounts.push_back(position["dd_minted"].getInt<int64_t>());
    }
    BOOST_CHECK(std::find(amounts.begin(), amounts.end(), 150) != amounts.end());
    BOOST_CHECK(std::find(amounts.begin(), amounts.end(), 10000) != amounts.end());
    BOOST_CHECK(std::find(amounts.begin(), amounts.end(), 50) == amounts.end());
}

BOOST_FIXTURE_TEST_CASE(wave1_estimatecollateral_matches_consensus_tiers_5_to_8, DigiDollarRPCUnitSetup)
{
    for (int tier = 5; tier <= 8; ++tier) {
        UniValue result = CallNodeRPC(strprintf("estimatecollateral 10000 %d 500000", tier));
        BOOST_REQUIRE(result.isObject());

        const int expected_ratio = ConsensusRatioForTier(tier);
        BOOST_CHECK_EQUAL(result["lock_tier"].getInt<int>(), tier);
        BOOST_CHECK_EQUAL(result["base_ratio"].getInt<int>(), expected_ratio);
        BOOST_CHECK_EQUAL(result["effective_ratio"].getInt<int>(), expected_ratio);
    }
}

BOOST_AUTO_TEST_SUITE_END()
