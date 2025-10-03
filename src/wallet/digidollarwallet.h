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
    uint256 position_id;  // txid of mint tx
    CAmount dd_minted;
    CAmount dgb_collateral;
    uint32_t lock_tier;
    int64_t unlock_height;
    bool is_active;

    WalletCollateralPosition()
        : dd_minted(0), dgb_collateral(0), lock_tier(0), unlock_height(0), is_active(false) {}
    WalletCollateralPosition(const uint256& id, CAmount dd, CAmount dgb, uint32_t tier, int64_t height)
        : position_id(id), dd_minted(dd), dgb_collateral(dgb), lock_tier(tier), unlock_height(height), is_active(true) {}

    SERIALIZE_METHODS(WalletCollateralPosition, obj)
    {
        READWRITE(obj.position_id);
        READWRITE(obj.dd_minted);
        READWRITE(obj.dgb_collateral);
        READWRITE(obj.lock_tier);
        READWRITE(obj.unlock_height);
        READWRITE(obj.is_active);
    }
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

    // Internal state tracking (Task 5.2)
    CAmount total_dd_balance;
    CAmount locked_collateral;

    // Pointer to wallet for UTXO access
    wallet::CWallet* m_wallet;

public:
    DigiDollarWallet();
    DigiDollarWallet(wallet::CWallet* wallet);
    virtual ~DigiDollarWallet() = default;

    // Set wallet pointer (for initialization)
    void SetWallet(wallet::CWallet* wallet) { m_wallet = wallet; }

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
     * Write collateral position to wallet database
     * @param position Collateral position data
     * @return true if write successful
     */
    bool WritePosition(const WalletCollateralPosition& position);

    /**
     * Update position active status
     * @param position_id Position identifier
     * @param active New active status
     * @return true if update successful
     */
    bool UpdatePositionStatus(const uint256& position_id, bool active);

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
     * Get list of collateral positions
     * @param active_only Only return active positions if true
     * @return Vector of collateral positions
     */
    std::vector<WalletCollateralPosition> GetPositions(bool active_only = true) const;

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
     * @param position_id Position to redeem
     * @param amount Amount of DD to redeem
     * @param tx_out Output transaction reference
     * @return true if transaction created successfully
     */
    bool RedeemDigiDollar(const uint256& position_id, const CAmount& amount, CTransactionRef& tx_out);

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

protected:
    // Internal helper functions for Phase 5
    bool ValidateMintParams(const CAmount& dd_amount, uint32_t lock_tier) const;
    bool ValidateTransferParams(const CDigiDollarAddress& to, const CAmount& amount) const;
    bool ValidateRedeemParams(const uint256& position_id, const CAmount& amount) const;

    // Coin selection and fee calculation helpers
    bool SelectDDCoins(const CAmount& target_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total) const;
    bool SelectFeeCoins(const CAmount& fee_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total) const;
    CAmount CalculateTransactionFee(const CMutableTransaction& tx) const;

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