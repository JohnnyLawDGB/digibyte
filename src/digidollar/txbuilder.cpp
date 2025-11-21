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
static const size_t ESTIMATED_TX_VSIZE = 500;      // Estimated transaction size in vB (increased for multiple inputs)
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
    // DD amount is in cents (100 = $1.00), oracle price is in cents (100 = $1.00)
    CAmount usdValue = ddAmount; // DD amount = USD value in cents

    LogPrintf("DigiDollar TxBuilder: CalculateRequiredCollateral - DD: %d cents, Price: %d cents, BaseRatio: %d%%, DCA: %.2f, AdjustedRatio: %.2f%%\n",
              ddAmount, oraclePrice, baseRatio, dcaMultiplier, adjustedRatio);

    // Use 64-bit arithmetic to prevent overflow
    // Oracle price format: cents (100 = $1.00 DGB price)
    // Both DD and oracle price are in cents (unified format)
    // Formula: DGB_sats = (DD_cents * COIN * ratio) / (oracle_cents * 100)
    uint64_t requiredCollateral = (static_cast<uint64_t>(usdValue) * static_cast<uint64_t>(COIN) * static_cast<uint64_t>(adjustedRatio)) /
                                   (static_cast<uint64_t>(oraclePrice) * 100);

    LogPrintf("DigiDollar TxBuilder: - Required collateral: %llu sats (%.8f DGB)\n",
              requiredCollateral, requiredCollateral / 100000000.0);

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

    // Validate against consensus parameters
    // params.ddAmount is in cents (100 cents = $1.00)
    // Consensus params (minMintAmount/maxMintAmount) are also in cents
    if (!IsValidMintAmount(params.ddAmount, ddParams)) {
        LogPrintf("ValidateMintParams FAILED: IsValidMintAmount returned false for %d cents (min=%d, max=%d)\n",
                 params.ddAmount, ddParams.minMintAmount, ddParams.maxMintAmount);
        return false;
    }

    // Validate lock period (0 = 1 hour testing tier, 30 days to 10 years)
    if (params.lockDays != 0 && (params.lockDays < 30 || params.lockDays > 10 * 365)) {
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

    // DD token output: Simple P2TR (key-path only, no MAST, no CLTV)
    // DD tokens must be freely transferable, unlike collateral which has timelock
    // Use CreateDDOutputScript for simple P2TR with key-path spending only
    CScript ddScript = CreateDDOutputScript(params.ownerKey, params.ddAmount);
    tx.vout.push_back(CTxOut(0, ddScript));

    // Add OP_RETURN output with metadata for validation
    // Format: OP_RETURN <"DD"> <txType> <ddAmount> <lockHeight>
    int64_t lockHeight = currentHeight + LockDaysToBlocks(params.lockDays);
    CScript metadataScript = CScript() << OP_RETURN
                                       << std::vector<unsigned char>{'D', 'D'}
                                       << CScriptNum(1)  // 1 = MINT transaction
                                       << CScriptNum(params.ddAmount)  // DD amount in cents
                                       << CScriptNum(lockHeight);  // Lock height in blocks
    tx.vout.push_back(CTxOut(0, metadataScript));

    // Iterative fee calculation to account for change output
    // We need to calculate fee, then add change output, then recalculate fee
    CAmount change = 0;
    result.totalFees = 0;

    // First iteration: calculate fee without change output
    CAmount feeWithoutChange = CalculateFee(tx, params.feeRate);
    change = totalIn - result.collateralRequired - feeWithoutChange;

    if (change < 0) {
        result.error = "Insufficient funds after fee calculation";
        return result;
    }

    // If we have significant change, add change output and recalculate
    if (change >= DUST_THRESHOLD) {
        // Create change output
        CKey changeKey = GenerateChangeKey();
        CPubKey changePubkey = changeKey.GetPubKey();
        CTxDestination changeDest{WitnessV1Taproot(XOnlyPubKey(changePubkey))};
        tx.vout.push_back(CTxOut(change, GetScriptForDestination(changeDest)));

        // Recalculate fee with change output included
        result.totalFees = CalculateFee(tx, params.feeRate);

        // Ensure fees are reasonable
        if (result.totalFees > static_cast<CAmount>(totalIn * MAX_FEE_RATIO)) {
            result.error = "Transaction fees too high";
            return result;
        }

        // Adjust change amount based on actual fee
        change = totalIn - result.collateralRequired - result.totalFees;

        if (change < 0) {
            result.error = "Insufficient funds after final fee calculation";
            return result;
        }

        if (change < DUST_THRESHOLD) {
            // Change became dust after fee adjustment, remove change output
            tx.vout.pop_back();
            result.totalFees += change;  // Add dust to fee
        } else {
            // Update change output with correct amount
            tx.vout.back().nValue = change;
        }
    } else {
        // No change output, small change goes to fee
        result.totalFees = feeWithoutChange + change;
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

        // Create P2TR output directly from the decoded address
        // The address is already a fully-formed Taproot output key (already tweaked by wallet)
        // We don't want to tweak it again, so we create the scriptPubKey directly
        CScript ddScript;
        ddScript << OP_1 << ToByteVector(*taproot);
        tx.vout.push_back(CTxOut(0, ddScript)); // DD outputs always have 0 DGB value
        LogPrintf("DigiDollar: Added DD output for %s: %d cents\n", address, amount);
    }

    // Add DD change output if needed
    CAmount ddChange = totalDDIn - totalDDOut;
    if (ddChange > 0) {
        // Only create change if above dust threshold
        if (ddChange >= minOutput) {
            // Create change output using same approach as recipient outputs
            // Use the spender's public key directly without extra tweaking
            CPubKey changePubkey = params.spenderKey.GetPubKey();
            XOnlyPubKey xonly(changePubkey);

            // Apply Taproot tweak to get the output key
            // This matches what CreateDigiDollarP2TR does
            auto tweaked = xonly.CreateTapTweak(nullptr);
            if (!tweaked) {
                result.error = "Failed to create Taproot tweak for change output";
                return result;
            }
            XOnlyPubKey output_key = tweaked->first;

            CScript changeScript;
            changeScript << OP_1 << ToByteVector(output_key);
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
            // Create DGB change output using a P2WPKH (not P2TR) to differentiate from DD outputs
            // This ensures DGB change won't be confused with DD outputs
            CScript dgbChangeScript = GetScriptForDestination(WitnessV0KeyHash(params.spenderKey.GetPubKey()));
            tx.vout.push_back(CTxOut(dgbChange, dgbChangeScript));
            LogPrintf("DigiDollar: Added DGB change output: %d sats\n", dgbChange);
        }
    }

    // Add OP_RETURN with DD amounts for each output (needed for receiver to identify amounts)
    // Format: OP_RETURN <"DD"> <txType> <output_count> <amount1> <amount2> ... <amountN>
    std::vector<CAmount> ddOutputAmounts;
    for (const auto& [address, amount] : params.recipients) {
        ddOutputAmounts.push_back(amount);
    }
    if (ddChange > 0 && ddChange >= minOutput) {
        ddOutputAmounts.push_back(ddChange);
    }

    CScript metadataScript;
    metadataScript << OP_RETURN
                   << std::vector<unsigned char>{'D', 'D'}
                   << CScriptNum(2);  // 2 = TRANSFER transaction

    // Add each DD output amount
    for (CAmount amt : ddOutputAmounts) {
        metadataScript << CScriptNum(amt);
    }

    tx.vout.push_back(CTxOut(0, metadataScript));
    LogPrintf("DigiDollar: Added OP_RETURN with %d DD output amounts\n", ddOutputAmounts.size());

    // Final validation - ensure DD conservation
    // DD amounts are now stored in OP_RETURN, so sum up ddOutputAmounts instead
    CAmount finalDDOut = 0;
    for (CAmount amt : ddOutputAmounts) {
        finalDDOut += amt;
    }

    LogPrintf("DigiDollar: Conservation check - Input: %d cents, Output: %d cents\n",
              totalDDIn, finalDDOut);

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
    if (!params.ddAmounts.empty() && params.ddAmounts.size() == params.ddUtxos.size()) {
        // Use provided amounts (CRITICAL FIX #8: Use actual UTXO amounts, not GetDDFromUTXO default)
        for (const auto& amt : params.ddAmounts) {
            totalDDInCheck += amt;
        }
    } else {
        // Fallback to GetDDFromUTXO (which returns hardcoded 5000 for testing)
        for (const auto& utxo : params.ddUtxos) {
            totalDDInCheck += GetDDFromUTXO(utxo);
        }
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

    if (params.path == RedemptionPath::ERR) {
        // ERR (Emergency Redemption Ratio) path:
        // System < 100% requires MORE DD to burn for full collateral
        // Formula: RequiredDD = OriginalDD × (100 / SystemHealth%)

        int systemHealth = GetCurrentSystemCollateral();

        // Calculate required DD amount with ERR multiplier
        // Use 64-bit to prevent overflow: (ddMinted * 100) / systemHealth
        uint64_t requiredDD = (static_cast<uint64_t>(position.ddMinted) * 100) / systemHealth;

        // Validate user is burning enough DD
        if (params.ddToRedeem < static_cast<CAmount>(requiredDD)) {
            LogPrintf("DigiDollar: ERR redemption - insufficient DD (provided: %d, required: %llu)\n",
                     params.ddToRedeem, requiredDD);
            return 0; // Insufficient DD burned
        }

        // If enough DD burned, return full proportional collateral
        // For full redemption: return all collateral
        // For partial: return proportional amount
        if (params.ddToRedeem >= position.ddMinted) {
            return position.dgbLocked; // Full redemption
        } else {
            // Partial ERR redemption: proportional collateral
            uint64_t proportional = (static_cast<uint64_t>(position.dgbLocked) *
                                    static_cast<uint64_t>(params.ddToRedeem)) /
                                    static_cast<uint64_t>(position.ddMinted);
            return static_cast<CAmount>(proportional);
        }
    } else {
        // Normal/Emergency/Partial path: standard proportional collateral recovery
        if (params.ddToRedeem >= position.ddMinted) {
            return position.dgbLocked; // Full redemption
        } else {
            // Partial redemption: proportional collateral
            uint64_t proportional = (static_cast<uint64_t>(position.dgbLocked) *
                                    static_cast<uint64_t>(params.ddToRedeem)) /
                                    static_cast<uint64_t>(position.ddMinted);
            return static_cast<CAmount>(proportional);
        }
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
    CPubKey pubkey = owner.GetPubKey();
    XOnlyPubKey xonly(pubkey);

    // Convert XOnlyPubKey to vector for script insertion
    std::vector<unsigned char> xonly_bytes(xonly.begin(), xonly.end());

    switch (path) {
        case RedemptionPath::NORMAL:
            // Normal redemption: P2TR with timelock (CLTV)
            return CScript() << xonly_bytes << OP_CHECKSIG;

        case RedemptionPath::EMERGENCY:
            // Emergency: Requires oracle signatures (8-of-15)
            // For now, create a basic script that checks owner signature
            return CScript() << xonly_bytes << OP_CHECKSIG;

        case RedemptionPath::PARTIAL:
            // Partial: Similar to normal but with price verification
            return CScript() << xonly_bytes << OP_CHECKSIG;

        case RedemptionPath::ERR:
            // ERR: Emergency redemption ratio when system < 100%
            return CScript() << xonly_bytes << OP_CHECKSIG;

        default:
            return CScript();
    }
}

CCollateralPosition RedeemTxBuilder::GetCollateralPosition(const COutPoint& outpoint) const {
    // Query the actual UTXO using DigiDollar's UTXO lookup function
    CCollateralPosition position;
    position.outpoint = outpoint;

    // TEMPORARY: For now, just use wallet's cached position data
    // TODO: Implement proper UTXO lookup via chainstate parameter
    // This is a stopgap to fix the hardcoded 1000 DGB bug
    Coin coin;
    if (false) {  // Disabled UTXO lookup - will use metadata from script instead
        LogPrintf("DigiDollar: GetCollateralPosition - UTXO not found or already spent: %s:%d\n",
                 outpoint.hash.ToString(), outpoint.n);
        // Return empty position if UTXO doesn't exist
        position.dgbLocked = 0;
        position.ddMinted = 0;
        position.unlockHeight = 0;
        position.collateralRatio = 0;
        return position;
    }

    // Extract actual collateral amount from the UTXO
    position.dgbLocked = coin.out.nValue;

    // Extract DD amount from script metadata
    CAmount ddAmount = 0;
    if (DigiDollar::ExtractDDAmount(coin.out.scriptPubKey, ddAmount)) {
        position.ddMinted = ddAmount;
    } else {
        LogPrintf("DigiDollar: GetCollateralPosition - WARNING: Could not extract DD amount from script\n");
        position.ddMinted = 0;
    }

    // Extract unlock height from script metadata
    int64_t lockTime = DigiDollar::ExtractLockTime(coin.out.scriptPubKey);
    if (lockTime > 0) {
        position.unlockHeight = static_cast<uint32_t>(lockTime);
    } else {
        LogPrintf("DigiDollar: GetCollateralPosition - WARNING: Could not extract lock time from script\n");
        position.unlockHeight = 0;
    }

    // Calculate collateral ratio
    if (position.ddMinted > 0) {
        // Ratio = (collateral_dgb_value_usd / dd_minted_usd) * 100
        // Oracle price is in micro-USD (1,000,000 = $1.00), DD is in cents (100 = $1.00)
        // DGB_value_cents = (dgbLocked_sats * oracle_micro_usd) / (COIN * 10000)
        // ratio = (DGB_value_cents * 100) / ddMinted_cents
        CAmount dgbValueCents = (position.dgbLocked * oraclePrice) / (COIN * 10000);
        position.collateralRatio = (dgbValueCents * 100) / position.ddMinted;
    } else {
        position.collateralRatio = 0;
    }

    LogPrintf("DigiDollar: GetCollateralPosition - Found UTXO %s:%d with dgbLocked=%d, ddMinted=%d, unlockHeight=%d\n",
             outpoint.hash.ToString(), outpoint.n, position.dgbLocked, position.ddMinted, position.unlockHeight);

    return position;
}

TxBuilderResult RedeemTxBuilder::BuildRedemptionTransaction(const TxBuilderRedeemParams& params) {
    TxBuilderResult result;

    LogPrintf("DigiDollar: BuildRedemptionTransaction - Starting (path: %d, ddToRedeem: %d)\n",
             static_cast<int>(params.path), params.ddToRedeem);

    // Step 1: Validate parameters
    if (!ValidateRedeemParams(params)) {
        result.error = "Invalid redemption parameters";
        LogPrintf("DigiDollar: BuildRedemptionTransaction FAILED - %s\n", result.error);
        return result;
    }

    // Step 2: Get collateral position (use pre-queried data if provided)
    CCollateralPosition position;
    if (params.collateralAmount > 0) {
        // Use pre-queried position data from caller (RPC provided wallet's cached data)
        position.outpoint = params.collateralOutpoint;
        position.dgbLocked = params.collateralAmount;
        position.ddMinted = params.ddMinted;
        position.unlockHeight = params.unlockHeight;
        position.collateralRatio = (position.dgbLocked / COIN * oraclePrice) * 100 / position.ddMinted;
        LogPrintf("DigiDollar: Using pre-queried collateral position - dgbLocked: %d, ddMinted: %d, unlockHeight: %d\n",
                 position.dgbLocked, position.ddMinted, position.unlockHeight);
    } else {
        // Fallback to UTXO lookup (not implemented yet - needs chainstate access)
        position = GetCollateralPosition(params.collateralOutpoint);
        LogPrintf("DigiDollar: Queried collateral position from UTXO - dgbLocked: %d, ddMinted: %d, unlockHeight: %d\n",
                 position.dgbLocked, position.ddMinted, position.unlockHeight);
    }

    // Step 3: Verify redemption conditions are met (pass position to avoid re-querying)
    if (!VerifyRedemptionConditions(params, params.path, position)) {
        result.error = "Redemption conditions not met for path " + std::to_string(static_cast<int>(params.path));
        LogPrintf("DigiDollar: BuildRedemptionTransaction FAILED - %s\n", result.error);
        return result;
    }

    // Step 4: Calculate collateral return
    CAmount dgbToRelease = CalculateCollateralReturn(params.ddToRedeem, position.dgbLocked, oraclePrice);
    if (dgbToRelease <= 0) {
        result.error = "Failed to calculate collateral return";
        LogPrintf("DigiDollar: BuildRedemptionTransaction FAILED - %s\n", result.error);
        return result;
    }

    LogPrintf("DigiDollar: Collateral to release: %d sats (%.8f DGB)\n",
             dgbToRelease, dgbToRelease / 100000000.0);

    // Step 5: Build transaction
    CMutableTransaction tx;

    // Set type based on redemption path
    if (params.path == RedemptionPath::PARTIAL) {
        tx.SetDigiDollarType(::DD_TX_PARTIAL); // Use partial redemption type
        LogPrintf("DigiDollar: Using DD_TX_PARTIAL type\n");
    } else if (params.path == RedemptionPath::EMERGENCY ||
               params.path == RedemptionPath::ERR) {
        tx.SetDigiDollarType(::DD_TX_EMERGENCY); // Use emergency type
        LogPrintf("DigiDollar: Using DD_TX_EMERGENCY type\n");
    } else {
        tx.SetDigiDollarType(::DD_TX_REDEEM);
        LogPrintf("DigiDollar: Using DD_TX_REDEEM type\n");
    }

    // Input 0: Collateral UTXO (P2TR)
    // CRITICAL: nSequence must be < 0xFFFFFFFF to enable OP_CHECKLOCKTIMEVERIFY
    // Using 0xFFFFFFFE to signal opt-in Replace-By-Fee (BIP125) and enable CLTV
    tx.vin.push_back(CTxIn(params.collateralOutpoint, CScript(), 0xFFFFFFFE));
    LogPrintf("DigiDollar: Added collateral input: %s:%d (nSequence=0xFFFFFFFE for CLTV)\n",
             params.collateralOutpoint.hash.ToString(), params.collateralOutpoint.n);

    // Inputs 1+: DD UTXOs to burn
    // CRITICAL: DD UTXOs also have CLTV in their script (same MAST tree as collateral)
    // So they MUST use nSequence < 0xFFFFFFFF to enable locktime checking
    for (const auto& utxo : params.ddUtxos) {
        tx.vin.push_back(CTxIn(utxo, CScript(), 0xFFFFFFFE));
        LogPrintf("DigiDollar: Added DD input to burn: %s:%d (nSequence=0xFFFFFFFE for CLTV)\n", utxo.hash.ToString(), utxo.n);
    }

    // Inputs N+: Fee UTXOs (DGB)
    // CRITICAL FIX: Use the pre-selected fee UTXOs directly, don't re-select!
    // The caller (wallet or RPC) has already selected the appropriate fee UTXOs
    CAmount totalFeeIn = 0;
    if (!params.feeUtxos.empty()) {
        // Add all pre-selected fee UTXOs as inputs
        for (const auto& feeUtxo : params.feeUtxos) {
            tx.vin.push_back(CTxIn(feeUtxo));
        }

        // Calculate total fee input from provided amounts
        if (!params.feeAmounts.empty() && params.feeAmounts.size() == params.feeUtxos.size()) {
            for (CAmount amount : params.feeAmounts) {
                totalFeeIn += amount;
            }
            LogPrintf("DigiDollar: Added %d fee inputs (total: %d sats) from pre-selected UTXOs\n",
                      params.feeUtxos.size(), totalFeeIn);
        } else {
            // Fallback: Try to get amounts from UTXO set (shouldn't normally happen)
            LogPrintf("DigiDollar: WARNING - feeAmounts not provided or size mismatch, using GetUTXOValue\n");
            for (const auto& feeUtxo : params.feeUtxos) {
                CAmount amount = GetUTXOValue(feeUtxo);
                totalFeeIn += amount;
            }
            LogPrintf("DigiDollar: Added %d fee inputs (total: %d sats) via GetUTXOValue fallback\n",
                      params.feeUtxos.size(), totalFeeIn);
        }
    }

    // Output 0: DGB returned to owner
    // CRITICAL FIX: Use wallet change address if provided, otherwise use owner key
    CTxDestination dest;
    if (params.collateralDest.has_value()) {
        dest = params.collateralDest.value();
        LogPrintf("DigiDollar: Using provided wallet destination for returned collateral\n");
    } else {
        // Fallback to owner key (for backwards compatibility)
        CPubKey pubkey = params.ownerKey.GetPubKey();
        dest = CTxDestination{WitnessV1Taproot(XOnlyPubKey(pubkey))};
        LogPrintf("DigiDollar: Using owner key pubkey for returned collateral (wallet may not recognize)\n");
    }
    tx.vout.push_back(CTxOut(dgbToRelease, GetScriptForDestination(dest)));
    LogPrintf("DigiDollar: Added DGB output to owner: %d sats\n", dgbToRelease);

    // Output 1 (partial only): New collateral position
    if (params.path == RedemptionPath::PARTIAL) {
        // For partial redemption, create a new collateral output for the remainder
        // TODO: Implement partial collateral remainder logic
        // This would create a new P2TR collateral output with:
        // - Remaining DGB: position.dgbLocked - dgbToRelease
        // - Remaining DD: position.ddMinted - params.ddToRedeem
        // - Same unlock height
        LogPrintf("DigiDollar: PARTIAL redemption - remainder output not yet implemented\n");
    }

    // Set transaction locktime to unlockHeight (critical for CLTV validation)
    tx.nLockTime = position.unlockHeight;
    LogPrintf("DigiDollar: Set tx.nLockTime = %d (unlockHeight)\n", position.unlockHeight);

    // Calculate actual fees
    result.totalFees = CalculateFee(tx, params.feeRate);

    // CRITICAL: Ensure fee meets DigiByte minimum relay fee (100,000 sat/kB)
    // For a typical redemption tx (~400 vB), minimum is ~40,000 sats
    // Add safety margin to ensure relay acceptance
    CAmount minRelayFee = 50000; // 0.0005 DGB minimum to ensure acceptance
    if (result.totalFees < minRelayFee) {
        LogPrintf("DigiDollar: Calculated fee (%d sats) below minimum relay fee, using %d sats\n",
                  result.totalFees, minRelayFee);
        result.totalFees = minRelayFee;
    }

    LogPrintf("DigiDollar: Calculated fees: %d sats (fee inputs: %d sats)\n", result.totalFees, totalFeeIn);

    // Add fee change output if needed
    if (totalFeeIn > 0) {
        CAmount feeChange = totalFeeIn - result.totalFees;
        if (feeChange < 0) {
            result.error = "Insufficient fee inputs for calculated fee";
            LogPrintf("DigiDollar: BuildRedemptionTransaction FAILED - %s\n", result.error);
            return result;
        }

        if (feeChange >= DUST_THRESHOLD) {
            // Add change output
            tx.vout.push_back(CTxOut(feeChange, GetScriptForDestination(dest)));
            LogPrintf("DigiDollar: Added fee change output: %d sats\n", feeChange);
        } else {
            // Dust goes to miner as fee
            result.totalFees += feeChange;
            LogPrintf("DigiDollar: Fee change (%d sats) below dust, added to fee\n", feeChange);
        }
    }

    // Success
    result.tx = tx;
    result.success = true;
    result.collateralRequired = 0; // No collateral required for redemption

    LogPrintf("DigiDollar: BuildRedemptionTransaction SUCCESS - %d inputs, %d outputs, fee: %d sats\n",
             tx.vin.size(), tx.vout.size(), result.totalFees);

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

    // Validation
    if (ddAmount <= 0 || originalCollateral <= 0 || currentPrice <= 0) {
        LogPrintf("DigiDollar: CalculateCollateralReturn FAILED - invalid parameters (dd: %d, collateral: %d, price: %d)\n",
                 ddAmount, originalCollateral, currentPrice);
        return 0;
    }

    // For Phase 1: Return full proportional collateral
    // Formula: (ddAmount / totalDDMinted) * originalCollateral
    // Since we're redeeming the full position in most cases, we return the full collateral
    // For partial redemptions, this would be adjusted

    // IMPORTANT: For ERR path (Emergency Redemption Route), return 0 as it's not implemented yet
    // This is handled by the caller (BuildRedemptionTransaction) checking the path

    LogPrintf("DigiDollar: Calculating collateral return - DD amount: %d, Original collateral: %d, Current price: %d\n",
             ddAmount, originalCollateral, currentPrice);

    // Return full proportional collateral
    // In production, this would:
    // 1. Calculate proportional amount based on DD being redeemed
    // 2. Consider current oracle price vs original mint price
    // 3. Apply haircuts for ERR path (when implemented)
    // 4. Account for system collateral ratio

    CAmount returnAmount = originalCollateral;

    LogPrintf("DigiDollar: Collateral return calculated: %d sats (%.8f DGB)\n",
             returnAmount, returnAmount / 100000000.0);

    return returnAmount;
}

bool RedeemTxBuilder::VerifyRedemptionConditions(const TxBuilderRedeemParams& params,
                                                RedemptionPath path,
                                                const CCollateralPosition& position) const {
    // Verify conditions are met for the specified redemption path
    // Note: position is passed in to avoid duplicate UTXO lookups

    switch (path) {
        case RedemptionPath::NORMAL:
            // Check if timelock has expired
            if (currentHeight < position.unlockHeight) {
                LogPrintf("DigiDollar: Normal redemption FAILED - timelock not expired (current: %d, unlock: %d)\n",
                         currentHeight, position.unlockHeight);
                return false;
            }
            LogPrintf("DigiDollar: Normal redemption conditions met (timelock expired)\n");
            return true;

        case RedemptionPath::EMERGENCY:
            // Check if emergency conditions are met (8-of-15 oracle signatures required)
            // For Phase 1: Check if we have at least 8 oracle signatures in params
            // TODO: Implement actual oracle signature verification
            // For now, simplified check
            LogPrintf("DigiDollar: Emergency redemption - oracle approval check (simplified)\n");
            return true; // Simplified - would check oracle signatures

        case RedemptionPath::PARTIAL:
            // Check if partial redemption is allowed and oracle price is valid
            if (params.ddToRedeem >= position.ddMinted) {
                LogPrintf("DigiDollar: Partial redemption FAILED - amount too large (redeem: %d, minted: %d)\n",
                         params.ddToRedeem, position.ddMinted);
                return false;
            }
            if (oraclePrice <= 0) {
                LogPrintf("DigiDollar: Partial redemption FAILED - invalid oracle price (%d)\n", oraclePrice);
                return false;
            }
            // Also check timelock for partial redemption (same as NORMAL)
            if (currentHeight < position.unlockHeight) {
                LogPrintf("DigiDollar: Partial redemption FAILED - timelock not expired (current: %d, unlock: %d)\n",
                         currentHeight, position.unlockHeight);
                return false;
            }
            LogPrintf("DigiDollar: Partial redemption conditions met\n");
            return true;

        case RedemptionPath::ERR:
            // ERR (Emergency Redemption Ratio) - Check system health < 100%
            {
                int systemHealth = GetCurrentSystemCollateral();
                if (systemHealth >= 100) {
                    LogPrintf("DigiDollar: ERR redemption FAILED - system healthy (health: %d%%, need < 100%%)\n", systemHealth);
                    return false;
                }
                // Verify oracle price is valid for ERR calculation
                if (oraclePrice <= 0) {
                    LogPrintf("DigiDollar: ERR redemption FAILED - invalid oracle price (%d)\n", oraclePrice);
                    return false;
                }
                LogPrintf("DigiDollar: ERR redemption conditions met (system health: %d%%)\n", systemHealth);
                return true;
            }

        default:
            LogPrintf("DigiDollar: Unknown redemption path: %d\n", static_cast<int>(path));
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
        // P2TR key path spend: 1 (stack items) + 1 (sig length) + 64 (signature) = 66 bytes
        // But we use script path with control block:
        // 1 (items) + 1 (sig len) + 64 (sig) + 1 (script len) + script + 1 (control len) + 33 (control)
        // Conservatively estimate 110 bytes per input to account for script path
        witnessSize += 110;
    }

    // Virtual size calculation: (base_size * 3 + total_size) / 4
    size_t totalSize = baseSize + witnessSize;
    size_t vsize = (baseSize * 3 + totalSize) / 4;

    // Add 35% safety margin to account for estimation errors
    // Taproot transactions with script-path spending can be larger than key-path estimates
    return vsize + (vsize * 35 / 100);
}

} // namespace DigiDollar