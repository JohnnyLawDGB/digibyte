// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/digidollarwallet.h>
#include <wallet/wallet.h>
#include <wallet/spend.h>
#include <wallet/coincontrol.h>
#include <digidollar/txbuilder.h>
#include <digidollar/validation.h>
#include <digidollar/scripts.h>
#include <util/strencodings.h>
#include <logging.h>
#include <util/time.h>
#include <kernel/chainparams.h>
#include <chainparams.h>
#include <crypto/common.h>
#include <streams.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/interpreter.h>
#include <random.h>

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

    // Load existing DigiDollar data from database
    if (m_wallet) {
        size_t loaded = LoadFromDatabase();
        LogPrintf("DigiDollarWallet: Initialized with %d items from database\n", loaded);
    }
}

size_t DigiDollarWallet::LoadFromDatabase()
{
    if (!m_wallet) {
        LogPrint(BCLog::WALLETDB, "DigiDollarWallet::LoadFromDatabase - No wallet pointer\n");
        return 0;
    }

    LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loading data from database...\n");

    size_t positions_loaded = LoadPositionsFromDatabase();
    size_t balances_loaded = LoadBalancesFromDatabase();
    size_t txs_loaded = LoadTransactionsFromDatabase();

    // FIX #1: Load DD UTXOs from database
    size_t utxos_loaded = 0;
    wallet::WalletBatch batch(m_wallet->GetDatabase());
    dd_utxos.clear();

    std::unique_ptr<wallet::DatabaseCursor> cursor = batch.GetNewCursor();
    if (cursor) {
        wallet::DatabaseCursor::Status status = wallet::DatabaseCursor::Status::MORE;
        while (status == wallet::DatabaseCursor::Status::MORE) {
            DataStream key{};
            DataStream value{};
            status = cursor->Next(key, value);

            if (status != wallet::DatabaseCursor::Status::MORE) break;

            std::string key_type;
            key >> key_type;

            if (key_type == wallet::DBKeys::DD_OUTPUT) {
                COutPoint outpoint;
                key >> outpoint;

                CAmount dd_amount;
                value >> dd_amount;

                dd_utxos[outpoint] = dd_amount;
                utxos_loaded++;

                LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loaded DD UTXO %s:%d (%d cents)\n",
                        outpoint.hash.ToString(), outpoint.n, dd_amount);
            }
        }
    }

    size_t total = positions_loaded + balances_loaded + txs_loaded + utxos_loaded;

    LogPrintf("DigiDollarWallet: Loaded %d positions, %d balances, %d transactions, %d DD UTXOs\n",
              positions_loaded, balances_loaded, txs_loaded, utxos_loaded);

    // Recalculate totals
    RecalculateTotals();

    return total;
}

size_t DigiDollarWallet::LoadPositionsFromDatabase()
{
    wallet::WalletBatch batch(m_wallet->GetDatabase());
    size_t count = 0;

    // Clear in-memory positions
    collateral_positions.clear();

    // Iterate through database using cursor
    std::unique_ptr<wallet::DatabaseCursor> cursor = batch.GetNewCursor();
    if (!cursor) {
        LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Failed to get database cursor\n");
        return 0;
    }

    wallet::DatabaseCursor::Status status = wallet::DatabaseCursor::Status::MORE;
    while (status == wallet::DatabaseCursor::Status::MORE) {
        DataStream key{};
        DataStream value{};
        status = cursor->Next(key, value);

        if (status != wallet::DatabaseCursor::Status::MORE) break;

        // Check if this is a position entry
        std::string key_type;
        key >> key_type;

        if (key_type == wallet::DBKeys::DD_POSITION) {
            uint256 dd_timelock_id;
            key >> dd_timelock_id;

            WalletCollateralPosition position;
            value >> position;

            collateral_positions[dd_timelock_id] = position;
            count++;

            LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loaded position %s\n",
                     dd_timelock_id.ToString());
        }
    }

    return count;
}

size_t DigiDollarWallet::LoadBalancesFromDatabase()
{
    wallet::WalletBatch batch(m_wallet->GetDatabase());
    size_t count = 0;

    dd_balances.clear();

    std::unique_ptr<wallet::DatabaseCursor> cursor = batch.GetNewCursor();
    if (!cursor) return 0;

    wallet::DatabaseCursor::Status status = wallet::DatabaseCursor::Status::MORE;
    while (status == wallet::DatabaseCursor::Status::MORE) {
        DataStream key{};
        DataStream value{};
        status = cursor->Next(key, value);

        if (status != wallet::DatabaseCursor::Status::MORE) break;

        std::string key_type;
        key >> key_type;

        if (key_type == wallet::DBKeys::DD_BALANCE) {
            std::string address;
            key >> address;

            WalletDDBalance balance;
            value >> balance;

            dd_balances[address] = balance;
            count++;

            LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loaded balance for %s\n", address);
        }
    }

    return count;
}

size_t DigiDollarWallet::LoadTransactionsFromDatabase()
{
    wallet::WalletBatch batch(m_wallet->GetDatabase());
    size_t count = 0;

    transaction_history.clear();

    std::unique_ptr<wallet::DatabaseCursor> cursor = batch.GetNewCursor();
    if (!cursor) return 0;

    wallet::DatabaseCursor::Status status = wallet::DatabaseCursor::Status::MORE;
    while (status == wallet::DatabaseCursor::Status::MORE) {
        DataStream key{};
        DataStream value{};
        status = cursor->Next(key, value);

        if (status != wallet::DatabaseCursor::Status::MORE) break;

        std::string key_type;
        key >> key_type;

        if (key_type == wallet::DBKeys::DD_TRANSACTION) {
            uint256 txid;
            key >> txid;

            DDTransaction ddtx;
            value >> ddtx;

            transaction_history.push_back(ddtx);
            count++;

            LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loaded transaction %s\n", ddtx.txid);
        }
    }

    return count;
}

void DigiDollarWallet::RecalculateTotals()
{
    // Recalculate total DD balance
    total_dd_balance = 0;
    for (const auto& [addr, bal] : dd_balances) {
        total_dd_balance += bal.balance;
    }

    // Recalculate locked collateral
    locked_collateral = 0;
    for (const auto& [id, pos] : collateral_positions) {
        if (pos.is_active) {
            locked_collateral += pos.dgb_collateral;
        }
    }

    LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Totals - DD Balance: %d, Locked: %d\n",
             total_dd_balance, locked_collateral);
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
        params.feeRate = 100000; // 100,000 sat/kB (DigiByte minimum relay fee)

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
        std::vector<CAmount> fee_amounts;
        CAmount selectedFeeTotal = 0;
        if (!SelectFeeCoins(estimatedFee, params.feeUtxos, selectedFeeTotal, &fee_amounts)) {
            // Fallback: Create mock fee UTXO for testing
            LogPrintf("DigiDollar: No DGB UTXOs found, using mock UTXO for fees\n");
            uint256 mockFeeTxid;
            mockFeeTxid.SetHex("fee1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab");
            COutPoint mockFeeUtxo(mockFeeTxid, 1);
            params.feeUtxos.push_back(mockFeeUtxo);
            fee_amounts.push_back(estimatedFee * 2);
        }
        params.feeAmounts = fee_amounts;  // Pass actual fee UTXO amounts

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

        // FIX #3: Broadcast transaction to network (GREEN PHASE)
        // Create transaction reference for broadcasting
        CTransactionRef tx_ref = MakeTransactionRef(result.tx);

        // Get transaction ID
        txid = tx_ref->GetHash().ToString();

        // Submit to mempool and broadcast to network
        if (m_wallet) {
            // Access chainstate through wallet's chain interface
            std::string broadcast_error;
            const CAmount max_tx_fee = wallet::DEFAULT_TRANSACTION_MAXFEE;  // Use default max fee

            // Use chain().broadcastTransaction() which handles both mempool acceptance and network broadcast
            bool broadcast_success = m_wallet->chain().broadcastTransaction(
                tx_ref,
                max_tx_fee,
                /*relay=*/true,  // Relay to network
                broadcast_error
            );

            if (!broadcast_success) {
                error = strprintf("Transaction rejected by network: %s", broadcast_error);
                LogPrintf("DigiDollar: Broadcast failed - %s\n", broadcast_error);
                return false;
            }

            LogPrintf("DigiDollar: Transaction broadcast successful - txid: %s\n", txid);
        } else {
            LogPrintf("DigiDollar: WARNING - No wallet context, transaction built but not broadcast\n");
            error = "No wallet context for broadcasting";
            return false;
        }

        // FIX #2: CRITICAL - Time-locks (collateral positions) NEVER change during transfers!
        // Only DD UTXOs move. The locked DGB stays in place until redemption.
        // DO NOT mark positions inactive. DO NOT create new collateral positions.
        LogPrintf("DigiDollar: Updating DD UTXO set after transfer (FIX #2)\n");

        // Remove spent DD UTXOs from tracking
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        for (const auto& spent_utxo : params.ddUtxos) {
            dd_utxos.erase(spent_utxo);

            // Also erase from database
            if (!batch.EraseDDUTXO(spent_utxo)) {
                LogPrintf("DigiDollar: WARNING - Failed to erase DD UTXO %s:%d from database\n",
                         spent_utxo.hash.ToString(), spent_utxo.n);
            }

            LogPrintf("DigiDollar: Marked DD UTXO %s:%d as spent\n",
                      spent_utxo.hash.ToString(), spent_utxo.n);
        }

        // Add new DD UTXOs from transaction outputs (for change and potentially recipient if to ourselves)
        for (size_t i = 0; i < result.tx.vout.size(); i++) {
            CAmount dd_amount = 0;

            // Check if this output contains DD
            if (DigiDollar::ExtractDDAmount(result.tx.vout[i].scriptPubKey, dd_amount)) {
                // Check if this output is to our address (change or self-transfer)
                if (m_wallet->IsMine(result.tx.vout[i])) {
                    COutPoint new_utxo(result.tx.GetHash(), i);
                    dd_utxos[new_utxo] = dd_amount;

                    // Persist to database
                    if (!batch.WriteDDUTXO(new_utxo, dd_amount)) {
                        LogPrintf("DigiDollar: WARNING - Failed to write DD UTXO %s:%d to database\n",
                                 new_utxo.hash.ToString(), i);
                    }

                    LogPrintf("DigiDollar: Added change DD UTXO %s:%d (%d cents)\n",
                              new_utxo.hash.ToString(), i, dd_amount);
                }
            }
        }

        // Time-lock positions remain ACTIVE and UNCHANGED
        LogPrintf("DigiDollar: Transfer complete - time-locks preserved (still ACTIVE)\n");

        // Calculate DD change for legacy mock balance update
        CAmount dd_change = selectedDDTotal - amount;

        // Update legacy mock balance for backwards compatibility
        if (mockBalance > 0) {
            mockBalance -= amount;
            if (dd_change > 0) {
                mockBalance += dd_change;
            }
        }

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

        // Verify balance updated correctly
        CAmount newBalance = GetTotalDDBalance();
        LogPrintf("DigiDollar: Transfer successful - %d cents to %s (txid: %s)\n",
                  amount, to.ToString(), txid);
        LogPrintf("DigiDollar: Balance updated: %d -> %d (change: %d)\n",
                  currentBalance, newBalance, dd_change);

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
    if (!m_wallet) {
        LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - No wallet pointer set\n");
        return error("DigiDollarWallet::WriteDDBalance: No wallet pointer set");
    }

    if (balance < 0) {
        LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - Negative balance: %d\n", balance);
        return error("DigiDollarWallet::WriteDDBalance: Negative balance not allowed");
    }

    std::string addr_str = addr.ToString();
    if (addr_str.empty()) {
        // For testing with mock addresses, serialize the address object as hex
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << addr;
        addr_str = "test_addr_" + HexStr(ss);

        // Don't allow completely zero addresses
        if (addr_str == "test_addr_00") {
            LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - Completely empty address\n");
            return error("DigiDollarWallet::WriteDDBalance: Empty address not allowed");
        }

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet::WriteDDBalance - Using test key for invalid address: %s\n", addr_str);
    }

    try {
        // Create balance record
        WalletDDBalance bal_record(addr, balance);
        bal_record.last_updated = GetTime();

        // Get wallet database batch
        wallet::WalletBatch batch(m_wallet->GetDatabase());

        // Write to database
        LogPrintf("DEBUG: DigiDollarWallet::WriteDDBalance - Writing to database: addr=%s, balance=%d\n", addr_str, balance);
        if (!batch.WriteDDBalance(addr_str, bal_record)) {
            LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - Database write failed for %s\n", addr_str);
            return error("DigiDollarWallet::WriteDDBalance: Database write failed for %s", addr_str.c_str());
        }

        // Update in-memory cache
        dd_balances[addr_str] = bal_record;

        // Recalculate and persist total balance
        CAmount total = 0;
        for (const auto& [address, bal] : dd_balances) {
            total += bal.balance;
        }
        total_dd_balance = total;
        batch.WriteDDMetadata("total_dd_balance", std::to_string(total));

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Wrote balance %d for %s (total: %d)\n",
                 balance, addr_str, total);
        return true;

    } catch (const std::exception& e) {
        LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - Exception: %s\n", e.what());
        return error("DigiDollarWallet::WriteDDBalance: Exception - %s", e.what());
    }
}

bool DigiDollarWallet::WriteDDTimeLock(const WalletCollateralPosition& position) {
    try {
        if (!m_wallet) {
            return error("DigiDollarWallet::WriteDDTimeLock: No wallet pointer");
        }

        if (position.dd_timelock_id.IsNull()) {
            return error("DigiDollarWallet::WriteDDTimeLock: Invalid position ID");
        }

        // Write to database
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTimeLock(position)) {
            return error("DigiDollarWallet::WriteDDTimeLock: Database write failed for %s",
                         position.dd_timelock_id.ToString());
        }

        // Update in-memory cache
        collateral_positions[position.dd_timelock_id] = position;

        // Recalculate locked collateral if active
        if (position.is_active) {
            CAmount total_locked = 0;
            for (const auto& [id, pos] : collateral_positions) {
                if (pos.is_active) {
                    total_locked += pos.dgb_collateral;
                }
            }
            locked_collateral = total_locked;
            batch.WriteDDMetadata("locked_collateral", std::to_string(total_locked));
        }

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Wrote DDTimeLock %s (DD: %d, DGB: %d, tier: %d)\n",
                 position.dd_timelock_id.ToString(), position.dd_minted, position.dgb_collateral, position.lock_tier);
        return true;

    } catch (const std::exception& e) {
        return error("DigiDollarWallet::WriteDDTimeLock: Exception - %s", e.what());
    }
}

bool DigiDollarWallet::UpdatePositionStatus(const uint256& dd_timelock_id, bool active) {
    if (dd_timelock_id.IsNull()) {
        return error("DigiDollarWallet::UpdatePositionStatus: Invalid position ID");
    }

    // Check if position exists in memory
    auto it = collateral_positions.find(dd_timelock_id);
    if (it == collateral_positions.end()) {
        return error("DigiDollarWallet::UpdatePositionStatus: Position %s not found",
                     dd_timelock_id.ToString());
    }

    // Update status in memory
    it->second.is_active = active;

    // Write updated position to database (if wallet pointer is set)
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTimeLock(it->second)) {
            return error("DigiDollarWallet::UpdatePositionStatus: Database write failed");
        }

        // Recalculate locked collateral
        CAmount total_locked = 0;
        for (const auto& [id, pos] : collateral_positions) {
            if (pos.is_active) {
                total_locked += pos.dgb_collateral;
            }
        }
        locked_collateral = total_locked;
        batch.WriteDDMetadata("locked_collateral", std::to_string(total_locked));

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Updated position %s status to %s (locked: %d)\n",
                 dd_timelock_id.ToString(), active ? "active" : "inactive", total_locked);
    } else {
        LogPrintf("DigiDollarWallet: Updated mock position %s status to %s - NO DB\n",
                  dd_timelock_id.ToString(), active ? "active" : "inactive");
    }

    return true;
}

// =============================================================================
// PHASE 5 TASK 5.2: BALANCE TRACKING IMPLEMENTATIONS
// =============================================================================

CAmount DigiDollarWallet::GetDDBalance(const CDigiDollarAddress& addr) const {
    try {
        std::string key = addr.ToString();
        if (key.empty()) {
            // For testing with mock addresses, serialize the address object as hex
            CDataStream ss(SER_DISK, CLIENT_VERSION);
            ss << addr;
            std::string test_key = "test_addr_" + HexStr(ss);

            // Check if we have this test address
            auto it = dd_balances.find(test_key);
            if (it != dd_balances.end()) {
                return it->second.balance;
            }

            // Return total balance if truly empty address (all zeros)
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
        // FIX #1: Calculate balance from dd_utxos map (not collateral_positions)
        CAmount balance = 0;
        for (const auto& [outpoint, dd_amount] : dd_utxos) {
            // Only count unspent UTXOs
            if (!m_wallet || !m_wallet->IsSpent(outpoint)) {
                balance += dd_amount;
            }
        }

        LogPrintf("DigiDollar: GetTotalDDBalance calculated %d cents from %d UTXOs\n",
                  balance, dd_utxos.size());
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

std::vector<WalletCollateralPosition> DigiDollarWallet::GetDDTimeLocks(bool active_only) const {
    std::vector<WalletCollateralPosition> positions;

    try {
        for (const auto& entry : collateral_positions) {
            if (!active_only || entry.second.is_active) {
                positions.push_back(entry.second);
            }
        }

        LogPrintf("DigiDollar: GetDDTimeLocks returned %d time-locked positions (active_only=%s)\n",
                  positions.size(), active_only ? "true" : "false");
        return positions;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: GetDDTimeLocks exception - %s\n", e.what());
        return positions; // Return empty vector
    }
}

std::vector<DDUtxo> DigiDollarWallet::GetDDUTXOs() const {
    std::vector<DDUtxo> utxos;

    LogPrintf("DigiDollar: GetDDUTXOs - Scanning dd_utxos map (FIX #1)\n");

    // FIX #1: Use actual tracked UTXOs instead of assuming positions
    for (const auto& [outpoint, dd_amount] : dd_utxos) {
        // Verify UTXO is still unspent in wallet
        if (m_wallet && m_wallet->IsSpent(outpoint)) {
            LogPrintf("DigiDollar: Skipping spent UTXO %s:%d\n",
                      outpoint.hash.ToString(), outpoint.n);
            continue; // Skip spent
        }

        DDUtxo utxo(outpoint, dd_amount);
        utxos.push_back(utxo);

        LogPrintf("DigiDollar: Found DD UTXO %s:%d (%d cents)\n",
                  outpoint.hash.ToString(), outpoint.n, dd_amount);
    }

    LogPrintf("DigiDollar: GetDDUTXOs - Found %d spendable UTXOs\n", utxos.size());
    return utxos;
}

CAmount DigiDollarWallet::GetDDFromUTXO(const COutPoint& outpoint) const {
    // FIX #1: Look up UTXO in dd_utxos map (not collateral_positions)
    auto it = dd_utxos.find(outpoint);
    if (it == dd_utxos.end()) {
        LogPrintf("DigiDollar: GetDDFromUTXO - UTXO %s:%d not found in dd_utxos map\n",
                  outpoint.hash.ToString(), outpoint.n);
        return 0;
    }

    CAmount dd_amount = it->second;

    // Verify UTXO is still unspent
    if (m_wallet && m_wallet->IsSpent(outpoint)) {
        LogPrintf("DigiDollar: GetDDFromUTXO - UTXO %s:%d is spent\n",
                  outpoint.hash.ToString(), outpoint.n);
        return 0;
    }

    LogPrintf("DigiDollar: GetDDFromUTXO - Found %d cents for UTXO %s:%d\n",
              dd_amount, outpoint.hash.ToString(), outpoint.n);

    return dd_amount;
}

void DigiDollarWallet::AddCollateralPosition(const WalletCollateralPosition& position) {
    try {
        // Write position to database (this also updates the in-memory map)
        if (!WriteDDTimeLock(position)) {
            LogPrintf("DigiDollar: Failed to write position to database\n");
            return;
        }

        LogPrintf("DigiDollar: Added collateral position - ID: %s, DD: %d, DGB: %d, Tier: %d, Active: %s\n",
                  position.dd_timelock_id.GetHex(), position.dd_minted, position.dgb_collateral,
                  position.lock_tier, position.is_active ? "YES" : "NO");

        // FIX #1: Add DD UTXO to tracking map
        // DD output from mint is always at vout 1
        COutPoint dd_outpoint(position.dd_timelock_id, 1);
        dd_utxos[dd_outpoint] = position.dd_minted;
        LogPrintf("DigiDollar: Added DD UTXO to tracking - %s:%d (%d cents)\n",
                  dd_outpoint.hash.ToString(), dd_outpoint.n, position.dd_minted);

        // Persist DD UTXO to database
        if (m_wallet) {
            wallet::WalletBatch batch(m_wallet->GetDatabase());
            if (!batch.WriteDDUTXO(dd_outpoint, position.dd_minted)) {
                LogPrintf("DigiDollar: WARNING - Failed to persist DD UTXO to database\n");
            }
        }

        // Also add a transaction record for mint
        DDTransaction tx;
        tx.txid = position.dd_timelock_id.GetHex();
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

        // PERSIST POSITION TO DATABASE
        if (!WriteDDTimeLock(position)) {
            LogPrintf("DigiDollarWallet::MintDigiDollar - Failed to write position to database\n");
            // Don't fail the mint, but log the error
        }

        // Create DD transaction record for history
        DDTransaction ddtx;
        ddtx.txid = positionId.ToString();
        ddtx.amount = dd_amount;
        ddtx.timestamp = GetTime();
        ddtx.confirmations = 0;
        ddtx.incoming = false;
        ddtx.address = "";  // Mint has no counterparty
        ddtx.category = "mint";

        // PERSIST TRANSACTION TO DATABASE
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTransaction(ddtx)) {
            LogPrintf("DigiDollarWallet::MintDigiDollar - Failed to write transaction to database\n");
        }

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

        // Validate transfer parameters
        if (!ValidateTransferParams(to, amount)) {
            LogPrintf("DigiDollar: TransferDigiDollar validation failed\n");
            return false;
        }

        // Phase 2.1: Select DD UTXOs to cover the amount
        std::vector<COutPoint> dd_utxos;
        CAmount selectedDDTotal = 0;
        if (!SelectDDCoins(amount, dd_utxos, selectedDDTotal)) {
            LogPrintf("DigiDollar: Insufficient DD balance for transfer (need %d, have %d)\n",
                      amount, GetTotalDDBalance());
            return false;
        }

        // Phase 2.1: Create mock transaction structure for fee estimation
        CMutableTransaction estimateTx;
        estimateTx.vin.resize(dd_utxos.size() + 1);  // DD inputs + 1 fee input
        estimateTx.vout.resize(2);  // Recipient + change outputs

        // Phase 2.1: Calculate estimated fee
        CAmount estimatedFee = CalculateTransactionFee(estimateTx);

        // Phase 2.1: Select DGB UTXOs for fees
        std::vector<COutPoint> fee_utxos;
        std::vector<CAmount> fee_amounts;
        CAmount selectedFeeTotal = 0;
        if (!SelectFeeCoins(estimatedFee, fee_utxos, selectedFeeTotal, &fee_amounts)) {
            // Fallback: Create mock fee UTXO for testing (Phase 2.1 temporary)
            LogPrintf("DigiDollar: No DGB UTXOs found, using mock UTXO for fees\n");
            uint256 mockFeeTxid;
            mockFeeTxid.SetHex("fee1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab");
            COutPoint mockFeeUtxo(mockFeeTxid, 1);
            fee_utxos.push_back(mockFeeUtxo);
            fee_amounts.push_back(estimatedFee * 2);
            selectedFeeTotal = estimatedFee * 2;  // Ensure sufficient
        }

        // Phase 2.1: Build transfer transaction using TxBuilder
        DigiDollar::TxBuilderTransferParams params;
        params.recipients.push_back({to.ToString(), amount});
        params.ddUtxos = dd_utxos;

        // Populate DD amounts for each UTXO
        for (const auto& utxo : dd_utxos) {
            CAmount dd_amount = GetDDFromUTXO(utxo);
            params.ddAmounts.push_back(dd_amount);
        }

        params.feeUtxos = fee_utxos;
        params.feeAmounts = fee_amounts;  // Pass actual fee UTXO amounts
        params.feeRate = 100000;  // 100,000 sat/kB (DigiByte minimum relay fee)

        // Get the spending key from wallet
        // For DD transfers, we need the key that owns the first DD UTXO
        if (dd_utxos.empty()) {
            LogPrintf("DigiDollar: No DD UTXOs available for transfer\n");
            return false;
        }

        // Look up the position for the first DD UTXO
        auto it = collateral_positions.find(dd_utxos[0].hash);
        if (it == collateral_positions.end()) {
            LogPrintf("DigiDollar: DD UTXO position not found: %s\n", dd_utxos[0].hash.ToString());
            return false;
        }

        const WalletCollateralPosition& position = it->second;

        // Retrieve owner key from DD owner keys map
        CKey spenderKey;
        if (!GetOwnerKey(dd_utxos[0].hash, spenderKey)) {
            LogPrintf("DigiDollar: Owner key not found for DD UTXO %s\n", dd_utxos[0].hash.ToString());
            return false;
        }

        params.spenderKey = spenderKey;
        LogPrintf("DigiDollar: Retrieved owner key for DD transfer from position %s\n", dd_utxos[0].hash.ToString());

        // Get current chain height and oracle price (mock values for now)
        int currentHeight = 100000;  // TODO: Get actual height from chainstate
        CAmount oraclePrice = 2500;   // TODO: Get from MockOracleManager

        // Build transaction
        DigiDollar::TransferTxBuilder builder(Params(), currentHeight, oraclePrice);
        DigiDollar::TxBuilderResult result = builder.BuildTransferTransaction(params);

        if (!result.success) {
            LogPrintf("DigiDollar: Transfer transaction build failed - %s\n", result.error);
            return false;
        }

        // Create transaction reference
        tx_out = MakeTransactionRef(result.tx);

        // FIX #3: Broadcast transaction to network (GREEN PHASE)
        if (m_wallet) {
            std::string broadcast_error;
            const CAmount max_tx_fee = wallet::DEFAULT_TRANSACTION_MAXFEE;

            // Broadcast transaction (includes mempool submission and network relay)
            bool broadcast_success = m_wallet->chain().broadcastTransaction(
                tx_out,
                max_tx_fee,
                /*relay=*/true,
                broadcast_error
            );

            if (!broadcast_success) {
                LogPrintf("DigiDollar: Broadcast failed - %s\n", broadcast_error);
                return false;
            }

            LogPrintf("DigiDollar: Transaction broadcast successful - txid: %s\n",
                     tx_out->GetHash().ToString());
        } else {
            LogPrintf("DigiDollar: WARNING - No wallet context, transaction built but not broadcast\n");
            return false;
        }

        // CRITICAL: DD Transfers DON'T create/destroy time-locks!
        // Time-locks (vout[0] of mint) stay intact until redemption
        // Only DD tokens (vout[1]) move between wallets
        //
        // What we need to do:
        // 1. Mark spent DD UTXOs as spent (NOT the time-lock position!)
        // 2. Add new DD UTXOs from transaction outputs (for change)
        // 3. DO NOT modify time-lock positions at all
        //
        // The time-lock position stays ACTIVE because the DGB is still locked!
        // Only the DD ownership changes.

        LogPrintf("DigiDollar: Transfer completed - DD ownership transferred, time-locks unchanged\n");

        // The transaction is built correctly by txbuilder:
        // - Inputs: DD UTXOs being spent
        // - Outputs: DD to recipient, DD change (if any), DGB change (if any)
        //
        // Balance will update automatically when we detect our change output
        // (either in this wallet if sending to self, or in receiving wallet)
        //
        // IMPORTANT: We do NOT mark positions inactive or create new positions!
        // The collateral positions represent time-locked DGB, which doesn't move.

        // Create DD transaction record for history
        DDTransaction ddtx;
        ddtx.txid = tx_out->GetHash().ToString();
        ddtx.amount = amount;
        ddtx.timestamp = GetTime();
        ddtx.confirmations = 0;
        ddtx.incoming = false;
        ddtx.address = to.ToString();
        ddtx.category = "send";

        // Add to transaction history
        transaction_history.push_back(ddtx);

        // Persist transaction to database if wallet available
        if (m_wallet) {
            wallet::WalletBatch batch(m_wallet->GetDatabase());
            if (!batch.WriteDDTransaction(ddtx)) {
                LogPrintf("DigiDollarWallet::TransferDigiDollar - Failed to write transaction\n");
            }
        }

        // Log balance update
        CAmount newBalance = GetTotalDDBalance();
        CAmount dd_change = selectedDDTotal - amount; // Calculate change from selected inputs
        LogPrintf("DigiDollar: Transfer successful - %d cents to %s (txid: %s)\n",
                  amount, to.ToString(), ddtx.txid);
        LogPrintf("DigiDollar: Balance after transfer: %d (change: %d)\n",
                  newBalance, dd_change);

        return true;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: TransferDigiDollar exception - %s\n", e.what());
        return false;
    }
}

bool DigiDollarWallet::RedeemDigiDollar(const uint256& dd_timelock_id, const CAmount& amount, CTransactionRef& tx_out) {
    try {
        LogPrintf("DigiDollar: RedeemDigiDollar called - position: %s, amount: %d\n", dd_timelock_id.ToString(), amount);

        // RED phase implementation - validation only
        if (!ValidateRedeemParams(dd_timelock_id, amount)) {
            LogPrintf("DigiDollar: RedeemDigiDollar validation failed\n");
            return false;
        }

        // For RED phase, return false as transaction creation not implemented
        LogPrintf("DigiDollar: RedeemDigiDollar not fully implemented (RED phase)\n");
        return false;

        // GREEN phase implementation would be:
        /*
        // Get position details
        auto it = collateral_positions.find(dd_timelock_id);
        if (it == collateral_positions.end() || !it->second.is_active) {
            LogPrintf("DigiDollar: Position not found or inactive\n");
            return false;
        }

        // Create redemption transaction using RedeemTxBuilder
        DigiDollar::RedeemTxBuilder builder(Params(), GetCurrentHeight(), GetOraclePrice());

        DigiDollar::TxBuilderRedeemParams params;
        params.collateralOutpoint = COutPoint(dd_timelock_id, 0); // Assuming output 0
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

        // Mark position as inactive
        if (!UpdatePositionStatus(dd_timelock_id, false)) {
            LogPrintf("DigiDollarWallet::RedeemDigiDollar - Failed to update position status\n");
        }

        // Record redemption transaction
        DDTransaction ddtx;
        ddtx.txid = tx_out->GetHash().ToString();
        ddtx.amount = amount;
        ddtx.timestamp = GetTime();
        ddtx.confirmations = 0;
        ddtx.incoming = true;  // Receiving DGB back
        ddtx.address = "";
        ddtx.category = "redeem";

        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTransaction(ddtx)) {
            LogPrintf("DigiDollarWallet::RedeemDigiDollar - Failed to write transaction\n");
        }

        return true;
        */

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: RedeemDigiDollar exception - %s\n", e.what());
        return false;
    }
}

// =============================================================================
// PHASE 5.3: DDTIMELOCK STATUS MANAGEMENT IMPLEMENTATIONS
// =============================================================================

bool DigiDollarWallet::UpdateDDTimeLockStatus(const uint256& dd_timelock_id, bool new_status) {
    // Validate input
    if (dd_timelock_id.IsNull()) {
        LogPrintf("DigiDollar: UpdateDDTimeLockStatus - Invalid DDTimeLock ID (null)\n");
        return false;
    }

    // Find DDTimeLock position
    auto it = collateral_positions.find(dd_timelock_id);
    if (it == collateral_positions.end()) {
        LogPrintf("DigiDollar: UpdateDDTimeLockStatus - DDTimeLock not found: %s\n",
                  dd_timelock_id.ToString());
        return false;
    }

    // Update status in memory
    bool old_status = it->second.is_active;
    it->second.is_active = new_status;

    // Persist to database if wallet available
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTimeLock(it->second)) {
            LogPrintf("DigiDollar: UpdateDDTimeLockStatus - Failed to persist status update\n");
            // Rollback in-memory change
            it->second.is_active = old_status;
            return false;
        }

        // Recalculate locked collateral
        CAmount total_locked = 0;
        for (const auto& [id, pos] : collateral_positions) {
            if (pos.is_active) {
                total_locked += pos.dgb_collateral;
            }
        }
        locked_collateral = total_locked;

        LogPrintf("DigiDollar: Updated DDTimeLock %s status: %s → %s (locked collateral: %d)\n",
                  dd_timelock_id.ToString(),
                  old_status ? "ACTIVE" : "INACTIVE",
                  new_status ? "ACTIVE" : "INACTIVE",
                  total_locked);
    } else {
        LogPrintf("DigiDollar: Updated DDTimeLock %s status to %s (NO DB - mock mode)\n",
                  dd_timelock_id.ToString(),
                  new_status ? "ACTIVE" : "INACTIVE");
    }

    return true;
}

bool DigiDollarWallet::TrackPartialRedemption(const uint256& dd_timelock_id, CAmount dd_redeemed) {
    // Validate input
    if (dd_timelock_id.IsNull()) {
        LogPrintf("DigiDollar: TrackPartialRedemption - Invalid DDTimeLock ID (null)\n");
        return false;
    }

    if (dd_redeemed <= 0) {
        LogPrintf("DigiDollar: TrackPartialRedemption - Invalid redemption amount: %d\n", dd_redeemed);
        return false;
    }

    // Find DDTimeLock position
    auto it = collateral_positions.find(dd_timelock_id);
    if (it == collateral_positions.end()) {
        LogPrintf("DigiDollar: TrackPartialRedemption - DDTimeLock not found: %s\n",
                  dd_timelock_id.ToString());
        return false;
    }

    // Validate redemption amount doesn't exceed minted amount
    if (it->second.dd_minted < dd_redeemed) {
        LogPrintf("DigiDollar: TrackPartialRedemption - Redemption amount (%d) exceeds minted amount (%d)\n",
                  dd_redeemed, it->second.dd_minted);
        return false;
    }

    // Store original values for logging
    CAmount original_dd = it->second.dd_minted;
    bool originally_active = it->second.is_active;

    // Reduce dd_minted by redeemed amount
    it->second.dd_minted -= dd_redeemed;

    // If fully redeemed (no DD remaining), mark inactive
    if (it->second.dd_minted == 0) {
        it->second.is_active = false;
    }

    // Persist to database if wallet available
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTimeLock(it->second)) {
            LogPrintf("DigiDollar: TrackPartialRedemption - Failed to persist redemption\n");
            // Rollback changes
            it->second.dd_minted = original_dd;
            it->second.is_active = originally_active;
            return false;
        }

        // Recalculate locked collateral if status changed
        if (it->second.is_active != originally_active) {
            CAmount total_locked = 0;
            for (const auto& [id, pos] : collateral_positions) {
                if (pos.is_active) {
                    total_locked += pos.dgb_collateral;
                }
            }
            locked_collateral = total_locked;
        }

        LogPrintf("DigiDollar: Tracked partial redemption - DDTimeLock %s: %d DD redeemed, %d DD remaining%s\n",
                  dd_timelock_id.ToString(),
                  dd_redeemed,
                  it->second.dd_minted,
                  it->second.dd_minted == 0 ? " (FULLY REDEEMED)" : "");
    } else {
        LogPrintf("DigiDollar: Tracked partial redemption - %d DD redeemed from %s (NO DB - mock mode)\n",
                  dd_redeemed, dd_timelock_id.ToString());
    }

    return true;
}

std::string DigiDollarWallet::GetDDTimeLockStatus(const uint256& dd_timelock_id) const {
    // Validate input
    if (dd_timelock_id.IsNull()) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: GetDDTimeLockStatus - Invalid DDTimeLock ID (null)\n");
        return "not_found";
    }

    // Find DDTimeLock position
    auto it = collateral_positions.find(dd_timelock_id);
    if (it == collateral_positions.end()) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: GetDDTimeLockStatus - DDTimeLock not found: %s\n",
                 dd_timelock_id.ToString());
        return "not_found";
    }

    const WalletCollateralPosition& position = it->second;

    // Determine status based on is_active and dd_minted
    if (!position.is_active) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: DDTimeLock %s status: fully_redeemed\n",
                 dd_timelock_id.ToString());
        return "fully_redeemed";
    }

    // Active position
    LogPrint(BCLog::WALLETDB, "DigiDollar: DDTimeLock %s status: active (%d DD)\n",
             dd_timelock_id.ToString(), position.dd_minted);
    return "active";
}

bool DigiDollarWallet::IsDDTimeLockRedeemable(const uint256& dd_timelock_id, int current_height) const {
    // Validate input
    if (dd_timelock_id.IsNull()) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: IsDDTimeLockRedeemable - Invalid DDTimeLock ID (null)\n");
        return false;
    }

    // Find DDTimeLock position
    auto it = collateral_positions.find(dd_timelock_id);
    if (it == collateral_positions.end()) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: IsDDTimeLockRedeemable - DDTimeLock not found: %s\n",
                 dd_timelock_id.ToString());
        return false;
    }

    const WalletCollateralPosition& position = it->second;

    // Must be active
    if (!position.is_active) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: DDTimeLock %s not redeemable - inactive\n",
                 dd_timelock_id.ToString());
        return false;
    }

    // Must be unlocked (current height >= unlock height)
    if (current_height < position.unlock_height) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: DDTimeLock %s not redeemable - still locked (height %d < unlock %d)\n",
                 dd_timelock_id.ToString(), current_height, position.unlock_height);
        return false;
    }

    // Must have DD remaining
    if (position.dd_minted == 0) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: DDTimeLock %s not redeemable - no DD remaining\n",
                 dd_timelock_id.ToString());
        return false;
    }

    LogPrint(BCLog::WALLETDB, "DigiDollar: DDTimeLock %s is REDEEMABLE (height: %d, unlock: %d, DD: %d)\n",
             dd_timelock_id.ToString(), current_height, position.unlock_height, position.dd_minted);

    return true;
}

// =============================================================================
// PHASE 5 TEST HELPERS AND UTILITY FUNCTIONS
// =============================================================================

void DigiDollarWallet::SetMockDDBalance(const CDigiDollarAddress& addr, CAmount balance) {
    WriteDDBalance(addr, balance);
}

void DigiDollarWallet::AddMockPosition(const uint256& id, CAmount dd, CAmount dgb, uint32_t tier, int64_t height) {
    WalletCollateralPosition position(id, dd, dgb, tier, height);

    // For testing without a wallet pointer, directly update in-memory cache
    if (!m_wallet) {
        collateral_positions[id] = position;
        LogPrintf("DigiDollarWallet: Added mock position %s (DD: %d, DGB: %d) - NO DB\n",
                  id.ToString(), dd, dgb);
    } else {
        WriteDDTimeLock(position);
    }
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

bool DigiDollarWallet::ValidateRedeemParams(const uint256& dd_timelock_id, const CAmount& amount) const {
    if (dd_timelock_id.IsNull()) {
        LogPrintf("DigiDollar: Invalid position ID\n");
        return false;
    }

    if (amount <= 0) {
        LogPrintf("DigiDollar: Invalid redemption amount: %d\n", amount);
        return false;
    }

    auto it = collateral_positions.find(dd_timelock_id);
    if (it == collateral_positions.end()) {
        LogPrintf("DigiDollar: Position not found: %s\n", dd_timelock_id.ToString());
        return false;
    }

    if (!it->second.is_active) {
        LogPrintf("DigiDollar: Position is not active: %s\n", dd_timelock_id.ToString());
        return false;
    }

    if (amount > it->second.dd_minted) {
        LogPrintf("DigiDollar: Redemption amount exceeds position: %d > %d\n", amount, it->second.dd_minted);
        return false;
    }

    return true;
}

bool DigiDollarWallet::SelectDDCoins(const CAmount& target_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total) const {
    // Reset output parameters
    selected_total = 0;
    selected_utxos.clear();

    // Validate target amount
    if (target_amount <= 0) {
        LogPrintf("DigiDollar: SelectDDCoins - Invalid target amount %d\n", target_amount);
        return false;
    }

    LogPrintf("DigiDollar: SelectDDCoins - target: %d cents\n", target_amount);

    // Get all spendable DD UTXOs
    std::vector<DDUtxo> available_utxos = GetDDUTXOs();

    if (available_utxos.empty()) {
        LogPrintf("DigiDollar: SelectDDCoins - No DD UTXOs available\n");
        return false;
    }

    // Greedy selection: Sort by amount (smallest first for better privacy)
    std::sort(available_utxos.begin(), available_utxos.end(),
              [](const DDUtxo& a, const DDUtxo& b) {
                  return a.dd_amount < b.dd_amount;
              });

    // Select UTXOs until target amount met
    for (const auto& utxo : available_utxos) {
        if (selected_total >= target_amount) break;

        selected_utxos.push_back(utxo.outpoint);
        selected_total += utxo.dd_amount;

        LogPrintf("DigiDollar: SelectDDCoins - Selected UTXO %s:%d (%d cents, total: %d)\n",
                  utxo.outpoint.hash.ToString(), utxo.outpoint.n,
                  utxo.dd_amount, selected_total);
    }

    bool success = (selected_total >= target_amount);

    if (!success) {
        LogPrintf("DigiDollar: SelectDDCoins - FAILED: need %d, have %d\n",
                  target_amount, selected_total);
        selected_utxos.clear();
        selected_total = 0;
    } else {
        LogPrintf("DigiDollar: SelectDDCoins - SUCCESS: selected %d cents from %d UTXOs\n",
                  selected_total, selected_utxos.size());
    }

    return success;
}

bool DigiDollarWallet::SelectFeeCoins(const CAmount& fee_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total, std::vector<CAmount>* selected_amounts) const {
    // Reset output parameters
    selected_total = 0;
    selected_utxos.clear();
    if (selected_amounts) selected_amounts->clear();

    // Validate fee amount
    if (fee_amount <= 0) {
        LogPrintf("DigiDollar: SelectFeeCoins - Invalid fee amount %d\n", fee_amount);
        return false;
    }

    // Check if wallet pointer exists
    if (!m_wallet) {
        LogPrintf("DigiDollar: SelectFeeCoins - No wallet available for UTXO selection\n");
        return false;
    }

    LogPrintf("DigiDollar: SelectFeeCoins - target fee: %d satoshis\n", fee_amount);

    // Get available DGB UTXOs from wallet
    std::vector<wallet::COutput> available_coins;

    // Lock wallet and get available coins
    LOCK(m_wallet->cs_wallet);
    wallet::CCoinControl coin_control;
    coin_control.m_include_unsafe_inputs = false;  // Only safe inputs

    wallet::CoinFilterParams filter_params;
    filter_params.only_spendable = true;
    filter_params.min_amount = 1;  // Minimum 1 satoshi
    filter_params.include_immature_coinbase = false;  // Exclude immature coinbase

    available_coins = wallet::AvailableCoins(*m_wallet, &coin_control, std::nullopt, filter_params).All();

    if (available_coins.empty()) {
        LogPrintf("DigiDollar: SelectFeeCoins - No DGB UTXOs available\n");
        return false;
    }

    LogPrintf("DigiDollar: SelectFeeCoins - Found %d available DGB UTXOs\n", available_coins.size());

    // Sort by amount (smallest first for efficiency)
    std::sort(available_coins.begin(), available_coins.end(),
              [](const wallet::COutput& a, const wallet::COutput& b) {
                  return a.txout.nValue < b.txout.nValue;
              });

    // Select UTXOs until fee covered
    for (const auto& coin : available_coins) {
        if (selected_total >= fee_amount) break;

        COutPoint outpoint = coin.outpoint;
        CAmount amount = coin.txout.nValue;

        selected_utxos.push_back(outpoint);
        if (selected_amounts) selected_amounts->push_back(amount);
        selected_total += amount;

        LogPrintf("DigiDollar: SelectFeeCoins - Selected UTXO %s:%d (%d sats)\n",
                  outpoint.hash.ToString(), outpoint.n, amount);
    }

    bool success = (selected_total >= fee_amount);

    if (!success) {
        LogPrintf("DigiDollar: SelectFeeCoins - FAILED: need %d sats, have %d\n",
                  fee_amount, selected_total);
        selected_utxos.clear();
        if (selected_amounts) selected_amounts->clear();
        selected_total = 0;
    } else {
        LogPrintf("DigiDollar: SelectFeeCoins - SUCCESS: selected %d sats from %d UTXOs\n",
                  selected_total, selected_utxos.size());
    }

    return success;
}

CAmount DigiDollarWallet::CalculateTransactionFee(const CMutableTransaction& tx) const {
    // DigiDollar fee constants (match network requirements)
    // DigiByte uses KvB (kilobyte), not vB (virtual bytes)
    static const CAmount MIN_RELAY_FEE_PER_KB = 1000;  // 0.00001 DGB/kB
    static const CAmount DEFAULT_FEE_RATE = 10000;     // 0.0001 DGB/kB (10x min for faster confirmation)

    // Calculate transaction size
    // NOTE: This is an estimate. Actual size determined after signing
    unsigned int tx_size = GetSerializeSize(tx, PROTOCOL_VERSION);

    // Add estimated witness size for P2TR inputs
    // Each P2TR witness is approximately 64 bytes (Schnorr signature)
    size_t num_inputs = tx.vin.size();
    unsigned int estimated_witness_size = num_inputs * 64;
    unsigned int total_size = tx_size + estimated_witness_size;

    // Calculate fee in satoshis
    // Formula: (size_in_bytes / 1000) * fee_rate_per_KB
    CAmount fee = (total_size * DEFAULT_FEE_RATE) / 1000;

    // Ensure minimum fee
    CAmount min_fee = (total_size * MIN_RELAY_FEE_PER_KB) / 1000;
    if (fee < min_fee) {
        fee = min_fee;
    }

    LogPrintf("DigiDollar: CalculateTransactionFee - size: %d bytes, fee: %d sats\n",
              total_size, fee);

    return fee;
}
// =============================================================================
// PHASE 3.1: P2TR SIGNING FOR DD INPUTS (SCHNORR SIGNATURES)
// =============================================================================

bool DigiDollarWallet::SignDDInputs(CMutableTransaction& tx, const std::vector<COutPoint>& dd_utxos) {
    if (!m_wallet) {
        LogPrintf("DigiDollar: SignDDInputs - No wallet available\n");
        return false;
    }

    LogPrintf("DigiDollar: SignDDInputs - Signing %d DD inputs\n", dd_utxos.size());

    // Verify dd_utxos matches number of transaction inputs at start
    if (dd_utxos.size() > tx.vin.size()) {
        LogPrintf("DigiDollar: SignDDInputs - More DD UTXOs (%d) than tx inputs (%d)\n",
                  dd_utxos.size(), tx.vin.size());
        return false;
    }

    // Sign each DD input with Schnorr signature
    for (size_t i = 0; i < dd_utxos.size(); i++) {
        const COutPoint& utxo = dd_utxos[i];

        // Get DDTimeLock for this UTXO to retrieve DD amount
        auto it = collateral_positions.find(utxo.hash);
        if (it == collateral_positions.end()) {
            LogPrintf("DigiDollar: SignDDInputs - DDTimeLock not found: %s\n",
                      utxo.hash.ToString());
            return false;
        }

        const WalletCollateralPosition& timelock = it->second;
        CAmount dd_amount = timelock.dd_minted;

        LogPrintf("DigiDollar: SignDDInputs - Input %d: UTXO %s:%d, DD amount: %d cents\n",
                  i, utxo.hash.ToString(), utxo.n, dd_amount);

        // Get signing key from wallet
        // TODO: In full wallet integration (Phase 7), retrieve actual key:
        // CKey signing_key;
        // if (!m_wallet->GetKey(address, signing_key)) {
        //     LogPrintf("DigiDollar: SignDDInputs - Failed to get signing key\n");
        //     return false;
        // }

        // For Phase 3, use placeholder key (will be replaced in Phase 7)
        CKey signing_key;
        signing_key.MakeNewKey(true);

        if (!signing_key.IsValid()) {
            LogPrintf("DigiDollar: SignDDInputs - Invalid signing key for input %d\n", i);
            return false;
        }

        // Create sighash for this input (BIP341 - Taproot)
        // For P2TR key path spending, prevoutScript is empty
        CScript prevoutScript;
        uint256 sighash = SignatureHash(
            prevoutScript,       // Empty for P2TR key path
            tx,                  // Transaction to sign
            i,                   // Input index
            SIGHASH_DEFAULT,     // Taproot default sighash type
            dd_amount,           // Amount (DD amount in this case)
            SigVersion::TAPROOT, // Taproot signing
            nullptr              // No precomputed data
        );

        LogPrintf("DigiDollar: SignDDInputs - Input %d sighash: %s\n", i, sighash.ToString());

        // Sign with Schnorr (BIP340)
        std::vector<unsigned char> sig(64); // Schnorr signatures are 64 bytes
        uint256 aux_rand = GetRandHash(); // Auxiliary random data for Schnorr

        if (!signing_key.SignSchnorr(sighash, sig, nullptr, aux_rand)) {
            LogPrintf("DigiDollar: SignDDInputs - Schnorr signing failed for input %d\n", i);
            return false;
        }

        LogPrintf("DigiDollar: SignDDInputs - Input %d signature created (%d bytes)\n", i, sig.size());

        // Build witness stack for P2TR key path: [signature]
        // For P2TR key path spending, witness stack contains only the signature
        tx.vin[i].scriptWitness.stack.clear();
        tx.vin[i].scriptWitness.stack.push_back(sig);

        LogPrintf("DigiDollar: SignDDInputs - Input %d witness stack built (1 element)\n", i);
    }

    LogPrintf("DigiDollar: SignDDInputs - Successfully signed %d DD inputs\n", dd_utxos.size());
    return true;
}


// =============================================================================
// PHASE 3.2: FEE INPUT SIGNING IMPLEMENTATION
// =============================================================================

bool DigiDollarWallet::SignFeeInputs(CMutableTransaction& tx,
                                      const std::vector<COutPoint>& fee_utxos,
                                      size_t dd_input_count) {
    if (!m_wallet) {
        LogPrintf("DigiDollar: SignFeeInputs - No wallet available\n");
        return false;
    }

    // Validate that we have fee inputs to sign
    if (fee_utxos.empty()) {
        LogPrintf("DigiDollar: SignFeeInputs - No fee UTXOs provided\n");
        return true; // No fee inputs to sign is valid (fee-less tx)
    }

    // Fee inputs come after DD inputs in the transaction
    size_t fee_input_start = dd_input_count;

    // Validate transaction structure
    if (tx.vin.size() < fee_input_start + fee_utxos.size()) {
        LogPrintf("DigiDollar: SignFeeInputs - Transaction missing fee inputs (have %d, need %d)\n",
                  tx.vin.size(), fee_input_start + fee_utxos.size());
        return false;
    }

    LogPrintf("DigiDollar: SignFeeInputs - Signing %d fee inputs starting at index %d\n",
              fee_utxos.size(), fee_input_start);

    // Lock wallet for thread-safe access
    LOCK(m_wallet->cs_wallet);

    // Sign each fee input
    for (size_t i = 0; i < fee_utxos.size(); i++) {
        size_t input_index = fee_input_start + i;
        const COutPoint& utxo = fee_utxos[i];

        // Get UTXO details from wallet
        const auto& it = m_wallet->mapWallet.find(utxo.hash);
        if (it == m_wallet->mapWallet.end()) {
            LogPrintf("DigiDollar: SignFeeInputs - Fee UTXO %s not found in wallet\n",
                      utxo.ToString());
            return false;
        }

        const wallet::CWalletTx& wtx = it->second;
        if (utxo.n >= wtx.tx->vout.size()) {
            LogPrintf("DigiDollar: SignFeeInputs - Invalid output index %d for UTXO %s\n",
                      utxo.n, utxo.hash.ToString());
            return false;
        }

        const CTxOut& prev_out = wtx.tx->vout[utxo.n];
        CAmount value = prev_out.nValue;
        const CScript& prevScript = prev_out.scriptPubKey;

        LogPrintf("DigiDollar: SignFeeInputs - Signing input %d: UTXO %s:%d, value: %d sats\n",
                  input_index, utxo.hash.ToString(), utxo.n, value);

        // Get the signing provider for this script
        std::unique_ptr<SigningProvider> provider = m_wallet->GetSolvingProvider(prevScript);
        if (!provider) {
            LogPrintf("DigiDollar: SignFeeInputs - No signing provider for fee input %d\n", input_index);
            return false;
        }

        // Create signature data structure
        SignatureData sigdata;

        // Sign the input using the signing provider
        // MutableTransactionSignatureCreator handles both P2TR and P2WPKH
        MutableTransactionSignatureCreator creator(tx, input_index, value, SIGHASH_ALL);

        // Use ProduceSignature to create the signature
        // This automatically handles P2TR (Taproot) and P2WPKH (SegWit) inputs
        bool sign_success = ProduceSignature(*provider, creator, prevScript, sigdata);

        if (!sign_success) {
            LogPrintf("DigiDollar: SignFeeInputs - Failed to sign fee input %d (UTXO %s:%d)\n",
                      input_index, utxo.hash.ToString(), utxo.n);
            return false;
        }

        // Update the transaction input with the signature
        UpdateInput(tx.vin[input_index], sigdata);

        LogPrintf("DigiDollar: SignFeeInputs - Successfully signed fee input %d\n", input_index);
    }

    LogPrintf("DigiDollar: SignFeeInputs - Successfully signed all %d fee inputs\n", fee_utxos.size());
    return true;
}

// =============================================================================
// PHASE 3.3: COMPLETE TRANSACTION SIGNING COORDINATION
// =============================================================================

bool DigiDollarWallet::SignTransaction(CMutableTransaction& tx,
                                        const std::vector<COutPoint>& dd_utxos,
                                        const std::vector<COutPoint>& fee_utxos) {
    LogPrintf("DigiDollar: SignTransaction - Signing %d DD inputs and %d fee inputs\n",
              dd_utxos.size(), fee_utxos.size());

    // Sign DD inputs first
    if (!SignDDInputs(tx, dd_utxos)) {
        LogPrintf("DigiDollar: SignTransaction - Failed to sign DD inputs\n");
        return false;
    }

    // Sign fee inputs
    if (!fee_utxos.empty()) {
        if (!SignFeeInputs(tx, fee_utxos, dd_utxos.size())) {
            LogPrintf("DigiDollar: SignTransaction - Failed to sign fee inputs\n");
            return false;
        }
    }

    // Verify all inputs have witnesses
    for (size_t i = 0; i < tx.vin.size(); i++) {
        if (tx.vin[i].scriptWitness.IsNull()) {
            LogPrintf("DigiDollar: SignTransaction - Input %d missing witness\n", i);
            return false;
        }
    }

    LogPrintf("DigiDollar: SignTransaction - Successfully signed all inputs\n");
    return true;
}

// =============================================================================
// PHASE 5.2: UTXO SET UPDATE IMPLEMENTATIONS
// =============================================================================

bool DigiDollarWallet::MarkDDUTXOsSpent(const std::vector<COutPoint>& spent_utxos) {
    LogPrintf("DigiDollar: MarkDDUTXOsSpent - Marking %d DD UTXOs as spent\n", spent_utxos.size());

    // Validate input
    if (spent_utxos.empty()) {
        LogPrintf("DigiDollar: MarkDDUTXOsSpent - No UTXOs to mark\n");
        return true; // No UTXOs to mark is valid
    }

    // Get database batch if wallet available (for persistence)
    std::unique_ptr<wallet::WalletBatch> batch;
    if (m_wallet) {
        batch = std::make_unique<wallet::WalletBatch>(m_wallet->GetDatabase());
    }

    // Mark each UTXO as spent
    for (const auto& utxo : spent_utxos) {
        // DD UTXOs are at vout[1] of DDTimeLock transactions
        // The outpoint.hash is the dd_timelock_id

        // Validate UTXO index (DD outputs are always at index 1)
        if (utxo.n != 1) {
            LogPrintf("DigiDollar: MarkDDUTXOsSpent - Invalid UTXO index %d (expected 1): %s:%d\n",
                      utxo.n, utxo.hash.ToString(), utxo.n);
            // Continue marking other UTXOs even if one is invalid
            continue;
        }

        // Find position in cache
        auto it = collateral_positions.find(utxo.hash);
        if (it == collateral_positions.end()) {
            LogPrintf("DigiDollar: MarkDDUTXOsSpent - Warning: UTXO not found: %s:%d\n",
                      utxo.hash.ToString(), utxo.n);
            return false; // UTXO doesnt exist - this is an error
        }

        // Mark as inactive (spent)
        it->second.is_active = false;

        // Persist to database if wallet available
        if (batch) {
            if (!batch->WriteDDTimeLock(it->second)) {
                LogPrintf("DigiDollar: MarkDDUTXOsSpent - Failed to persist spent UTXO: %s\n",
                          utxo.hash.ToString());
                return false;
            }
        }

        LogPrintf("DigiDollar: Marked DD UTXO as spent: %s:%d (%d DD)\n",
                  utxo.hash.ToString(), utxo.n, it->second.dd_minted);
    }

    LogPrintf("DigiDollar: Successfully marked %d DD UTXOs as spent\n", spent_utxos.size());
    return true;
}

bool DigiDollarWallet::AddDDChangeUTXO(const CTransactionRef& tx, uint32_t change_vout, CAmount dd_amount) {
    LogPrintf("DigiDollar: AddDDChangeUTXO - Adding change UTXO at vout[%d] with %d DD\n",
              change_vout, dd_amount);

    // Validate inputs
    if (!tx) {
        LogPrintf("DigiDollar: AddDDChangeUTXO - Null transaction reference\n");
        return false;
    }

    if (dd_amount <= 0) {
        LogPrintf("DigiDollar: AddDDChangeUTXO - Invalid DD amount: %d\n", dd_amount);
        return false;
    }

    if (change_vout >= tx->vout.size()) {
        LogPrintf("DigiDollar: AddDDChangeUTXO - Invalid vout index %d (tx has %d outputs)\n",
                  change_vout, tx->vout.size());
        return false;
    }

    // FIX #1: Add change UTXO to dd_utxos map
    COutPoint change_outpoint(tx->GetHash(), change_vout);
    dd_utxos[change_outpoint] = dd_amount;
    LogPrintf("DigiDollar: Added DD change UTXO to tracking - %s:%d (%d cents)\n",
              change_outpoint.hash.ToString(), change_outpoint.n, dd_amount);

    // Persist DD UTXO to database
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDUTXO(change_outpoint, dd_amount)) {
            LogPrintf("DigiDollar: AddDDChangeUTXO - Failed to persist change UTXO to database\n");
            return false;
        }
    }

    // Legacy: Also create WalletCollateralPosition for compatibility
    // (This can be removed once all code uses dd_utxos instead of collateral_positions)
    WalletCollateralPosition change_position;
    change_position.dd_timelock_id = tx->GetHash();
    change_position.dd_minted = dd_amount;
    change_position.dgb_collateral = 0;      // Change has no collateral
    change_position.lock_tier = 0;           // Not a mint, so no tier
    change_position.unlock_height = 0;       // Not locked
    change_position.is_active = true;        // Spendable immediately

    // Add to in-memory cache
    collateral_positions[tx->GetHash()] = change_position;

    // Persist to database if wallet available
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTimeLock(change_position)) {
            LogPrintf("DigiDollar: AddDDChangeUTXO - Failed to persist change position\n");
            // Don't fail, we already persisted the UTXO
        }
    }

    LogPrintf("DigiDollar: Successfully added DD change UTXO: %s:%d (%d DD)\n",
              tx->GetHash().ToString(), change_vout, dd_amount);
    return true;
}

bool DigiDollarWallet::UpdateDDUTXOSet(const CTransactionRef& tx,
                                       const std::vector<COutPoint>& input_utxos,
                                       int change_vout,
                                       CAmount change_amount) {
    LogPrintf("DigiDollar: UpdateDDUTXOSet - Updating UTXO set for tx %s\n",
              tx ? tx->GetHash().ToString() : "null");

    // Validate transaction
    if (!tx) {
        LogPrintf("DigiDollar: UpdateDDUTXOSet - Null transaction reference\n");
        return false;
    }

    // Step 1: Mark input UTXOs as spent
    if (!input_utxos.empty()) {
        if (!MarkDDUTXOsSpent(input_utxos)) {
            LogPrintf("DigiDollar: UpdateDDUTXOSet - Failed to mark inputs spent\n");
            return false;
        }
    }

    // Step 2: Add change UTXO if present
    if (change_vout >= 0 && change_amount > 0) {
        if (!AddDDChangeUTXO(tx, change_vout, change_amount)) {
            LogPrintf("DigiDollar: UpdateDDUTXOSet - Failed to add change UTXO\n");
            // Note: Inputs already marked as spent at this point
            // This is a consistency error that would need recovery
            return false;
        }
    }

    LogPrintf("DigiDollar: UTXO set updated successfully for tx %s\n",
              tx->GetHash().ToString());
    return true;
}

// =============================================================================
// PHASE 4.1: MEMPOOL SUBMISSION IMPLEMENTATION
// =============================================================================

bool DigiDollarWallet::CommitDDTransaction(const CTransactionRef& tx, std::string& error) {
    // Clear previous error
    error.clear();

    LogPrintf("DigiDollar: CommitDDTransaction - Starting transaction submission\n");

    // Validate transaction reference is not null
    if (!tx) {
        error = "Transaction reference is null";
        LogPrintf("DigiDollar: CommitDDTransaction - ERROR: %s\n", error);
        return false;
    }

    // Validate wallet pointer is set
    if (!m_wallet) {
        error = "Wallet not initialized";
        LogPrintf("DigiDollar: CommitDDTransaction - ERROR: %s\n", error);
        return false;
    }

    // Validate transaction has inputs and outputs
    if (tx->vin.empty() || tx->vout.empty()) {
        error = "Invalid transaction: no inputs or outputs";
        LogPrintf("DigiDollar: CommitDDTransaction - ERROR: %s\n", error);
        return false;
    }

    // Log transaction details
    uint256 txid = tx->GetHash();
    LogPrintf("DigiDollar: Committing DD transfer transaction %s\n", txid.ToString());
    LogPrintf("DigiDollar: Transaction has %d inputs and %d outputs\n",
              tx->vin.size(), tx->vout.size());

    try {
        // Use CWallet::CommitTransaction to submit to mempool
        // This handles:
        // - Adding tx to wallet
        // - Marking UTXOs as spent
        // - Broadcasting to mempool
        // - Relaying to peers

        wallet::mapValue_t mapValue;  // Empty metadata for DD transfers
        std::vector<std::pair<std::string, std::string>> orderForm; // Empty order form

        // CommitTransaction does not return a value, it throws on error
        m_wallet->CommitTransaction(tx, std::move(mapValue), std::move(orderForm));

        LogPrintf("DigiDollar: Successfully committed transaction %s to mempool\n", txid.ToString());
        return true;

    } catch (const std::exception& e) {
        error = strprintf("Failed to commit transaction: %s", e.what());
        LogPrintf("DigiDollar: CommitDDTransaction - EXCEPTION: %s\n", error);
        return false;
    }
}

// =============================================================================
// PHASE 4.3: CONFIRMATION TRACKING IMPLEMENTATION
// =============================================================================

int DigiDollarWallet::GetDDTransactionConfirmations(const uint256& txid) const {
    if (!m_wallet) {
        LogPrintf("DigiDollar: GetDDTransactionConfirmations - No wallet pointer\n");
        return 0;
    }

    LogPrint(BCLog::WALLETDB, "DigiDollar: GetDDTransactionConfirmations - Checking confirmations for %s\n",
             txid.ToString());

    // Get transaction from wallet
    LOCK(m_wallet->cs_wallet);
    auto it = m_wallet->mapWallet.find(txid);
    if (it == m_wallet->mapWallet.end()) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: Transaction %s not found in wallet\n", txid.ToString());
        return 0;
    }

    const wallet::CWalletTx& wtx = it->second;

    // Get depth in main chain (number of confirmations)
    int depth = m_wallet->GetTxDepthInMainChain(wtx);

    LogPrint(BCLog::WALLETDB, "DigiDollar: Transaction %s has %d confirmations\n",
             txid.ToString(), depth);

    return depth;
}

void DigiDollarWallet::UpdateDDConfirmations(const uint256& block_hash) {
    LogPrintf("DigiDollar: UpdateDDConfirmations - Updating confirmations for block %s\n",
              block_hash.ToString());

    // Iterate through all DD transactions and update confirmation counts
    for (auto& ddtx : transaction_history) {
        uint256 txid;
        txid.SetHex(ddtx.txid);

        // Get updated confirmation count
        int new_confirmations = GetDDTransactionConfirmations(txid);

        // Update if changed
        if (ddtx.confirmations != new_confirmations) {
            LogPrint(BCLog::WALLETDB, "DigiDollar: Transaction %s confirmations: %d -> %d\n",
                     ddtx.txid, ddtx.confirmations, new_confirmations);

            ddtx.confirmations = new_confirmations;

            // Persist to database if wallet available
            if (m_wallet) {
                wallet::WalletBatch batch(m_wallet->GetDatabase());
                if (!batch.WriteDDTransaction(ddtx)) {
                    LogPrintf("DigiDollar: WARNING - Failed to write updated confirmations for %s\n",
                              ddtx.txid);
                }
            }
        }
    }

    LogPrintf("DigiDollar: Updated confirmations for %d DD transactions\n",
              transaction_history.size());
}

std::vector<uint256> DigiDollarWallet::GetUnconfirmedDDTransactions() const {
    std::vector<uint256> unconfirmed;

    LogPrint(BCLog::WALLETDB, "DigiDollar: GetUnconfirmedDDTransactions - Scanning transaction history\n");

    // Iterate through transaction history
    for (const auto& ddtx : transaction_history) {
        uint256 txid;
        txid.SetHex(ddtx.txid);

        // Check if transaction has 0 confirmations
        int confirmations = GetDDTransactionConfirmations(txid);
        if (confirmations == 0) {
            unconfirmed.push_back(txid);
            LogPrint(BCLog::WALLETDB, "DigiDollar: Found unconfirmed transaction %s\n",
                     ddtx.txid);
        }
    }

    LogPrint(BCLog::WALLETDB, "DigiDollar: Found %d unconfirmed DD transactions\n",
             unconfirmed.size());

    return unconfirmed;
}

// =============================================================================
// PHASE 6: RECEIVE OPERATIONS (Tasks 6.1-6.3)
// =============================================================================

/**
 * Detect if transaction has DD outputs to our wallet (Task 6.1)
 */
bool DigiDollarWallet::DetectIncomingDDOutputs(const CTransactionRef& tx,
                                               std::vector<std::pair<uint32_t, CAmount>>& our_dd_outputs) {
    if (!tx) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: DetectIncomingDDOutputs - null transaction\n");
        return false;
    }

    our_dd_outputs.clear();

    LogPrint(BCLog::WALLETDB, "DigiDollar: DetectIncomingDDOutputs - Checking transaction %s\n",
             tx->GetHash().ToString());

    // Iterate through transaction outputs
    for (uint32_t i = 0; i < tx->vout.size(); i++) {
        const CTxOut& txout = tx->vout[i];

        // Check if output is DD token script
        if (!DigiDollar::IsDDTokenScript(txout.scriptPubKey)) {
            LogPrint(BCLog::WALLETDB, "DigiDollar: Output %d is not a DD token script\n", i);
            continue;  // Not a DD output
        }

        // Extract DD amount from script
        CAmount dd_amount = 0;
        if (!DigiDollar::ExtractDDAmount(txout.scriptPubKey, dd_amount)) {
            LogPrint(BCLog::WALLETDB, "DigiDollar: Failed to extract DD amount from output %d\n", i);
            continue;  // Invalid DD output
        }

        if (dd_amount <= 0) {
            LogPrint(BCLog::WALLETDB, "DigiDollar: Output %d has invalid DD amount: %d\n", i, dd_amount);
            continue;  // Invalid amount
        }

        // Check if output address belongs to our wallet
        // For DD outputs, we need to check if we can spend it
        if (m_wallet) {
            LOCK(m_wallet->cs_wallet);

            // Check if we own this output
            wallet::isminetype mine = m_wallet->IsMine(txout);
            if (mine & wallet::ISMINE_SPENDABLE) {
                LogPrintf("DigiDollar: Detected incoming DD output - vout[%d]: %d DD cents\n",
                          i, dd_amount);
                our_dd_outputs.push_back(std::make_pair(i, dd_amount));
            } else {
                LogPrint(BCLog::WALLETDB, "DigiDollar: Output %d is DD but not ours (mine=%d)\n", i, mine);
            }
        } else {
            LogPrint(BCLog::WALLETDB, "DigiDollar: No wallet pointer, cannot check ownership\n");
        }
    }

    bool found = !our_dd_outputs.empty();
    LogPrint(BCLog::WALLETDB, "DigiDollar: DetectIncomingDDOutputs - Found %d output(s) for our wallet\n",
             our_dd_outputs.size());

    return found;
}

/**
 * Add received DD UTXO to spendable set (Task 6.3)
 */
bool DigiDollarWallet::AddReceivedDDUTXO(const CTransactionRef& tx,
                                         uint32_t vout_index,
                                         CAmount dd_amount) {
    if (!tx) {
        LogPrintf("DigiDollar: AddReceivedDDUTXO - null transaction\n");
        return false;
    }

    if (dd_amount <= 0) {
        LogPrintf("DigiDollar: AddReceivedDDUTXO - invalid DD amount: %d\n", dd_amount);
        return false;
    }

    uint256 txid = tx->GetHash();

    // Check for duplicate processing
    if (collateral_positions.find(txid) != collateral_positions.end()) {
        LogPrint(BCLog::WALLETDB,
                 "DigiDollar: AddReceivedDDUTXO - UTXO %s already exists, skipping\n",
                 txid.ToString());
        return true;  // Not an error, just already processed
    }

    LogPrintf("DigiDollar: AddReceivedDDUTXO - Adding received DD UTXO: %s:%d (%d DD cents)\n",
              txid.ToString(), vout_index, dd_amount);

    // Create new WalletCollateralPosition for received DD
    // Key difference from minted DD: no collateral in our wallet
    WalletCollateralPosition received_position;
    received_position.dd_timelock_id = txid;
    received_position.dd_minted = dd_amount;
    received_position.dgb_collateral = 0;      // Received DD has no collateral in our wallet
    received_position.lock_tier = 0;           // Not a mint position
    received_position.unlock_height = 0;       // Not locked (immediately spendable)
    received_position.is_active = true;        // Spendable immediately

    // Add to in-memory cache
    collateral_positions[txid] = received_position;

    // Persist to database
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTimeLock(received_position)) {
            LogPrintf("DigiDollar: AddReceivedDDUTXO - Failed to persist received DD UTXO to database\n");
            // Remove from cache since DB write failed
            collateral_positions.erase(txid);
            return false;
        }
        LogPrint(BCLog::WALLETDB, "DigiDollar: Persisted received DD UTXO to wallet.dat\n");
    }

    // Balance updates automatically (UTXO-derived approach from Phase 5.1)
    // GetTotalDDBalance() will now include this new position
    CAmount new_balance = GetTotalDDBalance();
    LogPrintf("DigiDollar: Balance after receive: %d DD cents\n", new_balance);

    return true;
}

/**
 * Process incoming DD transaction (Tasks 6.1-6.3 combined)
 */
bool DigiDollarWallet::ProcessIncomingDDTransaction(const CTransactionRef& tx) {
    if (!tx) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: ProcessIncomingDDTransaction - null transaction\n");
        return false;
    }

    if (!m_wallet) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: ProcessIncomingDDTransaction - no wallet pointer\n");
        return false;
    }

    LogPrint(BCLog::WALLETDB, "DigiDollar: ProcessIncomingDDTransaction - Processing tx %s\n",
             tx->GetHash().ToString());

    // Task 6.1: Detect incoming DD outputs
    std::vector<std::pair<uint32_t, CAmount>> our_dd_outputs;
    if (!DetectIncomingDDOutputs(tx, our_dd_outputs)) {
        // No DD outputs for us in this transaction - this is normal, not an error
        LogPrint(BCLog::WALLETDB, "DigiDollar: No DD outputs for our wallet in tx %s\n",
                 tx->GetHash().ToString());
        return true;  // Not an error, just nothing for us
    }

    LogPrintf("DigiDollar: Found %d DD output(s) for our wallet in tx %s\n",
              our_dd_outputs.size(), tx->GetHash().ToString());

    // Task 6.3: Add each received DD UTXO to spendable set
    for (const auto& [vout_index, dd_amount] : our_dd_outputs) {
        if (!AddReceivedDDUTXO(tx, vout_index, dd_amount)) {
            LogPrintf("DigiDollar: Failed to add received UTXO vout[%d] from tx %s\n",
                      vout_index, tx->GetHash().ToString());
            return false;
        }

        // Task 6.2: Balance updates automatically (UTXO-derived)
        // GetTotalDDBalance() will now include this new position
        LogPrint(BCLog::WALLETDB, "DigiDollar: Added received UTXO vout[%d]: %d DD cents\n",
                 vout_index, dd_amount);
    }

    // Log final balance after receive
    CAmount new_balance = GetTotalDDBalance();
    LogPrintf("DigiDollar: Receive complete - New balance: %d DD cents (%.2f DD)\n",
              new_balance, new_balance / 100.0);

    return true;
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