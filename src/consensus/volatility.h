// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_CONSENSUS_VOLATILITY_H
#define DIGIBYTE_CONSENSUS_VOLATILITY_H

#include <consensus/amount.h>
#include <uint256.h>
#include <sync.h>

#include <vector>
#include <deque>
#include <cstdint>

// Forward declarations
class COraclePriceMessage;

namespace DigiDollar {
namespace Volatility {

// ============================================================================
// Data Structures
// ============================================================================

/** Price data point for volatility calculations */
struct PricePoint {
    CAmount price;         //!< Price in hundredths (e.g., 50000 = $500.00)
    int64_t timestamp;     //!< Unix timestamp
    uint32_t height;       //!< Block height when recorded

    PricePoint() : price(0), timestamp(0), height(0) {}
    PricePoint(CAmount p, int64_t t, uint32_t h) : price(p), timestamp(t), height(h) {}
};

/** Current volatility state */
struct VolatilityState {
    double hourlyVolatility;      //!< 1-hour volatility percentage
    double dailyVolatility;       //!< 24-hour volatility percentage
    double weeklyVolatility;      //!< 7-day volatility percentage
    bool mintingFrozen;           //!< True if new minting is frozen
    bool allOperationsFrozen;     //!< True if all DD operations are frozen
    uint32_t freezeHeight;        //!< Height when freeze was triggered
    uint32_t cooldownEndHeight;   //!< Height when cooldown period ends

    VolatilityState() :
        hourlyVolatility(0.0),
        dailyVolatility(0.0),
        weeklyVolatility(0.0),
        mintingFrozen(false),
        allOperationsFrozen(false),
        freezeHeight(0),
        cooldownEndHeight(0) {}
};

/** Volatility thresholds for triggering freeze mechanisms */
struct VolatilityThresholds {
    static constexpr double WARNING_1H = 10.0;          //!< 10% in 1 hour: warning
    static constexpr double FREEZE_MINT_1H = 20.0;      //!< 20% in 1 hour: freeze new mints
    static constexpr double FREEZE_ALL_24H = 30.0;      //!< 30% in 24 hours: freeze all operations
    static constexpr double EMERGENCY_7D = 50.0;        //!< 50% in 7 days: emergency mode

    static constexpr uint32_t COOLDOWN_BLOCKS = 8640;   //!< Cooldown period in blocks (8640 × 15s = 36 hours)
};

// ============================================================================
// Volatility Monitor Class
// ============================================================================

/**
 * Monitors price volatility and manages freeze mechanisms for DigiDollar system
 *
 * This class tracks price history, calculates volatility metrics, and determines
 * when operations should be frozen due to excessive price volatility.
 */
class VolatilityMonitor {
private:
    static RecursiveMutex cs_volatility;  //!< Protects all static data

    static std::deque<PricePoint> priceHistory;   //!< Price history (max 30 days)
    static VolatilityState currentState;          //!< Current volatility state
    static uint32_t lastUpdateHeight;             //!< Last height state was updated

    // Constants
    static constexpr size_t MAX_HISTORY_DAYS = 30;           //!< Maximum days of history to keep
    static constexpr size_t MAX_HISTORY_POINTS = 30 * 24;    //!< Maximum price points (hourly for 30 days)
    static constexpr int64_t MIN_PRICE_INTERVAL = 3600;      //!< Minimum seconds between price updates

    // Internal helper methods
    static void UpdateVolatilityState() EXCLUSIVE_LOCKS_REQUIRED(cs_volatility);
    static void CleanOldHistory() EXCLUSIVE_LOCKS_REQUIRED(cs_volatility);
    static std::vector<PricePoint> GetPricesInWindow(int64_t timeWindow) EXCLUSIVE_LOCKS_REQUIRED(cs_volatility);
    static double CalculateStandardDeviation(const std::vector<double>& values);

public:
    // ========================================================================
    // Price History Management
    // ========================================================================

    /**
     * Record a new price point in the volatility monitoring system
     * @param price Price in hundredths (e.g., 50000 = $500.00)
     * @param timestamp Unix timestamp of the price
     * @param height Block height (optional, uses current chain tip if 0)
     */
    static void RecordPrice(CAmount price, int64_t timestamp, uint32_t height = 0);

    /**
     * Get complete price history
     * @return Vector of all recorded price points
     */
    static std::vector<PricePoint> GetPriceHistory();

    /**
     * Clear all price history (primarily for testing)
     */
    static void ClearHistory();

    // ========================================================================
    // Volatility Calculations
    // ========================================================================

    /**
     * Calculate volatility for a specific time window
     * @param timeWindow Time window in seconds (e.g., 3600 for 1 hour)
     * @return Volatility as a percentage (e.g., 15.5 for 15.5%)
     */
    static double CalculateVolatility(int64_t timeWindow);

    /**
     * Get current volatility state
     * @return Current VolatilityState with all metrics
     */
    static VolatilityState GetCurrentState();

    /**
     * Update volatility calculations based on current price history
     * Should be called periodically (e.g., each block)
     */
    static void UpdateState(uint32_t currentHeight);

    // ========================================================================
    // Freeze Mechanism Controls
    // ========================================================================

    /**
     * Check if new minting should be frozen
     * @return True if minting should be frozen due to volatility
     */
    static bool ShouldFreezeMinting();

    /**
     * Check if all DigiDollar operations should be frozen
     * @return True if all operations should be frozen due to volatility
     */
    static bool ShouldFreezeAll();

    /**
     * Check if currently in cooldown period after a freeze
     * @return True if in cooldown period
     */
    static bool InCooldownPeriod();

    /**
     * Get the block height when cooldown period ends
     * @return Block height when cooldown expires (0 if not in cooldown)
     */
    static uint32_t GetCooldownEndHeight();

    // ========================================================================
    // Override Mechanism
    // ========================================================================


    /**
     * Manually trigger freeze (for testing or emergency)
     * @param freezeAll True to freeze all operations, false for minting only
     * @param height Block height when freeze is triggered
     */
    static void TriggerFreeze(bool freezeAll, uint32_t height);

    /**
     * Manually clear freeze state (for testing or after manual intervention)
     */
    static void ClearFreeze();

    /**
     * Reconstruct volatility state from block price data after restart
     * Re-feeds saved price history and recalculates freeze state
     * @param blockPrices Price points extracted from recent blocks
     * @param currentHeight Current chain height
     */
    static void ReconstructFromBlockData(const std::vector<PricePoint>& blockPrices, uint32_t currentHeight);

    // ========================================================================
    // Diagnostic Functions
    // ========================================================================

    /**
     * Get detailed volatility metrics for diagnostics
     * @return String with detailed volatility information
     */
    static std::string GetDiagnosticInfo();

    /**
     * Check if volatility monitoring is properly initialized
     * @return True if system is ready for volatility monitoring
     */
    static bool IsInitialized();

    /**
     * Get the age of the most recent price data
     * @return Seconds since last price update (0 if no data)
     */
    static int64_t GetDataAge();
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert volatility percentage to human-readable string
 * @param volatility Volatility as percentage
 * @return Formatted string (e.g., "15.2%")
 */
std::string FormatVolatility(double volatility);

/**
 * Calculate percentage change between two prices
 * @param oldPrice Previous price
 * @param newPrice Current price
 * @return Percentage change (positive or negative)
 */
double CalculatePercentageChange(CAmount oldPrice, CAmount newPrice);

/**
 * Check if a price change exceeds a threshold
 * @param oldPrice Previous price
 * @param newPrice Current price
 * @param threshold Threshold percentage
 * @return True if change exceeds threshold
 */
bool ExceedsThreshold(CAmount oldPrice, CAmount newPrice, double threshold);

} // namespace Volatility
} // namespace DigiDollar

#endif // DIGIBYTE_CONSENSUS_VOLATILITY_H