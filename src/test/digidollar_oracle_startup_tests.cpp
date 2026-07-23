// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Consensus-safety tests for the DigiDollar startup oracle price-cache scan
// (OracleBundleManager::LoadPricesFromChain / ShouldLoadStartupOraclePriceForBlock).
//
// The per-block startup gate was changed to evaluate the BIP9 DigiDollar
// activation predicate through the SHARED, memoized versionbits cache
// (chainman.m_versionbitscache) instead of allocating a throwaway
// VersionBitsCache on every one of the up-to ~172,800 iterations. That is a
// pure performance change: it MUST compute the identical activation boolean, so
// it can neither load a price from a block that would previously have been
// skipped (a bypass) nor skip a block that would previously have been loaded (a
// consensus divergence). These tests pin exactly that invariant.
//
// End-to-end loader correctness (a real MuSig2 v0x03 coinbase bundle -> price
// cache) is already covered by rh66_startup_oracle_price_loading_tests in
// rh61_coinbase_price_cache_poisoning_tests.cpp; the exhaustive BIP9 State()
// machine is covered by versionbits_tests.cpp. Here we prove only that the
// startup gate tracks that predicate exactly, on a real connected chain.

#include <chain.h>
#include <consensus/params.h>
#include <digidollar/digidollar.h>
#include <oracle/bundle_manager.h>
#include <sync.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(digidollar_oracle_startup_tests, TestChain100Setup)

// The two DigiDollar::IsDigiDollarEnabled overloads — the shared-cache one
// (const ChainstateManager&) the fix switches TO, and the throwaway-cache one
// (const Consensus::Params&) it switches AWAY from — must return the SAME
// activation boolean for every block on a real chain. Any divergence here would
// be a consensus divergence hiding behind a "performance" change.
BOOST_AUTO_TEST_CASE(startup_predicate_overloads_agree_on_real_chain)
{
    // Extend the pre-mined 100-block regtest chain so the scan spans a
    // substantial run of genuinely-connected blocks.
    mineBlocks(600);

    const Consensus::Params& consensus = m_node.chainman->GetConsensus();

    LOCK(cs_main);
    const CChain& chain = m_node.chainman->ActiveChain();
    BOOST_REQUIRE(chain.Height() > 200);

    for (int h = 0; h <= chain.Height(); ++h) {
        const CBlockIndex* index = chain[h];
        BOOST_REQUIRE(index != nullptr);
        const bool shared = DigiDollar::IsDigiDollarEnabled(index, *m_node.chainman);
        const bool throwaway = DigiDollar::IsDigiDollarEnabled(index, consensus);
        BOOST_CHECK_MESSAGE(shared == throwaway,
            "IsDigiDollarEnabled overloads diverged at height " << h
            << " (shared=" << shared << " throwaway=" << throwaway << ")");
    }
}

// The startup per-block gate must equal the BIP9 activation predicate applied to
// the block's parent — exactly the predicate ConnectBlock uses. Proving equality
// proves there is NO bypass (it never accepts a price from an un-activated block)
// and NO over-skip (it never drops an activated block's price).
BOOST_AUTO_TEST_CASE(should_load_startup_matches_bip9_predicate)
{
    mineBlocks(600);

    LOCK(cs_main);
    const CChain& chain = m_node.chainman->ActiveChain();
    BOOST_REQUIRE(chain.Height() > 200);

    for (int h = 1; h <= chain.Height(); ++h) {
        const CBlockIndex* index = chain[h];
        BOOST_REQUIRE(index != nullptr);
        BOOST_REQUIRE(index->pprev != nullptr);
        const bool gate = OracleBundleManager::ShouldLoadStartupOraclePriceForBlock(
            h, index, *m_node.chainman);
        const bool predicate = DigiDollar::IsDigiDollarEnabled(index->pprev, *m_node.chainman);
        BOOST_CHECK_MESSAGE(gate == predicate,
            "ShouldLoadStartupOraclePriceForBlock diverged from the BIP9 predicate at height "
            << h << " (gate=" << gate << " predicate=" << predicate << ")");
    }
}

BOOST_AUTO_TEST_SUITE_END()
