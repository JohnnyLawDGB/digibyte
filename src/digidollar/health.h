// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_DIGIDOLLAR_HEALTH_H
#define DIGIBYTE_DIGIDOLLAR_HEALTH_H

#include <consensus/amount.h>
#include <primitives/block.h>
#include <univalue/include/univalue.h>
#include <uint256.h>

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>

// Forward declarations
class CCoinsView;
class CTxMemPool;

namespace node {
    class BlockManager;
}

namespace wallet {
    class CWallet;
}

namespace DigiDollar {

/**
 * System-wide health metrics structure
 * Aggregates all DigiDollar system data for monitoring
 */
struct SystemMetrics {
    // Overall system metrics
    CAmount totalDDSupply;      //!< Total DigiDollar in circulation (cents)
    CAmount totalCollateral;    //!< Total DGB locked as collateral
    int systemHealth;           //!< Overall collateral ratio (percentage)

    // Per-tier breakdown structure
    struct TierMetrics {
        int lockDays;           //!< Lock period for this tier
        CAmount ddMinted;       //!< DD issued in this tier (cents)
        CAmount dgbLocked;      //!< DGB locked in this tier
        int positions;          //!< Number of active positions
        int healthRatio;        //!< Tier-specific health ratio (percentage)

        TierMetrics() : lockDays(0), ddMinted(0), dgbLocked(0), positions(0), healthRatio(0) {}
        TierMetrics(int days, CAmount dd, CAmount dgb, int pos, int health)
            : lockDays(days), ddMinted(dd), dgbLocked(dgb), positions(pos), healthRatio(health) {}
    };
    std::vector<TierMetrics> tiers;

    // Protection system status
    double dcaMultiplier;       //!< Current DCA multiplier (1.0-10.0)
    bool errActive;             //!< Emergency Redemption Ratio active
    double volatility;          //!< Current volatility percentage
    bool mintingFrozen;         //!< Whether new minting is frozen

    // Oracle system status
    int activeOracles;          //!< Number of active oracles
    CAmount lastOraclePrice;    //!< Last reported DGB price (cents: 100 = $1.00)
    int64_t lastOracleUpdate;   //!< Block height of last oracle update

    // Historical tracking
    std::vector<int> healthHistory;  //!< Recent health percentages

    SystemMetrics() : totalDDSupply(0), totalCollateral(0), systemHealth(0),
                     dcaMultiplier(1.0), errActive(false), volatility(0.0),
                     mintingFrozen(false), activeOracles(0), lastOraclePrice(0),
                     lastOracleUpdate(0) {}
};

/**
 * Alert threshold definitions
 * Defines when system should generate alerts
 */
struct AlertThresholds {
    static constexpr CAmount MAX_DD_SUPPLY = 10000000000;      // 100M DD ($100M)
    static constexpr int MIN_HEALTH_RATIO = 120;               // 120% minimum health
    static constexpr int CRITICAL_HEALTH_RATIO = 110;          // 110% critical health
    static constexpr int MAX_POSITIONS = 10000;                // Maximum positions
    static constexpr int MIN_ORACLES = 5;                      // Minimum oracle count
    static constexpr double MAX_VOLATILITY = 30.0;             // 30% max volatility
    static constexpr double CRITICAL_VOLATILITY = 50.0;        // 50% critical volatility
    static constexpr int STALE_ORACLE_BLOCKS = 100;            // Oracle data freshness
};

/**
 * System Health Monitor
 * Tracks and analyzes DigiDollar system health in real-time
 */
class SystemHealthMonitor {
public:
    /**
     * Get current comprehensive system metrics
     * Scans UTXO set and aggregates all DigiDollar data
     * @return Current system health metrics
     */
    static SystemMetrics GetSystemMetrics();

    /**
     * Get detailed per-tier breakdown
     * Analyzes each lock tier separately
     * @return Vector of tier-specific metrics
     */
    static std::vector<SystemMetrics::TierMetrics> GetTierBreakdown();

    /**
     * Check if system should generate alert for given metric
     * @param metric The metric name to check
     * @return True if alert threshold exceeded
     */
    static bool ShouldAlert(const std::string& metric);

    /**
     * Get historical health data
     * @param blocks Number of recent blocks to retrieve
     * @return Vector of health percentages (most recent first)
     */
    static std::vector<int> GetHealthHistory(int blocks);

    /**
     * Update system metrics with new block
     * Called during block processing to maintain real-time data
     * @param block The new block being processed
     */
    static void UpdateMetrics(const CBlock& block);

    /**
     * Get comprehensive JSON health report
     * Used by RPC commands and monitoring systems
     * @return JSON object with complete system status
     */
    static UniValue GetHealthReport();

    /**
     * Initialize health monitoring system
     * Sets up internal data structures and historical tracking
     */
    static void Initialize();

    /**
     * Shutdown health monitoring system
     * Cleans up resources and saves final state
     */
    static void Shutdown();

    /**
     * Aggregate DigiDollar statistics from all loaded wallets
     * Provides network-wide view by summing across all wallet positions
     * @param wallets Vector of loaded wallets to aggregate
     * @param totalDDSupply [out] Total DD supply across all wallets
     * @param totalCollateral [out] Total DGB collateral across all wallets
     */
    static void AggregateWalletStats(
        const std::vector<std::shared_ptr<wallet::CWallet>>& wallets,
        CAmount& totalDDSupply,
        CAmount& totalCollateral);

    /**
     * Scan UTXO set to find all DigiDollar vaults network-wide
     * This is the CRITICAL function for network-wide tracking
     * @param view CCoinsView to scan (usually from chainstate)
     * @param blockman BlockManager for accessing full transaction data
     * @param mempool Optional mempool for checking recent transactions
     */
    static void ScanUTXOSet(CCoinsView* view, CCoinsView* validation_view, const node::BlockManager* blockman, const CTxMemPool* mempool = nullptr);

    /**
     * Get cached metrics without triggering updates
     * Use this for lightweight access to current values without expensive operations
     * @return Copy of current cached metrics (thread-safe)
     */
    static SystemMetrics GetCachedMetrics() {
        std::lock_guard<std::mutex> lock(s_metricsMutex); // RH-44
        return s_currentMetrics;
    }

    /** Reset metrics to zero (test-only) */
    static void ResetMetrics() {
        std::lock_guard<std::mutex> lock(s_metricsMutex); // RH-44
        s_currentMetrics = SystemMetrics();
    }

    /**
     * Incrementally update metrics when a DD mint transaction is connected to a block.
     * Called from ConnectBlock() under cs_main.
     * @param ddAmount DD amount minted (from OP_RETURN metadata, in cents)
     * @param dgbCollateral DGB collateral locked (in satoshis)
     */
    static void OnMintConnected(CAmount ddAmount, CAmount dgbCollateral);

    /**
     * Incrementally update metrics when a DD redeem transaction is connected to a block.
     * Decrements supply/collateral as the vault is spent.
     * Called from ConnectBlock() under cs_main.
     * @param ddAmount DD amount from the original mint (in cents)
     * @param dgbCollateral DGB collateral released (in satoshis)
     */
    static void OnRedeemConnected(CAmount ddAmount, CAmount dgbCollateral);

    /**
     * Reverse incremental update when a DD mint transaction is disconnected.
     * Called from DisconnectBlock() under cs_main.
     * @param ddAmount DD amount that was minted (in cents)
     * @param dgbCollateral DGB collateral that was locked (in satoshis)
     */
    static void OnMintDisconnected(CAmount ddAmount, CAmount dgbCollateral);

    /**
     * Reverse incremental update when a DD redeem transaction is disconnected.
     * Restores supply/collateral as the vault is un-spent.
     * Called from DisconnectBlock() under cs_main.
     * @param ddAmount DD amount from the original mint (in cents)
     * @param dgbCollateral DGB collateral that was released (in satoshis)
     */
    static void OnRedeemDisconnected(CAmount ddAmount, CAmount dgbCollateral);

private:
    // Internal data structures
    static SystemMetrics s_currentMetrics;
    static std::mutex s_metricsMutex;  //!< RH-44: Protects s_currentMetrics from concurrent access
    static std::map<int64_t, int> s_healthHistory;
    static std::mutex s_historyMutex;  //!< RH-44: Protects s_healthHistory
    static bool s_initialized;

    // Internal helper methods
    static void UpdateTierMetrics();
    static void UpdateProtectionStatus();
    static void UpdateOracleStatus();
    static void RecordHealthHistory(int64_t height, int health);
    static int CalculateSystemHealth(CAmount ddSupply, CAmount collateral, CAmount price);
    static double GetCurrentVolatility();
    static double GetCurrentDCAMultiplier();
    static bool IsERRActive();
    static bool IsMintingFrozen();
    static int GetActiveOracleCount();
    static CAmount GetLastOraclePrice();
    static int64_t GetLastOracleUpdate();

    // Alert checking helpers
    static bool CheckSupplyAlert(const SystemMetrics& metrics);
    static bool CheckHealthAlert(const SystemMetrics& metrics);
    static bool CheckCollateralAlert(const SystemMetrics& metrics);
    static bool CheckOracleAlert(const SystemMetrics& metrics);
    static bool CheckVolatilityAlert(const SystemMetrics& metrics);
    static bool CheckPositionAlert(const SystemMetrics& metrics);
};

/**
 * Health monitoring utilities
 */
namespace HealthUtils {
    /**
     * Convert tier lock days to tier index
     * @param lockDays Number of lock days
     * @return Tier index (0-based)
     */
    int GetTierIndex(int lockDays);

    /**
     * Get tier lock days from tier index
     * @param tierIndex Tier index (0-based)
     * @return Number of lock days for tier
     */
    int GetTierLockDays(int tierIndex);

    /**
     * Calculate health ratio for given amounts
     * @param ddAmount DigiDollar amount (cents)
     * @param dgbAmount DGB collateral amount
     * @param dgbPrice Current DGB price (cents: 100 = $1.00)
     * @return Health ratio percentage
     */
    int CalculateHealthRatio(CAmount ddAmount, CAmount dgbAmount, CAmount dgbPrice);

    /**
     * Format health status as human-readable string
     * @param health Health ratio percentage
     * @return Status string ("Healthy", "Warning", "Critical")
     */
    std::string FormatHealthStatus(int health);

    /**
     * Get recommended action for current health
     * @param health Health ratio percentage
     * @return Action string ("Monitor", "Add Collateral", "Emergency")
     */
    std::string GetRecommendedAction(int health);
}

} // namespace DigiDollar

#endif // DIGIBYTE_DIGIDOLLAR_HEALTH_H