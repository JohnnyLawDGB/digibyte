// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_CONSENSUS_DIGIDOLLAR_H
#define DIGIBYTE_CONSENSUS_DIGIDOLLAR_H

#include <consensus/amount.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Forward declarations
class CBlockIndex;
class ChainstateManager;
class CTransaction;
class CScript;
namespace Consensus {
    struct Params;
}

namespace DigiDollar {

// DigiByte specific constants
static const int BLOCKS_PER_DAY = 24 * 60 * 4;  // 5760 blocks (15s blocks)
static const CAmount CENT = 1000000;  // DigiDollar cent in satoshis

// DigiDollar transaction types
enum DigiDollarTxType : uint8_t {
    DD_TX_NONE = 0,
    DD_TX_MINT = 1,      // Lock DGB, create DigiDollars
    DD_TX_TRANSFER = 2,  // Transfer DigiDollars between addresses
    DD_TX_REDEEM = 3,    // Burn DigiDollars, unlock DGB
    DD_TX_PARTIAL = 4,   // Partial redemption
    DD_TX_ERR = 5        // Emergency Redemption Ratio (ERR) redemption
};

/**
 * Core consensus parameters for the DigiDollar stablecoin system.
 * These parameters define the economic model, collateral requirements,
 * oracle configuration, and protection mechanisms.
 */
struct ConsensusParams {
    // Collateral ratios (higher for shorter periods - treasury model)
    // Map of lock time in blocks to collateral ratio percentage
    std::map<int64_t, int> collateralRatios = {
        {240, 1000},                       // 1 hour: 1000% (testing only)
        {30 * 24 * 60 * 4, 500},           // 30 days: 500%
        {90 * 24 * 60 * 4, 400},           // 3 months: 400%
        {180 * 24 * 60 * 4, 350},          // 6 months: 350%
        {365 * 24 * 60 * 4, 300},          // 1 year: 300%
        {3 * 365 * 24 * 60 * 4, 250},      // 3 years: 250%
        {5 * 365 * 24 * 60 * 4, 225},      // 5 years: 225%
        {7 * 365 * 24 * 60 * 4, 212},      // 7 years: 212%
        {10 * 365 * 24 * 60 * 4, 200}      // 10 years: 200%
    };

    // Transaction limits (amounts in cents: 100 cents = $1.00)
    CAmount minMintAmount = 10000;             // $100 minimum (10000 cents)
    CAmount maxMintAmount = 10000000;          // $100k maximum per tx (10000000 cents)
    CAmount minOutputAmount = 100;             // $1 minimum output (100 cents)

    // Oracle configuration
    uint32_t oracleCount = 30;                 // Total hardcoded oracles
    uint32_t activeOracles = 15;               // Active per epoch
    uint32_t oracleThreshold = 8;              // 8-of-15 consensus
    uint32_t priceValidBlocks = 20;            // 5 minutes at 15s blocks

    // Protection mechanisms
    uint32_t volatilityThreshold = 20;         // 20% triggers DCA
    uint32_t emergencyThreshold = 100;         // 100% collateral triggers ERR

    // System health thresholds for DCA (Dynamic Collateral Adjustment)
    struct DCALevel {
        int systemCollateral;   // System-wide collateral %
        int multiplier;         // Collateral requirement multiplier
    };

    std::vector<DCALevel> dcaLevels = {
        {150, 100},  // >150%: Normal (100% of base requirement)
        {120, 125},  // 120-150%: +25% collateral required
        {110, 150},  // 110-120%: +50% collateral required
        {100, 200},  // <110%: +100% collateral required
    };
};

// Helper functions for consensus parameter operations

/**
 * Get collateral ratio for a given lock time in blocks.
 * Returns the appropriate collateral ratio percentage based on lock period.
 * For lock times between defined tiers, returns the higher ratio (more conservative).
 */
int GetCollateralRatioForLockTime(int64_t lockBlocks, const ConsensusParams& params);

/**
 * Get DCA multiplier based on system health.
 * Returns the collateral requirement multiplier (100 = 100% = no change).
 * Lower system health requires higher collateral.
 */
double GetDCAMultiplier(int systemCollateral, const ConsensusParams& params);

/**
 * Check if amount is valid for minting operations.
 * Validates against minimum and maximum mint amounts.
 */
bool IsValidMintAmount(CAmount amount, const ConsensusParams& params);

/**
 * Get minimum DD output amount.
 * Returns the minimum amount for DigiDollar outputs.
 */
CAmount GetMinimumDDOutput(const ConsensusParams& params);

/**
 * Convert lock days to blocks using DigiByte's 15-second block time.
 * Utility function for converting human-readable lock periods.
 */
int64_t LockDaysToBlocks(int days);

/**
 * Convert blocks to approximate days.
 * Utility function for displaying lock periods to users.
 */
int BlocksToLockDays(int64_t blocks);

/**
 * Validate consensus parameters for sanity checks.
 * Ensures all parameters are within reasonable ranges and consistent.
 */
bool ValidateConsensusParams(const ConsensusParams& params, std::string& strError);

/**
 * Check if DigiDollar is active at given height.
 * Determines if DigiDollar functionality should be available.
 * @deprecated Use IsDigiDollarEnabled instead for BIP9 deployment checking
 */
bool IsDigiDollarActive(int nHeight, const Consensus::Params& consensusParams);

// Note: IsDigiDollarEnabled() functions are in digidollar/digidollar.h
// (not here) as they require deployment checking which is not part of consensus library

/**
 * Get the tier index for a lock period.
 * Returns the index in the collateral ratios map for the given lock period.
 * Returns -1 if no appropriate tier is found.
 */
int GetLockTierIndex(int64_t lockBlocks, const ConsensusParams& params);

/**
 * Format lock period for display.
 * Converts block count to human-readable format (e.g., "30 days", "1 year").
 */
std::string FormatLockPeriod(int64_t lockBlocks);

/**
 * Check if transaction has DigiDollar marker in version field
 */
bool HasDigiDollarMarker(const CTransaction& tx);

/**
 * Extract DigiDollar transaction type from version field
 */
DigiDollarTxType GetDigiDollarTxType(const CTransaction& tx);

// Note: IsDDTokenScript() and ExtractDDAmount() are declared in
// src/digidollar/validation.h as they require CScript methods not
// available in the consensus library

} // namespace DigiDollar

#endif // DIGIBYTE_CONSENSUS_DIGIDOLLAR_H