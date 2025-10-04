// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_WALLET_DIGIDOLLARWALLET_H
#define DIGIBYTE_WALLET_DIGIDOLLARWALLET_H

#include <consensus/amount.h>
#include <key.h>
#include <wallet/wallet.h>
#include <digidollar/digidollar.h>
#include <digidollar/txbuilder.h>
#include <base58.h>

#include <string>
#include <vector>

// Use existing CDigiDollarAddress from base58.h

/**
 * DigiDollar transaction structure for wallet
 * Represents a DD transaction from wallet perspective
 */
struct DDTransaction {
    std::string txid;
    CAmount amount;         // DD amount in cents
    uint64_t timestamp;
    int confirmations;
    bool incoming;          // true for receives, false for sends
    std::string address;    // counterparty address
    std::string category;   // "send", "receive", "mint", "redeem"

    DDTransaction();

    SERIALIZE_METHODS(DDTransaction, obj)
    {
        READWRITE(obj.txid);
        READWRITE(obj.amount);
        READWRITE(obj.timestamp);
        READWRITE(obj.confirmations);
        READWRITE(obj.incoming);
        READWRITE(obj.address);
        READWRITE(obj.category);
    }
};

// =============================================================================
// PHASE 5 WALLET CORE STRUCTURES (Tasks 5.1-5.3)
// =============================================================================

/**
 * Database schema for wallet.dat DD extension (Task 5.1)
 * Defines structures for storing DD wallet data
 */
struct WalletDDBalance {
    CDigiDollarAddress address;
    CAmount balance;
    int64_t last_updated;

    WalletDDBalance() : balance(0), last_updated(0) {}
    WalletDDBalance(const CDigiDollarAddress& addr, CAmount bal)
        : address(addr), balance(bal), last_updated(0) {}

    SERIALIZE_METHODS(WalletDDBalance, obj)
    {
        READWRITE(obj.address);
        READWRITE(obj.balance);
        READWRITE(obj.last_updated);
    }
};

struct WalletCollateralPosition {
    uint256 dd_timelock_id;  // txid of mint tx
    CAmount dd_minted;
    CAmount dgb_collateral;
    uint32_t lock_tier;
    int64_t unlock_height;
    bool is_active;
    CKeyID owner_keyid;      // Key ID that owns this DD time-lock

    WalletCollateralPosition()
        : dd_minted(0), dgb_collateral(0), lock_tier(0), unlock_height(0), is_active(false) {}
    WalletCollateralPosition(const uint256& id, CAmount dd, CAmount dgb, uint32_t tier, int64_t height)
        : dd_timelock_id(id), dd_minted(dd), dgb_collateral(dgb), lock_tier(tier), unlock_height(height), is_active(true) {}

    SERIALIZE_METHODS(WalletCollateralPosition, obj)
    {
        READWRITE(obj.dd_timelock_id);
        READWRITE(obj.dd_minted);
        READWRITE(obj.dgb_collateral);
        READWRITE(obj.lock_tier);
        READWRITE(obj.unlock_height);
        READWRITE(obj.is_active);
        READWRITE(obj.owner_keyid);
    }
};

/**
 * Represents a spendable DigiDollar UTXO
 * DD UTXOs are always at output index 1 of DDTimeLock mint transactions
 */
struct DDUtxo {
    COutPoint outpoint;     // UTXO reference (dd_timelock_id, 1)
    CAmount dd_amount;      // DD amount in cents
    bool is_spendable;      // Always true for active DDTimeLocks

    DDUtxo(const COutPoint& out, CAmount amt)
        : outpoint(out), dd_amount(amt), is_spendable(true) {}
};

/**
 * DigiDollar wallet functionality
 * Provides high-level interface for DD operations
 * Enhanced with Phase 5 core functions (Tasks 5.1-5.3)
 */
class DigiDollarWallet {
private:
    // Mock data for testing - in real implementation would use actual wallet
    CAmount mockBalance;
    std::vector<DDTransaction> mockHistory;
    std::vector<CDigiDollarOutput> mockUTXOs;

    // Phase 5 additions: Core wallet data structures (Task 5.1)
    std::map<std::string, WalletDDBalance> dd_balances;
    std::map<uint256, WalletCollateralPosition> collateral_positions;
    std::vector<DDTransaction> transaction_history;

    // FIX #1: Track actual DD UTXOs (not just positions)
    // Maps (txid, vout) → DD amount in cents
    // This replaces the broken assumption that DD is always at (mint_txid, 1)
    std::map<COutPoint, CAmount> dd_utxos;

    // Internal state tracking (Task 5.2)
    CAmount total_dd_balance;
    CAmount locked_collateral;

    // DD owner keys storage (for signing transfers)
    // Maps dd_timelock_id -> owner CKey
    std::map<uint256, CKey> dd_owner_keys;

    // Pointer to wallet for UTXO access
    wallet::CWallet* m_wallet;

public:
    DigiDollarWallet();
    DigiDollarWallet(wallet::CWallet* wallet);
    virtual ~DigiDollarWallet() = default;

    // Set wallet pointer (for initialization)
    void SetWallet(wallet::CWallet* wallet) { m_wallet = wallet; }

    // ====================================================================
    // DD OWNER KEY MANAGEMENT (for Taproot/Descriptor wallet compatibility)
    // ====================================================================

    /**
     * Store DD owner key for a time-lock position
     * @param dd_timelock_id The time-lock position ID (mint tx hash)
     * @param key The owner private key
     */
    void StoreOwnerKey(const uint256& dd_timelock_id, const CKey& key) {
        dd_owner_keys[dd_timelock_id] = key;
    }

    /**
     * Retrieve DD owner key for a time-lock position
     * @param dd_timelock_id The time-lock position ID
     * @param key Output parameter for the key
     * @return true if key found
     */
    bool GetOwnerKey(const uint256& dd_timelock_id, CKey& key) const {
        auto it = dd_owner_keys.find(dd_timelock_id);
        if (it == dd_owner_keys.end()) return false;
        key = it->second;
        return true;
    }

    // ====================================================================
    // PHASE 5 TASK 5.1: DATABASE EXTENSION FUNCTIONS
    // ====================================================================

    /**
     * Write DD balance to wallet database
     * @param addr DD address
     * @param balance Balance amount in cents
     * @return true if write successful
     */
    bool WriteDDBalance(const CDigiDollarAddress& addr, const CAmount& balance);

    /**
     * Write DDTimeLock (time-locked DGB backing DigiDollars) to wallet database
     * @param position DDTimeLock data (collateral position)
     * @return true if write successful
     */
    bool WriteDDTimeLock(const WalletCollateralPosition& position);

    /**
     * Update position active status
     * @param dd_timelock_id Position identifier
     * @param active New active status
     * @return true if update successful
     */
    bool UpdatePositionStatus(const uint256& dd_timelock_id, bool active);

    // ====================================================================
    // PHASE 5 TASK 5.2: BALANCE TRACKING FUNCTIONS
    // ====================================================================

    /**
     * Get DD balance for specific address or total if no address provided
     * @param addr DD address (empty for total balance)
     * @return DD balance in cents
     */
    CAmount GetDDBalance(const CDigiDollarAddress& addr = CDigiDollarAddress()) const;

    /**
     * Get total DD balance across all addresses
     * @return Total DD balance in cents
     */
    CAmount GetTotalDDBalance() const;

    /**
     * Scan wallet UTXOs for DigiDollar outputs and populate dd_balances map
     * Should be called after wallet loads and when new blocks are processed
     * @return Number of DD UTXOs found
     */
    size_t ScanForDDUTXOs();

    /**
     * Get total locked collateral from active positions
     * @return Locked collateral amount in satoshis
     */
    CAmount GetLockedCollateral() const;

    /**
     * Get list of DigiDollar Time-Locked DGB vaults (DDTimeLocks)
     * These are Time-Locked DGB backing DigiDollar value
     * @param active_only Only return active time locks if true
     * @return Vector of DDTimeLock positions
     */
    std::vector<WalletCollateralPosition> GetDDTimeLocks(bool active_only = true) const;

    /**
     * Get all spendable DigiDollar UTXOs from active DDTimeLocks
     * @return Vector of DD UTXOs (output index 1 of each DDTimeLock)
     */
    std::vector<DDUtxo> GetDDUTXOs() const;

    /**
     * Get DD amount from UTXO using DDTimeLock cache
     * @param outpoint UTXO outpoint (should be dd_timelock_id with n=1)
     * @return DD amount in cents, or 0 if UTXO not found or invalid
     */
    CAmount GetDDFromUTXO(const COutPoint& outpoint) const;

    /**
     * Add DD UTXO to tracking map (FIX #1)
     * @param outpoint UTXO outpoint (txid, vout)
     * @param dd_amount DD amount in cents
     */
    void AddDDUTXO(const COutPoint& outpoint, CAmount dd_amount) {
        dd_utxos[outpoint] = dd_amount;
    }

    /**
     * Remove DD UTXO from tracking map when spent (FIX #1)
     * @param outpoint UTXO outpoint to remove
     */
    void RemoveDDUTXO(const COutPoint& outpoint) {
        dd_utxos.erase(outpoint);
    }

    /**
     * Add a collateral position to the wallet
     * @param position The position to add
     */
    void AddCollateralPosition(const WalletCollateralPosition& position);

    // ====================================================================
    // PHASE 5 TASK 5.3: TRANSACTION CREATION FUNCTIONS
    // ====================================================================

    /**
     * Create mint transaction using transaction builders
     * @param dd_amount Amount of DD to mint (in cents)
     * @param lock_tier Lock tier (1-8)
     * @param tx_out Output transaction reference
     * @return true if transaction created successfully
     */
    bool MintDigiDollar(const CAmount& dd_amount, uint32_t lock_tier, CTransactionRef& tx_out);

    /**
     * Create transfer transaction using transaction builders
     * @param to Recipient DD address
     * @param amount Amount to transfer in cents
     * @param tx_out Output transaction reference
     * @return true if transaction created successfully
     */
    bool TransferDigiDollar(const CDigiDollarAddress& to, CAmount amount, CTransactionRef& tx_out);

    /**
     * Create redemption transaction using transaction builders
     * @param dd_timelock_id Position to redeem
     * @param amount Amount of DD to redeem
     * @param tx_out Output transaction reference
     * @return true if transaction created successfully
     */
    bool RedeemDigiDollar(const uint256& dd_timelock_id, const CAmount& amount, CTransactionRef& tx_out);

    /**
     * Transfer DigiDollars to another address
     * @param to Recipient DD address
     * @param amount Amount to transfer in cents
     * @param txid Output parameter for transaction ID
     * @param error Output parameter for error message
     * @return true if transfer successful, false otherwise
     */
    bool TransferDigiDollar(const CDigiDollarAddress& to, CAmount amount,
                          std::string& txid, std::string& error);

    /**
     * Get current DD balance (legacy method)
     * @return DD balance in cents
     */
    CAmount GetDDBalanceLegacy() const;

    /**
     * Get DD transaction history
     * @return Vector of DD transactions
     */
    std::vector<DDTransaction> GetDDTransactionHistory() const;

    /**
     * Validate DD address format
     * @param address DD address string to validate
     * @return true if valid, false otherwise
     */
    bool ValidateDDAddress(const std::string& address) const;

    // ====================================================================
    // Redemption Functions (Task 3.9)
    // ====================================================================

    /**
     * Redeem DigiDollars and unlock collateral
     * @param collateralUtxo The collateral UTXO to unlock
     * @param ddAmount Amount of DD to redeem and burn
     * @param path Redemption path to use
     * @param txid Output parameter for transaction ID
     * @param error Output parameter for error message
     * @return true if redemption successful, false otherwise
     */
    bool RedeemDigiDollar(const COutPoint& collateralUtxo,
                         CAmount ddAmount,
                         DigiDollar::RedemptionPath path,
                         std::string& txid,
                         std::string& error);

    /**
     * Get list of redeemable positions
     * @return Vector of positions that can be redeemed
     */
    std::vector<DigiDollar::RedeemablePosition> GetRedeemablePositions() const;

    /**
     * Calculate redemption value for a position
     * @param position Position to calculate value for
     * @return Estimated DGB return amount
     */
    CAmount CalculateRedemptionValue(const COutPoint& position) const;

    /**
     * Check if position can be redeemed
     * @param position Position to check
     * @param availablePath Output parameter for best available path
     * @return true if position can be redeemed
     */
    bool CanRedeem(const COutPoint& position, DigiDollar::RedemptionPath& availablePath) const;

    /**
     * Get redemption transaction history
     * @return Vector of redemption transactions
     */
    std::vector<DDTransaction> GetRedemptionHistory() const;

    /**
     * Estimate redemption transaction fees
     * @param position Position to redeem
     * @param path Redemption path to use
     * @return Estimated fee amount in satoshis
     */
    CAmount EstimateRedemptionFee(const COutPoint& position, DigiDollar::RedemptionPath path) const;

    /**
     * Get current DGB balance (for fee payments)
     * @return DGB balance in satoshis
     */
    CAmount GetDGBBalance() const;

    // ====================================================================
    // PHASE 5.2: UTXO SET UPDATE FUNCTIONS
    // ====================================================================

    /**
     * Mark DD UTXOs as spent after transaction committed
     * @param spent_utxos Vector of UTXOs that were consumed as inputs
     * @return true if UTXOs marked successfully
     */
    bool MarkDDUTXOsSpent(const std::vector<COutPoint>& spent_utxos);

    /**
     * Add new DD UTXO from change output
     * @param tx The committed transaction
     * @param change_vout Index of change output in transaction
     * @param dd_amount DD amount in change output
     * @return true if UTXO added successfully
     */
    bool AddDDChangeUTXO(const CTransactionRef& tx, uint32_t change_vout, CAmount dd_amount);

    /**
     * Complete UTXO set update after transfer
     * Marks inputs spent, adds change output
     * @param tx The committed transaction
     * @param input_utxos DD UTXOs used as inputs
     * @param change_vout Index of change output (-1 if no change)
     * @param change_amount DD amount in change output
     * @return true if update successful
     */
    bool UpdateDDUTXOSet(const CTransactionRef& tx,
                         const std::vector<COutPoint>& input_utxos,
                         int change_vout,
                         CAmount change_amount);

    // ====================================================================
    // PHASE 5.3: DDTIMELOCK STATUS MANAGEMENT FUNCTIONS
    // ====================================================================

    /**
     * Update DDTimeLock status after DD transfer or redemption
     * @param dd_timelock_id The DDTimeLock position ID
     * @param new_status New active status (true = active, false = fully redeemed)
     * @return true if status updated successfully
     */
    bool UpdateDDTimeLockStatus(const uint256& dd_timelock_id, bool new_status);

    /**
     * Track partial DD redemption from a DDTimeLock
     * @param dd_timelock_id The DDTimeLock position ID
     * @param dd_redeemed Amount of DD redeemed
     * @return true if partial redemption tracked successfully
     */
    bool TrackPartialRedemption(const uint256& dd_timelock_id, CAmount dd_redeemed);

    /**
     * Get DDTimeLock lifecycle status
     * @param dd_timelock_id The DDTimeLock position ID
     * @return Status string: "active", "partially_redeemed", "fully_redeemed", "not_found"
     */
    std::string GetDDTimeLockStatus(const uint256& dd_timelock_id) const;

    /**
     * Check if DDTimeLock is redeemable (unlocked and has DD remaining)
     * @param dd_timelock_id The DDTimeLock position ID
     * @param current_height Current blockchain height
     * @return true if redeemable
     */
    bool IsDDTimeLockRedeemable(const uint256& dd_timelock_id, int current_height) const;

    // Test/mock functions
    void SetMockBalance(CAmount balance) { mockBalance = balance; }
    void AddMockTransaction(const DDTransaction& tx) { mockHistory.push_back(tx); }
    void AddMockUTXO(const CDigiDollarOutput& utxo) { mockUTXOs.push_back(utxo); }
    void ClearMockData() { mockBalance = 0; mockHistory.clear(); mockUTXOs.clear(); }

    // Phase 5 test helpers
    void SetMockDDBalance(const CDigiDollarAddress& addr, CAmount balance);
    void AddMockPosition(const uint256& id, CAmount dd, CAmount dgb, uint32_t tier, int64_t height);
    size_t GetBalanceCount() const { return dd_balances.size(); }
    size_t GetPositionCount() const { return collateral_positions.size(); }
    void ClearWalletData();

    // Coin selection and fee calculation helpers (public for testing and integration)
    bool SelectDDCoins(const CAmount& target_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total) const;
    bool SelectFeeCoins(const CAmount& fee_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total, std::vector<CAmount>* selected_amounts = nullptr) const;
    CAmount CalculateTransactionFee(const CMutableTransaction& tx) const;

    // Phase 3.1: P2TR signing for DD inputs (Schnorr signatures)
    bool SignDDInputs(CMutableTransaction& tx, const std::vector<COutPoint>& dd_utxos);

    // Phase 3.2: Fee input signing (standard P2TR/P2WPKH DGB inputs)
    bool SignFeeInputs(CMutableTransaction& tx,
                       const std::vector<COutPoint>& fee_utxos,
                       size_t dd_input_count);

    // Phase 3.3: Complete transaction signing coordination
    bool SignTransaction(CMutableTransaction& tx,
                        const std::vector<COutPoint>& dd_utxos,
                        const std::vector<COutPoint>& fee_utxos);

    // Phase 4.1: Mempool submission
    bool CommitDDTransaction(const CTransactionRef& tx, std::string& error);

    // Phase 4.3: Confirmation tracking
    /**
     * Get confirmation count for DD transaction
     * @param txid Transaction ID to check
     * @return Number of confirmations (0 if unconfirmed or not found)
     */
    int GetDDTransactionConfirmations(const uint256& txid) const;

    /**
     * Update confirmation counts for all DD transactions when new block arrives
     * @param block_hash Hash of the newly connected block
     */
    void UpdateDDConfirmations(const uint256& block_hash);

    /**
     * Get all unconfirmed DD transactions (0 confirmations)
     * @return Vector of transaction IDs with 0 confirmations
     */
    std::vector<uint256> GetUnconfirmedDDTransactions() const;

    // ====================================================================
    // PHASE 6: RECEIVE OPERATIONS (Tasks 6.1-6.3)
    // ====================================================================

    /**
     * Detect if transaction has DD outputs to our wallet (Task 6.1)
     * Checks each transaction output to see if it's a DD output owned by this wallet
     * @param tx Transaction to check
     * @param our_dd_outputs Output: Vector of (vout_index, dd_amount) for our outputs
     * @return true if any outputs are ours
     */
    bool DetectIncomingDDOutputs(const CTransactionRef& tx,
                                 std::vector<std::pair<uint32_t, CAmount>>& our_dd_outputs);

    /**
     * Add received DD UTXO to spendable set (Task 6.3)
     * Creates a WalletCollateralPosition for received DD (not from our mint)
     * Similar to AddDDChangeUTXO() but for DD received from others
     * @param tx The transaction containing the DD output
     * @param vout_index Index of DD output we received
     * @param dd_amount DD amount received
     * @return true if UTXO added successfully
     */
    bool AddReceivedDDUTXO(const CTransactionRef& tx, uint32_t vout_index, CAmount dd_amount);

    /**
     * Process incoming DD transaction (Tasks 6.1-6.3 combined)
     * Main coordinator for receiving DD:
     * 1. Detects DD outputs to our wallet
     * 2. Credits balance (automatic via UTXO-derived approach)
     * 3. Adds received UTXOs to spendable set
     * @param tx The incoming transaction
     * @return true if processing successful
     */
    bool ProcessIncomingDDTransaction(const CTransactionRef& tx);

protected:
    // Internal helper functions for Phase 5
    bool ValidateMintParams(const CAmount& dd_amount, uint32_t lock_tier) const;
    bool ValidateTransferParams(const CDigiDollarAddress& to, const CAmount& amount) const;
    bool ValidateRedeemParams(const uint256& dd_timelock_id, const CAmount& amount) const;

private:
    /**
     * Helper: Load all positions from database
     */
    size_t LoadPositionsFromDatabase();

    /**
     * Helper: Load all DD balances from database
     */
    size_t LoadBalancesFromDatabase();

    /**
     * Helper: Load all DD transactions from database
     */
    size_t LoadTransactionsFromDatabase();

    /**
     * Recalculate totals from loaded data
     */
    void RecalculateTotals();

public:
    /**
     * Load all DigiDollar data from wallet database
     * Called during wallet initialization
     * @return Number of items loaded
     */
    size_t LoadFromDatabase();
};

/**
 * Utility functions for DigiDollar wallet operations
 */
namespace DigiDollarWalletUtils {
    /**
     * Convert lock tier to lock period in days
     * @param tier Lock tier (1-8)
     * @return Lock period in days
     */
    int GetLockDaysForTier(uint32_t tier);

    /**
     * Calculate minimum collateral ratio for lock tier
     * @param tier Lock tier (1-8)
     * @return Collateral ratio percentage
     */
    int GetMinCollateralRatio(uint32_t tier);

    /**
     * Validate DD address format
     * @param address DD address string
     * @return true if valid format
     */
    bool IsValidDDAddress(const std::string& address);
}

#endif // DIGIBYTE_WALLET_DIGIDOLLARWALLET_H