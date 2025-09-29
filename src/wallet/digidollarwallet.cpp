// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/digidollarwallet.h>
#include <digidollar/txbuilder.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <util/strencodings.h>
#include <logging.h>
#include <util/time.h>
#include <kernel/chainparams.h>
#include <chainparams.h>
#include <crypto/common.h>

#include <algorithm>
#include <regex>

// CDigiDollarAddress is defined in base58.h - no need to redefine

// =============================================================================
// DDTransaction Implementation
// =============================================================================

DDTransaction::DDTransaction()
    : amount(0), timestamp(0), confirmations(0), incoming(false), category("unknown") {}

// =============================================================================
// DigiDollarWallet Implementation
// =============================================================================

DigiDollarWallet::DigiDollarWallet() : mockBalance(0), total_dd_balance(0), locked_collateral(0) {
    // Initialize with some test data for development
    LogPrintf("DigiDollar: Wallet initialized\n");
}

bool DigiDollarWallet::TransferDigiDollar(const CDigiDollarAddress& to, CAmount amount,
                                        std::string& txid, std::string& error) {
    // Clear previous results
    txid.clear();
    error.clear();

    try {
        // Check if DigiDollar is active
        // This is a simplified check - in full implementation would get current height
        // For now, assume we're past activation height in regtest mode

        // Validate recipient address
        if (!to.IsValid()) {
            error = "Invalid recipient address";
            return false;
        }

        // Validate amount
        if (amount <= 0) {
            error = "Amount must be positive";
            return false;
        }

        if (amount > 10000000) { // $100,000.00 maximum
            error = "Amount exceeds maximum transfer limit ($100,000)";
            return false;
        }

        // Check balance
        if (amount > mockBalance) {
            error = "Insufficient DD balance";
            return false;
        }

        // Build transfer transaction using TxBuilder
        DigiDollar::TxBuilderTransferParams params;
        params.recipients.push_back({to.ToString(), amount});
        params.feeRate = 1000; // 1000 sat/vB default

        // Mock DD UTXOs (in real implementation, would query wallet UTXO set)
        uint256 mockTxid;
        mockTxid.SetHex("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
        COutPoint mockUtxo(mockTxid, 0);
        params.ddUtxos.push_back(mockUtxo);

        // Mock fee UTXOs
        uint256 mockFeeTxid;
        mockFeeTxid.SetHex("fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321");
        COutPoint mockFeeUtxo(mockFeeTxid, 1);
        params.feeUtxos.push_back(mockFeeUtxo);

        // Generate mock spending key
        CKey spenderKey;
        spenderKey.MakeNewKey(true);
        params.spenderKey = spenderKey;

        // Build transaction
        DigiDollar::TransferTxBuilder builder(Params(), 100000, 2500); // Mock height and price
        DigiDollar::TxBuilderResult result = builder.BuildTransferTransaction(params);

        if (!result.success) {
            error = "Failed to build transaction: " + result.error;
            return false;
        }

        // In real implementation, would broadcast transaction
        txid = result.tx.GetHash().ToString();

        // Update mock balance and history
        mockBalance -= amount;

        DDTransaction tx;
        tx.txid = txid;
        tx.amount = amount;
        tx.timestamp = GetTime();
        tx.confirmations = 0;
        tx.incoming = false;
        tx.address = to.ToString();
        tx.category = "send";

        mockHistory.push_back(tx);

        LogPrintf("DigiDollar: Transfer successful - %d cents to %s (txid: %s)\n",
                  amount, to.ToString(), txid);

        return true;

    } catch (const std::exception& e) {
        error = "Transfer failed: " + std::string(e.what());
        LogPrintf("DigiDollar: Transfer exception - %s\n", error);
        return false;
    }
}

CAmount DigiDollarWallet::GetDDBalanceLegacy() const {
    // Legacy function - redirect to new implementation with empty address (total balance)
    return GetDDBalance(CDigiDollarAddress());
}

std::vector<DDTransaction> DigiDollarWallet::GetDDTransactionHistory() const {
    // In real implementation, would query wallet transaction database
    // and filter for DD transactions

    std::vector<DDTransaction> history = mockHistory;

    // Sort by timestamp (newest first)
    std::sort(history.begin(), history.end(),
              [](const DDTransaction& a, const DDTransaction& b) {
                  return a.timestamp > b.timestamp;
              });

    return history;
}

bool DigiDollarWallet::ValidateDDAddress(const std::string& address) const {
    return CDigiDollarAddress::IsValidDigiDollarAddress(address);
}

// =============================================================================
// Redemption Function Implementations (Task 3.9)
// =============================================================================

bool DigiDollarWallet::RedeemDigiDollar(const COutPoint& collateralUtxo,
                                       CAmount ddAmount,
                                       DigiDollar::RedemptionPath path,
                                       std::string& txid,
                                       std::string& error) {
    // Clear previous results
    txid.clear();
    error.clear();

    try {
        LogPrintf("DigiDollar: Starting redemption - %d cents via path %d\n", ddAmount, static_cast<int>(path));

        // Validate inputs
        if (collateralUtxo.IsNull()) {
            error = "Invalid collateral position";
            return false;
        }

        if (ddAmount <= 0) {
            error = "Invalid redemption amount";
            return false;
        }

        // Check if position can be redeemed
        DigiDollar::RedemptionPath availablePath;
        if (!CanRedeem(collateralUtxo, availablePath)) {
            error = "Position cannot be redeemed at this time";
            return false;
        }

        // For now, return placeholder implementation
        // In GREEN phase, would use actual RedeemTxBuilder
        error = "Redemption function not fully implemented (RED phase)";
        return false;

        // GREEN phase implementation would be:
        /*
        // Create redemption transaction using builder
        DigiDollar::RedeemTxBuilder builder(Params(), GetCurrentHeight(), GetOraclePrice());

        DigiDollar::TxBuilderRedeemParams params;
        params.collateralOutpoint = collateralUtxo;
        params.ddToRedeem = ddAmount;
        params.path = path;
        params.ownerKey = GetOwnerKey(); // Would get from wallet
        params.feeRate = GetCurrentFeeRate();
        params.ddUtxos = GetDDUTXOsForAmount(ddAmount);
        params.feeUtxos = GetFeeUTXOs();

        auto result = builder.BuildRedemptionTransaction(params);
        if (!result.success) {
            error = result.error;
            return false;
        }

        // Submit transaction to network
        if (!SubmitTransaction(result.tx)) {
            error = "Failed to submit redemption transaction";
            return false;
        }

        txid = result.tx.GetHash().ToString();

        // Update wallet state
        UpdateWalletAfterRedemption(collateralUtxo, ddAmount, result.tx);

        return true;
        */

    } catch (const std::exception& e) {
        error = "Redemption failed: " + std::string(e.what());
        LogPrintf("DigiDollar: Redemption exception - %s\n", error);
        return false;
    }
}

std::vector<DigiDollar::RedeemablePosition> DigiDollarWallet::GetRedeemablePositions() const {
    // Placeholder implementation for RED phase
    std::vector<DigiDollar::RedeemablePosition> positions;

    // In GREEN phase, would query actual wallet state:
    /*
    // Scan wallet for collateral UTXOs
    for (const auto& utxo : GetCollateralUTXOs()) {
        DigiDollar::RedeemablePosition pos;
        pos.collateralOutpoint = utxo.outpoint;
        pos.ddAmount = ExtractDDAmountFromPosition(utxo);
        pos.dgbLocked = utxo.nValue;
        pos.unlockHeight = ExtractUnlockHeight(utxo);
        pos.availablePaths = DetermineAvailablePaths(utxo);
        pos.canRedeemNow = !pos.availablePaths.empty();
        pos.estimatedReturn = CalculateRedemptionValue(utxo.outpoint);
        positions.push_back(pos);
    }
    */

    return positions;
}

CAmount DigiDollarWallet::CalculateRedemptionValue(const COutPoint& position) const {
    // Placeholder implementation for RED phase
    CAmount redemptionValue = 0;

    // In GREEN phase, would calculate based on:
    /*
    // Get position details
    auto positionData = GetCollateralPosition(position);

    // Get current oracle price
    CAmount currentPrice = GetOraclePrice();

    // Determine best available redemption path
    DigiDollar::RedemptionPath bestPath;
    if (CanRedeem(position, bestPath)) {
        // Calculate return based on path
        switch (bestPath) {
            case DigiDollar::RedemptionPath::NORMAL:
                redemptionValue = positionData.dgbLocked;
                break;
            case DigiDollar::RedemptionPath::ERR:
                redemptionValue = positionData.dgbLocked * 90 / 100; // 90% recovery
                break;
            case DigiDollar::RedemptionPath::EMERGENCY:
            case DigiDollar::RedemptionPath::PARTIAL:
                redemptionValue = positionData.dgbLocked;
                break;
        }

        // Subtract estimated fees
        CAmount fees = EstimateRedemptionFee(position, bestPath);
        redemptionValue = std::max(CAmount(0), redemptionValue - fees);
    }
    */

    return redemptionValue;
}

bool DigiDollarWallet::CanRedeem(const COutPoint& position, DigiDollar::RedemptionPath& availablePath) const {
    // Placeholder implementation for RED phase
    availablePath = DigiDollar::RedemptionPath::NORMAL;

    // In GREEN phase, would check:
    /*
    // Get position details
    auto positionData = GetCollateralPosition(position);
    if (positionData.outpoint.IsNull()) {
        return false; // Position doesn't exist
    }

    int currentHeight = GetCurrentHeight();
    int systemCollateral = GetSystemCollateral();

    // Check normal redemption (timelock expired)
    if (currentHeight >= positionData.unlockHeight) {
        availablePath = DigiDollar::RedemptionPath::NORMAL;
        return true;
    }

    // Check ERR redemption (system unhealthy)
    if (systemCollateral < 100) {
        availablePath = DigiDollar::RedemptionPath::ERR;
        return true;
    }

    // Check emergency redemption (oracle approval)
    if (HasEmergencyApproval()) {
        availablePath = DigiDollar::RedemptionPath::EMERGENCY;
        return true;
    }

    // Check partial redemption
    if (GetOraclePrice() > 0) {
        availablePath = DigiDollar::RedemptionPath::PARTIAL;
        return true;
    }
    */

    return false; // No redemption path available in RED phase
}

std::vector<DDTransaction> DigiDollarWallet::GetRedemptionHistory() const {
    // Placeholder implementation for RED phase
    std::vector<DDTransaction> redemptions;

    // In GREEN phase, would filter transaction history:
    /*
    for (const auto& tx : GetDDTransactionHistory()) {
        if (tx.category == "redeem") {
            redemptions.push_back(tx);
        }
    }
    */

    return redemptions;
}

CAmount DigiDollarWallet::EstimateRedemptionFee(const COutPoint& position, DigiDollar::RedemptionPath path) const {
    // Placeholder implementation for RED phase
    CAmount estimatedFee = 0;

    // In GREEN phase, would estimate based on:
    /*
    // Get current fee rate
    CAmount feeRate = GetCurrentFeeRate();

    // Estimate transaction size based on redemption path
    size_t estimatedSize = 250; // Base size

    switch (path) {
        case DigiDollar::RedemptionPath::NORMAL:
            estimatedSize += 50; // Basic script path
            break;
        case DigiDollar::RedemptionPath::EMERGENCY:
            estimatedSize += 150; // Oracle signatures
            break;
        case DigiDollar::RedemptionPath::PARTIAL:
            estimatedSize += 100; // Additional outputs
            break;
        case DigiDollar::RedemptionPath::ERR:
            estimatedSize += 75; // ERR validation
            break;
    }

    estimatedFee = (estimatedSize * feeRate) / 1000;
    */

    return estimatedFee;
}

CAmount DigiDollarWallet::GetDGBBalance() const {
    // Placeholder implementation for RED phase
    CAmount dgbBalance = 0;

    // In GREEN phase, would sum unspent DGB UTXOs:
    /*
    for (const auto& utxo : GetDGBUTXOs()) {
        dgbBalance += utxo.nValue;
    }
    */

    return dgbBalance;
}

// =============================================================================
// PHASE 5 TASK 5.1: DATABASE EXTENSION IMPLEMENTATIONS
// =============================================================================

bool DigiDollarWallet::WriteDDBalance(const CDigiDollarAddress& addr, const CAmount& balance) {
    try {
        std::string key = addr.ToString();
        if (key.empty()) {
            LogPrintf("DigiDollar: Invalid address in WriteDDBalance\n");
            return false;
        }

        WalletDDBalance walletBalance(addr, balance);
        walletBalance.last_updated = GetTime();
        dd_balances[key] = walletBalance;

        LogPrintf("DigiDollar: Wrote balance %d cents for address %s\n", balance, key);
        return true;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: WriteDDBalance exception - %s\n", e.what());
        return false;
    }
}

bool DigiDollarWallet::WritePosition(const WalletCollateralPosition& position) {
    try {
        if (position.position_id.IsNull()) {
            LogPrintf("DigiDollar: Invalid position ID in WritePosition\n");
            return false;
        }

        collateral_positions[position.position_id] = position;

        LogPrintf("DigiDollar: Wrote position %s - DD: %d, DGB: %d, tier: %d\n",
                  position.position_id.ToString(), position.dd_minted,
                  position.dgb_collateral, position.lock_tier);
        return true;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: WritePosition exception - %s\n", e.what());
        return false;
    }
}

bool DigiDollarWallet::UpdatePositionStatus(const uint256& position_id, bool active) {
    try {
        auto it = collateral_positions.find(position_id);
        if (it == collateral_positions.end()) {
            LogPrintf("DigiDollar: Position %s not found in UpdatePositionStatus\n", position_id.ToString());
            return false;
        }

        it->second.is_active = active;
        LogPrintf("DigiDollar: Updated position %s status to %s\n",
                  position_id.ToString(), active ? "active" : "inactive");
        return true;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: UpdatePositionStatus exception - %s\n", e.what());
        return false;
    }
}

// =============================================================================
// PHASE 5 TASK 5.2: BALANCE TRACKING IMPLEMENTATIONS
// =============================================================================

CAmount DigiDollarWallet::GetDDBalance(const CDigiDollarAddress& addr) const {
    try {
        std::string key = addr.ToString();
        if (key.empty()) {
            // Return total balance if no specific address
            return GetTotalDDBalance();
        }

        auto it = dd_balances.find(key);
        CAmount balance = (it != dd_balances.end()) ? it->second.balance : 0;

        LogPrintf("DigiDollar: GetDDBalance for %s returned %d cents\n", key, balance);
        return balance;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: GetDDBalance exception - %s\n", e.what());
        return 0;
    }
}

CAmount DigiDollarWallet::GetTotalDDBalance() const {
    try {
        CAmount total = 0;
        for (const auto& entry : dd_balances) {
            total += entry.second.balance;
        }

        LogPrintf("DigiDollar: GetTotalDDBalance returned %d cents\n", total);
        return total;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: GetTotalDDBalance exception - %s\n", e.what());
        return 0;
    }
}

CAmount DigiDollarWallet::GetLockedCollateral() const {
    try {
        CAmount locked = 0;
        for (const auto& entry : collateral_positions) {
            if (entry.second.is_active) {
                locked += entry.second.dgb_collateral;
            }
        }

        LogPrintf("DigiDollar: GetLockedCollateral returned %d satoshis\n", locked);
        return locked;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: GetLockedCollateral exception - %s\n", e.what());
        return 0;
    }
}

std::vector<WalletCollateralPosition> DigiDollarWallet::GetPositions(bool active_only) const {
    std::vector<WalletCollateralPosition> positions;

    try {
        for (const auto& entry : collateral_positions) {
            if (!active_only || entry.second.is_active) {
                positions.push_back(entry.second);
            }
        }

        LogPrintf("DigiDollar: GetPositions returned %d positions (active_only=%s)\n",
                  positions.size(), active_only ? "true" : "false");
        return positions;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: GetPositions exception - %s\n", e.what());
        return positions; // Return empty vector
    }
}

// =============================================================================
// PHASE 5 TASK 5.3: TRANSACTION CREATION IMPLEMENTATIONS
// =============================================================================

bool DigiDollarWallet::MintDigiDollar(const CAmount& dd_amount, uint32_t lock_tier, CTransactionRef& tx_out) {
    try {
        LogPrintf("DigiDollar: MintDigiDollar called - amount: %d, tier: %d\n", dd_amount, lock_tier);

        // RED phase implementation - validation only
        if (!ValidateMintParams(dd_amount, lock_tier)) {
            LogPrintf("DigiDollar: MintDigiDollar validation failed\n");
            return false;
        }

        // For RED phase, return false as transaction creation not implemented
        LogPrintf("DigiDollar: MintDigiDollar not fully implemented (RED phase)\n");
        return false;

        // GREEN phase implementation would be:
        /*
        // Create mint transaction using MintTxBuilder
        DigiDollar::MintTxBuilder builder(Params(), GetCurrentHeight(), GetOraclePrice());

        DigiDollar::TxBuilderMintParams params;
        params.ddAmount = dd_amount;
        params.lockDays = DigiDollarWallet::GetLockDaysForTier(lock_tier);
        params.ownerKey = GetWalletKey();
        params.feeRate = GetCurrentFeeRate();
        params.utxos = GetAvailableUTXOs();

        auto result = builder.BuildMintTransaction(params);
        if (!result.success) {
            LogPrintf("DigiDollar: Mint transaction build failed - %s\n", result.error);
            return false;
        }

        tx_out = MakeTransactionRef(result.tx);

        // Create and store position in wallet
        uint256 positionId = result.tx.GetHash();
        WalletCollateralPosition position(positionId, dd_amount, result.collateralRequired, lock_tier,
                                         GetCurrentHeight() + builder.LockDaysToBlocks(params.lockDays));
        WritePosition(position);

        return true;
        */

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: MintDigiDollar exception - %s\n", e.what());
        return false;
    }
}

bool DigiDollarWallet::TransferDigiDollar(const CDigiDollarAddress& to, CAmount amount, CTransactionRef& tx_out) {
    try {
        LogPrintf("DigiDollar: TransferDigiDollar called - to: %s, amount: %d\n", to.ToString(), amount);

        // RED phase implementation - validation only
        if (!ValidateTransferParams(to, amount)) {
            LogPrintf("DigiDollar: TransferDigiDollar validation failed\n");
            return false;
        }

        // For RED phase, return false as transaction creation not implemented
        LogPrintf("DigiDollar: TransferDigiDollar not fully implemented (RED phase)\n");
        return false;

        // GREEN phase implementation would be:
        /*
        // Create transfer transaction using TransferTxBuilder
        DigiDollar::TransferTxBuilder builder(Params(), GetCurrentHeight(), GetOraclePrice());

        DigiDollar::TxBuilderTransferParams params;
        params.recipients.push_back({to.ToString(), amount});
        params.feeRate = GetCurrentFeeRate();
        params.spenderKey = GetWalletKey();

        // Select DD UTXOs for spending
        if (!SelectDDCoins(amount, params.ddUtxos, selectedTotal)) {
            LogPrintf("DigiDollar: Insufficient DD balance for transfer\n");
            return false;
        }

        // Select DGB UTXOs for fees
        CAmount estimatedFee = CalculateTransactionFee(estimatedTx);
        if (!SelectFeeCoins(estimatedFee, params.feeUtxos, selectedFeeTotal)) {
            LogPrintf("DigiDollar: Insufficient DGB balance for fees\n");
            return false;
        }

        auto result = builder.BuildTransferTransaction(params);
        if (!result.success) {
            LogPrintf("DigiDollar: Transfer transaction build failed - %s\n", result.error);
            return false;
        }

        tx_out = MakeTransactionRef(result.tx);
        return true;
        */

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: TransferDigiDollar exception - %s\n", e.what());
        return false;
    }
}

bool DigiDollarWallet::RedeemDigiDollar(const uint256& position_id, const CAmount& amount, CTransactionRef& tx_out) {
    try {
        LogPrintf("DigiDollar: RedeemDigiDollar called - position: %s, amount: %d\n", position_id.ToString(), amount);

        // RED phase implementation - validation only
        if (!ValidateRedeemParams(position_id, amount)) {
            LogPrintf("DigiDollar: RedeemDigiDollar validation failed\n");
            return false;
        }

        // For RED phase, return false as transaction creation not implemented
        LogPrintf("DigiDollar: RedeemDigiDollar not fully implemented (RED phase)\n");
        return false;

        // GREEN phase implementation would be:
        /*
        // Get position details
        auto it = collateral_positions.find(position_id);
        if (it == collateral_positions.end() || !it->second.is_active) {
            LogPrintf("DigiDollar: Position not found or inactive\n");
            return false;
        }

        // Create redemption transaction using RedeemTxBuilder
        DigiDollar::RedeemTxBuilder builder(Params(), GetCurrentHeight(), GetOraclePrice());

        DigiDollar::TxBuilderRedeemParams params;
        params.collateralOutpoint = COutPoint(position_id, 0); // Assuming output 0
        params.ddToRedeem = amount;
        params.path = builder.DetermineRedemptionPath(params);
        params.ownerKey = GetWalletKey();
        params.feeRate = GetCurrentFeeRate();

        // Select DD UTXOs to burn
        if (!SelectDDCoins(amount, params.ddUtxos, selectedTotal)) {
            LogPrintf("DigiDollar: Insufficient DD balance for redemption\n");
            return false;
        }

        // Select DGB UTXOs for fees
        CAmount estimatedFee = CalculateTransactionFee(estimatedTx);
        if (!SelectFeeCoins(estimatedFee, params.feeUtxos, selectedFeeTotal)) {
            LogPrintf("DigiDollar: Insufficient DGB balance for fees\n");
            return false;
        }

        auto result = builder.BuildRedemptionTransaction(params);
        if (!result.success) {
            LogPrintf("DigiDollar: Redemption transaction build failed - %s\n", result.error);
            return false;
        }

        tx_out = MakeTransactionRef(result.tx);

        // Update position status
        UpdatePositionStatus(position_id, false);

        return true;
        */

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: RedeemDigiDollar exception - %s\n", e.what());
        return false;
    }
}

// =============================================================================
// PHASE 5 TEST HELPERS AND UTILITY FUNCTIONS
// =============================================================================

void DigiDollarWallet::SetMockDDBalance(const CDigiDollarAddress& addr, CAmount balance) {
    WriteDDBalance(addr, balance);
}

void DigiDollarWallet::AddMockPosition(const uint256& id, CAmount dd, CAmount dgb, uint32_t tier, int64_t height) {
    WalletCollateralPosition position(id, dd, dgb, tier, height);
    WritePosition(position);
}

void DigiDollarWallet::ClearWalletData() {
    dd_balances.clear();
    collateral_positions.clear();
    transaction_history.clear();
    total_dd_balance = 0;
    locked_collateral = 0;

    // Also clear legacy mock data
    ClearMockData();
}

// =============================================================================
// PHASE 5 VALIDATION AND HELPER FUNCTIONS
// =============================================================================

bool DigiDollarWallet::ValidateMintParams(const CAmount& dd_amount, uint32_t lock_tier) const {
    if (dd_amount <= 0) {
        LogPrintf("DigiDollar: Invalid mint amount: %d\n", dd_amount);
        return false;
    }

    if (lock_tier < 1 || lock_tier > 8) {
        LogPrintf("DigiDollar: Invalid lock tier: %d\n", lock_tier);
        return false;
    }

    if (dd_amount > MAX_DIGIDOLLAR) {
        LogPrintf("DigiDollar: Mint amount exceeds maximum: %d > %d\n", dd_amount, MAX_DIGIDOLLAR);
        return false;
    }

    return true;
}

bool DigiDollarWallet::ValidateTransferParams(const CDigiDollarAddress& to, const CAmount& amount) const {
    if (!to.IsValid()) {
        LogPrintf("DigiDollar: Invalid recipient address\n");
        return false;
    }

    if (amount <= 0) {
        LogPrintf("DigiDollar: Invalid transfer amount: %d\n", amount);
        return false;
    }

    if (amount > GetTotalDDBalance()) {
        LogPrintf("DigiDollar: Transfer amount exceeds balance: %d > %d\n", amount, GetTotalDDBalance());
        return false;
    }

    return true;
}

bool DigiDollarWallet::ValidateRedeemParams(const uint256& position_id, const CAmount& amount) const {
    if (position_id.IsNull()) {
        LogPrintf("DigiDollar: Invalid position ID\n");
        return false;
    }

    if (amount <= 0) {
        LogPrintf("DigiDollar: Invalid redemption amount: %d\n", amount);
        return false;
    }

    auto it = collateral_positions.find(position_id);
    if (it == collateral_positions.end()) {
        LogPrintf("DigiDollar: Position not found: %s\n", position_id.ToString());
        return false;
    }

    if (!it->second.is_active) {
        LogPrintf("DigiDollar: Position is not active: %s\n", position_id.ToString());
        return false;
    }

    if (amount > it->second.dd_minted) {
        LogPrintf("DigiDollar: Redemption amount exceeds position: %d > %d\n", amount, it->second.dd_minted);
        return false;
    }

    return true;
}

bool DigiDollarWallet::SelectDDCoins(const CAmount& target_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total) const {
    // RED phase implementation - placeholder
    LogPrintf("DigiDollar: SelectDDCoins not implemented (RED phase)\n");
    return false;

    // GREEN phase would implement actual coin selection algorithm
}

bool DigiDollarWallet::SelectFeeCoins(const CAmount& fee_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total) const {
    // RED phase implementation - placeholder
    LogPrintf("DigiDollar: SelectFeeCoins not implemented (RED phase)\n");
    return false;

    // GREEN phase would implement actual coin selection algorithm
}

CAmount DigiDollarWallet::CalculateTransactionFee(const CMutableTransaction& tx) const {
    // RED phase implementation - placeholder
    LogPrintf("DigiDollar: CalculateTransactionFee not implemented (RED phase)\n");
    return 1000; // Return placeholder fee

    // GREEN phase would calculate actual transaction fees
}

// =============================================================================
// UTILITY FUNCTION IMPLEMENTATIONS
// =============================================================================

namespace DigiDollarWalletUtils {

int GetLockDaysForTier(uint32_t tier) {
    // Lock tiers: 1=30d, 2=90d, 3=180d, 4=365d, 5=730d, 6=1095d, 7=1825d, 8=3650d
    switch (tier) {
        case 1: return 30;
        case 2: return 90;
        case 3: return 180;
        case 4: return 365;
        case 5: return 730;   // 2 years
        case 6: return 1095;  // 3 years
        case 7: return 1825;  // 5 years
        case 8: return 3650;  // 10 years
        default: return 30;   // Default to tier 1
    }
}

int GetMinCollateralRatio(uint32_t tier) {
    // Longer locks require less collateral
    switch (tier) {
        case 1: return 180;  // 180% for 30 days
        case 2: return 170;  // 170% for 90 days
        case 3: return 160;  // 160% for 180 days
        case 4: return 150;  // 150% for 1 year
        case 5: return 140;  // 140% for 2 years
        case 6: return 130;  // 130% for 3 years
        case 7: return 120;  // 120% for 5 years
        case 8: return 110;  // 110% for 10 years
        default: return 180; // Default to tier 1
    }
}

bool IsValidDDAddress(const std::string& address) {
    return CDigiDollarAddress::IsValidDigiDollarAddress(address);
}

} // namespace DigiDollarWalletUtils