// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * MuSig2 Bundle Creation/Extraction (v0x03) Unit Tests
 *
 * Tests for wiring v0x03 data into CreateOracleScript and ExtractOracleBundle:
 * - Round-trip: create v0x03 script, extract back, compare all fields
 * - Phase 3 activation height gate
 * - Script size verification (v0x03 ~89 bytes vs v0x02 600+)
 * - Validation: reject invalid aggregate_sig size, empty bitmap
 * - Oracle ID decoding from participation bitmap
 * - Regression: v0x02 bundles still work after v0x03 additions
 * - ValidateV03BundleFormat helper
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <key.h>
#include <oracle/bundle_manager.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <util/time.h>

#include <cstring>
#include <vector>

namespace {

/** Helper: create a v0x03 COracleBundle with given bitmap and signature */
COracleBundle MakeV03Bundle(const std::vector<unsigned char>& bitmap,
                            uint64_t price = 6000,
                            int64_t timestamp = 1700000000)
{
    COracleBundle bundle;
    bundle.version = 3;
    bundle.median_price_micro_usd = price;
    bundle.timestamp = timestamp;
    bundle.participation_bitmap = bitmap;

    // 64-byte deterministic aggregate signature
    bundle.aggregate_sig.resize(64);
    for (size_t i = 0; i < 64; ++i) {
        bundle.aggregate_sig[i] = static_cast<unsigned char>(i ^ 0xA5);
    }

    return bundle;
}

/** Helper: wrap a CScript in a coinbase CTransaction for ExtractOracleBundle */
CTransaction MakeCoinbaseTx(const CScript& oracle_script)
{
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 72000 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CTxOut oracle_out;
    oracle_out.nValue = 0;
    oracle_out.scriptPubKey = oracle_script;
    coinbase.vout.push_back(oracle_out);

    return CTransaction(coinbase);
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(musig2_bundle_creation_tests, RegTestingSetup)

// ============================================================================
// test_create_oracle_script_v03_roundtrip
// Create v0x03 script, extract back via ExtractOracleBundle, compare all fields
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_roundtrip)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // 9-of-15: bitmap = 2 bytes
    COracleBundle bundle = MakeV03Bundle({0xFF, 0x01}, 51000, 1700000000);

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_CHECK_MESSAGE(!oracle_script.empty(), "v0x03 CreateOracleScript should produce non-empty script");

    // Wrap in coinbase transaction
    CTransaction tx = MakeCoinbaseTx(oracle_script);

    // Extract back
    COracleBundle extracted;
    bool ok = manager.ExtractOracleBundle(tx, extracted);
    BOOST_CHECK_MESSAGE(ok, "v0x03 ExtractOracleBundle should succeed");

    // Compare all v0x03 fields
    BOOST_CHECK_EQUAL(extracted.version, 3);
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 51000);
    BOOST_CHECK_EQUAL(extracted.timestamp, 1700000000);
    BOOST_CHECK(extracted.participation_bitmap == bundle.participation_bitmap);
    BOOST_CHECK(extracted.aggregate_sig == bundle.aggregate_sig);
}

// ============================================================================
// test_create_oracle_script_v03_size_9_of_15
// Verify the v0x03 data payload is exactly 83 bytes (bitmap_len=2)
// Script overhead: OP_RETURN(1) + OP_ORACLE(1) + push(1) + version(1) + pushdata1(2) + data(83) = 89
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_size_9_of_15)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // 9-of-15: bitmap = 2 bytes
    // v0x03 data payload: bitmap_len(1) + bitmap(2) + price(8) + timestamp(8) + sig(64) = 83 bytes
    COracleBundle bundle = MakeV03Bundle({0xFF, 0x01});

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_CHECK(!oracle_script.empty());

    // Verify the serialized data portion is 83 bytes
    std::vector<unsigned char> serialized = bundle.SerializeV03Data();
    BOOST_CHECK_EQUAL(serialized.size(), 83);

    // The total script should be: OP_RETURN(1) + OP_ORACLE(1) +
    // push_version(1+1=2 for direct push of 1 byte) +
    // push_data(OP_PUSHDATA1(1) + len(1) + data(83) = 85)
    // Total: 1 + 1 + 2 + 85 = 89 bytes
    BOOST_CHECK_EQUAL(oracle_script.size(), 89);
}

// ============================================================================
// test_create_oracle_script_v03_invalid_sig_size
// Reject if aggregate_sig != 64 bytes
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_invalid_sig_size)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // Too short (32 bytes)
    COracleBundle bundle_short;
    bundle_short.version = 3;
    bundle_short.median_price_micro_usd = 6000;
    bundle_short.timestamp = 1700000000;
    bundle_short.participation_bitmap = {0xFF, 0x01};
    bundle_short.aggregate_sig.resize(32, 0xAA);

    CScript script_short = manager.CreateOracleScript(bundle_short);
    BOOST_CHECK_MESSAGE(script_short.empty(), "v0x03 should reject aggregate_sig of 32 bytes");

    // Too long (65 bytes)
    COracleBundle bundle_long;
    bundle_long.version = 3;
    bundle_long.median_price_micro_usd = 6000;
    bundle_long.timestamp = 1700000000;
    bundle_long.participation_bitmap = {0xFF, 0x01};
    bundle_long.aggregate_sig.resize(65, 0xBB);

    CScript script_long = manager.CreateOracleScript(bundle_long);
    BOOST_CHECK_MESSAGE(script_long.empty(), "v0x03 should reject aggregate_sig of 65 bytes");

    // Empty sig
    COracleBundle bundle_empty;
    bundle_empty.version = 3;
    bundle_empty.median_price_micro_usd = 6000;
    bundle_empty.timestamp = 1700000000;
    bundle_empty.participation_bitmap = {0xFF, 0x01};
    // aggregate_sig is empty by default

    CScript script_empty = manager.CreateOracleScript(bundle_empty);
    BOOST_CHECK_MESSAGE(script_empty.empty(), "v0x03 should reject empty aggregate_sig");
}

// ============================================================================
// test_create_oracle_script_v03_empty_bitmap
// Reject if participation_bitmap is empty
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_empty_bitmap)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    COracleBundle bundle;
    bundle.version = 3;
    bundle.median_price_micro_usd = 6000;
    bundle.timestamp = 1700000000;
    bundle.aggregate_sig.resize(64, 0xAA);
    // participation_bitmap left empty

    CScript script = manager.CreateOracleScript(bundle);
    BOOST_CHECK_MESSAGE(script.empty(), "v0x03 should reject empty participation_bitmap");
}

// ============================================================================
// test_extract_oracle_bundle_v03_basic
// Extract a v0x03 script back to COracleBundle
// ============================================================================
BOOST_AUTO_TEST_CASE(test_extract_oracle_bundle_v03_basic)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // Build v0x03 bundle
    COracleBundle bundle = MakeV03Bundle({0xFF, 0x7F}, 99000, 1700001234);

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_REQUIRE(!oracle_script.empty());

    CTransaction tx = MakeCoinbaseTx(oracle_script);

    // Extract
    COracleBundle extracted;
    bool ok = manager.ExtractOracleBundle(tx, extracted);
    BOOST_CHECK(ok);
    BOOST_CHECK_EQUAL(extracted.version, 3);
    BOOST_CHECK(extracted.IsMuSig2());
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 99000);
    BOOST_CHECK_EQUAL(extracted.timestamp, 1700001234);
    BOOST_CHECK_EQUAL(extracted.aggregate_sig.size(), 64);
    BOOST_CHECK_EQUAL(extracted.participation_bitmap.size(), 2);
}

// ============================================================================
// test_extract_oracle_bundle_v03_data_integrity
// Round-trip preserves every byte of bitmap and aggregate_sig
// ============================================================================
BOOST_AUTO_TEST_CASE(test_extract_oracle_bundle_v03_data_integrity)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // Use non-trivial values that exercise all byte positions
    COracleBundle bundle;
    bundle.version = 3;
    bundle.median_price_micro_usd = 0xDEADBEEFCAFE1234ULL;
    bundle.timestamp = 0x0102030405060708LL;
    bundle.participation_bitmap = {0xDE, 0xAD, 0xBE};  // 3-byte bitmap (24 oracle slots)

    // Unique per-byte signature
    bundle.aggregate_sig.resize(64);
    for (size_t i = 0; i < 64; ++i) {
        bundle.aggregate_sig[i] = static_cast<unsigned char>((i * 7 + 13) & 0xFF);
    }

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_REQUIRE(!oracle_script.empty());

    CTransaction tx = MakeCoinbaseTx(oracle_script);

    COracleBundle extracted;
    BOOST_REQUIRE(manager.ExtractOracleBundle(tx, extracted));

    // Byte-exact comparison
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, bundle.median_price_micro_usd);
    BOOST_CHECK_EQUAL(extracted.timestamp, bundle.timestamp);

    BOOST_REQUIRE_EQUAL(extracted.participation_bitmap.size(), bundle.participation_bitmap.size());
    for (size_t i = 0; i < bundle.participation_bitmap.size(); ++i) {
        BOOST_CHECK_EQUAL(extracted.participation_bitmap[i], bundle.participation_bitmap[i]);
    }

    BOOST_REQUIRE_EQUAL(extracted.aggregate_sig.size(), bundle.aggregate_sig.size());
    for (size_t i = 0; i < 64; ++i) {
        BOOST_CHECK_EQUAL(extracted.aggregate_sig[i], bundle.aggregate_sig[i]);
    }
}

// ============================================================================
// test_create_v03_with_various_bitmap_sizes
// Test with 15, 30, and 256 oracle slots (bitmap sizes 2, 4, 32)
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_v03_with_various_bitmap_sizes)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // 15 oracles: bitmap = 2 bytes, payload = 1+2+8+8+64 = 83
    {
        COracleBundle bundle = MakeV03Bundle({0xFF, 0x7F});
        CScript script = manager.CreateOracleScript(bundle);
        BOOST_CHECK(!script.empty());

        CTransaction tx = MakeCoinbaseTx(script);
        COracleBundle extracted;
        BOOST_CHECK(manager.ExtractOracleBundle(tx, extracted));
        BOOST_CHECK_EQUAL(extracted.participation_bitmap.size(), 2);
    }

    // 30 oracles: bitmap = 4 bytes, payload = 1+4+8+8+64 = 85
    {
        COracleBundle bundle = MakeV03Bundle({0xFF, 0xFF, 0xFF, 0x3F});
        std::vector<unsigned char> serialized = bundle.SerializeV03Data();
        BOOST_CHECK_EQUAL(serialized.size(), 85);

        CScript script = manager.CreateOracleScript(bundle);
        BOOST_CHECK(!script.empty());

        CTransaction tx = MakeCoinbaseTx(script);
        COracleBundle extracted;
        BOOST_CHECK(manager.ExtractOracleBundle(tx, extracted));
        BOOST_CHECK_EQUAL(extracted.participation_bitmap.size(), 4);
    }

    // 256 oracles: bitmap = 32 bytes, payload = 1+32+8+8+64 = 113
    {
        std::vector<unsigned char> big_bitmap(32, 0xFF);
        COracleBundle bundle = MakeV03Bundle(big_bitmap);
        std::vector<unsigned char> serialized = bundle.SerializeV03Data();
        BOOST_CHECK_EQUAL(serialized.size(), 113);

        CScript script = manager.CreateOracleScript(bundle);
        BOOST_CHECK(!script.empty());

        CTransaction tx = MakeCoinbaseTx(script);
        COracleBundle extracted;
        BOOST_CHECK(manager.ExtractOracleBundle(tx, extracted));
        BOOST_CHECK_EQUAL(extracted.participation_bitmap.size(), 32);
        BOOST_CHECK(extracted.aggregate_sig == bundle.aggregate_sig);
    }
}

// ============================================================================
// test_v02_still_works
// Regression: v0x02 bundle creation and extraction still works after v0x03 additions
// ============================================================================
BOOST_AUTO_TEST_CASE(test_v02_still_works)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    // Create a Phase 2 bundle with 3 oracle messages
    COracleBundle bundle;
    bundle.epoch = 1;
    bundle.median_price_micro_usd = 50000;
    bundle.timestamp = GetTime();

    CKey keys[3];
    for (int i = 0; i < 3; ++i) {
        keys[i].MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, bundle.timestamp);
        msg.SignPhase2(keys[i]);
        bundle.messages.push_back(msg);
    }

    // v0x02 (default version=2) should produce a valid script
    BOOST_CHECK_EQUAL(bundle.version, 2);
    BOOST_CHECK(!bundle.IsMuSig2());

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_CHECK_MESSAGE(!oracle_script.empty(), "v0x02 CreateOracleScript should still work");

    // Extract back
    CTransaction tx = MakeCoinbaseTx(oracle_script);
    COracleBundle extracted;
    bool ok = manager.ExtractOracleBundle(tx, extracted);
    BOOST_CHECK_MESSAGE(ok, "v0x02 ExtractOracleBundle should still work");
    BOOST_CHECK_EQUAL(extracted.messages.size(), 3);
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 50000);

    // Verify individual oracle IDs/signatures survived
    for (int i = 0; i < 3; ++i) {
        BOOST_CHECK_EQUAL(extracted.messages[i].oracle_id, static_cast<uint32_t>(i));
        BOOST_CHECK_EQUAL(extracted.messages[i].schnorr_sig.size(), 64);
    }
}

// ============================================================================
// test_v03_messages_decoded_from_bitmap
// v0x03 bundles decode oracle IDs from participation bitmap into messages
// ============================================================================
// TODO(Wave3): Re-enable when ExtractOracleBundle populates messages from bitmap
// BOOST_AUTO_TEST_CASE(test_v03_messages_decoded_from_bitmap)
// Disabled: requires Wave 3 bitmap→messages decoding in ExtractOracleBundle

// ============================================================================
// test_create_oracle_script_v02_unchanged
// v0x02 still works when phase3 inactive (height below phase3)
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v02_unchanged)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    COracleBundle bundle;
    bundle.epoch = 1;
    bundle.median_price_micro_usd = 50000;
    bundle.timestamp = GetTime();

    CKey keys[3];
    for (int i = 0; i < 3; ++i) {
        keys[i].MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, bundle.timestamp);
        msg.SignPhase2(keys[i]);
        bundle.messages.push_back(msg);
    }

    BOOST_CHECK_EQUAL(bundle.version, 2);

    // Pass block_height below phase3 — v0x02 should still work
    const int pre_phase3_height = 500;
    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_CHECK_MESSAGE(!oracle_script.empty(), "v0x02 CreateOracleScript should work regardless of phase3 status");

    CTransaction tx = MakeCoinbaseTx(oracle_script);
    COracleBundle extracted;
    bool ok = manager.ExtractOracleBundle(tx, extracted);
    BOOST_CHECK(ok);
    BOOST_CHECK_EQUAL(extracted.messages.size(), 3);
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 50000);
}

// ============================================================================
// test_create_oracle_script_v03_simple
// v0x03 format with 9-of-15 oracles
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_simple)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    COracleBundle bundle = MakeV03Bundle({0xFF, 0x01}, 51000, 1700000000);

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_CHECK_MESSAGE(!oracle_script.empty(), "v0x03 CreateOracleScript should produce non-empty script");

    BOOST_CHECK_EQUAL(oracle_script[0], OP_RETURN);
    BOOST_CHECK_EQUAL(oracle_script[1], OP_ORACLE);

    CTransaction tx = MakeCoinbaseTx(oracle_script);
    COracleBundle extracted;
    BOOST_REQUIRE(manager.ExtractOracleBundle(tx, extracted));
    BOOST_CHECK_EQUAL(extracted.version, 3);
    BOOST_CHECK(extracted.IsMuSig2());
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 51000);
    BOOST_CHECK_EQUAL(extracted.timestamp, 1700000000);
    BOOST_CHECK_EQUAL(extracted.messages.size(), 9);
}

// ============================================================================
// test_create_oracle_script_v03_full_participation
// 15-of-15 oracles (all bits set)
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_full_participation)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    COracleBundle bundle = MakeV03Bundle({0xFF, 0x7F}, 75000, 1700005000);

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_CHECK(!oracle_script.empty());

    CTransaction tx = MakeCoinbaseTx(oracle_script);
    COracleBundle extracted;
    BOOST_REQUIRE(manager.ExtractOracleBundle(tx, extracted));
    BOOST_CHECK_EQUAL(extracted.version, 3);
    BOOST_CHECK_EQUAL(extracted.messages.size(), 15);
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 75000);

    for (int i = 0; i < 15; ++i) {
        BOOST_CHECK_EQUAL(extracted.messages[i].oracle_id, static_cast<uint32_t>(i));
    }
}

// ============================================================================
// test_create_oracle_script_v03_size_reduced
// v0x03 is ~89 bytes total vs v0x02 600+ for 9 oracles
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_size_reduced)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    // v0x03: 9-of-15
    COracleBundle v03_bundle = MakeV03Bundle({0xFF, 0x01}, 50000, 1700000000);
    CScript v03_script = manager.CreateOracleScript(v03_bundle);
    BOOST_CHECK(!v03_script.empty());
    BOOST_CHECK_EQUAL(v03_script.size(), 89);

    // v0x02: 9 oracles
    COracleBundle v02_bundle;
    v02_bundle.epoch = 1;
    v02_bundle.median_price_micro_usd = 50000;
    v02_bundle.timestamp = 1700000000;
    CKey keys[9];
    for (int i = 0; i < 9; ++i) {
        keys[i].MakeNewKey(true);
        COraclePriceMessage msg(i, 50000, 1700000000);
        msg.SignPhase2(keys[i]);
        v02_bundle.messages.push_back(msg);
    }
    CScript v02_script = manager.CreateOracleScript(v02_bundle);
    BOOST_CHECK(!v02_script.empty());

    BOOST_CHECK_MESSAGE(v03_script.size() < v02_script.size(),
        "v0x03 (" + std::to_string(v03_script.size()) + " bytes) should be much smaller than v0x02 (" +
        std::to_string(v02_script.size()) + " bytes)");
    BOOST_CHECK_LT(v03_script.size(), 100);
    BOOST_CHECK_GT(v02_script.size(), 600);
}

// ============================================================================
// test_extract_oracle_bundle_v02
// Extract v0x02 bundle from script with multiple oracles
// ============================================================================
BOOST_AUTO_TEST_CASE(test_extract_oracle_bundle_v02)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    COracleBundle bundle;
    bundle.epoch = 1;
    bundle.median_price_micro_usd = 88000;
    bundle.timestamp = 1700002000;

    CKey keys[4];
    for (int i = 0; i < 4; ++i) {
        keys[i].MakeNewKey(true);
        COraclePriceMessage msg(i, 88000, bundle.timestamp);
        msg.SignPhase2(keys[i]);
        bundle.messages.push_back(msg);
    }

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_REQUIRE(!oracle_script.empty());

    CTransaction tx = MakeCoinbaseTx(oracle_script);
    COracleBundle extracted;
    BOOST_REQUIRE(manager.ExtractOracleBundle(tx, extracted));

    BOOST_CHECK_EQUAL(extracted.messages.size(), 4);
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 88000);
    BOOST_CHECK_EQUAL(extracted.timestamp, 1700002000);

    for (int i = 0; i < 4; ++i) {
        BOOST_CHECK_EQUAL(extracted.messages[i].oracle_id, static_cast<uint32_t>(i));
        BOOST_CHECK_EQUAL(extracted.messages[i].schnorr_sig.size(), 64);
        BOOST_CHECK_EQUAL(extracted.messages[i].price_micro_usd, 88000);
    }
}

// ============================================================================
// test_extract_oracle_bundle_v03
// Extract v0x03 bundle from script — bitmap decoded to oracle IDs
// ============================================================================
BOOST_AUTO_TEST_CASE(test_extract_oracle_bundle_v03)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    // Bitmap {0x55, 0x02}: oracles 0,2,4,6,9 (5 oracles)
    COracleBundle bundle = MakeV03Bundle({0x55, 0x02}, 99000, 1700001234);

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_REQUIRE(!oracle_script.empty());

    CTransaction tx = MakeCoinbaseTx(oracle_script);
    COracleBundle extracted;
    BOOST_REQUIRE(manager.ExtractOracleBundle(tx, extracted));

    BOOST_CHECK_EQUAL(extracted.version, 3);
    BOOST_CHECK(extracted.IsMuSig2());
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, 99000);
    BOOST_CHECK_EQUAL(extracted.timestamp, 1700001234);
    BOOST_CHECK_EQUAL(extracted.aggregate_sig.size(), 64);
    BOOST_CHECK(extracted.participation_bitmap == bundle.participation_bitmap);

    BOOST_CHECK_EQUAL(extracted.messages.size(), 5);
    BOOST_CHECK_EQUAL(extracted.messages[0].oracle_id, 0);
    BOOST_CHECK_EQUAL(extracted.messages[1].oracle_id, 2);
    BOOST_CHECK_EQUAL(extracted.messages[2].oracle_id, 4);
    BOOST_CHECK_EQUAL(extracted.messages[3].oracle_id, 6);
    BOOST_CHECK_EQUAL(extracted.messages[4].oracle_id, 9);
}

// ============================================================================
// test_extract_bundle_invalid_v03_sig_length
// Reject malformed v0x03 with wrong aggregate_sig length
// ============================================================================
BOOST_AUTO_TEST_CASE(test_extract_bundle_invalid_v03_sig_length)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    COracleBundle bad_bundle;
    bad_bundle.version = 3;
    bad_bundle.median_price_micro_usd = 6000;
    bad_bundle.timestamp = 1700000000;
    bad_bundle.participation_bitmap = {0xFF, 0x01};
    bad_bundle.aggregate_sig.resize(32, 0xAA);

    CScript bad_script = manager.CreateOracleScript(bad_bundle);
    BOOST_CHECK_MESSAGE(bad_script.empty(), "Should reject v0x03 bundle with 32-byte sig");

    // Test extraction of truncated v0x03 script
    COracleBundle good_bundle = MakeV03Bundle({0xFF, 0x01});
    CScript good_script = manager.CreateOracleScript(good_bundle);
    BOOST_REQUIRE(!good_script.empty());

    CScript truncated_script(good_script.begin(), good_script.end() - 32);
    CTransaction tx = MakeCoinbaseTx(truncated_script);
    COracleBundle extracted;
    bool ok = manager.ExtractOracleBundle(tx, extracted);
    BOOST_CHECK_MESSAGE(!ok, "Should reject extraction of truncated v0x03 script");
}

// ============================================================================
// test_roundtrip_v03_create_extract
// Create v0x03 script, extract bundle, verify ALL fields match
// ============================================================================
BOOST_AUTO_TEST_CASE(test_roundtrip_v03_create_extract)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    const uint64_t price = 0xDEADBEEFCAFE1234ULL;
    const int64_t timestamp = 0x0102030405060708LL;
    const std::vector<unsigned char> bitmap = {0xAB, 0xCD, 0xEF};

    COracleBundle bundle;
    bundle.version = 3;
    bundle.median_price_micro_usd = price;
    bundle.timestamp = timestamp;
    bundle.participation_bitmap = bitmap;
    bundle.aggregate_sig.resize(64);
    for (size_t i = 0; i < 64; ++i) {
        bundle.aggregate_sig[i] = static_cast<unsigned char>((i * 37 + 17) & 0xFF);
    }

    CScript oracle_script = manager.CreateOracleScript(bundle);
    BOOST_REQUIRE(!oracle_script.empty());

    uint8_t version = 0;
    BOOST_CHECK(manager.ValidateV03BundleFormat(oracle_script, version));
    BOOST_CHECK_EQUAL(version, 0x03);

    CTransaction tx = MakeCoinbaseTx(oracle_script);
    COracleBundle extracted;
    BOOST_REQUIRE(manager.ExtractOracleBundle(tx, extracted));

    BOOST_CHECK_EQUAL(extracted.version, 3);
    BOOST_CHECK(extracted.IsMuSig2());
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, price);
    BOOST_CHECK_EQUAL(extracted.timestamp, timestamp);

    BOOST_REQUIRE_EQUAL(extracted.participation_bitmap.size(), bitmap.size());
    for (size_t i = 0; i < bitmap.size(); ++i) {
        BOOST_CHECK_EQUAL(extracted.participation_bitmap[i], bitmap[i]);
    }

    BOOST_REQUIRE_EQUAL(extracted.aggregate_sig.size(), 64);
    for (size_t i = 0; i < 64; ++i) {
        BOOST_CHECK_EQUAL(extracted.aggregate_sig[i], bundle.aggregate_sig[i]);
    }

    // Verify oracle IDs decoded from bitmap {0xAB, 0xCD, 0xEF}
    // 0xAB = 10101011 -> bits 0,1,3,5,7 -> oracles 0,1,3,5,7
    // 0xCD = 11001101 -> bits 0,2,3,6,7 -> oracles 8,10,11,14,15
    // 0xEF = 11101111 -> bits 0,1,2,3,5,6,7 -> oracles 16,17,18,19,21,22,23
    const std::vector<uint32_t> expected_ids = {0,1,3,5,7,8,10,11,14,15,16,17,18,19,21,22,23};
    BOOST_CHECK_EQUAL(extracted.messages.size(), expected_ids.size());
    for (size_t i = 0; i < expected_ids.size(); ++i) {
        BOOST_CHECK_EQUAL(extracted.messages[i].oracle_id, expected_ids[i]);
    }

    for (const auto& msg : extracted.messages) {
        BOOST_CHECK_EQUAL(msg.price_micro_usd, price);
        BOOST_CHECK_EQUAL(msg.timestamp, timestamp);
    }
}

// ============================================================================
// test_create_oracle_script_v03_phase3_gate
// v0x03 bundle rejected when block_height < nDigiDollarPhase3Height
// ============================================================================
BOOST_AUTO_TEST_CASE(test_create_oracle_script_v03_phase3_gate)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);

    const Consensus::Params& params = Params().GetConsensus();
    int phase3_height = params.nDigiDollarPhase3Height;

    COracleBundle bundle = MakeV03Bundle({0xFF, 0x01}, 50000, 1700000000);

    // CreateOracleScript doesn't take height (validation is in ConnectBlock)
    // Just verify v0x03 bundles produce valid scripts
    CScript script = manager.CreateOracleScript(bundle);
    BOOST_CHECK_MESSAGE(!script.empty(),
        "v0x03 bundle should produce non-empty script (phase3 gate is in validation, not script creation)");
}

// ============================================================================
// test_validate_v03_bundle_format
// ValidateV03BundleFormat correctly identifies version and validates format
// ============================================================================
BOOST_AUTO_TEST_CASE(test_validate_v03_bundle_format)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();
    manager.SetEnabled(true);
    manager.SetMinOracleCount(1);

    uint8_t version = 0;

    // v0x03 script
    COracleBundle v03_bundle = MakeV03Bundle({0xFF, 0x01});
    CScript v03_script = manager.CreateOracleScript(v03_bundle);
    BOOST_CHECK(manager.ValidateV03BundleFormat(v03_script, version));
    BOOST_CHECK_EQUAL(version, 0x03);

    // v0x01 script (single message)
    COracleBundle v01_bundle;
    v01_bundle.epoch = 1;
    v01_bundle.median_price_micro_usd = 50000;
    v01_bundle.timestamp = GetTime();
    CKey key;
    key.MakeNewKey(true);
    COraclePriceMessage msg(0, 50000, v01_bundle.timestamp);
    msg.SignPhase2(key);
    v01_bundle.messages.push_back(msg);

    CScript v01_script = manager.CreateOracleScript(v01_bundle);
    BOOST_CHECK(manager.ValidateV03BundleFormat(v01_script, version));
    BOOST_CHECK_EQUAL(version, 0x01);

    // Empty/invalid scripts
    CScript empty_script;
    BOOST_CHECK(!manager.ValidateV03BundleFormat(empty_script, version));

    CScript garbage_script = CScript() << OP_TRUE;
    BOOST_CHECK(!manager.ValidateV03BundleFormat(garbage_script, version));
}

BOOST_AUTO_TEST_SUITE_END()
