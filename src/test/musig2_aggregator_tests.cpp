// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <oracle/musig2_aggregator.h>
#include <primitives/oracle.h>
#include <chainparams.h>
#include <hash.h>
#include <key.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>
#include <set>
#include <vector>

#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_musig.h>

namespace {

//! Serialize an x-only pubkey to a 32-byte array for deterministic comparison.
std::array<unsigned char, 32> SerializeXOnly(const secp256k1_xonly_pubkey& pk)
{
    std::array<unsigned char, 32> buf{};
    secp256k1_xonly_pubkey_serialize(secp256k1_context_static, buf.data(), &pk);
    return buf;
}

} // anonymous namespace

// Use testnet fixture: 15 oracles, 9-of-15 consensus
struct TestnetSetup : public BasicTestingSetup {
    TestnetSetup() : BasicTestingSetup(ChainType::TESTNET) {}
};

BOOST_FIXTURE_TEST_SUITE(musig2_aggregator_tests, TestnetSetup)

// ============================================================================
// Bitmap Encoding/Decoding Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_bitmap_encode_decode_9_of_15)
{
    std::vector<uint8_t> oracle_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    uint16_t total = 15;

    auto bitmap = MuSig2OracleAggregator::EncodeBitmap(oracle_ids, total);
    BOOST_REQUIRE(!bitmap.empty());
    BOOST_CHECK_EQUAL(bitmap.size(), 2u); // ceil(15/8) = 2 bytes

    // Verify bit pattern: bits 0-8 set
    BOOST_CHECK_EQUAL(bitmap[0], 0xFF); // oracles 0-7
    BOOST_CHECK_EQUAL(bitmap[1], 0x01); // oracle 8

    auto decoded = MuSig2OracleAggregator::DecodeBitmap(bitmap, total);
    BOOST_CHECK_EQUAL(decoded.size(), oracle_ids.size());
    BOOST_CHECK(decoded == oracle_ids);
}

BOOST_AUTO_TEST_CASE(test_bitmap_variable_length_30_oracles)
{
    // Select 10 oracles spread across 30 slots
    std::vector<uint8_t> oracle_ids = {0, 3, 7, 10, 15, 18, 22, 25, 27, 29};
    uint16_t total = 30;

    auto bitmap = MuSig2OracleAggregator::EncodeBitmap(oracle_ids, total);
    BOOST_REQUIRE(!bitmap.empty());
    BOOST_CHECK_EQUAL(bitmap.size(), 4u); // ceil(30/8) = 4 bytes

    auto decoded = MuSig2OracleAggregator::DecodeBitmap(bitmap, total);
    BOOST_CHECK(decoded == oracle_ids);
}

BOOST_AUTO_TEST_CASE(test_bitmap_variable_length_256_oracles)
{
    // Select 12 oracles spread across all 256 slots
    std::vector<uint8_t> oracle_ids = {0, 15, 31, 63, 100, 127, 128, 150, 200, 240, 250, 255};
    uint16_t total = 256;

    auto bitmap = MuSig2OracleAggregator::EncodeBitmap(oracle_ids, total);
    BOOST_REQUIRE(!bitmap.empty());
    BOOST_CHECK_EQUAL(bitmap.size(), 32u); // ceil(256/8) = 32 bytes

    auto decoded = MuSig2OracleAggregator::DecodeBitmap(bitmap, total);
    BOOST_CHECK(decoded == oracle_ids);
}

BOOST_AUTO_TEST_CASE(test_bitmap_invalid_empty)
{
    std::vector<uint8_t> empty_ids;
    auto bitmap = MuSig2OracleAggregator::EncodeBitmap(empty_ids, 15);
    BOOST_CHECK(bitmap.empty());
}

BOOST_AUTO_TEST_CASE(test_bitmap_invalid_below_threshold)
{
    // 7 oracles < ORACLE_CONSENSUS_REQUIRED (8) — must be rejected
    std::vector<uint8_t> oracle_ids = {0, 1, 2, 3, 4, 5, 6};
    auto bitmap = MuSig2OracleAggregator::EncodeBitmap(oracle_ids, 15);
    BOOST_CHECK(bitmap.empty());
}

// ============================================================================
// Key Aggregation Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_aggregate_pubkey_deterministic)
{
    MuSig2OracleAggregator agg;

    std::vector<uint8_t> oracle_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    secp256k1_xonly_pubkey pk1{}, pk2{};
    secp256k1_musig_keyagg_cache cache1{}, cache2{};

    BOOST_REQUIRE(agg.ComputeAggregatePubkey(oracle_ids, pk1, cache1));

    // Clear cache to force full recomputation
    agg.ClearCache();

    BOOST_REQUIRE(agg.ComputeAggregatePubkey(oracle_ids, pk2, cache2));

    // Same inputs must always produce the same aggregate pubkey
    BOOST_CHECK(SerializeXOnly(pk1) == SerializeXOnly(pk2));
}

BOOST_AUTO_TEST_CASE(test_aggregate_pubkey_different_subsets)
{
    MuSig2OracleAggregator agg;

    std::vector<uint8_t> set_a = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint8_t> set_b = {0, 1, 2, 3, 4, 5, 6, 7, 9}; // oracle 9 instead of 8

    secp256k1_xonly_pubkey pk_a{}, pk_b{};
    secp256k1_musig_keyagg_cache cache_a{}, cache_b{};

    BOOST_REQUIRE(agg.ComputeAggregatePubkey(set_a, pk_a, cache_a));
    BOOST_REQUIRE(agg.ComputeAggregatePubkey(set_b, pk_b, cache_b));

    // Different oracle subsets must produce different aggregate pubkeys
    BOOST_CHECK(SerializeXOnly(pk_a) != SerializeXOnly(pk_b));
}

BOOST_AUTO_TEST_CASE(test_aggregate_pubkey_order_independent)
{
    MuSig2OracleAggregator agg;

    std::vector<uint8_t> ascending  = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint8_t> descending = {8, 7, 6, 5, 4, 3, 2, 1, 0};

    secp256k1_xonly_pubkey pk1{}, pk2{};
    secp256k1_musig_keyagg_cache cache1{}, cache2{};

    BOOST_REQUIRE(agg.ComputeAggregatePubkey(ascending, pk1, cache1));

    agg.ClearCache(); // force recomputation

    BOOST_REQUIRE(agg.ComputeAggregatePubkey(descending, pk2, cache2));

    // Internal sorting guarantees order independence
    BOOST_CHECK(SerializeXOnly(pk1) == SerializeXOnly(pk2));
}

BOOST_AUTO_TEST_CASE(test_aggregate_pubkey_cache)
{
    MuSig2OracleAggregator agg;

    std::vector<uint8_t> oracle_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    secp256k1_xonly_pubkey pk_computed{}, pk_cached{};
    secp256k1_musig_keyagg_cache cache{};

    // First call computes and caches
    BOOST_REQUIRE(agg.ComputeAggregatePubkey(oracle_ids, pk_computed, cache));

    // Build the bitmap that ComputeAggregatePubkey used internally
    const auto& nodes = Params().GetOracleNodes();
    auto bitmap = MuSig2OracleAggregator::EncodeBitmap(oracle_ids, static_cast<uint16_t>(nodes.size()));
    BOOST_REQUIRE(!bitmap.empty());

    // Cache lookup should succeed
    BOOST_REQUIRE(agg.GetCachedAggregatePubkey(bitmap, pk_cached));

    // Cached key must match computed key
    BOOST_CHECK(SerializeXOnly(pk_computed) == SerializeXOnly(pk_cached));

    // After clearing cache, lookup must fail
    agg.ClearCache();
    BOOST_CHECK(!agg.GetCachedAggregatePubkey(bitmap, pk_cached));
}

BOOST_AUTO_TEST_CASE(test_all_5005_subsets_9_of_15)
{
    MuSig2OracleAggregator agg;
    std::set<std::array<unsigned char, 32>> unique_keys;

    // Generate 15 deterministic keypairs (testnet has 2 invalid placeholder keys,
    // so we generate our own to guarantee all 15 are valid EC points).
    constexpr size_t N = 15;
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    std::vector<secp256k1_pubkey> all_pubkeys(N);

    for (size_t i = 0; i < N; ++i) {
        // Deterministic secret key from hash of index
        uint256 seed = Hash(std::to_string(i));
        secp256k1_keypair kp;
        BOOST_REQUIRE(secp256k1_keypair_create(ctx, &kp, seed.data()));
        BOOST_REQUIRE(secp256k1_keypair_pub(ctx, &all_pubkeys[i], &kp));
    }

    // Generate all C(15,9) = 5005 combinations.
    std::vector<int> selector(N, 0);
    std::fill(selector.end() - 9, selector.end(), 1);

    int count = 0;
    do {
        // Build pointer array for this subset
        std::vector<const secp256k1_pubkey*> subset_ptrs;
        for (size_t i = 0; i < N; ++i) {
            if (selector[i]) subset_ptrs.push_back(&all_pubkeys[i]);
        }
        BOOST_REQUIRE_EQUAL(subset_ptrs.size(), 9u);

        secp256k1_xonly_pubkey pk{};
        secp256k1_musig_keyagg_cache cache{};
        BOOST_REQUIRE_MESSAGE(
            agg.AggregatePubkeys(subset_ptrs.data(), subset_ptrs.size(), pk, cache),
            "Failed to aggregate subset #" + std::to_string(count));

        unique_keys.insert(SerializeXOnly(pk));
        ++count;
    } while (std::next_permutation(selector.begin(), selector.end()));

    secp256k1_context_destroy(ctx);

    // Must have visited exactly 5005 subsets
    BOOST_CHECK_EQUAL(count, 5005);

    // Every subset must produce a unique aggregate pubkey
    BOOST_CHECK_EQUAL(unique_keys.size(), 5005u);
}

BOOST_AUTO_TEST_SUITE_END()
