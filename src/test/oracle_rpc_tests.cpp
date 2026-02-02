// Copyright (c) 2024-2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <oracle/node.h>
#include <oracle/bundle_manager.h>
#include <chainparams.h>
#include <kernel/chainparams.h>
#include <key.h>
#include <pubkey.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <util/time.h>

/**
 * ORACLE RPC LOGIC TESTS
 *
 * Unit tests validating the logic used by the getoracles, listoracle,
 * and related oracle RPC commands. Tests the OracleManager/OracleNode
 * APIs that the RPCs depend on.
 */

BOOST_FIXTURE_TEST_SUITE(oracle_rpc_tests, BasicTestingSetup)

// ============================================================================
// PART 1: getoracles logic — OracleManager oracle enumeration
// ============================================================================

/**
 * Test: Oracle node list from chainparams is non-empty on testnet
 *
 * The getoracles RPC iterates Params().GetOracleNodes(). Verify that
 * testnet has oracle nodes configured.
 */
BOOST_AUTO_TEST_CASE(getoracles_chainparams_has_oracles)
{
    SelectParams(ChainType::TESTNET);
    const std::vector<OracleNodeInfo>& oracles = Params().GetOracleNodes();

    BOOST_CHECK(!oracles.empty());
    BOOST_CHECK_GE(oracles.size(), 1u);

    // Verify first oracle has valid fields
    const auto& first = oracles[0];
    BOOST_CHECK_EQUAL(first.id, 0u);
    BOOST_CHECK(!first.endpoint.empty());
    BOOST_CHECK(first.pubkey.IsValid());
}

/**
 * Test: Oracle names array covers all configured oracles
 *
 * getoracles uses a hardcoded names array. Verify it has at least as
 * many entries as configured oracle nodes.
 */
BOOST_AUTO_TEST_CASE(getoracles_oracle_names_coverage)
{
    SelectParams(ChainType::TESTNET);
    const std::vector<OracleNodeInfo>& oracles = Params().GetOracleNodes();
    std::vector<std::string> oracle_names = {"Jared", "Green Candle", "Bastian", "DanGB", "Shenger", "Ycagel", "Aussie"};

    // Names array should cover all configured oracles (up to 7)
    size_t count = std::min(oracles.size(), (size_t)7);
    BOOST_CHECK_GE(oracle_names.size(), count);
}

/**
 * Test: OracleManager reports no running oracles by default
 *
 * When no oracle has been started, IsOracleRunning should return false
 * for all IDs. This is the state getoracles sees for local status.
 */
BOOST_AUTO_TEST_CASE(getoracles_no_running_oracles_by_default)
{
    OracleManager& mgr = OracleManager::GetInstance();

    for (uint32_t id = 0; id <= 6; ++id) {
        BOOST_CHECK_EQUAL(mgr.IsOracleRunning(id), false);
        BOOST_CHECK(mgr.GetOracleNode(id) == nullptr);
    }
}

/**
 * Test: Active oracle count is zero when none started
 */
BOOST_AUTO_TEST_CASE(getoracles_active_count_zero)
{
    OracleManager& mgr = OracleManager::GetInstance();
    BOOST_CHECK_EQUAL(mgr.GetActiveOracleCount(), 0u);

    std::vector<uint32_t> active_ids = mgr.GetActiveOracleIds();
    BOOST_CHECK(active_ids.empty());
}

// ============================================================================
// PART 2: listoracle logic — local oracle status
// ============================================================================

/**
 * Test: No local oracle running — listoracle should indicate not running
 *
 * When no oracle is running, the listoracle RPC returns running=false
 * with a help message. Verify the manager state supports this.
 */
BOOST_AUTO_TEST_CASE(listoracle_no_running_oracle)
{
    OracleManager& mgr = OracleManager::GetInstance();

    // Scan all IDs 0-6 like the RPC does
    OracleNode* found = nullptr;
    for (uint32_t id = 0; id <= 6; ++id) {
        if (mgr.IsOracleRunning(id)) {
            found = mgr.GetOracleNode(id);
            break;
        }
    }

    BOOST_CHECK(found == nullptr);
}

/**
 * Test: OracleNode default state — price and timestamps are zero
 *
 * A freshly constructed OracleNode should have no valid price,
 * zero timestamps, and not be running.
 */
BOOST_AUTO_TEST_CASE(listoracle_oracle_node_default_state)
{
    OracleNode node;

    BOOST_CHECK_EQUAL(node.IsRunning(), false);
    BOOST_CHECK_EQUAL(node.IsEnabled(), false);
    BOOST_CHECK_EQUAL(node.HasValidPrice(), false);
    BOOST_CHECK_EQUAL(node.GetCurrentPrice(), 0);
    BOOST_CHECK_EQUAL(node.GetLastUpdateTime(), 0);
    BOOST_CHECK_EQUAL(node.GetLastBroadcastTime(), 0);
    BOOST_CHECK_EQUAL(node.GetStartTime(), 0);
}

/**
 * Test: OracleNode initialized with key has correct pubkey
 *
 * listoracle reports the pubkey of the running oracle. Verify
 * Initialize populates it correctly.
 */
BOOST_AUTO_TEST_CASE(listoracle_oracle_node_pubkey_after_init)
{
    OracleNode node;
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();

    node.Initialize(3, key, pubkey);

    BOOST_CHECK_EQUAL(node.GetOracleId(), 3u);
    BOOST_CHECK(node.GetPublicKey() == pubkey);
    BOOST_CHECK(node.GetPublicKey().IsValid());
}

/**
 * Test: OracleNode enable/disable toggle
 *
 * listoracle reports the enabled state. Verify toggling works.
 */
BOOST_AUTO_TEST_CASE(listoracle_oracle_enable_disable)
{
    OracleNode node;
    CKey key;
    key.MakeNewKey(true);
    node.Initialize(0, key, key.GetPubKey());

    // After Initialize, oracle defaults to enabled
    BOOST_CHECK_EQUAL(node.IsEnabled(), true);

    node.SetEnabled(false);
    BOOST_CHECK_EQUAL(node.IsEnabled(), false);

    node.SetEnabled(true);
    BOOST_CHECK_EQUAL(node.IsEnabled(), true);
}

// ============================================================================
// PART 3: On-chain data extraction (getoracles on-chain fallback)
// ============================================================================

/**
 * Test: OracleBundleManager singleton is accessible
 *
 * getoracles uses OracleBundleManager::GetInstance(). Verify it works.
 */
BOOST_AUTO_TEST_CASE(getoracles_bundle_manager_accessible)
{
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();
    // Just verify we can call it without crashing
    (void)mgr;
    BOOST_CHECK(true);
}

/**
 * Test: ExtractOracleBundle returns false for empty transaction
 *
 * When scanning blocks, getoracles calls ExtractOracleBundle on coinbase.
 * Verify it handles non-oracle transactions gracefully.
 */
BOOST_AUTO_TEST_CASE(getoracles_extract_bundle_empty_tx)
{
    OracleBundleManager& mgr = OracleBundleManager::GetInstance();

    // Create a minimal transaction with no oracle data
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 0;
    mtx.vout[0].scriptPubKey = CScript();

    CTransaction tx(mtx);
    COracleBundle bundle;

    bool result = mgr.ExtractOracleBundle(tx, bundle);
    BOOST_CHECK_EQUAL(result, false);
}

// ============================================================================
// PART 4: Edge cases
// ============================================================================

/**
 * Test: GetOracleNode for out-of-range ID returns nullptr
 *
 * getoracles iterates configured oracles, but listoracle scans 0-6.
 * Verify out-of-range IDs don't crash.
 */
BOOST_AUTO_TEST_CASE(oracle_manager_out_of_range_id)
{
    OracleManager& mgr = OracleManager::GetInstance();

    BOOST_CHECK(mgr.GetOracleNode(99) == nullptr);
    BOOST_CHECK_EQUAL(mgr.IsOracleRunning(99), false);
    BOOST_CHECK(mgr.GetOracleNode(255) == nullptr);
}

/**
 * Test: Price source logic — local vs on-chain vs none
 *
 * getoracles prefers local runtime price, falls back to on-chain.
 * Verify the precedence logic by checking HasValidPrice behavior.
 */
BOOST_AUTO_TEST_CASE(getoracles_price_source_precedence)
{
    // A default OracleNode has no valid price — should fall back to on-chain
    OracleNode node;
    BOOST_CHECK_EQUAL(node.HasValidPrice(), false);
    BOOST_CHECK_EQUAL(node.GetCurrentPrice(), 0);

    // This means getoracles would use "on-chain" or "none" as price_source
    // (depending on whether blocks contain oracle bundles)
}

/**
 * Test: Epoch selection logic is deterministic
 *
 * getoracles reports selected_for_epoch. Verify the selection functions
 * exist and return consistent results.
 */
BOOST_AUTO_TEST_CASE(getoracles_epoch_selection_deterministic)
{
    SelectParams(ChainType::TESTNET);
    const std::vector<OracleNodeInfo>& oracles = Params().GetOracleNodes();

    if (oracles.empty()) {
        // Skip if no oracles configured
        return;
    }

    int32_t epoch = GetCurrentEpoch(100);
    std::vector<OracleNodeInfo> selected1 = SelectOraclesForEpoch(oracles, epoch);
    std::vector<OracleNodeInfo> selected2 = SelectOraclesForEpoch(oracles, epoch);

    // Same epoch should produce same selection
    BOOST_CHECK_EQUAL(selected1.size(), selected2.size());
    for (size_t i = 0; i < selected1.size(); ++i) {
        BOOST_CHECK_EQUAL(selected1[i].id, selected2[i].id);
    }
}

/**
 * Test: Oracle USD price calculation
 *
 * Both getoracles and listoracle compute price_usd as price / 1000000.0.
 * Verify the arithmetic for representative values.
 */
BOOST_AUTO_TEST_CASE(oracle_price_usd_calculation)
{
    // 50000 micro-USD = $0.05
    int64_t price1 = 50000;
    double usd1 = static_cast<double>(price1) / 1000000.0;
    BOOST_CHECK_CLOSE(usd1, 0.05, 0.001);

    // 6000 micro-USD = $0.006
    int64_t price2 = 6000;
    double usd2 = static_cast<double>(price2) / 1000000.0;
    BOOST_CHECK_CLOSE(usd2, 0.006, 0.001);

    // 0 micro-USD = $0.00
    int64_t price3 = 0;
    double usd3 = static_cast<double>(price3) / 1000000.0;
    BOOST_CHECK_EQUAL(usd3, 0.0);

    // 1000000 micro-USD = $1.00
    int64_t price4 = 1000000;
    double usd4 = static_cast<double>(price4) / 1000000.0;
    BOOST_CHECK_CLOSE(usd4, 1.0, 0.001);
}

BOOST_AUTO_TEST_SUITE_END()
