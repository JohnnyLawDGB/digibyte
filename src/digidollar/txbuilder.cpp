// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <digidollar/txbuilder.h>
#include <digidollar/scripts.h>
#include <digidollar/digidollar.h>
#include <digidollar/validation.h>
#include <consensus/digidollar.h>
#include <script/standard.h>
#include <validation.h>
#include <base58.h>
#include <random.h>
#include <policy/policy.h>
#include <logging.h>

#include <algorithm>
#include <cassert>
#include <map>

// Forward declare to avoid namespace conflicts

namespace DigiDollar {

// Constants for mint transaction building
static const CAmount DUST_THRESHOLD = 1000;        // Minimum change output (1000 sats)
static const size_t ESTIMATED_TX_VSIZE = 250;      // Estimated transaction size in vB
static const int DEFAULT_SYSTEM_COLLATERAL = 150;   // Default system health (150%)
static const double MAX_FEE_RATIO = 0.5;           // Maximum fee as ratio of total input

// ============================================================================
// Base TxBuilder implementation
// ============================================================================

TxBuilder::TxBuilder(const CChainParams& params, int height, CAmount price)
    : chainParams(params), currentHeight(height), oraclePrice(price) {}

CAmount TxBuilder::CalculateFee(const CMutableTransaction& tx, CAmount feeRate) const {
    // Estimate transaction virtual size
    size_t vsize = EstimateTransactionVSize(tx);
    return (vsize * feeRate) / 1000;  // Convert sat/vB to total fee
}

bool TxBuilder::SelectCoins(const std::vector<COutPoint>& utxos, CAmount target,
                           std::vector<CTxIn>& inputs, CAmount& total) const {
    total = 0;
    inputs.clear();

    // Simple greedy selection - in production would use more sophisticated algorithm
    for (const auto& utxo : utxos) {
        // Use virtual GetUTXOValueVirtual to allow child classes to override UTXO lookup
        CAmount value = GetUTXOValueVirtual(utxo);
        if (value > 0) {
            inputs.push_back(CTxIn(utxo));
            total += value;
            if (total >= target) {
                return true;
            }
        }
    }
    return false;
}

CAmount TxBuilder::GetUTXOValue(const COutPoint& outpoint) const {
    // This is a simplified implementation
    // In production, this would query the UTXO set
    // For now, return a reasonable value for testing
    return 100 * COIN; // 100 DGB placeholder
}

CAmount TransferTxBuilder::GetDGBFromUTXO(const COutPoint& outpoint) const {
    // This is a simplified implementation
    // In production, this would query the UTXO set
    // For now, return a reasonable value for testing
    return 100 * COIN; // 100 DGB placeholder
}

bool TxBuilder::ValidateAmount(CAmount amount) const {
    return amount > 0 && amount <= MAX_MONEY;
}

bool TxBuilder::ValidateFeeRate(CAmount feeRate) const {
    // Fee rate is in sat/kB (used in formula: (vsize * feeRate) / 1000)
    // DigiByte minimum relay fee: 100,000 sat/kB (0.001 DGB/kB)
    // For 1 DGB minimum fee on ~200 vB tx: need ~500,000 sat/kB
    // Allow up to 10 DGB for flexibility: 5,000,000 sat/kB
    return feeRate >= 100000 && feeRate <= 5000000; // 100k to 5M sat/kB
}

// ============================================================================
// MintTxBuilder implementation
// ============================================================================

CAmount MintTxBuilder::CalculateRequiredCollateral(CAmount ddAmount, int lockDays) const {
    // Validate inputs
    if (ddAmount <= 0) {
        return 0;
    }

    if (oraclePrice <= 0) {
        return 0; // Cannot calculate without valid oracle price
    }

    // Convert days to blocks
    int64_t lockBlocks = LockDaysToBlocks(lockDays);

    // Get base collateral ratio
    const auto& ddParams = chainParams.GetDigiDollarParams();
    int baseRatio = GetCollateralRatioForLockTime(lockBlocks, ddParams);

    // Apply DCA if needed (check actual system health)
    int systemCollateral = GetCurrentSystemCollateral(); // Would query chain state
    double dcaMultiplier = GetDCAMultiplier(systemCollateral, ddParams);
    // baseRatio is already a percentage (e.g., 500 for 500%)
    // dcaMultiplier is a decimal (e.g., 1.0 for no adjustment, 1.5 for 50% increase)
    double adjustedRatio = baseRatio * dcaMultiplier;

    // Calculate required DGB
    // DD amount is in cents, oracle price is in cents per DGB
    CAmount usdValue = ddAmount; // DD amount = USD value in cents

    LogPrintf("DigiDollar TxBuilder: CalculateRequiredCollateral - DD: %d cents, Price: %d cents/DGB, BaseRatio: %d%%, DCA: %.2f, AdjustedRatio: %.2f%%\n",
              ddAmount, oraclePrice, baseRatio, dcaMultiplier, adjustedRatio);

    // Use 64-bit arithmetic to prevent overflow
    uint64_t dgbFor100Percent = (static_cast<uint64_t>(usdValue) * static_cast<uint64_t>(COIN)) / static_cast<uint64_t>(oraclePrice);
    // adjustedRatio is a percentage (e.g., 500 for 500%), convert to multiplier by dividing by 100
    uint64_t requiredCollateral = (dgbFor100Percent * static_cast<uint64_t>(adjustedRatio)) / 100;

    LogPrintf("DigiDollar TxBuilder: - DGB for 100%%: %llu sats, Required collateral: %llu sats (%.8f DGB)\n",
              dgbFor100Percent, requiredCollateral, requiredCollateral / 100000000.0);

    // Check for overflow
    if (requiredCollateral > static_cast<uint64_t>(MAX_MONEY)) {
        return 0; // Amount too large
    }

    return static_cast<CAmount>(requiredCollateral);
}

int64_t MintTxBuilder::LockDaysToBlocks(int days) const {
    return DigiDollar::LockDaysToBlocks(days);
}

CScript MintTxBuilder::CreateCollateralScript(const TxBuilderMintParams& params) const {
    // Create P2TR collateral locking script using the scripts.h MintParams
    // Use global namespace to access the correct MintParams from scripts.h
    ::DigiDollar::MintParams scriptParams;
    scriptParams.ddAmount = params.ddAmount;
    scriptParams.lockHeight = currentHeight + LockDaysToBlocks(params.lockDays);

    CPubKey pubkey = params.ownerKey.GetPubKey();
    scriptParams.ownerKey = XOnlyPubKey(pubkey);
    scriptParams.internalKey = XOnlyPubKey(pubkey);
    scriptParams.oracleKeys = GetOracleKeys(15); // Mock oracle keys

    return CreateCollateralP2TR(scriptParams);
}

CScript MintTxBuilder::CreateDDOutputScript(const CKey& owner, CAmount amount) const {
    CPubKey pubkey = owner.GetPubKey();
    return CreateDigiDollarP2TR(XOnlyPubKey(pubkey), amount);
}

bool MintTxBuilder::ValidateMintParams(const TxBuilderMintParams& params) const {
    const auto& ddParams = chainParams.GetDigiDollarParams();

    // Validate amount range (check for zero, negative, and excessive amounts)
    if (params.ddAmount <= 0) {
        LogPrintf("ValidateMintParams FAILED: ddAmount <= 0 (%d)\n", params.ddAmount);
        return false;
    }

    // Convert cents to CENT encoding for validation
    // params.ddAmount is in cents (100 cents = $1.00)
    // CENT represents $1.00 in satoshis (despite the misleading name)
    // So to convert: cents / 100 * CENT
    if (!IsValidMintAmount((params.ddAmount / 100) * CENT, ddParams)) {
        LogPrintf("ValidateMintParams FAILED: IsValidMintAmount returned false for %d cents\n", params.ddAmount);
        return false;
    }

    // Validate lock period (30 days to 10 years)
    if (params.lockDays < 30 || params.lockDays > 10 * 365) {
        LogPrintf("ValidateMintParams FAILED: lockDays out of range (%d)\n", params.lockDays);
        return false;
    }

    // Validate key
    if (!params.ownerKey.IsValid()) {
        LogPrintf("ValidateMintParams FAILED: ownerKey is invalid\n");
        return false;
    }

    // Validate fee rate (must be reasonable)
    if (!ValidateFeeRate(params.feeRate)) {
        LogPrintf("ValidateMintParams FAILED: fee rate invalid (%d)\n", params.feeRate);
        return false;
    }

    // Validate that UTXOs are provided
    if (params.utxos.empty()) {
        LogPrintf("ValidateMintParams FAILED: no UTXOs provided\n");
        return false;
    }

    // Additional sanity checks
    if (params.ddAmount > MAX_DIGIDOLLAR) {
        LogPrintf("ValidateMintParams FAILED: ddAmount > MAX_DIGIDOLLAR (%d > %d)\n", params.ddAmount, MAX_DIGIDOLLAR);
        return false;
    }

    LogPrintf("ValidateMintParams PASSED\n");
    return true;
}

int TxBuilder::GetCurrentSystemCollateral() const {
    // Placeholder implementation - in production this would:
    // 1. Query the UTXO set for all DigiDollar collateral positions
    // 2. Calculate total DGB locked vs total DD minted
    // 3. Apply current oracle price to get system-wide collateral ratio
    // For now, return a conservative default
    return DEFAULT_SYSTEM_COLLATERAL;
}

CKey MintTxBuilder::GenerateChangeKey() const {
    // Generate a new key for change
    CKey changeKey;
    changeKey.MakeNewKey(true);
    return changeKey;
}

TxBuilderResult MintTxBuilder::BuildMintTransaction(const TxBuilderMintParams& params) {
    TxBuilderResult result;

    // Validate parameters
    if (!ValidateMintParams(params)) {
        result.error = "Invalid mint parameters";
        return result;
    }

    // Check oracle price availability
    if (oraclePrice <= 0) {
        result.error = "Oracle price unavailable or invalid";
        return result;
    }

    // Create transaction
    CMutableTransaction tx;
    tx.SetDigiDollarType(::DD_TX_MINT);

    // Calculate required collateral
    result.collateralRequired = CalculateRequiredCollateral(params.ddAmount, params.lockDays);

    // Check if collateral calculation failed
    if (result.collateralRequired <= 0) {
        result.error = "Failed to calculate required collateral";
        return result;
    }

    // Estimate fees (rough estimate before final inputs are selected)
    CAmount estimatedFees = ESTIMATED_TX_VSIZE * params.feeRate / 1000;

    // Sanity check on total required amount
    if (result.collateralRequired > MAX_MONEY - estimatedFees) {
        result.error = "Required collateral amount too large";
        return result;
    }

    // Select inputs for collateral + fees
    std::vector<CTxIn> inputs;
    CAmount totalIn = 0;
    if (!SelectCoins(params.utxos, result.collateralRequired + estimatedFees,
                     inputs, totalIn)) {
        result.error = "Insufficient funds for collateral and fees";
        return result;
    }

    tx.vin = inputs;

    // Create collateral output (P2TR)
    CScript collateralScript = CreateCollateralScript(params);
    tx.vout.push_back(CTxOut(result.collateralRequired, collateralScript));

    // Create DD output (P2TR) - 0 DGB value, amount in witness/script
    CScript ddScript = CreateDDOutputScript(params.ownerKey, params.ddAmount);
    tx.vout.push_back(CTxOut(0, ddScript));

    // Calculate actual fees and change
    result.totalFees = CalculateFee(tx, params.feeRate);

    // Ensure fees are reasonable
    if (result.totalFees > static_cast<CAmount>(totalIn * MAX_FEE_RATIO)) {
        result.error = "Transaction fees too high";
        return result;
    }

    CAmount change = totalIn - result.collateralRequired - result.totalFees;

    if (change < 0) {
        result.error = "Insufficient funds after fee calculation";
        return result;
    } else if (change > 0) {
        // Only create change output if amount is above dust threshold
        if (change >= DUST_THRESHOLD) {
            // Create change output
            CKey changeKey = GenerateChangeKey();
            CPubKey changePubkey = changeKey.GetPubKey();
            CTxDestination changeDest{WitnessV1Taproot(XOnlyPubKey(changePubkey))};
            tx.vout.push_back(CTxOut(change, GetScriptForDestination(changeDest)));
        } else {
            // Small change goes to fee (dust avoidance)
            result.totalFees += change;
        }
    }

    result.tx = tx;
    result.success = true;
    return result;
}

// ============================================================================
// TransferTxBuilder implementation
// ============================================================================

bool TransferTxBuilder::ValidateDDAddress(const std::string& address) const {
    return CDigiDollarAddress::IsValidDigiDollarAddress(address);
}

CAmount TransferTxBuilder::GetDDFromUTXO(const COutPoint& outpoint) const {
    // This would query the UTXO set to get the DD amount
    // In production, this would look up the output in the blockchain
    // and extract the DD amount from the script
    // For testing, return a reasonable default value (can be overridden in test subclass)
    return 5000; // Default: $50.00 in cents for testing
}

bool TransferTxBuilder::ValidateTransferParams(const TxBuilderTransferParams& params) const {
    // Must have recipients
    if (params.recipients.empty()) {
        LogPrintf("DigiDollar: ValidateTransferParams FAILED - No recipients\n");
        return false;
    }

    // Validate all recipient addresses and amounts
    const auto& ddParams = chainParams.GetDigiDollarParams();
    CAmount minOutput = DigiDollar::GetMinimumDDOutput(ddParams);
    CAmount totalOutput = 0;
    for (const auto& [address, amount] : params.recipients) {
        // Validate address format
        if (!ValidateDDAddress(address)) {
            LogPrintf("DigiDollar: ValidateTransferParams FAILED - Invalid address: %s\n", address);
            return false;
        }

        // Validate amount ranges
        if (amount <= 0) {
            LogPrintf("DigiDollar: ValidateTransferParams FAILED - Non-positive amount: %d\n", amount);
            return false; // No zero or negative amounts
        }

        if (amount < minOutput) {
            LogPrintf("DigiDollar: ValidateTransferParams FAILED - Below dust threshold: %d < %d\n", amount, minOutput);
            return false; // Below dust threshold
        }

        // Check maximum single transfer limit ($100,000)
        if (amount > 10000000) { // $100,000.00 in cents
            LogPrintf("DigiDollar: ValidateTransferParams FAILED - Exceeds max transfer: %d > 10000000\n", amount);
            return false;
        }

        totalOutput += amount;
    }

    // Must have DD inputs
    if (params.ddUtxos.empty()) {
        LogPrintf("DigiDollar: ValidateTransferParams FAILED - No DD UTXOs\n");
        return false;
    }

    // Validate key
    if (!params.spenderKey.IsValid()) {
        LogPrintf("DigiDollar: ValidateTransferParams FAILED - Invalid spender key\n");
        return false;
    }

    // Validate fee rate
    if (!ValidateFeeRate(params.feeRate)) {
        LogPrintf("DigiDollar: ValidateTransferParams FAILED - Invalid fee rate: %d\n", params.feeRate);
        return false;
    }

    LogPrintf("DigiDollar: ValidateTransferParams PASSED\n");
    return true;
}

CAmount TransferTxBuilder::CalculateTotalDDInputs(const std::vector<COutPoint>& ddUtxos) const {
    CAmount total = 0;
    for (const auto& utxo : ddUtxos) {
        total += GetDDFromUTXO(utxo);
    }
    return total;
}

CAmount TransferTxBuilder::CalculateTotalDDOutputs(const std::vector<std::pair<std::string, CAmount>>& recipients) const {
    CAmount total = 0;
    for (const auto& [address, amount] : recipients) {
        total += amount;
    }
    return total;
}

// New functions for enhanced transfer functionality

CAmount TransferTxBuilder::CalculateTotalDDInput(const std::vector<CTxOut>& inputs,
                                                const std::vector<CAmount>& amounts) const {
    CAmount total = 0;

    // If amounts are provided, use them (for testing/mocking)
    if (!amounts.empty()) {
        for (CAmount amount : amounts) {
            total += amount;
        }
        return total;
    }

    // Otherwise extract from actual outputs
    for (const auto& output : inputs) {
        CAmount ddAmount = 0;
        if (DigiDollar::ExtractDDAmount(output.scriptPubKey, ddAmount)) {
            total += ddAmount;
        }
    }

    return total;
}

CScript TransferTxBuilder::CreateDDTransferScript(const CPubKey& recipient, CAmount amount) const {
    // Create P2TR script for DD transfer
    XOnlyPubKey xonly(recipient);
    return CreateDigiDollarP2TR(xonly, amount);
}

bool TransferTxBuilder::SelectDDInputs(const std::vector<CTxOut>& available, CAmount needed,
                                      std::vector<CTxOut>& selected, CAmount& total) {
    selected.clear();
    total = 0;

    // Simple greedy selection
    for (const auto& output : available) {
        CAmount ddAmount = 0;
        if (DigiDollar::ExtractDDAmount(output.scriptPubKey, ddAmount) && ddAmount > 0) {
            selected.push_back(output);
            total += ddAmount;

            if (total >= needed) {
                return true; // Found sufficient inputs
            }
        }
    }

    return total >= needed;
}

TxBuilderResult TransferTxBuilder::BuildTransferTransaction(const TxBuilderTransferParams& params) {
    TxBuilderResult result;

    // Validate parameters
    if (!ValidateTransferParams(params)) {
        result.error = "Invalid transfer parameters";
        return result;
    }

    // Calculate totals and check DD conservation
    CAmount totalDDIn = 0;
    if (!params.ddAmounts.empty() && params.ddAmounts.size() == params.ddUtxos.size()) {
        // Use provided amounts
        for (CAmount amount : params.ddAmounts) {
            totalDDIn += amount;
        }
    } else {
        // Fallback to UTXO lookup
        totalDDIn = CalculateTotalDDInputs(params.ddUtxos);
    }
    CAmount totalDDOut = CalculateTotalDDOutputs(params.recipients);

    // Strict DD conservation check
    if (totalDDIn < totalDDOut) {
        result.error = "Insufficient DD balance for transfer";
        return result;
    }

    // Check for dust outputs
    const auto& ddParams = chainParams.GetDigiDollarParams();
    CAmount minOutput = DigiDollar::GetMinimumDDOutput(ddParams);
    for (const auto& [address, amount] : params.recipients) {
        if (amount < minOutput) {
            result.error = "Transfer amount below dust threshold";
            return result;
        }
    }

    // Create transaction with DD transfer marker
    CMutableTransaction tx;
    tx.SetDigiDollarType(::DD_TX_TRANSFER);

    // Add DD inputs
    for (const auto& utxo : params.ddUtxos) {
        tx.vin.push_back(CTxIn(utxo));
    }

    // Add DGB fee inputs (after DD inputs)
    // Note: Phase 2.1 already selected these UTXOs, so we just add them directly
    LogPrintf("DigiDollar: TxBuilder - feeUtxos.size=%d, feeAmounts.size=%d\n",
              params.feeUtxos.size(), params.feeAmounts.size());

    CAmount totalFeeIn = 0;
    for (size_t i = 0; i < params.feeUtxos.size(); ++i) {
        const auto& utxo = params.feeUtxos[i];
        tx.vin.push_back(CTxIn(utxo));
        // Get actual fee UTXO amount from feeAmounts
        CAmount feeAmount = (i < params.feeAmounts.size()) ? params.feeAmounts[i] : GetDGBFromUTXO(utxo);
        LogPrintf("DigiDollar: Fee input %d - using %s: %d sats\n",
                  i, (i < params.feeAmounts.size()) ? "feeAmounts[i]" : "GetDGBFromUTXO()", feeAmount);
        totalFeeIn += feeAmount;
        LogPrintf("DigiDollar: Added fee input %s:%d (%d sats)\n",
                  utxo.hash.ToString(), utxo.n, feeAmount);
    }

    // Add DD outputs for recipients (all with 0 DGB value)
    LogPrintf("DigiDollar: TxBuilder - recipients.size=%d\n", params.recipients.size());
    for (const auto& [address, amount] : params.recipients) {
        LogPrintf("DigiDollar: Creating DD output - address=%s, amount=%d cents\n", address, amount);
        CTxDestination dest = DecodeDigiDollarAddress(address);
        const auto* taproot = std::get_if<WitnessV1Taproot>(&dest);
        if (!taproot) {
            result.error = "Failed to decode DD address: " + address;
            return result;
        }

        CScript ddScript = CreateDigiDollarP2TR(*taproot, amount);
        tx.vout.push_back(CTxOut(0, ddScript)); // DD outputs always have 0 DGB value
        LogPrintf("DigiDollar: Added DD output for %s: %d cents\n", address, amount);
    }

    // Add DD change output if needed
    CAmount ddChange = totalDDIn - totalDDOut;
    if (ddChange > 0) {
        // Only create change if above dust threshold
        if (ddChange >= minOutput) {
            CPubKey changePubkey = params.spenderKey.GetPubKey();
            CScript changeScript = CreateDigiDollarP2TR(XOnlyPubKey(changePubkey), ddChange);
            tx.vout.push_back(CTxOut(0, changeScript));
            LogPrintf("DigiDollar: Added DD change output: %d cents\n", ddChange);
        } else {
            // If change is dust, add it to fees (this violates strict conservation but handles dust)
            result.error = "DD change amount is below dust threshold";
            return result;
        }
    }

    // Calculate actual fee based on transaction with all outputs
    const CAmount MIN_DD_FEE = 10000000;  // 0.1 DGB minimum fee
    CAmount calculatedFee = CalculateFee(tx, params.feeRate);
    CAmount actualFee = std::max(calculatedFee, MIN_DD_FEE);

    LogPrintf("DigiDollar: Fee calculation - calculated: %d sats, minimum: %d sats, actual: %d sats\n",
              calculatedFee, MIN_DD_FEE, actualFee);

    // Add DGB change output if needed (after we know actual fee)
    if (totalFeeIn > 0) {
        CAmount dgbChange = totalFeeIn - actualFee;
        if (dgbChange > 0 && dgbChange >= DUST_THRESHOLD) {
            // Create DGB change output
            CPubKey changePubkey = params.spenderKey.GetPubKey();
            CTxDestination changeDest{WitnessV1Taproot(XOnlyPubKey(changePubkey))};
            CScript dgbChangeScript = GetScriptForDestination(changeDest);
            tx.vout.push_back(CTxOut(dgbChange, dgbChangeScript));
            LogPrintf("DigiDollar: Added DGB change output: %d sats\n", dgbChange);
        }
    }

    // Final validation - ensure DD conservation
    CAmount finalDDOut = 0;
    for (const auto& output : tx.vout) {
        CAmount ddAmount = 0;
        if (DigiDollar::ExtractDDAmount(output.scriptPubKey, ddAmount)) {
            finalDDOut += ddAmount;
        }
    }

    if (totalDDIn != finalDDOut) {
        result.error = "DD conservation violation: input=" + std::to_string(totalDDIn) +
                      " output=" + std::to_string(finalDDOut);
        return result;
    }

    // Set actual fees (already calculated above)
    result.totalFees = actualFee;

    // ============================================================================
    // Phase 2.6: Transaction Finalization
    // ============================================================================

    // Set transaction version - use DigiDollar marker to bypass dust checks
    // Format: bits 0-15 = 0x0770 (marker), bits 24-31 = transaction type
    // DD_TX_TRANSFER = 2, so version = 0x02000770
    tx.nVersion = 0x02000770;

    // Set locktime (0 for immediate broadcast)
    tx.nLockTime = 0;

    // Validate transaction structure - must have inputs
    if (tx.vin.empty()) {
        result.error = "Transaction has no inputs";
        return result;
    }

    // Validate transaction structure - must have outputs
    if (tx.vout.empty()) {
        result.error = "Transaction has no outputs";
        return result;
    }

    // Verify DD amounts balance (conservation check)
    CAmount totalDDInCheck = 0;
    for (const auto& utxo : params.ddUtxos) {
        totalDDInCheck += GetDDFromUTXO(utxo);
    }

    CAmount totalDDOutCheck = 0;
    for (const auto& [addr, amt] : params.recipients) {
        totalDDOutCheck += amt;
    }
    // Add DD change if exists
    if (ddChange > 0 && ddChange >= minOutput) {
        totalDDOutCheck += ddChange;
    }

    if (totalDDInCheck < totalDDOutCheck) {
        result.error = strprintf("DD amount mismatch: in=%d, out=%d", totalDDInCheck, totalDDOutCheck);
        return result;
    }

    // Transaction is valid - finalize
    result.tx = tx;
    result.success = true;

    LogPrintf("DigiDollar: Transaction finalized - %d inputs, %d outputs, version=%d, locktime=%d\n",
              tx.vin.size(), tx.vout.size(), tx.nVersion, tx.nLockTime);

    return result;
}

// ============================================================================
// RedeemTxBuilder implementation
// ============================================================================

bool RedeemTxBuilder::ValidateRedemptionPath(const TxBuilderRedeemParams& params) const {
    // Validate that the chosen path is valid for current conditions
    switch (params.path) {
        case RedemptionPath::NORMAL:
            // Would check if timelock has expired
            return true; // Simplified
        case RedemptionPath::EMERGENCY:
            // Would check if oracle approval exists
            return true; // Simplified
        case RedemptionPath::PARTIAL:
            // Would check if partial redemption is allowed
            return true; // Simplified
        case RedemptionPath::ERR:
            // Would check if system collateral < 100%
            return true; // Simplified
        default:
            return false;
    }
}

CAmount RedeemTxBuilder::CalculateRedemptionAmount(const TxBuilderRedeemParams& params) const {
    // Get collateral position data
    CCollateralPosition position = GetCollateralPosition(params.collateralOutpoint);

    // Calculate DGB to release based on DD burned and current conditions
    // This is simplified - actual implementation would consider:
    // - Current oracle price
    // - Redemption path specifics
    // - Partial vs full redemption

    if (params.path == RedemptionPath::ERR) {
        // ERR path: may get less DGB due to system under-collateralization
        return position.dgbLocked * 90 / 100; // 90% recovery simplified
    } else {
        // Normal/emergency path: full collateral recovery
        return position.dgbLocked;
    }
}

bool RedeemTxBuilder::ValidateRedeemParams(const TxBuilderRedeemParams& params) const {
    // Validate DD amount
    if (params.ddToRedeem <= 0) {
        return false;
    }

    // Validate redemption path
    if (!ValidateRedemptionPath(params)) {
        return false;
    }

    // Validate key
    if (!params.ownerKey.IsValid()) {
        return false;
    }

    // Validate fee rate
    if (!ValidateFeeRate(params.feeRate)) {
        return false;
    }

    // Validate collateral outpoint
    if (params.collateralOutpoint.IsNull()) {
        return false;
    }

    // Validate DD UTXOs
    if (params.ddUtxos.empty()) {
        return false;
    }

    return true;
}

CScript RedeemTxBuilder::CreateRedemptionScript(RedemptionPath path, const CKey& owner) const {
    // Create the appropriate redemption script based on path
    // This would use the script functions from scripts.h
    // For now, return a placeholder
    return CScript();
}

CCollateralPosition RedeemTxBuilder::GetCollateralPosition(const COutPoint& outpoint) const {
    // This would query the chain state for collateral position details
    // For testing, return a mock position that allows immediate redemption
    CCollateralPosition position;
    position.outpoint = outpoint;
    position.dgbLocked = 1000 * COIN; // 1000 DGB
    position.ddMinted = 50000; // $500 in cents
    position.unlockHeight = currentHeight - 100; // Already unlocked (in the past)
    position.collateralRatio = 300;
    return position;
}

TxBuilderResult RedeemTxBuilder::BuildRedemptionTransaction(const TxBuilderRedeemParams& params) {
    TxBuilderResult result;

    // Validate parameters
    if (!ValidateRedeemParams(params)) {
        result.error = "Invalid redemption parameters";
        return result;
    }

    // Check oracle price availability
    if (oraclePrice <= 0) {
        result.error = "Oracle price unavailable for redemption";
        return result;
    }

    // Verify redemption conditions are met
    if (!VerifyRedemptionConditions(params, params.path)) {
        result.error = "Redemption conditions not met for path " + std::to_string(static_cast<int>(params.path));
        return result;
    }

    // Create transaction
    CMutableTransaction tx;

    // Set type based on redemption path
    if (params.path == RedemptionPath::PARTIAL) {
        tx.SetDigiDollarType(::DD_TX_PARTIAL); // Use partial redemption type
    } else if (params.path == RedemptionPath::EMERGENCY ||
               params.path == RedemptionPath::ERR) {
        tx.SetDigiDollarType(::DD_TX_EMERGENCY); // Use emergency type
    } else {
        tx.SetDigiDollarType(::DD_TX_REDEEM);
    }

    // Add collateral input
    tx.vin.push_back(CTxIn(params.collateralOutpoint));

    // Add DD inputs to burn
    for (const auto& utxo : params.ddUtxos) {
        tx.vin.push_back(CTxIn(utxo));
    }

    // Add fee inputs if provided
    if (!params.feeUtxos.empty()) {
        std::vector<CTxIn> feeInputs;
        CAmount totalFeeIn = 0;
        CAmount estimatedFees = 300 * params.feeRate / 1000; // Rough estimate

        if (!SelectCoins(params.feeUtxos, estimatedFees, feeInputs, totalFeeIn)) {
            result.error = "Insufficient funds for fees";
            return result;
        }

        tx.vin.insert(tx.vin.end(), feeInputs.begin(), feeInputs.end());
    }

    // Calculate DGB to release
    CAmount dgbToRelease = CalculateRedemptionAmount(params);

    // Add DGB output to owner
    CPubKey pubkey = params.ownerKey.GetPubKey();
    CTxDestination dest{WitnessV1Taproot(XOnlyPubKey(pubkey))};
    tx.vout.push_back(CTxOut(dgbToRelease, GetScriptForDestination(dest)));

    // Handle partial redemption remainder
    if (params.path == RedemptionPath::PARTIAL) {
        // Create new collateral output for remainder
        // This would need to be implemented based on specific partial redemption logic
    }

    // Calculate fees and handle change
    result.totalFees = CalculateFee(tx, params.feeRate);

    result.tx = tx;
    result.success = true;
    return result;
}

RedemptionPath RedeemTxBuilder::DetermineRedemptionPath(const TxBuilderRedeemParams& params) const {
    // Analyze current conditions to determine optimal redemption path

    // Check if ERR conditions are met (system under-collateralized)
    if (GetCurrentSystemCollateral() < 100) {
        return RedemptionPath::ERR;
    }

    // Get collateral position to check timelock
    CCollateralPosition position = GetCollateralPosition(params.collateralOutpoint);

    // Check if normal redemption is available (timelock expired)
    if (currentHeight >= position.unlockHeight) {
        return RedemptionPath::NORMAL;
    }

    // Check if partial redemption is requested
    if (params.ddToRedeem < position.ddMinted) {
        return RedemptionPath::PARTIAL;
    }

    // Default to emergency path if normal conditions not met
    return RedemptionPath::EMERGENCY;
}

CAmount RedeemTxBuilder::CalculateCollateralReturn(CAmount ddAmount, CAmount originalCollateral,
                                                  CAmount currentPrice) const {
    // Calculate collateral return based on DD amount and current price
    if (ddAmount <= 0 || originalCollateral <= 0 || currentPrice <= 0) {
        return 0;
    }

    // For simplicity, return proportional amount of original collateral
    // In production, this would consider price changes and redemption path specifics
    return originalCollateral; // Simplified - return full collateral for now
}

bool RedeemTxBuilder::VerifyRedemptionConditions(const TxBuilderRedeemParams& params,
                                                RedemptionPath path) const {
    // Verify conditions are met for the specified redemption path

    CCollateralPosition position = GetCollateralPosition(params.collateralOutpoint);

    switch (path) {
        case RedemptionPath::NORMAL:
            // Check if timelock has expired
            return currentHeight >= position.unlockHeight;

        case RedemptionPath::EMERGENCY:
            // Check if emergency conditions are met (oracle approval would be verified here)
            return true; // Simplified - would check oracle signatures

        case RedemptionPath::PARTIAL:
            // Check if partial redemption is allowed and oracle price is valid
            return params.ddToRedeem < position.ddMinted && oraclePrice > 0;

        case RedemptionPath::ERR:
            // Check if system is under-collateralized
            return GetCurrentSystemCollateral() < 100;

        default:
            return false;
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

// LockDaysToBlocks, GetCollateralRatioForLockTime, and GetDCAMultiplier
// are implemented in consensus/digidollar.cpp

std::string EncodeDigiDollarAddress(const CTxDestination& dest, const CChainParams& chainParams) {
    const auto* taproot = std::get_if<WitnessV1Taproot>(&dest);
    if (!taproot) {
        return "";
    }

    // Determine network type from chain params
    int networkType;
    std::string chainType = chainParams.GetChainTypeString();
    if (chainType == "regtest") {
        networkType = CChainParams::DIGIDOLLAR_ADDRESS_REGTEST;
    } else if (chainType == "test") {
        networkType = CChainParams::DIGIDOLLAR_ADDRESS_TESTNET;
    } else {
        networkType = CChainParams::DIGIDOLLAR_ADDRESS;
    }

    // Use the CDigiDollarAddress class to encode
    CDigiDollarAddress addr;
    if (addr.SetDigiDollar(dest, networkType)) {
        return addr.ToString();
    }

    return "";
}

size_t EstimateTransactionVSize(const CMutableTransaction& tx) {
    // Simplified estimation - actual implementation would be more sophisticated
    size_t baseSize = ::GetSerializeSize(tx, PROTOCOL_VERSION);

    // Add witness overhead estimation
    size_t witnessSize = 0;
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        // Estimate P2TR witness size (signature + control block)
        witnessSize += 64 + 33; // signature + control block estimate
    }

    // Virtual size calculation: (base_size * 3 + total_size) / 4
    size_t totalSize = baseSize + witnessSize;
    return (baseSize * 3 + totalSize) / 4;
}

} // namespace DigiDollar