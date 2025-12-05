// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/digidollar.h>
#include <consensus/params.h>
#include <tinyformat.h>
#include <primitives/transaction.h>
#include <script/script.h>

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
    // Special case: 0 days = 1 hour (240 blocks) for testing
    if (days == 0) {
        return 240; // 1 hour at 15-second blocks
    }
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

// Note: IsDigiDollarEnabled() functions moved to digidollar/digidollar.cpp
// as they require deployment checking which is not part of consensus library

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

bool HasDigiDollarMarker(const CTransaction& tx)
{
    // DigiDollar transactions use version field with specific marker
    // Format: Lower 16 bits must match DD_TX_VERSION (0x0770)
    // Bits 16-23: flags, Bits 24-31: transaction type
    const int32_t DD_TX_VERSION = 0x0D1D0770;
    const int32_t DD_VERSION_MASK = 0x0000FFFF;
    return (tx.nVersion & DD_VERSION_MASK) == (DD_TX_VERSION & DD_VERSION_MASK);
}

DigiDollarTxType GetDigiDollarTxType(const CTransaction& tx)
{
    if (!HasDigiDollarMarker(tx)) {
        return DD_TX_NONE;
    }
    // Extract type from bits 24-31 of version field
    const int32_t DD_TYPE_MASK = 0xFF000000;
    return static_cast<DigiDollarTxType>((tx.nVersion & DD_TYPE_MASK) >> 24);
}

bool IsDDTokenScript(const CScript& script)
{
    // DigiDollar token scripts use OP_DIGIDOLLAR (OP_NOP10)
    // Simple check: look for OP_DIGIDOLLAR in script
    for (auto pc = script.begin(); pc != script.end();) {
        opcodetype opcode;
        if (!script.GetOp(pc, opcode)) {
            break;
        }
        if (opcode == OP_NOP10) { // OP_DIGIDOLLAR
            return true;
        }
    }

    // Note: For P2TR scripts, the OP_DIGIDOLLAR is inside the Taproot script tree,
    // not directly visible in scriptPubKey. Full DD token detection requires
    // UTXO database lookup (Phase 2). This consensus function only checks for
    // explicit OP_DIGIDOLLAR in the script.
    return false;
}

bool ExtractDDAmount(const CScript& script, CAmount& amount)
{
    // Initialize to invalid
    amount = -1;

    // Try parsing OP_RETURN format: OP_RETURN <"DD"> <txType> <ddAmount> <lockHeight>
    auto pc = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;

    // Check for OP_RETURN
    if (!script.GetOp(pc, opcode, data) || opcode != OP_RETURN) {
        // Not an OP_RETURN - full DD amount extraction for P2TR scripts
        // requires UTXO database lookup (Phase 2). This consensus function
        // only parses explicit OP_RETURN data.
        return false;
    }

    // Save position after OP_RETURN to try multiple formats
    auto pc_after_opreturn = pc;

    // TRY FORMAT 1: New format - OP_RETURN <"DD"> <txType> <ddAmount> <lockHeight>
    // Get next opcode/data
    if (script.GetOp(pc, opcode, data)) {
        // Check if it's "DD" marker (2 bytes)
        if (data.size() == 2 && data[0] == 'D' && data[1] == 'D') {
            // Get transaction type (CScriptNum)
            if (script.GetOp(pc, opcode, data)) {
                // Get DD amount (CScriptNum) - this is the critical value
                if (script.GetOp(pc, opcode, data)) {
                    // Decode CScriptNum from data
                    try {
                        CScriptNum scriptNum(data, false);  // false = don't require minimal encoding
                        amount = scriptNum.GetInt64();

                        // Sanity check: DD amount should be reasonable
                        if (amount >= 1 && amount <= 100000000000LL) {  // 1 cent to 1 trillion cents
                            return true;
                        }
                    } catch (const scriptnum_error&) {
                        // Fall through to try other formats
                    }
                }
            }
        }
    }

    // TRY FORMAT 2: Old format - OP_RETURN OP_NOP10 <8_byte_amount>
    // Reset to position after OP_RETURN
    pc = pc_after_opreturn;

    // Check for OP_NOP10 (OP_DIGIDOLLAR marker)
    if (script.GetOp(pc, opcode, data) && opcode == OP_NOP10) {
        // Next should be the 8-byte amount
        if (script.GetOp(pc, opcode, data) && data.size() == 8) {
            // Extract 8-byte amount (little-endian)
            amount = 0;
            for (size_t i = 0; i < 8; i++) {
                amount |= static_cast<CAmount>(data[i]) << (i * 8);
            }
            return amount > 0;
        }
    }

    return false;
}

} // namespace DigiDollar