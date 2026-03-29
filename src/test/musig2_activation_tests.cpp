// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <test/util/setup_common.h>

#include <limits>

namespace {

static bool IsBundleVersionAccepted(uint8_t bundle_version, int32_t block_height, const Consensus::Params& params)
{
    if (block_height >= params.nDigiDollarPhase3Height) {
        return bundle_version == 3;
    }
    return bundle_version == 1 || bundle_version == 2;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(musig2_activation_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(phase3_activation_heights_are_configured)
{
    SelectParams(ChainType::MAIN);
    BOOST_CHECK_EQUAL(Params().GetConsensus().nDigiDollarPhase3Height, std::numeric_limits<int>::max());

    SelectParams(ChainType::TESTNET);
    BOOST_CHECK_EQUAL(Params().GetConsensus().nDigiDollarPhase3Height, 2000);

    SelectParams(ChainType::REGTEST);
    BOOST_CHECK_EQUAL(Params().GetConsensus().nDigiDollarPhase3Height, 200);
}

BOOST_AUTO_TEST_CASE(pre_phase3_accepts_legacy_and_rejects_v03)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    const int32_t h = params.nDigiDollarPhase3Height - 1;

    BOOST_CHECK(IsBundleVersionAccepted(1, h, params));
    BOOST_CHECK(IsBundleVersionAccepted(2, h, params));
    BOOST_CHECK(!IsBundleVersionAccepted(3, h, params));
}

BOOST_AUTO_TEST_CASE(at_phase3_requires_v03)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    const int32_t h = params.nDigiDollarPhase3Height;

    BOOST_CHECK(!IsBundleVersionAccepted(1, h, params));
    BOOST_CHECK(!IsBundleVersionAccepted(2, h, params));
    BOOST_CHECK(IsBundleVersionAccepted(3, h, params));
}

BOOST_AUTO_TEST_CASE(post_phase3_requires_v03)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    const int32_t h = params.nDigiDollarPhase3Height + 1;

    BOOST_CHECK(!IsBundleVersionAccepted(1, h, params));
    BOOST_CHECK(!IsBundleVersionAccepted(2, h, params));
    BOOST_CHECK(IsBundleVersionAccepted(3, h, params));
}

BOOST_AUTO_TEST_CASE(phase3_boundary_off_by_one)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    const int32_t n = params.nDigiDollarPhase3Height;

    BOOST_CHECK(!params.IsPhaseThreeActive(n - 1));
    BOOST_CHECK(params.IsPhaseThreeActive(n));

    BOOST_CHECK(!IsBundleVersionAccepted(3, n - 1, params));
    BOOST_CHECK(IsBundleVersionAccepted(3, n, params));
}

BOOST_AUTO_TEST_SUITE_END()
