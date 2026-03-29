// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/musig2_aggregator.h>

#include <chainparams.h>
#include <hash.h>
#include <primitives/oracle.h>
#include <pubkey.h>

#include <algorithm>
#include <cassert>

MuSig2OracleAggregator::MuSig2OracleAggregator()
{
    m_ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    assert(m_ctx != nullptr);
}

MuSig2OracleAggregator::~MuSig2OracleAggregator()
{
    if (m_ctx) {
        secp256k1_context_destroy(m_ctx);
        m_ctx = nullptr;
    }
}

// ============================================================================
// Bitmap Operations
// ============================================================================

std::vector<unsigned char> MuSig2OracleAggregator::EncodeBitmap(
    const std::vector<uint8_t>& oracle_ids, uint16_t total_oracles)
{
    if (oracle_ids.empty()) return {};
    if (static_cast<int>(oracle_ids.size()) < ORACLE_CONSENSUS_REQUIRED) return {};
    if (total_oracles == 0 || total_oracles > 256) return {};

    size_t num_bytes = (total_oracles + 7) / 8;
    std::vector<unsigned char> bitmap(num_bytes, 0);

    for (uint8_t id : oracle_ids) {
        if (id >= total_oracles) return {};
        size_t byte_idx = id / 8;
        uint8_t bit_mask = 1 << (id % 8);
        if (bitmap[byte_idx] & bit_mask) return {}; // reject duplicate
        bitmap[byte_idx] |= bit_mask;
    }

    return bitmap;
}

std::vector<uint8_t> MuSig2OracleAggregator::DecodeBitmap(
    const std::vector<unsigned char>& bitmap, uint16_t total_oracles)
{
    if (bitmap.empty()) return {};
    if (total_oracles == 0 || total_oracles > 256) return {};

    size_t expected_bytes = (total_oracles + 7) / 8;
    if (bitmap.size() != expected_bytes) return {};

    std::vector<uint8_t> oracle_ids;
    for (uint16_t i = 0; i < total_oracles; ++i) {
        if (bitmap[i / 8] & (1 << (i % 8))) {
            oracle_ids.push_back(static_cast<uint8_t>(i));
        }
    }

    return oracle_ids;
}

// ============================================================================
// Key Aggregation
// ============================================================================

bool MuSig2OracleAggregator::ComputeAggregatePubkey(
    const std::vector<uint8_t>& oracle_ids,
    secp256k1_xonly_pubkey& agg_pk,
    secp256k1_musig_keyagg_cache& cache)
{
    // Sort and deduplicate oracle IDs for deterministic aggregation
    std::vector<uint8_t> sorted_ids = oracle_ids;
    std::sort(sorted_ids.begin(), sorted_ids.end());
    sorted_ids.erase(std::unique(sorted_ids.begin(), sorted_ids.end()), sorted_ids.end());

    const auto& nodes = Params().GetOracleNodes();
    uint16_t total = static_cast<uint16_t>(nodes.size());

    // Encode bitmap for caching
    auto bitmap = EncodeBitmap(sorted_ids, total);
    if (bitmap.empty()) return false;

    // Check cache first
    uint256 hash = ComputeBitmapHash(bitmap);
    {
        LOCK(m_cache_mutex);
        auto it = m_cache.find(hash);
        if (it != m_cache.end()) {
            agg_pk = it->second.first;
            cache = it->second.second;
            return true;
        }
    }

    // Look up and parse compressed pubkeys from chainparams
    std::vector<secp256k1_pubkey> pubkeys;
    pubkeys.reserve(sorted_ids.size());
    for (uint8_t id : sorted_ids) {
        if (id >= nodes.size()) return false;
        const CPubKey& cpk = nodes[id].pubkey;
        if (!cpk.IsValid()) return false;

        secp256k1_pubkey pk;
        if (!secp256k1_ec_pubkey_parse(m_ctx, &pk, cpk.data(), cpk.size())) {
            return false;
        }
        pubkeys.push_back(pk);
    }

    // Build pointer array for secp256k1_musig_pubkey_agg
    std::vector<const secp256k1_pubkey*> pubkey_ptrs(pubkeys.size());
    for (size_t i = 0; i < pubkeys.size(); ++i) {
        pubkey_ptrs[i] = &pubkeys[i];
    }

    // BIP-327 key aggregation via secp256k1 MuSig2 module
    if (!secp256k1_musig_pubkey_agg(m_ctx, &agg_pk, &cache,
                                     pubkey_ptrs.data(), pubkey_ptrs.size())) {
        return false;
    }

    // Store in cache
    {
        LOCK(m_cache_mutex);
        m_cache[hash] = {agg_pk, cache};
    }

    return true;
}

bool MuSig2OracleAggregator::ComputeAggregatePubkeyFromBitmap(
    const std::vector<unsigned char>& bitmap,
    uint16_t total_oracles,
    secp256k1_xonly_pubkey& agg_pk,
    secp256k1_musig_keyagg_cache& cache)
{
    auto oracle_ids = DecodeBitmap(bitmap, total_oracles);
    if (oracle_ids.empty()) return false;
    return ComputeAggregatePubkey(oracle_ids, agg_pk, cache);
}

bool MuSig2OracleAggregator::AggregatePubkeys(
    const secp256k1_pubkey* const* pubkeys,
    size_t n_pubkeys,
    secp256k1_xonly_pubkey& agg_pk,
    secp256k1_musig_keyagg_cache& cache)
{
    if (n_pubkeys == 0 || pubkeys == nullptr) return false;
    return secp256k1_musig_pubkey_agg(m_ctx, &agg_pk, &cache, pubkeys, n_pubkeys) == 1;
}

bool MuSig2OracleAggregator::GetCachedAggregatePubkey(
    const std::vector<unsigned char>& bitmap,
    secp256k1_xonly_pubkey& agg_pk)
{
    uint256 hash = ComputeBitmapHash(bitmap);
    LOCK(m_cache_mutex);
    auto it = m_cache.find(hash);
    if (it == m_cache.end()) return false;
    agg_pk = it->second.first;
    return true;
}

void MuSig2OracleAggregator::ClearCache()
{
    LOCK(m_cache_mutex);
    m_cache.clear();
}

uint256 MuSig2OracleAggregator::ComputeBitmapHash(const std::vector<unsigned char>& bitmap)
{
    return Hash(bitmap);
}
