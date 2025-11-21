// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <logging.h>
#include <util/strencodings.h>
#include <primitives/oracle.h>
#include <key.h>
#include <pubkey.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <test/util/setup_common.h>
#include <test/util/random.h>
#include <uint256.h>
#include <streams.h>

BOOST_FIXTURE_TEST_SUITE(oracle_message_tests, BasicTestingSetup)

//
// CATEGORY 1: SCHNORR SIGNATURE TESTS (6 tests)
//

/**
 * RED TEST: Test creating Schnorr signature for price message
 *
 * EXPECTED TO FAIL: COraclePriceMessage does not have:
 * - XOnlyPubKey oracle_pubkey field
 * - uint64_t price_micro_usd field
 * - std::vector<unsigned char> schnorr_sig field
 * - GetHash() method
 */
BOOST_AUTO_TEST_CASE(schnorr_signature_creation_valid)
{
    // Generate test keypair
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    // Create oracle message
    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL: field doesn't exist
    msg.price_micro_usd = 5;                   // WILL FAIL: field doesn't exist (should be $0.05)
    msg.timestamp = GetTime();                      // OK: exists but wrong type (int64_t exists)

    // Get message hash for signing
    uint256 hash = msg.GetSignatureHash();         // Fixed: use GetSignatureHash()

    // Create Schnorr signature (64 bytes)
    msg.schnorr_sig.resize(64);                    // WILL FAIL: field doesn't exist

    // Sign message with Schnorr
    BOOST_REQUIRE(privkey.SignSchnorr(hash, msg.schnorr_sig, nullptr, uint256()));

    // Verify signature size is exactly 64 bytes (BIP-340)
    BOOST_CHECK_EQUAL(msg.schnorr_sig.size(), 64);
}

/**
 * RED TEST: Test verifying valid Schnorr signature
 *
 * EXPECTED TO FAIL: COraclePriceMessage does not have Verify() method
 */
BOOST_AUTO_TEST_CASE(schnorr_signature_verification_valid)
{
    // Generate keypair
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    // Create and sign message
    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL
    msg.price_micro_usd = 12;                  // WILL FAIL: $0.123456
    msg.timestamp = GetTime();

    uint256 hash = msg.GetSignatureHash();         // Fixed: use GetSignatureHash()
    msg.schnorr_sig.resize(64);                    // WILL FAIL
    BOOST_REQUIRE(privkey.SignSchnorr(hash, msg.schnorr_sig, nullptr, uint256()));

    // Verify signature using XOnlyPubKey
    BOOST_CHECK(pubkey.VerifySchnorr(hash, msg.schnorr_sig));

    // Also test message-level Verify() method
    BOOST_CHECK(msg.Verify());                     // WILL FAIL: method doesn't exist
}

/**
 * RED TEST: Test rejecting invalid Schnorr signature
 *
 * EXPECTED TO FAIL: Verify() method doesn't exist
 */
BOOST_AUTO_TEST_CASE(schnorr_signature_verification_invalid)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL
    msg.price_micro_usd = 5;                   // WILL FAIL
    msg.timestamp = GetTime();

    // Create invalid signature (all zeros)
    msg.schnorr_sig.resize(64, 0);                 // WILL FAIL

    // Verify should fail with invalid signature
    BOOST_CHECK(!msg.Verify());                    // WILL FAIL
}

/**
 * RED TEST: Test rejecting signature with wrong public key
 *
 * EXPECTED TO FAIL: Fields and methods don't exist
 */
BOOST_AUTO_TEST_CASE(schnorr_signature_wrong_pubkey)
{
    // Create two different keypairs
    CKey privkey1, privkey2;
    privkey1.MakeNewKey(true);
    privkey2.MakeNewKey(true);

    XOnlyPubKey pubkey1(privkey1.GetPubKey());
    XOnlyPubKey pubkey2(privkey2.GetPubKey());

    // Create message signed by privkey1
    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey1;                   // WILL FAIL
    msg.price_micro_usd = 5;                   // WILL FAIL
    msg.timestamp = GetTime();

    uint256 hash = msg.GetSignatureHash();         // Fixed: use GetSignatureHash()
    msg.schnorr_sig.resize(64);                    // WILL FAIL
    BOOST_REQUIRE(privkey1.SignSchnorr(hash, msg.schnorr_sig, nullptr, uint256()));

    // Replace with wrong public key
    msg.oracle_pubkey = pubkey2;                   // WILL FAIL

    // Verification should fail
    BOOST_CHECK(!msg.Verify());                    // WILL FAIL
}

/**
 * RED TEST: Test rejecting signature when message is modified
 *
 * EXPECTED TO FAIL: Fields don't exist
 */
BOOST_AUTO_TEST_CASE(schnorr_signature_tampered_message)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    // Create and sign original message
    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL
    msg.price_micro_usd = 5;                   // WILL FAIL: $0.05
    msg.timestamp = GetTime();

    uint256 hash = msg.GetSignatureHash();         // Fixed: use GetSignatureHash()
    msg.schnorr_sig.resize(64);                    // WILL FAIL
    BOOST_REQUIRE(privkey.SignSchnorr(hash, msg.schnorr_sig, nullptr, uint256()));

    // Verify signature is valid initially
    BOOST_REQUIRE(msg.Verify());                   // WILL FAIL

    // Tamper with message (change price)
    msg.price_micro_usd = 6;                   // WILL FAIL: $0.06

    // Verification should now fail
    BOOST_CHECK(!msg.Verify());                    // WILL FAIL
}

/**
 * RED TEST: Test signature is exactly 64 bytes
 *
 * EXPECTED TO FAIL: schnorr_sig field doesn't exist
 */
BOOST_AUTO_TEST_CASE(schnorr_signature_64_bytes)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL
    msg.price_micro_usd = 5;                   // WILL FAIL
    msg.timestamp = GetTime();

    uint256 hash = msg.GetSignatureHash();         // Fixed: use GetSignatureHash()
    msg.schnorr_sig.resize(64);                    // WILL FAIL
    BOOST_REQUIRE(privkey.SignSchnorr(hash, msg.schnorr_sig, nullptr, uint256()));

    // BIP-340 Schnorr signatures are EXACTLY 64 bytes
    BOOST_CHECK_EQUAL(msg.schnorr_sig.size(), 64); // WILL FAIL

    // Not 71-73 bytes like ECDSA DER signatures
    BOOST_CHECK_NE(msg.schnorr_sig.size(), 71);
    BOOST_CHECK_NE(msg.schnorr_sig.size(), 72);
    BOOST_CHECK_NE(msg.schnorr_sig.size(), 73);
}

//
// CATEGORY 2: CORACLEPRICEMESSAGE TESTS (9 tests)
//

/**
 * RED TEST: Test creating COraclePriceMessage with required fields
 *
 * EXPECTED TO FAIL: New fields don't exist
 */
BOOST_AUTO_TEST_CASE(oracle_message_creation)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    // Create message with all required fields
    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL: 32-byte x-only pubkey
    msg.price_micro_usd = 5;                   // WILL FAIL: $0.05 in micro-USD
    msg.timestamp = GetTime();                      // OK: exists
    msg.schnorr_sig.resize(64);                    // WILL FAIL: 64-byte signature

    // Verify field types
    BOOST_CHECK(msg.oracle_pubkey.IsFullyValid()); // WILL FAIL
    BOOST_CHECK_GT(msg.price_micro_usd, 0);        // WILL FAIL
    BOOST_CHECK_GT(msg.timestamp, 0);               // Should work
    BOOST_CHECK_EQUAL(msg.schnorr_sig.size(), 64); // WILL FAIL
}

/**
 * RED TEST: Test message serialization
 *
 * EXPECTED TO FAIL: SERIALIZE_METHODS not updated for new fields
 */
BOOST_AUTO_TEST_CASE(oracle_message_serialization)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    // Create and sign message
    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL
    msg.price_micro_usd = 12;                  // WILL FAIL: $0.123456
    msg.timestamp = 1700000000;

    uint256 hash = msg.GetSignatureHash();         // Fixed: use GetSignatureHash()
    msg.schnorr_sig.resize(64);                    // WILL FAIL
    privkey.SignSchnorr(hash, msg.schnorr_sig, nullptr, uint256());

    // Serialize to stream
    DataStream ss{};
    ss << msg;                                      // WILL FAIL: SERIALIZE_METHODS missing new fields

    // Verify serialized size
    // Expected: 32 (pubkey) + 8 (price) + 8 (timestamp) + 64 (sig) = 112 bytes minimum
    BOOST_CHECK_GE(ss.size(), 112);
}

/**
 * RED TEST: Test message deserialization
 *
 * EXPECTED TO FAIL: SERIALIZE_METHODS not updated
 */
BOOST_AUTO_TEST_CASE(oracle_message_deserialization)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    // Create original message
    COraclePriceMessage msg1;
    msg1.oracle_pubkey = pubkey;                   // WILL FAIL
    msg1.price_micro_usd = 5;                  // WILL FAIL
    msg1.timestamp = 1700000000;

    uint256 hash = msg1.GetSignatureHash();        // Fixed: use GetSignatureHash()
    msg1.schnorr_sig.resize(64);                   // WILL FAIL
    privkey.SignSchnorr(hash, msg1.schnorr_sig, nullptr, uint256());

    // Serialize
    DataStream ss{};
    ss << msg1;                                     // WILL FAIL

    // Deserialize to new message
    COraclePriceMessage msg2;
    ss >> msg2;                                     // WILL FAIL

    // Verify fields match
    BOOST_CHECK_EQUAL(msg1.price_micro_usd, msg2.price_micro_usd);   // WILL FAIL
    BOOST_CHECK_EQUAL(msg1.timestamp, msg2.timestamp);
    BOOST_CHECK(msg1.schnorr_sig == msg2.schnorr_sig);               // WILL FAIL
}

/**
 * RED TEST: Test GetHash() returns correct hash
 *
 * EXPECTED TO FAIL: GetHash() method doesn't exist
 */
BOOST_AUTO_TEST_CASE(oracle_message_hash_calculation)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL
    msg.price_micro_usd = 5;                   // WILL FAIL
    msg.timestamp = 1700000000;

    // GetHash() should return deterministic hash
    uint256 hash1 = msg.GetSignatureHash();        // Fixed: use GetSignatureHash()
    uint256 hash2 = msg.GetSignatureHash();        // Fixed: use GetSignatureHash()

    // Same message should produce same hash
    BOOST_CHECK(hash1 == hash2);
    BOOST_CHECK(!hash1.IsNull());

    // Different message should produce different hash
    msg.price_micro_usd = 6;                   // WILL FAIL
    uint256 hash3 = msg.GetSignatureHash();        // Fixed: use GetSignatureHash()
    BOOST_CHECK(hash1 != hash3);
}

/**
 * RED TEST: Test signing and verifying message
 *
 * EXPECTED TO FAIL: Multiple missing fields and methods
 */
BOOST_AUTO_TEST_CASE(oracle_message_sign_and_verify)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    // Create message
    COraclePriceMessage msg;
    msg.oracle_pubkey = pubkey;                    // WILL FAIL
    msg.price_micro_usd = 5;                   // WILL FAIL
    msg.timestamp = GetTime();

    // Sign message
    uint256 hash = msg.GetSignatureHash();         // Fixed: use GetSignatureHash()
    msg.schnorr_sig.resize(64);                    // WILL FAIL
    BOOST_REQUIRE(privkey.SignSchnorr(hash, msg.schnorr_sig, nullptr, uint256()));

    // Verify signature
    BOOST_CHECK(msg.Verify());                     // WILL FAIL: method doesn't exist

    // Verify using explicit pubkey
    BOOST_CHECK(pubkey.VerifySchnorr(hash, msg.schnorr_sig));
}

/**
 * RED TEST: Test price is in micro-USD format
 *
 * EXPECTED TO FAIL: price_micro_usd field doesn't exist
 */
BOOST_AUTO_TEST_CASE(oracle_message_micro_usd_format)
{
    COraclePriceMessage msg;

    // Test micro-USD conversion
    // 1 USD = 1,000,000 micro-USD
    // $0.05 per DGB = 50,000 micro-USD
    msg.price_micro_usd = 5;                   // WILL FAIL: field doesn't exist
    BOOST_CHECK_EQUAL(msg.price_micro_usd, 50000); // WILL FAIL

    // Test various price points
    msg.price_micro_usd = 100;                 // WILL FAIL: $1.00
    BOOST_CHECK_EQUAL(msg.price_micro_usd, 1000000);

    msg.price_micro_usd = 12340;                   // WILL FAIL: $0.01234
    BOOST_CHECK_EQUAL(msg.price_micro_usd, 12340);

    // Test range (DGB price should be $0.0001 to $10.00)
    msg.price_micro_usd = 100;                     // WILL FAIL: $0.0001 (minimum)
    BOOST_CHECK_GE(msg.price_micro_usd, 100);

    msg.price_micro_usd = 1000;                // WILL FAIL: $10.00 (maximum)
    BOOST_CHECK_LE(msg.price_micro_usd, 10000000);

    // Verify NOT in satoshis (current implementation)
    // Old format: price_satoshis (DGB sats per USD)
    // New format: price_micro_usd (micro-USD per DGB)
    // These are COMPLETELY different!
}

/**
 * RED TEST: Test timestamp validation
 *
 * EXPECTED TO FAIL: Validation logic needs update
 */
BOOST_AUTO_TEST_CASE(oracle_message_timestamp_validation)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.oracle_pubkey = pubkey;
    msg.price_micro_usd = 5;
    msg.block_height = 1000;
    msg.nonce = 12345;

    // Test current timestamp (should be valid)
    msg.timestamp = GetTime();
    BOOST_REQUIRE(msg.Sign(privkey));
    BOOST_CHECK(msg.IsValid());

    // Test timestamp 1 minute ago (should be valid)
    msg.timestamp = GetTime() - 60;
    BOOST_REQUIRE(msg.Sign(privkey));
    BOOST_CHECK(msg.IsValid());

    // Test timestamp 30 minutes ago (should be valid)
    msg.timestamp = GetTime() - 1800;
    BOOST_REQUIRE(msg.Sign(privkey));
    BOOST_CHECK(msg.IsValid());

    // Test timestamp 59 minutes ago (should be valid, within 1 hour)
    msg.timestamp = GetTime() - 3540;
    BOOST_REQUIRE(msg.Sign(privkey));
    BOOST_CHECK(msg.IsValid());
}

/**
 * RED TEST: Test rejecting messages from the future
 *
 * EXPECTED TO FAIL: Validation might not be strict enough
 */
BOOST_AUTO_TEST_CASE(oracle_message_reject_future_timestamp)
{
    CKey privkey;
    privkey.MakeNewKey(true);
    XOnlyPubKey pubkey(privkey.GetPubKey());

    COraclePriceMessage msg;
    msg.oracle_id = 0;
    msg.oracle_pubkey = pubkey;
    msg.price_micro_usd = 5;
    msg.block_height = 1000;
    msg.nonce = 12345;

    // Test future timestamp (5 minutes ahead, should be invalid)
    msg.timestamp = GetTime() + 300;
    BOOST_REQUIRE(msg.Sign(privkey));
    BOOST_CHECK(!msg.IsValid());

    // Test far future (1 hour ahead, should be invalid)
    msg.timestamp = GetTime() + 3600;
    BOOST_REQUIRE(msg.Sign(privkey));
    BOOST_CHECK(!msg.IsValid());

    // Test timestamp 2 minutes ahead (within clock skew tolerance, might be ok)
    // Spec allows 1 minute tolerance
    msg.timestamp = GetTime() + 120;
    BOOST_CHECK(!msg.IsValid());
}

/**
 * RED TEST: Test rejecting messages older than 1 hour
 *
 * EXPECTED TO FAIL: Validation exists but may need adjustment
 */
BOOST_AUTO_TEST_CASE(oracle_message_reject_old_timestamp)
{
    CKey privkey;
    privkey.MakeNewKey(true);

    // Test timestamp 61 minutes ago (should be invalid)
    COraclePriceMessage msg1;
    msg1.oracle_id = 1;
    msg1.price_micro_usd = 5;
    msg1.timestamp = GetTime() - 3660;  // 61 minutes
    msg1.block_height = 0;
    msg1.nonce = 0;
    msg1.Sign(privkey);
    BOOST_CHECK(!msg1.IsValid());  // Should fail due to old timestamp

    // Test timestamp 2 hours ago (should be invalid)
    COraclePriceMessage msg2;
    msg2.oracle_id = 1;
    msg2.price_micro_usd = 5;
    msg2.timestamp = GetTime() - 7200;
    msg2.block_height = 0;
    msg2.nonce = 0;
    msg2.Sign(privkey);
    BOOST_CHECK(!msg2.IsValid());

    // Test timestamp exactly 1 hour ago (boundary, should still be valid)
    COraclePriceMessage msg3;
    msg3.oracle_id = 1;
    msg3.price_micro_usd = 5;
    msg3.timestamp = GetTime() - 3600;
    msg3.block_height = 0;
    msg3.nonce = 0;
    msg3.Sign(privkey);
    BOOST_CHECK(msg3.IsValid());
}

BOOST_AUTO_TEST_SUITE_END()
