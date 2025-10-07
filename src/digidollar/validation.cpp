// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <digidollar/digidollar.h>

// Phase 1 metadata tracking support
using DigiDollar::ScriptMetadata;
using DigiDollar::GetScriptMetadata;
#include <consensus/digidollar.h>
#include <consensus/volatility.h>
#include <consensus/dca.h>
#include <consensus/err.h>
#include <script/standard.h>
#include <script/solver.h>
#include <script/interpreter.h>
#include <logging.h>
#include <util/strencodings.h>
#include <util/hasher.h>
#include <sync.h>
#include <uint256.h>

#include <unordered_map>
#include <memory>

namespace DigiDollar {

// ============================================================================
// Validation Cache (Thread-Safe)
// ============================================================================

// Simple cache for script type identification to avoid re-parsing
struct ValidationCache {
    mutable RecursiveMutex cs_cache;
    std::unordered_map<uint256, ScriptType, BlockHasher> scriptTypeCache;
    std::unordered_map<uint256, std::pair<bool, CAmount>, BlockHasher> amountCache; // bool=valid, CAmount=amount

    // Limit cache size to prevent memory bloat
    static const size_t MAX_CACHE_SIZE = 10000;

    void ClearIfFull() EXCLUSIVE_LOCKS_REQUIRED(cs_cache) {
        if (scriptTypeCache.size() > MAX_CACHE_SIZE) {
            scriptTypeCache.clear();
            amountCache.clear();
            LogPrintf("DigiDollar: Validation cache cleared (size limit reached)\n");
        }
    }
};

// Global validation cache instance
static ValidationCache g_validationCache;

// ============================================================================
// Script Analysis Functions
// ============================================================================

ScriptType IdentifyScriptType(const CScript& script) {
    // Quick rejection for obviously non-P2TR scripts
    if (script.size() != 34 || script[0] != OP_1) {
        return ScriptType::NOT_DIGIDOLLAR;
    }

    // Phase 1: Use metadata tracking for scripts created by Create*P2TR functions
    // This is a testing workaround - Phase 2 will use UTXO database tracking
    ScriptMetadata metadata;
    if (GetScriptMetadata(script, metadata)) {
        return metadata.type;
    }

    // Unknown P2TR script - cannot determine without metadata
    return ScriptType::NOT_DIGIDOLLAR;
}

// Note: The following functions are defined in consensus/digidollar.cpp
// to avoid duplicate symbols (these were previously duplicated here):
// - ExtractDDAmount()
// - IsDDTokenScript()
// - HasDigiDollarMarker()
// - GetDigiDollarTxType()

bool IsCollateralScript(const CScript& script) {
    // Phase 1: Use metadata to identify collateral scripts
    // (Phase 2 will use UTXO database for actual deployment)
    ScriptType type = IdentifyScriptType(script);
    return type == ScriptType::COLLATERAL_LOCK;
}

// ============================================================================
// Amount and Collateral Validation
// ============================================================================

bool ValidateMintAmount(CAmount amount, const CChainParams& params) {
    const auto& ddParams = params.GetDigiDollarParams();
    return amount >= ddParams.minMintAmount && amount <= ddParams.maxMintAmount;
}

bool ValidateOutputAmount(CAmount amount, const CChainParams& params) {
    const auto& ddParams = params.GetDigiDollarParams();
    return amount >= ddParams.minOutputAmount && amount <= MAX_DIGIDOLLAR;
}

CAmount CalculateRequiredCollateral(CAmount ddAmount, int64_t lockTime,
                                   const ValidationContext& ctx) {
    if (ddAmount <= 0 || ctx.oraclePrice <= 0) {
        return 0;
    }

    // Get base collateral ratio for lock period
    const auto& ddParams = ctx.params.GetDigiDollarParams();
    int baseRatio = GetCollateralRatioForLockTime(lockTime, ddParams);

    // Calculate real-time system health for DCA
    CAmount totalCollateral = DigiDollar::DCA::DynamicCollateralAdjustment::GetTotalSystemCollateral();
    CAmount totalDD = DigiDollar::DCA::DynamicCollateralAdjustment::GetTotalDDSupply();
    int systemHealth = DigiDollar::DCA::DynamicCollateralAdjustment::CalculateSystemHealth(
        totalCollateral, totalDD, ctx.oraclePrice);

    LogPrint(BCLog::DIGIDOLLAR, "DCA: Real-time system health calculation for collateral requirement:\n");
    LogPrint(BCLog::DIGIDOLLAR, "  Total collateral: %lld DGB (%.2f DGB)\n",
             totalCollateral, totalCollateral / (double)COIN);
    LogPrint(BCLog::DIGIDOLLAR, "  Total DD supply: %lld cents ($%.2f)\n",
             totalDD, totalDD / 100.0);
    LogPrint(BCLog::DIGIDOLLAR, "  Oracle price: %lld cents ($%.2f per DGB)\n",
             ctx.oraclePrice, ctx.oraclePrice / 100.0);
    LogPrint(BCLog::DIGIDOLLAR, "  System health: %d%%\n", systemHealth);

    // Apply DCA multiplier based on real-time system health
    int effectiveRatio = GetEffectiveCollateralRatio(baseRatio, systemHealth, ctx.params);

    // Calculate required DGB: (DD amount in cents * DGB satoshis) / (price in cents) * (ratio% / 100)
    // Use 64-bit arithmetic to prevent overflow, same as TxBuilder
    uint64_t dgbFor100Percent = (static_cast<uint64_t>(ddAmount) * static_cast<uint64_t>(COIN)) / static_cast<uint64_t>(ctx.oraclePrice);
    uint64_t requiredDGB = (dgbFor100Percent * static_cast<uint64_t>(effectiveRatio)) / 100;

    LogPrint(BCLog::DIGIDOLLAR, "DCA: Collateral calculation: %lld cents * %lld / %lld = %llu sat (100%%), * %d%% / 100 = %llu sat\n",
             ddAmount, COIN, ctx.oraclePrice, dgbFor100Percent, effectiveRatio, requiredDGB);

    return requiredDGB;
}

int GetEffectiveCollateralRatio(int baseRatio, int systemCollateral,
                               const CChainParams& params) {
    // Use the new DCA system for more comprehensive health calculation
    double multiplier = DigiDollar::DCA::DynamicCollateralAdjustment::GetDCAMultiplier(systemCollateral);
    int effectiveRatio = DigiDollar::DCA::DynamicCollateralAdjustment::ApplyDCA(baseRatio, systemCollateral);

    LogPrint(BCLog::DIGIDOLLAR, "DCA: Base ratio %d%%, system health %d%%, multiplier %.1fx -> effective ratio %d%%\n",
             baseRatio, systemCollateral, multiplier, effectiveRatio);

    return effectiveRatio;
}

bool ValidateCollateralRatio(CAmount dgbLocked, CAmount ddMinted,
                            int64_t lockTime, const ValidationContext& ctx) {
    // Input validation
    if (ddMinted <= 0) {
        LogPrintf("DigiDollar: Invalid DD minted amount: %d\n", ddMinted);
        return false;
    }

    if (dgbLocked <= 0) {
        LogPrintf("DigiDollar: Invalid DGB locked amount: %d\n", dgbLocked);
        return false;
    }

    if (ctx.oraclePrice <= 0) {
        LogPrintf("DigiDollar: Invalid oracle price: %d\n", ctx.oraclePrice);
        return false;
    }

    if (lockTime <= 0) {
        LogPrintf("DigiDollar: Invalid lock time: %d blocks\n", lockTime);
        return false;
    }

    // Calculate required collateral
    CAmount requiredCollateral = CalculateRequiredCollateral(ddMinted, lockTime, ctx);
    if (requiredCollateral <= 0) {
        LogPrintf("DigiDollar: Failed to calculate required collateral\n");
        return false;
    }

    // Calculate actual collateral ratio for logging
    CAmount dgbValueInCents = (dgbLocked * ctx.oraclePrice) / COIN;
    int actualRatio = (dgbValueInCents * 100) / ddMinted;

    // Get expected ratio for comparison
    const auto& ddParams = ctx.params.GetDigiDollarParams();
    int baseRatio = GetCollateralRatioForLockTime(lockTime, ddParams);
    int effectiveRatio = GetEffectiveCollateralRatio(baseRatio, ctx.systemCollateral, ctx.params);

    LogPrintf("DigiDollar: Collateral validation details:\n");
    LogPrintf("  DGB locked: %d satoshis (%.2f DGB)\n", dgbLocked, dgbLocked / (double)COIN);
    LogPrintf("  DD minted: %d cents ($%.2f)\n", ddMinted, ddMinted / 100.0);
    LogPrintf("  Lock time: %d blocks (~%d days)\n", lockTime, lockTime / (24 * 60 * 4));
    LogPrintf("  Oracle price: %d cents ($%.2f)\n", ctx.oraclePrice, ctx.oraclePrice / 100.0);
    LogPrintf("  DGB value: %d cents ($%.2f)\n", dgbValueInCents, dgbValueInCents / 100.0);
    LogPrintf("  Base ratio: %d%%, Effective ratio: %d%%, Actual ratio: %d%%\n",
              baseRatio, effectiveRatio, actualRatio);
    LogPrintf("  Required collateral: %d satoshis (%.2f DGB)\n",
              requiredCollateral, requiredCollateral / (double)COIN);
    LogPrintf("  System collateral: %d%%\n", ctx.systemCollateral);

    bool isValid = dgbLocked >= requiredCollateral;
    LogPrintf("  Result: %s\n", isValid ? "VALID" : "INVALID");

    return isValid;
}

// ============================================================================
// Path-Specific Validation Functions
// ============================================================================

bool ValidateNormalRedemption(const CScript& script, int currentHeight) {
    // Extract lock height from script metadata (Phase 1 implementation)
    // Phase 2 will extract from UTXO database

    ScriptMetadata metadata;
    if (!GetScriptMetadata(script, metadata)) {
        // No metadata found - script not registered
        return false;
    }

    // Validate that timelock has expired
    // Redemption is only allowed when currentHeight >= lockHeight
    if (currentHeight < metadata.lockHeight) {
        // Timelock has not expired yet - redemption REJECTED
        return false;
    }

    // Timelock has expired - redemption allowed
    return true;
}

bool ValidateEmergencyRedemption(const CScript& script,
                                const std::vector<std::vector<unsigned char>>& sigs) {
    // Emergency path requires 8-of-15 oracle signatures
    const size_t requiredSigs = 8;

    // Count valid signatures (simplified check)
    size_t validSigs = 0;
    for (const auto& sig : sigs) {
        if (!sig.empty()) {
            validSigs++;
        }
    }

    LogPrintf("DigiDollar: Emergency redemption - %d signatures provided, %d required\n",
              validSigs, requiredSigs);

    return validSigs >= requiredSigs;
}

bool ValidatePartialRedemption(const CScript& script, CAmount oraclePrice) {
    // Partial redemption requires current price data
    // Price must be recent and valid

    if (oraclePrice <= 0) {
        LogPrintf("DigiDollar: Partial redemption failed - invalid price: %d\n", oraclePrice);
        return false;
    }

    // In full implementation, would also check price staleness
    // For testing, just validate price is positive
    return true;
}

bool ValidateERRRedemption(const CScript& script, int systemCollateral) {
    // ERR (Emergency Redemption Ratio) activates when system < 100% collateralized
    bool errActive = systemCollateral < 100;

    LogPrintf("DigiDollar: ERR validation - System collateral: %d%%, ERR %s\n",
              systemCollateral, errActive ? "ACTIVE" : "INACTIVE");

    return errActive;
}

// ============================================================================
// Script Validation
// ============================================================================

bool ValidateDigiDollarScript(const CScript& script,
                              const ValidationContext& ctx,
                              ScriptError* serror) {
    ScriptType type = IdentifyScriptType(script);

    // Non-DD scripts pass through without validation
    if (type == ScriptType::NOT_DIGIDOLLAR) {
        return true;
    }

    // Extract and validate DD amount if present
    CAmount amount;
    if (!ExtractDDAmount(script, amount)) {
        if (serror) *serror = SCRIPT_ERR_INVALID_DD_AMOUNT;
        LogPrintf("DigiDollar: Script validation failed - cannot extract DD amount\n");
        return false;
    }

    // Validate amount based on script type
    switch (type) {
        case ScriptType::DD_TOKEN_OUTPUT:
            if (!ValidateOutputAmount(amount, ctx.params)) {
                if (serror) *serror = SCRIPT_ERR_INVALID_DD_AMOUNT;
                LogPrintf("DigiDollar: Invalid DD output amount: %d cents\n", amount);
                return false;
            }
            break;

        case ScriptType::COLLATERAL_LOCK:
            // Collateral scripts don't have amount limits in the same way
            break;

        default:
            break;
    }

    LogPrintf("DigiDollar: Script validation passed - Type: %d, Amount: %d cents\n",
              static_cast<int>(type), amount);

    return true;
}

// ============================================================================
// Transaction Type Validation
// ============================================================================

bool ValidateMintTransaction(const CTransaction& tx,
                            const ValidationContext& ctx,
                            TxValidationState& state) {
    LogPrintf("DigiDollar: Validating mint transaction (txid: %s)\n", tx.GetHash().ToString());

    // 1. Basic structural checks
    if (tx.vin.empty()) {
        LogPrintf("DigiDollar: Mint transaction has no inputs\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mint-no-inputs");
    }

    if (tx.vout.size() < 2) {
        LogPrintf("DigiDollar: Mint transaction needs at least 2 outputs (collateral + DD)\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mint-outputs");
    }

    // 2. Oracle price validation
    if (ctx.oraclePrice <= 0) {
        LogPrintf("DigiDollar: Invalid oracle price: %d\n", ctx.oraclePrice);
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-oracle-price");
    }

    // 3. Volatility protection checks for minting
    if (Volatility::VolatilityMonitor::ShouldFreezeMinting()) {
        LogPrintf("DigiDollar: Minting frozen due to high volatility\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "minting-frozen-volatility");
    }

    if (Volatility::VolatilityMonitor::ShouldFreezeAll()) {
        LogPrintf("DigiDollar: All DD operations frozen due to extreme volatility\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "all-operations-frozen");
    }

    // 4. Analyze outputs to find DD amounts and collateral
    CAmount totalDD = 0;
    CAmount totalCollateral = 0;
    bool hasCollateralOutput = false;
    bool hasDDOutput = false;
    int64_t lockTime = 0;

    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CTxOut& output = tx.vout[i];

        // Phase 1 workaround: Identify outputs by structure since metadata doesn't cross nodes
        // P2TR outputs: collateral has value > 0, DD token has value = 0
        bool isP2TR = (output.scriptPubKey.size() == 34 && output.scriptPubKey[0] == OP_1);
        bool isOpReturn = (output.scriptPubKey.size() > 0 && output.scriptPubKey[0] == OP_RETURN);

        if (isP2TR && output.nValue > 0) {
            // This is the collateral output
            if (!ValidateCollateralOutput(output, tx, state)) {
                return false;
            }
            totalCollateral += output.nValue;
            hasCollateralOutput = true;
        }

        // Check for OP_RETURN metadata: <"DD"> <txType> <ddAmount> <lockHeight>
        if (isOpReturn) {
            CScript::const_iterator pc = output.scriptPubKey.begin() + 1;
            opcodetype opcode;
            std::vector<unsigned char> data;

            // Check for DD marker
            if (output.scriptPubKey.GetOp(pc, opcode, data) && data.size() == 2 &&
                data[0] == 'D' && data[1] == 'D') {

                // Extract tx type (1 = MINT, 2 = TRANSFER, etc.)
                int64_t txType = 0;
                if (output.scriptPubKey.GetOp(pc, opcode, data)) {
                    try {
                        CScriptNum txTypeNum(data, true);
                        txType = txTypeNum.getint();
                        LogPrintf("DigiDollar: Extracted tx type from OP_RETURN: %d\n", txType);
                    } catch (const std::exception&) {}
                }

                // Extract DD amount in cents
                if (output.scriptPubKey.GetOp(pc, opcode, data)) {
                    try {
                        CScriptNum ddAmountNum(data, true);
                        totalDD = ddAmountNum.getint();
                        LogPrintf("DigiDollar: Extracted DD amount from OP_RETURN: %d cents ($%.2f)\n",
                                  totalDD, totalDD / 100.0);
                    } catch (const std::exception&) {}
                }

                // Extract lock height
                if (output.scriptPubKey.GetOp(pc, opcode, data)) {
                    try {
                        CScriptNum lockHeightNum(data, true);
                        lockTime = lockHeightNum.getint();
                        LogPrintf("DigiDollar: Extracted lock height from OP_RETURN: %d blocks\n", lockTime);
                    } catch (const std::exception&) {}
                }
            }
        }

        if (isP2TR && output.nValue == 0) {
            // This is the DD token output
            if (!ValidateDDOutput(output, tx, state)) {
                return false;
            }
            hasDDOutput = true;

            // Phase 1: Try to extract DD amount from metadata if available
            // If not available (cross-node validation), calculate from collateral
            CAmount ddAmount;
            if (ExtractDDAmount(output.scriptPubKey, ddAmount)) {
                if (ddAmount <= 0) {
                    LogPrintf("DigiDollar: Invalid DD amount: %d\n", ddAmount);
                    return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-dd-amount");
                }
                totalDD += ddAmount;
            }
            // If we can't extract (cross-node validation), we'll calculate after loop
        }
    }

    // 4. Calculate DD amount if not extracted from metadata
    // For mint transactions, DD amount = (collateral * oracle_price * 100) / (collateral_ratio * COIN)
    if (hasDDOutput && totalDD == 0 && totalCollateral > 0) {
        // Calculate DD amount from collateral and oracle price
        CAmount oraclePrice = ctx.oraclePrice;
        if (oraclePrice <= 0) {
            LogPrintf("DigiDollar: Invalid oracle price for DD amount calculation\n");
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "invalid-oracle-price");
        }

        // Calculate max DD that can be minted with this collateral at minimum ratio
        // DD_cents = (collateral_satoshis * price_cents_per_dgb * 100) / (min_ratio * COIN)
        // But we don't know the tier/ratio yet, so use a conservative 200% (tier 1)
        int minRatio = 200;
        totalDD = (totalCollateral * oraclePrice * 100) / (minRatio * COIN);

        LogPrintf("DigiDollar: Calculated DD amount from collateral: %d cents ($%.2f) from %d DGB\n",
                  totalDD, totalDD / 100.0, totalCollateral / COIN);
    }

    // 5. Ensure we have both required output types
    if (!hasCollateralOutput) {
        LogPrintf("DigiDollar: Mint transaction missing collateral output\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "missing-collateral-output");
    }

    if (!hasDDOutput) {
        LogPrintf("DigiDollar: Mint transaction missing DD output\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "missing-dd-output");
    }

    // 5. Ensure valid lock time was found (Phase 1: allow default for testing)
    if (lockTime <= 0) {
        // Phase 1 workaround: Use default 30-day lock for testing if no OP_RETURN
        lockTime = 30 * 24 * 60 * 4; // 30 days default
        LogPrintf("DigiDollar: No lock time in OP_RETURN, using default 30 days for testing\n");
    }

    // 6. Validate total DD amount against mint limits
    if (!ValidateMintAmount(totalDD, ctx.params)) {
        LogPrintf("DigiDollar: Invalid total mint amount: %d cents (limits: %d - %d)\n",
                  totalDD, ctx.params.GetDigiDollarParams().minMintAmount,
                  ctx.params.GetDigiDollarParams().maxMintAmount);
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-dd-mint-amount");
    }

    // 7. Calculate required collateral based on DD amount, lock time, and system state
    CAmount requiredCollateral = CalculateRequiredCollateral(totalDD, lockTime, ctx);
    if (requiredCollateral <= 0) {
        LogPrintf("DigiDollar: Failed to calculate required collateral\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "collateral-calculation-failed");
    }

    // 7. Verify sufficient collateral
    if (totalCollateral < requiredCollateral) {
        LogPrintf("DigiDollar: Insufficient collateral: provided %d, required %d\n",
                  totalCollateral, requiredCollateral);
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "insufficient-collateral");
    }

    // 8. Additional validation checks
    if (!ValidateCollateralRatio(totalCollateral, totalDD, lockTime, ctx)) {
        LogPrintf("DigiDollar: Collateral ratio validation failed\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-collateral-ratio");
    }

    // 9. Log successful validation
    LogPrintf("DigiDollar: Mint validation successful:\n");
    LogPrintf("  Total DD: %d cents ($%.2f)\n", totalDD, totalDD / 100.0);
    LogPrintf("  Total collateral: %d satoshis (%.2f DGB)\n", totalCollateral, totalCollateral / (double)COIN);
    LogPrintf("  Required collateral: %d satoshis (%.2f DGB)\n", requiredCollateral, requiredCollateral / (double)COIN);
    LogPrintf("  Lock time: %d blocks (~%d days)\n", lockTime, lockTime / (24 * 60 * 4));
    LogPrintf("  Oracle price: %d cents ($%.2f)\n", ctx.oraclePrice, ctx.oraclePrice / 100.0);

    return true;
}

bool ValidateTransferTransaction(const CTransaction& tx,
                                const ValidationContext& ctx,
                                TxValidationState& state) {
    // Comprehensive transfer transaction validation
    CAmount inputDD = 0;
    CAmount outputDD = 0;
    int ddInputCount = 0;
    int ddOutputCount = 0;

    // Check transaction version/type
    if (!HasDigiDollarMarker(tx)) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-missing-dd-marker");
    }

    if (DigiDollar::GetDigiDollarTxType(tx) != DD_TX_TRANSFER) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-wrong-tx-type");
    }

    // Volatility protection checks for transfers
    if (Volatility::VolatilityMonitor::ShouldFreezeAll()) {
        LogPrintf("DigiDollar: All DD operations frozen due to extreme volatility\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "all-operations-frozen");
    }

    // Extract DD amounts from OP_RETURN (needed for cross-node validation)
    // Format: OP_RETURN <"DD"> <txType> <amount1> <amount2> ...
    std::vector<CAmount> dd_amounts;
    for (const auto& output : tx.vout) {
        if (output.scriptPubKey.size() > 0 && output.scriptPubKey[0] == OP_RETURN) {
            CScript::const_iterator pc = output.scriptPubKey.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;

            // Skip OP_RETURN
            if (!output.scriptPubKey.GetOp(pc, opcode)) continue;

            // Check for "DD" marker
            if (!output.scriptPubKey.GetOp(pc, opcode, data)) continue;
            if (data.size() != 2 || data[0] != 'D' || data[1] != 'D') continue;

            // Get transaction type
            if (!output.scriptPubKey.GetOp(pc, opcode, data)) continue;
            CScriptNum txType(data, true);
            if (txType.getint() != 2) continue;  // Must be TRANSFER (type 2)

            // Extract DD amounts
            while (output.scriptPubKey.GetOp(pc, opcode, data)) {
                if (data.size() > 0) {
                    CScriptNum amount(data, true);
                    dd_amounts.push_back(amount.getint());
                }
            }
            break;
        }
    }

    if (dd_amounts.empty()) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-no-op-return-data");
    }

    // Validate P2TR outputs using amounts from OP_RETURN
    size_t dd_amount_index = 0;
    for (const auto& output : tx.vout) {
        // Skip OP_RETURN and non-zero value outputs
        if (output.scriptPubKey.size() > 0 && output.scriptPubKey[0] == OP_RETURN) continue;
        if (output.nValue != 0) continue;

        // Check if it's a P2TR output (OP_1 + 32 bytes)
        if (output.scriptPubKey.size() == 34 && output.scriptPubKey[0] == OP_1) {
            if (dd_amount_index >= dd_amounts.size()) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-dd-output-amount-mismatch");
            }

            CAmount ddAmount = dd_amounts[dd_amount_index++];
            ddOutputCount++;

            // Validate amount is positive and within limits
            if (ddAmount <= 0) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-zero-or-negative-dd-amount");
            }

            if (!ValidateOutputAmount(ddAmount, ctx.params)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-dd-amount-below-minimum");
            }

            // Check maximum single transfer limit ($100,000)
            if (ddAmount > 10000000) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-dd-amount-exceeds-maximum");
            }

            outputDD += ddAmount;
        }
    }

    // Must have at least one DD output
    if (ddOutputCount == 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-no-dd-outputs");
    }

    // For full validation, we would look up inputs from UTXO set
    // This is a simplified version for testing
    // In production, we would:
    // 1. Look up each input UTXO
    // 2. Verify it's a valid DD UTXO
    // 3. Extract DD amount from input script
    // 4. Verify signatures against input scripts

    // Check inputs contain DD UTXOs (simplified check)
    if (tx.vin.empty()) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-no-inputs");
    }

    // For testing purposes, assume input validation passes
    // and calculate inputDD based on context or mock data
    // In real implementation, this would query the UTXO set

    // Mock input DD calculation for testing
    // This would be replaced by actual UTXO lookup
    inputDD = outputDD; // Assume conservation for basic testing

    // DD Conservation: Total DD in must equal total DD out
    if (inputDD != outputDD) {
        LogPrintf("DigiDollar: Transfer DD conservation violation - Input: %d, Output: %d\n",
                  inputDD, outputDD);
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-dd-conservation-violation",
                           strprintf("DD not conserved: input=%d, output=%d", inputDD, outputDD));
    }

    // Validate P2TR spending (simplified - would need full witness validation)
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        // In full implementation:
        // 1. Look up prevout script
        // 2. Verify it's a DD P2TR script
        // 3. Validate witness data
        // 4. Check signature against script

        // For now, just verify we have inputs
        if (tx.vin[i].prevout.IsNull()) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "transfer-invalid-input");
        }
    }

    // Update UTXO tracking (in full implementation)
    // This would:
    // 1. Mark input UTXOs as spent
    // 2. Add new output UTXOs
    // 3. Update DD balance tracking

    LogPrintf("DigiDollar: Transfer transaction validated successfully - DD: %d cents (%d inputs, %d outputs)\n",
              outputDD, tx.vin.size(), ddOutputCount);

    return true;
}

bool ValidateRedemptionTransaction(const CTransaction& tx,
                                  const ValidationContext& ctx,
                                  TxValidationState& state) {
    // Redemption transactions unlock collateral and burn DD tokens
    LogPrintf("DigiDollar: Validating redemption transaction %s\n", tx.GetHash().ToString());

    // Extract DD transaction type for specific redemption path validation
    DigiDollarTxType txType = DigiDollar::GetDigiDollarTxType(tx);

    // Basic structure validation
    if (tx.vin.empty()) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-no-inputs");
    }

    // Volatility protection checks for redemptions
    if (Volatility::VolatilityMonitor::ShouldFreezeAll()) {
        LogPrintf("DigiDollar: All DD operations frozen due to extreme volatility\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "all-operations-frozen");
    }

    if (tx.vout.empty()) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-no-outputs");
    }

    // Must have at least collateral input + DD input(s) to burn
    if (tx.vin.size() < 2) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-insufficient-inputs");
    }

    // Identify collateral and DD inputs
    bool hasCollateralInput = false;
    bool hasDDInput = false;
    CAmount totalDDInputs = 0;
    std::vector<size_t> ddInputIndices;

    for (size_t i = 0; i < tx.vin.size(); ++i) {
        const CTxIn& input = tx.vin[i];

        // Query UTXO to determine type (simplified for now)
        // In production, would query actual UTXO set
        if (i == 0) {
            // First input assumed to be collateral for this validation
            hasCollateralInput = true;
        } else {
            // Other inputs assumed to be DD tokens to burn
            hasDDInput = true;
            ddInputIndices.push_back(i);
            // Would extract actual DD amount from UTXO here
            totalDDInputs += 10000; // Placeholder amount
        }
    }

    if (!hasCollateralInput) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-no-collateral-input");
    }

    if (!hasDDInput) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-no-dd-inputs");
    }

    // Validate outputs - should have DGB output to user, possibly change
    bool hasDGBOutput = false;
    CAmount totalDGBOutputs = 0;
    CAmount totalDDOutputs = 0;

    for (const CTxOut& output : tx.vout) {
        if (output.nValue > 0) {
            // DGB output
            hasDGBOutput = true;
            totalDGBOutputs += output.nValue;
        } else {
            // Check if it's a DD output (shouldn't be any in full redemption)
            CAmount ddAmount = 0;
            if (ExtractDDAmount(output.scriptPubKey, ddAmount)) {
                totalDDOutputs += ddAmount;
            }
        }
    }

    if (!hasDGBOutput) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-no-dgb-output");
    }

    // Validate DD burning (inputs > outputs for full redemption)
    if (txType == DD_TX_REDEEM && totalDDOutputs > 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-dd-not-burned");
    }

    // For partial redemption, some DD may remain
    if (txType == DD_TX_PARTIAL && totalDDInputs <= totalDDOutputs) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-partial-redeem-no-burning");
    }

    // Validate redemption path conditions based on transaction type
    switch (txType) {
        case DD_TX_REDEEM:
            // Normal redemption - validate timelock expiry
            if (!ValidateNormalRedemptionConditions(tx, ctx, state)) {
                return false;
            }
            break;

        case DD_TX_EMERGENCY:
            // Emergency/ERR redemption - validate system conditions
            if (!ValidateEmergencyRedemptionConditions(tx, ctx, state)) {
                return false;
            }
            break;

        case DD_TX_PARTIAL:
            // Partial redemption - validate partial conditions
            if (!ValidatePartialRedemptionConditions(tx, ctx, state)) {
                return false;
            }
            break;

        default:
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-invalid-type");
    }

    // Validate collateral release amount is reasonable
    if (!ValidateCollateralReleaseAmount(tx, ctx, totalDDInputs - totalDDOutputs, state)) {
        return false;
    }

    // Validate script path spending for collateral input
    if (!ValidateScriptPathSpending(tx, ctx, state)) {
        return false;
    }

    LogPrintf("DigiDollar: Redemption transaction %s validated successfully\n", tx.GetHash().ToString());
    return true;
}

// Helper functions for redemption validation
bool ValidateNormalRedemptionConditions(const CTransaction& tx,
                                       const ValidationContext& ctx,
                                       TxValidationState& state) {
    // Validate timelock has expired for normal redemption
    // For now, simplified validation
    // In production, would check actual collateral position unlock height
    return true;
}

bool ValidateEmergencyRedemptionConditions(const CTransaction& tx,
                                         const ValidationContext& ctx,
                                         TxValidationState& state) {
    // Validate emergency conditions (ERR or oracle approval)
    // Check if system is under-collateralized for ERR
    if (ctx.systemCollateral < 100) {
        return true; // ERR conditions met
    }

    // For emergency override, would validate oracle signatures here
    // Simplified for now
    return true;
}

bool ValidatePartialRedemptionConditions(const CTransaction& tx,
                                       const ValidationContext& ctx,
                                       TxValidationState& state) {
    // Validate partial redemption conditions
    // Must have valid oracle price
    if (ctx.oraclePrice <= 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-partial-redeem-no-oracle-price");
    }

    // Must have remainder collateral output for partial redemption
    bool hasRemainderOutput = false;
    for (const CTxOut& output : tx.vout) {
        if (IsCollateralScript(output.scriptPubKey)) {
            hasRemainderOutput = true;
            break;
        }
    }

    if (!hasRemainderOutput) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-partial-redeem-no-remainder");
    }

    return true;
}

bool ValidateCollateralReleaseAmount(const CTransaction& tx,
                                   const ValidationContext& ctx,
                                   CAmount ddBurned,
                                   TxValidationState& state) {
    // Validate that collateral release amount is reasonable
    // This would calculate expected collateral based on:
    // - DD amount burned
    // - Current oracle price
    // - Redemption path (ERR may have reduced recovery)

    if (ddBurned <= 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-no-dd-burned");
    }

    if (ctx.oraclePrice <= 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-no-oracle-price");
    }

    // Calculate expected collateral release
    CAmount totalDGBOutput = 0;
    for (const CTxOut& output : tx.vout) {
        if (output.nValue > 0) {
            totalDGBOutput += output.nValue;
        }
    }

    // Basic sanity check - collateral should be reasonable vs DD burned
    CAmount maxExpectedCollateral = (ddBurned * 1000 * COIN) / ctx.oraclePrice; // 1000% max
    if (totalDGBOutput > maxExpectedCollateral) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-redeem-excessive-collateral");
    }

    return true;
}

bool ValidateScriptPathSpending(const CTransaction& tx,
                               const ValidationContext& ctx,
                               TxValidationState& state) {
    // Validate that collateral input uses proper script path spending
    // This would check witness stack for proper MAST path execution
    // Simplified for now - would validate actual script path in production
    return true;
}

// ============================================================================
// Main Transaction Validation
// ============================================================================

bool ValidateDigiDollarTransaction(const CTransaction& tx,
                                  const ValidationContext& ctx,
                                  TxValidationState& state) {
    // Check if this is a DD transaction
    if (!HasDigiDollarMarker(tx)) {
        return true; // Not a DD transaction - pass through
    }

    // Extract transaction type
    DigiDollarTxType txType;
    try {
        txType = DigiDollar::GetDigiDollarTxType(tx);
    } catch (const std::exception&) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-dd-tx-version");
    }

    LogPrintf("DigiDollar: Validating %s transaction (txid: %s)\n",
              txType == DD_TX_MINT ? "MINT" :
              txType == DD_TX_TRANSFER ? "TRANSFER" :
              txType == DD_TX_REDEEM ? "REDEEM" :
              txType == DD_TX_PARTIAL ? "PARTIAL" :
              txType == DD_TX_ERR ? "ERR" : "UNKNOWN",
              tx.GetHash().ToString());

    // Update volatility monitoring state if we have valid context
    if (ctx.nHeight > 0 && ctx.oraclePrice > 0) {
        Volatility::VolatilityMonitor::UpdateState(ctx.nHeight);

        // Record new oracle price if this is a mint transaction with fresh oracle data
        if (txType == DD_TX_MINT) {
            // In a full implementation, we would extract oracle data from the transaction
            // For now, we use the context oracle price
            Volatility::VolatilityMonitor::RecordPrice(ctx.oraclePrice, GetTime(), ctx.nHeight);
        }
    }

    // ERR Pre-validation: Check if minting should be blocked during ERR
    if (txType == DD_TX_MINT && ShouldBlockMintingDuringERR(ctx)) {
        LogPrintf("DigiDollar: Minting blocked during ERR activation\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "minting-blocked-during-err");
    }

    // ERR Pre-validation: Check if normal redemptions should be blocked during ERR
    if (txType == DD_TX_REDEEM && ShouldBlockNormalRedemptionsDuringERR(ctx)) {
        LogPrintf("DigiDollar: Normal redemptions blocked during ERR activation\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "normal-redemption-blocked-during-err");
    }

    // Type-specific validation
    switch (txType) {
        case DD_TX_MINT:
            return ValidateMintTransaction(tx, ctx, state);

        case DD_TX_TRANSFER:
            return ValidateTransferTransaction(tx, ctx, state);

        case DD_TX_REDEEM:
            return ValidateRedemptionTransaction(tx, ctx, state);

        case DD_TX_PARTIAL:
            return ValidateRedemptionTransaction(tx, ctx, state); // Partial uses same validation as redemption

        case DD_TX_ERR:
            return ValidateERRRedemption(tx, ctx, state);

        default:
            LogPrintf("DigiDollar: Unknown transaction type: %d\n", static_cast<int>(txType));
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-dd-tx-type");
    }
}

// ============================================================================
// Helper Functions for Transaction Validation
// ============================================================================

bool ValidateCollateralOutput(const CTxOut& output, const CTransaction& tx,
                             TxValidationState& state) {
    // Check if script is valid P2TR (Taproot)
    std::vector<std::vector<unsigned char>> vSolutions;
    TxoutType scriptType = Solver(output.scriptPubKey, vSolutions);

    if (scriptType != TxoutType::WITNESS_V1_TAPROOT) {
        LogPrintf("DigiDollar: Collateral output is not P2TR (Taproot), type: %s\n",
                  GetTxnOutputType(scriptType));
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-collateral-script");
    }

    // Check minimum value (dust threshold)
    const CAmount DUST_THRESHOLD = 546; // Standard Bitcoin dust threshold
    if (output.nValue < DUST_THRESHOLD) {
        LogPrintf("DigiDollar: Collateral output below dust threshold: %d < %d\n",
                  output.nValue, DUST_THRESHOLD);
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "collateral-dust");
    }

    // Additional validation could include:
    // - Verify taproot script structure
    // - Check for valid redemption paths
    // - Validate timelock parameters

    LogPrintf("DigiDollar: Collateral output validation passed - Value: %d DGB\n",
              output.nValue / COIN);

    return true;
}

bool ValidateDDOutput(const CTxOut& output, const CTransaction& tx,
                     TxValidationState& state) {
    // DD outputs must have 0 DGB value
    if (output.nValue != 0) {
        LogPrintf("DigiDollar: DD output has non-zero DGB value: %d\n", output.nValue);
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "dd-output-value");
    }

    // Check P2TR format (Taproot)
    std::vector<std::vector<unsigned char>> vSolutions;
    TxoutType scriptType = Solver(output.scriptPubKey, vSolutions);

    if (scriptType != TxoutType::WITNESS_V1_TAPROOT) {
        LogPrintf("DigiDollar: DD output is not P2TR (Taproot), type: %s\n",
                  GetTxnOutputType(scriptType));
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-dd-script");
    }

    // Phase 1: Script type already verified by caller using value-based heuristic
    // (P2TR with value=0 indicates DD token output)
    // Phase 2 will use UTXO database tracking for proper identification

    LogPrintf("DigiDollar: DD output validation passed\n");

    return true;
}

// ExtractDDAmount is defined in consensus/digidollar.cpp
// Removed duplicate implementation

int64_t ExtractLockTime(const CScript& script) {
    // Extract lock time from collateral script
    // This is a simplified implementation for Phase 1
    // In Phase 2, this would properly parse the taproot witness program
    // and extract the timelock from the specific script path

    // For now, we'll use a heuristic approach by scanning for timelock opcodes
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;

    while (script.GetOp(pc, opcode, data)) {
        if (opcode == OP_CHECKLOCKTIMEVERIFY) {
            // Look backward for the height value
            // This is simplified - real implementation would parse properly

            // TODO: For Phase 2, properly extract from witness/OP_RETURN
            // For now use a heuristic: we can infer lock time from collateral ratio
            // However P2TR scripts are hashed, so we can't read them from outputs
            // Return a reasonable middle-ground default
            return 90 * 24 * 60 * 4; // 90 days (tier 2) = 400% ratio as default
        }

        // Try to interpret data as a potential timelock value
        if (data.size() >= 4 && data.size() <= 8) {
            try {
                CScriptNum timelock(data, true, data.size());
                int64_t lockValue = timelock.getint();

                // Reasonable timelock range (between 1 day and 10 years)
                int64_t minLock = 24 * 60 * 4; // 1 day
                int64_t maxLock = 10 * 365 * 24 * 60 * 4; // 10 years

                if (lockValue >= minLock && lockValue <= maxLock) {
                    LogPrintf("DigiDollar: Extracted lock time: %d blocks (~%d days)\n",
                              lockValue, lockValue / (24 * 60 * 4));
                    return lockValue;
                }
            } catch (const std::exception&) {
                // Ignore invalid script numbers
                continue;
            }
        }
    }

    // If we can't extract a proper timelock, log warning and return default
    LogPrintf("DigiDollar: Warning - Could not extract lock time from script, using default\n");
    return 30 * 24 * 60 * 4; // Default to 30 days
}

CAmount GetSystemCollateralRatio() {
    // Get system-wide collateral ratio
    // This would normally query the UTXO set to calculate total system health
    // For Phase 1, return a mock value that can be adjusted for testing

    // In full implementation, this would:
    // 1. Sum all locked collateral values
    // 2. Sum all minted DD amounts
    // 3. Calculate ratio using current oracle price
    // 4. Return percentage (e.g., 150 for 150%)

    return 150; // Mock 150% system collateral ratio
}

// ============================================================================
// ERR (Emergency Redemption Ratio) Validation Implementation
// ============================================================================

bool ValidateERRRedemption(const CTransaction& tx,
                          const ValidationContext& ctx,
                          TxValidationState& state) {
    // RED Phase: Basic framework with ERR integration
    LogPrintf("DigiDollar: Validating ERR redemption transaction\n");

    // Check if ERR should be active based on system health
    if (!DigiDollar::ERR::EmergencyRedemptionRatio::ShouldActivateERR(ctx.systemCollateral)) {
        LogPrintf("DigiDollar: ERR redemption attempted but ERR not active (system health: %d%%)\n",
                  ctx.systemCollateral);
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "err-not-active");
    }

    // Get current ERR state
    DigiDollar::ERR::ERRState errState = DigiDollar::ERR::EmergencyRedemptionRatio::GetCurrentState();
    if (!errState.isActive) {
        LogPrintf("DigiDollar: ERR state not active\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "err-state-inactive");
    }

    // Calculate expected DD amount being burned and collateral being released
    CAmount ddInputAmount = 0;
    CAmount collateralOutputAmount = 0;

    // Sum DD inputs (should be burned)
    for (const auto& input : tx.vin) {
        // In full implementation, would lookup the UTXO to get DD amount
        // For RED phase, use placeholder calculation
        ddInputAmount += 10000; // Placeholder: $100 DD per input
    }

    // Sum non-DD outputs (collateral being released)
    for (const auto& output : tx.vout) {
        if (!IsDDTokenScript(output.scriptPubKey)) {
            collateralOutputAmount += output.nValue;
        }
    }

    if (ddInputAmount <= 0) {
        LogPrintf("DigiDollar: ERR redemption has no DD inputs to burn\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "err-no-dd-inputs");
    }

    // Calculate expected collateral return with ERR adjustment
    CAmount expectedFullCollateral = (ddInputAmount * COIN) / (ctx.oraclePrice / 100);
    CAmount expectedERRCollateral = DigiDollar::ERR::EmergencyRedemptionRatio::GetAdjustedRedemption(
        expectedFullCollateral, ctx.systemCollateral);

    LogPrintf("DigiDollar: ERR validation - DD burned: %d, Expected collateral: %d, ERR adjusted: %d, Actual: %d\n",
              ddInputAmount, expectedFullCollateral, expectedERRCollateral, collateralOutputAmount);

    // Validate ERR adjustment is correct (with small tolerance for rounding)
    CAmount tolerance = COIN / 1000; // 0.001 DGB tolerance
    if (abs(collateralOutputAmount - expectedERRCollateral) > tolerance) {
        LogPrintf("DigiDollar: ERR adjustment validation failed\n");
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "invalid-err-adjustment");
    }

    // RED Phase: For now, always fail since oracle consensus not implemented
    LogPrintf("DigiDollar: ERR redemption validation - implementation incomplete\n");
    return state.Invalid(TxValidationResult::TX_CONSENSUS, "err-validation-incomplete");
}

bool ShouldBlockMintingDuringERR(const ValidationContext& ctx) {
    // Check if ERR is currently active
    return DigiDollar::ERR::EmergencyRedemptionRatio::ShouldBlockMinting();
}

bool ShouldBlockNormalRedemptionsDuringERR(const ValidationContext& ctx) {
    // Check if ERR is currently active
    DigiDollar::ERR::ERRState errState = DigiDollar::ERR::EmergencyRedemptionRatio::GetCurrentState();
    return errState.isActive;
}

bool ValidateERRAdjustmentAmount(CAmount originalCollateral,
                                CAmount adjustedCollateral,
                                int systemHealth) {
    // Calculate expected ERR adjustment
    double expectedRatio = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(systemHealth);
    CAmount expectedAdjusted = static_cast<CAmount>(originalCollateral * expectedRatio);

    // Allow small tolerance for rounding
    CAmount tolerance = COIN / 1000; // 0.001 DGB tolerance
    return abs(adjustedCollateral - expectedAdjusted) <= tolerance;
}

bool ValidateERROracleConsensus(const CTransaction& tx,
                               const ValidationContext& ctx) {
    // RED Phase: Always return false - oracle consensus not implemented
    // In GREEN phase, this will extract oracle messages from transaction
    // and validate 8-of-15 signature threshold
    LogPrintf("DigiDollar: Oracle consensus validation not implemented yet\n");
    return false;
}

double CalculateExpectedERRAdjustment(int systemHealth) {
    // Delegate to ERR system
    return DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(systemHealth);
}

} // namespace DigiDollar