// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/volatility.h>
#include <consensus/digidollar.h>
#include <primitives/oracle.h>
#include <logging.h>
#include <util/time.h>
#include <util/moneystr.h>
#include <limits>
#include <hash.h>
#include <pubkey.h>
#include <serialize.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <set>

namespace DigiDollar {
namespace Volatility {

// ============================================================================
// Static Member Definitions
// ============================================================================

RecursiveMutex VolatilityMonitor::cs_volatility;
std::deque<PricePoint> VolatilityMonitor::priceHistory;
VolatilityState VolatilityMonitor::currentState;
uint32_t VolatilityMonitor::lastUpdateHeight = 0;

// ============================================================================
// VolatilityMonitor Implementation
// ============================================================================

void VolatilityMonitor::RecordPrice(CAmount price, int64_t timestamp, uint32_t height)
{
    LOCK(cs_volatility);

    // Use current time if timestamp is 0
    if (timestamp == 0) {
        timestamp = GetTime();
    }

    // Use provided height or 0 if not available
    // In full implementation, this would query the chain state

    // Check if we should record this price (avoid spam)
    if (!priceHistory.empty()) {
        const auto& lastPrice = priceHistory.back();

        // Don't record if too recent (< 1 hour interval)
        if (timestamp - lastPrice.timestamp < MIN_PRICE_INTERVAL) {
            LogPrintf("VolatilityMonitor: Skipping price update (too recent: %d seconds)\n",
                     timestamp - lastPrice.timestamp);
            return;
        }
    }

    // Add new price point
    priceHistory.emplace_back(price, timestamp, height);

    LogPrintf("VolatilityMonitor: Recorded price %s at height %d (timestamp %d)\n",
             FormatMoney(price), height, timestamp);

    // Clean old history to maintain size limits
    CleanOldHistory();

    // Update volatility state
    UpdateVolatilityState();
}

std::vector<PricePoint> VolatilityMonitor::GetPriceHistory()
{
    LOCK(cs_volatility);
    return std::vector<PricePoint>(priceHistory.begin(), priceHistory.end());
}

void VolatilityMonitor::ClearHistory()
{
    LOCK(cs_volatility);
    priceHistory.clear();
    currentState = VolatilityState();
    lastUpdateHeight = 0;

    LogPrintf("VolatilityMonitor: History cleared\n");
}

double VolatilityMonitor::CalculateVolatility(int64_t timeWindow)
{
    LOCK(cs_volatility);

    if (priceHistory.size() < 2) {
        return 0.0;
    }

    // Get prices in the specified time window
    auto windowPrices = GetPricesInWindow(timeWindow);

    if (windowPrices.size() < 2) {
        return 0.0;
    }

    // Calculate volatility as the maximum absolute percentage change from the starting price
    // This better reflects the actual price movement for freeze thresholds
    CAmount startPrice = windowPrices.front().price;
    double maxAbsChange = 0.0;

    for (size_t i = 1; i < windowPrices.size(); ++i) {
        double change = std::abs(CalculatePercentageChange(startPrice, windowPrices[i].price));
        maxAbsChange = std::max(maxAbsChange, change);
    }

    // Also calculate standard deviation for additional volatility metric
    std::vector<double> percentChanges;
    percentChanges.reserve(windowPrices.size() - 1);

    for (size_t i = 1; i < windowPrices.size(); ++i) {
        double change = CalculatePercentageChange(windowPrices[i-1].price, windowPrices[i].price);
        percentChanges.push_back(std::abs(change));
    }

    double stdDevVolatility = CalculateStandardDeviation(percentChanges);

    // Return the maximum of: max absolute change and 2x standard deviation
    // This captures both large single moves and sustained volatility
    return std::max(maxAbsChange, stdDevVolatility * 2.0);
}

VolatilityState VolatilityMonitor::GetCurrentState()
{
    LOCK(cs_volatility);
    return currentState;
}

void VolatilityMonitor::UpdateState(uint32_t currentHeight)
{
    LOCK(cs_volatility);

    if (priceHistory.empty()) {
        return;
    }

    lastUpdateHeight = currentHeight;

    // Calculate volatility for different time windows
    currentState.hourlyVolatility = CalculateVolatility(3600);      // 1 hour
    currentState.dailyVolatility = CalculateVolatility(24 * 3600);  // 24 hours
    currentState.weeklyVolatility = CalculateVolatility(7 * 24 * 3600); // 7 days

    // Update freeze state based on thresholds
    bool shouldFreezeMint = (currentState.hourlyVolatility >= VolatilityThresholds::FREEZE_MINT_1H);
    bool shouldFreezeAll = (currentState.dailyVolatility >= VolatilityThresholds::FREEZE_ALL_24H) ||
                          (currentState.weeklyVolatility >= VolatilityThresholds::EMERGENCY_7D);

    // Log warnings
    if (currentState.hourlyVolatility >= VolatilityThresholds::WARNING_1H) {
        LogPrintf("VolatilityMonitor: WARNING - High volatility detected: 1h=%.2f%%, 24h=%.2f%%, 7d=%.2f%%\n",
                  currentState.hourlyVolatility, currentState.dailyVolatility, currentState.weeklyVolatility);
    }

    // Handle freeze state changes
    if (shouldFreezeAll && !currentState.allOperationsFrozen) {
        // Trigger all operations freeze
        currentState.allOperationsFrozen = true;
        currentState.mintingFrozen = true;
        currentState.freezeHeight = currentHeight;
        currentState.cooldownEndHeight = (currentHeight > std::numeric_limits<uint32_t>::max() - VolatilityThresholds::COOLDOWN_BLOCKS)
            ? std::numeric_limits<uint32_t>::max()
            : currentHeight + VolatilityThresholds::COOLDOWN_BLOCKS;

        LogPrintf("VolatilityMonitor: FREEZE - All DigiDollar operations frozen due to high volatility (%.2f%% daily, %.2f%% weekly)\n",
                  currentState.dailyVolatility, currentState.weeklyVolatility);

    } else if (shouldFreezeMint && !currentState.mintingFrozen) {
        // Trigger minting freeze only
        currentState.mintingFrozen = true;
        currentState.freezeHeight = currentHeight;
        currentState.cooldownEndHeight = (currentHeight > std::numeric_limits<uint32_t>::max() - VolatilityThresholds::COOLDOWN_BLOCKS)
            ? std::numeric_limits<uint32_t>::max()
            : currentHeight + VolatilityThresholds::COOLDOWN_BLOCKS;

        LogPrintf("VolatilityMonitor: FREEZE - DigiDollar minting frozen due to high volatility (%.2f%% hourly)\n",
                  currentState.hourlyVolatility);
    }

    // Check if we should unfreeze (only if not in cooldown and volatility is low)
    if ((currentState.mintingFrozen || currentState.allOperationsFrozen) &&
        currentHeight > currentState.cooldownEndHeight) {

        bool canUnfreeze = (currentState.hourlyVolatility < VolatilityThresholds::WARNING_1H) &&
                          (currentState.dailyVolatility < VolatilityThresholds::FREEZE_MINT_1H) &&
                          (currentState.weeklyVolatility < VolatilityThresholds::FREEZE_ALL_24H);

        if (canUnfreeze) {
            LogPrintf("VolatilityMonitor: UNFREEZE - Volatility stabilized, unfreezing operations\n");
            currentState.mintingFrozen = false;
            currentState.allOperationsFrozen = false;
            currentState.freezeHeight = 0;
            currentState.cooldownEndHeight = 0;
        }
    }
}

bool VolatilityMonitor::ShouldFreezeMinting()
{
    LOCK(cs_volatility);
    return currentState.mintingFrozen;
}

bool VolatilityMonitor::ShouldFreezeAll()
{
    LOCK(cs_volatility);
    return currentState.allOperationsFrozen;
}

bool VolatilityMonitor::InCooldownPeriod()
{
    LOCK(cs_volatility);

    if (currentState.cooldownEndHeight == 0) {
        return false;
    }

    // In full implementation, this would check against current chain height
    // For now, return based on last known height
    return lastUpdateHeight <= currentState.cooldownEndHeight;
}

uint32_t VolatilityMonitor::GetCooldownEndHeight()
{
    LOCK(cs_volatility);
    return currentState.cooldownEndHeight;
}


void VolatilityMonitor::TriggerFreeze(bool freezeAll, uint32_t height)
{
    LOCK(cs_volatility);

    if (height == 0) {
        height = lastUpdateHeight; // Use last known height
    }

    currentState.mintingFrozen = true;
    currentState.allOperationsFrozen = freezeAll;
    currentState.freezeHeight = height;
    currentState.cooldownEndHeight = (height > std::numeric_limits<uint32_t>::max() - VolatilityThresholds::COOLDOWN_BLOCKS)
        ? std::numeric_limits<uint32_t>::max()
        : height + VolatilityThresholds::COOLDOWN_BLOCKS;

    LogPrintf("VolatilityMonitor: MANUAL FREEZE - %s frozen at height %d\n",
              freezeAll ? "All operations" : "Minting only", height);
}

void VolatilityMonitor::ClearFreeze()
{
    LOCK(cs_volatility);

    currentState.mintingFrozen = false;
    currentState.allOperationsFrozen = false;
    currentState.freezeHeight = 0;
    currentState.cooldownEndHeight = 0;

    LogPrintf("VolatilityMonitor: MANUAL UNFREEZE - All freeze states cleared\n");
}

void VolatilityMonitor::ReconstructFromBlockData(const std::vector<PricePoint>& blockPrices, uint32_t currentHeight)
{
    LOCK(cs_volatility);

    // Clear existing state
    priceHistory.clear();
    currentState = VolatilityState();
    lastUpdateHeight = 0;

    LogPrintf("VolatilityMonitor: Reconstructing state from %d block price points at height %d\n",
              blockPrices.size(), currentHeight);

    // Re-feed all price points (they're already sorted chronologically from blocks)
    for (const auto& pp : blockPrices) {
        priceHistory.emplace_back(pp);
    }

    // Update last known height
    lastUpdateHeight = currentHeight;

    // Clean old history to maintain size limits
    CleanOldHistory();

    // Recalculate volatility state from reconstructed history
    if (!priceHistory.empty()) {
        UpdateVolatilityState();
    }

    LogPrintf("VolatilityMonitor: Reconstruction complete - frozen=%d, mintFrozen=%d, pricePoints=%d\n",
              currentState.allOperationsFrozen, currentState.mintingFrozen, priceHistory.size());
}

std::string VolatilityMonitor::GetDiagnosticInfo()
{
    LOCK(cs_volatility);

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "=== DigiDollar Volatility Monitor Status ===\n";
    ss << "Price History Points: " << priceHistory.size() << "\n";

    if (!priceHistory.empty()) {
        ss << "Latest Price: " << FormatMoney(priceHistory.back().price)
           << " (height " << priceHistory.back().height << ")\n";
        ss << "Data Age: " << GetDataAge() << " seconds\n";
    }

    ss << "\nVolatility Metrics:\n";
    ss << "  1-hour:  " << FormatVolatility(currentState.hourlyVolatility) << "\n";
    ss << "  24-hour: " << FormatVolatility(currentState.dailyVolatility) << "\n";
    ss << "  7-day:   " << FormatVolatility(currentState.weeklyVolatility) << "\n";

    ss << "\nFreeze Status:\n";
    ss << "  Minting Frozen: " << (currentState.mintingFrozen ? "YES" : "NO") << "\n";
    ss << "  All Operations Frozen: " << (currentState.allOperationsFrozen ? "YES" : "NO") << "\n";

    if (currentState.freezeHeight > 0) {
        ss << "  Freeze Height: " << currentState.freezeHeight << "\n";
        ss << "  Cooldown End Height: " << currentState.cooldownEndHeight << "\n";
        ss << "  In Cooldown: " << (InCooldownPeriod() ? "YES" : "NO") << "\n";
    }

    ss << "\nThresholds:\n";
    ss << "  Warning (1h): " << VolatilityThresholds::WARNING_1H << "%\n";
    ss << "  Freeze Mint (1h): " << VolatilityThresholds::FREEZE_MINT_1H << "%\n";
    ss << "  Freeze All (24h): " << VolatilityThresholds::FREEZE_ALL_24H << "%\n";
    ss << "  Emergency (7d): " << VolatilityThresholds::EMERGENCY_7D << "%\n";

    return ss.str();
}

bool VolatilityMonitor::IsInitialized()
{
    LOCK(cs_volatility);
    return !priceHistory.empty();
}

int64_t VolatilityMonitor::GetDataAge()
{
    LOCK(cs_volatility);

    if (priceHistory.empty()) {
        return 0;
    }

    return GetTime() - priceHistory.back().timestamp;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void VolatilityMonitor::UpdateVolatilityState()
{
    // This is called from RecordPrice, which already holds the lock
    AssertLockHeld(cs_volatility);

    if (priceHistory.empty()) {
        return;
    }

    // Calculate volatility for different time windows
    currentState.hourlyVolatility = CalculateVolatility(3600);      // 1 hour
    currentState.dailyVolatility = CalculateVolatility(24 * 3600);  // 24 hours
    currentState.weeklyVolatility = CalculateVolatility(7 * 24 * 3600); // 7 days

    // Get the most recent height from price history (if available)
    uint32_t currentHeight = priceHistory.empty() ? lastUpdateHeight : priceHistory.back().height;

    // Update freeze state based on thresholds
    bool shouldFreezeMint = (currentState.hourlyVolatility >= VolatilityThresholds::FREEZE_MINT_1H);
    bool shouldFreezeAll = (currentState.dailyVolatility >= VolatilityThresholds::FREEZE_ALL_24H) ||
                          (currentState.weeklyVolatility >= VolatilityThresholds::EMERGENCY_7D);

    // Log warnings
    if (currentState.hourlyVolatility >= VolatilityThresholds::WARNING_1H) {
        LogPrintf("VolatilityMonitor: WARNING - High volatility detected: 1h=%.2f%%, 24h=%.2f%%, 7d=%.2f%%\n",
                  currentState.hourlyVolatility, currentState.dailyVolatility, currentState.weeklyVolatility);
    }

    // Handle freeze state changes
    if (shouldFreezeAll && !currentState.allOperationsFrozen) {
        // Trigger all operations freeze
        currentState.allOperationsFrozen = true;
        currentState.mintingFrozen = true;
        currentState.freezeHeight = currentHeight;
        currentState.cooldownEndHeight = (currentHeight > std::numeric_limits<uint32_t>::max() - VolatilityThresholds::COOLDOWN_BLOCKS)
            ? std::numeric_limits<uint32_t>::max()
            : currentHeight + VolatilityThresholds::COOLDOWN_BLOCKS;

        LogPrintf("VolatilityMonitor: FREEZE - All DigiDollar operations frozen due to high volatility (%.2f%% daily, %.2f%% weekly)\n",
                  currentState.dailyVolatility, currentState.weeklyVolatility);

    } else if (shouldFreezeMint && !currentState.mintingFrozen) {
        // Trigger minting freeze only
        currentState.mintingFrozen = true;
        currentState.freezeHeight = currentHeight;
        currentState.cooldownEndHeight = (currentHeight > std::numeric_limits<uint32_t>::max() - VolatilityThresholds::COOLDOWN_BLOCKS)
            ? std::numeric_limits<uint32_t>::max()
            : currentHeight + VolatilityThresholds::COOLDOWN_BLOCKS;

        LogPrintf("VolatilityMonitor: FREEZE - DigiDollar minting frozen due to high volatility (%.2f%% hourly)\n",
                  currentState.hourlyVolatility);
    }

    // Check if we should unfreeze (only if not in cooldown and volatility is low)
    if ((currentState.mintingFrozen || currentState.allOperationsFrozen) &&
        currentHeight > currentState.cooldownEndHeight) {

        bool canUnfreeze = (currentState.hourlyVolatility < VolatilityThresholds::WARNING_1H) &&
                          (currentState.dailyVolatility < VolatilityThresholds::FREEZE_MINT_1H) &&
                          (currentState.weeklyVolatility < VolatilityThresholds::FREEZE_ALL_24H);

        if (canUnfreeze) {
            LogPrintf("VolatilityMonitor: UNFREEZE - Volatility stabilized, unfreezing operations\n");
            currentState.mintingFrozen = false;
            currentState.allOperationsFrozen = false;
            currentState.freezeHeight = 0;
            currentState.cooldownEndHeight = 0;
        }
    }
}

void VolatilityMonitor::CleanOldHistory()
{
    AssertLockHeld(cs_volatility);

    if (priceHistory.empty()) {
        return;
    }

    // Use the most recent timestamp as reference, not current time
    // This ensures consistent behavior in tests and when replaying history
    const int64_t cutoffTime = priceHistory.back().timestamp - (MAX_HISTORY_DAYS * 24 * 3600);

    // Remove old entries
    while (!priceHistory.empty() && priceHistory.front().timestamp < cutoffTime) {
        priceHistory.pop_front();
    }

    // Also enforce maximum point limit
    while (priceHistory.size() > MAX_HISTORY_POINTS) {
        priceHistory.pop_front();
    }
}

std::vector<PricePoint> VolatilityMonitor::GetPricesInWindow(int64_t timeWindow)
{
    AssertLockHeld(cs_volatility);

    std::vector<PricePoint> windowPrices;

    if (priceHistory.empty()) {
        return windowPrices;
    }

    const int64_t cutoffTime = priceHistory.back().timestamp - timeWindow;

    // Find all prices within the time window
    for (auto it = priceHistory.rbegin(); it != priceHistory.rend(); ++it) {
        if (it->timestamp >= cutoffTime) {
            windowPrices.push_back(*it);
        } else {
            break; // Older than window
        }
    }

    // Reverse to get chronological order
    std::reverse(windowPrices.begin(), windowPrices.end());

    return windowPrices;
}

double VolatilityMonitor::CalculateStandardDeviation(const std::vector<double>& values)
{
    if (values.size() < 2) {
        return 0.0;
    }

    // Calculate mean
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();

    // Calculate variance
    double squaredDiffSum = 0.0;
    for (double value : values) {
        double diff = value - mean;
        squaredDiffSum += diff * diff;
    }

    double variance = squaredDiffSum / (values.size() - 1); // Sample variance
    return std::sqrt(variance);
}


// ============================================================================
// Utility Functions
// ============================================================================

std::string FormatVolatility(double volatility)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << volatility << "%";
    return ss.str();
}

double CalculatePercentageChange(CAmount oldPrice, CAmount newPrice)
{
    if (oldPrice == 0) {
        return 0.0;
    }

    return (static_cast<double>(newPrice - oldPrice) / static_cast<double>(oldPrice)) * 100.0;
}

bool ExceedsThreshold(CAmount oldPrice, CAmount newPrice, double threshold)
{
    double change = std::abs(CalculatePercentageChange(oldPrice, newPrice));
    return change >= threshold;
}

} // namespace Volatility
} // namespace DigiDollar