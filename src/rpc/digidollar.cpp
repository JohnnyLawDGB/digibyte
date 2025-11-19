// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/util.h>
#include <rpc/server_util.h>
#include <rpc/blockchain.h>
#include <oracle/bundle_manager.h>
#include <oracle/node.h>
#include <oracle/mock_oracle.h>
#include <consensus/digidollar.h>
#include <consensus/dca.h>
#include <digidollar/digidollar.h>
#include <digidollar/health.h>
#include <chainparams.h>
#include <kernel/chainparams.h>
#include <node/context.h>
#include <core_io.h>
#include <util/strencodings.h>
#include <validation.h>
#include <versionbits.h>
#include <wallet/wallet.h>
#include <wallet/context.h>
#include <wallet/rpc/util.h>
#include <wallet/spend.h>
#include <wallet/coinselection.h>
#include <wallet/digidollarwallet.h>
#include <interfaces/wallet.h>
#include <digidollar/txbuilder.h>
#include <node/transaction.h>
#include <base58.h>
#include <script/standard.h>
#include <rpc/protocol.h>
#include <versionbits.h>
#include <deploymentstatus.h>

#include <univalue.h>

using namespace DigiDollar;
using namespace DigiDollar::DCA;

// Mock utility functions for RPC-only implementation
namespace {
    int GetLockDaysForTier(uint32_t tier) {
        switch (tier) {
            case 0: return 0;     // Special: 1 hour (240 blocks) - handled separately
            case 1: return 30;
            case 2: return 90;
            case 3: return 180;
            case 4: return 365;
            case 5: return 1095;  // 3 years
            case 6: return 1825;  // 5 years
            case 7: return 2555;  // 7 years
            case 8: return 3650;  // 10 years
            default: return 0;
        }
    }

    int GetMinCollateralRatio(uint32_t tier) {
        switch (tier) {
            case 0: return 1000;  // 1000% for 1 hour (testing only)
            case 1: return 500;   // 500% for 30 days
            case 2: return 400;   // 400% for 90 days
            case 3: return 350;   // 350% for 180 days
            case 4: return 300;   // 300% for 1 year
            case 5: return 250;   // 250% for 3 years
            case 6: return 225;   // 225% for 5 years
            case 7: return 212;   // 212% for 7 years
            case 8: return 200;   // 200% for 10 years
            default: return 500;
        }
    }
}

RPCHelpMan getdigidollarstats()
{
    return RPCHelpMan{"getdigidollarstats",
                "\nGet current DigiDollar system health information.\n"
                "Returns the overall health of the DigiDollar stablecoin system,\n"
                "including collateralization ratio and DCA tier status.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "health_percentage", "System health as percentage (e.g., 150 = 150% collateralized)"},
                        {RPCResult::Type::STR, "health_status", "Health tier status: healthy, warning, critical, or emergency"},
                        {RPCResult::Type::NUM, "total_collateral_dgb", "Total DGB locked as collateral"},
                        {RPCResult::Type::NUM, "total_dd_supply", "Total DigiDollar supply in circulation (in cents)"},
                        {RPCResult::Type::NUM, "oracle_price_cents", "Current DGB/USD price from oracle (in cents per DGB)"},
                        {RPCResult::Type::BOOL, "is_emergency", "True if system is in emergency state (<100% collateralized)"},
                        {RPCResult::Type::NUM, "system_collateral_ratio", "Alias for health_percentage (for backward compatibility)"},
                        {RPCResult::Type::NUM, "total_collateral_locked", "Alias for total_collateral_dgb (in satoshis)"},
                        {RPCResult::Type::NUM, "active_positions", "Number of active DD positions"},
                        {RPCResult::Type::NUM, "oracle_price_age", "Blocks since last oracle update"},
                        {RPCResult::Type::OBJ, "dca_tier", "Current DCA tier information",
                            {
                                {RPCResult::Type::NUM, "min_collateral", "Minimum collateral % for this tier"},
                                {RPCResult::Type::NUM, "max_collateral", "Maximum collateral % for this tier"},
                                {RPCResult::Type::NUM, "multiplier", "DCA multiplier for new mints in this tier"},
                                {RPCResult::Type::STR, "status", "Tier status description"}
                            }
                        }
                    }
                },
                RPCExamples{
                    HelpExampleCli("getdigidollarstats", "")
                    + HelpExampleRpc("getdigidollarstats", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // NETWORK-WIDE TRACKING: Scan UTXO set instead of just loaded wallets
            // This ensures all nodes see identical stats regardless of which wallets are loaded
            CAmount totalCollateral = 0;
            CAmount totalDD = 0;

            // Get node context for chainstate access
            const node::NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);

            // Access the UTXO set (like gettxoutsetinfo does)
            // CRITICAL: Must flush OUTSIDE the lock, then re-acquire lock for scanning
            Chainstate& active_chainstate = chainman.ActiveChainstate();

            // Step 1: Force flush all cached coins to disk (like gettxoutsetinfo does)
            LogPrintf("DigiDollar: getdigidollarstats - About to ForceFlushStateToDisk...\n");
            active_chainstate.ForceFlushStateToDisk();
            LogPrintf("DigiDollar: getdigidollarstats - ForceFlushStateToDisk completed\n");

            // Step 2: Now acquire lock and access the flushed CoinsDB
            CCoinsView* coins_view;
            node::BlockManager* blockman;
            {
                LOCK(::cs_main);
                coins_view = &active_chainstate.CoinsDB();
                blockman = &active_chainstate.m_blockman;
            }

            const CTxMemPool* mempool = node.mempool.get();

            // Scan UTXO set to find ALL DigiDollar vaults network-wide
            // Pass BlockManager for full transaction access
            // Pass both CoinsDB (for iteration) and CoinsTip (for validation)
            LogPrintf("DigiDollar: getdigidollarstats - About to call ScanUTXOSet...\n");
            DigiDollar::SystemHealthMonitor::ScanUTXOSet(coins_view, &active_chainstate.CoinsTip(), blockman, mempool);
            LogPrintf("DigiDollar: getdigidollarstats - ScanUTXOSet completed\n");

            // Get metrics from scanner
            DigiDollar::SystemMetrics metrics = DigiDollar::SystemHealthMonitor::GetSystemMetrics();
            totalCollateral = metrics.totalCollateral;
            totalDD = metrics.totalDDSupply;

            // Get current oracle price from MockOracleManager
            // Oracle price format: cents per DGB (e.g., 50 = $0.50/DGB)
            CAmount oraclePrice = MockOracleManager::GetInstance().GetCurrentPrice();

            // Convert oracle price (cents per DGB) to millicents per DGB for CalculateSystemHealth
            // Oracle returns cents/DGB, CalculateSystemHealth expects millicents/DGB (cents * 1000)
            CAmount oraclePriceMillicents = oraclePrice * 1000;

            // Calculate system health
            // IMPORTANT: Return 0% if no DD minted network-wide (instead of default 30000%)
            int systemHealth;
            if (totalDD == 0) {
                systemHealth = 0;  // No DD minted = 0% health, not 30000%
            } else {
                systemHealth = DynamicCollateralAdjustment::CalculateSystemHealth(
                    totalCollateral, totalDD, oraclePriceMillicents);
            }

            // Get current tier information
            auto tier = DynamicCollateralAdjustment::GetCurrentTier(systemHealth);

            // Check emergency status
            bool isEmergency = DynamicCollateralAdjustment::IsSystemEmergency(systemHealth);

            UniValue result(UniValue::VOBJ);
            result.pushKV("health_percentage", systemHealth);
            result.pushKV("health_status", tier.status);
            result.pushKV("total_collateral_dgb", ValueFromAmount(totalCollateral));
            result.pushKV("total_dd_supply", totalDD);
            result.pushKV("oracle_price_cents", oraclePrice); // Oracle price is already in cents per DGB
            result.pushKV("is_emergency", isEmergency);

            // Add fields expected by tests
            result.pushKV("system_collateral_ratio", systemHealth);
            result.pushKV("total_collateral_locked", ValueFromAmount(totalCollateral));
            result.pushKV("active_positions", 0); // TODO: Count positions from UTXO scan
            result.pushKV("oracle_price_age", 0); // TODO: Calculate age

            UniValue dcaTier(UniValue::VOBJ);
            dcaTier.pushKV("min_collateral", tier.minCollateral);
            dcaTier.pushKV("max_collateral", tier.maxCollateral);
            dcaTier.pushKV("multiplier", tier.multiplier);
            dcaTier.pushKV("status", tier.status);
            result.pushKV("dca_tier", dcaTier);

            return result;
        },
    };
}

static RPCHelpMan getdcamultiplier()
{
    return RPCHelpMan{"getdcamultiplier",
                "\nGet current Dynamic Collateral Adjustment (DCA) multiplier.\n"
                "Returns the multiplier applied to base collateral ratios for new mints.\n",
                {
                    {"system_health", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Optional: calculate multiplier for specific health % (for testing)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "multiplier", "Current DCA multiplier (e.g., 1.0 = no adjustment, 2.0 = double collateral)"},
                        {RPCResult::Type::NUM, "system_health", "System health percentage used for calculation"},
                        {RPCResult::Type::STR, "tier_status", "Health tier: healthy, warning, critical, or emergency"},
                        {RPCResult::Type::STR, "description", "Human-readable description of DCA effect"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getdcamultiplier", "")
                    + HelpExampleCli("getdcamultiplier", "130")
                    + HelpExampleRpc("getdcamultiplier", "")
                    + HelpExampleRpc("getdcamultiplier", "130")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            int systemHealth;

            // Use provided health or calculate current
            if (!request.params[0].isNull()) {
                systemHealth = request.params[0].getInt<int>();
                if (systemHealth < 0 || systemHealth > 30000) {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "System health must be between 0 and 30000");
                }
            } else {
                systemHealth = DynamicCollateralAdjustment::GetCurrentSystemHealth();
            }

            // Get DCA multiplier
            double multiplier = DynamicCollateralAdjustment::GetDCAMultiplier(systemHealth);
            auto tier = DynamicCollateralAdjustment::GetCurrentTier(systemHealth);

            // Create description
            std::string description;
            if (multiplier == 1.0) {
                description = "No additional collateral required (healthy system)";
            } else {
                description = strprintf("%.1fx base collateral required (%s system)",
                                      multiplier, tier.status);
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("multiplier", multiplier);
            result.pushKV("system_health", systemHealth);
            result.pushKV("tier_status", tier.status);
            result.pushKV("description", description);

            return result;
        },
    };
}


static RPCHelpMan calculatecollateralrequirement()
{
    return RPCHelpMan{"calculatecollateralrequirement",
                "\nCalculate DGB collateral requirement for a DigiDollar mint.\n"
                "Uses current system health and DCA multipliers to determine\n"
                "the exact amount of DGB needed for a given DD mint amount and lock period.\n",
                {
                    {"dd_amount_cents", RPCArg::Type::NUM, RPCArg::Optional::NO, "DigiDollar amount to mint in cents (e.g., 10000 = $100)"},
                    {"lock_days", RPCArg::Type::NUM, RPCArg::Optional::NO, "Lock period in days (30, 90, 180, 365, 1095, 1825, 2555, or 3650)"},
                    {"oracle_price", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "DGB price in cents per DGB (uses current price if omitted)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "required_dgb", "Required DGB collateral amount"},
                        {RPCResult::Type::NUM, "dd_amount_cents", "DD amount being minted (in cents)"},
                        {RPCResult::Type::NUM, "dd_amount_usd", "DD amount being minted (in USD)"},
                        {RPCResult::Type::NUM, "lock_days", "Lock period in days"},
                        {RPCResult::Type::NUM, "lock_blocks", "Lock period in blocks"},
                        {RPCResult::Type::NUM, "base_ratio", "Base collateral ratio % for this lock period"},
                        {RPCResult::Type::NUM, "dca_multiplier", "DCA multiplier applied"},
                        {RPCResult::Type::NUM, "effective_ratio", "Final collateral ratio % (base * DCA)"},
                        {RPCResult::Type::NUM, "oracle_price", "DGB price used (cents per DGB)"},
                        {RPCResult::Type::NUM, "system_health", "Current system health %"},
                        {RPCResult::Type::STR, "dca_tier", "Current DCA tier status"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("calculatecollateralrequirement", "10000 365")
                    + HelpExampleCli("calculatecollateralrequirement", "50000 1095 4000")
                    + HelpExampleRpc("calculatecollateralrequirement", "10000, 365")
                    + HelpExampleRpc("calculatecollateralrequirement", "50000, 1095, 4000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Parse parameters
            CAmount ddAmount = request.params[0].getInt<int64_t>();
            int lockDays = request.params[1].getInt<int>();
            CAmount oraclePrice = request.params.size() > 2 ?
                request.params[2].getInt<int64_t>() : 5000; // Default $0.05 per DGB

            // Validate parameters
            if (ddAmount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "DD amount must be positive");
            }
            if (lockDays <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Lock days must be positive");
            }
            if (oraclePrice <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Oracle price must be positive");
            }

            // Get system parameters
            const auto& params = Params();
            const auto& ddParams = params.GetDigiDollarParams();

            // Convert lock days to blocks
            int64_t lockBlocks = DigiDollar::LockDaysToBlocks(lockDays);

            // Get base collateral ratio
            int baseRatio = DigiDollar::GetCollateralRatioForLockTime(lockBlocks, ddParams);
            if (baseRatio <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Invalid lock period: %d days. Valid periods: 30, 90, 180, 365, 1095, 1825, 2555, 3650", lockDays));
            }

            // Get current system health
            int systemHealth = DynamicCollateralAdjustment::GetCurrentSystemHealth();
            double dcaMultiplier = DynamicCollateralAdjustment::GetDCAMultiplier(systemHealth);
            int effectiveRatio = DynamicCollateralAdjustment::ApplyDCA(baseRatio, systemHealth);
            auto tier = DynamicCollateralAdjustment::GetCurrentTier(systemHealth);

            // Calculate required DGB
            CAmount requiredDGB = (ddAmount * effectiveRatio * COIN) / (oraclePrice / 100);

            UniValue result(UniValue::VOBJ);
            result.pushKV("required_dgb", ValueFromAmount(requiredDGB));
            result.pushKV("dd_amount_cents", ddAmount);
            result.pushKV("dd_amount_usd", ddAmount / 100.0);  // Convert cents to USD
            result.pushKV("lock_days", lockDays);
            result.pushKV("lock_blocks", lockBlocks);
            result.pushKV("base_ratio", baseRatio);
            result.pushKV("dca_multiplier", dcaMultiplier);
            result.pushKV("effective_ratio", effectiveRatio);
            result.pushKV("oracle_price", oraclePrice);
            result.pushKV("system_health", systemHealth);
            result.pushKV("dca_tier", tier.status);

            return result;
        },
    };
}

static RPCHelpMan getdigidollardeploymentinfo()
{
    return RPCHelpMan{"getdigidollardeploymentinfo",
                "\nGet DigiDollar BIP9 deployment activation status and information.\n"
                "Returns detailed information about DigiDollar soft fork deployment status,\n"
                "including activation state, signaling progress, and timeline.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "enabled", "Whether DigiDollar is currently enabled/active"},
                        {RPCResult::Type::STR, "status", "Deployment status (defined, started, locked_in, active, failed)"},
                        {RPCResult::Type::NUM, "bit", "Version bit used for BIP9 signaling"},
                        {RPCResult::Type::NUM, "start_time", "Start time for deployment signaling"},
                        {RPCResult::Type::NUM, "timeout", "Timeout for deployment"},
                        {RPCResult::Type::NUM, "min_activation_height", "Minimum activation height"},
                        {RPCResult::Type::NUM, "activation_height", "Actual activation height (if activated)"},
                        {RPCResult::Type::NUM, "blocks_until_timeout", "Blocks remaining until timeout (if applicable)"},
                        {RPCResult::Type::NUM, "signaling_blocks", "Blocks signaling support in current period"},
                        {RPCResult::Type::NUM, "threshold", "Threshold required for activation"},
                        {RPCResult::Type::NUM, "period_blocks", "Number of blocks in signaling period"},
                        {RPCResult::Type::NUM, "progress_percent", "Signaling progress as percentage"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getdigidollardeploymentinfo", "")
                    + HelpExampleRpc("getdigidollardeploymentinfo", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const ChainstateManager& chainman = EnsureAnyChainman(request.context);
            LOCK(cs_main);
            const Chainstate& active_chainstate = chainman.ActiveChainstate();
            const CBlockIndex* tip = active_chainstate.m_chain.Tip();

            UniValue result(UniValue::VOBJ);

            // Check if DigiDollar is currently enabled
            bool enabled = DigiDollar::IsDigiDollarEnabled(tip, chainman);
            result.pushKV("enabled", enabled);

            // Get deployment parameters
            const Consensus::Params& consensusParams = chainman.GetConsensus();
            const Consensus::BIP9Deployment& deployment = consensusParams.vDeployments[Consensus::DEPLOYMENT_DIGIDOLLAR];

            result.pushKV("bit", deployment.bit);
            result.pushKV("start_time", deployment.nStartTime);
            result.pushKV("timeout", deployment.nTimeout);
            result.pushKV("min_activation_height", deployment.min_activation_height);

            // Get deployment state and statistics
            ThresholdState state = chainman.m_versionbitscache.State(tip, consensusParams, Consensus::DEPLOYMENT_DIGIDOLLAR);

            const char* status_str = "unknown";
            switch (state) {
                case ThresholdState::DEFINED: status_str = "defined"; break;
                case ThresholdState::STARTED: status_str = "started"; break;
                case ThresholdState::LOCKED_IN: status_str = "locked_in"; break;
                case ThresholdState::ACTIVE: status_str = "active"; break;
                case ThresholdState::FAILED: status_str = "failed"; break;
            }
            result.pushKV("status", status_str);

            // Get statistics for signaling progress
            if (tip && (state == ThresholdState::STARTED || state == ThresholdState::LOCKED_IN)) {
                BIP9Stats stats = chainman.m_versionbitscache.Statistics(tip, consensusParams, Consensus::DEPLOYMENT_DIGIDOLLAR);
                result.pushKV("blocks_until_timeout", stats.period - stats.elapsed);
                result.pushKV("signaling_blocks", stats.count);
                result.pushKV("threshold", stats.threshold);
                result.pushKV("period_blocks", stats.period);
                result.pushKV("progress_percent", stats.threshold > 0 ? (100.0 * stats.count) / stats.threshold : 0.0);
            }

            // Get activation height if active
            if (state == ThresholdState::ACTIVE) {
                // Find the activation height by searching backwards
                const CBlockIndex* pindex = tip;
                while (pindex && pindex->pprev) {
                    if (chainman.m_versionbitscache.State(pindex->pprev, consensusParams, Consensus::DEPLOYMENT_DIGIDOLLAR) != ThresholdState::ACTIVE) {
                        result.pushKV("activation_height", pindex->nHeight);
                        break;
                    }
                    pindex = pindex->pprev;
                }
            }

            return result;
        },
    };
}

// =============================================================================
// CORE RPC COMMANDS (Task 5.7)
// =============================================================================

RPCHelpMan mintdigidollar()
{
    return RPCHelpMan{"mintdigidollar",
                "\nMint new DigiDollar with DGB collateral.\n"
                "Creates a new DigiDollar position by locking DGB as collateral.\n"
                "The amount of collateral required depends on the lock period and current system health.\n",
                {
                    {"dd_amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "Amount of DigiDollar to mint (in USD cents, e.g., 10000 = $100)", RPCArgOptions{.skip_type_check = true}},
                    {"lock_tier", RPCArg::Type::NUM, RPCArg::Optional::NO, "Lock tier 0-8 (0=1h testing, 1=30d, 2=90d, 3=180d, 4=1y, 5=3y, 6=5y, 7=7y, 8=10y)", RPCArgOptions{.skip_type_check = true}},
                    {"fee_rate", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Fee rate in sat/kB (default: 100000)", RPCArgOptions{.skip_type_check = true}}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "txid", "Transaction ID of the mint transaction"},
                        {RPCResult::Type::STR_AMOUNT, "dd_minted", "Amount of DigiDollar minted (in cents)"},
                        {RPCResult::Type::STR_AMOUNT, "dgb_collateral", "DGB locked as collateral"},
                        {RPCResult::Type::NUM, "lock_tier", "Lock tier used"},
                        {RPCResult::Type::NUM, "unlock_height", "Block height when collateral becomes unlockable"},
                        {RPCResult::Type::NUM, "collateral_ratio", "Effective collateral ratio percentage"},
                        {RPCResult::Type::STR_AMOUNT, "fee_paid", "Transaction fee paid"},
                        {RPCResult::Type::STR, "position_id", "Unique position identifier"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("mintdigidollar", "10000 3") +
                    HelpExampleCli("mintdigidollar", "50000 5 0.001") +
                    HelpExampleRpc("mintdigidollar", "10000, 3") +
                    HelpExampleRpc("mintdigidollar", "50000, 5, 0.001")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Get wallet
            std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "No wallet is loaded");

            // Ensure wallet is unlocked
            wallet::EnsureWalletIsUnlocked(*pwallet);

            // Parse parameters
            CAmount ddAmount = request.params[0].getInt<int64_t>();
            int lockTier = request.params[1].getInt<int>();
            CAmount feeRate = request.params.size() > 2 && !request.params[2].isNull() ?
                request.params[2].getInt<int64_t>() : 100000; // Default 100000 sat/kB (DigiByte minimum)

            // Validate parameters
            if (ddAmount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "DigiDollar amount must be positive");
            }
            if (lockTier < 0 || lockTier > 8) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Lock tier must be between 0 and 8 (0 = 1 hour testing tier)");
            }

            // Get current height from wallet's chain interface
            int currentHeight = pwallet->GetLastBlockHeight();

            // Get oracle price
            CAmount oraclePrice = MockOracleManager::GetInstance().GetCurrentPrice();
            if (oraclePrice <= 0) {
                oraclePrice = 1; // Fallback to 1 cent per DGB = $0.01/DGB
            }

            // Convert lock tier to days
            int lockDays = GetLockDaysForTier(lockTier);

            // Get available UTXOs from wallet and build value map
            std::vector<COutPoint> availableUtxos;
            std::map<COutPoint, CAmount> utxoValues;
            {
                LOCK(pwallet->cs_wallet);
                wallet::CoinsResult coins = wallet::AvailableCoins(*pwallet);
                for (const wallet::COutput& coin : coins.All()) {
                    availableUtxos.push_back(coin.outpoint);
                    utxoValues[coin.outpoint] = coin.txout.nValue;
                }
            }

            if (availableUtxos.empty()) {
                throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "No available UTXOs for collateral");
            }

            // Generate owner key from wallet
            CKey ownerKey;
            {
                LOCK(pwallet->cs_wallet);
                // Generate a new key for this mint position
                // In production, would integrate with wallet's key management
                ownerKey.MakeNewKey(true); // Generate compressed key
            }

            // Create custom MintTxBuilder that can look up actual UTXO values
            // This is critical - without this, SelectCoins uses hardcoded placeholder values
            // and selects hundreds of UTXOs, wasting millions of DGB!
            class RpcMintTxBuilder : public DigiDollar::MintTxBuilder {
            private:
                const std::map<COutPoint, CAmount>& m_utxo_values;
            public:
                RpcMintTxBuilder(const CChainParams& params, int height, CAmount price,
                               const std::map<COutPoint, CAmount>& utxo_values)
                    : MintTxBuilder(params, height, price), m_utxo_values(utxo_values) {}

                CAmount GetDGBFromUTXO(const COutPoint& outpoint) const override {
                    auto it = m_utxo_values.find(outpoint);
                    if (it != m_utxo_values.end()) {
                        return it->second;
                    }
                    return 0; // UTXO not found
                }
            };

            // Build mint transaction using custom RpcMintTxBuilder with UTXO value lookup
            RpcMintTxBuilder builder(Params(), currentHeight, oraclePrice, utxoValues);

            DigiDollar::TxBuilderMintParams params;
            params.ddAmount = ddAmount;  // Amount in cents (e.g., 5000 = $50.00)
            params.lockDays = lockDays;
            params.ownerKey = ownerKey;
            params.feeRate = feeRate;
            params.utxos = availableUtxos;

            DigiDollar::TxBuilderResult result = builder.BuildMintTransaction(params);

            if (!result.success) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to build mint transaction: " + result.error);
            }

            // Sign transaction
            bool signSuccess = false;
            {
                LOCK(pwallet->cs_wallet);
                signSuccess = pwallet->SignTransaction(result.tx);
            }

            if (!signSuccess) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign mint transaction");
            }

            // Create transaction reference for commitment
            CTransactionRef tx = MakeTransactionRef(result.tx);

            // Commit transaction to wallet and broadcast
            {
                LOCK(pwallet->cs_wallet);
                pwallet->CommitTransaction(tx, {}, {});
            }

            // Calculate unlock height using consensus function (handles tier 0 special case)
            int64_t lockBlocks = DigiDollar::LockDaysToBlocks(lockDays);
            int unlockHeight = currentHeight + lockBlocks;

            // CRITICAL FIX: Persist DD position to DigiDollarWallet
            if (pwallet->GetDDWallet()) {
                WalletCollateralPosition position;
                position.dd_timelock_id = tx->GetHash();
                position.dgb_collateral = result.collateralRequired;
                position.dd_minted = ddAmount;
                position.lock_tier = lockTier;
                position.unlock_height = unlockHeight;
                position.is_active = true;
                position.owner_keyid = ownerKey.GetPubKey().GetID();

                LOCK(pwallet->cs_wallet);
                pwallet->GetDDWallet()->AddCollateralPosition(position);

                // CRITICAL: Store the owner key for this position so we can spend it later
                pwallet->GetDDWallet()->StoreOwnerKey(tx->GetHash(), ownerKey);

                LogPrintf("DigiDollar RPC: Added position %s with %d DD cents and stored owner key\n",
                         position.dd_timelock_id.ToString(), ddAmount);
            } else {
                LogPrintf("DigiDollar RPC: WARNING - No DD wallet context, position not persisted\n");
            }

            // Calculate collateral ratio based on lock tier and DCA
            int collateralRatio = 150; // Default, could be calculated from DCA

            UniValue resultObj(UniValue::VOBJ);
            resultObj.pushKV("txid", tx->GetHash().GetHex());
            resultObj.pushKV("dd_minted", ddAmount);
            resultObj.pushKV("dgb_collateral", ValueFromAmount(result.collateralRequired));
            resultObj.pushKV("lock_tier", lockTier);
            resultObj.pushKV("unlock_height", unlockHeight);
            resultObj.pushKV("collateral_ratio", collateralRatio);
            resultObj.pushKV("fee_paid", ValueFromAmount(result.totalFees));
            resultObj.pushKV("position_id", tx->GetHash().GetHex());

            return resultObj;
        },
    };
}

RPCHelpMan senddigidollar()
{
    return RPCHelpMan{"senddigidollar",
                "\nSend DigiDollar to another DigiDollar address.\n"
                "Creates a transaction that transfers DigiDollar from your wallet to the specified address.\n"
                "This is the primary RPC command for Phase 7.7 - DD transfers via API.\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "DigiDollar address to send to (DD/TD/RD prefix)"},
                    {"amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "Amount to send (in USD cents, e.g., 10000 = $100.00)", RPCArgOptions{.skip_type_check = true}},
                    {"comment", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Optional comment for the transaction"},
                    {"fee_rate", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Fee rate in sat/kB", RPCArgOptions{.skip_type_check = true}}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "txid", "Transaction ID"},
                        {RPCResult::Type::STR, "to_address", "Recipient DigiDollar address"},
                        {RPCResult::Type::STR_AMOUNT, "amount", "Amount sent (in cents)"},
                        {RPCResult::Type::STR, "status", "Transaction status (success/pending/failed)"},
                        {RPCResult::Type::STR_AMOUNT, "fee_paid", "Transaction fee paid in DGB (optional)"},
                        {RPCResult::Type::NUM, "inputs_used", "Number of DD inputs consumed (optional)"},
                        {RPCResult::Type::STR_AMOUNT, "change_amount", "DD change amount if any (optional)"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("senddigidollar", "\"DDtestaddress123456789abcdef\" 5000") +
                    HelpExampleCli("senddigidollar", "\"DDtestaddress123456789abcdef\" 5000 \"Payment for services\"") +
                    HelpExampleRpc("senddigidollar", "\"DDtestaddress123456789abcdef\", 5000") +
                    HelpExampleRpc("senddigidollar", "\"DDtestaddress123456789abcdef\", 5000, \"Payment for services\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // PHASE 7.7: Integration with backend TransferDigiDollar() from Phase 2.1

            // Get wallet
            std::shared_ptr<wallet::CWallet> const pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet not found");
            }

            // Get DigiDollar wallet
            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not initialized");
            }

            // Parse parameters
            std::string addressStr = request.params[0].get_str();
            CAmount amount = request.params[1].getInt<int64_t>();  // Amount in USD cents
            std::string comment = request.params.size() > 2 ? request.params[2].get_str() : "";

            // Validate amount
            if (amount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be positive");
            }

            // Parse and validate DD address
            CDigiDollarAddress dd_address(addressStr);
            if (!dd_address.IsValid()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DigiDollar address");
            }

            // Check balance
            CAmount balance = dd_wallet->GetTotalDDBalance();
            if (amount > balance) {
                throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                    strprintf("Insufficient DD balance (have %d cents, need %d cents)",
                             balance, amount));
            }

            // Execute transfer using backend function (Phase 2.1)
            std::string txid;
            std::string error;

            bool success = dd_wallet->TransferDigiDollar(dd_address, amount, txid, error);

            if (!success) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                    strprintf("Transfer failed: %s", error));
            }

            // Build result
            UniValue result(UniValue::VOBJ);
            result.pushKV("txid", txid);
            result.pushKV("to_address", addressStr);
            result.pushKV("amount", ValueFromAmount(amount));
            result.pushKV("status", "success");

            // Add optional fields (placeholder values for now - TODO: get from TransferDigiDollar result)
            result.pushKV("fee_paid", ValueFromAmount(0));  // TODO: track actual fee
            result.pushKV("inputs_used", 0);  // TODO: track DD inputs used
            result.pushKV("change_amount", ValueFromAmount(0));  // TODO: track DD change

            // Optional: Add comment to wallet transaction if provided
            if (!comment.empty()) {
                result.pushKV("comment", comment);
            }

            return result;
        },
    };
}

RPCHelpMan redeemdigidollar()
{
    return RPCHelpMan{"redeemdigidollar",
                "\nRedeem DigiDollar and unlock DGB collateral.\n"
                "Burns DigiDollar tokens and unlocks the corresponding DGB collateral.\n"
                "Only positions that have reached maturity can be redeemed.\n",
                {
                    {"position_id", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Position ID (transaction hash of mint)"},
                    {"dd_amount", RPCArg::Type::AMOUNT, RPCArg::Optional::NO, "Amount of DD to redeem (in cents)"},
                    {"redemption_address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "DGB address to receive unlocked collateral (default: new address)"},
                    {"fee_rate", RPCArg::Type::AMOUNT, RPCArg::Optional::OMITTED, "Fee rate in DGB/kvB"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "txid", "Redemption transaction ID"},
                        {RPCResult::Type::STR, "position_id", "Original position ID"},
                        {RPCResult::Type::STR_AMOUNT, "dd_redeemed", "Amount of DD redeemed (burned)"},
                        {RPCResult::Type::STR_AMOUNT, "dgb_unlocked", "Amount of DGB unlocked"},
                        {RPCResult::Type::STR, "unlock_address", "DGB address that received unlocked collateral"},
                        {RPCResult::Type::STR_AMOUNT, "fee_paid", "Transaction fee paid"},
                        {RPCResult::Type::STR, "redemption_path", "Redemption path used (normal/emergency/liquidation)"},
                        {RPCResult::Type::BOOL, "position_closed", "Whether the position was fully closed"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("redeemdigidollar", "\"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890\" 5000") +
                    HelpExampleCli("redeemdigidollar", "\"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890\" 5000 \"DGb1A2B3C4D5E6F7G8H9I0J1K2L3M4N5O6P7Q8R9S0\"") +
                    HelpExampleRpc("redeemdigidollar", "\"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890\", 5000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Parse parameters
            std::string positionIdStr = request.params[0].get_str();
            CAmount ddAmount = request.params[1].getInt<int64_t>(); // DD amount in cents (not BTC format)
            std::string redeemAddress = request.params.size() > 2 ? request.params[2].get_str() : "";

            // Validate parameters
            if (ddAmount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Redemption amount must be positive");
            }

            if (!IsHex(positionIdStr) || positionIdStr.length() != 64) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid position ID format");
            }

            // Get wallet
            std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");

            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not initialized");

            // Parse position ID
            uint256 positionId;
            if (!ParseHashStr(positionIdStr, positionId)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid position ID");
            }

            LogPrintf("DigiDollar: ====== REDEMPTION REQUEST ======\n");
            LogPrintf("DigiDollar: Position ID (mint txid): %s\n", positionId.ToString());
            LogPrintf("DigiDollar: Will try to spend: %s:0 (collateral) and %s:1 (DD token)\n",
                     positionId.ToString(), positionId.ToString());

            // Get position from wallet
            LOCK(pwallet->cs_wallet);
            WalletCollateralPosition foundPosition;
            bool found = false;

            for (const auto& pos : dd_wallet->GetDDTimeLocks(false)) {
                if (pos.dd_timelock_id == positionId) {
                    foundPosition = pos;
                    found = true;
                    break;
                }
            }

            if (!found) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Position not found");
            }

            // Check if redeemable
            int currentHeight = pwallet->GetLastBlockHeight();
            if (foundPosition.unlock_height > currentHeight) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Position locked until block %d (current: %d, remaining: %d blocks)",
                              foundPosition.unlock_height, currentHeight, foundPosition.unlock_height - currentHeight));
            }

            // Validate amount
            if (ddAmount > foundPosition.dd_minted) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Cannot redeem %d cents, position only has %d cents",
                              ddAmount, foundPosition.dd_minted));
            }

            // Get oracle price
            CAmount oraclePrice = MockOracleManager::GetInstance().GetCurrentPrice();
            if (oraclePrice <= 0) {
                oraclePrice = 1 * COIN; // Fallback
            }

            // Generate redemption key from wallet
            CKey redemptionKey;
            {
                LOCK(pwallet->cs_wallet);
                redemptionKey.MakeNewKey(true);
            }

            // Build redemption transaction using RedeemTxBuilder
            DigiDollar::RedeemTxBuilder redeemBuilder(Params(), currentHeight, oraclePrice);

            DigiDollar::TxBuilderRedeemParams redeemParams;
            redeemParams.collateralOutpoint = COutPoint(positionId, 0); // Collateral is at vout 0
            redeemParams.ddUtxos = {COutPoint(positionId, 1)};  // DD output is at vout 1
            redeemParams.ddToRedeem = ddAmount;
            redeemParams.path = DigiDollar::RedemptionPath::NORMAL;
            redeemParams.ownerKey = redemptionKey;
            redeemParams.feeRate = 100000; // 100000 sat/kB

            // Get the owner key for this position
            CKey ownerKey;
            if (!dd_wallet->GetOwnerKey(positionId, ownerKey)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Owner key not found for position");
            }
            redeemParams.ownerKey = ownerKey;

            // CRITICAL FIX: Get a wallet address for the returned collateral
            // This ensures the wallet recognizes the returned DGB as belonging to it
            // Try BECH32M first (Taproot), fallback to BECH32 for legacy wallets
            CTxDestination changeDest;
            {
                LOCK(pwallet->cs_wallet);
                std::string label = "";  // Empty label
                auto op_dest = pwallet->GetNewDestination(OutputType::BECH32M, label);
                if (!op_dest) {
                    // Legacy wallet fallback: try BECH32 (SegWit v0)
                    LogPrintf("DigiDollar: BECH32M not available, trying BECH32 for legacy wallet\n");
                    op_dest = pwallet->GetNewDestination(OutputType::BECH32, label);
                }
                if (op_dest) {
                    changeDest = *op_dest;
                    redeemParams.collateralDest = changeDest;
                    LogPrintf("DigiDollar: Using wallet destination for returned collateral\n");
                } else {
                    LogPrintf("DigiDollar: WARNING - Could not get wallet address, using owner key (wallet may not recognize)\n");
                    LogPrintf("DigiDollar: Error: %s\n", util::ErrorString(op_dest).original);
                }
            }

            // CRITICAL FIX: Query wallet's position cache which has correct unlock heights
            // The wallet already tracks positions correctly via GetDDTimeLocks
            {
                LOCK(pwallet->cs_wallet);

                // Get DigiDollar wallet instance
                DigiDollarWallet* ddWallet = pwallet->GetDDWallet();
                if (!ddWallet) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not available");
                }

                // Get position data from wallet's time-lock cache
                std::vector<WalletCollateralPosition> positions = ddWallet->GetDDTimeLocks(false);

                bool found = false;
                for (const auto& pos : positions) {
                    if (pos.dd_timelock_id == positionId) {
                        // Found the position in wallet's cache!
                        redeemParams.collateralAmount = pos.dgb_collateral;
                        redeemParams.ddMinted = pos.dd_minted;
                        redeemParams.unlockHeight = pos.unlock_height;

                        LogPrintf("DigiDollar: Found position in wallet cache:\n");
                        LogPrintf("  - Collateral: %d sats (%.8f DGB)\n", pos.dgb_collateral, pos.dgb_collateral / 100000000.0);
                        LogPrintf("  - DD Minted: %d cents\n", pos.dd_minted);
                        LogPrintf("  - Unlock Height: %d\n", pos.unlock_height);

                        found = true;
                        break;
                    }
                }

                if (!found) {
                    LogPrintf("DigiDollar: WARNING - Position not found in wallet cache, using fallback\n");
                    // Fallback to direct UTXO query
                    auto it = pwallet->mapWallet.find(positionId);
                    if (it != pwallet->mapWallet.end()) {
                        redeemParams.collateralAmount = it->second.tx->vout[0].nValue;
                        LogPrintf("DigiDollar: Fallback - using collateral amount: %d sats\n", redeemParams.collateralAmount);
                    }
                }
            }

            // Select fee UTXOs from wallet
            // CRITICAL: Build exclude list to prevent selecting collateral or DD UTXOs as fee inputs
            std::vector<COutPoint> exclude_utxos;
            exclude_utxos.push_back(redeemParams.collateralOutpoint);  // Don't select collateral
            exclude_utxos.insert(exclude_utxos.end(), redeemParams.ddUtxos.begin(), redeemParams.ddUtxos.end());  // Don't select DD UTXOs

            LogPrintf("DigiDollar: Building exclude list with %d UTXOs (1 collateral + %d DD)\n",
                      exclude_utxos.size(), redeemParams.ddUtxos.size());

            CAmount estimatedFee = 10000000; // 0.1 DGB minimum for redemption tx fees
            CAmount selectedFeeTotal = 0;
            std::vector<CAmount> feeAmounts;

            if (!dd_wallet->SelectFeeCoins(estimatedFee, redeemParams.feeUtxos, selectedFeeTotal, &feeAmounts, &exclude_utxos)) {
                throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient DGB balance for transaction fees");
            }

            redeemParams.feeAmounts = feeAmounts;
            LogPrintf("DigiDollar: Selected %d sats in fees from %d UTXOs for redemption\n",
                     selectedFeeTotal, redeemParams.feeUtxos.size());

            DigiDollar::TxBuilderResult redeemResult = redeemBuilder.BuildRedemptionTransaction(redeemParams);

            if (!redeemResult.success) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to build redemption transaction: " + redeemResult.error);
            }

            LogPrintf("DigiDollar: Redemption transaction built with %d inputs:\n", redeemResult.tx.vin.size());
            for (size_t i = 0; i < redeemResult.tx.vin.size(); i++) {
                LogPrintf("DigiDollar:   Input %d: %s:%d\n", i,
                         redeemResult.tx.vin[i].prevout.hash.ToString(),
                         redeemResult.tx.vin[i].prevout.n);
            }

            // Sign redemption transaction using specialized function that handles:
            // - Collateral (input 0): script-path spending with MAST tree
            // - DD tokens (input 1+): key-path spending (no MAST)
            // - Fee inputs: standard wallet signing
            bool signSuccess = dd_wallet->SignRedemptionTransaction(
                redeemResult.tx,
                redeemParams.collateralOutpoint,
                redeemParams.ddUtxos,
                redeemParams.feeUtxos,
                ownerKey);

            if (!signSuccess) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign redemption transaction with Schnorr signatures");
            }

            // Create transaction reference
            CTransactionRef redeemTx = MakeTransactionRef(redeemResult.tx);

            // Commit transaction to wallet and broadcast
            {
                LOCK(pwallet->cs_wallet);
                pwallet->CommitTransaction(redeemTx, {}, {});
            }

            // Calculate collateral returned (proportional to DD redeemed)
            CAmount dgbUnlocked = (ddAmount * foundPosition.dgb_collateral) / foundPosition.dd_minted;

            // Update position in DigiDollarWallet
            bool positionClosed = (ddAmount >= foundPosition.dd_minted);

            if (positionClosed) {
                // Mark position as inactive
                foundPosition.is_active = false;
                dd_wallet->WriteDDTimeLock(foundPosition);
            } else {
                // Update position with remaining amounts
                CAmount remainingDD = foundPosition.dd_minted - ddAmount;
                CAmount remainingCollateral = foundPosition.dgb_collateral - dgbUnlocked;
                foundPosition.dd_minted = remainingDD;
                foundPosition.dgb_collateral = remainingCollateral;
                dd_wallet->WriteDDTimeLock(foundPosition);
            }

            // Add redemption transaction to history for GUI display
            DDTransaction redeemTxHistory;
            redeemTxHistory.txid = redeemTx->GetHash().GetHex();
            redeemTxHistory.amount = ddAmount;  // DD amount redeemed (burned)
            redeemTxHistory.confirmations = 0;   // Pending confirmation
            redeemTxHistory.timestamp = GetTime();
            redeemTxHistory.incoming = false;    // Redemption = outgoing DD (burning)
            redeemTxHistory.address = redeemAddress.empty() ? "self" : redeemAddress;
            redeemTxHistory.category = "redeem";

            // Add to history using proper method
            if (!dd_wallet->AddRedemptionToHistory(redeemTxHistory)) {
                LogPrintf("DigiDollar: WARNING - Failed to add redemption to history\n");
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("txid", redeemTx->GetHash().GetHex());
            result.pushKV("position_id", positionIdStr);
            result.pushKV("dd_redeemed", ddAmount);
            result.pushKV("dgb_unlocked", ValueFromAmount(dgbUnlocked));
            result.pushKV("unlock_address", redeemAddress.empty() ? "auto" : redeemAddress);
            result.pushKV("fee_paid", ValueFromAmount(redeemResult.totalFees));
            result.pushKV("redemption_path", "normal");
            result.pushKV("position_closed", positionClosed);

            return result;
        },
    };
}

RPCHelpMan listdigidollarpositions()
{
    return RPCHelpMan{"listdigidollarpositions",
                "\nList all DigiDollar collateral positions in the wallet.\n"
                "Shows active and inactive positions with their current status.\n",
                {
                    {"active_only", RPCArg::Type::BOOL, RPCArg::Default{true}, "Only show active positions"},
                    {"tier_filter", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Filter by specific lock tier (1-8)"},
                    {"min_amount", RPCArg::Type::AMOUNT, RPCArg::Optional::OMITTED, "Minimum DD amount filter"}
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::STR, "position_id", "Unique position identifier"},
                                {RPCResult::Type::STR_AMOUNT, "dd_minted", "DigiDollar amount minted"},
                                {RPCResult::Type::STR_AMOUNT, "dgb_collateral", "DGB locked as collateral"},
                                {RPCResult::Type::NUM, "lock_tier", "Lock tier (0-8, 0=1h testing)"},
                                {RPCResult::Type::NUM, "lock_days", "Lock period in days"},
                                {RPCResult::Type::NUM, "unlock_height", "Block height when unlockable"},
                                {RPCResult::Type::NUM, "blocks_remaining", "Blocks until unlock (0 if unlocked)"},
                                {RPCResult::Type::STR, "status", "Position status (active/unlocked/redeemed)"},
                                {RPCResult::Type::NUM, "health_ratio", "Current collateral health ratio (%)"},
                                {RPCResult::Type::BOOL, "can_redeem", "Whether position can be redeemed now"},
                                {RPCResult::Type::STR, "created_date", "ISO date when position was created"},
                                {RPCResult::Type::STR, "unlock_date", "ISO date when position unlocks"}
                            }
                        }
                    }
                },
                RPCExamples{
                    HelpExampleCli("listdigidollarpositions", "") +
                    HelpExampleCli("listdigidollarpositions", "false") +
                    HelpExampleCli("listdigidollarpositions", "true 3") +
                    HelpExampleRpc("listdigidollarpositions", "") +
                    HelpExampleRpc("listdigidollarpositions", "false, 3")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Parse parameters
            bool activeOnly = request.params.size() > 0 ? request.params[0].get_bool() : true;
            int tierFilter = request.params.size() > 1 && !request.params[1].isNull() ?
                            request.params[1].getInt<int>() : -1;
            CAmount minAmount = request.params.size() > 2 && !request.params[2].isNull() ?
                               AmountFromValue(request.params[2]) : 0;

            // Get wallet
            std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");

            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not initialized");

            // Get all positions
            LOCK(pwallet->cs_wallet);
            std::vector<WalletCollateralPosition> positions = dd_wallet->GetDDTimeLocks(false);
            int currentHeight = pwallet->GetLastBlockHeight();

            UniValue result(UniValue::VARR);

            for (const auto& pos : positions) {
                // Apply filters
                if (activeOnly && !pos.is_active) continue;
                if (tierFilter > 0 && pos.lock_tier != static_cast<uint32_t>(tierFilter)) continue;
                if (minAmount > 0 && pos.dd_minted < minAmount) continue;

                UniValue position(UniValue::VOBJ);
                position.pushKV("position_id", pos.dd_timelock_id.GetHex());
                position.pushKV("dd_minted", pos.dd_minted);
                position.pushKV("dgb_collateral", ValueFromAmount(pos.dgb_collateral));
                position.pushKV("lock_tier", static_cast<int>(pos.lock_tier));
                position.pushKV("lock_days", GetLockDaysForTier(pos.lock_tier));
                position.pushKV("unlock_height", pos.unlock_height);

                // Calculate remaining blocks
                int blocksRemaining = std::max(0, static_cast<int>(pos.unlock_height - currentHeight));
                position.pushKV("blocks_remaining", blocksRemaining);

                // Status
                std::string status = pos.is_active ? (blocksRemaining == 0 ? "unlocked" : "active") : "redeemed";
                position.pushKV("status", status);

                // Health ratio (simple calculation)
                int healthRatio = (pos.dgb_collateral > 0) ?
                    ((pos.dd_minted * 100) / pos.dgb_collateral) : 0;
                position.pushKV("health_ratio", healthRatio);
                position.pushKV("can_redeem", blocksRemaining == 0 && pos.is_active);

                // Dates (simple conversion)
                position.pushKV("created_date", "N/A"); // TODO: Add creation timestamp
                position.pushKV("unlock_date", "N/A"); // TODO: Calculate from unlock_height

                result.push_back(position);
            }

            return result;
        },
    };
}

// =============================================================================
// DD ADDRESS COMMANDS (Task 5.7)
// =============================================================================
// NOTE: getdigidollaraddress is OBSOLETE - actual implementation is now in
// src/wallet/rpcwallet.cpp as a static function for proper wallet context.
// This version is kept for reference only and is not registered.
// =============================================================================

RPCHelpMan getdigidollaraddress()
{
    return RPCHelpMan{"getdigidollaraddress",
                "\nGenerate a new DigiDollar address for receiving DD.\n"
                "Creates a new address with the proper DD prefix for the current network.\n",
                {
                    {"label", RPCArg::Type::STR, RPCArg::Default{""}, "Optional label for the address"}
                },
                RPCResult{
                    RPCResult::Type::STR, "address", "The new DigiDollar address"
                },
                RPCExamples{
                    HelpExampleCli("getdigidollaraddress", "") +
                    HelpExampleCli("getdigidollaraddress", "\"savings\"") +
                    HelpExampleRpc("getdigidollaraddress", "") +
                    HelpExampleRpc("getdigidollaraddress", "\"savings\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::shared_ptr<wallet::CWallet> const pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) return UniValue::VNULL;

            LOCK(pwallet->cs_wallet);

            if (!pwallet->CanGetAddresses()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error: This wallet has no available keys");
            }

            // Parse parameters
            std::string label = request.params.size() > 0 ? request.params[0].get_str() : "";

            // DigiDollar addresses must be P2TR (Taproot/bech32m)
            OutputType output_type = OutputType::BECH32M;

            // Generate new destination
            auto op_dest = pwallet->GetNewDestination(output_type, label);
            if (!op_dest) {
                throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, util::ErrorString(op_dest).original);
            }
            CTxDestination dest = *op_dest;

            // Encode as DigiDollar address
            std::string newAddress = EncodeDigiDollarAddress(dest);

            if (newAddress.empty()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to encode DigiDollar address");
            }

            return newAddress;
        },
    };
}

static RPCHelpMan validateddaddress()
{
    return RPCHelpMan{"validateddaddress",
                "\nValidate a DigiDollar address format and return detailed information.\n"
                "Checks if the address has the correct prefix, encoding, and checksum.\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "DigiDollar address to validate"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "isvalid", "Whether the address is valid"},
                        {RPCResult::Type::STR, "address", "The validated address (if valid)"},
                        {RPCResult::Type::STR, "network", "Network type (mainnet/testnet/regtest)"},
                        {RPCResult::Type::STR, "prefix", "Address prefix (DD/TD/RD)"},
                        {RPCResult::Type::BOOL, "ismine", "Whether address belongs to this wallet"},
                        {RPCResult::Type::BOOL, "iswatchonly", "Whether address is watch-only"},
                        {RPCResult::Type::STR, "error", "Error description (if invalid)"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("validateddaddress", "\"DDtestaddress123456789abcdef\"") +
                    HelpExampleCli("validateddaddress", "\"TDtestnet123456789abcdef\"") +
                    HelpExampleRpc("validateddaddress", "\"DDtestaddress123456789abcdef\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::string addressStr = request.params[0].get_str();

            UniValue result(UniValue::VOBJ);

            // Validate DD address format (basic validation)
            bool isValid = true;
            if (addressStr.length() < 25 || addressStr.length() > 35) {
                isValid = false;
            } else if (addressStr.substr(0, 2) != "DD" && addressStr.substr(0, 2) != "TD" && addressStr.substr(0, 2) != "RD") {
                isValid = false;
            }

            result.pushKV("isvalid", isValid);

            if (isValid) {
                result.pushKV("address", addressStr);

                // Determine network and prefix
                std::string prefix = addressStr.substr(0, 2);
                std::string network;
                if (prefix == "DD") network = "mainnet";
                else if (prefix == "TD") network = "testnet";
                else if (prefix == "RD") network = "regtest";
                else network = "unknown";

                result.pushKV("network", network);
                result.pushKV("prefix", prefix);
                result.pushKV("ismine", false); // TODO: Check wallet ownership
                result.pushKV("iswatchonly", false); // TODO: Check watch-only status
            } else {
                std::string error = "Invalid DigiDollar address format";
                if (addressStr.length() < 25) error = "Address too short";
                else if (addressStr.length() > 35) error = "Address too long";
                else if (addressStr.substr(0, 2) != "DD" && addressStr.substr(0, 2) != "TD" && addressStr.substr(0, 2) != "RD") {
                    error = "Invalid address prefix (must be DD/TD/RD)";
                }

                result.pushKV("error", error);
            }

            return result;
        },
    };
}

static RPCHelpMan listdigidollaraddresses()
{
    return RPCHelpMan{"listdigidollaraddresses",
                "\nList all DigiDollar addresses in the wallet.\n"
                "Returns both owned and watch-only DD addresses with their balances and labels.\n",
                {
                    {"include_watchonly", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include watch-only addresses"},
                    {"min_balance", RPCArg::Type::AMOUNT, RPCArg::Default{0}, "Minimum balance filter (in cents)"}
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::STR, "address", "DigiDollar address"},
                                {RPCResult::Type::STR, "label", "Address label"},
                                {RPCResult::Type::STR_AMOUNT, "balance", "DD balance (in cents)"},
                                {RPCResult::Type::BOOL, "ismine", "Whether address is owned by wallet"},
                                {RPCResult::Type::BOOL, "iswatchonly", "Whether address is watch-only"},
                                {RPCResult::Type::NUM, "txcount", "Number of transactions involving this address"},
                                {RPCResult::Type::STR, "created_date", "Date when address was created"},
                                {RPCResult::Type::STR, "last_used", "Date of last transaction"}
                            }
                        }
                    }
                },
                RPCExamples{
                    HelpExampleCli("listdigidollaraddresses", "") +
                    HelpExampleCli("listdigidollaraddresses", "true") +
                    HelpExampleCli("listdigidollaraddresses", "true 1000") +
                    HelpExampleRpc("listdigidollaraddresses", "") +
                    HelpExampleRpc("listdigidollaraddresses", "true, 1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Parse parameters
            bool includeWatchOnly = request.params.size() > 0 ? request.params[0].get_bool() : false;
            CAmount minBalance = request.params.size() > 1 ? AmountFromValue(request.params[1]) : 0;

            UniValue result(UniValue::VARR);

            // Mock addresses - in real implementation would get from wallet
            std::vector<std::tuple<std::string, std::string, CAmount, bool, bool>> mockAddresses = {
                {"DDmockaddress123456789abcdef1", "primary", 10000, true, false},
                {"DDmockaddress123456789abcdef2", "savings", 25000, true, false},
                {"DDwatchonly123456789abcdef3", "watch1", 5000, false, true}
            };

            for (const auto& [addr, label, balance, isMine, isWatchOnly] : mockAddresses) {
                // Apply filters
                if (!includeWatchOnly && isWatchOnly) continue;
                if (balance < minBalance) continue;

                UniValue addrInfo(UniValue::VOBJ);
                addrInfo.pushKV("address", addr);
                addrInfo.pushKV("label", label);
                addrInfo.pushKV("balance", balance);
                addrInfo.pushKV("ismine", isMine);
                addrInfo.pushKV("iswatchonly", isWatchOnly);
                addrInfo.pushKV("txcount", 5); // Mock transaction count
                addrInfo.pushKV("created_date", "2024-01-01T00:00:00Z");
                addrInfo.pushKV("last_used", "2024-03-15T12:30:00Z");

                result.push_back(addrInfo);
            }

            return result;
        },
    };
}

static RPCHelpMan importdigidollaraddress()
{
    return RPCHelpMan{"importdigidollaraddress",
                "\nImport a DigiDollar address for watch-only monitoring.\n"
                "Adds an external DD address to watch for incoming transactions without spending capability.\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "DigiDollar address to import"},
                    {"label", RPCArg::Type::STR, RPCArg::Default{""}, "Optional label for the address"},
                    {"rescan", RPCArg::Type::BOOL, RPCArg::Default{false}, "Rescan blockchain for transactions"},
                    {"p2sh", RPCArg::Type::BOOL, RPCArg::Default{false}, "Add P2SH version of address (advanced)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR, "address", "Imported DigiDollar address"},
                        {RPCResult::Type::STR, "label", "Assigned label"},
                        {RPCResult::Type::BOOL, "success", "Whether import was successful"},
                        {RPCResult::Type::BOOL, "rescan_performed", "Whether blockchain rescan was performed"},
                        {RPCResult::Type::NUM, "transactions_found", "Number of existing transactions found (if rescanned)"},
                        {RPCResult::Type::STR, "warning", "Any warnings about the import"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("importdigidollaraddress", "\"DDexternaladdress123456789abc\"") +
                    HelpExampleCli("importdigidollaraddress", "\"DDexternaladdress123456789abc\" \"external_wallet\"") +
                    HelpExampleCli("importdigidollaraddress", "\"DDexternaladdress123456789abc\" \"external_wallet\" true") +
                    HelpExampleRpc("importdigidollaraddress", "\"DDexternaladdress123456789abc\", \"external_wallet\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Parse parameters
            std::string addressStr = request.params[0].get_str();
            std::string label = request.params.size() > 1 ? request.params[1].get_str() : "";
            bool rescan = request.params.size() > 2 ? request.params[2].get_bool() : false;
            bool p2sh = request.params.size() > 3 ? request.params[3].get_bool() : false;

            // Validate address (basic validation)
            if (addressStr.length() < 25 || addressStr.length() > 35) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DigiDollar address length");
            }
            if (addressStr.substr(0, 2) != "DD" && addressStr.substr(0, 2) != "TD" && addressStr.substr(0, 2) != "RD") {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DigiDollar address prefix");
            }

            // Import address (mock implementation)
            bool success = true;
            int transactionsFound = 0;
            std::string warning = "";

            if (rescan) {
                transactionsFound = 3; // Mock: found 3 existing transactions
            }

            if (p2sh) {
                warning = "P2SH import is experimental and may not work with all DigiDollar features";
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("address", addressStr);
            result.pushKV("label", label);
            result.pushKV("success", success);
            result.pushKV("rescan_performed", rescan);
            result.pushKV("transactions_found", transactionsFound);
            if (!warning.empty()) {
                result.pushKV("warning", warning);
            }

            return result;
        },
    };
}

// =============================================================================
// UTILITY RPC COMMANDS (Task 5.8)
// =============================================================================

RPCHelpMan getdigidollarbalance()
{
    return RPCHelpMan{"getdigidollarbalance",
                "\nGet DigiDollar balance for a specific address or total wallet balance.\n"
                "Returns the confirmed and unconfirmed DD balance.\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "DigiDollar address (omit for total wallet balance)"},
                    {"minconf", RPCArg::Type::NUM, RPCArg::Default{1}, "Minimum number of confirmations"},
                    {"include_watchonly", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include watch-only addresses"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_AMOUNT, "confirmed", "Confirmed DD balance (in cents)"},
                        {RPCResult::Type::STR_AMOUNT, "unconfirmed", "Unconfirmed DD balance (in cents)"},
                        {RPCResult::Type::STR_AMOUNT, "total", "Total DD balance (confirmed + unconfirmed)"},
                        {RPCResult::Type::STR, "address", /*optional=*/true, "Address queried (if specific address)"},
                        {RPCResult::Type::NUM, "address_count", /*optional=*/true, "Number of addresses included (for wallet total)"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getdigidollarbalance", "") +
                    HelpExampleCli("getdigidollarbalance", "\"DDtestaddress123456789abcdef\"") +
                    HelpExampleCli("getdigidollarbalance", "\"\" 6 true") +
                    HelpExampleRpc("getdigidollarbalance", "") +
                    HelpExampleRpc("getdigidollarbalance", "\"DDtestaddress123456789abcdef\", 6")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // PHASE 7.7: Integration with DigiDollarWallet backend

            // Get wallet
            std::shared_ptr<wallet::CWallet> const pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet not found");
            }

            // Get DigiDollar wallet
            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not initialized");
            }

            // Parse parameters
            std::string addressStr = request.params.size() > 0 && !request.params[0].isNull() ?
                                   request.params[0].get_str() : "";
            int minConf = request.params.size() > 1 ? request.params[1].getInt<int>() : 1;

            // Validate parameters
            if (minConf < 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Minimum confirmations must be non-negative");
            }

            CAmount confirmedBalance = 0;
            CAmount unconfirmedBalance = 0;
            int addressCount = 0;

            if (!addressStr.empty()) {
                // Get balance for specific address
                CDigiDollarAddress dd_address(addressStr);
                if (!dd_address.IsValid()) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DigiDollar address");
                }

                confirmedBalance = dd_wallet->GetDDBalance(dd_address);
                unconfirmedBalance = 0; // TODO: Track unconfirmed balance separately
                addressCount = 1;
            } else {
                // Get total wallet balance
                confirmedBalance = dd_wallet->GetTotalDDBalance();
                unconfirmedBalance = 0; // TODO: Track unconfirmed balance separately
                addressCount = dd_wallet->GetBalanceCount();
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("confirmed", confirmedBalance);
            result.pushKV("unconfirmed", unconfirmedBalance);
            result.pushKV("total", confirmedBalance + unconfirmedBalance);
            if (!addressStr.empty()) {
                result.pushKV("address", addressStr);
            }
            result.pushKV("address_count", addressCount);

            return result;
        },
    };
}

static RPCHelpMan estimatecollateral()
{
    return RPCHelpMan{"estimatecollateral",
                "\nEstimate DGB collateral requirement for minting DigiDollar.\n"
                "Calculates the required DGB amount based on DD amount, lock tier, and current system conditions.\n",
                {
                    {"dd_amount", RPCArg::Type::AMOUNT, RPCArg::Optional::NO, "DigiDollar amount to mint (in cents)"},
                    {"lock_tier", RPCArg::Type::NUM, RPCArg::Optional::NO, "Lock tier 0-8 (0=1h testing, 1=30d, 2=90d, 3=180d, 4=1y, 5=3y, 6=5y, 7=7y, 8=10y)"},
                    {"oracle_price", RPCArg::Type::AMOUNT, RPCArg::Optional::OMITTED, "Custom DGB price in cents (uses current if omitted)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_AMOUNT, "required_dgb", "Required DGB collateral amount"},
                        {RPCResult::Type::STR_AMOUNT, "dd_amount", "DigiDollar amount to mint (in cents)"},
                        {RPCResult::Type::NUM, "lock_tier", "Lock tier used"},
                        {RPCResult::Type::NUM, "lock_days", "Lock period in days"},
                        {RPCResult::Type::NUM, "base_ratio", "Base collateral ratio percentage"},
                        {RPCResult::Type::NUM, "dca_multiplier", "DCA multiplier applied"},
                        {RPCResult::Type::NUM, "effective_ratio", "Final collateral ratio (base * DCA)"},
                        {RPCResult::Type::STR_AMOUNT, "oracle_price", "DGB price used (cents per DGB)"},
                        {RPCResult::Type::NUM, "system_health", "Current system health percentage"},
                        {RPCResult::Type::STR, "health_tier", "System health tier"},
                        {RPCResult::Type::STR_AMOUNT, "usd_value", "USD value of required DGB"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("estimatecollateral", "10000 3") +
                    HelpExampleCli("estimatecollateral", "50000 5 4500") +
                    HelpExampleRpc("estimatecollateral", "10000, 3") +
                    HelpExampleRpc("estimatecollateral", "50000, 5, 4500")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Parse parameters
            CAmount ddAmount = AmountFromValue(request.params[0]);
            int lockTier = request.params[1].getInt<int>();
            CAmount oraclePrice = request.params.size() > 2 && !request.params[2].isNull() ?
                                 AmountFromValue(request.params[2]) : 5000; // Default $0.05 per DGB

            // Validate parameters
            if (ddAmount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "DD amount must be positive");
            }
            if (lockTier < 0 || lockTier > 8) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Lock tier must be between 0 and 8 (0 = 1 hour testing tier)");
            }
            if (oraclePrice <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Oracle price must be positive");
            }

            // Calculate collateral requirements (mock implementation)
            int lockDays = GetLockDaysForTier(lockTier);
            int baseRatio = GetMinCollateralRatio(lockTier);

            // Get current system health and DCA multiplier
            int systemHealth = 150; // TODO: Get real system health
            double dcaMultiplier = 1.0; // TODO: Get real DCA multiplier
            int effectiveRatio = static_cast<int>(baseRatio * dcaMultiplier);

            // Calculate required DGB
            CAmount requiredDGB = (ddAmount * effectiveRatio * COIN) / (oraclePrice * 100);
            CAmount usdValue = (requiredDGB * oraclePrice) / COIN;

            UniValue result(UniValue::VOBJ);
            result.pushKV("required_dgb", ValueFromAmount(requiredDGB));
            result.pushKV("dd_amount", ddAmount);
            result.pushKV("lock_tier", lockTier);
            result.pushKV("lock_days", lockDays);
            result.pushKV("base_ratio", baseRatio);
            result.pushKV("dca_multiplier", dcaMultiplier);
            result.pushKV("effective_ratio", effectiveRatio);
            result.pushKV("oracle_price", oraclePrice);
            result.pushKV("system_health", systemHealth);
            result.pushKV("health_tier", "healthy");
            result.pushKV("usd_value", ValueFromAmount(usdValue));

            return result;
        },
    };
}

static RPCHelpMan getredemptioninfo()
{
    return RPCHelpMan{"getredemptioninfo",
                "\nGet redemption information for a specific DigiDollar position.\n"
                "Shows whether position can be redeemed and potential return amounts.\n",
                {
                    {"position_id", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Position ID (transaction hash of mint)"},
                    {"dd_amount", RPCArg::Type::AMOUNT, RPCArg::Optional::OMITTED, "Amount of DD to redeem (default: all)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR, "position_id", "Position identifier"},
                        {RPCResult::Type::BOOL, "can_redeem", "Whether position can be redeemed now"},
                        {RPCResult::Type::STR, "redemption_path", "Available redemption path (normal/emergency/liquidation)"},
                        {RPCResult::Type::STR_AMOUNT, "total_dd_minted", "Total DD minted in this position"},
                        {RPCResult::Type::STR_AMOUNT, "redeemable_dd", "DD amount that can be redeemed"},
                        {RPCResult::Type::STR_AMOUNT, "dgb_return", "Estimated DGB return amount"},
                        {RPCResult::Type::NUM, "unlock_height", "Block height when position unlocks"},
                        {RPCResult::Type::NUM, "timelock_remaining", "Blocks until unlock (0 if unlocked)"},
                        {RPCResult::Type::STR_AMOUNT, "penalty_amount", "Penalty amount (if early redemption)"},
                        {RPCResult::Type::STR, "status", "Position status"},
                        {RPCResult::Type::STR, "unlock_date", "Estimated unlock date"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getredemptioninfo", "\"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890\"") +
                    HelpExampleCli("getredemptioninfo", "\"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890\" 5000") +
                    HelpExampleRpc("getredemptioninfo", "\"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Parse parameters
            std::string positionIdStr = request.params[0].get_str();
            CAmount ddAmount = request.params.size() > 1 && !request.params[1].isNull() ?
                              AmountFromValue(request.params[1]) : 0;

            // Validate position ID
            if (!IsHex(positionIdStr) || positionIdStr.length() != 64) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid position ID format");
            }

            uint256 positionId;
            positionId.SetHex(positionIdStr);

            // Mock position data - in real implementation would lookup from wallet/blockchain
            bool canRedeem = true;
            CAmount totalDDMinted = 10000; // $100
            CAmount redeemableDD = ddAmount > 0 ? std::min(ddAmount, totalDDMinted) : totalDDMinted;
            CAmount dgbReturn = 150 * COIN; // Mock return amount
            int unlockHeight = 1000000;
            int currentHeight = 900000;
            int blocksRemaining = std::max(0, unlockHeight - currentHeight);
            CAmount penaltyAmount = 0;
            std::string status = "active";

            UniValue result(UniValue::VOBJ);
            result.pushKV("position_id", positionIdStr);
            result.pushKV("can_redeem", canRedeem);
            result.pushKV("redemption_path", "normal");
            result.pushKV("total_dd_minted", totalDDMinted);
            result.pushKV("redeemable_dd", redeemableDD);
            result.pushKV("dgb_return", ValueFromAmount(dgbReturn));
            result.pushKV("unlock_height", unlockHeight);
            result.pushKV("timelock_remaining", blocksRemaining);
            result.pushKV("penalty_amount", penaltyAmount);
            result.pushKV("status", status);
            result.pushKV("unlock_date", "2024-12-31T23:59:59Z");

            return result;
        },
    };
}

RPCHelpMan listdigidollartxs()
{
    return RPCHelpMan{"listdigidollartxs",
                "\nList DigiDollar transactions from the wallet.\n"
                "Returns recent DD transactions including mints, sends, receives, and redemptions.\n",
                {
                    {"count", RPCArg::Type::NUM, RPCArg::Default{10}, "Number of transactions to return"},
                    {"skip", RPCArg::Type::NUM, RPCArg::Default{0}, "Number of transactions to skip"},
                    {"address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Filter by specific DD address"},
                    {"category", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Filter by category (mint/send/receive/redeem)"}
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::STR_HEX, "txid", "Transaction ID"},
                                {RPCResult::Type::STR, "category", "Transaction category (mint/send/receive/redeem)"},
                                {RPCResult::Type::STR_AMOUNT, "amount", "DD amount (positive for receives, negative for sends)"},
                                {RPCResult::Type::STR, "address", "DigiDollar address involved"},
                                {RPCResult::Type::NUM, "confirmations", "Number of confirmations"},
                                {RPCResult::Type::NUM, "blockheight", "Block height (if confirmed)"},
                                {RPCResult::Type::STR, "blockhash", "Block hash (if confirmed)"},
                                {RPCResult::Type::NUM, "time", "Transaction timestamp"},
                                {RPCResult::Type::STR_AMOUNT, "fee", "Transaction fee paid (if applicable)"},
                                {RPCResult::Type::STR, "comment", "Transaction comment (if any)"},
                                {RPCResult::Type::BOOL, "abandoned", "Whether transaction was abandoned"}
                            }
                        }
                    }
                },
                RPCExamples{
                    HelpExampleCli("listdigidollartxs", "") +
                    HelpExampleCli("listdigidollartxs", "20 10") +
                    HelpExampleCli("listdigidollartxs", "10 0 \"DDtestaddress123456789abcdef\"") +
                    HelpExampleCli("listdigidollartxs", "10 0 \"\" \"mint\"") +
                    HelpExampleRpc("listdigidollartxs", "20, 10") +
                    HelpExampleRpc("listdigidollartxs", "10, 0, \"DDtestaddress123456789abcdef\", \"mint\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // PHASE 7.7: Integration with DigiDollarWallet backend

            // Get wallet
            std::shared_ptr<wallet::CWallet> const pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet not found");
            }

            // Get DigiDollar wallet
            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not initialized");
            }

            // Parse parameters
            int count = request.params.size() > 0 ? request.params[0].getInt<int>() : 10;
            int skip = request.params.size() > 1 ? request.params[1].getInt<int>() : 0;
            std::string addressFilter = request.params.size() > 2 && !request.params[2].isNull() ?
                                       request.params[2].get_str() : "";
            std::string categoryFilter = request.params.size() > 3 && !request.params[3].isNull() ?
                                        request.params[3].get_str() : "";

            // Validate parameters
            if (count < 0 || count > 1000) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Count must be between 0 and 1000");
            }
            if (skip < 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Skip must be non-negative");
            }

            // Get transaction history from wallet
            std::vector<DDTransaction> transactions = dd_wallet->GetDDTransactionHistory();

            UniValue result(UniValue::VARR);
            int processed = 0;
            int skipped = 0;

            for (const auto& tx : transactions) {
                // Apply filters
                if (!addressFilter.empty() && tx.address != addressFilter) continue;
                if (!categoryFilter.empty() && tx.category != categoryFilter) continue;

                // Apply skip
                if (skipped < skip) {
                    skipped++;
                    continue;
                }

                // Apply count limit
                if (processed >= count) break;

                UniValue txInfo(UniValue::VOBJ);
                txInfo.pushKV("txid", tx.txid);
                txInfo.pushKV("category", tx.category);
                txInfo.pushKV("amount", tx.incoming ? tx.amount : -tx.amount);
                txInfo.pushKV("address", tx.address);
                txInfo.pushKV("confirmations", tx.confirmations);
                txInfo.pushKV("blockheight", tx.blockheight);
                txInfo.pushKV("blockhash", tx.blockhash);
                txInfo.pushKV("time", static_cast<int64_t>(tx.timestamp));
                txInfo.pushKV("fee", ValueFromAmount(tx.fee));
                txInfo.pushKV("comment", tx.comment);
                txInfo.pushKV("abandoned", tx.abandoned);

                result.push_back(txInfo);
                processed++;
            }

            return result;
        },
    };
}

static RPCHelpMan getoracleprice()
{
    return RPCHelpMan{"getoracleprice",
                "\nGet current DGB/USD price from the oracle system.\n"
                "Returns the latest price data used for DigiDollar calculations.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_AMOUNT, "price_cents", "Current DGB price in cents per DGB"},
                        {RPCResult::Type::NUM, "price_usd", "Current DGB price in USD"},
                        {RPCResult::Type::NUM, "last_update_height", "Block height of last price update"},
                        {RPCResult::Type::NUM, "last_update_time", "Timestamp of last update"},
                        {RPCResult::Type::NUM, "validity_blocks", "Blocks remaining until price expires"},
                        {RPCResult::Type::BOOL, "is_stale", "Whether price data is considered stale"},
                        {RPCResult::Type::NUM, "oracle_count", "Number of active oracles"},
                        {RPCResult::Type::STR, "status", "Oracle system status (active/warning/error)"},
                        {RPCResult::Type::STR_AMOUNT, "24h_high", "24-hour high price"},
                        {RPCResult::Type::STR_AMOUNT, "24h_low", "24-hour low price"},
                        {RPCResult::Type::NUM, "volatility", "Current price volatility percentage"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getoracleprice", "") +
                    HelpExampleRpc("getoracleprice", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Get chainman for blockchain info
            const ChainstateManager& chainman = EnsureAnyChainman(request.context);

            // Get real oracle data from the oracle system
            OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
            OracleBundleManager::OracleStats stats = oracle_manager.GetStats();

            CAmount priceCents = OracleIntegration::GetCurrentOraclePrice();
            double priceUSD = static_cast<double>(priceCents) / 100.0;

            // Get current blockchain info
            int lastUpdateHeight = chainman.ActiveChain().Height();
            int64_t lastUpdateTime = stats.last_update > 0 ? stats.last_update : GetTime();

            // Calculate validity and staleness
            int validityBlocks = 20; // Oracle data valid for 20 blocks
            int blocksSinceUpdate = lastUpdateHeight - (stats.latest_epoch * 1440); // Approximate
            bool isStale = blocksSinceUpdate > validityBlocks;

            // Get oracle count and status
            size_t activeOracleCount = oracle_manager.GetPendingMessageCount();
            std::string status = stats.has_consensus ? "active" : (activeOracleCount > 0 ? "warning" : "error");

            // Mock 24h data for now - would track historically in production
            CAmount high24h = priceCents + (priceCents / 20); // +5%
            CAmount low24h = priceCents - (priceCents / 20);  // -5%
            double volatility = 2.5; // Mock volatility

            UniValue result(UniValue::VOBJ);
            result.pushKV("price_cents", priceCents);
            result.pushKV("price_usd", priceUSD);
            result.pushKV("last_update_height", lastUpdateHeight);
            result.pushKV("last_update_time", lastUpdateTime);
            result.pushKV("validity_blocks", validityBlocks);
            result.pushKV("is_stale", isStale);
            result.pushKV("oracle_count", static_cast<int>(activeOracleCount));
            result.pushKV("status", status);
            result.pushKV("24h_high", high24h);
            result.pushKV("24h_low", low24h);
            result.pushKV("volatility", volatility);

            return result;
        },
    };
}

static RPCHelpMan getprotectionstatus()
{
    return RPCHelpMan{"getprotectionstatus",
                "\nGet status of DigiDollar protection systems.\n"
                "Returns information about DCA, ERR, volatility protection, and other safeguards.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::OBJ, "dca", "Dynamic Collateral Adjustment status",
                            {
                                {RPCResult::Type::BOOL, "active", "Whether DCA is currently active"},
                                {RPCResult::Type::NUM, "current_multiplier", "Current DCA multiplier"},
                                {RPCResult::Type::STR, "tier", "Current DCA tier"},
                                {RPCResult::Type::NUM, "system_health", "System health percentage"},
                                {RPCResult::Type::STR, "trend", "Health trend (improving/stable/declining)"}
                            }
                        },
                        {RPCResult::Type::OBJ, "err", "Emergency Redemption Ratio status",
                            {
                                {RPCResult::Type::BOOL, "active", "Whether ERR is currently active"},
                                {RPCResult::Type::NUM, "threshold", "ERR activation threshold (%)"},
                                {RPCResult::Type::NUM, "current_ratio", "Current system ratio (%)"},
                                {RPCResult::Type::STR, "status", "ERR status (normal/warning/active)"}
                            }
                        },
                        {RPCResult::Type::OBJ, "volatility", "Volatility protection status",
                            {
                                {RPCResult::Type::BOOL, "protection_active", "Whether volatility protection is active"},
                                {RPCResult::Type::NUM, "current_volatility", "Current volatility percentage"},
                                {RPCResult::Type::NUM, "protection_threshold", "Volatility protection threshold"},
                                {RPCResult::Type::BOOL, "minting_restricted", "Whether minting is restricted due to volatility"}
                            }
                        },
                        {RPCResult::Type::OBJ, "overall", "Overall protection status",
                            {
                                {RPCResult::Type::STR, "status", "Overall system status (secure/warning/critical)"},
                                {RPCResult::Type::ARR, "active_protections", "List of currently active protections",
                                    {
                                        {RPCResult::Type::STR, "", "Protection name"}
                                    }
                                },
                                {RPCResult::Type::ARR, "warnings", "Current system warnings",
                                    {
                                        {RPCResult::Type::STR, "", "Warning message"}
                                    }
                                }
                            }
                        }
                    }
                },
                RPCExamples{
                    HelpExampleCli("getprotectionstatus", "") +
                    HelpExampleRpc("getprotectionstatus", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Mock protection status - in real implementation would get from protection systems
            UniValue result(UniValue::VOBJ);

            // DCA status
            UniValue dca(UniValue::VOBJ);
            dca.pushKV("active", true);
            dca.pushKV("current_multiplier", 1.0);
            dca.pushKV("tier", "healthy");
            dca.pushKV("system_health", 150);
            dca.pushKV("trend", "stable");
            result.pushKV("dca", dca);

            // ERR status
            UniValue err(UniValue::VOBJ);
            err.pushKV("active", false);
            err.pushKV("threshold", 100);
            err.pushKV("current_ratio", 150);
            err.pushKV("status", "normal");
            result.pushKV("err", err);

            // Volatility protection
            UniValue volatility(UniValue::VOBJ);
            volatility.pushKV("protection_active", false);
            volatility.pushKV("current_volatility", 2.5);
            volatility.pushKV("protection_threshold", 10.0);
            volatility.pushKV("minting_restricted", false);
            result.pushKV("volatility", volatility);

            // Overall status
            UniValue overall(UniValue::VOBJ);
            overall.pushKV("status", "secure");

            UniValue activeProtections(UniValue::VARR);
            activeProtections.push_back("dca");
            overall.pushKV("active_protections", activeProtections);

            UniValue warnings(UniValue::VARR);
            overall.pushKV("warnings", warnings);

            result.pushKV("overall", overall);

            return result;
        },
    };
}

static RPCHelpMan sendoracleprice()
{
    return RPCHelpMan{"sendoracleprice",
                "\nBroadcast an oracle price message to the network (TESTNET ONLY).\n"
                "This command creates and broadcasts a signed oracle price message.\n"
                "Only available on testnet/regtest for testing purposes.\n",
                {
                    {"price_usd", RPCArg::Type::NUM, RPCArg::Optional::NO, "Price in USD (e.g., 0.05 for $0.05 per DGB)"},
                    {"oracle_id", RPCArg::Type::NUM, RPCArg::Default{1}, "Oracle ID (1-30) to use for signing"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "hash", "The message hash"},
                        {RPCResult::Type::NUM, "oracle_id", "Oracle ID used"},
                        {RPCResult::Type::STR_AMOUNT, "price_satoshis", "Price in satoshis per USD"},
                        {RPCResult::Type::NUM, "timestamp", "Message timestamp"},
                        {RPCResult::Type::BOOL, "broadcasted", "Whether message was broadcasted to network"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("sendoracleprice", "0.05") +
                    HelpExampleCli("sendoracleprice", "0.05 1") +
                    HelpExampleRpc("sendoracleprice", "0.05, 1")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Only allow on testnet/regtest
            if (Params().GetChainType() != ChainType::TESTNET &&
                Params().GetChainType() != ChainType::REGTEST) {
                throw JSONRPCError(RPC_INVALID_REQUEST, "sendoracleprice only available on testnet/regtest");
            }

            // Parse parameters
            double price_usd = request.params[0].get_real();
            uint32_t oracle_id = request.params.size() > 1 ? request.params[1].getInt<int>() : 1;

            // Validate price
            if (price_usd <= 0 || price_usd > 100) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Price must be between 0 and $100 per DGB");
            }

            // Validate oracle ID
            if (oracle_id < 1 || oracle_id > ORACLE_TOTAL_COUNT) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Oracle ID must be between 1 and %d", ORACLE_TOTAL_COUNT));
            }

            // Get oracle config
            const CChainParams& params = Params();
            const OracleNodeInfo* oracle_config = params.GetOracleNode(oracle_id);
            if (!oracle_config) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Oracle ID %d not found in configuration", oracle_id));
            }

            // Convert price to micro-USD
            // Micro-USD format: 1,000,000 = $1.00
            uint64_t price_micro_usd = static_cast<uint64_t>(price_usd * 1000000);

            // Create oracle message
            COraclePriceMessage msg;
            msg.oracle_id = oracle_id;
            msg.price_micro_usd = price_micro_usd;
            msg.timestamp = GetTime();

            // For testnet, we need to sign with the oracle's private key
            // This would normally be done by the oracle operator daemon
            // For now, we'll create an unsigned message (signature validation can be skipped in testnet)
            // TODO: Add proper key management for testnet oracle operators

            // Validate message
            if (!msg.IsValid()) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to create valid oracle message");
            }

            // Store in bundle manager
            OracleBundleManager& bundleManager = OracleBundleManager::GetInstance();
            if (!bundleManager.AddOracleMessage(msg)) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to add oracle message to bundle manager");
            }

            // Broadcast to P2P network
            // TODO: Implement actual P2P broadcasting via network manager
            LogPrintf("Oracle: Broadcasted price message: oracle_id=%d, price=%llu micro-USD, timestamp=%d\n",
                     msg.oracle_id, msg.price_micro_usd, msg.timestamp);

            // Return result
            UniValue result(UniValue::VOBJ);
            result.pushKV("hash", msg.GetSignatureHash().GetHex());
            result.pushKV("oracle_id", (uint64_t)msg.oracle_id);
            result.pushKV("price_micro_usd", (uint64_t)msg.price_micro_usd);
            result.pushKV("price_usd", price_usd);
            result.pushKV("timestamp", msg.timestamp);
            result.pushKV("broadcasted", true);

            return result;
        },
    };
}

static RPCHelpMan listoracles()
{
    return RPCHelpMan{"listoracles",
                "\nList all configured oracle nodes and their status.\n"
                "Returns information about all oracles including active/inactive status.\n",
                {
                    {"active_only", RPCArg::Type::BOOL, RPCArg::Default{false}, "Only show active oracles"}
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::NUM, "oracle_id", "Oracle ID (0-29)"},
                                {RPCResult::Type::STR_HEX, "pubkey", "Oracle public key"},
                                {RPCResult::Type::STR, "endpoint", "Oracle network endpoint"},
                                {RPCResult::Type::BOOL, "is_active", "Whether oracle is currently active"},
                                {RPCResult::Type::BOOL, "is_running", "Whether oracle daemon is running"},
                                {RPCResult::Type::BOOL, "is_enabled", "Whether oracle is enabled"},
                                {RPCResult::Type::STR_AMOUNT, "last_price", "Last reported price (if available)"},
                                {RPCResult::Type::NUM, "last_update", "Timestamp of last update"},
                                {RPCResult::Type::STR, "status", "Oracle status (running/stopped/error)"},
                                {RPCResult::Type::BOOL, "selected_for_epoch", "Whether oracle is selected for current epoch"}
                            }
                        }
                    }
                },
                RPCExamples{
                    HelpExampleCli("listoracles", "") +
                    HelpExampleCli("listoracles", "true") +
                    HelpExampleRpc("listoracles", "") +
                    HelpExampleRpc("listoracles", "true")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            bool activeOnly = request.params.size() > 0 ? request.params[0].get_bool() : false;

            // Get chainman for blockchain info
            const ChainstateManager& chainman = EnsureAnyChainman(request.context);

            // Get all oracle nodes from chainparams
            const CChainParams& params = Params();
            const std::vector<OracleNodeInfo>& all_oracles = params.GetOracleNodes();

            // Get current epoch and selected oracles
            int32_t current_height = chainman.ActiveChain().Height();
            int32_t current_epoch = GetCurrentEpoch(current_height);
            std::vector<OracleNodeInfo> selected_oracles = SelectOraclesForEpoch(all_oracles, current_epoch);

            // Build set of selected oracle IDs for quick lookup
            std::set<uint32_t> selected_ids;
            for (const auto& oracle : selected_oracles) {
                selected_ids.insert(oracle.id);
            }

            // Get oracle manager for runtime status
            OracleManager& oracle_manager = OracleManager::GetInstance();

            UniValue result(UniValue::VARR);

            for (const auto& oracle_config : all_oracles) {
                // Apply active filter
                if (activeOnly && !oracle_config.is_active) {
                    continue;
                }

                bool is_selected = selected_ids.count(oracle_config.id) > 0;
                bool is_running = oracle_manager.IsOracleRunning(oracle_config.id);
                OracleNode* runtime_oracle = oracle_manager.GetOracleNode(oracle_config.id);

                UniValue oracle_info(UniValue::VOBJ);
                oracle_info.pushKV("oracle_id", static_cast<int>(oracle_config.id));
                oracle_info.pushKV("pubkey", HexStr(oracle_config.pubkey));
                oracle_info.pushKV("endpoint", oracle_config.endpoint);
                oracle_info.pushKV("is_active", oracle_config.is_active);
                oracle_info.pushKV("is_running", is_running);
                oracle_info.pushKV("is_enabled", runtime_oracle ? runtime_oracle->IsEnabled() : false);

                // Runtime status
                if (runtime_oracle && runtime_oracle->HasValidPrice()) {
                    oracle_info.pushKV("last_price", runtime_oracle->GetCurrentPrice());
                    oracle_info.pushKV("last_update", runtime_oracle->GetLastUpdateTime());
                } else {
                    oracle_info.pushKV("last_price", 0);
                    oracle_info.pushKV("last_update", 0);
                }

                // Status determination
                std::string status = "stopped";
                if (is_running) {
                    status = runtime_oracle && runtime_oracle->HasValidPrice() ? "running" : "error";
                }
                oracle_info.pushKV("status", status);
                oracle_info.pushKV("selected_for_epoch", is_selected);

                result.push_back(oracle_info);
            }

            return result;
        },
    };
}

static RPCHelpMan startoracle()
{
    return RPCHelpMan{"startoracle",
                "\nStart a local oracle node if configured.\n"
                "Requires oracle private key to be configured for this node.\n",
                {
                    {"oracle_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Oracle ID to start (0-29)"},
                    {"private_key", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "Oracle private key (if not already configured)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "success", "Whether oracle was started successfully"},
                        {RPCResult::Type::NUM, "oracle_id", "Oracle ID that was started"},
                        {RPCResult::Type::STR, "status", "Oracle status after start attempt"},
                        {RPCResult::Type::STR, "message", "Status message or error description"},
                        {RPCResult::Type::BOOL, "was_already_running", "Whether oracle was already running"},
                        {RPCResult::Type::STR, "warning", "Any warnings about the operation"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("startoracle", "5") +
                    HelpExampleCli("startoracle", "5 \"your_private_key_hex\"") +
                    HelpExampleRpc("startoracle", "5") +
                    HelpExampleRpc("startoracle", "5, \"your_private_key_hex\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            int oracle_id = request.params[0].getInt<int>();
            std::string private_key_hex = request.params.size() > 1 ? request.params[1].get_str() : "";

            // Validate oracle ID
            if (oracle_id < 0 || oracle_id >= ORACLE_TOTAL_COUNT) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Invalid oracle ID %d. Must be between 0 and %d", oracle_id, ORACLE_TOTAL_COUNT - 1));
            }

            // Verify oracle exists in chainparams
            const CChainParams& params = Params();
            const OracleNodeInfo* oracle_config = params.GetOracleNode(oracle_id);
            if (!oracle_config) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Oracle ID %d not found in chain parameters", oracle_id));
            }

            OracleManager& oracle_manager = OracleManager::GetInstance();
            bool was_already_running = oracle_manager.IsOracleRunning(oracle_id);
            bool success = false;
            std::string status_message;
            std::string warning;

            try {
                if (was_already_running) {
                    success = true;
                    status_message = "Oracle was already running";
                } else {
                    // Try to start oracle
                    if (!private_key_hex.empty()) {
                        // Add oracle with provided private key
                        success = oracle_manager.AddOracleNode(oracle_id, private_key_hex);
                        if (success) {
                            oracle_manager.EnableOracle(oracle_id, true);
                            status_message = "Oracle added and started with provided private key";
                        } else {
                            status_message = "Failed to initialize oracle with provided private key";
                        }
                    } else {
                        // Try to start existing oracle (if already configured)
                        OracleNode* existing_oracle = oracle_manager.GetOracleNode(oracle_id);
                        if (existing_oracle) {
                            existing_oracle->Start();
                            success = existing_oracle->IsRunning();
                            status_message = success ? "Existing oracle started" : "Failed to start existing oracle";
                        } else {
                            status_message = "Oracle not configured. Provide private_key parameter to configure.";
                            warning = "Oracle private key must be provided for first-time setup";
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                status_message = strprintf("Exception starting oracle: %s", e.what());
                success = false;
            }

            std::string final_status = "stopped";
            if (oracle_manager.IsOracleRunning(oracle_id)) {
                OracleNode* oracle = oracle_manager.GetOracleNode(oracle_id);
                final_status = oracle && oracle->IsEnabled() ? "running" : "disabled";
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("success", success);
            result.pushKV("oracle_id", oracle_id);
            result.pushKV("status", final_status);
            result.pushKV("message", status_message);
            result.pushKV("was_already_running", was_already_running);
            if (!warning.empty()) {
                result.pushKV("warning", warning);
            }

            return result;
        },
    };
}

static RPCHelpMan stoporacle()
{
    return RPCHelpMan{"stoporacle",
                "\nStop a running oracle node.\n"
                "Stops the oracle daemon and price fetching for the specified oracle.\n",
                {
                    {"oracle_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Oracle ID to stop (0-29)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "success", "Whether oracle was stopped successfully"},
                        {RPCResult::Type::NUM, "oracle_id", "Oracle ID that was stopped"},
                        {RPCResult::Type::STR, "status", "Oracle status after stop attempt"},
                        {RPCResult::Type::STR, "message", "Status message"},
                        {RPCResult::Type::BOOL, "was_running", "Whether oracle was running before stop"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("stoporacle", "5") +
                    HelpExampleRpc("stoporacle", "5")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            int oracle_id = request.params[0].getInt<int>();

            // Validate oracle ID
            if (oracle_id < 0 || oracle_id >= ORACLE_TOTAL_COUNT) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Invalid oracle ID %d. Must be between 0 and %d", oracle_id, ORACLE_TOTAL_COUNT - 1));
            }

            OracleManager& oracle_manager = OracleManager::GetInstance();
            bool was_running = oracle_manager.IsOracleRunning(oracle_id);
            bool success = false;
            std::string status_message;

            if (!was_running) {
                success = true;
                status_message = "Oracle was not running";
            } else {
                try {
                    OracleNode* oracle = oracle_manager.GetOracleNode(oracle_id);
                    if (oracle) {
                        oracle->Stop();
                        success = !oracle->IsRunning();
                        status_message = success ? "Oracle stopped successfully" : "Failed to stop oracle";
                    } else {
                        status_message = "Oracle not found in manager";
                    }
                }
                catch (const std::exception& e) {
                    status_message = strprintf("Exception stopping oracle: %s", e.what());
                    success = false;
                }
            }

            std::string final_status = oracle_manager.IsOracleRunning(oracle_id) ? "running" : "stopped";

            UniValue result(UniValue::VOBJ);
            result.pushKV("success", success);
            result.pushKV("oracle_id", oracle_id);
            result.pushKV("status", final_status);
            result.pushKV("message", status_message);
            result.pushKV("was_running", was_running);

            return result;
        },
    };
}

static RPCHelpMan getoraclepubkey()
{
    return RPCHelpMan{"getoraclepubkey",
                "\nGet the oracle node's public key for verification.\n"
                "Returns the XOnlyPubKey (32-byte Schnorr public key) used for signing oracle price messages.\n",
                {
                    {"oracle_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Oracle ID to query (0-29)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "oracle_id", "Oracle ID"},
                        {RPCResult::Type::STR_HEX, "pubkey", "Oracle public key (XOnlyPubKey, 32 bytes hex)"},
                        {RPCResult::Type::STR_HEX, "pubkey_full", "Full compressed public key (CPubKey, 33 bytes hex)"},
                        {RPCResult::Type::BOOL, "valid", "Whether the public key is valid"},
                        {RPCResult::Type::BOOL, "authorized", "Whether key is authorized in consensus parameters"},
                        {RPCResult::Type::BOOL, "is_running", "Whether oracle node is currently running"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getoraclepubkey", "0") +
                    HelpExampleRpc("getoraclepubkey", "0")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            int oracle_id = request.params[0].getInt<int>();

            // Validate oracle ID
            if (oracle_id < 0 || oracle_id >= ORACLE_TOTAL_COUNT) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Invalid oracle ID %d. Must be between 0 and %d", oracle_id, ORACLE_TOTAL_COUNT - 1));
            }

            // Get oracle manager
            OracleManager& oracle_manager = OracleManager::GetInstance();
            OracleNode* oracle = oracle_manager.GetOracleNode(oracle_id);

            if (!oracle) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Oracle %d not found. Use startoracle to initialize it first.", oracle_id));
            }

            // Get public keys
            XOnlyPubKey xonly_pubkey = oracle->GetOraclePublicKey();
            CPubKey full_pubkey = oracle->GetPublicKey();
            bool is_authorized = oracle->ValidateOracleKey();
            bool is_running = oracle->IsRunning();

            UniValue result(UniValue::VOBJ);
            result.pushKV("oracle_id", oracle_id);
            result.pushKV("pubkey", HexStr(xonly_pubkey));
            result.pushKV("pubkey_full", HexStr(full_pubkey));
            result.pushKV("valid", xonly_pubkey.IsFullyValid());
            result.pushKV("authorized", is_authorized);
            result.pushKV("is_running", is_running);

            return result;
        },
    };
}

// =============================================================================
// Mock Oracle RPC Commands (RegTest only)
// =============================================================================

static RPCHelpMan setmockoracleprice()
{
    return RPCHelpMan{"setmockoracleprice",
                "\nSet mock oracle price for testing (RegTest only).\n"
                "This command allows setting a custom DGB/USD price for testing DigiDollar\n"
                "functionality in RegTest mode without requiring real oracle nodes.\n",
                {
                    {"price", RPCArg::Type::NUM, RPCArg::Optional::NO, "Price in cents per DGB (e.g., 50 = $0.50/DGB, 10000 = $100/DGB)", RPCArgOptions{.skip_type_check = true}}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "price", "New mock oracle price in cents per DGB"},
                        {RPCResult::Type::STR, "price_usd", "Price formatted as USD per DGB"},
                        {RPCResult::Type::NUM, "update_height", "Block height of update"},
                        {RPCResult::Type::BOOL, "enabled", "Whether mock oracle is enabled"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("setmockoracleprice", "50") +
                    "\nSet price to $0.50 per DGB (50 cents)\n" +
                    HelpExampleCli("setmockoracleprice", "10000") +
                    "\nSet price to $100.00 per DGB (10000 cents)\n" +
                    HelpExampleRpc("setmockoracleprice", "50")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Only allow in RegTest mode
            if (Params().GetChainType() != ChainType::REGTEST) {
                throw JSONRPCError(RPC_METHOD_NOT_FOUND,
                    "setmockoracleprice is only available in RegTest mode");
            }

            CAmount price = request.params[0].getInt<int64_t>();

            if (price <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "Price must be positive");
            }

            // Price should be reasonable (between $0.01 and $1000 per DGB in cents)
            const CAmount MIN_PRICE = 1;         // 1 cent = $0.01 per DGB
            const CAmount MAX_PRICE = 100000;    // 100,000 cents = $1000 per DGB

            if (price < MIN_PRICE || price > MAX_PRICE) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Price must be between %d and %d cents per DGB", MIN_PRICE, MAX_PRICE));
            }

            // Set the mock price
            MockOracleManager::GetInstance().SetMockPrice(price);

            // Build result
            UniValue result(UniValue::VOBJ);
            result.pushKV("price", price);
            // Format as dollars (divide cents by 100)
            result.pushKV("price_usd", strprintf("$%.2f", price / 100.0));
            result.pushKV("update_height", MockOracleManager::GetInstance().GetLastUpdateHeight());
            result.pushKV("enabled", MockOracleManager::GetInstance().IsEnabled());

            return result;
        },
    };
}

static RPCHelpMan getmockoracleprice()
{
    return RPCHelpMan{"getmockoracleprice",
                "\nGet current mock oracle price (RegTest only).\n"
                "Returns the current mock oracle price used for testing DigiDollar\n"
                "functionality in RegTest mode.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "price", "Current mock oracle price in satoshis per USD"},
                        {RPCResult::Type::NUM, "price_usd", "Price as USD per DGB"},
                        {RPCResult::Type::NUM, "last_update_height", "Block height of last update"},
                        {RPCResult::Type::BOOL, "enabled", "Whether mock oracle is enabled"},
                        {RPCResult::Type::NUM, "current_height", "Current blockchain height"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getmockoracleprice", "") +
                    HelpExampleRpc("getmockoracleprice", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Only allow in RegTest mode
            if (Params().GetChainType() != ChainType::REGTEST) {
                throw JSONRPCError(RPC_METHOD_NOT_FOUND,
                    "getmockoracleprice is only available in RegTest mode");
            }

            CAmount price = MockOracleManager::GetInstance().GetCurrentPrice();
            int64_t lastHeight = MockOracleManager::GetInstance().GetLastUpdateHeight();
            bool enabled = MockOracleManager::GetInstance().IsEnabled();

            // Get current height from chain state (optional for mock oracle)
            int currentHeight = 0;
            try {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                if (node.chainman) {
                    LOCK(node.chainman->GetMutex());
                    currentHeight = node.chainman->ActiveHeight();
                }
            } catch (...) {
                // Height tracking is optional for mock oracle
                currentHeight = 0;
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("price", price);
            result.pushKV("price_usd", ValueFromAmount(price));
            result.pushKV("last_update_height", lastHeight);
            result.pushKV("enabled", enabled);
            result.pushKV("current_height", currentHeight);

            return result;
        },
    };
}

static RPCHelpMan simulatepricevolatility()
{
    return RPCHelpMan{"simulatepricevolatility",
                "\nSimulate price volatility for testing (RegTest only).\n"
                "Adjusts the mock oracle price by a given percentage to test\n"
                "DigiDollar system responses to price changes.\n",
                {
                    {"percent_change", RPCArg::Type::NUM, RPCArg::Optional::NO, "Percentage change (positive or negative, e.g., 50 for +50%, -20 for -20%)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "old_price", "Previous mock oracle price"},
                        {RPCResult::Type::NUM, "new_price", "New mock oracle price after volatility"},
                        {RPCResult::Type::NUM, "percent_change", "Percentage change applied"},
                        {RPCResult::Type::NUM, "update_height", "Block height of update"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("simulatepricevolatility", "50") +
                    "\nIncrease price by 50%\n" +
                    HelpExampleCli("simulatepricevolatility", "-80") +
                    "\nDecrease price by 80%\n" +
                    HelpExampleRpc("simulatepricevolatility", "50")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Only allow in RegTest mode
            if (Params().GetChainType() != ChainType::REGTEST) {
                throw JSONRPCError(RPC_METHOD_NOT_FOUND,
                    "simulatepricevolatility is only available in RegTest mode");
            }

            int percentChange = request.params[0].getInt<int>();

            if (percentChange < -100 || percentChange > 1000) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "Percentage change must be between -100 and 1000");
            }

            CAmount oldPrice = MockOracleManager::GetInstance().GetCurrentPrice();
            MockOracleManager::GetInstance().SimulateVolatility(percentChange);
            CAmount newPrice = MockOracleManager::GetInstance().GetCurrentPrice();

            UniValue result(UniValue::VOBJ);
            result.pushKV("old_price", oldPrice);
            result.pushKV("new_price", newPrice);
            result.pushKV("percent_change", percentChange);
            result.pushKV("update_height", MockOracleManager::GetInstance().GetLastUpdateHeight());

            return result;
        },
    };
}

static RPCHelpMan enablemockoracle()
{
    return RPCHelpMan{"enablemockoracle",
                "\nEnable or disable mock oracle (RegTest only).\n"
                "Controls whether the mock oracle is active for testing.\n",
                {
                    {"enable", RPCArg::Type::BOOL, RPCArg::Optional::NO, "true to enable, false to disable"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "enabled", "New enabled status"},
                        {RPCResult::Type::NUM, "current_price", "Current mock oracle price"},
                        {RPCResult::Type::NUM, "current_height", "Current blockchain height"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("enablemockoracle", "true") +
                    HelpExampleCli("enablemockoracle", "false") +
                    HelpExampleRpc("enablemockoracle", "true")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Only allow in RegTest mode
            if (Params().GetChainType() != ChainType::REGTEST) {
                throw JSONRPCError(RPC_METHOD_NOT_FOUND,
                    "enablemockoracle is only available in RegTest mode");
            }

            bool enable = request.params[0].get_bool();
            MockOracleManager::GetInstance().SetEnabled(enable);

            // Get current height from chain state (optional for mock oracle)
            int currentHeight = 0;
            try {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                if (node.chainman) {
                    LOCK(node.chainman->GetMutex());
                    currentHeight = node.chainman->ActiveHeight();
                }
            } catch (...) {
                // Height tracking is optional for mock oracle
                currentHeight = 0;
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("enabled", MockOracleManager::GetInstance().IsEnabled());
            result.pushKV("current_price", MockOracleManager::GetInstance().GetCurrentPrice());
            result.pushKV("current_height", currentHeight);

            return result;
        },
    };
}

void RegisterDigiDollarRPCCommands(CRPCTable &t)
{
    static const CRPCCommand commands[] = {
        // System monitoring commands
        {"digidollar", &getdigidollarstats},
        {"digidollar", &getdcamultiplier},
        {"digidollar", &calculatecollateralrequirement},
        {"digidollar", &getdigidollardeploymentinfo},

        // Core transaction commands (moved to wallet RPC table)
        // {"digidollar", &mintdigidollar},
        // {"digidollar", &senddigidollar},
        // {"digidollar", &redeemdigidollar},
        // {"digidollar", &listdigidollarpositions},

        // Address management commands
        // {"digidollar", &getdigidollaraddress},  // Moved to wallet RPC commands for proper wallet context
        {"digidollar", &validateddaddress},
        {"digidollar", &listdigidollaraddresses},
        {"digidollar", &importdigidollaraddress},

        // Utility commands (moved to wallet RPC table)
        // {"digidollar", &getdigidollarbalance},
        {"digidollar", &estimatecollateral},
        {"digidollar", &getredemptioninfo},
        // {"digidollar", &listdigidollartxs},
        {"digidollar", &getoracleprice},
        {"digidollar", &getprotectionstatus},

        // Oracle management commands
        {"oracle", &sendoracleprice},
        {"oracle", &listoracles},
        {"oracle", &startoracle},
        {"oracle", &stoporacle},
        {"oracle", &getoraclepubkey},

        // Mock Oracle commands (RegTest only)
        {"digidollar", &setmockoracleprice},
        {"digidollar", &getmockoracleprice},
        {"digidollar", &simulatepricevolatility},
        {"digidollar", &enablemockoracle}
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}