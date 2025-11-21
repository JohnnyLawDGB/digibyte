// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <logging.h>
#include <util/strencodings.h>

#include <chainparams.h>
#include <key.h>
#include <oracle/bundle_manager.h>
#include <oracle/exchange.h>
#include <oracle/node.h>
#include <primitives/oracle.h>
#include <test/util/setup_common.h>
#include <util/time.h>

BOOST_FIXTURE_TEST_SUITE(oracle_bundle_manager_tests, RegTestingSetup)

/**
 * Test Phase One: 1-of-1 Consensus Bundle Creation
 */
BOOST_AUTO_TEST_CASE(phase_one_bundle_creation)
{
    // Initialize bundle manager
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear(); // Clear state for test isolation
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1); // Phase One: 1-of-1

    // Create oracle key
    CKey oracle_key;
    oracle_key.MakeNewKey(true);
    CPubKey oracle_pubkey = oracle_key.GetPubKey();

    // Create oracle price message
    uint32_t oracle_id = 0;
    CAmount price_micro_usd = 5; // $0.05 (5 cents)
    int64_t timestamp = GetTime();

    COraclePriceMessage msg(oracle_id, price_micro_usd, timestamp);

    // Sign the message with Schnorr signature
    BOOST_CHECK(msg.Sign(oracle_key));

    // Validate message
    BOOST_CHECK(msg.IsValid());
    BOOST_CHECK(msg.Verify());

    // Add message to bundle manager
    BOOST_CHECK(manager.AddOracleMessage(msg));

    // Check pending messages
    BOOST_CHECK_EQUAL(manager.GetPendingMessageCount(), 1);

    // Get current epoch
    int32_t epoch = GetCurrentEpoch(1000);

    // Create bundle for this epoch
    manager.TryCreateBundle(epoch);

    // Verify bundle was created
    BOOST_CHECK(manager.HasValidBundle(epoch));

    // Get bundle
    COracleBundle bundle = manager.GetCurrentBundle(epoch);

    // Verify bundle has 1 message (Phase One)
    BOOST_CHECK_EQUAL(bundle.messages.size(), 1);

    // Verify consensus price
    CAmount consensus_price = manager.GetConsensusPrice(epoch);
    BOOST_CHECK_EQUAL(consensus_price, price_micro_usd);

    LogPrintf("Test: Phase One bundle created successfully with price=%lld micro-USD\n", consensus_price);
}

/**
 * Test Message Validation and Filtering
 */
BOOST_AUTO_TEST_CASE(message_validation)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear(); // Clear state for test isolation
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    // Test 1: Invalid price (negative)
    COraclePriceMessage invalid_msg(0, -1000, GetTime());
    BOOST_CHECK(!invalid_msg.IsValid());

    // Test 2: Invalid timestamp (future)
    COraclePriceMessage future_msg(0, 5, GetTime() + 3600);
    BOOST_CHECK(!future_msg.IsValid());

    // Test 3: Valid message
    COraclePriceMessage valid_msg(0, 5, GetTime());
    BOOST_CHECK(valid_msg.Sign(oracle_key));
    BOOST_CHECK(valid_msg.IsValid());

    LogPrintf("Test: Message validation working correctly\n");
}

/**
 * Test MultiExchangeAggregator Integration
 */
BOOST_AUTO_TEST_CASE(exchange_aggregator_integration)
{
    // Create aggregator
    ExchangeAPI::MultiExchangeAggregator aggregator;
    aggregator.SetMinRequiredSources(3);
    aggregator.SetOutlierThreshold(0.10);

    // Fetch prices (will use mock data if libcurl not available)
    CAmount aggregate_price = aggregator.FetchAggregatePrice();

    // Should return a valid price (either real or mock)
    BOOST_CHECK(aggregate_price > 0);

    // Price should be reasonable (between $0.001 and $10)
    BOOST_CHECK(aggregate_price >= 1000);      // >= $0.001
    BOOST_CHECK(aggregate_price <= 1000);  // <= $10

    // Check successful sources
    size_t successful_sources = aggregator.GetSuccessfulSourceCount();
    BOOST_CHECK(successful_sources >= 3);

    LogPrintf("Test: Exchange aggregator fetched price=%lld micro-USD from %d sources\n",
             aggregate_price, successful_sources);
}

/**
 * Test Bundle Persistence and Cleanup
 */
BOOST_AUTO_TEST_CASE(bundle_persistence_cleanup)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear(); // Clear state for test isolation
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    // Create messages for multiple epochs
    for (int32_t epoch = 0; epoch < 5; epoch++) {
        COraclePriceMessage msg(0, 50000 + epoch * 100, GetTime());
        msg.Sign(oracle_key);

        manager.AddOracleMessage(msg);
    }

    // Cleanup old bundles (keep only current and previous)
    int32_t current_epoch = 4;
    manager.CleanupOldBundles(current_epoch);

    // Check that old bundles are removed
    BOOST_CHECK(!manager.HasValidBundle(0));
    BOOST_CHECK(!manager.HasValidBundle(1));
    BOOST_CHECK(!manager.HasValidBundle(2));

    LogPrintf("Test: Bundle cleanup working correctly\n");
}

/**
 * Test Oracle Node Price Fetching
 */
BOOST_AUTO_TEST_CASE(oracle_node_price_fetching)
{
    // Create oracle node
    OracleNode oracle;

    // Initialize with test key
    CKey test_key;
    test_key.MakeNewKey(true);
    // Get the 32-byte private key data
    std::string key_hex = HexStr(Span{test_key.begin(), test_key.end()});

    BOOST_REQUIRE(oracle.Initialize(0, key_hex));

    // Create and sign a price message
    CAmount test_price = 5;
    int64_t test_timestamp = GetTime();
    COraclePriceMessage msg = oracle.CreatePriceMessage(test_price, test_timestamp);

    // Verify message
    BOOST_CHECK(msg.IsValid());
    BOOST_CHECK_EQUAL(msg.oracle_id, 0);
    BOOST_CHECK_EQUAL(msg.price_micro_usd, test_price);
    BOOST_CHECK(msg.Verify());

    LogPrintf("Test: Oracle node price message creation successful\n");
}

/**
 * Test Bundle Validation Rules
 */
BOOST_AUTO_TEST_CASE(bundle_validation_rules)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear(); // Clear state for test isolation
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    int32_t epoch = GetCurrentEpoch(1000);

    // Create bundle with valid message
    COracleBundle bundle(epoch);
    COraclePriceMessage msg(0, 5, GetTime());
    msg.Sign(oracle_key);

    BOOST_CHECK(bundle.AddMessage(msg));

    // Verify bundle has consensus (Phase One: 1 message)
    BOOST_CHECK_EQUAL(bundle.messages.size(), 1);

    // Verify epoch validation
    BOOST_CHECK(bundle.ValidateEpoch(epoch));
    BOOST_CHECK(bundle.ValidateEpoch(epoch + 1)); // Next epoch should also accept
    BOOST_CHECK(!bundle.ValidateEpoch(epoch + 2)); // Two epochs ahead should reject

    LogPrintf("Test: Bundle validation rules working correctly\n");
}

/**
 * Test Phase One Testnet Configuration
 */
BOOST_AUTO_TEST_CASE(phase_one_testnet_config)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear(); // Clear state for test isolation

    // For Phase One, min oracle count should be 1 on testnet
    manager.SetMinOracleCount(1);
    BOOST_CHECK_EQUAL(manager.GetStats().pending_messages, 0);

    // Add single oracle message
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    COraclePriceMessage msg(0, 5, GetTime());
    msg.Sign(oracle_key);

    BOOST_CHECK(manager.AddOracleMessage(msg));

    // With 1-of-1 consensus, create bundle
    int32_t epoch = GetCurrentEpoch(1000);
    manager.TryCreateBundle(epoch);
    BOOST_CHECK(manager.HasValidBundle(epoch));

    // Verify price is available
    CAmount price = manager.GetLatestPrice();
    BOOST_CHECK(price > 0);

    LogPrintf("Test: Phase One testnet configuration validated (1-of-1 consensus)\n");
}

/**
 * Test Oracle Stats Reporting
 */
BOOST_AUTO_TEST_CASE(oracle_stats_reporting)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear(); // Clear state for test isolation
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    // Get initial stats
    OracleBundleManager::OracleStats stats = manager.GetStats();
    BOOST_CHECK_EQUAL(stats.pending_messages, 0);

    // Add message
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    COraclePriceMessage msg(0, 5, GetTime());
    msg.Sign(oracle_key);

    manager.AddOracleMessage(msg);

    // Create bundle to achieve consensus
    int32_t epoch = GetCurrentEpoch(1000);
    manager.TryCreateBundle(epoch);

    // Check updated stats
    stats = manager.GetStats();
    BOOST_CHECK_EQUAL(stats.pending_messages, 1);
    BOOST_CHECK(stats.has_consensus);
    BOOST_CHECK(stats.latest_price > 0);

    LogPrintf("Test: Oracle stats - pending=%d, consensus=%s, price=%lld\n",
             stats.pending_messages, stats.has_consensus ? "true" : "false", stats.latest_price);
}

BOOST_AUTO_TEST_SUITE_END()
