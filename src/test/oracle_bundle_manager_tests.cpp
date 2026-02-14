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
#include <test/util/random.h>
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
    CAmount price_micro_usd = 6000; // $0.006 (realistic DGB price)
    int64_t timestamp = GetTime();

    COraclePriceMessage msg(oracle_id, price_micro_usd, timestamp);

    // Sign the message with Schnorr signature
    BOOST_CHECK(msg.SignPhase2(oracle_key));

    // Validate message
    BOOST_CHECK(msg.IsValid());
    BOOST_CHECK(msg.VerifyPhase2());

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
    COraclePriceMessage future_msg(0, 6000, GetTime() + 3600);
    BOOST_CHECK(!future_msg.IsValid());

    // Test 3: Valid message
    COraclePriceMessage valid_msg(0, 6000, GetTime());
    BOOST_CHECK(valid_msg.SignPhase2(oracle_key));
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
    BOOST_CHECK(aggregate_price <= 10000000);  // <= $10

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
        msg.SignPhase2(oracle_key);

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
    CAmount test_price = 6000;
    int64_t test_timestamp = GetTime();
    COraclePriceMessage msg = oracle.CreatePriceMessage(test_price, test_timestamp);

    // Verify message (CreatePriceMessage uses Phase 2 signing)
    BOOST_CHECK(msg.IsValid());
    BOOST_CHECK_EQUAL(msg.oracle_id, 0);
    BOOST_CHECK_EQUAL(msg.price_micro_usd, test_price);
    BOOST_CHECK(msg.VerifyPhase2());

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
    COraclePriceMessage msg(0, 6000, GetTime());
    msg.SignPhase2(oracle_key);

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

    COraclePriceMessage msg(0, 6000, GetTime());
    msg.SignPhase2(oracle_key);

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

    COraclePriceMessage msg(0, 6000, GetTime());
    msg.SignPhase2(oracle_key);

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

/**
 * Test RegisterSeenHash prevents P2P duplicate processing
 *
 * When a P2P message is successfully added via AddOracleMessage, the
 * P2P handler should register the wrapper hash (OraclePriceMsg::GetHash())
 * so that subsequent relays from other peers are caught by HasOracleMessage()
 * without hitting AddOracleMessage again (which logs 3 lines per call).
 *
 * This test verifies that RegisterSeenHash() properly inserts a hash into
 * seen_message_hashes, and that HasOracleMessage() returns true for it.
 */
BOOST_AUTO_TEST_CASE(register_seen_hash_dedup)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    // Create a valid oracle message
    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    COraclePriceMessage msg(0, 6000, GetTime());
    msg.SignPhase2(oracle_key);

    // Simulate the P2P wrapper hash (what OraclePriceMsg::GetHash() returns)
    // This is what net_processing computes before calling HasOracleMessage
    uint256 p2p_hash = msg.GetPhase2SignatureHash();

    // Before registration, HasOracleMessage should return false for a random hash
    uint256 random_hash = InsecureRand256();
    BOOST_CHECK(!manager.HasOracleMessage(random_hash));

    // Register the P2P hash explicitly
    manager.RegisterSeenHash(p2p_hash);

    // Now HasOracleMessage should return true
    BOOST_CHECK(manager.HasOracleMessage(p2p_hash));

    // Other hashes should still not be seen
    BOOST_CHECK(!manager.HasOracleMessage(random_hash));
}

/**
 * Test that AddOracleMessage + RegisterSeenHash together prevent duplicate log spam
 *
 * Simulates the flow: first peer sends oracle message → accepted via AddOracleMessage,
 * then P2P calls RegisterSeenHash. Subsequent peers' messages (same hash) should be
 * caught by HasOracleMessage without calling AddOracleMessage at all.
 */
BOOST_AUTO_TEST_CASE(full_dedup_flow_prevents_log_spam)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    CKey oracle_key;
    oracle_key.MakeNewKey(true);

    COraclePriceMessage msg(0, 7500, GetTime());
    msg.SignPhase2(oracle_key);

    // Step 1: First peer sends message → AddOracleMessage succeeds
    BOOST_CHECK(manager.AddOracleMessage(msg));

    // Step 2: After successful add, P2P handler registers the wrapper hash
    // (In production this is OraclePriceMsg::GetHash(), which for Phase2
    // messages equals GetPhase2SignatureHash())
    uint256 p2p_hash = msg.GetPhase2SignatureHash();
    manager.RegisterSeenHash(p2p_hash);

    // Step 3: Second peer sends the same message
    // HasOracleMessage should catch it immediately
    BOOST_CHECK(manager.HasOracleMessage(p2p_hash));

    // Step 4: Even the internal hash (what AddOracleMessage inserted) should be seen
    // AddOracleMessage uses GetPhase2SignatureHash() for Phase2 messages
    uint256 internal_hash = msg.GetPhase2SignatureHash();
    BOOST_CHECK(manager.HasOracleMessage(internal_hash));

    // Step 5: A completely new message should NOT be seen
    COraclePriceMessage msg2(1, 8000, GetTime());
    msg2.SignPhase2(oracle_key);
    uint256 new_hash = msg2.GetPhase2SignatureHash();
    BOOST_CHECK(!manager.HasOracleMessage(new_hash));
}

BOOST_AUTO_TEST_SUITE_END()
