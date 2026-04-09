// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <digidollar/health.h>
#include <digidollar/digidollar.h>
#include <digidollar/validation.h>
#include <consensus/digidollar.h>
#include <consensus/volatility.h>
#include <consensus/dca.h>
#include <consensus/err.h>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <core_io.h>
#include <logging.h>
#include <node/context.h>
#include <node/transaction.h>
#include <txmempool.h>
#include <util/time.h>

#include <limits>
#include <util/moneystr.h>
#include <validation.h>
#include <node/blockstorage.h>
#include <txdb.h>
#include <wallet/wallet.h>
#include <wallet/digidollarwallet.h>

#include <algorithm>
#include <memory>

namespace DigiDollar {

// Static member definitions
SystemMetrics SystemHealthMonitor::s_currentMetrics;
std::mutex SystemHealthMonitor::s_metricsMutex;
std::map<int64_t, int> SystemHealthMonitor::s_healthHistory;
std::mutex SystemHealthMonitor::s_historyMutex;
bool SystemHealthMonitor::s_initialized = false;

// Standard tier definitions (lock days)
// NOTE: Tier 0 uses 0 to represent 1 hour (240 blocks) - special case handled by LockDaysToBlocks()
static const std::vector<int> TIER_LOCK_DAYS = {0, 30, 90, 180, 365, 730, 1095, 1825, 2555, 3650}; // 1h, 30d to 10y (10 tiers)

SystemMetrics SystemHealthMonitor::GetSystemMetrics()
{
    if (!s_initialized) {
        Initialize();
    }

    // Update current metrics by scanning UTXO set
    // Note: Internal calls don't have chainstate access, metrics updated on-demand
    // ScanUTXOSet(nullptr);
    UpdateTierMetrics();
    UpdateProtectionStatus();
    UpdateOracleStatus();

    return s_currentMetrics;
}

std::vector<SystemMetrics::TierMetrics> SystemHealthMonitor::GetTierBreakdown()
{
    if (!s_initialized) {
        Initialize();
    }

    // Ensure metrics are current
    // Note: UTXO scan happens on-demand from RPC
    // ScanUTXOSet(nullptr);
    UpdateTierMetrics();

    return s_currentMetrics.tiers;
}

bool SystemHealthMonitor::ShouldAlert(const std::string& metric)
{
    if (!s_initialized) {
        Initialize();
    }

    SystemMetrics metrics = GetSystemMetrics();

    if (metric == "system_health") {
        return CheckHealthAlert(metrics);
    } else if (metric == "total_supply") {
        return CheckSupplyAlert(metrics);
    } else if (metric == "total_collateral") {
        return CheckCollateralAlert(metrics);
    } else if (metric == "oracle_status") {
        return CheckOracleAlert(metrics);
    } else if (metric == "volatility") {
        return CheckVolatilityAlert(metrics);
    } else if (metric == "position_count") {
        return CheckPositionAlert(metrics);
    }

    // Unknown metric
    return false;
}

std::vector<int> SystemHealthMonitor::GetHealthHistory(int blocks)
{
    if (!s_initialized) {
        Initialize();
    }

    std::vector<int> history;
    if (blocks <= 0) {
        return history;
    }

    // Cap the maximum history size to prevent excessive memory allocation
    const int MAX_HISTORY_REQUEST = 100000;
    int blocksToFetch = std::min(blocks, MAX_HISTORY_REQUEST);

    // Get current chain tip
    // TODO: Fix chainstate access - temporary mock implementation
    // In a production system, this should receive a ChainstateManager reference
    const CBlockIndex* tip = nullptr;

    // For now, use mock data to prevent compilation errors
    // This should be replaced with proper chainstate access
    int64_t currentHeight = 1000000; // Mock current height

    // Collect history from most recent to oldest
    for (int i = 0; i < blocksToFetch && (currentHeight - i) >= 0; ++i) {
        int64_t height = currentHeight - i;
        auto it = s_healthHistory.find(height);
        if (it != s_healthHistory.end()) {
            history.push_back(it->second);
        } else {
            // If no recorded data, use current health as estimate
            history.push_back(s_currentMetrics.systemHealth);
        }
    }

    return history;
}

void SystemHealthMonitor::UpdateMetrics(const CBlock& block)
{
    if (!s_initialized) {
        Initialize();
    }

    // Update metrics with new block data
    // Note: UTXO scan requires chainstate access from caller
    // ScanUTXOSet(nullptr);
    UpdateTierMetrics();
    UpdateProtectionStatus();
    UpdateOracleStatus();

    // RH-36a: First real health update from block processing — unlock
    // ERR state so GetCurrentState() can read DCA cache again.
    DigiDollar::ERR::EmergencyRedemptionRatio::ClearStateReconstructed();

    // Record health history
    // TODO: Fix chainstate access - temporary mock implementation
    // For now, use mock height to prevent compilation errors
    int64_t mockHeight = 1000000; // This should be replaced with proper chainstate access
    RecordHealthHistory(mockHeight, s_currentMetrics.systemHealth);

    LogPrint(BCLog::DIGIDOLLAR, "DigiDollar health updated: %d%% health, %s DD supply, %s DGB collateral\n",
             s_currentMetrics.systemHealth,
             FormatMoney(s_currentMetrics.totalDDSupply),
             FormatMoney(s_currentMetrics.totalCollateral));
}

UniValue SystemHealthMonitor::GetHealthReport()
{
    if (!s_initialized) {
        Initialize();
    }

    SystemMetrics metrics = GetSystemMetrics();

    UniValue result(UniValue::VOBJ);

    // Overall system metrics
    result.pushKV("supply", ValueFromAmount(metrics.totalDDSupply));
    result.pushKV("collateral", ValueFromAmount(metrics.totalCollateral));
    result.pushKV("health", metrics.systemHealth);

    // Protection system status
    result.pushKV("dca_multiplier", metrics.dcaMultiplier);
    result.pushKV("err_active", metrics.errActive);
    result.pushKV("volatility", metrics.volatility);
    result.pushKV("minting_frozen", metrics.mintingFrozen);

    // Tier breakdown
    UniValue tiers(UniValue::VARR);
    for (const auto& tier : metrics.tiers) {
        UniValue t(UniValue::VOBJ);
        t.pushKV("lock_days", tier.lockDays);
        t.pushKV("dd_minted", ValueFromAmount(tier.ddMinted));
        t.pushKV("dgb_locked", ValueFromAmount(tier.dgbLocked));
        t.pushKV("positions", tier.positions);
        t.pushKV("health", tier.healthRatio);
        t.pushKV("status", HealthUtils::FormatHealthStatus(tier.healthRatio));
        t.pushKV("action", HealthUtils::GetRecommendedAction(tier.healthRatio));
        tiers.push_back(t);
    }
    result.pushKV("tiers", tiers);

    // Oracle status
    UniValue oracles(UniValue::VOBJ);
    oracles.pushKV("active_count", metrics.activeOracles);
    oracles.pushKV("last_price", ValueFromAmount(metrics.lastOraclePrice));
    oracles.pushKV("last_update", metrics.lastOracleUpdate);

    // TODO: Fix chainstate access - temporary mock implementation
    // For now, use mock height to prevent compilation errors
    int64_t mockHeight = 1000000; // This should be replaced with proper chainstate access
    oracles.pushKV("blocks_since_update", mockHeight - metrics.lastOracleUpdate);
    oracles.pushKV("is_stale", (mockHeight - metrics.lastOracleUpdate) > AlertThresholds::STALE_ORACLE_BLOCKS);
    result.pushKV("oracles", oracles);

    // Alert summary
    UniValue alerts(UniValue::VARR);
    if (ShouldAlert("system_health")) alerts.push_back("system_health");
    if (ShouldAlert("total_supply")) alerts.push_back("total_supply");
    if (ShouldAlert("total_collateral")) alerts.push_back("total_collateral");
    if (ShouldAlert("oracle_status")) alerts.push_back("oracle_status");
    if (ShouldAlert("volatility")) alerts.push_back("volatility");
    if (ShouldAlert("position_count")) alerts.push_back("position_count");
    result.pushKV("active_alerts", alerts);

    // System recommendations
    result.pushKV("overall_status", HealthUtils::FormatHealthStatus(metrics.systemHealth));
    result.pushKV("recommended_action", HealthUtils::GetRecommendedAction(metrics.systemHealth));

    return result;
}

void SystemHealthMonitor::Initialize()
{
    if (s_initialized) {
        LogPrint(BCLog::DIGIDOLLAR, "Initialize: Already initialized, skipping\n");
        return;
    }

    LogPrint(BCLog::DIGIDOLLAR, "Initialize: Initializing DigiDollar health monitoring system (totalDDSupply=%lld)\n",
             static_cast<long long>(s_currentMetrics.totalDDSupply));

    // Don't reset metrics if they've already been populated by ScanUTXOSet
    if (s_currentMetrics.totalDDSupply == 0 && s_currentMetrics.totalCollateral == 0) {
        LogPrint(BCLog::DIGIDOLLAR, "Initialize: No existing data, initializing fresh metrics structure\n");
        // Initialize metrics structure
        s_currentMetrics = SystemMetrics();

        // Initialize tier breakdown
        s_currentMetrics.tiers.clear();
        for (int lockDays : TIER_LOCK_DAYS) {
            s_currentMetrics.tiers.emplace_back(lockDays, 0, 0, 0, 0);
        }
    } else {
        LogPrint(BCLog::DIGIDOLLAR, "Initialize: Metrics already populated (totalDDSupply=%lld, totalCollateral=%lld), preserving data\n",
                 static_cast<long long>(s_currentMetrics.totalDDSupply), static_cast<long long>(s_currentMetrics.totalCollateral));
        // Just ensure tiers are initialized if empty
        if (s_currentMetrics.tiers.empty()) {
            for (int lockDays : TIER_LOCK_DAYS) {
                s_currentMetrics.tiers.emplace_back(lockDays, 0, 0, 0, 0);
            }
        }
    }

    // Clear health history
    s_healthHistory.clear();

    // Perform initial scan
    // Note: UTXO scan happens on-demand from RPC with chainstate access
    // ScanUTXOSet(nullptr);
    UpdateTierMetrics();
    UpdateProtectionStatus();
    UpdateOracleStatus();

    s_initialized = true;

    LogPrint(BCLog::DIGIDOLLAR, "DigiDollar health monitoring initialized: %d%% initial health\n",
             s_currentMetrics.systemHealth);
}

void SystemHealthMonitor::Shutdown()
{
    if (!s_initialized) {
        return;
    }

    LogPrint(BCLog::DIGIDOLLAR, "Shutting down DigiDollar health monitoring system\n");

    // Clear data structures
    s_currentMetrics = SystemMetrics();
    s_healthHistory.clear();

    s_initialized = false;
}

void SystemHealthMonitor::ScanUTXOSet(CCoinsView* view, CCoinsView* validation_view, const node::BlockManager* blockman, const CTxMemPool* mempool)
{
    // Reset counters
    s_currentMetrics.totalDDSupply = 0;
    s_currentMetrics.totalCollateral = 0;

    // Reset tier counters
    for (auto& tier : s_currentMetrics.tiers) {
        tier.ddMinted = 0;
        tier.dgbLocked = 0;
        tier.positions = 0;
        tier.healthRatio = 0;
    }

    if (!view) {
        LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: No coins view provided\n");
        return;
    }

    if (!blockman) {
        LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: No block manager provided - skipping UTXO scan (unit test mode?)\n");
        return;
    }

    // Create cursor to iterate all UTXOs (similar to gettxoutsetinfo)
    std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());
    if (!pcursor) {
        LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Unable to create UTXO cursor\n");
        return;
    }

    LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Starting blockchain-wide UTXO scan with full transaction access\n");
    LogPrintf("DigiDollar: ========== STARTING UTXO SCAN ==========\n");

    size_t vaults_found = 0;
    size_t dd_amount_extracted = 0;
    size_t dd_amount_estimated = 0;
    size_t utxos_scanned = 0;
    size_t output0_checked = 0;
    size_t p2tr_found = 0;

    // Track which transactions we've seen to avoid double-counting
    std::set<uint256> processed_txids;

    // Iterate through ALL UTXOs in the blockchain
    while (pcursor->Valid()) {
        COutPoint key;
        Coin coin;

        if (!pcursor->GetKey(key) || !pcursor->GetValue(coin)) {
            LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Error reading UTXO\n");
            break;
        }

        const uint256& txid = key.hash;

        //  Skip spent coins - they are marked for deletion but haven't been pruned yet
        if (coin.IsSpent()) {
            pcursor->Next();
            continue;
        }

        utxos_scanned++;

        // Check if this UTXO is part of a DD transaction we haven't processed
        if (processed_txids.find(txid) == processed_txids.end()) {
            // Check output 0
            if (key.n == 0) {
                output0_checked++;
                // Note: Removed per-UTXO logging - too verbose for networks with many UTXOs

                // Check if output 0 is a P2TR collateral output (vaults are 34 bytes starting with OP_1)
                // DigiDollar minting creates native P2TR (Taproot) outputs for collateral vaults
                if (coin.out.scriptPubKey.size() == 34 &&
                    coin.out.scriptPubKey[0] == OP_1 && coin.out.nValue > 0) {
                    p2tr_found++;  // Found potential P2TR vault
                    LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Found P2TR output 0 with value, fetching full transaction...\n");

                    // CRITICAL: Before processing, validate this UTXO still exists in current chainstate
                    // CoinsDB may contain spent-but-not-pruned coins
                    if (validation_view) {
                        Coin validation_coin;
                        if (!validation_view->GetCoin(key, validation_coin) || validation_coin.IsSpent()) {
                            LogPrintf("DigiDollar: Skipping spent DD vault: %s:%d\n", txid.ToString(), key.n);
                            // Skip this output - mark txid as processed to avoid checking other outputs
                            processed_txids.insert(txid);
                            // Continue to next UTXO (main loop will call pcursor->Next())
                            break; // Break out of the if(key.n == 0) block
                        }
                    }

                    // This looks like a DD vault (output 0 of mint tx)
                    // Now fetch the full transaction to check for OP_RETURN and extract DD amount

                    CAmount collateral = coin.out.nValue;
                    CAmount ddAmount = 0;
                    bool exactAmount = false;

                    // Get the full transaction from block storage
                    uint256 hashBlock;
                    CTransactionRef tx = node::GetTransaction(nullptr, mempool, txid, hashBlock, *blockman);

                    if (tx) {
                        // Check if this is actually a DigiDollar mint transaction
                        // DD mint structure:
                        // - Output 0: P2TR collateral vault (has value > 0, 34 bytes, OP_1)
                        // - Output 1: P2TR DD token (value = 0, 34 bytes, OP_1, simple key-path)
                        // - Output 2: OP_RETURN with DD metadata (contains exact DD amount)

                        bool isValidDDMint = false;

                        // Find the DD OP_RETURN output (searches all vouts)
                        int ddOpReturnIdx = DigiDollar::FindDDOpReturn(*tx);
                        if (ddOpReturnIdx >= 0) {
                            const CScript& ddScript = tx->vout[ddOpReturnIdx].scriptPubKey;

                            // Log the OP_RETURN script for debugging
                            LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Found OP_RETURN for tx %s at vout[%d], size=%d, first bytes: %02x %02x %02x %02x\n",
                                     txid.ToString(), ddOpReturnIdx, ddScript.size(),
                                     ddScript.size() > 0 ? ddScript[0] : 0,
                                     ddScript.size() > 1 ? ddScript[1] : 0,
                                     ddScript.size() > 2 ? ddScript[2] : 0,
                                     ddScript.size() > 3 ? ddScript[3] : 0);

                            // CRITICAL: Check txType to ensure this is a MINT (1), not REDEEM (3) or TRANSFER (2)
                            DigiDollarTxType txType = DigiDollar::GetDigiDollarTxType(*tx);
                            if (txType != DD_TX_MINT) {
                                LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Skipping tx %s - txType=%d (not MINT)\n",
                                         txid.ToString(), static_cast<int>(txType));
                                processed_txids.insert(txid);
                                pcursor->Next();
                                continue;
                            }

                            // Try to extract DD amount from OP_RETURN
                            if (DigiDollar::ExtractDDAmount(ddScript, ddAmount)) {
                                isValidDDMint = true;
                                exactAmount = true;
                                dd_amount_extracted++;
                                LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Extracted exact DD amount %s from tx %s\n",
                                         FormatMoney(ddAmount), txid.ToString());
                            } else {
                                LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: FAILED to extract DD amount from tx %s OP_RETURN\n",
                                         txid.ToString());
                            }
                        }

                        if (!isValidDDMint) {
                            // Not a valid DD mint - skip this UTXO
                            processed_txids.insert(txid);
                            pcursor->Next();
                            continue;
                        }
                    } else {
                        // Could not fetch transaction - fall back to estimation
                        LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Could not fetch tx %s, using estimation\n", txid.ToString());

                        // Estimate DD supply from collateral
                        // Using oracle price to estimate
                        CAmount oraclePrice = GetLastOraclePrice();
                        if (oraclePrice == 0) {
                            oraclePrice = 50; // Default $0.50 per DGB (50 cents)
                        }
                        // Oracle price is in cents (100 = $1.00)
                        CAmount collateralValue = (collateral * oraclePrice) / COIN; // in cents
                        ddAmount = (collateralValue * 100) / 150; // Reverse 150% ratio (conservative estimate)
                        dd_amount_estimated++;
                    }

                    // Add to totals
                    s_currentMetrics.totalCollateral += collateral;
                    s_currentMetrics.totalDDSupply += ddAmount;
                    vaults_found++;
                    processed_txids.insert(txid);

                    LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Found DD vault - collateral=%s, DD=%s (%s)\n",
                             FormatMoney(collateral), FormatMoney(ddAmount),
                             exactAmount ? "exact" : "estimated");

                    // ALWAYS log vault findings (not just BCLog::DIGIDOLLAR)
                    LogPrintf("DigiDollar: UTXO Scanner found vault: %s:0 - Collateral=%s DGB, DD=%s cents\n",
                             txid.ToString(), FormatMoney(collateral), FormatMoney(ddAmount));
                }
            }
        }

        pcursor->Next();
    }

    LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Scan complete - %d UTXOs scanned, %d output0s checked, %d P2TR found\n",
             utxos_scanned, output0_checked, p2tr_found);
    LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Results - Found %d vaults, %s DGB collateral, %s DD supply\n",
             vaults_found, FormatMoney(s_currentMetrics.totalCollateral),
             FormatMoney(s_currentMetrics.totalDDSupply));
    LogPrint(BCLog::DIGIDOLLAR, "ScanUTXOSet: Exact amounts: %d, Estimated amounts: %d\n",
             dd_amount_extracted, dd_amount_estimated);

    LogPrintf("DigiDollar: ========== UTXO SCAN COMPLETE ==========\n");
    LogPrintf("DigiDollar: Found %zu vaults, Total Collateral: %s DGB, Total DD: %s cents\n",
             vaults_found, FormatMoney(s_currentMetrics.totalCollateral),
             FormatMoney(s_currentMetrics.totalDDSupply));
}

// ============================================================================
// Incremental metrics tracking (T5-06)
// Called from ConnectBlock/DisconnectBlock under cs_main.
// ============================================================================

void SystemHealthMonitor::OnMintConnected(CAmount ddAmount, CAmount dgbCollateral)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex); // RH-44: thread safety
    // SECURITY [RH-11]: Prevent supply overflow — cap at MAX_DIGIDOLLAR
    if (ddAmount > 0 && s_currentMetrics.totalDDSupply <= MAX_DIGIDOLLAR - ddAmount) {
        s_currentMetrics.totalDDSupply += ddAmount;
    } else if (ddAmount > 0) {
        LogPrintf("Health: WARNING - totalDDSupply would exceed MAX_DIGIDOLLAR, capping at %s\n",
                 FormatMoney(MAX_DIGIDOLLAR));
        s_currentMetrics.totalDDSupply = MAX_DIGIDOLLAR;
    }
    // Cap collateral at MAX_MONEY to prevent int64_t overflow
    if (dgbCollateral > 0 && s_currentMetrics.totalCollateral <= std::numeric_limits<CAmount>::max() - dgbCollateral) {
        s_currentMetrics.totalCollateral += dgbCollateral;
    } else if (dgbCollateral > 0) {
        LogPrintf("Health: WARNING - totalCollateral would overflow, capping\n");
        s_currentMetrics.totalCollateral = std::numeric_limits<CAmount>::max();
    }
    LogPrint(BCLog::DIGIDOLLAR, "Health: Mint connected - DD +%s, Collateral +%s (totals: DD=%s, Collateral=%s)\n",
             FormatMoney(ddAmount), FormatMoney(dgbCollateral),
             FormatMoney(s_currentMetrics.totalDDSupply), FormatMoney(s_currentMetrics.totalCollateral));
}

void SystemHealthMonitor::OnRedeemConnected(CAmount ddAmount, CAmount dgbCollateral)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex); // RH-44: thread safety
    s_currentMetrics.totalDDSupply = std::max<CAmount>(0, s_currentMetrics.totalDDSupply - ddAmount);
    s_currentMetrics.totalCollateral = std::max<CAmount>(0, s_currentMetrics.totalCollateral - dgbCollateral);
    LogPrint(BCLog::DIGIDOLLAR, "Health: Redeem connected - DD -%s, Collateral -%s (totals: DD=%s, Collateral=%s)\n",
             FormatMoney(ddAmount), FormatMoney(dgbCollateral),
             FormatMoney(s_currentMetrics.totalDDSupply), FormatMoney(s_currentMetrics.totalCollateral));
}

void SystemHealthMonitor::OnMintDisconnected(CAmount ddAmount, CAmount dgbCollateral)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex); // RH-44: thread safety
    s_currentMetrics.totalDDSupply = std::max<CAmount>(0, s_currentMetrics.totalDDSupply - ddAmount);
    s_currentMetrics.totalCollateral = std::max<CAmount>(0, s_currentMetrics.totalCollateral - dgbCollateral);
    LogPrint(BCLog::DIGIDOLLAR, "Health: Mint disconnected - DD -%s, Collateral -%s (totals: DD=%s, Collateral=%s)\n",
             FormatMoney(ddAmount), FormatMoney(dgbCollateral),
             FormatMoney(s_currentMetrics.totalDDSupply), FormatMoney(s_currentMetrics.totalCollateral));
}

void SystemHealthMonitor::OnRedeemDisconnected(CAmount ddAmount, CAmount dgbCollateral)
{
    std::lock_guard<std::mutex> lock(s_metricsMutex); // RH-44: thread safety
    // SECURITY [RH-11]: Same overflow protection as OnMintConnected
    if (ddAmount > 0 && s_currentMetrics.totalDDSupply <= MAX_DIGIDOLLAR - ddAmount) {
        s_currentMetrics.totalDDSupply += ddAmount;
    } else if (ddAmount > 0) {
        s_currentMetrics.totalDDSupply = MAX_DIGIDOLLAR;
    }
    if (dgbCollateral > 0 && s_currentMetrics.totalCollateral <= std::numeric_limits<CAmount>::max() - dgbCollateral) {
        s_currentMetrics.totalCollateral += dgbCollateral;
    } else if (dgbCollateral > 0) {
        s_currentMetrics.totalCollateral = std::numeric_limits<CAmount>::max();
    }
    LogPrint(BCLog::DIGIDOLLAR, "Health: Redeem disconnected - DD +%s, Collateral +%s (totals: DD=%s, Collateral=%s)\n",
             FormatMoney(ddAmount), FormatMoney(dgbCollateral),
             FormatMoney(s_currentMetrics.totalDDSupply), FormatMoney(s_currentMetrics.totalCollateral));
}

void SystemHealthMonitor::AggregateWalletStats(
    const std::vector<std::shared_ptr<wallet::CWallet>>& wallets,
    CAmount& totalDDSupply,
    CAmount& totalCollateral)
{
    // Reset output parameters
    totalDDSupply = 0;
    totalCollateral = 0;

    // Aggregate across all loaded wallets
    for (const auto& wallet : wallets) {
        if (!wallet) continue;

        // Get DigiDollar wallet interface
        DigiDollarWallet* ddWallet = wallet->GetDDWallet();
        if (!ddWallet) continue;

        // Get all active positions from this wallet
        std::vector<WalletCollateralPosition> positions = ddWallet->GetDDTimeLocks(true);

        // Sum up collateral and DD minted from all positions
        for (const auto& pos : positions) {
            totalCollateral += pos.dgb_collateral;
            totalDDSupply += pos.dd_minted;
        }
    }

    LogPrint(BCLog::DIGIDOLLAR, "AggregateWalletStats: Aggregated %d wallets -> %s DD supply, %s DGB collateral\n",
             wallets.size(),
             FormatMoney(totalDDSupply),
             FormatMoney(totalCollateral));
}

void SystemHealthMonitor::UpdateTierMetrics()
{
    // Calculate current DGB price for health calculations
    CAmount currentPrice = GetLastOraclePrice();
    if (currentPrice == 0) {
        currentPrice = 50; // Default $0.50 per DGB (50 cents)
    }

    // Update per-tier metrics
    // Note: In real implementation, this would analyze actual positions by tier
    // MOCK MODE DISABLED - Always use actual on-chain data from ScanUTXOSet
    LogPrint(BCLog::DIGIDOLLAR, "UpdateTierMetrics: Using actual on-chain data (tiers.size()=%zu, totalDDSupply=%lld)\n",
             s_currentMetrics.tiers.size(), static_cast<long long>(s_currentMetrics.totalDDSupply));

    // Mock mode completely disabled per user requirement: "do not fall back to mock data!!!"
    // When scanner finds 0 vaults, stats should correctly show 0, not mock data
    /*
    if (s_currentMetrics.tiers.size() >= 6 && s_currentMetrics.totalDDSupply == 0) {
        LogPrint(BCLog::DIGIDOLLAR, "UpdateTierMetrics: MOCK MODE TRIGGERED!\n");
        // Mock mode - populate with test data
        // Tier 0: 30-day (mock data) - 150% ratio
        s_currentMetrics.tiers[0].ddMinted = 3600; // $36.00
        s_currentMetrics.tiers[0].dgbLocked = 10800000000; // 108 DGB worth $54
        s_currentMetrics.tiers[0].positions = 2;
        s_currentMetrics.tiers[0].healthRatio = HealthUtils::CalculateHealthRatio(
            s_currentMetrics.tiers[0].ddMinted,
            s_currentMetrics.tiers[0].dgbLocked,
            currentPrice
        );

        // Tier 1: 90-day (mock data) - 125% ratio
        s_currentMetrics.tiers[1].ddMinted = 5000; // $50.00
        s_currentMetrics.tiers[1].dgbLocked = 12500000000; // 125 DGB worth $62.50
        s_currentMetrics.tiers[1].positions = 2;
        s_currentMetrics.tiers[1].healthRatio = HealthUtils::CalculateHealthRatio(
            s_currentMetrics.tiers[1].ddMinted,
            s_currentMetrics.tiers[1].dgbLocked,
            currentPrice
        );

        // Tier 2: 180-day (mock data) - 120% ratio
        s_currentMetrics.tiers[2].ddMinted = 4166; // $41.66
        s_currentMetrics.tiers[2].dgbLocked = 10000000000; // 100 DGB worth $50
        s_currentMetrics.tiers[2].positions = 2;
        s_currentMetrics.tiers[2].healthRatio = HealthUtils::CalculateHealthRatio(
            s_currentMetrics.tiers[2].ddMinted,
            s_currentMetrics.tiers[2].dgbLocked,
            currentPrice
        );

        // Tier 3: 365-day (mock data) - 250% ratio
        s_currentMetrics.tiers[3].ddMinted = 2000; // $20.00
        s_currentMetrics.tiers[3].dgbLocked = 10000000000; // 100 DGB worth $50
        s_currentMetrics.tiers[3].positions = 2;
        s_currentMetrics.tiers[3].healthRatio = HealthUtils::CalculateHealthRatio(
            s_currentMetrics.tiers[3].ddMinted,
            s_currentMetrics.tiers[3].dgbLocked,
            currentPrice
        );

        // Tier 4: 365-day (mock data) - 300% ratio
        s_currentMetrics.tiers[4].ddMinted = 2500; // $25.00
        s_currentMetrics.tiers[4].dgbLocked = 10000000000; // 100 DGB worth $50
        s_currentMetrics.tiers[4].positions = 2;
        s_currentMetrics.tiers[4].healthRatio = HealthUtils::CalculateHealthRatio(
            s_currentMetrics.tiers[4].ddMinted,
            s_currentMetrics.tiers[4].dgbLocked,
            currentPrice
        );

        // Tier 5: 1825-day (mock data) - 180% ratio
        s_currentMetrics.tiers[5].ddMinted = 2777; // $27.77
        s_currentMetrics.tiers[5].dgbLocked = 10000000000; // 100 DGB worth $50
        s_currentMetrics.tiers[5].positions = 2;
        s_currentMetrics.tiers[5].healthRatio = HealthUtils::CalculateHealthRatio(
            s_currentMetrics.tiers[5].ddMinted,
            s_currentMetrics.tiers[5].dgbLocked,
            currentPrice
        );

        // Calculate totals from tier data (for mock mode)
        s_currentMetrics.totalDDSupply = 0;
        s_currentMetrics.totalCollateral = 0;
        for (const auto& tier : s_currentMetrics.tiers) {
            s_currentMetrics.totalDDSupply += tier.ddMinted;
            s_currentMetrics.totalCollateral += tier.dgbLocked;
        }
    }
    */

    // Update overall system health
    s_currentMetrics.systemHealth = CalculateSystemHealth(
        s_currentMetrics.totalDDSupply,
        s_currentMetrics.totalCollateral,
        currentPrice
    );

    LogPrint(BCLog::DIGIDOLLAR, "Tier metrics updated: %zu tiers analyzed, total DD=%s, total collateral=%s\n",
             s_currentMetrics.tiers.size(),
             FormatMoney(s_currentMetrics.totalDDSupply),
             FormatMoney(s_currentMetrics.totalCollateral));
}

void SystemHealthMonitor::UpdateProtectionStatus()
{
    s_currentMetrics.dcaMultiplier = GetCurrentDCAMultiplier();
    s_currentMetrics.errActive = IsERRActive();
    s_currentMetrics.volatility = GetCurrentVolatility();
    s_currentMetrics.mintingFrozen = IsMintingFrozen();

    LogPrint(BCLog::DIGIDOLLAR, "Protection status updated: DCA=%.2f, ERR=%s, Vol=%.1f%%, Frozen=%s\n",
             s_currentMetrics.dcaMultiplier,
             s_currentMetrics.errActive ? "YES" : "NO",
             s_currentMetrics.volatility,
             s_currentMetrics.mintingFrozen ? "YES" : "NO");
}

void SystemHealthMonitor::UpdateOracleStatus()
{
    s_currentMetrics.activeOracles = GetActiveOracleCount();
    s_currentMetrics.lastOraclePrice = GetLastOraclePrice();
    s_currentMetrics.lastOracleUpdate = GetLastOracleUpdate();

    LogPrint(BCLog::DIGIDOLLAR, "Oracle status updated: %d active, price=%s, last_update=%lld\n",
             s_currentMetrics.activeOracles,
             FormatMoney(s_currentMetrics.lastOraclePrice),
             static_cast<long long>(s_currentMetrics.lastOracleUpdate));
}

void SystemHealthMonitor::RecordHealthHistory(int64_t height, int health)
{
    s_healthHistory[height] = health;

    // Limit history size to prevent memory bloat
    const size_t MAX_HISTORY = 100000; // Keep last 100k blocks
    if (s_healthHistory.size() > MAX_HISTORY) {
        // Remove oldest entries
        auto it = s_healthHistory.begin();
        size_t toRemove = s_healthHistory.size() - MAX_HISTORY;
        for (size_t i = 0; i < toRemove && it != s_healthHistory.end(); ++i) {
            it = s_healthHistory.erase(it);
        }
    }
}

int SystemHealthMonitor::CalculateSystemHealth(CAmount ddSupply, CAmount collateral, CAmount price)
{
    if (ddSupply == 0) {
        return 300; // Perfect health if no DD issued
    }

    // Guard against invalid price (same pattern as DCA::CalculateSystemHealth)
    if (price <= 0) {
        return 0; // Cannot calculate without valid price
    }

    // Calculate collateral value in cents
    // price is in cents (100 = $1.00 DGB price)
    // collateral is in satoshis
    // Formula: (satoshis * price_cents) / COIN = cents
    // Guard against overflow: divide first when collateral is large
    CAmount collateralValue;
    const CAmount maxSafe = std::numeric_limits<CAmount>::max() / price;
    if (collateral > maxSafe) {
        collateralValue = (collateral / COIN) * price;
    } else {
        collateralValue = (collateral * price) / COIN;
    }

    // Health = (Collateral Value / DD Value) * 100
    // Guard against overflow in numerator
    int health;
    const CAmount maxSafeMul = std::numeric_limits<CAmount>::max() / 100;
    if (collateralValue > maxSafeMul) {
        // When ddSupply is 1-99, ddSupply/100 is 0 due to integer division.
        // Return max health since collateral dwarfs the tiny supply.
        CAmount scaledSupply = ddSupply / 100;
        if (scaledSupply == 0) {
            return 300;
        }
        health = static_cast<int>(collateralValue / scaledSupply);
    } else {
        health = static_cast<int>((collateralValue * 100) / ddSupply);
    }

    // Cap at reasonable maximum
    return std::min(health, 300);
}

double SystemHealthMonitor::GetCurrentVolatility()
{
    using namespace DigiDollar::Volatility;
    VolatilityState state = VolatilityMonitor::GetCurrentState();
    // Return the most severe volatility metric for health assessment
    return std::max({state.hourlyVolatility, state.dailyVolatility, state.weeklyVolatility});
}

double SystemHealthMonitor::GetCurrentDCAMultiplier()
{
    using namespace DigiDollar::DCA;
    // Get actual DCA multiplier from the DCA system
    return DynamicCollateralAdjustment::GetDCAMultiplier(s_currentMetrics.systemHealth);
}

bool SystemHealthMonitor::IsERRActive()
{
    using namespace DigiDollar::ERR;
    // Get actual ERR state from the ERR system
    ERRState state = EmergencyRedemptionRatio::GetCurrentState();
    return state.isActive;
}

bool SystemHealthMonitor::IsMintingFrozen()
{
    using namespace DigiDollar::Volatility;
    // Check both volatility freeze and ERR freeze conditions
    bool volatilityFrozen = VolatilityMonitor::ShouldFreezeMinting() || VolatilityMonitor::ShouldFreezeAll();
    bool errFrozen = IsERRActive(); // ERR may also freeze minting
    return volatilityFrozen || errFrozen;
}

int SystemHealthMonitor::GetActiveOracleCount()
{
    // Get actual oracle count from oracle system
    // TODO: Implement proper oracle system integration
    // For now, return a reasonable default until oracle system is fully implemented
    return 8; // Conservative estimate until proper integration
}

CAmount SystemHealthMonitor::GetLastOraclePrice()
{
    // Get actual price from oracle system
    // TODO: Implement proper oracle system integration
    // For now, check if we have volatility data which implies oracle data
    using namespace DigiDollar::Volatility;
    if (VolatilityMonitor::IsInitialized()) {
        auto history = VolatilityMonitor::GetPriceHistory();
        if (!history.empty()) {
            return history.back().price;
        }
    }
    return 50; // Default $0.50 per DGB (50 cents)
}

int64_t SystemHealthMonitor::GetLastOracleUpdate()
{
    // Get actual oracle update time from oracle system
    // TODO: Implement proper oracle system integration
    // For now, check if we have volatility data which implies oracle data
    using namespace DigiDollar::Volatility;
    if (VolatilityMonitor::IsInitialized()) {
        auto history = VolatilityMonitor::GetPriceHistory();
        if (!history.empty()) {
            return history.back().height;
        }
    }
    // Fallback to mock recent height
    return 1000000 - 5; // Conservative estimate
}

// Alert checking implementations
bool SystemHealthMonitor::CheckSupplyAlert(const SystemMetrics& metrics)
{
    return metrics.totalDDSupply > AlertThresholds::MAX_DD_SUPPLY;
}

bool SystemHealthMonitor::CheckHealthAlert(const SystemMetrics& metrics)
{
    return metrics.systemHealth < AlertThresholds::MIN_HEALTH_RATIO;
}

bool SystemHealthMonitor::CheckCollateralAlert(const SystemMetrics& metrics)
{
    // Alert if collateral is insufficient for current supply
    return metrics.systemHealth < AlertThresholds::CRITICAL_HEALTH_RATIO;
}

bool SystemHealthMonitor::CheckOracleAlert(const SystemMetrics& metrics)
{
    // TODO: Fix chainstate access - temporary mock implementation
    // For now, use mock height to prevent compilation errors
    int64_t mockHeight = 1000000; // This should be replaced with proper chainstate access
    bool staleData = (mockHeight - metrics.lastOracleUpdate) > AlertThresholds::STALE_ORACLE_BLOCKS;
    bool lowCount = metrics.activeOracles < AlertThresholds::MIN_ORACLES;
    return staleData || lowCount;
}

bool SystemHealthMonitor::CheckVolatilityAlert(const SystemMetrics& metrics)
{
    return metrics.volatility > AlertThresholds::MAX_VOLATILITY;
}

bool SystemHealthMonitor::CheckPositionAlert(const SystemMetrics& metrics)
{
    int totalPositions = 0;
    for (const auto& tier : metrics.tiers) {
        totalPositions += tier.positions;
    }
    return totalPositions > AlertThresholds::MAX_POSITIONS;
}

// Health utility implementations
namespace HealthUtils {

int GetTierIndex(int lockDays)
{
    for (size_t i = 0; i < TIER_LOCK_DAYS.size(); ++i) {
        if (lockDays <= TIER_LOCK_DAYS[i]) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(TIER_LOCK_DAYS.size() - 1); // Longest tier
}

int GetTierLockDays(int tierIndex)
{
    if (tierIndex >= 0 && tierIndex < static_cast<int>(TIER_LOCK_DAYS.size())) {
        return TIER_LOCK_DAYS[tierIndex];
    }
    return TIER_LOCK_DAYS.back(); // Default to longest
}

int CalculateHealthRatio(CAmount ddAmount, CAmount dgbAmount, CAmount dgbPrice)
{
    if (ddAmount <= 0) {
        return 300; // Perfect if no DD issued
    }

    if (dgbPrice <= 0 || dgbAmount <= 0) {
        return 0; // Cannot calculate without valid price/amount
    }

    // Calculate DGB value in cents using __int128 to prevent overflow.
    // dgbPrice is in cents (100 = $1.00 DGB price)
    // dgbAmount is in satoshis
    // Formula: (satoshis * price_cents) / COIN = value_in_cents
    // Then health = (value_in_cents * 100) / ddAmount
    //
    // Using __int128 is safe here because this is a monitoring/display
    // function, not consensus-critical code. The consensus equivalent
    // (CalculateSystemHealth) uses a divide-first pattern, but __int128
    // is simpler and handles all edge cases without precision loss.
    __int128 dgbValue128 = static_cast<__int128>(dgbAmount) * static_cast<__int128>(dgbPrice);
    dgbValue128 /= COIN;

    // Health = (Collateral Value / DD Value) * 100
    __int128 health128 = (dgbValue128 * 100) / static_cast<__int128>(ddAmount);

    // Clamp to [0, 300]
    if (health128 < 0) return 0;
    if (health128 > 300) return 300;
    return static_cast<int>(health128);
}

std::string FormatHealthStatus(int health)
{
    if (health >= AlertThresholds::MIN_HEALTH_RATIO) {
        return "Healthy";
    } else if (health >= AlertThresholds::CRITICAL_HEALTH_RATIO) {
        return "Warning";
    } else {
        return "Critical";
    }
}

std::string GetRecommendedAction(int health)
{
    if (health >= AlertThresholds::MIN_HEALTH_RATIO) {
        return "Monitor";
    } else if (health >= AlertThresholds::CRITICAL_HEALTH_RATIO) {
        return "Add Collateral";
    } else {
        return "Emergency Action Required";
    }
}

} // namespace HealthUtils

} // namespace DigiDollar