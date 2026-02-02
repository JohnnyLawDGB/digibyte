// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/oracle.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <mutex>
#include <numeric>
#include <set>

#include <chainparams.h>
#include <hash.h>
#include <logging.h>
#include <protocol.h>
#include <util/time.h>

/**
 * COraclePriceMessage Implementation
 */

COraclePriceMessage::COraclePriceMessage(uint32_t oracle_id_in, uint64_t price_in, int64_t timestamp_in)
    : oracle_id(oracle_id_in), price_micro_usd(price_in), timestamp(timestamp_in)
{
}

bool COraclePriceMessage::IsValid(int64_t reference_time) const
{
    // Check price is positive and in reasonable range
    // price_micro_usd format: 1,000,000 micro-USD = $1.00
    // Uses shared constants from oracle.h: ORACLE_MIN/MAX_PRICE_MICRO_USD
    if (price_micro_usd < ORACLE_MIN_PRICE_MICRO_USD) return false;
    if (price_micro_usd > ORACLE_MAX_PRICE_MICRO_USD) return false;

    // Use provided reference time (block time during validation) or current time
    int64_t current_time = (reference_time > 0) ? reference_time : GetTime();

    // Check timestamp is not in the future (with 1 minute tolerance for clock skew)
    if (timestamp > current_time + 60) return false;

    // Check timestamp is not too old (1 hour max)
    if (timestamp < current_time - ORACLE_MAX_AGE_SECONDS) return false;

    // Verify Schnorr signature (skip for Phase One compact format)
    // Compact format messages don't have embedded signatures
    if (!schnorr_sig.empty()) {
        // Try Phase 2 verification first (signs only oracle_id + price + timestamp)
        // Phase 2 messages don't include block_height/nonce in their signature hash
        if (VerifyPhase2()) {
            return true;
        }
        // Fall back to Phase 1 full verification (includes block_height + nonce)
        return Verify();
    }

    // Compact format: Trust based on chainparams oracle pubkey (verified at extraction)
    return true;
}

bool COraclePriceMessage::Sign(const CKey& key, const uint256* merkle_root, const uint256& aux)
{
    // Get message hash
    uint256 hash = GetSignatureHash();

    // Create Schnorr signature (64 bytes)
    schnorr_sig.resize(64);
    if (!key.SignSchnorr(hash, schnorr_sig, merkle_root, aux)) {
        schnorr_sig.clear();
        return false;
    }

    // Set oracle pubkey from private key
    oracle_pubkey = XOnlyPubKey(key.GetPubKey());

    return true;
}

bool COraclePriceMessage::Verify() const
{
    // Check signature size
    if (schnorr_sig.size() != 64) {
        return false;
    }

    // Check pubkey is valid
    if (!oracle_pubkey.IsFullyValid()) {
        return false;
    }

    // Verify Schnorr signature
    uint256 hash = GetSignatureHash();
    return oracle_pubkey.VerifySchnorr(hash, schnorr_sig);
}

bool COraclePriceMessage::CheckForConflictingMessages(const std::vector<COraclePriceMessage>& messages)
{
    if (messages.empty()) return true; // Empty list has no conflicts

    std::map<uint32_t, const COraclePriceMessage*> oracle_messages;

    for (const auto& msg : messages) {
        // Validate message first
        if (!msg.IsValid()) {
            LogPrint(BCLog::DIGIDOLLAR, "CheckForConflictingMessages: Invalid message from oracle %u\n", msg.oracle_id);
            continue; // Skip invalid messages
        }

        auto it = oracle_messages.find(msg.oracle_id);
        if (it != oracle_messages.end()) {
            // Found conflict - same oracle has multiple messages
            const COraclePriceMessage* existing = it->second;

            // Check if they're actually different (not just duplicate)
            if (existing->price_micro_usd != msg.price_micro_usd ||
                existing->timestamp != msg.timestamp) {
                LogPrint(BCLog::DIGIDOLLAR, "CheckForConflictingMessages: Conflict detected for oracle %u: "
                         "existing price %llu micro-USD (time %d) vs new price %llu micro-USD (time %d)\n",
                         msg.oracle_id, existing->price_micro_usd, existing->timestamp,
                         msg.price_micro_usd, msg.timestamp);
                return false; // Conflicting messages found
            }
            // If messages are identical, it's just a duplicate - continue
        } else {
            oracle_messages[msg.oracle_id] = &msg;
        }
    }

    return true; // No conflicts found
}

uint256 COraclePriceMessage::GetSignatureHash() const
{
    // Create deterministic hash for signing (BIP-340 compatible)
    // Hash all message fields EXCEPT signature and pubkey
    CHashWriter ss(0);
    ss << oracle_id;
    ss << price_micro_usd;
    ss << timestamp;
    ss << block_height;
    ss << nonce;

    return ss.GetHash();
}

uint256 COraclePriceMessage::GetPhase2SignatureHash() const
{
    // Phase 2 signature hash: only consensus-critical fields
    // block_height and nonce are NOT stored on-chain in Phase 2 format
    CHashWriter ss(0);
    ss << oracle_id;
    ss << price_micro_usd;
    ss << timestamp;
    return ss.GetHash();
}

bool COraclePriceMessage::SignPhase2(const CKey& key)
{
    uint256 hash = GetPhase2SignatureHash();
    schnorr_sig.resize(64);
    if (!key.SignSchnorr(hash, schnorr_sig, nullptr, uint256())) {
        schnorr_sig.clear();
        return false;
    }
    oracle_pubkey = XOnlyPubKey(key.GetPubKey());
    return true;
}

bool COraclePriceMessage::VerifyPhase2() const
{
    if (schnorr_sig.size() != 64) return false;
    if (!oracle_pubkey.IsFullyValid()) return false;
    uint256 hash = GetPhase2SignatureHash();
    return oracle_pubkey.VerifySchnorr(hash, schnorr_sig);
}

bool operator==(const COraclePriceMessage& a, const COraclePriceMessage& b)
{
    return a.oracle_id == b.oracle_id &&
           a.price_micro_usd == b.price_micro_usd &&
           a.timestamp == b.timestamp &&
           a.block_height == b.block_height &&
           a.nonce == b.nonce &&
           a.oracle_pubkey == b.oracle_pubkey &&
           a.schnorr_sig == b.schnorr_sig;
}

bool operator!=(const COraclePriceMessage& a, const COraclePriceMessage& b)
{
    return !(a == b);
}

/**
 * COracleBundle Implementation
 */

COracleBundle::COracleBundle(int32_t epoch_in) : epoch(epoch_in)
{
}

bool COracleBundle::IsValid(int64_t reference_time, int min_required) const
{
    // Check if bundle has messages
    if (messages.empty()) {
        return false;
    }

    // Use provided reference time (block time during validation) or current time
    int64_t current_time = (reference_time > 0) ? reference_time : GetTime();

    // Verify all message signatures (skip for Phase One compact format)
    for (const auto& msg : messages) {
        // Phase One compact format: Signature not embedded (verified at creation time)
        // Trust is based on chainparams oracle pubkey validation
        if (!msg.schnorr_sig.empty()) {
            // Full format with embedded signature - verify it
            if (!msg.VerifyPhase2()) {
                LogPrint(BCLog::DIGIDOLLAR, "Oracle: Message signature verification failed for oracle %u\n", msg.oracle_id);
                return false;
            }
        }

        // Basic message validation (price range, timestamp, etc)
        if (!msg.IsValid(reference_time)) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Message validation failed for oracle %u\n", msg.oracle_id);
            return false;
        }
    }

    // Verify timestamp is reasonable (within 1 hour of reference time)
    if (timestamp > current_time + 3600 || timestamp < current_time - 3600) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Bundle timestamp out of range: %d (current: %d)\n", timestamp, current_time);
        return false;
    }

    // Verify median price matches calculated consensus price (if consensus achieved)
    if (HasConsensus(min_required)) {
        uint64_t calculated_median = GetConsensusPrice(min_required);
        if (median_price_micro_usd != calculated_median) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Median price mismatch: bundle=%llu, calculated=%llu\n",
                     median_price_micro_usd, calculated_median);
            return false;
        }
    }

    return true;
}

bool COracleBundle::AddMessage(const COraclePriceMessage& message)
{
    // Don't allow more than 15 messages (max active oracles)
    if (messages.size() >= ORACLE_ACTIVE_COUNT) return false;

    // Check if oracle already submitted a message
    for (const auto& existing : messages) {
        if (existing.oracle_id == message.oracle_id) return false;
    }

    // Note: Timestamp/price validation should be done at P2P layer via ValidateIncomingMessage()
    // AddMessage() only checks structural constraints (no duplicates, size limits)

    messages.push_back(message);
    return true;
}

bool COracleBundle::HasConsensus(int min_required) const
{
    // Check if we have enough valid messages for consensus
    return messages.size() >= static_cast<size_t>(min_required);
}

uint64_t COracleBundle::GetConsensusPrice(int min_required) const
{
    if (!HasConsensus(min_required)) return 0;

    // Filter outliers first
    std::vector<COraclePriceMessage> filtered = FilterOutliers();
    if (filtered.empty()) return 0;

    // Extract prices and sort them
    std::vector<uint64_t> prices;
    prices.reserve(filtered.size());
    for (const auto& msg : filtered) {
        prices.push_back(msg.price_micro_usd);
    }
    std::sort(prices.begin(), prices.end());

    // Calculate median
    size_t size = prices.size();
    if (size % 2 == 0) {
        // Even number of elements - average of middle two
        return (prices[size/2 - 1] + prices[size/2]) / 2;
    } else {
        // Odd number of elements - middle element
        return prices[size/2];
    }
}

bool COracleBundle::ValidateEpoch(int32_t current_epoch) const
{
    // Accept current epoch or previous epoch only
    return epoch == current_epoch || epoch == current_epoch - 1;
}

std::vector<COraclePriceMessage> COracleBundle::FilterOutliers() const
{
    if (messages.size() < 3) {
        // With fewer than 3 messages, can't filter outliers effectively
        return messages;
    }

    // Calculate initial median
    std::vector<uint64_t> prices;
    for (const auto& msg : messages) {
        prices.push_back(msg.price_micro_usd);
    }
    std::sort(prices.begin(), prices.end());

    uint64_t median;
    size_t size = prices.size();
    if (size % 2 == 0) {
        median = (prices[size/2 - 1] + prices[size/2]) / 2;
    } else {
        median = prices[size/2];
    }

    // Filter messages that are within 10% of median
    std::vector<COraclePriceMessage> filtered;
    uint64_t threshold = median * ORACLE_OUTLIER_THRESHOLD_PCT / 100;

    for (const auto& msg : messages) {
        uint64_t deviation = (msg.price_micro_usd > median) ?
                             (msg.price_micro_usd - median) :
                             (median - msg.price_micro_usd);
        if (deviation <= threshold) {
            filtered.push_back(msg);
        }
    }

    return filtered;
}

std::vector<COraclePriceMessage> COracleBundle::FilterOutliersAdvanced() const
{
    const size_t MIN_MESSAGES_FOR_FILTERING = 3;
    if (messages.size() < MIN_MESSAGES_FOR_FILTERING) {
        return messages;
    }

    std::vector<COraclePriceMessage> valid_messages;
    valid_messages.reserve(messages.size()); // Optimize memory allocation

    // First pass: remove obviously invalid prices
    // Uses shared constants from oracle.h (BUG #1 FIX: was $10, now $100)
    for (const auto& msg : messages) {
        // Extreme bounds checking with early exit conditions
        if (msg.price_micro_usd == 0 ||
            msg.price_micro_usd > ORACLE_MAX_PRICE_MICRO_USD ||
            msg.price_micro_usd < ORACLE_MIN_PRICE_MICRO_USD) {
            LogPrint(BCLog::DIGIDOLLAR, "FilterOutliersAdvanced: Rejecting price %llu micro-USD from oracle %u (out of bounds)\n",
                     msg.price_micro_usd, msg.oracle_id);
            continue;
        }

        valid_messages.push_back(msg);
    }

    if (valid_messages.size() < MIN_MESSAGES_FOR_FILTERING) {
        LogPrint(BCLog::DIGIDOLLAR, "FilterOutliersAdvanced: Insufficient valid messages (%d), returning all\n",
                 valid_messages.size());
        return valid_messages;
    }

    // Second pass: statistical outlier removal using modified Z-score
    std::vector<uint64_t> prices;
    for (const auto& msg : valid_messages) {
        prices.push_back(msg.price_micro_usd);
    }
    std::sort(prices.begin(), prices.end());

    // Calculate median
    uint64_t median;
    size_t size = prices.size();
    if (size % 2 == 0) {
        median = (prices[size/2 - 1] + prices[size/2]) / 2;
    } else {
        median = prices[size/2];
    }

    // Calculate MAD (Median Absolute Deviation)
    std::vector<uint64_t> deviations;
    for (uint64_t price : prices) {
        uint64_t deviation = (price > median) ? (price - median) : (median - price);
        deviations.push_back(deviation);
    }
    std::sort(deviations.begin(), deviations.end());

    uint64_t mad;
    if (deviations.size() % 2 == 0) {
        mad = (deviations[deviations.size()/2 - 1] + deviations[deviations.size()/2]) / 2;
    } else {
        mad = deviations[deviations.size()/2];
    }

    // Filter using modified Z-score (threshold: 3.5)
    std::vector<COraclePriceMessage> final_filtered;
    const uint64_t threshold_multiplier = 35; // 3.5 * 10 for integer math
    const uint64_t mad_multiplier = 10;

    for (const auto& msg : valid_messages) {
        if (mad == 0) {
            // All values are identical - include all
            final_filtered.push_back(msg);
        } else {
            uint64_t deviation = (msg.price_micro_usd > median) ?
                                 (msg.price_micro_usd - median) :
                                 (median - msg.price_micro_usd);
            uint64_t modified_zscore = (deviation * mad_multiplier) / mad;
            if (modified_zscore <= threshold_multiplier) {
                final_filtered.push_back(msg);
            }
        }
    }

    return final_filtered;
}

std::vector<COraclePriceMessage> COracleBundle::FilterOutliersIQR() const
{
    if (messages.size() < 3) {
        return messages;
    }

    // Extract and sort prices
    std::vector<std::pair<uint64_t, size_t>> price_indices;
    for (size_t i = 0; i < messages.size(); i++) {
        price_indices.push_back({messages[i].price_micro_usd, i});
    }
    std::sort(price_indices.begin(), price_indices.end());

    // Calculate quartiles
    size_t n = price_indices.size();
    size_t q1_index = n / 4;
    size_t q3_index = 3 * n / 4;

    uint64_t q1 = price_indices[q1_index].first;
    uint64_t q3 = price_indices[q3_index].first;
    uint64_t iqr = q3 - q1;

    // IQR bounds (1.5 * IQR rule)
    uint64_t lower_bound = (q1 > (iqr * 3 / 2)) ? (q1 - (iqr * 3 / 2)) : 0; // Prevent underflow
    uint64_t upper_bound = q3 + (iqr * 3 / 2);

    // Filter messages within bounds
    std::vector<COraclePriceMessage> filtered;
    for (const auto& msg : messages) {
        if (msg.price_micro_usd >= lower_bound && msg.price_micro_usd <= upper_bound) {
            filtered.push_back(msg);
        }
    }

    return filtered;
}

bool operator==(const COracleBundle& a, const COracleBundle& b)
{
    return a.epoch == b.epoch && a.messages == b.messages;
}

bool operator!=(const COracleBundle& a, const COracleBundle& b)
{
    return !(a == b);
}

/**
 * OracleNode Implementation
 */

OracleNodeInfo::OracleNodeInfo(uint32_t id_in, const CPubKey& pubkey_in, const std::string& endpoint_in, bool is_active_in)
    : id(id_in), pubkey(pubkey_in), endpoint(endpoint_in), is_active(is_active_in)
{
}

bool OracleNodeInfo::IsValid() const
{
    // Check pubkey is valid
    if (!pubkey.IsValid()) return false;

    // Check endpoint is not empty and has reasonable format
    if (endpoint.empty()) return false;

    // Basic format check: should contain a colon for host:port
    size_t colon_pos = endpoint.find(':');
    if (colon_pos == std::string::npos) return false;

    // Check port is valid (1-65535)
    std::string port_str = endpoint.substr(colon_pos + 1);
    try {
        int port = std::stoi(port_str);
        if (port < 1 || port > 65535) return false;
    } catch (...) {
        return false;
    }

    return true;
}

bool operator==(const OracleNodeInfo& a, const OracleNodeInfo& b)
{
    return a.id == b.id &&
           a.pubkey == b.pubkey &&
           a.endpoint == b.endpoint &&
           a.is_active == b.is_active;
}

bool operator!=(const OracleNodeInfo& a, const OracleNodeInfo& b)
{
    return !(a == b);
}

/**
 * Oracle Selection Functions
 */

std::vector<OracleNodeInfo> SelectOraclesForEpoch(const std::vector<OracleNodeInfo>& all_oracles, int32_t epoch)
{
    // Filter active oracles
    std::vector<OracleNodeInfo> active_oracles;
    for (const auto& oracle : all_oracles) {
        if (oracle.is_active) {
            active_oracles.push_back(oracle);
        }
    }

    // If we have 15 or fewer active oracles, return all of them
    if (active_oracles.size() <= ORACLE_ACTIVE_COUNT) {
        return active_oracles;
    }

    // Use deterministic selection based on epoch
    std::vector<OracleNodeInfo> selected;
    selected.reserve(ORACLE_ACTIVE_COUNT);

    // Note: We don't need to store the epoch seed since we'll use
    // individual oracle hashes for deterministic sorting

    // Create indices for deterministic sorting
    std::vector<size_t> indices;
    for (size_t i = 0; i < active_oracles.size(); i++) {
        indices.push_back(i);
    }

    // Sort indices by oracle scores (deterministic based on epoch)
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        HashWriter hasher_a{}, hasher_b{};
        hasher_a << epoch << active_oracles[a].id;
        hasher_b << epoch << active_oracles[b].id;
        return hasher_a.GetHash() < hasher_b.GetHash();
    });

    // Select first 15 after deterministic sorting
    for (size_t i = 0; i < indices.size() && selected.size() < ORACLE_ACTIVE_COUNT; i++) {
        selected.push_back(active_oracles[indices[i]]);
    }

    // Ensure we have exactly 15 (or all available if less)
    selected.resize(std::min(selected.size(), size_t(ORACLE_ACTIVE_COUNT)));

    return selected;
}

int32_t GetCurrentEpoch(int32_t block_height)
{
    // Get epoch length from consensus parameters
    // This varies by network: mainnet=100, testnet=50, regtest=10
    const Consensus::Params& params = Params().GetConsensus();
    int32_t epoch_length = params.nDDOracleEpochBlocks;

    if (epoch_length <= 0) {
        // Fallback to default if not set (shouldn't happen)
        epoch_length = 1440;
    }

    return block_height / epoch_length;
}

/**
 * Oracle P2P Validation Implementation
 */

namespace OracleP2P {

// Rate limiting state
static std::map<uint32_t, std::pair<int64_t, int>> rate_limit_state; // oracle_id -> (last_time, count)
static std::mutex rate_limit_mutex;

bool ValidateIncomingMessage(const COraclePriceMessage& message)
{
    // Basic validation first
    if (!message.IsValid()) return false;

    // Check oracle ID is within valid range
    if (message.oracle_id >= ORACLE_TOTAL_COUNT) return false;

    // Check message size
    if (!CheckMessageSize(message)) return false;

    // Check rate limiting
    if (!CheckRateLimit(message.oracle_id)) return false;

    return true;
}

bool ValidateBundleMessage(const COracleBundle& bundle)
{
    // Bundle cannot exceed maximum oracle count
    if (bundle.messages.size() > ORACLE_ACTIVE_COUNT) return false;

    // Each message in bundle must be valid
    for (const auto& msg : bundle.messages) {
        if (!msg.IsValid()) return false;
    }

    // Check for duplicate oracle IDs in bundle
    std::set<uint32_t> oracle_ids;
    for (const auto& msg : bundle.messages) {
        if (oracle_ids.find(msg.oracle_id) != oracle_ids.end()) {
            return false; // Duplicate oracle ID
        }
        oracle_ids.insert(msg.oracle_id);
    }

    return true;
}

bool ValidateGetOracleRequest(const GetOracleDataMsg& request)
{
    // Epoch must be non-negative
    if (request.epoch < 0) return false;

    // Oracle ID must be valid (or 0xFFFFFFFF for all)
    if (request.oracle_id != 0xFFFFFFFF && request.oracle_id >= ORACLE_TOTAL_COUNT) {
        return false;
    }

    return true;
}

bool CheckRateLimit(uint32_t oracle_id)
{
    std::lock_guard<std::mutex> lock(rate_limit_mutex);

    int64_t current_time = GetTime();
    const int max_messages_per_minute = 3;
    const int64_t time_window = 60; // seconds

    auto it = rate_limit_state.find(oracle_id);
    if (it == rate_limit_state.end()) {
        // First message from this oracle
        rate_limit_state[oracle_id] = {current_time, 1};
        return true;
    }

    int64_t last_time = it->second.first;
    int count = it->second.second;

    if (current_time - last_time > time_window) {
        // Time window expired, reset counter
        rate_limit_state[oracle_id] = {current_time, 1};
        return true;
    } else {
        // Within time window, check count
        if (count >= max_messages_per_minute) {
            return false; // Rate limited
        } else {
            rate_limit_state[oracle_id] = {last_time, count + 1};
            return true;
        }
    }
}

bool CheckMessageSize(const COraclePriceMessage& message)
{
    const size_t expected_schnorr_sig_size = 64; // BIP-340 Schnorr signature size

    // Check signature size - must be exactly 64 bytes for Schnorr
    if (message.schnorr_sig.size() != expected_schnorr_sig_size) return false;

    // Message structure is now fixed size with Schnorr signatures
    return true;
}

void UpdateRateLimits()
{
    std::lock_guard<std::mutex> lock(rate_limit_mutex);

    int64_t current_time = GetTime();
    const int64_t cleanup_age = 3600; // Clean up entries older than 1 hour

    auto it = rate_limit_state.begin();
    while (it != rate_limit_state.end()) {
        if (current_time - it->second.first > cleanup_age) {
            it = rate_limit_state.erase(it);
        } else {
            ++it;
        }
    }
}

void ClearRateLimitState()
{
    std::lock_guard<std::mutex> lock(rate_limit_mutex);
    rate_limit_state.clear();
}

} // namespace OracleP2P