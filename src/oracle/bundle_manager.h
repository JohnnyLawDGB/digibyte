// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_ORACLE_BUNDLE_MANAGER_H
#define DIGIBYTE_ORACLE_BUNDLE_MANAGER_H

#include <memory>
#include <mutex>
#include <unordered_map>

#include <consensus/amount.h>
#include <primitives/oracle.h>
#include <script/script.h>
#include <uint256.h>

class CBlock;
class CTransaction;
class CBlockIndex;

namespace Consensus { struct Params; }

/**
 * Oracle Bundle Manager
 * Manages oracle message collection, validation, and block integration
 */
class OracleBundleManager
{
private:
    mutable std::mutex mtx_bundles;
    mutable std::mutex mtx_messages;

    // Current oracle bundles by epoch
    std::unordered_map<int32_t, COracleBundle> epoch_bundles;

    // Individual oracle messages waiting to be bundled
    std::unordered_map<uint32_t, COraclePriceMessage> pending_messages;

    // Cached price data
    CAmount cached_price{0};
    int32_t cached_epoch{-1};
    int64_t last_update_time{0};

    // Configuration
    bool enabled{true};
    int32_t min_oracle_count{ORACLE_CONSENSUS_REQUIRED};

public:
    OracleBundleManager();
    ~OracleBundleManager();

    //! Configuration
    void SetEnabled(bool enable) { enabled = enable; }
    bool IsEnabled() const { return enabled; }
    void SetMinOracleCount(int32_t min_count) { min_oracle_count = min_count; }

    //! Message management
    bool AddOracleMessage(const COraclePriceMessage& message);
    bool RemoveOracleMessage(uint32_t oracle_id);
    std::vector<COraclePriceMessage> GetPendingMessages() const;
    size_t GetPendingMessageCount() const;

    //! Bundle management
    COracleBundle GetCurrentBundle(int32_t epoch) const;
    bool UpdateBundle(const COracleBundle& bundle);
    bool HasValidBundle(int32_t epoch) const;
    void CleanupOldBundles(int32_t current_epoch);

    //! Block integration
    bool AddOracleBundleToBlock(CBlock& block, int32_t block_height) const;
    CScript CreateOracleScript(const COracleBundle& bundle) const;
    bool ExtractOracleBundle(const CTransaction& coinbase_tx, COracleBundle& bundle) const;

    //! Price functions
    CAmount GetConsensusPrice(int32_t epoch) const;
    CAmount GetLatestPrice() const;
    bool UpdateCachedPrice(int32_t epoch);

    //! Validation
    bool ValidateOracleBundle(const COracleBundle& bundle, int32_t block_height, const Consensus::Params& params) const;
    bool ValidateOracleDataInBlock(const CBlock& block, int32_t block_height, const Consensus::Params& params) const;

    //! Network functions
    void BroadcastMessage(const COraclePriceMessage& message);
    void ProcessIncomingMessage(const COraclePriceMessage& message);

    //! Status and statistics
    struct OracleStats {
        size_t pending_messages;
        size_t active_bundles;
        CAmount latest_price;
        int32_t latest_epoch;
        int64_t last_update;
        bool has_consensus;
    };
    OracleStats GetStats() const;

    //! Singleton access
    static OracleBundleManager& GetInstance();
    static void Initialize();
    static void Shutdown();

private:
    //! Internal helpers
    bool TryCreateBundle(int32_t epoch);
    void UpdateEpochBundle(int32_t epoch);
    bool IsValidOracleMessage(const COraclePriceMessage& message) const;
    std::vector<uint32_t> GetActiveOraclesForEpoch(int32_t epoch) const;
    bool HasRequiredSignatures(const COracleBundle& bundle, int32_t block_height) const;
};

/**
 * Oracle Data Validator
 * Validates oracle data in blocks and transactions
 */
class OracleDataValidator
{
public:
    //! Block validation
    static bool ValidateBlockOracleData(const CBlock& block, const CBlockIndex* pindex_prev, const Consensus::Params& params);

    //! Transaction validation for DigiDollar operations
    static bool ValidateOraclePriceForTx(const CTransaction& tx, CAmount oracle_price, int32_t block_height);

    //! Oracle message validation
    static bool ValidateOracleMessage(const COraclePriceMessage& message, const Consensus::Params& params);

    //! Bundle validation
    static bool ValidateOracleBundle(const COracleBundle& bundle, int32_t epoch, const Consensus::Params& params);

private:
    //! Internal validation helpers
    static bool CheckOracleSignatures(const COracleBundle& bundle, const Consensus::Params& params);
    static bool CheckOracleEpoch(const COracleBundle& bundle, int32_t current_epoch);
    static bool CheckOracleConsensus(const COracleBundle& bundle);
};

//! Global oracle bundle manager instance
extern std::unique_ptr<OracleBundleManager> g_oracle_bundle_manager;

//! Utility functions for integration with existing code
namespace OracleIntegration {

    //! Get current oracle price for DigiDollar operations
    CAmount GetCurrentOraclePrice();

    //! Check if oracle system is ready
    bool IsOracleSystemReady();

    //! Get oracle data for specific epoch/height
    COracleBundle GetOracleBundleForHeight(int32_t block_height);

    //! Validate oracle requirements for DigiDollar transactions
    bool ValidateOracleRequirements(const CTransaction& tx, int32_t block_height);

} // namespace OracleIntegration

#endif // DIGIBYTE_ORACLE_BUNDLE_MANAGER_H