// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/digidollar.h>
#include <consensus/params.h>
#include <deploymentstatus.h>
#include <validation.h>
#include <tinyformat.h>

#include <algorithm>
#include <sstream>

namespace DigiDollar {

int GetCollateralRatioForLockTime(int64_t lockBlocks, const ConsensusParams& params)
{
    // First check for exact matches
    auto exact_it = params.collateralRatios.find(lockBlocks);
    if (exact_it != params.collateralRatios.end()) {
        return exact_it->second;
    }

    // For non-exact matches, find the first tier with lock time >= lockBlocks
    // Use lower_bound to find the first tier with lock time >= lockBlocks
    auto it = params.collateralRatios.lower_bound(lockBlocks);

    if (it == params.collateralRatios.end()) {
        // Lock time is longer than the longest tier, use the longest tier's ratio
        return params.collateralRatios.rbegin()->second;
    }

    // Use the tier we found (which has lock time >= lockBlocks)
    // This ensures we use the more conservative (higher) ratio for shorter lock times
    return it->second;
}

double GetDCAMultiplier(int systemCollateral, const ConsensusParams& params)
{
    // Find the appropriate DCA level based on system collateral
    for (const auto& level : params.dcaLevels) {
        if (systemCollateral >= level.systemCollateral) {
            return level.multiplier / 100.0; // Convert percentage to multiplier
        }
    }

    // If we reach here, system collateral is below all thresholds
    // Use the most restrictive level (last in the vector)
    if (!params.dcaLevels.empty()) {
        return params.dcaLevels.back().multiplier / 100.0;
    }

    // Fallback: double collateral requirement if no levels defined
    return 2.0;
}

bool IsValidMintAmount(CAmount amount, const ConsensusParams& params)
{
    return amount >= params.minMintAmount && amount <= params.maxMintAmount;
}

CAmount GetMinimumDDOutput(const ConsensusParams& params)
{
    return params.minOutputAmount;
}

int64_t LockDaysToBlocks(int days)
{
    return static_cast<int64_t>(days) * BLOCKS_PER_DAY;
}

int BlocksToLockDays(int64_t blocks)
{
    return static_cast<int>(blocks / BLOCKS_PER_DAY);
}

bool ValidateConsensusParams(const ConsensusParams& params, std::string& strError)
{
    // Validate collateral ratios
    if (params.collateralRatios.empty()) {
        strError = "Collateral ratios map cannot be empty";
        return false;
    }

    for (const auto& [lockTime, ratio] : params.collateralRatios) {
        if (lockTime <= 0) {
            strError = "Lock time must be positive";
            return false;
        }
        if (ratio < 100) {
            strError = "Collateral ratio must be at least 100%";
            return false;
        }
        if (ratio > 10000) { // 100x seems like a reasonable upper bound
            strError = "Collateral ratio is unreasonably high";
            return false;
        }
    }

    // Validate transaction limits
    if (params.minMintAmount <= 0) {
        strError = "Minimum mint amount must be positive";
        return false;
    }
    if (params.maxMintAmount <= params.minMintAmount) {
        strError = "Maximum mint amount must be greater than minimum";
        return false;
    }
    if (params.minOutputAmount <= 0) {
        strError = "Minimum output amount must be positive";
        return false;
    }

    // Validate oracle configuration
    if (params.oracleCount == 0) {
        strError = "Oracle count cannot be zero";
        return false;
    }
    if (params.activeOracles > params.oracleCount) {
        strError = "Active oracles cannot exceed total oracle count";
        return false;
    }
    if (params.oracleThreshold > params.activeOracles) {
        strError = "Oracle threshold cannot exceed active oracles";
        return false;
    }
    if (params.oracleThreshold <= params.activeOracles / 2) {
        strError = "Oracle threshold must be more than half of active oracles";
        return false;
    }
    if (params.priceValidBlocks == 0) {
        strError = "Price valid blocks cannot be zero";
        return false;
    }

    // Validate protection mechanisms
    if (params.volatilityThreshold == 0 || params.volatilityThreshold > 100) {
        strError = "Volatility threshold must be between 1% and 100%";
        return false;
    }
    if (params.emergencyThreshold == 0 || params.emergencyThreshold > 200) {
        strError = "Emergency threshold must be between 1% and 200%";
        return false;
    }

    // Validate DCA levels
    if (params.dcaLevels.empty()) {
        strError = "DCA levels cannot be empty";
        return false;
    }

    for (size_t i = 0; i < params.dcaLevels.size(); ++i) {
        const auto& level = params.dcaLevels[i];
        if (level.systemCollateral <= 0) {
            strError = "DCA system collateral must be positive";
            return false;
        }
        if (level.multiplier <= 0) {
            strError = "DCA multiplier must be positive";
            return false;
        }

        // Check ordering: levels should be in descending order of system collateral
        if (i > 0 && level.systemCollateral >= params.dcaLevels[i-1].systemCollateral) {
            strError = "DCA levels must be in descending order of system collateral";
            return false;
        }
    }

    return true;
}

bool IsDigiDollarActive(int nHeight, const Consensus::Params& consensusParams)
{
    // Legacy height-based activation check - deprecated
    return nHeight >= consensusParams.nDDActivationHeight;
}

bool IsDigiDollarEnabled(const CBlockIndex* pindexPrev, const ChainstateManager& chainman)
{
    return DeploymentActiveAfter(pindexPrev, chainman, Consensus::DEPLOYMENT_DIGIDOLLAR);
}

bool IsDigiDollarEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    // For cases where we only have consensus params and a VersionBitsCache isn't available
    // We'll need to create a temporary cache - not ideal but needed for some contexts
    VersionBitsCache cache;
    return DeploymentActiveAfter(pindexPrev, params, Consensus::DEPLOYMENT_DIGIDOLLAR, cache);
}

int GetLockTierIndex(int64_t lockBlocks, const ConsensusParams& params)
{
    int index = 0;
    for (const auto& [tierLockTime, ratio] : params.collateralRatios) {
        if (lockBlocks <= tierLockTime) {
            return index;
        }
        ++index;
    }

    // If lock time is longer than all tiers, return the last (longest) tier
    return static_cast<int>(params.collateralRatios.size()) - 1;
}

std::string FormatLockPeriod(int64_t lockBlocks)
{
    int days = BlocksToLockDays(lockBlocks);

    if (days < 30) {
        return strprintf("%d days", days);
    } else if (days < 365) {
        int months = days / 30;
        if (months == 1) {
            return "1 month";
        } else if (months < 12) {
            return strprintf("%d months", months);
        } else {
            return "1 year";
        }
    } else {
        int years = days / 365;
        if (years == 1) {
            return "1 year";
        } else {
            return strprintf("%d years", years);
        }
    }
}

} // namespace DigiDollar