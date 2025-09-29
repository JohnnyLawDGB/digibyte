// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_PRIMITIVES_ORACLE_H
#define DIGIBYTE_PRIMITIVES_ORACLE_H

#include <consensus/amount.h>
#include <pubkey.h>
#include <serialize.h>
#include <uint256.h>

#include <cstdint>
#include <string>
#include <vector>

/**
 * Oracle Price Message
 * Individual price report from a single oracle node
 */
class COraclePriceMessage
{
public:
    uint32_t oracle_id{0};
    CAmount price_satoshis{0};    // Price in satoshis per USD
    int64_t timestamp{0};
    std::vector<unsigned char> signature;  // ECDSA signature

    //! Constructors
    COraclePriceMessage() = default;
    COraclePriceMessage(uint32_t oracle_id_in, CAmount price_in, int64_t timestamp_in);

    //! Serialization
    SERIALIZE_METHODS(COraclePriceMessage, obj)
    {
        READWRITE(obj.oracle_id);
        READWRITE(obj.price_satoshis);
        READWRITE(obj.timestamp);
        READWRITE(obj.signature);
    }

    //! Validation
    bool IsValid() const;
    bool ValidateSignature(const CPubKey& oracle_pubkey) const;

    /**
     * Validate signature with timestamp expiry check.
     * Combines signature validation with timestamp freshness validation.
     * @param oracle_pubkey The public key to verify the signature against
     * @return true if signature is valid and timestamp is not expired
     */
    bool ValidateSignatureWithTimestamp(const CPubKey& oracle_pubkey) const;

    //! Get hash for signature verification
    uint256 GetSignatureHash() const;

    /**
     * Check for conflicting messages from the same oracle.
     * Detects if an oracle has submitted multiple different price messages.
     * @param messages Vector of oracle messages to check
     * @return true if no conflicts found, false if conflicts detected
     */
    static bool CheckForConflictingMessages(const std::vector<COraclePriceMessage>& messages);

    //! Equality operators
    friend bool operator==(const COraclePriceMessage& a, const COraclePriceMessage& b);
    friend bool operator!=(const COraclePriceMessage& a, const COraclePriceMessage& b);
};

/**
 * Oracle Bundle
 * Collection of oracle messages for consensus calculation
 */
class COracleBundle
{
public:
    std::vector<COraclePriceMessage> messages;
    int32_t epoch{0};

    //! Constructors
    COracleBundle() = default;
    explicit COracleBundle(int32_t epoch_in);

    //! Serialization
    SERIALIZE_METHODS(COracleBundle, obj)
    {
        READWRITE(obj.messages);
        READWRITE(obj.epoch);
    }

    //! Message management
    bool AddMessage(const COraclePriceMessage& message);

    //! Consensus validation
    bool HasConsensus() const;              // Requires 8 of 15 messages
    CAmount GetConsensusPrice() const;      // Median price calculation
    bool ValidateEpoch(int32_t current_epoch) const;

    //! Outlier filtering
    /**
     * Basic outlier filtering using median +/- threshold.
     * @return Vector of messages within acceptable range of median
     */
    std::vector<COraclePriceMessage> FilterOutliers() const;

    /**
     * Advanced outlier filtering using Modified Z-Score (MAD-based).
     * More robust against extreme outliers than basic filtering.
     * @return Vector of statistically valid messages
     */
    std::vector<COraclePriceMessage> FilterOutliersAdvanced() const;

    /**
     * Outlier filtering using Interquartile Range (IQR) method.
     * Uses 1.5 * IQR rule for outlier detection.
     * @return Vector of messages within IQR bounds
     */
    std::vector<COraclePriceMessage> FilterOutliersIQR() const;

    //! Equality operators
    friend bool operator==(const COracleBundle& a, const COracleBundle& b);
    friend bool operator!=(const COracleBundle& a, const COracleBundle& b);
};

/**
 * Oracle Node Definition
 * Represents a single oracle node in the network
 */
struct OracleNodeInfo
{
    uint32_t id{0};
    CPubKey pubkey;
    std::string endpoint;
    bool is_active{false};

    //! Constructors
    OracleNodeInfo() = default;
    OracleNodeInfo(uint32_t id_in, const CPubKey& pubkey_in, const std::string& endpoint_in, bool is_active_in);

    //! Serialization
    SERIALIZE_METHODS(OracleNodeInfo, obj)
    {
        READWRITE(obj.id);
        READWRITE(obj.pubkey);
        READWRITE(obj.endpoint);
        READWRITE(obj.is_active);
    }

    //! Validation
    bool IsValid() const;

    //! Equality operators
    friend bool operator==(const OracleNodeInfo& a, const OracleNodeInfo& b);
    friend bool operator!=(const OracleNodeInfo& a, const OracleNodeInfo& b);
};

/**
 * Oracle Selection Functions
 * Deterministic oracle selection for each epoch
 */

//! Select 15 active oracles for given epoch (deterministic)
std::vector<OracleNodeInfo> SelectOraclesForEpoch(const std::vector<OracleNodeInfo>& all_oracles, int32_t epoch);

//! Get current epoch based on block height
int32_t GetCurrentEpoch(int32_t block_height);

//! Oracle consensus constants
static constexpr int ORACLE_CONSENSUS_REQUIRED = 8;     // 8 of 15 required
static constexpr int ORACLE_ACTIVE_COUNT = 15;         // 15 active oracles per epoch
static constexpr int ORACLE_TOTAL_COUNT = 30;          // 30 total hardcoded oracles
static constexpr int ORACLE_MAX_AGE_SECONDS = 3600;    // 1 hour max age for prices
static constexpr int ORACLE_OUTLIER_THRESHOLD_PCT = 10; // 10% outlier threshold

//! Forward declarations for P2P
class GetOracleDataMsg;

/**
 * Oracle P2P Validation Namespace
 * Handles validation of oracle messages over the P2P network with DOS protection
 */
namespace OracleP2P {

    //! Message validation
    /**
     * Validate incoming oracle price message from P2P network.
     * Performs comprehensive validation including basic checks, rate limiting, and size limits.
     * @param message The oracle price message to validate
     * @return true if message passes all validation checks
     */
    bool ValidateIncomingMessage(const COraclePriceMessage& message);

    /**
     * Validate oracle bundle message from P2P network.
     * Checks bundle size limits, message validity, and duplicate prevention.
     * @param bundle The oracle bundle to validate
     * @return true if bundle is valid for P2P transmission
     */
    bool ValidateBundleMessage(const COracleBundle& bundle);

    /**
     * Validate oracle data request message.
     * Ensures request parameters are within acceptable bounds.
     * @param request The oracle data request to validate
     * @return true if request is valid
     */
    bool ValidateGetOracleRequest(const GetOracleDataMsg& request);

    //! Rate limiting and DOS protection
    /**
     * Check if oracle ID is within rate limits.
     * Implements sliding window rate limiting to prevent spam.
     * @param oracle_id The oracle ID to check
     * @return true if within rate limits, false if rate limited
     */
    bool CheckRateLimit(uint32_t oracle_id);

    /**
     * Validate message size to prevent oversized messages.
     * @param message The message to check
     * @return true if message size is acceptable
     */
    bool CheckMessageSize(const COraclePriceMessage& message);

    /**
     * Update and cleanup rate limiting state.
     * Should be called periodically to clean up old entries.
     */
    void UpdateRateLimits();

    //! Internal state management
    /**
     * Clear all rate limiting state.
     * Used for testing and node restart scenarios.
     */
    void ClearRateLimitState();

} // namespace OracleP2P

#endif // DIGIBYTE_PRIMITIVES_ORACLE_H