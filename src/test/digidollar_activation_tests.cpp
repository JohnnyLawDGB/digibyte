// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <consensus/params.h>
#include <digidollar/digidollar.h>
#include <kernel/chainparams.h>
#include <test/util/setup_common.h>
#include <util/time.h>

#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

/**
 * DigiDollar activation tests (post-burial).
 *
 * DigiDollar is a buried deployment (BIP90): activation is a hardcoded
 * chainparams height (Consensus::Params::DigiDollarHeight, reported by
 * DeploymentHeight(DEPLOYMENT_DIGIDOLLAR)) rather than BIP9 versionbits
 * signaling. The historical BIP9 state-machine / threshold / rollback /
 * timeout cases were removed with the burial; generic BIP9 mechanics remain
 * covered by versionbits_tests.cpp via DEPLOYMENT_TESTDUMMY.
 *
 * This suite pins the buried boundary contract of
 * DigiDollar::IsDigiDollarEnabled(pindexPrev, params): the block being judged
 * is pindexPrev->nHeight + 1 (0 for a null prev), and DigiDollar is enabled
 * iff that height is >= the buried deployment height.
 */

namespace {
    // Buried activation boundary used by the boundary tests below.
    constexpr int TEST_ACTIVATION_HEIGHT = 432;

    // Static storage for test blocks - ensures memory management
    std::vector<std::unique_ptr<CBlockIndex>> g_test_blocks;

    // Constants for better readability and maintainability
    constexpr uint32_t DEFAULT_REGTEST_BITS = 0x207fffff;

    /**
     * Create a synthetic block index for buried-boundary testing.
     *
     * @param pprev Previous block index (nullptr for genesis)
     * @param nTime Block timestamp
     * @return Pointer to created block index (managed by static storage)
     */
    CBlockIndex* CreateTestBlock(CBlockIndex* pprev, int64_t nTime)
    {
        auto pindex = std::make_unique<CBlockIndex>();

        pindex->pprev = pprev;
        pindex->nVersion = 0x20000000;
        pindex->nTime = static_cast<uint32_t>(nTime);
        pindex->nHeight = pprev ? pprev->nHeight + 1 : 0;
        pindex->nBits = DEFAULT_REGTEST_BITS;
        pindex->BuildSkip();

        CBlockIndex* result = pindex.get();
        g_test_blocks.push_back(std::move(pindex));
        return result;
    }

    /**
     * Build a fresh synthetic chain of `count` blocks (heights 0..count-1)
     * and return every index in height order.
     */
    std::vector<CBlockIndex*> BuildChain(int count)
    {
        std::vector<CBlockIndex*> chain;
        chain.reserve(count);
        CBlockIndex* tip = nullptr;
        const int64_t base_time = GetTime() - static_cast<int64_t>(count) * 15;
        for (int h = 0; h < count; ++h) {
            tip = CreateTestBlock(tip, base_time + h * 15);
            chain.push_back(tip);
        }
        return chain;
    }
} // namespace

BOOST_FIXTURE_TEST_SUITE(digidollar_activation_tests, RegTestingSetup)

/**
 * Pre/post activation behaviour across the buried boundary.
 *
 * -digidollaractivationheight=N retargets the buried deployment height AND
 * the static DD/oracle/MuSig2 gates, so DigiDollar activates at exactly N:
 * the first active block is N, judged with pindexPrev->nHeight + 1 == N.
 */
BOOST_AUTO_TEST_CASE(test_pre_post_activation_behavior)
{
    CChainParams::RegTestOptions opts;
    opts.digidollar_activation_height = TEST_ACTIVATION_HEIGHT;
    const auto chainparams = CChainParams::RegTest(opts);
    const Consensus::Params& params = chainparams->GetConsensus();
    BOOST_REQUIRE_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR),
                        TEST_ACTIVATION_HEIGHT);
    BOOST_REQUIRE_EQUAL(params.nDDActivationHeight, TEST_ACTIVATION_HEIGHT);
    BOOST_REQUIRE_EQUAL(params.nOracleActivationHeight, TEST_ACTIVATION_HEIGHT);
    BOOST_REQUIRE_EQUAL(params.nDigiDollarMuSig2Height, TEST_ACTIVATION_HEIGHT);

    // A null prev judges block 0: below the boundary, disabled.
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(nullptr, params));

    // Walk a synthetic chain across the boundary: the tip at height h judges
    // block h + 1, so the predicate flips exactly at tip height N - 1.
    const std::vector<CBlockIndex*> chain = BuildChain(TEST_ACTIVATION_HEIGHT + 3);
    for (CBlockIndex* idx : chain) {
        BOOST_CHECK_EQUAL(DigiDollar::IsDigiDollarEnabled(idx, params),
                          idx->nHeight + 1 >= TEST_ACTIVATION_HEIGHT);
    }

    // Explicit off-by-one pins on the boundary.
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(chain[TEST_ACTIVATION_HEIGHT - 2], params)); // judges block N-1
    BOOST_CHECK(DigiDollar::IsDigiDollarEnabled(chain[TEST_ACTIVATION_HEIGHT - 1], params));  // judges block N
    BOOST_CHECK(DigiDollar::IsDigiDollarEnabled(chain.back(), params));                       // beyond N
}

/**
 * Default regtest buries DigiDollar at height 0: the deployment is enabled
 * from genesis (block 0, judged by a null prev), while the static
 * nDDActivationHeight oracle/P2P height gate stays at 650.
 */
BOOST_AUTO_TEST_CASE(test_default_regtest_buried_at_height_zero)
{
    const Consensus::Params& params = Params().GetConsensus();
    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR), 0);
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 650);
    BOOST_CHECK(DigiDollar::IsDigiDollarEnabled(nullptr, params));
}

/**
 * -testactivationheight=digidollar@H moves ONLY the buried deployment
 * height; the static DD/oracle gates keep their regtest defaults, and the
 * effective MuSig2 boundary follows min(nDDActivationHeight, DigiDollarHeight).
 * The enabled predicate follows the buried height, not the static gates.
 */
BOOST_AUTO_TEST_CASE(test_activation_heights_option_moves_only_buried_height)
{
    CChainParams::RegTestOptions opts;
    opts.activation_heights[Consensus::DEPLOYMENT_DIGIDOLLAR] = TEST_ACTIVATION_HEIGHT;
    const auto chainparams = CChainParams::RegTest(opts);
    const Consensus::Params& params = chainparams->GetConsensus();

    BOOST_CHECK_EQUAL(params.DeploymentHeight(Consensus::DEPLOYMENT_DIGIDOLLAR),
                      TEST_ACTIVATION_HEIGHT);
    // Static gates stay at the regtest defaults.
    BOOST_CHECK_EQUAL(params.nDDActivationHeight, 650);
    BOOST_CHECK_EQUAL(params.nOracleActivationHeight, 650);
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, TEST_ACTIVATION_HEIGHT);

    // The predicate boundary tracks the buried height.
    const std::vector<CBlockIndex*> chain = BuildChain(TEST_ACTIVATION_HEIGHT + 1);
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(nullptr, params));
    BOOST_CHECK(!DigiDollar::IsDigiDollarEnabled(chain[TEST_ACTIVATION_HEIGHT - 2], params)); // judges block N-1
    BOOST_CHECK(DigiDollar::IsDigiDollarEnabled(chain[TEST_ACTIVATION_HEIGHT - 1], params));  // judges block N
}

BOOST_AUTO_TEST_SUITE_END()
