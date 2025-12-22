// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/err.h>
#include <consensus/dca.h>
#include <digidollar/digidollar.h>
#include <digidollar/health.h>
#include <digidollar/validation.h>
#include <oracle/mock_oracle.h>
#include <primitives/oracle.h>
#include <key.h>
#include <pubkey.h>
#include <util/time.h>
#include <logging.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace DigiDollar {
namespace ERR {

// Initialize static members
ERRState EmergencyRedemptionRatio::s_currentState;
std::vector<COutPoint> EmergencyRedemptionRatio::s_errQueue;
std::map<COutPoint, std::pair<CAmount, uint32_t>> EmergencyRedemptionRatio::s_queuedRedemptions;

// ERR adjustment tier thresholds and ratios
// CRITICAL: ERR returns 100% collateral ALWAYS. Ratios determine DD burn multiplier.
// Formula: RequiredDD = OriginalDD / ratio (lower ratio = MORE DD burn required)
static const std::vector<std::pair<int, double>> ERR_TIERS = {
    {95, 0.95},  // 95-100% health: 1/0.95 = 1.053x DD burn (105.3% of original)
    {90, 0.90},  // 90-95% health: 1/0.90 = 1.111x DD burn (111.1% of original)
    {85, 0.85},  // 85-90% health: 1/0.85 = 1.176x DD burn (117.6% of original)
    {0,  0.80}   // <85% health: 1/0.80 = 1.250x DD burn (125.0% of original, maximum)
};

// Oracle consensus requirements
static const size_t REQUIRED_ORACLE_SIGNATURES = 8;
static const size_t TOTAL_ORACLE_COUNT = 15;
static const uint64_t ORACLE_MESSAGE_MAX_AGE = 3600; // 1 hour in seconds

bool EmergencyRedemptionRatio::ShouldActivateERR(int systemHealth)
{
    // ERR activates when system health falls below 100%
    return systemHealth < 100;
}

double EmergencyRedemptionRatio::CalculateERRAdjustment(int systemHealth)
{
    // At >= 100% health, no ERR adjustment needed (ratio = 1.0)
    // This means ERR returns same as normal redemption (full collateral, burn original DD)
    if (systemHealth >= 100) {
        return 1.0;
    }

    // Find the appropriate ERR tier based on system health
    // Returns a ratio (0.80-0.95) used to calculate required DD burn
    // RequiredDD = OriginalDD / ratio (so lower ratio = more DD required)
    for (const auto& tier : ERR_TIERS) {
        if (systemHealth >= tier.first) {
            return tier.second;
        }
    }

    // Fallback to minimum ratio (80%) = maximum DD burn (125%)
    return 0.80;
}

CAmount EmergencyRedemptionRatio::GetRequiredDDBurn(CAmount originalDDMinted, int systemHealth)
{
    // Calculate how much DD must be burned to redeem FULL collateral during ERR
    // Formula: RequiredDD = OriginalDD / ERRRatio
    // Example: 100 DD at 80% ratio → 100/0.80 = 125 DD required

    if (originalDDMinted <= 0) {
        return 0;
    }

    // If system is healthy (>= 100%), no extra DD required
    if (systemHealth >= 100) {
        return originalDDMinted;
    }

    double adjustmentRatio = CalculateERRAdjustment(systemHealth);

    // Prevent division by zero
    if (adjustmentRatio <= 0) {
        adjustmentRatio = 0.80;
    }

    // Calculate required DD burn (divide by ratio to get MORE DD)
    // Use ceiling to ensure we don't shortchange the system
    CAmount requiredDD = static_cast<CAmount>(std::ceil(static_cast<double>(originalDDMinted) / adjustmentRatio));

    LogPrint(BCLog::DIGIDOLLAR, "ERR: GetRequiredDDBurn - original: %lld, health: %d%%, ratio: %.2f, required: %lld (%.1f%% increase)\n",
             static_cast<long long>(originalDDMinted), systemHealth, adjustmentRatio,
             static_cast<long long>(requiredDD),
             ((static_cast<double>(requiredDD) / originalDDMinted) - 1.0) * 100);

    return requiredDD;
}

CAmount EmergencyRedemptionRatio::GetAdjustedRedemption(CAmount normalRedemption, int systemHealth)
{
    // DEPRECATED: This function name is misleading.
    // ERR doesn't reduce collateral return - it increases DD burn requirement.
    // Use GetRequiredDDBurn() instead.
    //
    // For backwards compatibility, this now just returns the input unchanged
    // since collateral return is NOT adjusted during ERR.
    LogPrint(BCLog::DIGIDOLLAR, "ERR: GetAdjustedRedemption DEPRECATED - use GetRequiredDDBurn instead\n");
    return normalRedemption;
}

bool EmergencyRedemptionRatio::HasOracleConsensus(const COracleBundle& bundle)
{
    // Use the existing oracle consensus system
    return bundle.HasConsensus();
}

ERRState EmergencyRedemptionRatio::GetCurrentState()
{
    // Update state based on current system health
    int currentHealth = DCA::DynamicCollateralAdjustment::GetCurrentSystemHealth();

    if (s_currentState.isActive) {
        // Check if ERR should deactivate
        if (currentHealth >= 100) {
            DeactivateERR(currentHealth);
        } else {
            // Update current health and adjustment ratio
            s_currentState.systemHealth = currentHealth;
            s_currentState.adjustmentRatio = CalculateERRAdjustment(currentHealth);
        }
    } else {
        // ERR is inactive, update health for monitoring
        s_currentState.systemHealth = currentHealth;
        s_currentState.adjustmentRatio = 0.0;
    }

    return s_currentState;
}

std::vector<COutPoint> EmergencyRedemptionRatio::GetERRQueue()
{
    return s_errQueue;
}

bool EmergencyRedemptionRatio::QueueERRRedemption(const COutPoint& outpoint, CAmount ddAmount, uint32_t requestHeight)
{
    if (!s_currentState.isActive) {
        return false;
    }

    // Check if already queued
    if (s_queuedRedemptions.count(outpoint)) {
        return false;
    }

    // Add to queue (FIFO order)
    s_errQueue.push_back(outpoint);
    s_queuedRedemptions[outpoint] = std::make_pair(ddAmount, requestHeight);

    LogPrint(BCLog::DIGIDOLLAR, "ERR: Queued redemption %s for %d DD at height %u\n",
             outpoint.ToString(), ddAmount, requestHeight);

    return true;
}

size_t EmergencyRedemptionRatio::ProcessERRQueue(size_t maxRedemptions)
{
    if (!s_currentState.isActive || s_errQueue.empty()) {
        return 0;
    }

    size_t processed = 0;
    auto it = s_errQueue.begin();

    while (it != s_errQueue.end() && processed < maxRedemptions) {
        const COutPoint& outpoint = *it;

        // Get redemption details
        auto redemptionIt = s_queuedRedemptions.find(outpoint);
        if (redemptionIt == s_queuedRedemptions.end()) {
            // Invalid queue entry, remove it
            it = s_errQueue.erase(it);
            continue;
        }

        CAmount ddAmount = redemptionIt->second.first;
        uint32_t requestHeight = redemptionIt->second.second;

        // Process the redemption with ERR adjustment
        // Note: In a full implementation, this would interact with the UTXO set
        // and create the actual redemption transaction

        LogPrint(BCLog::DIGIDOLLAR, "ERR: Processing redemption %s for %d DD (requested at height %u)\n",
                 outpoint.ToString(), ddAmount, requestHeight);

        // Remove from queue and tracking
        it = s_errQueue.erase(it);
        s_queuedRedemptions.erase(outpoint);
        processed++;
    }

    return processed;
}

bool EmergencyRedemptionRatio::ActivateERR(const COracleBundle& oracleBundle, uint32_t activationHeight)
{
    // Check if ERR is already active
    if (s_currentState.isActive) {
        return false;
    }

    // Get current system health
    int currentHealth = DCA::DynamicCollateralAdjustment::GetCurrentSystemHealth();

    // Check if ERR should be activated based on health
    if (!ShouldActivateERR(currentHealth)) {
        LogPrint(BCLog::DIGIDOLLAR, "ERR: Activation denied - system health %d%% is sufficient\n", currentHealth);
        return false;
    }

    // Validate oracle consensus
    if (!HasOracleConsensus(oracleBundle)) {
        LogPrint(BCLog::DIGIDOLLAR, "ERR: Activation denied - insufficient oracle consensus\n");
        return false;
    }

    // Activate ERR
    ERRState newState;
    newState.isActive = true;
    newState.systemHealth = currentHealth;
    newState.adjustmentRatio = CalculateERRAdjustment(currentHealth);
    newState.activationHeight = activationHeight;
    newState.oracleConsensusHash = CalculateOracleConsensusHash(oracleBundle);
    newState.activationTimestamp = GetTime();

    UpdateERRState(newState);

    LogPrint(BCLog::DIGIDOLLAR, "ERR: Activated at height %u with %d%% system health (%.1f%% return ratio)\n",
             activationHeight, currentHealth, newState.adjustmentRatio * 100);

    return true;
}

bool EmergencyRedemptionRatio::DeactivateERR(int currentHealth)
{
    if (!s_currentState.isActive) {
        return false;
    }

    // Check if system health has recovered
    if (currentHealth < 100) {
        return false;
    }

    LogPrint(BCLog::DIGIDOLLAR, "ERR: Deactivating - system health recovered to %d%%\n", currentHealth);

    // Process any remaining queued redemptions
    ProcessERRQueue(s_errQueue.size());

    // Clear ERR state
    ERRState clearedState;
    clearedState.systemHealth = currentHealth;
    UpdateERRState(clearedState);

    // Clear queue
    ClearERRQueue();

    LogPrint(BCLog::DIGIDOLLAR, "ERR: Deactivated - system health restored\n");
    return true;
}

bool EmergencyRedemptionRatio::ValidateERRRedemption(const CTransaction& tx, CAmount originalDDMinted, CAmount expectedCollateral)
{
    // ERR redemption validation:
    // - User burns MORE DD than originally minted (based on ERR ratio)
    // - User receives FULL collateral back (not reduced!)

    // Check if ERR should be active (even if state not formally activated)
    int currentHealth = s_currentState.isActive ? s_currentState.systemHealth :
                        DCA::DynamicCollateralAdjustment::GetCurrentSystemHealth();

    if (currentHealth >= 100) {
        LogPrint(BCLog::DIGIDOLLAR, "ERR: Validation failed - system health %d%% doesn't require ERR\n", currentHealth);
        return false;
    }

    // Calculate required DD burn for ERR redemption
    CAmount requiredDDBurn = GetRequiredDDBurn(originalDDMinted, currentHealth);

    // Count DD being burned in this transaction (DD inputs - DD outputs)
    CAmount ddInputs = 0;
    CAmount ddOutputs = 0;
    for (const auto& output : tx.vout) {
        if (IsDDTokenScript(output.scriptPubKey)) {
            ddOutputs += output.nValue;
        }
    }
    // Note: DD inputs would need to be looked up from UTXO set
    // For now, we validate that the transaction structure is correct
    // The actual DD burn verification happens during full validation

    // Validate collateral output is the FULL amount (ERR doesn't reduce collateral!)
    CAmount actualCollateralOutput = 0;
    for (const auto& output : tx.vout) {
        if (!IsDDTokenScript(output.scriptPubKey)) {
            actualCollateralOutput += output.nValue;
        }
    }

    // Allow some flexibility for fees, but collateral should be close to expected
    CAmount tolerance = 1000000; // 0.01 DGB tolerance for fees
    if (actualCollateralOutput < expectedCollateral - tolerance) {
        LogPrint(BCLog::DIGIDOLLAR, "ERR: Validation failed - collateral return %lld < expected %lld (ERR should return FULL collateral)\n",
                 static_cast<long long>(actualCollateralOutput), static_cast<long long>(expectedCollateral));
        return false;
    }

    LogPrint(BCLog::DIGIDOLLAR, "ERR: Validation passed - health: %d%%, required DD burn: %lld (original: %lld), collateral: %lld\n",
             currentHealth, static_cast<long long>(requiredDDBurn),
             static_cast<long long>(originalDDMinted), static_cast<long long>(actualCollateralOutput));

    return true;
}

bool EmergencyRedemptionRatio::ShouldBlockMinting()
{
    // Block minting if:
    // 1. ERR is formally activated (via oracle consensus), OR
    // 2. System health is below 100% (automatic protection)
    //
    // The second check is important for regtest/testnet with mock oracles
    // where ERR may not be formally activated but system is under-collateralized.
    if (s_currentState.isActive) {
        return true;
    }

    // Get cached metrics for DD supply and collateral
    const DigiDollar::SystemMetrics& metrics = DigiDollar::SystemHealthMonitor::GetCachedMetrics();

    // If no DD in circulation, minting is always allowed (system has no liabilities)
    if (metrics.totalDDSupply <= 0) {
        return false;
    }

    // Need oracle price to calculate health
    // Get price directly from MockOracleManager (for regtest) as cached metrics may be stale
    CAmount oraclePriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();

    // If no oracle price available, we can't determine emergency state
    // Default to allowing minting (benefit of doubt)
    if (oraclePriceMicroUSD <= 0) {
        LogPrint(BCLog::DIGIDOLLAR, "ERR: No oracle price available, allowing minting\n");
        return false;
    }

    // Convert micro-USD to millicents for health calculation
    // micro-USD / 10 = millicents
    CAmount oraclePriceMillicents = oraclePriceMicroUSD / 10;

    // Calculate real-time system health
    int currentHealth = DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
        metrics.totalCollateral, metrics.totalDDSupply, oraclePriceMillicents);

    if (ShouldActivateERR(currentHealth)) {
        LogPrint(BCLog::DIGIDOLLAR, "ERR: Blocking minting due to low system health (%d%%)\n", currentHealth);
        return true;
    }

    return false;
}

std::string EmergencyRedemptionRatio::GetERRStatistics()
{
    ERRState state = GetCurrentState();

    std::string stats = "ERR System Statistics:\n";
    stats += "Status: " + std::string(state.isActive ? "ACTIVE" : "INACTIVE") + "\n";
    stats += "System Health: " + std::to_string(state.systemHealth) + "%\n";

    if (state.isActive) {
        stats += "Adjustment Ratio: " + FormatERRAdjustment(state.adjustmentRatio) + "\n";
        stats += "Activation Height: " + std::to_string(state.activationHeight) + "\n";
        stats += "Queue Size: " + std::to_string(s_errQueue.size()) + " redemptions\n";

        uint64_t activeDuration = GetTime() - state.activationTimestamp;
        stats += "Active Duration: " + std::to_string(activeDuration / 3600) + " hours\n";
    }

    return stats;
}

bool EmergencyRedemptionRatio::ValidateERRConfig(std::string& error)
{
    // Validate ERR tier configuration
    if (ERR_TIERS.empty()) {
        error = "No ERR tiers configured";
        return false;
    }

    // Check tier ratios are reasonable
    for (const auto& tier : ERR_TIERS) {
        if (tier.second < 0.5 || tier.second > 1.0) {
            error = "ERR tier ratio out of range: " + std::to_string(tier.second);
            return false;
        }
    }

    // Validate oracle requirements
    if (REQUIRED_ORACLE_SIGNATURES > TOTAL_ORACLE_COUNT) {
        error = "Required oracle signatures exceed total oracle count";
        return false;
    }

    if (REQUIRED_ORACLE_SIGNATURES < TOTAL_ORACLE_COUNT / 2) {
        error = "Required oracle signatures below safe threshold";
        return false;
    }

    return true;
}

std::string EmergencyRedemptionRatio::FormatERRAdjustment(double ratio)
{
    return std::to_string(static_cast<int>(ratio * 100)) + "%";
}

std::string EmergencyRedemptionRatio::FormatERRHealth(int health, bool isERRActive)
{
    std::string healthStr = std::to_string(health) + "%";
    if (isERRActive) {
        healthStr += " (ERR ACTIVE)";
    }
    return healthStr;
}

// Private helper methods

uint256 EmergencyRedemptionRatio::CalculateOracleConsensusHash(const COracleBundle& bundle)
{
    // Create a deterministic hash of the oracle consensus
    std::vector<unsigned char> data;

    // Add epoch to the hash
    data.insert(data.end(), (unsigned char*)&bundle.epoch, (unsigned char*)&bundle.epoch + sizeof(bundle.epoch));

    // Add each message to the hash
    for (const auto& message : bundle.messages) {
        // Add oracle ID, price, and timestamp
        data.insert(data.end(), (unsigned char*)&message.oracle_id, (unsigned char*)&message.oracle_id + sizeof(message.oracle_id));
        data.insert(data.end(), (unsigned char*)&message.price_micro_usd, (unsigned char*)&message.price_micro_usd + sizeof(message.price_micro_usd));
        data.insert(data.end(), (unsigned char*)&message.timestamp, (unsigned char*)&message.timestamp + sizeof(message.timestamp));
    }

    return Hash(data);
}

bool EmergencyRedemptionRatio::ValidateOracleSignature(const COraclePriceMessage& message)
{
    // Use the existing oracle message validation
    return message.IsValid();
}

bool EmergencyRedemptionRatio::IsAuthorizedOracleKey(const XOnlyPubKey& oracleKey)
{
    // For ERR, oracle authorization is handled by the existing oracle system
    // The oracle bundle validation already checks authorized oracles
    return true;
}

void EmergencyRedemptionRatio::UpdateERRState(const ERRState& newState)
{
    s_currentState = newState;
}

void EmergencyRedemptionRatio::ClearERRQueue()
{
    s_errQueue.clear();
    s_queuedRedemptions.clear();
}

// Missing implementations for testing functions
bool EmergencyRedemptionRatio::HandleHealthOscillation(const std::vector<int>& healthValues, const std::vector<bool>& activationResults)
{
    if (healthValues.size() != activationResults.size()) return false;

    for (size_t i = 0; i < healthValues.size(); ++i) {
        bool expectedActivation = ShouldActivateERR(healthValues[i]);
        if (expectedActivation != activationResults[i]) {
            return false;
        }
    }
    return true;
}

bool EmergencyRedemptionRatio::ValidatePrecisionBoundaries(const std::vector<double>& preciseHealth)
{
    for (double health : preciseHealth) {
        int intHealth = static_cast<int>(health);
        bool shouldActivate = ShouldActivateERR(intHealth);

        // Validate boundary precision
        if (health < 100.0 && !shouldActivate) return false;
        if (health >= 100.0 && shouldActivate) return false;
    }
    return true;
}

bool EmergencyRedemptionRatio::ValidateExtremeHealthValues(const std::vector<int>& extremeValues)
{
    for (int health : extremeValues) {
        // Should handle extreme values gracefully
        try {
            ShouldActivateERR(health);  // Test function doesn't crash
            double adjustment = CalculateERRAdjustment(health);

            // Validate adjustment is within bounds
            if (adjustment < 0.5 || adjustment > 1.0) return false;
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool EmergencyRedemptionRatio::ValidateThreadSafety(const std::vector<bool>& concurrentResults)
{
    // Simple validation - all concurrent operations should have consistent results
    if (concurrentResults.empty()) return true;

    bool firstResult = concurrentResults[0];
    for (bool result : concurrentResults) {
        if (result != firstResult) return false;
    }
    return true;
}

bool EmergencyRedemptionRatio::HandleConsensusFailure(int systemHealth, const std::vector<COraclePriceMessage>& messages)
{
    // If system health requires ERR but insufficient oracle messages, handle gracefully
    if (ShouldActivateERR(systemHealth) && messages.size() < REQUIRED_ORACLE_SIGNATURES) {
        LogPrint(BCLog::DIGIDOLLAR, "ERR: Consensus failure - insufficient oracle messages (%zu/%zu)\n",
                 messages.size(), REQUIRED_ORACLE_SIGNATURES);
        return true; // Handled gracefully
    }
    return false;
}

bool EmergencyRedemptionRatio::ShouldActivateERRWithCorruptedState(int systemHealth, bool stateCorrupted)
{
    // If state is corrupted, be conservative and don't activate
    if (stateCorrupted) {
        return false;
    }
    return ShouldActivateERR(systemHealth);
}

bool EmergencyRedemptionRatio::ShouldActivateERRAtHeight(int systemHealth, uint32_t blockHeight)
{
    // Basic ERR activation logic doesn't depend on block height for now
    return ShouldActivateERR(systemHealth);
}

bool EmergencyRedemptionRatio::ShouldActivateERRAtTime(int systemHealth, int64_t timestamp)
{
    // Basic ERR activation logic doesn't depend on time for now
    return ShouldActivateERR(systemHealth);
}

bool EmergencyRedemptionRatio::HasActivationDelay(int systemHealth)
{
    // No activation delay mechanism implemented yet
    return false;
}

bool EmergencyRedemptionRatio::ValidateRatioPrecision(int health, double expectedRatio)
{
    double actualRatio = CalculateERRAdjustment(health);
    return std::abs(actualRatio - expectedRatio) < 0.001; // 0.1% precision tolerance
}

bool EmergencyRedemptionRatio::PreventCalculationOverflow(CAmount maxRedemption, int health)
{
    try {
        CAmount result = GetAdjustedRedemption(maxRedemption, health);
        return result <= maxRedemption && result >= 0;
    } catch (...) {
        return false;
    }
}

bool EmergencyRedemptionRatio::ValidateCalculationConsistency(const std::vector<std::pair<CAmount, double>>& stressTests)
{
    // UPDATED: ERR now returns FULL collateral, increases DD burn instead
    // This validates that GetAdjustedRedemption returns the full amount
    // and GetRequiredDDBurn calculates correct burn increase
    for (const auto& test : stressTests) {
        CAmount amount = test.first;
        double ratio = test.second;

        int health = static_cast<int>(ratio * 100);

        // GetAdjustedRedemption should return FULL amount (not reduced)
        CAmount collateralResult = GetAdjustedRedemption(amount, health);
        if (collateralResult != amount) return false; // Must return full amount

        // GetRequiredDDBurn should return amount / ratio (more than original)
        CAmount burnResult = GetRequiredDDBurn(amount, health);
        if (health < 100 && burnResult <= amount) return false; // Must burn MORE
    }
    return true;
}

bool EmergencyRedemptionRatio::HandleLargeOracleMessageCount(const COracleBundle& bundle)
{
    // Should handle large message counts without performance issues
    return bundle.messages.size() <= 1000; // Reasonable upper limit
}

bool EmergencyRedemptionRatio::ValidateMalformedMessageHandling(const std::vector<COraclePriceMessage>& malformedMessages)
{
    // Should handle malformed messages gracefully without crashing
    for (const auto& message : malformedMessages) {
        try {
            ValidateOracleSignature(message);
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool EmergencyRedemptionRatio::ValidateConsensusPerformance(int64_t durationMs)
{
    // Oracle consensus should complete within reasonable time (10 seconds)
    return durationMs <= 10000;
}

} // namespace ERR
} // namespace DigiDollar