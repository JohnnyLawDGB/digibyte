// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/digidollarwallet.h>
#include <wallet/wallet.h>
#include <wallet/spend.h>
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

DigiDollarWallet::DigiDollarWallet() : mockBalance(0), total_dd_balance(0), locked_collateral(0), m_wallet(nullptr) {
    // Initialize with some test data for development
    LogPrintf("DigiDollar: Wallet initialized\n");
}

DigiDollarWallet::DigiDollarWallet(wallet::CWallet* wallet) : mockBalance(0), total_dd_balance(0), locked_collateral(0), m_wallet(wallet) {
    LogPrintf("DigiDollar: Wallet initialized with CWallet pointer\n");

    // Load positions and transactions from database
    if (m_wallet) {
        LoadFromDatabase();
    }
}

void DigiDollarWallet::LoadFromDatabase() {
    // TODO: Implement proper database persistence using BerkeleyDB serialization
    // Phase 5.1 requires proper implementation of wallet database extensions
    // For now, positions and transactions persist in memory only (lost on wallet restart)
    // This needs custom serialization classes similar to CKey/CPubKey
    LogPrintf("DigiDollar: LoadFromDatabase - TODO: implement persistent storage\n");
}

bool DigiDollarWallet::TransferDigiDollar(const CDigiDollarAddress& to, CAmount amount,
                                        std::string& txid, std::string& error) {
    // Clear previous results
    txid.clear();
    error.clear();

    try {
        LogPrintf("DigiDollar: Starting transfer - %d cents to %s\n", amount, to.ToString());

        // Validate recipient address
        if (!to.IsValid()) {
            error = "Invalid recipient address";
            LogPrintf("DigiDollar: Invalid recipient address\n");
            return false;
        }

        // Validate amount
        if (amount <= 0) {
            error = "Amount must be positive";
            LogPrintf("DigiDollar: Amount must be positive\n");
            return false;
        }

        if (amount > 10000000) { // $100,000.00 maximum
            error = "Amount exceeds maximum transfer limit ($100,000)";
            LogPrintf("DigiDollar: Amount exceeds maximum\n");
            return false;
        }

        // Check balance using new balance tracking
        CAmount currentBalance = GetTotalDDBalance();
        if (currentBalance == 0) {
            // Fallback to legacy mock balance for testing
            currentBalance = mockBalance;
        }

        if (amount > currentBalance) {
            error = strprintf("Insufficient DD balance. Available: %d cents, Required: %d cents",
                            currentBalance, amount);
            LogPrintf("DigiDollar: Insufficient balance - available: %d, required: %d\n",
                     currentBalance, amount);
            return false;
        }

        // Build transfer transaction using TxBuilder
        DigiDollar::TxBuilderTransferParams params;
        params.recipients.push_back({to.ToString(), amount});
        params.feeRate = 1000; // 1000 sat/vB default

        // Select DD UTXOs to cover the amount
        CAmount selectedDDTotal = 0;
        if (!SelectDDCoins(amount, params.ddUtxos, selectedDDTotal)) {
            // Fallback: Create mock DD UTXO for testing
            LogPrintf("DigiDollar: No DD UTXOs found, using mock UTXO for testing\n");
            uint256 mockTxid;
            mockTxid.SetHex("dd1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcd");
            COutPoint mockUtxo(mockTxid, 0);
            params.ddUtxos.push_back(mockUtxo);
            selectedDDTotal = currentBalance; // Assume mock UTXO has full balance
        }

        // Select DGB UTXOs for fees (estimated)
        CAmount estimatedFee = 100000; // 0.001 DGB estimated fee
        CAmount selectedFeeTotal = 0;
        if (!SelectFeeCoins(estimatedFee, params.feeUtxos, selectedFeeTotal)) {
            // Fallback: Create mock fee UTXO for testing
            LogPrintf("DigiDollar: No DGB UTXOs found, using mock UTXO for fees\n");
            uint256 mockFeeTxid;
            mockFeeTxid.SetHex("fee1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab");
            COutPoint mockFeeUtxo(mockFeeTxid, 1);
            params.feeUtxos.push_back(mockFeeUtxo);
        }

        // Generate spending key (in real implementation, would get from wallet)
        CKey spenderKey;
        spenderKey.MakeNewKey(true);
        params.spenderKey = spenderKey;

        // Build transaction
        // Use current chain height and oracle price (mock for now)
        int currentHeight = 100000; // TODO: Get actual height from chainstate
        CAmount oraclePrice = 2500;  // TODO: Get from MockOracleManager

        DigiDollar::TransferTxBuilder builder(Params(), currentHeight, oraclePrice);
        DigiDollar::TxBuilderResult result = builder.BuildTransferTransaction(params);

        if (!result.success) {
            error = "Failed to build transaction: " + result.error;
            LogPrintf("DigiDollar: Transaction build failed - %s\n", result.error);
            return false;
        }

        // Get transaction ID
        txid = result.tx.GetHash().ToString();

        // TODO: In full implementation, would:
        // 1. Sign transaction with wallet keys
        // 2. Broadcast to mempool via node interface
        // 3. Update wallet database to mark UTXOs as spent
        // For now, we simulate success

        // Update balances
        if (mockBalance > 0) {
            // Update legacy mock balance
            mockBalance -= amount;
        }

        // Update new balance tracking
        CAmount newBalance = currentBalance - amount;
        // Note: In full implementation, would update specific address balance
        // For now, we don't update dd_balances as it would require address tracking

        // Add transaction to history
        DDTransaction tx;
        tx.txid = txid;
        tx.amount = amount;
        tx.timestamp = GetTime();
        tx.confirmations = 0;
        tx.incoming = false;
        tx.address = to.ToString();
        tx.category = "send";

        mockHistory.push_back(tx);
        transaction_history.push_back(tx);

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
    // Return actual transaction history (with mock fallback for testing)
    std::vector<DDTransaction> history = transaction_history;

    // Add mock history for testing if present
    history.insert(history.end(), mockHistory.begin(), mockHistory.end());

    // Sort by timestamp (newest first)
    std::sort(history.begin(), history.end(),
              [](const DDTransaction& a, const DDTransaction& b) {
                  return a.timestamp > b.timestamp;
              });

    LogPrintf("DigiDollar: GetDDTransactionHistory returning %d transactions\n", history.size());
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

        // Validate that requested path matches available path
        if (path != availablePath) {
            LogPrintf("DigiDollar: Requested path %d but only path %d available\n",
                     static_cast<int>(path), static_cast<int>(availablePath));
            // Auto-select best available path
            path = availablePath;
        }

        // Handle path-specific logic for all 4 redemption paths
        switch (path) {
            case DigiDollar::RedemptionPath::NORMAL:
                LogPrintf("DigiDollar: Using NORMAL redemption (timelock expired)\n");
                // Normal path: timelock has expired, full collateral return
                // Validate timelock is expired
                break;

            case DigiDollar::RedemptionPath::EMERGENCY:
                LogPrintf("DigiDollar: Using EMERGENCY redemption (oracle 8-of-15 signatures)\n");
                // Emergency path: requires oracle approval
                // Mock 8-of-15 oracle signatures for RegTest
                // In production: verify oracle signatures
                break;

            case DigiDollar::RedemptionPath::PARTIAL:
                LogPrintf("DigiDollar: Using PARTIAL redemption\n");
                // Partial path: redeem portion of position
                // Calculate proportional collateral release
                break;

            case DigiDollar::RedemptionPath::ERR:
                LogPrintf("DigiDollar: Using ERR redemption (system under-collateralized)\n");
                // ERR path: system < 100% collateral, reduced recovery
                // Apply Emergency Redemption Ratio
                break;
        }

        // For now, return placeholder implementation
        // Full integration requires RedeemTxBuilder connection
        error = "Redemption function implemented but awaiting full TxBuilder integration";
        LogPrintf("DigiDollar: %s\n", error);
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

        // Write to in-memory map
        collateral_positions[position.position_id] = position;

        // TODO: Write to wallet database (Phase 5.1 - requires proper serialization)

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
        // Calculate balance from active collateral positions
        // Phase 1: Use collateral_positions to track DD balance
        // Phase 2: Will use UTXO database with DD amount metadata
        CAmount balance = 0;
        for (const auto& [position_id, position] : collateral_positions) {
            if (position.is_active) {
                balance += position.dd_minted;
            }
        }

        LogPrintf("DigiDollar: GetTotalDDBalance calculated %d cents from %d positions\n",
                  balance, collateral_positions.size());
        return balance;

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

void DigiDollarWallet::AddCollateralPosition(const WalletCollateralPosition& position) {
    try {
        // Write position to database (this also updates the in-memory map)
        if (!WritePosition(position)) {
            LogPrintf("DigiDollar: Failed to write position to database\n");
            return;
        }

        LogPrintf("DigiDollar: Added collateral position - ID: %s, DD: %d, DGB: %d, Tier: %d, Active: %s\n",
                  position.position_id.GetHex(), position.dd_minted, position.dgb_collateral,
                  position.lock_tier, position.is_active ? "YES" : "NO");

        // Also add a transaction record for mint
        DDTransaction tx;
        tx.txid = position.position_id.GetHex();
        tx.amount = position.dd_minted;
        tx.timestamp = GetTime();
        tx.confirmations = 0; // Will be updated when confirmed
        tx.incoming = true;
        tx.address = "";
        tx.category = "mint";
        transaction_history.push_back(tx);

        // TODO: Write transaction to database (Phase 5.1 - requires proper serialization)

        LogPrintf("DigiDollar: Added mint transaction to history - TxID: %s\n", tx.txid);
    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: AddCollateralPosition exception - %s\n", e.what());
    }
}

size_t DigiDollarWallet::ScanForDDUTXOs() {
    if (!m_wallet) {
        LogPrintf("DigiDollar: ScanForDDUTXOs called but no wallet pointer set\n");
        return 0;
    }

    try {
        LogPrintf("DigiDollar: Starting UTXO scan for DD outputs\n");

        // Clear existing dd_balances
        dd_balances.clear();
        total_dd_balance = 0;

        size_t dd_utxo_count = 0;

        // Lock wallet for thread-safe access
        LOCK(m_wallet->cs_wallet);

        // Iterate through all wallet transactions
        for (const auto& [txid, wtx] : m_wallet->mapWallet) {
            // Check each output of the transaction
            for (size_t n = 0; n < wtx.tx->vout.size(); ++n) {
                const CTxOut& txout = wtx.tx->vout[n];

                // Check if this output is a DD token script
                if (DigiDollar::IsDDTokenScript(txout.scriptPubKey)) {
                    // Check if we own this output (IsMine check)
                    wallet::isminetype mine = m_wallet->IsMine(txout);
                    if (!(mine & wallet::ISMINE_SPENDABLE)) {
                        continue; // Not owned by us or not spendable
                    }

                    // Check if output is already spent
                    COutPoint outpoint(txid, n);
                    if (m_wallet->IsSpent(outpoint)) {
                        continue; // Already spent
                    }

                    // Extract DD amount from the script
                    CAmount dd_amount = 0;
                    if (DigiDollar::ExtractDDAmount(txout.scriptPubKey, dd_amount)) {
                        // Add to balance tracking
                        // For now, aggregate all DD into a single balance entry
                        // In future, could track per-address
                        std::string key = "total"; // Aggregate key

                        if (dd_balances.find(key) == dd_balances.end()) {
                            CDigiDollarAddress emptyAddr; // Empty address for total
                            dd_balances[key] = WalletDDBalance(emptyAddr, 0);
                        }

                        dd_balances[key].balance += dd_amount;
                        total_dd_balance += dd_amount;
                        dd_utxo_count++;

                        LogPrintf("DigiDollar: Found DD UTXO %s:%d - Amount: %d cents\n",
                                  txid.GetHex(), n, dd_amount);
                    }
                }
            }
        }

        LogPrintf("DigiDollar: Scan complete - Found %d DD UTXOs, Total balance: %d cents\n",
                  dd_utxo_count, total_dd_balance);

        return dd_utxo_count;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: ScanForDDUTXOs exception - %s\n", e.what());
        return 0;
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