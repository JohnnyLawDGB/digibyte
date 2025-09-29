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

#include <hash.h>
#include <logging.h>
#include <protocol.h>
#include <util/time.h>

/**
 * COraclePriceMessage Implementation
 */

COraclePriceMessage::COraclePriceMessage(uint32_t oracle_id_in, CAmount price_in, int64_t timestamp_in)
    : oracle_id(oracle_id_in), price_satoshis(price_in), timestamp(timestamp_in)
{
}

bool COraclePriceMessage::IsValid() const
{
    // Check price is positive and reasonable
    if (price_satoshis <= 0) return false;

    // Check price is not unrealistically high (100 DGB per USD)
    if (price_satoshis > 100000000000LL) return false;

    // Check timestamp is not in the future (with 5 minute tolerance)
    int64_t current_time = GetTime();
    if (timestamp > current_time + 300) return false;

    // Check timestamp is not too old (1 hour max)
    if (timestamp < current_time - ORACLE_MAX_AGE_SECONDS) return false;

    return true;
}

bool COraclePriceMessage::ValidateSignature(const CPubKey& oracle_pubkey) const
{
    if (signature.empty()) return false;
    if (!oracle_pubkey.IsValid()) return false;

    // Get the hash to verify
    uint256 hash = GetSignatureHash();

    // Verify ECDSA signature
    return oracle_pubkey.Verify(hash, signature);
}

bool COraclePriceMessage::ValidateSignatureWithTimestamp(const CPubKey& oracle_pubkey) const
{
    // First check basic signature validity
    if (!ValidateSignature(oracle_pubkey)) return false;

    // Then check timestamp validity (not expired)
    int64_t current_time = GetTime();
    if (timestamp < current_time - ORACLE_MAX_AGE_SECONDS) return false;

    return true;
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
            if (existing->price_satoshis != msg.price_satoshis ||
                existing->timestamp != msg.timestamp) {
                LogPrint(BCLog::DIGIDOLLAR, "CheckForConflictingMessages: Conflict detected for oracle %u: "
                         "existing price %d (time %d) vs new price %d (time %d)\n",
                         msg.oracle_id, existing->price_satoshis, existing->timestamp,
                         msg.price_satoshis, msg.timestamp);
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
    // Create deterministic hash for signing
    HashWriter ss{};
    ss << oracle_id;
    ss << price_satoshis;
    ss << timestamp;

    return ss.GetHash();
}

bool operator==(const COraclePriceMessage& a, const COraclePriceMessage& b)
{
    return a.oracle_id == b.oracle_id &&
           a.price_satoshis == b.price_satoshis &&
           a.timestamp == b.timestamp &&
           a.signature == b.signature;
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

bool COracleBundle::AddMessage(const COraclePriceMessage& message)
{
    // Don't allow more than 15 messages (max active oracles)
    if (messages.size() >= ORACLE_ACTIVE_COUNT) return false;

    // Check if oracle already submitted a message
    for (const auto& existing : messages) {
        if (existing.oracle_id == message.oracle_id) return false;
    }

    // Validate the message
    if (!message.IsValid()) return false;

    messages.push_back(message);
    return true;
}

bool COracleBundle::HasConsensus() const
{
    // Need at least 8 valid messages for consensus
    return messages.size() >= ORACLE_CONSENSUS_REQUIRED;
}

CAmount COracleBundle::GetConsensusPrice() const
{
    if (!HasConsensus()) return 0;

    // Filter outliers first
    std::vector<COraclePriceMessage> filtered = FilterOutliers();
    if (filtered.empty()) return 0;

    // Extract prices and sort them
    std::vector<CAmount> prices;
    prices.reserve(filtered.size());
    for (const auto& msg : filtered) {
        prices.push_back(msg.price_satoshis);
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
    std::vector<CAmount> prices;
    for (const auto& msg : messages) {
        prices.push_back(msg.price_satoshis);
    }
    std::sort(prices.begin(), prices.end());

    CAmount median;
    size_t size = prices.size();
    if (size % 2 == 0) {
        median = (prices[size/2 - 1] + prices[size/2]) / 2;
    } else {
        median = prices[size/2];
    }

    // Filter messages that are within 10% of median
    std::vector<COraclePriceMessage> filtered;
    CAmount threshold = median * ORACLE_OUTLIER_THRESHOLD_PCT / 100;

    for (const auto& msg : messages) {
        CAmount deviation = std::abs(msg.price_satoshis - median);
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

    // First pass: remove obviously invalid prices with defined bounds
    static constexpr CAmount MIN_REALISTIC_PRICE = 1000;        // $0.00001 per DGB
    static constexpr CAmount MAX_REALISTIC_PRICE = 10000000000LL; // $100 per DGB

    for (const auto& msg : messages) {
        // Extreme bounds checking with early exit conditions
        if (msg.price_satoshis <= 0 ||
            msg.price_satoshis > MAX_REALISTIC_PRICE ||
            msg.price_satoshis < MIN_REALISTIC_PRICE) {
            LogPrint(BCLog::DIGIDOLLAR, "FilterOutliersAdvanced: Rejecting price %d from oracle %u (out of bounds)\n",
                     msg.price_satoshis, msg.oracle_id);
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
    std::vector<CAmount> prices;
    for (const auto& msg : valid_messages) {
        prices.push_back(msg.price_satoshis);
    }
    std::sort(prices.begin(), prices.end());

    // Calculate median
    CAmount median;
    size_t size = prices.size();
    if (size % 2 == 0) {
        median = (prices[size/2 - 1] + prices[size/2]) / 2;
    } else {
        median = prices[size/2];
    }

    // Calculate MAD (Median Absolute Deviation)
    std::vector<CAmount> deviations;
    for (CAmount price : prices) {
        deviations.push_back(std::abs(price - median));
    }
    std::sort(deviations.begin(), deviations.end());

    CAmount mad;
    if (deviations.size() % 2 == 0) {
        mad = (deviations[deviations.size()/2 - 1] + deviations[deviations.size()/2]) / 2;
    } else {
        mad = deviations[deviations.size()/2];
    }

    // Filter using modified Z-score (threshold: 3.5)
    std::vector<COraclePriceMessage> final_filtered;
    const CAmount threshold_multiplier = 35; // 3.5 * 10 for integer math
    const CAmount mad_multiplier = 10;

    for (const auto& msg : valid_messages) {
        if (mad == 0) {
            // All values are identical - include all
            final_filtered.push_back(msg);
        } else {
            CAmount modified_zscore = (std::abs(msg.price_satoshis - median) * mad_multiplier) / mad;
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
    std::vector<std::pair<CAmount, size_t>> price_indices;
    for (size_t i = 0; i < messages.size(); i++) {
        price_indices.push_back({messages[i].price_satoshis, i});
    }
    std::sort(price_indices.begin(), price_indices.end());

    // Calculate quartiles
    size_t n = price_indices.size();
    size_t q1_index = n / 4;
    size_t q3_index = 3 * n / 4;

    CAmount q1 = price_indices[q1_index].first;
    CAmount q3 = price_indices[q3_index].first;
    CAmount iqr = q3 - q1;

    // IQR bounds (1.5 * IQR rule)
    CAmount lower_bound = q1 - (iqr * 3 / 2); // 1.5 * IQR
    CAmount upper_bound = q3 + (iqr * 3 / 2);

    // Filter messages within bounds
    std::vector<COraclePriceMessage> filtered;
    for (const auto& msg : messages) {
        if (msg.price_satoshis >= lower_bound && msg.price_satoshis <= upper_bound) {
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
    // Oracle epochs change every 1440 blocks (approximately 6 hours at 15s blocks)
    // This gives oracles time to coordinate and submit prices
    const int32_t BLOCKS_PER_EPOCH = 1440;
    return block_height / BLOCKS_PER_EPOCH;
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
    const size_t max_signature_size = 1000; // Reasonable limit for ECDSA signature

    // Check signature size
    if (message.signature.size() > max_signature_size) return false;

    // Message structure is fixed size except for signature, so this is sufficient
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