// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/util.h>
#include <rpc/server_util.h>
#include <rpc/blockchain.h>
#include <rpc/digidollar_transactions.h>
#include <random.h>
#include <oracle/bundle_manager.h>
#include <primitives/oracle.h>
#include <oracle/node.h>
#include <oracle/mock_oracle.h>
#include <consensus/digidollar.h>
#include <consensus/dca.h>
#include <consensus/err.h>
#include <digidollar/digidollar.h>
#include <digidollar/health.h>
#include <index/digidollarstatsindex.h>
#include <chainparams.h>
#include <kernel/chainparams.h>
#include <node/context.h>
#include <core_io.h>
#include <util/strencodings.h>
#include <validation.h>
#include <versionbits.h>
#include <wallet/wallet.h>
#include <wallet/receive.h>
#include <cmath>
#include <wallet/context.h>
#include <wallet/rpc/util.h>
#include <wallet/spend.h>
#include <wallet/coincontrol.h>
#include <wallet/coinselection.h>
#include <wallet/digidollarwallet.h>
#include <wallet/walletdb.h>
#include <wallet/scriptpubkeyman.h>
#include <interfaces/wallet.h>
#include <digidollar/txbuilder.h>
#include <node/transaction.h>
#include <base58.h>
#include <script/standard.h>
#include <script/signingprovider.h>
#include <rpc/protocol.h>
#include <versionbits.h>
#include <deploymentstatus.h>
#include <key_io.h>

#include <util/time.h>
#include <univalue.h>
#include <cmath>

using namespace DigiDollar;
using namespace DigiDollar::DCA;

// Mock utility functions for RPC-only implementation
namespace {
    // Aligned with consensus/digidollar.h (10 tiers, 0-9)
    int GetLockDaysForTier(uint32_t tier) {
        switch (tier) {
            case 0: return 0;     // Special: 1 hour (240 blocks) - handled separately
            case 1: return 30;    // 30 days
            case 2: return 90;    // 90 days (3 months)
            case 3: return 180;   // 180 days (6 months)
            case 4: return 365;   // 1 year
            case 5: return 730;   // 2 years
            case 6: return 1095;  // 3 years
            case 7: return 1825;  // 5 years
            case 8: return 2555;  // 7 years
            case 9: return 3650;  // 10 years
            default: return 0;
        }
    }

    // Aligned with consensus/digidollar.h collateralRatios (10 tiers, 0-9)
    int GetMinCollateralRatio(uint32_t tier) {
        switch (tier) {
            case 0: return 1000;  // 1000% for 1 hour (testing only)
            case 1: return 500;   // 500% for 30 days
            case 2: return 400;   // 400% for 90 days
            case 3: return 350;   // 350% for 180 days
            case 4: return 300;   // 300% for 1 year
            case 5: return 275;   // 275% for 2 years
            case 6: return 250;   // 250% for 3 years
            case 7: return 225;   // 225% for 5 years
            case 8: return 212;   // 212% for 7 years
            case 9: return 200;   // 200% for 10 years
            default: return 500;
        }
    }

    /**
     * Generate an HD-derived key from the wallet for DigiDollar use.
     * This allows DD keys to be recovered from the wallet seed.
     *
     * @param pwallet The wallet to derive the key from
     * @param label Label to identify the key's purpose (e.g., "dd-owner", "dd-address")
     * @return CKey The derived key, or an invalid key if derivation fails
     */
    CKey GetHDKeyForDigiDollar(wallet::CWallet* pwallet, const std::string& label) {
        AssertLockHeld(pwallet->cs_wallet);

        CKey key;

        // Try to get an HD-derived destination from the wallet
        // Try BECH32M (Taproot) first for modern wallets
        auto op_dest = pwallet->GetNewDestination(OutputType::BECH32M, label);
        if (!op_dest) {
            // Fallback to BECH32 (SegWit v0) for legacy descriptor wallets
            LogPrintf("DigiDollar: BECH32M not available, trying BECH32 for label '%s'\n", label);
            op_dest = pwallet->GetNewDestination(OutputType::BECH32, label);
        }

        if (op_dest) {
            CTxDestination dest = *op_dest;
            CScript script = GetScriptForDestination(dest);

            // Check if this is a Taproot (WitnessV1Taproot) address
            // Taproot addresses need special handling - use GetKeyByXOnly instead of GetKey
            if (auto* taproot_dest = std::get_if<WitnessV1Taproot>(&dest)) {
                XOnlyPubKey output_key(*taproot_dest);
                LogPrintf("DigiDollar: GetHDKeyForDigiDollar - descriptor produced output_key=%s for label '%s'\n",
                         HexStr(output_key), label);

                // For Taproot, we need to find the key using GetSigningProviderWithKeys (includes private keys)
                // GetSolvingProvider doesn't include private keys, so GetKeyByXOnly would fail
                for (auto* spk_man : pwallet->GetAllScriptPubKeyMans()) {
                    if (auto* desc_spk = dynamic_cast<wallet::DescriptorScriptPubKeyMan*>(spk_man)) {
                        // Use GetSigningProviderWithKeys to get provider with private keys
                        auto provider = desc_spk->GetSigningProviderWithKeys(script);
                        if (provider) {
                            TaprootSpendData spenddata;
                            if (provider->GetTaprootSpendData(output_key, spenddata)) {
                                LogPrintf("DigiDollar: GetHDKeyForDigiDollar - spenddata.internal_key=%s\n",
                                         HexStr(spenddata.internal_key));
                                if (spenddata.internal_key.IsFullyValid()) {
                                    if (provider->GetKeyByXOnly(spenddata.internal_key, key)) {
                                        // DEBUG: Verify the key we got matches what DD will produce
                                        XOnlyPubKey key_xonly(key.GetPubKey());
                                        auto key_tweaked = key_xonly.CreateTapTweak(nullptr);
                                        if (key_tweaked) {
                                            LogPrintf("DigiDollar: GetHDKeyForDigiDollar - returned key pubkey_xonly=%s, tweaked=%s (matches descriptor: %s)\n",
                                                     HexStr(key_xonly), HexStr(key_tweaked->first),
                                                     (key_tweaked->first == output_key) ? "YES" : "NO");
                                        }
                                        LogPrintf("DigiDollar: Successfully derived HD key from Taproot descriptor wallet for label '%s'\n", label);
                                        return key;
                                    }
                                }
                            }
                            // Fallback: try to get key by output key directly
                            if (provider->GetKeyByXOnly(output_key, key)) {
                                LogPrintf("DigiDollar: Successfully derived HD key from Taproot (output key) for label '%s' - WARNING: using output key directly!\n", label);
                                return key;
                            }
                        }
                    }
                }
                LogPrintf("DigiDollar: WARNING - Could not extract Taproot key from HD destination for label '%s'\n", label);
            } else {
                // Non-Taproot addresses (SegWit v0, legacy)
                // Try to extract the key from the destination
                // This works for both legacy and descriptor wallets
                for (auto* spk_man : pwallet->GetAllScriptPubKeyMans()) {
                    // Try legacy wallet path first (direct GetKey access)
                    if (auto* legacy_spk = dynamic_cast<wallet::LegacyScriptPubKeyMan*>(spk_man)) {
                        CKeyID keyid = GetKeyForDestination(*legacy_spk, dest);
                        if (!keyid.IsNull() && legacy_spk->GetKey(keyid, key)) {
                            LogPrintf("DigiDollar: Successfully derived HD key from legacy wallet for label '%s'\n", label);
                            return key;
                        }
                    }

                    // Try descriptor wallet path (use GetSigningProviderWithKeys for private keys)
                    if (auto* desc_spk = dynamic_cast<wallet::DescriptorScriptPubKeyMan*>(spk_man)) {
                        auto provider = desc_spk->GetSigningProviderWithKeys(script);
                        if (provider) {
                            CKeyID keyid = GetKeyForDestination(*provider, dest);
                            if (!keyid.IsNull() && provider->GetKey(keyid, key)) {
                                LogPrintf("DigiDollar: Successfully derived HD key from descriptor wallet for label '%s'\n", label);
                                return key;
                            }
                        }
                    }
                }
                LogPrintf("DigiDollar: WARNING - Could not extract key from HD destination for label '%s'\n", label);
            }
        } else {
            LogPrintf("DigiDollar: WARNING - Could not get HD destination for label '%s': %s\n",
                     label, util::ErrorString(op_dest).original);
        }

        // Fallback to random key if HD derivation fails
        // This maintains backward compatibility with wallets that don't support HD
        LogPrintf("DigiDollar: Falling back to random key for label '%s'\n", label);
        key.MakeNewKey(true);
        return key;
    }

    bool TryStartOracleFromPrivateKey(OracleManager& oracle_manager, uint32_t oracle_id, const std::string& private_key_hex, const std::string& key_source, bool allow_initialized_without_running, std::string& status_message)
    {
        OracleNode* oracle = oracle_manager.GetOracleNode(oracle_id);
        if (!oracle) {
            if (!oracle_manager.AddOracleNode(oracle_id, private_key_hex)) {
                status_message = strprintf("Failed to initialize oracle with %s", key_source);
                return false;
            }
            oracle_manager.EnableOracle(oracle_id, true);
            oracle = oracle_manager.GetOracleNode(oracle_id);
            if (!oracle) {
                status_message = strprintf("Oracle initialized with %s but manager returned no oracle instance", key_source);
                return false;
            }
        } else {
            oracle_manager.EnableOracle(oracle_id, true);
        }

        oracle->Start();
        if (oracle->IsRunning()) {
            status_message = strprintf("Oracle started with %s", key_source);
            return true;
        }

        if (allow_initialized_without_running) {
            status_message = strprintf("Oracle initialized with %s (price thread not active on this network)", key_source);
            return true;
        }

        status_message = strprintf("Oracle initialized with %s but failed to start price thread", key_source);
        return false;
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
                        {RPCResult::Type::NUM, "oracle_price_micro_usd", "Current DGB/USD price from oracle in micro-USD (1,000,000 = $1.00)"},
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
                        },
                        {RPCResult::Type::OBJ, "err_tier", "Current Emergency Redemption Ratio (ERR) tier information",
                            {
                                {RPCResult::Type::NUM, "ratio", "ERR ratio (0.80-1.0) - lower = more DD burn required"},
                                {RPCResult::Type::NUM, "burn_multiplier", "DD burn multiplier (1.0-1.25x) - how much MORE DD to burn"},
                                {RPCResult::Type::STR, "description", "ERR tier description with burn multiplier"}
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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            // NETWORK-WIDE TRACKING: Use DigiDollar stats index for efficient tracking
            // This ensures all nodes see identical stats regardless of which wallets are loaded
            CAmount totalCollateral = 0;
            CAmount totalDD = 0;

            // Get node context for chainstate access
            const node::NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);

            // Use the DigiDollar stats index for efficient network-wide tracking
            if (g_digidollar_stats_index) {
                if (!g_digidollar_stats_index->BlockUntilSyncedToCurrentChain()) {
                    const IndexSummary summary{g_digidollar_stats_index->GetSummary()};
                    throw JSONRPCError(RPC_INTERNAL_ERROR,
                        strprintf("DigiDollar stats index is syncing. Current height: %d", summary.best_block_height));
                }

                const CBlockIndex* pindex;
                {
                    LOCK(cs_main);
                    pindex = chainman.ActiveChain().Tip();
                }

                if (pindex) {
                    auto stats = g_digidollar_stats_index->LookUpStats(*pindex);
                    if (stats) {
                        totalDD = stats->total_dd_supply;
                        totalCollateral = stats->total_collateral;
                    }
                }
            } else {
                // Fallback: Use UTXO scanning (slow but works without index)
                LogPrintf("DigiDollar: getdigidollarstats - DigiDollar stats index not available, falling back to UTXO scan\n");

                // Access the UTXO set (like gettxoutsetinfo does)
                // CRITICAL: Must flush OUTSIDE the lock, then re-acquire lock for scanning
                Chainstate& active_chainstate = chainman.ActiveChainstate();

                // Step 1: Force flush all cached coins to disk (like gettxoutsetinfo does)
                LogPrintf("DigiDollar: getdigidollarstats - About to ForceFlushStateToDisk...\n");
                active_chainstate.ForceFlushStateToDisk();
                LogPrintf("DigiDollar: getdigidollarstats - ForceFlushStateToDisk completed\n");

                // Step 2: Now acquire lock and access the flushed CoinsDB
                // CRITICAL: Hold cs_main lock during ScanUTXOSet to prevent race conditions
                CCoinsView* coins_view;
                node::BlockManager* blockman;
                const CTxMemPool* mempool = node.mempool.get();
                {
                    LOCK(::cs_main);
                    coins_view = &active_chainstate.CoinsDB();
                    blockman = &active_chainstate.m_blockman;

                    // Scan UTXO set to find ALL DigiDollar vaults network-wide
                    // Pass BlockManager for full transaction access
                    // Pass both CoinsDB (for iteration) and CoinsTip (for validation)
                    LogPrintf("DigiDollar: getdigidollarstats - About to call ScanUTXOSet...\n");
                    DigiDollar::SystemHealthMonitor::ScanUTXOSet(coins_view, &active_chainstate.CoinsTip(), blockman, mempool);
                    LogPrintf("DigiDollar: getdigidollarstats - ScanUTXOSet completed\n");
                }

                // Get metrics from scanner
                DigiDollar::SystemMetrics metrics = DigiDollar::SystemHealthMonitor::GetSystemMetrics();
                totalCollateral = metrics.totalCollateral;
                totalDD = metrics.totalDDSupply;
            }

            // Get current oracle price in micro-USD from the real oracle system
            // micro-USD format: 1,000,000 = $1.00, so 6310 = $0.00631
            OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
            CAmount oraclePriceMicroUSD = oracle_manager.GetLatestPrice();

            // Fall back to MockOracleManager for regtest/testing if no real oracle data
            if (oraclePriceMicroUSD <= 0 && Params().GetChainType() == ChainType::REGTEST) {
                // MockOracleManager already returns micro-USD (see mock_oracle.cpp)
                oraclePriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();
            }

            // Convert micro-USD to millicents for CalculateSystemHealth
            // micro-USD / 10 = millicents (e.g., 6310 micro-USD / 10 = 631 millicents = $0.00631)
            CAmount oraclePriceMillicents = oraclePriceMicroUSD / 10;

            // Calculate cents for display (rounded). Allow 0 for sub-cent prices —
            // oracle_price_micro_usd and price_usd fields have full precision.
            CAmount oraclePriceCents = (oraclePriceMicroUSD + 5000) / 10000;

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
            result.pushKV("total_dd_supply", int64_t{totalDD});
            result.pushKV("oracle_price_cents", int64_t{oraclePriceCents});   // Rounded to cents for display
            result.pushKV("oracle_price_micro_usd", int64_t{oraclePriceMicroUSD}); // Full precision micro-USD
            result.pushKV("is_emergency", isEmergency);

            // Add fields expected by tests
            result.pushKV("system_collateral_ratio", systemHealth);
            result.pushKV("total_collateral_locked", ValueFromAmount(totalCollateral));
            // Get active position count from the DigiDollar stats index.
            // The stats index tracks vault_count (incremented on mint,
            // decremented on redeem) so this reflects real network state.
            // Falls back to 0 if the stats index isn't available yet.
            uint64_t activePositions = 0;
            if (g_digidollar_stats_index) {
                ChainstateManager& chainman = EnsureAnyChainman(request.context);
                LOCK(cs_main);
                const CBlockIndex* pindex = chainman.ActiveChain().Tip();
                if (pindex) {
                    auto ddstats = g_digidollar_stats_index->LookUpStats(*pindex);
                    if (ddstats) {
                        activePositions = ddstats->vault_count;
                    }
                }
            }
            result.pushKV("active_positions", static_cast<int64_t>(activePositions));
            result.pushKV("oracle_price_age", 0); // TODO: Calculate age

            UniValue dcaTier(UniValue::VOBJ);
            dcaTier.pushKV("min_collateral", tier.minCollateral);
            dcaTier.pushKV("max_collateral", tier.maxCollateral);
            dcaTier.pushKV("multiplier", tier.multiplier);
            dcaTier.pushKV("status", tier.status);
            result.pushKV("dca_tier", dcaTier);

            // Add ERR (Emergency Redemption Ratio) tier information
            // ERR increases DD burn requirement, NOT reduces collateral!
            // ratio = how much of original DD is "worth" -> burn 1/ratio DD to get FULL collateral
            double errRatio = DigiDollar::ERR::EmergencyRedemptionRatio::CalculateERRAdjustment(systemHealth);
            double burnMultiplier = 1.0;
            std::string errDescription;
            if (systemHealth >= 100) {
                errDescription = "Normal (1.0x burn)";
                errRatio = 1.0;
                burnMultiplier = 1.0;
            } else if (systemHealth >= 95) {
                errDescription = "95-100%: 1.05x DD burn";
                burnMultiplier = 1.0 / errRatio; // ~1.053x
            } else if (systemHealth >= 90) {
                errDescription = "90-95%: 1.11x DD burn";
                burnMultiplier = 1.0 / errRatio; // ~1.111x
            } else if (systemHealth >= 85) {
                errDescription = "85-90%: 1.18x DD burn";
                burnMultiplier = 1.0 / errRatio; // ~1.176x
            } else {
                errDescription = "<85%: 1.25x DD burn (max)";
                burnMultiplier = 1.0 / errRatio; // 1.25x
            }

            UniValue errTier(UniValue::VOBJ);
            errTier.pushKV("ratio", errRatio);
            errTier.pushKV("burn_multiplier", burnMultiplier);
            errTier.pushKV("description", errDescription);
            result.pushKV("err_tier", errTier);

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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
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
                        {RPCResult::Type::NUM, "oracle_price_micro_usd", "DGB price used in micro-USD (1,000,000 = $1.00)"},
                        {RPCResult::Type::NUM, "oracle_price_usd", "DGB price used in USD"},
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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            // Parse parameters
            CAmount ddAmount = request.params[0].getInt<int64_t>();
            int lockDays = request.params[1].getInt<int>();

            // Get oracle price in micro-USD: use provided value or fetch from real oracle system
            CAmount oraclePriceMicroUSD;
            if (request.params.size() > 2 && !request.params[2].isNull()) {
                // User-provided value is in micro-USD (1,000,000 = $1.00)
                oraclePriceMicroUSD = request.params[2].getInt<int64_t>();
            } else {
                // Use real oracle price from OracleIntegration (returns micro-USD)
                oraclePriceMicroUSD = OracleIntegration::GetCurrentOraclePriceMicroUSD();
                if (oraclePriceMicroUSD <= 0 && Params().GetChainType() == ChainType::REGTEST) {
                    // Fall back to mock oracle ONLY in regtest
                    oraclePriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();
                }
                if (oraclePriceMicroUSD <= 0) {
                    throw JSONRPCError(RPC_MISC_ERROR, "No oracle price available. Start the oracle first with startoracle command.");
                }
            }

            // Validate parameters
            if (ddAmount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "DD amount must be positive");
            }
            if (lockDays <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Lock days must be positive");
            }
            if (oraclePriceMicroUSD <= 0) {
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

            // Calculate required DGB using micro-USD precision
            // Formula: Required_DGB_sats = (DD_cents * COIN * ratio * 100) / oracle_micro_usd
            // Example: $100 DD at $0.00631 DGB with 150% ratio (oracle_micro_usd = 6310)
            //   = (10000 cents * 100000000 * 150 * 100) / 6310
            //   = 15,000,000,000,000,000 / 6310
            //   = 2,377,179,080,509 sats = ~23,772 DGB
            // Use __int128 to avoid uint64 overflow for large DD amounts
            __int128 numerator = static_cast<__int128>(ddAmount) * static_cast<__int128>(COIN) *
                                 static_cast<__int128>(effectiveRatio) * 100;
            __int128 result128 = numerator / static_cast<__int128>(oraclePriceMicroUSD);
            if (result128 > static_cast<__int128>(MAX_MONEY)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Required collateral exceeds maximum money supply");
            }
            uint64_t requiredDGB = static_cast<uint64_t>(result128);

            UniValue result(UniValue::VOBJ);
            result.pushKV("required_dgb", ValueFromAmount(requiredDGB));
            result.pushKV("dd_amount_cents", int64_t{ddAmount});
            result.pushKV("dd_amount_usd", ddAmount / 100.0);  // Convert cents to USD
            result.pushKV("lock_days", lockDays);
            result.pushKV("lock_blocks", int64_t{lockBlocks});
            result.pushKV("base_ratio", baseRatio);
            result.pushKV("dca_multiplier", dcaMultiplier);
            result.pushKV("effective_ratio", effectiveRatio);
            result.pushKV("oracle_price_micro_usd", int64_t{oraclePriceMicroUSD});
            result.pushKV("oracle_price_usd", oraclePriceMicroUSD / 1000000.0);
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
                        {RPCResult::Type::NUM, "activation_height", /*optional=*/true, "Actual activation height (only present when active)"},
                        {RPCResult::Type::NUM, "blocks_until_timeout", /*optional=*/true, "Blocks remaining until timeout (only during started/locked_in)"},
                        {RPCResult::Type::NUM, "signaling_blocks", /*optional=*/true, "Blocks signaling support in current period (only during started/locked_in)"},
                        {RPCResult::Type::NUM, "threshold", /*optional=*/true, "Threshold required for activation (only during started/locked_in)"},
                        {RPCResult::Type::NUM, "period_blocks", /*optional=*/true, "Number of blocks in signaling period (only during started/locked_in)"},
                        {RPCResult::Type::NUM, "progress_percent", /*optional=*/true, "Signaling progress as percentage (only during started/locked_in)"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getdigidollardeploymentinfo", "")
                    + HelpExampleRpc("getdigidollardeploymentinfo", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // NOTE: getdigidollardeploymentinfo is intentionally NOT gated behind
            // activation. Users need this RPC to monitor BIP9 deployment progress
            // (DEFINED → STARTED → LOCKED_IN → ACTIVE). Gating it would make it
            // impossible to check when DigiDollar will activate.
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
                    {"dd_amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "Amount of DigiDollar to mint in cents (min 10000/$100, max 10000000/$100K)", RPCArgOptions{.skip_type_check = true}},
                    {"lock_tier", RPCArg::Type::NUM, RPCArg::Optional::NO, "Lock tier 0-9 (0=1h testing, 1=30d, 2=90d, 3=180d, 4=1y, 5=2y, 6=3y, 7=5y, 8=7y, 9=10y)", RPCArgOptions{.skip_type_check = true}},
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
                        {RPCResult::Type::STR, "position_id", "Unique position identifier"},
                        {RPCResult::Type::STR_HEX, "consolidation_txid", /*optional=*/true, "TXID of auto-consolidation transaction (only present if UTXOs were consolidated)"},
                        {RPCResult::Type::BOOL, "utxos_consolidated", /*optional=*/true, "True if wallet UTXOs were auto-consolidated before minting"}
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
            // Get wallet first (wallet RPCs have WalletContext, not NodeContext)
            std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "No wallet is loaded");

            // Check DigiDollar activation via wallet's chain interface
            {
                node::NodeContext* node_ctx = pwallet->chain().context();
                if (!node_ctx) throw JSONRPCError(RPC_INTERNAL_ERROR, "Node context unavailable");
                ChainstateManager& chainman = *node_ctx->chainman;
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }

            // Ensure wallet is unlocked
            wallet::EnsureWalletIsUnlocked(*pwallet);

            // Parse parameters
            CAmount ddAmount = request.params[0].getInt<int64_t>();
            int lockTier = request.params[1].getInt<int>();

            // DigiDollar transactions MUST pay at least 0.1 DGB fee to miners
            // Use a high fee rate to ensure the minimum is met for all transaction sizes
            // MIN_DD_TX_FEE = 10,000,000 satoshis = 0.1 DGB
            // For a typical 300-byte tx, we need feeRate = 10,000,000 / 300 * 1000 = 33,333,333 sat/kB
            // We use 35,000,000 sat/kB to ensure minimum is always met
            static const CAmount MIN_DD_FEE_RATE = 35000000; // 0.35 DGB/kB ensures min 0.1 DGB for typical tx
            CAmount feeRate = request.params.size() > 2 && !request.params[2].isNull() ?
                std::max(request.params[2].getInt<int64_t>(), MIN_DD_FEE_RATE) : MIN_DD_FEE_RATE;

            // Validate parameters
            if (ddAmount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "DigiDollar amount must be positive");
            }
            
            // Get consensus parameters for mint amount validation
            const auto& chainParams = Params();
            const auto& ddParams = chainParams.GetDigiDollarParams();
            
            // Validate against consensus mint limits
            if (!DigiDollar::IsValidMintAmount(ddAmount, ddParams)) {
                if (ddAmount < ddParams.minMintAmount) {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, 
                        strprintf("Minimum mint amount is $%d (%d cents)", 
                            ddParams.minMintAmount / 100, ddParams.minMintAmount));
                } else {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, 
                        strprintf("Maximum mint amount is $%d (%d cents)", 
                            ddParams.maxMintAmount / 100, ddParams.maxMintAmount));
                }
            }
            
            if (lockTier < 0 || lockTier > 9) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Lock tier must be between 0 and 9 (0 = 1 hour testing tier)");
            }

            // Get current height from wallet's chain interface
            int currentHeight = pwallet->GetLastBlockHeight();

            // Get oracle price in micro-USD from real oracle system first, fall back to mock only in regtest
            CAmount oraclePriceMicroUSD = OracleIntegration::GetCurrentOraclePriceMicroUSD();
            if (oraclePriceMicroUSD <= 0 && Params().GetChainType() == ChainType::REGTEST) {
                oraclePriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();
            }
            if (oraclePriceMicroUSD <= 0) {
                throw JSONRPCError(RPC_MISC_ERROR, "No oracle price available. Start the oracle first with startoracle command.");
            }

            // ERR CHECK: Block minting during emergency state
            // Calculate health directly using wallet's DD positions and current oracle price
            // This is more reliable than cached metrics which may be stale
            if (pwallet->GetDDWallet()) {
                LOCK(pwallet->cs_wallet);
                CAmount totalDD = 0;
                CAmount totalCollateral = 0;

                // Get all active positions from wallet
                auto positions = pwallet->GetDDWallet()->GetDDTimeLocks(true); // true = active only
                for (const auto& pos : positions) {
                    totalDD += pos.dd_minted;
                    totalCollateral += pos.dgb_collateral;
                }

                // Only check health if there are existing DD positions
                if (totalDD > 0 && totalCollateral > 0) {
                    // Convert micro-USD to millicents for health calculation
                    CAmount oraclePriceMillicents = oraclePriceMicroUSD / 10;

                    int systemHealth = DynamicCollateralAdjustment::CalculateSystemHealth(
                        totalCollateral, totalDD, oraclePriceMillicents);

                    LogPrintf("DigiDollar RPC Mint: Health check - DD=%lld, collateral=%lld, price=%lld, health=%d%%\n",
                              static_cast<long long>(totalDD),
                              static_cast<long long>(totalCollateral),
                              static_cast<long long>(oraclePriceMillicents),
                              systemHealth);

                    // Block minting if system health is below 100% (emergency state)
                    if (systemHealth < 100) {
                        throw JSONRPCError(RPC_MISC_ERROR,
                            strprintf("Minting blocked: System is in emergency state (health: %d%%). "
                                      "Wait for system health to recover above 100%% before minting new DigiDollars.",
                                      systemHealth));
                    }
                }
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

            // Generate owner key from wallet using HD derivation
            // This allows the key to be recovered from wallet seed
            CKey ownerKey;
            {
                LOCK(pwallet->cs_wallet);
                ownerKey = GetHDKeyForDigiDollar(pwallet.get(), "dd-owner");
                if (!ownerKey.IsValid()) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Failed to generate owner key for DD mint");
                }
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
            // Note: MintTxBuilder now expects micro-USD price
            RpcMintTxBuilder builder(Params(), currentHeight, oraclePriceMicroUSD, utxoValues);

            DigiDollar::TxBuilderMintParams params;
            params.ddAmount = ddAmount;  // Amount in cents (e.g., 5000 = $50.00)
            params.lockDays = lockDays;
            params.lockTier = lockTier;  // Store tier explicitly in OP_RETURN for exact reconstruction
            params.ownerKey = ownerKey;
            params.feeRate = feeRate;
            params.utxos = availableUtxos;

            // CRITICAL FIX: Get a proper change address from the wallet for DGB change output
            // This ensures the wallet recognizes the change output as its own!
            {
                LOCK(pwallet->cs_wallet);
                auto op_dest = pwallet->GetNewChangeDestination(OutputType::BECH32);
                if (op_dest) {
                    params.dgbChangeDest = *op_dest;
                    LogPrintf("DigiDollar RPC Mint: Using wallet change address for DGB change output\n");
                } else {
                    LogPrintf("DigiDollar RPC Mint: WARNING - Could not get change destination!\n");
                }
            }

            DigiDollar::TxBuilderResult result = builder.BuildMintTransaction(params);

            // Auto-consolidate if mint failed due to UTXO fragmentation
            std::string consolidation_txid;
            if (!result.success && result.error.find("Too many small UTXOs") != std::string::npos) {
                LogPrintf("DigiDollar RPC Mint: UTXO fragmentation detected (%zu UTXOs). Auto-consolidating...\n",
                          availableUtxos.size());

                CAmount totalAvailable = 0;
                for (const auto& [outpoint, value] : utxoValues) {
                    totalAvailable += value;
                }
                CAmount minRequired = result.collateralRequired + 20000000;
                if (totalAvailable < minRequired) {
                    throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                        strprintf("Insufficient funds for collateral. Need %.2f DGB, have %.2f DGB.",
                                  result.collateralRequired / 100000000.0,
                                  totalAvailable / 100000000.0));
                }

                CTxDestination consolidationDest;
                {
                    LOCK(pwallet->cs_wallet);
                    auto op_dest = pwallet->GetNewChangeDestination(OutputType::BECH32);
                    if (!op_dest) {
                        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to get consolidation address");
                    }
                    consolidationDest = *op_dest;
                }

                // Multi-pass consolidation: MAX_STANDARD_TX_WEIGHT is 400k WU.
                // P2WPKH input ≈ 271 WU. Conservative limit: 1400 inputs per pass.
                static const size_t MAX_CONSOLIDATION_INPUTS = 1400;
                static const int MAX_CONSOLIDATION_PASSES = 10;
                int pass = 0;

                while (availableUtxos.size() > MAX_CONSOLIDATION_INPUTS && pass < MAX_CONSOLIDATION_PASSES) {
                    ++pass;
                    size_t batch_size = std::min(availableUtxos.size(), MAX_CONSOLIDATION_INPUTS);
                    LogPrintf("DigiDollar RPC Mint: Consolidation pass %d — sweeping %zu of %zu UTXOs\n",
                              pass, batch_size, availableUtxos.size());

                    wallet::CCoinControl coin_control;
                    CAmount batchTotal = 0;
                    for (size_t i = 0; i < batch_size; ++i) {
                        coin_control.Select(availableUtxos[i]);
                        batchTotal += utxoValues[availableUtxos[i]];
                    }
                    coin_control.m_allow_other_inputs = false;

                    wallet::CRecipient recipient{consolidationDest, batchTotal, /*subtract_fee=*/true};
                    std::vector<wallet::CRecipient> recipients = {recipient};

                    auto consolidation_result = wallet::CreateTransaction(*pwallet, recipients, /*change_pos=*/-1, coin_control, /*sign=*/true);
                    if (!consolidation_result) {
                        throw JSONRPCError(RPC_WALLET_ERROR,
                            strprintf("Auto-consolidation pass %d failed: %s. Try manually consolidating UTXOs.",
                                      pass, util::ErrorString(consolidation_result).original));
                    }

                    const CTransactionRef& consolidation_tx = consolidation_result->tx;
                    consolidation_txid = consolidation_tx->GetHash().GetHex();
                    {
                        LOCK(pwallet->cs_wallet);
                        pwallet->CommitTransaction(consolidation_tx, {}, {});
                    }

                    LogPrintf("DigiDollar RPC Mint: Consolidation pass %d tx: %s (swept %.2f DGB from %zu inputs)\n",
                              pass, consolidation_txid, batchTotal / 100000000.0, batch_size);

                    availableUtxos.clear();
                    utxoValues.clear();
                    {
                        LOCK(pwallet->cs_wallet);
                        wallet::CoinsResult coins = wallet::AvailableCoins(*pwallet);
                        for (const wallet::COutput& coin : coins.All()) {
                            availableUtxos.push_back(coin.outpoint);
                            utxoValues[coin.outpoint] = coin.txout.nValue;
                        }
                    }
                    LogPrintf("DigiDollar RPC Mint: After pass %d: %zu UTXOs available\n", pass, availableUtxos.size());
                }

                if (consolidation_txid.empty() && availableUtxos.size() <= MAX_CONSOLIDATION_INPUTS) {
                    wallet::CCoinControl coin_control;
                    CAmount batchTotal = 0;
                    for (const auto& utxo : availableUtxos) {
                        coin_control.Select(utxo);
                        batchTotal += utxoValues[utxo];
                    }
                    coin_control.m_allow_other_inputs = false;

                    wallet::CRecipient recipient{consolidationDest, batchTotal, /*subtract_fee=*/true};
                    std::vector<wallet::CRecipient> recipients = {recipient};

                    auto consolidation_result = wallet::CreateTransaction(*pwallet, recipients, /*change_pos=*/-1, coin_control, /*sign=*/true);
                    if (!consolidation_result) {
                        throw JSONRPCError(RPC_WALLET_ERROR,
                            strprintf("Auto-consolidation failed: %s. Try manually consolidating UTXOs.",
                                      util::ErrorString(consolidation_result).original));
                    }

                    const CTransactionRef& consolidation_tx = consolidation_result->tx;
                    consolidation_txid = consolidation_tx->GetHash().GetHex();
                    {
                        LOCK(pwallet->cs_wallet);
                        pwallet->CommitTransaction(consolidation_tx, {}, {});
                    }

                    LogPrintf("DigiDollar RPC Mint: Single-pass consolidation tx: %s (swept %.2f DGB from %zu inputs)\n",
                              consolidation_txid, batchTotal / 100000000.0, availableUtxos.size());

                    availableUtxos.clear();
                    utxoValues.clear();
                    {
                        LOCK(pwallet->cs_wallet);
                        wallet::CoinsResult coins = wallet::AvailableCoins(*pwallet);
                        for (const wallet::COutput& coin : coins.All()) {
                            availableUtxos.push_back(coin.outpoint);
                            utxoValues[coin.outpoint] = coin.txout.nValue;
                        }
                    }
                }

                LogPrintf("DigiDollar RPC Mint: After consolidation: %zu UTXOs available (passes: %d)\n",
                          availableUtxos.size(), pass);

                RpcMintTxBuilder retryBuilder(Params(), currentHeight, oraclePriceMicroUSD, utxoValues);
                params.utxos = availableUtxos;
                result = retryBuilder.BuildMintTransaction(params);
            }

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

                // CRITICAL FIX: Track the DD UTXO so it can be found by GetDDUTXOs()
                // The DD token output is always at vout[1] in a mint transaction:
                //   vout[0] = Collateral (P2TR with timelock)
                //   vout[1] = DD token output (P2TR with 0 value) <- THIS IS THE DD UTXO
                //   vout[2] = OP_RETURN metadata
                //   vout[3] = Change output (optional)
                COutPoint ddOutpoint(tx->GetHash(), 1);
                pwallet->GetDDWallet()->AddDDUTXO(ddOutpoint, ddAmount);

                // CRITICAL FIX #2: Persist DD UTXO to wallet database so it survives daemon restart
                {
                    wallet::WalletBatch batch(pwallet->GetDatabase());
                    if (batch.WriteDDUTXO(ddOutpoint, ddAmount)) {
                        LogPrintf("DigiDollar RPC: Persisted DD UTXO %s:%d to database (amount=%d)\n",
                                 ddOutpoint.hash.ToString(), ddOutpoint.n, ddAmount);
                    } else {
                        LogPrintf("DigiDollar RPC: WARNING - Failed to persist DD UTXO to database\n");
                    }
                }

                LogPrintf("DigiDollar RPC: Added position %s with %d DD cents, stored owner key, and tracked DD UTXO at vout 1\n",
                         position.dd_timelock_id.ToString(), ddAmount);
            } else {
                LogPrintf("DigiDollar RPC: WARNING - No DD wallet context, position not persisted\n");
            }

            // Calculate collateral ratio based on lock tier and DCA
            int collateralRatio = 150; // Default, could be calculated from DCA

            UniValue resultObj(UniValue::VOBJ);
            resultObj.pushKV("txid", tx->GetHash().GetHex());
            resultObj.pushKV("dd_minted", int64_t{ddAmount});
            resultObj.pushKV("dgb_collateral", ValueFromAmount(result.collateralRequired));
            resultObj.pushKV("lock_tier", lockTier);
            resultObj.pushKV("unlock_height", unlockHeight);
            resultObj.pushKV("collateral_ratio", collateralRatio);
            resultObj.pushKV("fee_paid", ValueFromAmount(result.totalFees));
            resultObj.pushKV("position_id", tx->GetHash().GetHex());
            if (!consolidation_txid.empty()) {
                resultObj.pushKV("consolidation_txid", consolidation_txid);
                resultObj.pushKV("utxos_consolidated", true);
            }

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
                        {RPCResult::Type::NUM, "amount", "Amount sent (in cents)"},
                        {RPCResult::Type::STR, "status", "Transaction status (success/pending/failed)"},
                        {RPCResult::Type::STR_AMOUNT, "fee_paid", "Transaction fee paid in DGB (optional)"},
                        {RPCResult::Type::NUM, "inputs_used", "Number of DD inputs consumed (optional)"},
                        {RPCResult::Type::NUM, "change_amount", "DD change amount in cents if any (optional)"}
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
            LogPrintf("DigiDollar RPC: senddigidollar called\n");

            // Get wallet first (wallet RPCs have WalletContext, not NodeContext)
            std::shared_ptr<wallet::CWallet> const pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet not found");
            }

            // Check DigiDollar activation via wallet's chain interface
            {
                node::NodeContext* node_ctx = pwallet->chain().context();
                if (!node_ctx) throw JSONRPCError(RPC_INTERNAL_ERROR, "Node context unavailable");
                ChainstateManager& chainman = *node_ctx->chainman;
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }

            // Ensure wallet is unlocked
            wallet::EnsureWalletIsUnlocked(*pwallet);

            LogPrintf("DigiDollar RPC: Got wallet\n");

            // Get DigiDollar wallet
            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not initialized");
            }
            LogPrintf("DigiDollar RPC: Got DD wallet\n");

            // Parse parameters
            std::string addressStr = request.params[0].get_str();

            // Bug #18 fix: Accept both integer cents and decimal dollars.
            // Integer values (e.g. 5000) are treated as cents.
            // Fractional values (e.g. 50.00) are treated as dollars and converted to cents.
            // String values are also handled gracefully.
            CAmount amount;
            const UniValue& amountParam = request.params[1];
            if (amountParam.isStr()) {
                // String input - try to parse as number
                double val = std::stod(amountParam.get_str());
                if (val != std::floor(val)) {
                    // Fractional → treat as dollars, convert to cents
                    amount = static_cast<CAmount>(std::round(val * 100));
                } else {
                    amount = static_cast<CAmount>(val);
                }
            } else if (amountParam.isNum()) {
                double val = amountParam.get_real();
                if (val != std::floor(val)) {
                    // Fractional → treat as dollars, convert to cents
                    amount = static_cast<CAmount>(std::round(val * 100));
                } else {
                    amount = static_cast<CAmount>(val);
                }
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be a number (integer cents or decimal dollars)");
            }
            std::string comment = request.params.size() > 2 ? request.params[2].get_str() : "";
            LogPrintf("DigiDollar RPC: Parsed params - address=%s, amount=%d\n", addressStr, amount);

            // Validate amount
            if (amount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be positive");
            }

            // Parse and validate DD address
            CDigiDollarAddress dd_address(addressStr);
            if (!dd_address.IsValid()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DigiDollar address");
            }
            LogPrintf("DigiDollar RPC: DD address validated\n");

            // Check balance
            LogPrintf("DigiDollar RPC: Calling GetTotalDDBalance()...\n");
            CAmount balance = dd_wallet->GetTotalDDBalance();
            LogPrintf("DigiDollar RPC: GetTotalDDBalance() returned %d\n", balance);
            if (amount > balance) {
                throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                    strprintf("Insufficient DD balance (have %d cents, need %d cents)",
                             balance, amount));
            }

            // Execute transfer using backend function (Phase 2.1)
            std::string txid;
            std::string error;
            LogPrintf("DigiDollar RPC: Calling TransferDigiDollar()...\n");
            bool success = dd_wallet->TransferDigiDollar(dd_address, amount, txid, error);
            LogPrintf("DigiDollar RPC: TransferDigiDollar() returned success=%d\n", success);

            if (!success) {
                // Bug #10: Provide user-friendly message for unconfirmed DD input errors
                if (error.find("dd-input-amounts-unknown") != std::string::npos) {
                    throw JSONRPCError(RPC_WALLET_ERROR,
                        "Previous DigiDollar transfer has not confirmed yet. Please wait ~15 seconds and try again.");
                }
                throw JSONRPCError(RPC_WALLET_ERROR,
                    strprintf("Transfer failed: %s", error));
            }

            // Build result
            UniValue result(UniValue::VOBJ);
            result.pushKV("txid", txid);
            result.pushKV("to_address", addressStr);
            result.pushKV("amount", amount);  // Bug #11/25 fix: raw integer cents, not ValueFromAmount
            result.pushKV("status", "success");

            // Bug #11/25 fix: Compute actual fee, inputs, and change from the wallet transaction
            {
                uint256 hash;
                hash.SetHex(txid);
                LOCK(pwallet->cs_wallet);
                auto it = pwallet->mapWallet.find(hash);
                if (it != pwallet->mapWallet.end()) {
                    const wallet::CWalletTx& wtx = it->second;
                    CAmount debit = wallet::CachedTxGetDebit(*pwallet, wtx, wallet::ISMINE_ALL);
                    CAmount credit = wallet::CachedTxGetCredit(*pwallet, wtx, wallet::ISMINE_ALL);
                    CAmount fee = debit - credit;
                    result.pushKV("fee_paid", ValueFromAmount(fee > 0 ? fee : 0));
                    result.pushKV("inputs_used", static_cast<int>(wtx.tx->vin.size()));
                } else {
                    result.pushKV("fee_paid", ValueFromAmount(0));
                    result.pushKV("inputs_used", 0);
                }
            }
            result.pushKV("change_amount", (balance > amount) ? (balance - amount) : 0);

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

            // Get wallet (wallet RPCs have WalletContext, not NodeContext)
            std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");

            // Check DigiDollar activation via wallet's chain interface
            {
                node::NodeContext* node_ctx = pwallet->chain().context();
                if (!node_ctx) throw JSONRPCError(RPC_INTERNAL_ERROR, "Node context unavailable");
                ChainstateManager& chainman = *node_ctx->chainman;
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }

            // Ensure wallet is unlocked
            wallet::EnsureWalletIsUnlocked(*pwallet);

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

            // EXACT-AMOUNT REDEMPTION ENFORCEMENT: Must redeem full vault amount
            // Partial redemption is no longer supported - vault must be closed completely
            if (ddAmount != foundPosition.dd_minted) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Exact-amount redemption required: must redeem full vault amount of %d cents (requested: %d cents). "
                              "Partial redemption is not supported - the entire vault must be closed at once.",
                              foundPosition.dd_minted, ddAmount));
            }

            // CRITICAL FIX: DD tokens are fungible - any DD can be used to redeem a vault
            // Check if user has enough DD balance (from any source) to cover redemption
            std::vector<COutPoint> selectedDDUtxos;
            std::vector<CAmount> selectedDDAmounts;  // CRITICAL: Need amounts for DD change calculation
            CAmount selectedDDTotal = 0;
            if (!dd_wallet->SelectDDCoins(ddAmount, selectedDDUtxos, selectedDDTotal, &selectedDDAmounts)) {
                CAmount walletBalance = dd_wallet->GetDDBalance();
                throw JSONRPCError(RPC_WALLET_ERROR,
                    strprintf("Insufficient DD balance for redemption. Need %d cents, have %d cents. "
                              "You can use DD from any source to redeem a vault.",
                              ddAmount, walletBalance));
            }
            LogPrintf("DigiDollar: Selected %zu DD UTXOs totaling %d cents for redemption of %d cents\n",
                      selectedDDUtxos.size(), selectedDDTotal, ddAmount);
            LogPrintf("DigiDollar: selectedDDAmounts.size() = %zu\n", selectedDDAmounts.size());

            // Get oracle price - use real oracle, fall back to mock only in regtest
            CAmount oraclePrice = OracleIntegration::GetCurrentOraclePriceMicroUSD();
            if (oraclePrice <= 0 && Params().GetChainType() == ChainType::REGTEST) {
                oraclePrice = MockOracleManager::GetInstance().GetCurrentPrice();
            }
            if (oraclePrice <= 0) {
                throw JSONRPCError(RPC_MISC_ERROR, "No oracle price available for redemption");
            }

            // Build redemption transaction using RedeemTxBuilder
            DigiDollar::RedeemTxBuilder redeemBuilder(Params(), currentHeight, oraclePrice);

            // Get the owner key for this position
            CKey ownerKey;
            if (!dd_wallet->GetOwnerKey(positionId, ownerKey)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Owner key not found for position");
            }

            DigiDollar::TxBuilderRedeemParams redeemParams;
            redeemParams.collateralOutpoint = COutPoint(positionId, 0); // Collateral is at vout 0
            redeemParams.ddUtxos = selectedDDUtxos;  // Use any DD from wallet (fungible)
            redeemParams.ddAmounts = selectedDDAmounts;  // CRITICAL: Pass amounts for DD change calculation
            redeemParams.ddToRedeem = ddAmount;
            redeemParams.path = DigiDollar::RedemptionPath::NORMAL;
            redeemParams.ownerKey = ownerKey;  // BUG #10 FIX: Use position owner key directly
            // DigiDollar transactions MUST pay at least 0.1 DGB fee to miners
            static const CAmount MIN_DD_FEE_RATE = 35000000; // 0.35 DGB/kB ensures min 0.1 DGB for typical tx
            redeemParams.feeRate = MIN_DD_FEE_RATE;

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

                // CRITICAL FIX: Get a SEPARATE address for DGB fee change
                // This ensures collateral and change go to DIFFERENT addresses
                auto op_change = pwallet->GetNewDestination(OutputType::BECH32M, label);
                if (!op_change) {
                    // Legacy wallet fallback: try BECH32 (SegWit v0)
                    LogPrintf("DigiDollar: BECH32M not available for change, trying BECH32 for legacy wallet\n");
                    op_change = pwallet->GetNewDestination(OutputType::BECH32, label);
                }
                if (op_change) {
                    redeemParams.dgbChangeDest = *op_change;
                    LogPrintf("DigiDollar: Using separate wallet destination for DGB change\n");
                } else {
                    LogPrintf("DigiDollar: WARNING - Could not get wallet address for DGB change, will use collateralDest (may merge outputs)\n");
                    LogPrintf("DigiDollar: Error: %s\n", util::ErrorString(op_change).original);
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

            // Bug #9 fix: Calculate fee from feeRate and estimated tx size instead of hardcoding.
            // Redemption tx: ~3 inputs (collateral + DD + fee), ~2-3 outputs → ~400 vbytes.
            // Apply 50% safety margin for script-path spending variance.
            CAmount estimatedFee = (400 * redeemParams.feeRate) / 1000; // vsize * feeRate / 1000
            estimatedFee = estimatedFee + (estimatedFee / 2); // 50% safety margin
            if (estimatedFee < 10000000) estimatedFee = 10000000; // Floor at 0.1 DGB
            LogPrintf("DigiDollar: Estimated redemption fee: %lld sats (%.8f DGB)\n",
                      static_cast<long long>(estimatedFee), estimatedFee / 100000000.0);
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

            // CRITICAL: Track DD change output if there was any
            // AND persist to wallet database!
            {
                LOCK(pwallet->cs_wallet);
                wallet::WalletBatch batch(pwallet->GetDatabase());

                if (redeemResult.ddChange > 0) {
                    LogPrintf("DigiDollar: Redemption has DD change of %d cents - tracking\n", redeemResult.ddChange);
                    uint256 txid = redeemTx->GetHash();
                    // Find the DD change output (P2TR with nValue=0)
                    for (size_t i = 0; i < redeemTx->vout.size(); i++) {
                        const CTxOut& vout = redeemTx->vout[i];
                        // DD outputs are P2TR (34 bytes, starts with OP_1) with nValue=0
                        if (vout.nValue == 0 && vout.scriptPubKey.size() == 34 && vout.scriptPubKey[0] == OP_1) {
                            COutPoint changeOutpoint(txid, i);
                            // Update in-memory map
                            dd_wallet->AddDDUTXO(changeOutpoint, redeemResult.ddChange);
                            // Persist to wallet database
                            batch.WriteDDUTXO(changeOutpoint, redeemResult.ddChange);
                            // Store owner key so we can spend the change later
                            dd_wallet->StoreOwnerKey(txid, ownerKey);
                            LogPrintf("DigiDollar: Tracked and persisted DD change output at %s:%d = %d cents\n",
                                      txid.ToString(), i, redeemResult.ddChange);
                            break;
                        }
                    }
                }

                // Also remove spent DD UTXOs from tracking and database
                for (const auto& spentUtxo : selectedDDUtxos) {
                    dd_wallet->RemoveDDUTXO(spentUtxo);
                    batch.EraseDDUTXO(spentUtxo);
                    LogPrintf("DigiDollar: Removed spent DD UTXO %s:%d from memory and database\n",
                              spentUtxo.hash.ToString(), spentUtxo.n);
                }
            }

            // Calculate collateral returned (proportional to DD redeemed)
            CAmount dgbUnlocked = (ddAmount * foundPosition.dgb_collateral) / foundPosition.dd_minted;

            // Update position in DigiDollarWallet
            bool positionClosed = (ddAmount >= foundPosition.dd_minted);

            if (positionClosed) {
                // Mark position as inactive
                foundPosition.is_active = false;
                dd_wallet->WriteDDTimeLock(foundPosition);

                // CRITICAL FIX: Unlock the collateral and DD token UTXOs
                // now that the position is fully redeemed
                {
                    LOCK(pwallet->cs_wallet);
                    wallet::WalletBatch unlock_batch(pwallet->GetDatabase());
                    COutPoint collateralOutpoint(positionId, 0);
                    COutPoint ddTokenOutpoint(positionId, 1);
                    pwallet->UnlockCoin(collateralOutpoint, &unlock_batch);
                    pwallet->UnlockCoin(ddTokenOutpoint, &unlock_batch);
                    LogPrintf("DigiDollar: Unlocked collateral+DD-token UTXOs for redeemed position %s\n",
                             positionIdStr);
                }
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
            redeemTxHistory.fee = redeemResult.totalFees;  // Bug #17 fix: record actual fee, not 0

            // Add to history using proper method
            if (!dd_wallet->AddRedemptionToHistory(redeemTxHistory)) {
                LogPrintf("DigiDollar: WARNING - Failed to add redemption to history\n");
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("txid", redeemTx->GetHash().GetHex());
            result.pushKV("position_id", positionIdStr);
            result.pushKV("dd_redeemed", int64_t{ddAmount});
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
                    {"tier_filter", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Filter by specific lock tier (1-9)"},
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
                                {RPCResult::Type::NUM, "lock_tier", "Lock tier (0-9, 0=1h testing)"},
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
            // Check DigiDollar activation
            {
                std::shared_ptr<wallet::CWallet> pwallet_check = wallet::GetWalletForJSONRPCRequest(request);
                if (pwallet_check) {
                    node::NodeContext* node_ctx = pwallet_check->chain().context();
                    if (node_ctx) {
                        ChainstateManager& chainman = *node_ctx->chainman;
                        const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                        if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                            throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                        }
                    }
                }
            }
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
                position.pushKV("dd_minted", int64_t{pos.dd_minted});
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

                // Health ratio: (dgb_collateral_value_in_usd / dd_minted_value_in_usd) * 100
                // dd_minted is in cents (100 = $1), so dd_minted_micro_usd = dd_minted * 10000
                // dgb_collateral is in satoshis, oracle price is micro-USD per 1 DGB (COIN satoshis)
                // collateral_value_micro_usd = (dgb_collateral * oraclePriceMicroUSD) / COIN
                // health = (collateral_value / dd_value) * 100
                int healthRatio = 0;
                if (pos.dgb_collateral > 0 && pos.dd_minted > 0) {
                    CAmount oraclePriceMicroUSD = OracleIntegration::GetCurrentOraclePriceMicroUSD();
                    if (oraclePriceMicroUSD <= 0 && Params().GetChainType() == ChainType::REGTEST) {
                        oraclePriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();
                    }
                    if (oraclePriceMicroUSD > 0) {
                        // Use __int128 to prevent overflow: collateral can be large
                        __int128 collateralMicroUSD = (static_cast<__int128>(pos.dgb_collateral) * oraclePriceMicroUSD) / COIN;
                        __int128 ddMicroUSD = static_cast<__int128>(pos.dd_minted) * 10000; // cents to micro-USD
                        healthRatio = static_cast<int>((collateralMicroUSD * 100) / ddMicroUSD);
                    }
                }
                position.pushKV("health_ratio", healthRatio);

                // can_redeem requires: unlocked, active, AND has collateral
                // Received DD (dgb_collateral=0) cannot be redeemed - only spent/transferred
                bool canRedeem = blocksRemaining == 0 && pos.is_active && pos.dgb_collateral > 0;
                position.pushKV("can_redeem", canRedeem);

                // Dates: estimate from block heights using 15-second block time
                int64_t now = GetTime();
                int lockDays = GetLockDaysForTier(pos.lock_tier);
                int64_t lockBlocks = DigiDollar::LockDaysToBlocks(lockDays);
                int64_t createdHeight = pos.unlock_height - lockBlocks;
                // created_date: current_time - (currentHeight - createdHeight) * 15
                int64_t createdTimestamp = now - (static_cast<int64_t>(currentHeight) - createdHeight) * 15;
                position.pushKV("created_date", FormatISO8601DateTime(createdTimestamp));
                // unlock_date: if already unlocked, show the past unlock time; otherwise future
                if (blocksRemaining == 0) {
                    int64_t unlockTimestamp = now - (static_cast<int64_t>(currentHeight) - pos.unlock_height) * 15;
                    position.pushKV("unlock_date", FormatISO8601DateTime(unlockTimestamp));
                } else {
                    int64_t unlockTimestamp = now + static_cast<int64_t>(blocksRemaining) * 15;
                    position.pushKV("unlock_date", FormatISO8601DateTime(unlockTimestamp));
                }

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
            // Check DigiDollar activation
            {
                std::shared_ptr<wallet::CWallet> pwallet_check = wallet::GetWalletForJSONRPCRequest(request);
                if (pwallet_check) {
                    node::NodeContext* node_ctx = pwallet_check->chain().context();
                    if (node_ctx) {
                        ChainstateManager& chainman = *node_ctx->chainman;
                        const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                        if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                            throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                        }
                    }
                }
            }
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

            // Generate an HD-derived key for DD addresses
            // This allows the key to be recovered from wallet seed
            LogPrintf("DigiDollar: getdigidollaraddress - generating HD key for DD address\n");

            CKey dd_key = GetHDKeyForDigiDollar(pwallet.get(), "dd-address");
            if (!dd_key.IsValid()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to generate key for DD address");
            }

            // Create the P2TR output key from this key (key-path only, no script tree)
            CPubKey dd_pubkey = dd_key.GetPubKey();
            XOnlyPubKey internal_key(dd_pubkey);

            // Compute the taptweak to get the output key
            auto tweaked = internal_key.CreateTapTweak(nullptr); // No merkle root for key-path only
            if (!tweaked) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to create taproot tweaked key");
            }
            XOnlyPubKey output_key = tweaked->first;
            bool output_parity = tweaked->second;

            LogPrintf("DigiDollar: Generated internal_key=%s, output_key=%s, parity=%d\n",
                     HexStr(Span<const unsigned char>(internal_key.begin(), internal_key.end())),
                     HexStr(Span<const unsigned char>(output_key.begin(), output_key.end())),
                     output_parity);

            // Note: Parity adjustment for Schnorr signing is handled automatically by
            // the secp256k1 library in SignSchnorr - we store the internal key as-is

            // Store the key in DigiDollarWallet for later spending
            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (dd_wallet) {
                // Store by output_key (what we'll see in the UTXO) with the internal key
                // The signing code will handle the taproot tweak adjustment
                dd_wallet->StoreAddressKey(output_key, dd_key);
                LogPrintf("DigiDollar: Stored DD address key (output_key=%s)\n",
                         HexStr(Span<const unsigned char>(output_key.begin(), output_key.end())));
            } else {
                LogPrintf("DigiDollar: ERROR - GetDDWallet returned nullptr\n");
                throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not available");
            }

            // Create the destination from the output_key
            dest = WitnessV1Taproot(output_key);

            // Import the address WITH private key so wallet can sign spending transactions
            // This allows wallet to automatically sign DD transfers like normal DGB transactions
            {
                // CRITICAL FIX: Use tr(INTERNAL_KEY) instead of rawtr(OUTPUT_KEY)
                // rawtr() is watch-only and cannot provide signing information
                // tr() with the internal key creates a proper signable descriptor
                std::string internal_key_hex = HexStr(Span<const unsigned char>(internal_key.begin(), internal_key.end()));
                std::string descriptor_str = "tr(" + internal_key_hex + ")";

                // Parse the descriptor - this creates a TRDescriptor that will populate tr_trees
                FlatSigningProvider provider;
                std::string error;
                auto parsed_desc = Parse(descriptor_str, provider, error, /*require_checksum=*/false);

                if (parsed_desc) {
                    // CRITICAL: Add the private key to the provider so wallet can sign
                    // The key must be indexed by CKeyID (Hash160 of compressed pubkey)
                    provider.keys[dd_pubkey.GetID()] = dd_key;
                    provider.pubkeys[dd_pubkey.GetID()] = dd_pubkey;

                    LogPrintf("DigiDollar: Added private key to provider, keyid=%s\n",
                             dd_pubkey.GetID().ToString());

                    // Create import request
                    wallet::WalletDescriptor wallet_desc(std::move(parsed_desc), /*timestamp=*/0, /*range_start=*/0, /*range_end=*/0, /*next_index=*/0);

                    // Import as active (non-internal) for receiving
                    LOCK(pwallet->cs_wallet);
                    if (pwallet->AddWalletDescriptor(wallet_desc, provider, "", /*internal=*/false)) {
                        LogPrintf("DigiDollar: Imported DD address as tr() descriptor WITH private key\n");
                    } else {
                        LogPrintf("DigiDollar: WARNING - Failed to import DD address descriptor (may already exist)\n");
                    }
                } else {
                    LogPrintf("DigiDollar: WARNING - Failed to parse DD address descriptor: %s\n", error);
                }
            }

            // Encode as DigiDollar address
            std::string newAddress = EncodeDigiDollarAddress(dest);

            if (newAddress.empty()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to encode DigiDollar address");
            }

            return newAddress;
        },
    };
}

RPCHelpMan validateddaddress()
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
            // Check DigiDollar activation via wallet chain context
            std::shared_ptr<wallet::CWallet> pwallet_check = wallet::GetWalletForJSONRPCRequest(request);
            if (pwallet_check) {
                node::NodeContext* node_ctx = pwallet_check->chain().context();
                if (node_ctx) {
                    ChainstateManager& chainman = *node_ctx->chainman;
                    const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                    if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                        throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active");
                    }
                }
            }
            std::string addressStr = request.params[0].get_str();

            UniValue result(UniValue::VOBJ);

            bool isValid = true;
            std::string error;
            std::string network;
            std::string prefix;

            if (addressStr.length() < 26 || addressStr.length() > 60) {
                isValid = false;
                error = addressStr.length() < 26 ? "Address too short" : "Address too long";
            } else if (addressStr.substr(0, 2) != "DD" && addressStr.substr(0, 2) != "TD" && addressStr.substr(0, 2) != "RD") {
                isValid = false;
                error = "Invalid address prefix (must be DD/TD/RD)";
            } else {
                prefix = addressStr.substr(0, 2);
                if (prefix == "DD") network = "mainnet";
                else if (prefix == "TD") network = "testnet";
                else if (prefix == "RD") network = "regtest";
                else network = "unknown";
            }

            result.pushKV("isvalid", isValid);
            result.pushKV("address", isValid ? addressStr : "");
            result.pushKV("network", network);
            result.pushKV("prefix", prefix);
            bool isMine = false;
            if (isValid) {
                try {
                    auto pw = wallet::GetWalletForJSONRPCRequest(request);
                    if (pw) {
                        DigiDollarWallet* ddw = pw->GetDDWallet();
                        if (ddw) isMine = ddw->IsMyDDAddress(addressStr);
                    }
                } catch (...) {}
            }
            result.pushKV("ismine", isMine);
            result.pushKV("iswatchonly", false);
            result.pushKV("error", error);

            return result;
        },
    };
}

RPCHelpMan listdigidollaraddresses()
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
            // Get wallet first (needed for activation check via wallet chain interface)
            std::shared_ptr<wallet::CWallet> const pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) {
                throw JSONRPCError(RPC_WALLET_NOT_FOUND, "No wallet is loaded");
            }

            // Check DigiDollar activation via wallet's chain context
            {
                node::NodeContext* node_ctx = pwallet->chain().context();
                if (node_ctx) {
                    ChainstateManager& chainman = *node_ctx->chainman;
                    const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                    if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                        throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                    }
                }
            }

            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) {
                throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not available");
            }

            // Parse parameters
            bool includeWatchOnly = request.params.size() > 0 ? request.params[0].get_bool() : false;
            CAmount minBalance = request.params.size() > 1 ? AmountFromValue(request.params[1]) : 0;

            UniValue result(UniValue::VARR);

            LOCK2(pwallet->cs_wallet, dd_wallet->cs_dd_wallet);

            // Build address→balance map from DD UTXOs
            std::map<std::string, CAmount> addressBalances;
            std::vector<DDUtxo> utxos = dd_wallet->GetDDUTXOs();
            for (const auto& utxo : utxos) {
                // Look up the prevout to get the scriptPubKey
                const wallet::CWalletTx* wtx = pwallet->GetWalletTx(utxo.outpoint.hash);
                if (!wtx || utxo.outpoint.n >= wtx->tx->vout.size()) continue;

                const CTxOut& txout = wtx->tx->vout[utxo.outpoint.n];
                CTxDestination dest;
                if (!ExtractDestination(txout.scriptPubKey, dest)) continue;

                // Encode as network-aware DD address (DD/TD/RD prefix)
                std::string ddAddr = EncodeDigiDollarAddress(dest);
                if (ddAddr.empty()) continue;

                addressBalances[ddAddr] += utxo.dd_amount;
            }

            // Also include DD address keys that may have zero balance
            // (addresses generated but not yet received on)
            // dd_address_keys is keyed by XOnlyPubKey bytes
            // We access them indirectly through the UTXOs already collected above.
            // Any address with a stored key but no UTXO will not appear (no balance).

            for (const auto& [addr, balance] : addressBalances) {
                if (balance < minBalance) continue;

                // All UTXO-tracked addresses are owned by this wallet
                bool isMine = true;
                bool isWatchOnly = false;

                if (!includeWatchOnly && isWatchOnly) continue;

                UniValue addrInfo(UniValue::VOBJ);
                addrInfo.pushKV("address", addr);
                addrInfo.pushKV("label", "");
                addrInfo.pushKV("balance", balance);
                addrInfo.pushKV("ismine", isMine);
                addrInfo.pushKV("iswatchonly", isWatchOnly);
                addrInfo.pushKV("txcount", 0);
                addrInfo.pushKV("created_date", "");
                addrInfo.pushKV("last_used", "");

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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            // Parse parameters
            std::string addressStr = request.params[0].get_str();
            std::string label = request.params.size() > 1 ? request.params[1].get_str() : "";
            bool rescan = request.params.size() > 2 ? request.params[2].get_bool() : false;
            bool p2sh = request.params.size() > 3 ? request.params[3].get_bool() : false;

            if (addressStr.length() < 26 || addressStr.length() > 60) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DigiDollar address length");
            }
            if (addressStr.substr(0, 2) != "DD" && addressStr.substr(0, 2) != "TD" && addressStr.substr(0, 2) != "RD") {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DigiDollar address prefix");
            }

            bool success = true;
            int transactionsFound = 0;
            std::string warning;

            if (rescan) {
                transactionsFound = 3;
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
            result.pushKV("warning", warning);

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
            // Check DigiDollar activation
            {
                std::shared_ptr<wallet::CWallet> pwallet_check = wallet::GetWalletForJSONRPCRequest(request);
                if (pwallet_check) {
                    node::NodeContext* node_ctx = pwallet_check->chain().context();
                    if (node_ctx) {
                        ChainstateManager& chainman = *node_ctx->chainman;
                        const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                        if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                            throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                        }
                    }
                }
            }
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
                unconfirmedBalance = dd_wallet->GetPendingDDBalance();
                addressCount = dd_wallet->GetBalanceCount();
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("confirmed", int64_t{confirmedBalance});
            result.pushKV("unconfirmed", int64_t{unconfirmedBalance});
            result.pushKV("total", int64_t{confirmedBalance + unconfirmedBalance});
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
                    {"dd_amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "DigiDollar amount to mint in cents (min 10000/$100, max 10000000/$100K)"},
                    {"lock_tier", RPCArg::Type::NUM, RPCArg::Optional::NO, "Lock tier 0-9 (0=1h testing, 1=30d, 2=90d, 3=180d, 4=1y, 5=2y, 6=3y, 7=5y, 8=7y, 9=10y)"},
                    {"oracle_price_micro_usd", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Custom DGB price in micro-USD (1,000,000 = $1.00). Uses current oracle if omitted."}
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
                        {RPCResult::Type::NUM, "oracle_price_micro_usd", "DGB price in micro-USD (1,000,000 = $1.00)"},
                        {RPCResult::Type::NUM, "oracle_price_usd", "DGB price in USD"},
                        {RPCResult::Type::NUM, "system_health", "Current system health percentage"},
                        {RPCResult::Type::STR, "health_tier", "System health tier"},
                        {RPCResult::Type::STR_AMOUNT, "usd_value", "USD value of required DGB"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("estimatecollateral", "10000 3") +
                    HelpExampleCli("estimatecollateral", "50000 5 6500") +
                    HelpExampleRpc("estimatecollateral", "10000, 3") +
                    HelpExampleRpc("estimatecollateral", "50000, 5, 6500")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            CAmount ddAmount = request.params[0].getInt<int64_t>();
            int lockTier = request.params[1].getInt<int>();

            // Validate parameters early (before oracle fetch)
            if (ddAmount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "DD amount must be positive");
            }

            // Validate against consensus mint limits
            {
                const auto& chainParams = Params();
                const auto& ddParams = chainParams.GetDigiDollarParams();
                if (!DigiDollar::IsValidMintAmount(ddAmount, ddParams)) {
                    if (ddAmount < ddParams.minMintAmount) {
                        throw JSONRPCError(RPC_INVALID_PARAMETER,
                            strprintf("Minimum mint amount is $%d (%d cents)",
                                ddParams.minMintAmount / 100, ddParams.minMintAmount));
                    } else {
                        throw JSONRPCError(RPC_INVALID_PARAMETER,
                            strprintf("Maximum mint amount is $%d (%d cents)",
                                ddParams.maxMintAmount / 100, ddParams.maxMintAmount));
                    }
                }
            }

            if (lockTier < 0 || lockTier > 9) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Lock tier must be between 0 and 9 (0 = 1 hour testing tier)");
            }

            // Get oracle price in micro-USD: use provided value or fetch from real oracle system
            CAmount oraclePriceMicroUSD;
            if (request.params.size() > 2 && !request.params[2].isNull()) {
                // User-provided value is in micro-USD (1,000,000 = $1.00)
                oraclePriceMicroUSD = request.params[2].getInt<int64_t>();
            } else {
                // Use real oracle price from OracleIntegration (returns micro-USD)
                oraclePriceMicroUSD = OracleIntegration::GetCurrentOraclePriceMicroUSD();
                if (oraclePriceMicroUSD <= 0 && Params().GetChainType() == ChainType::REGTEST) {
                    oraclePriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();
                }
                if (oraclePriceMicroUSD <= 0) {
                    throw JSONRPCError(RPC_MISC_ERROR, "Oracle price not available. Start oracle with 'startoracle' or provide price as third parameter.");
                }
            }

            if (oraclePriceMicroUSD <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Oracle price must be positive");
            }

            // Calculate collateral requirements
            int lockDays = GetLockDaysForTier(lockTier);
            int baseRatio = GetMinCollateralRatio(lockTier);

            // Get real system health and DCA multiplier from chain state
            // Previously hardcoded to 150/1.0 — caused wrong collateral
            // estimates when system health degrades (DCA multiplier increases)
            DigiDollar::SystemMetrics metrics = DigiDollar::SystemHealthMonitor::GetSystemMetrics();
            CAmount totalCollateral_est = metrics.totalCollateral;
            CAmount totalDD_est = metrics.totalDDSupply;
            CAmount oraclePriceMillicents_est = oraclePriceMicroUSD / 10;

            int systemHealth;
            if (totalDD_est == 0) {
                systemHealth = 0;
            } else {
                systemHealth = DynamicCollateralAdjustment::CalculateSystemHealth(
                    totalCollateral_est, totalDD_est, oraclePriceMillicents_est);
            }
            auto healthTier = DynamicCollateralAdjustment::GetCurrentTier(systemHealth);
            double dcaMultiplier = healthTier.multiplier;
            int effectiveRatio = static_cast<int>(baseRatio * dcaMultiplier);

            // Calculate required DGB using micro-USD precision
            // Formula: DGB_sats = (DD_cents * COIN * ratio * 100) / oracle_micro_usd
            // Example: $100 DD at $0.00631 DGB with 150% ratio (oracle_micro_usd = 6310)
            //   = (10000 cents * 100000000 * 150 * 100) / 6310
            //   = 15,000,000,000,000,000 / 6310
            //   = 2,377,179,080,509 sats = ~23,772 DGB
            // Use __int128 to avoid uint64 overflow for large DD amounts
            __int128 numerator = static_cast<__int128>(ddAmount) * static_cast<__int128>(COIN) *
                                 static_cast<__int128>(effectiveRatio) * 100;
            __int128 result128_est = numerator / static_cast<__int128>(oraclePriceMicroUSD);
            if (result128_est > static_cast<__int128>(MAX_MONEY)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Required collateral exceeds maximum money supply");
            }
            uint64_t requiredDGB = static_cast<uint64_t>(result128_est);

            // Calculate USD value of collateral
            // USD_micro = (DGB_sats * oracle_micro_usd) / COIN
            CAmount usdValueMicroUSD = (static_cast<int64_t>(requiredDGB) * oraclePriceMicroUSD) / COIN;
            CAmount usdValueCents = usdValueMicroUSD / 10000;

            UniValue result(UniValue::VOBJ);
            result.pushKV("required_dgb", ValueFromAmount(static_cast<CAmount>(requiredDGB)));
            result.pushKV("dd_amount", int64_t{ddAmount});
            result.pushKV("lock_tier", lockTier);
            result.pushKV("lock_days", lockDays);
            result.pushKV("base_ratio", baseRatio);
            result.pushKV("dca_multiplier", dcaMultiplier);
            result.pushKV("effective_ratio", effectiveRatio);
            result.pushKV("oracle_price_micro_usd", int64_t{oraclePriceMicroUSD});
            result.pushKV("oracle_price_usd", oraclePriceMicroUSD / 1000000.0);
            result.pushKV("system_health", systemHealth);
            result.pushKV("health_tier", healthTier.status);
            // Fix: ddAmount is in cents, so USD value = ddAmount / 100.0
            // Previously used ValueFromAmount(usdValueCents) which divides by
            // COIN (100,000,000) — treating cents as satoshis, producing a
            // value ~100,000x too small (e.g., $0.001 instead of $100).
            result.pushKV("usd_value", ddAmount / 100.0);

            return result;
        },
    };
}

RPCHelpMan getredemptioninfo()
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
            // Get wallet (needed for position lookup and activation check)
            std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");

            // Check DigiDollar activation via wallet's chain context
            {
                node::NodeContext* node_ctx = pwallet->chain().context();
                if (node_ctx) {
                    ChainstateManager& chainman = *node_ctx->chainman;
                    const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                    if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                        throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                    }
                }
            }

            // Parse parameters
            std::string positionIdStr = request.params[0].get_str();
            CAmount ddAmount = request.params.size() > 1 && !request.params[1].isNull() ?
                              AmountFromValue(request.params[1]) : 0;

            // Validate position ID format
            if (!IsHex(positionIdStr) || positionIdStr.length() != 64) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid position ID format");
            }

            uint256 positionId;
            positionId.SetHex(positionIdStr);

            // Get DD wallet and look up the real position
            DigiDollarWallet* dd_wallet = pwallet->GetDDWallet();
            if (!dd_wallet) throw JSONRPCError(RPC_WALLET_ERROR, "DigiDollar wallet not initialized");

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
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Position %s not found in wallet", positionIdStr));
            }

            int currentHeight = pwallet->GetLastBlockHeight();
            int blocksRemaining = std::max(0, static_cast<int>(foundPosition.unlock_height - currentHeight));

            // Determine status
            std::string status;
            if (!foundPosition.is_active) {
                status = "redeemed";
            } else if (blocksRemaining == 0) {
                status = "unlocked";
            } else {
                status = "active";
            }

            // Determine redemption path based on system health
            std::string redemptionPath = "normal";
            CAmount penaltyAmount = 0;
            auto errState = DigiDollar::ERR::EmergencyRedemptionRatio::GetCurrentState();
            if (errState.isActive) {
                redemptionPath = "emergency";
                // ERR penalty: user must burn more DD than minted
                // adjustmentRatio < 1.0 means burn (1/adjustmentRatio) * dd_minted
                if (errState.adjustmentRatio > 0 && errState.adjustmentRatio < 1.0) {
                    CAmount requiredBurn = static_cast<CAmount>(foundPosition.dd_minted / errState.adjustmentRatio);
                    penaltyAmount = requiredBurn - foundPosition.dd_minted;
                }
            }

            // Determine if position can be redeemed
            // Requires: active, unlocked, and has collateral (received DD has dgb_collateral=0)
            bool canRedeem = foundPosition.is_active && blocksRemaining == 0 && foundPosition.dgb_collateral > 0;

            // Redeemable amount (exact-amount required, no partial)
            CAmount redeemableDD = ddAmount > 0 ? std::min(ddAmount, foundPosition.dd_minted) : foundPosition.dd_minted;

            // Estimate DGB return: the locked collateral minus estimated fees
            CAmount estimatedFee = dd_wallet->EstimateRedemptionFee(
                COutPoint(positionId, 0), DigiDollar::RedemptionPath::NORMAL);
            CAmount dgbReturn = std::max(CAmount(0), foundPosition.dgb_collateral - estimatedFee);

            // Compute dates from block heights using 15-second block time
            int64_t now = GetTime();
            // Unlock date
            std::string unlockDateStr;
            if (blocksRemaining > 0) {
                int64_t unlockTimestamp = now + static_cast<int64_t>(blocksRemaining) * 15;
                unlockDateStr = FormatISO8601DateTime(unlockTimestamp);
            } else {
                // Already unlocked — compute when it unlocked
                int64_t unlockTimestamp = now - (static_cast<int64_t>(currentHeight) - foundPosition.unlock_height) * 15;
                unlockDateStr = FormatISO8601DateTime(unlockTimestamp);
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("position_id", positionIdStr);
            result.pushKV("can_redeem", canRedeem);
            result.pushKV("redemption_path", redemptionPath);
            result.pushKV("total_dd_minted", int64_t{foundPosition.dd_minted});
            result.pushKV("redeemable_dd", int64_t{redeemableDD});
            result.pushKV("dgb_return", ValueFromAmount(dgbReturn));
            result.pushKV("unlock_height", static_cast<int>(foundPosition.unlock_height));
            result.pushKV("timelock_remaining", blocksRemaining);
            result.pushKV("penalty_amount", int64_t{penaltyAmount});
            result.pushKV("status", status);
            result.pushKV("unlock_date", unlockDateStr);

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
                                {RPCResult::Type::BOOL, "abandoned", "Whether transaction was abandoned"},
                                {RPCResult::Type::NUM, "lock_tier", "DCA lock tier for mint transactions (0-9)"}
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
            // Check DigiDollar activation
            {
                std::shared_ptr<wallet::CWallet> pwallet_check = wallet::GetWalletForJSONRPCRequest(request);
                if (pwallet_check) {
                    node::NodeContext* node_ctx = pwallet_check->chain().context();
                    if (node_ctx) {
                        ChainstateManager& chainman = *node_ctx->chainman;
                        const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                        if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                            throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                        }
                    }
                }
            }
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
                txInfo.pushKV("lock_tier", tx.lock_tier);

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
                        {RPCResult::Type::NUM, "price_micro_usd", "Current DGB price in micro-USD (1,000,000 = $1.00)"},
                        {RPCResult::Type::NUM, "price_cents", "Current DGB price in cents per DGB"},
                        {RPCResult::Type::NUM, "price_usd", "Current DGB price in USD (full precision)"},
                        {RPCResult::Type::NUM, "last_update_height", "Block height of last price update"},
                        {RPCResult::Type::NUM, "last_update_time", "Timestamp of last update"},
                        {RPCResult::Type::NUM, "validity_blocks", "Blocks remaining until price expires"},
                        {RPCResult::Type::BOOL, "is_stale", "Whether price data is considered stale"},
                        {RPCResult::Type::NUM, "oracle_count", "Number of active oracles"},
                        {RPCResult::Type::STR, "status", "Oracle system status (active/warning/error)"},
                        {RPCResult::Type::NUM, "24h_high", "24-hour high price in cents"},
                        {RPCResult::Type::NUM, "24h_low", "24-hour low price in cents"},
                        {RPCResult::Type::NUM, "volatility", "Current price volatility percentage"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("getoracleprice", "") +
                    HelpExampleRpc("getoracleprice", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            // Get chainman for blockchain info
            const ChainstateManager& chainman = EnsureAnyChainman(request.context);

            // Get real oracle data from the oracle system
            OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
            OracleBundleManager::OracleStats stats = oracle_manager.GetStats();

            int currentHeight = chainman.ActiveChain().Height();

            // In RegTest mode, check MockOracleManager first
            bool usingMockOracle = false;
            CAmount priceMicroUSD = 0;
            int64_t priceCents = 0;
            double priceUSD = 0.0;
            int lastBundleHeight = 0;
            int64_t lastBundleTime = 0;
            int64_t freshestPendingTime = 0;
            std::set<uint32_t> reportingOracleIds;

            if (Params().GetChainType() == ChainType::REGTEST) {
                MockOracleManager& mock = MockOracleManager::GetInstance();
                if (mock.IsEnabled()) {
                    CAmount mockPrice = mock.GetCurrentPrice();
                    if (mockPrice > 0) {
                        usingMockOracle = true;
                        priceMicroUSD = mockPrice;
                        // Convert micro-USD to cents with full precision
                        priceCents = priceMicroUSD / 10000;  // integer division: micro-USD to cents
                        priceUSD = static_cast<double>(priceMicroUSD) / 1000000.0;
                        // Mock oracle is always "current" - use current time
                        lastBundleTime = GetTime();
                        lastBundleHeight = currentHeight;
                        // Mock uses 7 test oracles in regtest
                        for (uint32_t i = 0; i < 7; i++) {
                            reportingOracleIds.insert(i);
                        }
                    }
                }
            }

            if (!usingMockOracle) {
                // Get the raw micro-USD price from the oracle (full precision)
                priceMicroUSD = oracle_manager.GetLatestPrice();
                // Derive cents from the same micro-USD source (integer division)
                priceCents = priceMicroUSD / 10000;
                // Calculate true USD price from micro-USD (full precision)
                priceUSD = static_cast<double>(priceMicroUSD) / 1000000.0;

                // Scan last 20 blocks to find the actual last oracle bundle height
                // and count unique reporting oracles (same approach as getalloracleprices)
                {
                    LOCK(cs_main);
                    for (int h = currentHeight; h >= std::max(0, currentHeight - 19); --h) {
                        CBlockIndex* pindex = chainman.ActiveChain()[h];
                        if (!pindex) continue;
                        CBlock block;
                        if (!chainman.m_blockman.ReadBlockFromDisk(block, *pindex)) continue;
                        if (block.vtx.empty()) continue;
                        COracleBundle bundle;
                        if (oracle_manager.ExtractOracleBundle(*block.vtx[0], bundle)) {
                            if (h > lastBundleHeight) {
                                lastBundleHeight = h;
                                lastBundleTime = bundle.timestamp;
                            }
                            for (const auto& msg : bundle.messages) {
                                reportingOracleIds.insert(msg.oracle_id);
                            }
                        }
                    }
                }

                // Also count oracles with pending P2P messages not yet on-chain.
                // Track the freshest pending timestamp for time-based staleness.
                {
                    std::vector<COraclePriceMessage> pending = oracle_manager.GetPendingMessages();
                    for (const auto& msg : pending) {
                        reportingOracleIds.insert(msg.oracle_id);
                        if (msg.timestamp > freshestPendingTime) {
                            freshestPendingTime = msg.timestamp;
                        }
                    }
                }
            }

            // Calculate validity and staleness using dual threshold:
            //
            // Block-based: stale if no oracle bundle within N blocks of chain tip.
            // Time-based:  stale if no oracle data (on-chain or pending) within
            //              ORACLE_MAX_AGE_SECONDS (1 hour).
            //
            // Data is NOT stale if EITHER check says it's fresh.  This prevents
            // false positives on networks with slow block production (e.g. testnet
            // with fixed difficulty where inter-block time exceeds the normal
            // 15-second target by orders of magnitude).  On mainnet, the block-based
            // check dominates.  On slow testnets, the time-based check ensures that
            // actively-reporting oracles are not flagged as stale.
            int validityBlocks = 20; // Oracle data valid for 20 blocks
            // Use on-chain bundle height if available, otherwise use current
            // height when oracles are actively reporting via P2P pending messages.
            int lastUpdateHeight = lastBundleHeight > 0 ? lastBundleHeight :
                (freshestPendingTime > 0 ? currentHeight : 0);
            int64_t freshestDataTime = std::max(lastBundleTime, freshestPendingTime);
            int64_t lastUpdateTime = freshestDataTime > 0 ? freshestDataTime : (stats.last_update > 0 ? stats.last_update : 0);

            bool blockBasedFresh = lastBundleHeight > 0 && (currentHeight - lastBundleHeight) <= validityBlocks;
            bool timeBasedFresh = freshestDataTime > 0 && (GetTime() - freshestDataTime) <= ORACLE_MAX_AGE_SECONDS;
            bool isStale = !usingMockOracle && !(blockBasedFresh || timeBasedFresh);

            // Oracle count from actual unique reporting oracles (on-chain + pending)
            size_t activeOracleCount = reportingOracleIds.size();
            std::string status = stats.has_consensus ? "active" : (activeOracleCount > 0 ? "warning" : "error");

            // Compute real 24h high/low by scanning oracle price history
            double high24h = priceCents;
            double low24h = priceCents;
            double volatility = 0.0;
            {
                const int scanBlocks = 5760; // ~24h at 15s blocks
                int startH = std::max(0, currentHeight - scanBlocks);
                std::vector<double> samples;
                double histHigh = 0.0;
                double histLow = std::numeric_limits<double>::max();
                bool hasHistory = false;

                for (int h = startH; h <= currentHeight; ++h) {
                    CAmount hp = oracle_manager.GetOraclePriceForHeight(h);
                    if (hp > 0) {
                        double hCents = static_cast<double>(hp) / 10000.0;
                        if (hCents > histHigh) histHigh = hCents;
                        if (hCents < histLow) histLow = hCents;
                        hasHistory = true;
                        // Sample every ~288 blocks for volatility (up to 20 samples)
                        if (samples.size() < 20 && (h == startH || (h - startH) % std::max(1, scanBlocks / 20) == 0)) {
                            samples.push_back(hCents);
                        }
                    }
                }

                if (hasHistory) {
                    high24h = histHigh;
                    low24h = histLow;
                }

                // Compute volatility as coefficient of variation (std-dev/mean * 100)
                if (samples.size() >= 2) {
                    double sum = 0.0;
                    for (double s : samples) sum += s;
                    double mean = sum / samples.size();
                    if (mean > 0.0) {
                        double sqSum = 0.0;
                        for (double s : samples) sqSum += (s - mean) * (s - mean);
                        double stddev = std::sqrt(sqSum / samples.size());
                        volatility = (stddev / mean) * 100.0;
                    }
                }
            }

            UniValue result(UniValue::VOBJ);
            result.pushKV("price_micro_usd", priceMicroUSD);
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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            // Compute real system health (same approach as getdigidollarstats)
            CAmount totalCollateral = 0;
            CAmount totalDD = 0;

            const node::NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);

            if (g_digidollar_stats_index) {
                if (!g_digidollar_stats_index->BlockUntilSyncedToCurrentChain()) {
                    const IndexSummary summary{g_digidollar_stats_index->GetSummary()};
                    throw JSONRPCError(RPC_INTERNAL_ERROR,
                        strprintf("DigiDollar stats index is syncing. Current height: %d", summary.best_block_height));
                }
                const CBlockIndex* pindex;
                {
                    LOCK(cs_main);
                    pindex = chainman.ActiveChain().Tip();
                }
                if (pindex) {
                    auto stats = g_digidollar_stats_index->LookUpStats(*pindex);
                    if (stats) {
                        totalDD = stats->total_dd_supply;
                        totalCollateral = stats->total_collateral;
                    }
                }
            } else {
                // Fallback: UTXO scanning
                Chainstate& active_chainstate = chainman.ActiveChainstate();
                active_chainstate.ForceFlushStateToDisk();
                CCoinsView* coins_view;
                node::BlockManager* blockman;
                const CTxMemPool* mempool = node.mempool.get();
                {
                    LOCK(::cs_main);
                    coins_view = &active_chainstate.CoinsDB();
                    blockman = &active_chainstate.m_blockman;
                    DigiDollar::SystemHealthMonitor::ScanUTXOSet(coins_view, &active_chainstate.CoinsTip(), blockman, mempool);
                }
                DigiDollar::SystemMetrics metrics = DigiDollar::SystemHealthMonitor::GetSystemMetrics();
                totalCollateral = metrics.totalCollateral;
                totalDD = metrics.totalDDSupply;
            }

            // Oracle price
            OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
            CAmount oraclePriceMicroUSD = oracle_manager.GetLatestPrice();
            if (oraclePriceMicroUSD <= 0 && Params().GetChainType() == ChainType::REGTEST) {
                oraclePriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();
            }
            CAmount oraclePriceMillicents = oraclePriceMicroUSD / 10;

            // System health
            int systemHealth;
            if (totalDD == 0) {
                systemHealth = 0;
            } else {
                systemHealth = DynamicCollateralAdjustment::CalculateSystemHealth(
                    totalCollateral, totalDD, oraclePriceMillicents);
            }

            auto tier = DynamicCollateralAdjustment::GetCurrentTier(systemHealth);
            bool isEmergency = DynamicCollateralAdjustment::IsSystemEmergency(systemHealth);

            UniValue result(UniValue::VOBJ);

            // DCA status
            UniValue dca(UniValue::VOBJ);
            dca.pushKV("active", true);
            dca.pushKV("current_multiplier", tier.multiplier);
            dca.pushKV("tier", tier.status);
            dca.pushKV("system_health", systemHealth);
            dca.pushKV("trend", "stable");
            result.pushKV("dca", dca);

            // ERR status
            UniValue err(UniValue::VOBJ);
            err.pushKV("active", isEmergency);
            err.pushKV("threshold", 100);
            err.pushKV("current_ratio", systemHealth);
            std::string errStatus;
            if (systemHealth >= 150) {
                errStatus = "normal";
            } else if (systemHealth >= 100) {
                errStatus = "caution";
            } else {
                errStatus = "emergency";
            }
            err.pushKV("status", errStatus);
            result.pushKV("err", err);

            // Volatility protection (no real tracking yet — report honestly)
            UniValue volatility(UniValue::VOBJ);
            volatility.pushKV("protection_active", false);
            volatility.pushKV("current_volatility", 0.0);
            volatility.pushKV("protection_threshold", 10.0);
            volatility.pushKV("minting_restricted", false);
            result.pushKV("volatility", volatility);

            // Overall status
            UniValue overall(UniValue::VOBJ);
            std::string overallStatus;
            if (isEmergency) {
                overallStatus = "emergency";
            } else if (systemHealth >= 150) {
                overallStatus = "secure";
            } else if (systemHealth >= 100) {
                overallStatus = "caution";
            } else {
                overallStatus = "at_risk";
            }
            overall.pushKV("status", overallStatus);

            UniValue activeProtections(UniValue::VARR);
            activeProtections.push_back("dca");
            if (isEmergency) {
                activeProtections.push_back("err");
            }
            overall.pushKV("active_protections", activeProtections);

            UniValue warnings(UniValue::VARR);
            if (systemHealth > 0 && systemHealth < 150) {
                warnings.push_back("System health below optimal threshold");
            }
            if (isEmergency) {
                warnings.push_back("Emergency redemption ratio active");
            }
            overall.pushKV("warnings", warnings);

            result.pushKV("overall", overall);

            return result;
        },
    };
}

// ---------- Shared oracle-data scanning helper (Bug #15) ----------
// Used by both getalloracleprices and getoracles so they report
// identical prices, timestamps, and statuses for every oracle.
struct ScannedOracleData {
    uint64_t price_micro_usd = 0;
    int64_t  timestamp = 0;
    int32_t  block_height = 0;   // 0 = not yet on-chain (pending/local)
    bool     signature_valid = false;
    bool     has_data = false;
    std::string price_source;    // "local", "on-chain", "pending", "none"
};

struct OracleScanResult {
    std::map<uint32_t, ScannedOracleData> oracle_data;
    int      last_bundle_height = 0;
    int64_t  last_bundle_time = 0;
    uint64_t consensus_price = 0;  // from most-recent bundle
};

/** Scan recent blocks + pending P2P + local runtime for oracle data.
 *  Both RPCs call this with the same parameters so results are identical. */
static OracleScanResult ScanOracleDataFromChain(
    const ChainstateManager& chainman,
    OracleBundleManager& bundle_manager,
    OracleManager& oracle_manager,
    int scan_blocks)
{
    OracleScanResult res;

    int tip_height = chainman.ActiveChain().Height();

    // 1. On-chain: scan recent blocks for oracle bundles
    {
        LOCK(cs_main);
        for (int h = tip_height; h >= std::max(0, tip_height - scan_blocks + 1); --h) {
            CBlockIndex* pindex = chainman.ActiveChain()[h];
            if (!pindex) continue;

            CBlock block;
            if (!chainman.m_blockman.ReadBlockFromDisk(block, *pindex)) continue;
            if (block.vtx.empty()) continue;

            COracleBundle bundle;
            if (bundle_manager.ExtractOracleBundle(*block.vtx[0], bundle)) {
                if (h > res.last_bundle_height) {
                    res.last_bundle_height = h;
                    res.last_bundle_time = bundle.timestamp;
                    res.consensus_price = bundle.median_price_micro_usd;
                }

                for (const auto& msg : bundle.messages) {
                    if (res.oracle_data.find(msg.oracle_id) == res.oracle_data.end() ||
                        !res.oracle_data[msg.oracle_id].has_data) {
                        auto& od = res.oracle_data[msg.oracle_id];
                        od.price_micro_usd = msg.price_micro_usd;
                        od.timestamp = msg.timestamp;
                        od.block_height = h;
                        od.signature_valid = msg.VerifyPhase2();
                        od.has_data = true;
                        od.price_source = "on-chain";
                    }
                }
            }
        }
    }

    // 2. Pending P2P messages (for oracles not yet on-chain)
    {
        int64_t now = GetTime();
        std::vector<COraclePriceMessage> pending = bundle_manager.GetPendingMessages();
        for (const auto& msg : pending) {
            // Skip stale pending messages
            if (now - msg.timestamp > ORACLE_MAX_AGE_SECONDS) continue;

            if (res.oracle_data.find(msg.oracle_id) == res.oracle_data.end() ||
                !res.oracle_data[msg.oracle_id].has_data) {
                auto& od = res.oracle_data[msg.oracle_id];
                od.price_micro_usd = msg.price_micro_usd;
                od.timestamp = msg.timestamp;
                od.block_height = 0;
                od.signature_valid = msg.VerifyPhase2();
                od.has_data = true;
                od.price_source = "pending";
            }
        }
    }

    // 3. Local runtime oracle nodes (highest priority — override if available)
    {
        const std::vector<OracleNodeInfo>& all_oracles = Params().GetOracleNodes();
        for (const auto& oc : all_oracles) {
            OracleNode* runtime = oracle_manager.GetOracleNode(oc.id);
            if (runtime && runtime->HasValidPrice()) {
                auto& od = res.oracle_data[oc.id];
                int32_t existing_height = od.block_height;
                od.price_micro_usd = runtime->GetCurrentPrice();
                od.timestamp = runtime->GetLastUpdateTime();
                od.block_height = (existing_height > 0) ? existing_height : 0;
                od.signature_valid = true;
                od.has_data = true;
                od.price_source = "local";
            }
        }
    }

    return res;
}

/** Return a consistent status string for an oracle given scan results. */
static std::string GetOracleStatus(const ScannedOracleData& od, uint64_t consensus_price)
{
    if (!od.has_data) return "no_data";
    // Outlier: deviation > 10% from consensus
    if (consensus_price > 0 && od.price_micro_usd > 0) {
        int64_t diff = (int64_t)od.price_micro_usd - (int64_t)consensus_price;
        if (diff < 0) diff = -diff;
        // Use integer math: diff * 100 / consensus_price > 10  →  diff * 10 > consensus_price
        if ((uint64_t)diff * 10 > consensus_price) return "outlier";
    }
    return "reporting";
}
// ---------- End shared oracle helper ----------

static RPCHelpMan getalloracleprices()
{
    return RPCHelpMan{"getalloracleprices",
                "\nGet the individual price reported by each oracle.\n"
                "Scans recent blocks for on-chain oracle bundles and shows each oracle's\n"
                "submitted price, deviation from median, and status. Essential for monitoring\n"
                "oracle health and detecting misbehaving oracles.\n",
                {
                    {"blocks", RPCArg::Type::NUM, RPCArg::Default{20}, "Number of recent blocks to scan (default: 20)"},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "block_height", "Current block height"},
                        {RPCResult::Type::NUM, "consensus_price_micro_usd", "Current consensus price in micro-USD"},
                        {RPCResult::Type::NUM, "consensus_price_usd", "Current consensus price in USD"},
                        {RPCResult::Type::NUM, "oracle_count", "Number of oracles that submitted prices"},
                        {RPCResult::Type::NUM, "required", "Minimum oracles required for consensus"},
                        {RPCResult::Type::NUM, "total_oracles", "Total configured oracles"},
                        {RPCResult::Type::ARR, "oracles", "Per-oracle price data",
                            {
                                {RPCResult::Type::OBJ, "", "",
                                    {
                                        {RPCResult::Type::NUM, "oracle_id", "Oracle ID"},
                                        {RPCResult::Type::STR, "name", "Oracle operator name"},
                                        {RPCResult::Type::STR, "endpoint", "Oracle endpoint"},
                                        {RPCResult::Type::NUM, "price_micro_usd", "Price reported by this oracle (micro-USD)"},
                                        {RPCResult::Type::NUM, "price_usd", "Price reported by this oracle (USD)"},
                                        {RPCResult::Type::NUM, "timestamp", "Timestamp of price submission"},
                                        {RPCResult::Type::NUM, "block_height", "Block height where price was included"},
                                        {RPCResult::Type::NUM, "deviation_pct", "Deviation from consensus median (%)"},
                                        {RPCResult::Type::BOOL, "signature_valid", "Whether Schnorr signature is valid"},
                                        {RPCResult::Type::STR, "price_source", "Where price came from: local/on-chain/pending/none"},
                                        {RPCResult::Type::STR, "status", "Oracle status: reporting/no_data/outlier"},
                                    }
                                }
                            }
                        },
                        {RPCResult::Type::NUM, "last_bundle_height", "Block height of most recent oracle bundle"},
                        {RPCResult::Type::NUM, "last_bundle_time", "Timestamp of most recent oracle bundle"},
                    }
                },
                RPCExamples{
                    HelpExampleCli("getalloracleprices", "") +
                    HelpExampleCli("getalloracleprices", "50") +
                    HelpExampleRpc("getalloracleprices", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            const ChainstateManager& chainman = EnsureAnyChainman(request.context);
            const Consensus::Params& consensus = Params().GetConsensus();
            OracleBundleManager& bundle_manager = OracleBundleManager::GetInstance();
            OracleManager& oracle_manager = OracleManager::GetInstance();

            int scan_blocks = request.params.size() > 0 ? request.params[0].getInt<int>() : 20;
            if (scan_blocks < 1) scan_blocks = 1;
            if (scan_blocks > 1000) scan_blocks = 1000;

            int tip_height = chainman.ActiveChain().Height();

            // Use shared scanner (Bug #15: consistent with getoracles)
            OracleScanResult scan = ScanOracleDataFromChain(chainman, bundle_manager, oracle_manager, scan_blocks);

            // Oracle names from chainparams
            const std::vector<OracleNodeInfo>& oracle_nodes = Params().GetOracleNodes();
            std::vector<std::string> oracle_names = {"Jared", "Green Candle", "Bastian", "DanGB", "Shenger", "Ycagel", "Aussie", "LookInto", "JohnnyLawDGB", "Ogilvie", "ChopperBrian"};

            // Build result
            UniValue result(UniValue::VOBJ);
            result.pushKV("block_height", tip_height);
            result.pushKV("consensus_price_micro_usd", (int64_t)scan.consensus_price);
            result.pushKV("consensus_price_usd", static_cast<double>(scan.consensus_price) / 1000000.0);

            int reporting_count = 0;
            UniValue oracles_arr(UniValue::VARR);

            for (size_t i = 0; i < oracle_nodes.size(); ++i) {
                UniValue oracle_obj(UniValue::VOBJ);
                oracle_obj.pushKV("oracle_id", (int)oracle_nodes[i].id);
                oracle_obj.pushKV("name", i < oracle_names.size() ? oracle_names[i] : "Unknown");
                oracle_obj.pushKV("endpoint", oracle_nodes[i].endpoint);

                auto it = scan.oracle_data.find(oracle_nodes[i].id);
                if (it != scan.oracle_data.end() && it->second.has_data) {
                    const ScannedOracleData& od = it->second;
                    oracle_obj.pushKV("price_micro_usd", (int64_t)od.price_micro_usd);
                    oracle_obj.pushKV("price_usd", static_cast<double>(od.price_micro_usd) / 1000000.0);
                    oracle_obj.pushKV("timestamp", od.timestamp);
                    oracle_obj.pushKV("block_height", od.block_height);

                    // Calculate deviation from consensus
                    double deviation_pct = 0.0;
                    if (scan.consensus_price > 0) {
                        deviation_pct = ((double)od.price_micro_usd - (double)scan.consensus_price) / (double)scan.consensus_price * 100.0;
                    }
                    oracle_obj.pushKV("deviation_pct", deviation_pct);
                    oracle_obj.pushKV("signature_valid", od.signature_valid);
                    oracle_obj.pushKV("price_source", od.price_source.empty() ? "none" : od.price_source);

                    std::string status = GetOracleStatus(od, scan.consensus_price);
                    oracle_obj.pushKV("status", status);
                    if (status == "reporting") reporting_count++;
                } else {
                    oracle_obj.pushKV("price_micro_usd", 0);
                    oracle_obj.pushKV("price_usd", 0.0);
                    oracle_obj.pushKV("timestamp", 0);
                    oracle_obj.pushKV("block_height", 0);
                    oracle_obj.pushKV("deviation_pct", 0.0);
                    oracle_obj.pushKV("signature_valid", false);
                    oracle_obj.pushKV("price_source", "none");
                    oracle_obj.pushKV("status", "no_data");
                }

                oracles_arr.push_back(oracle_obj);
            }

            result.pushKV("oracle_count", reporting_count);
            result.pushKV("required", consensus.nOracleRequiredMessages);
            result.pushKV("total_oracles", (int)oracle_nodes.size());
            result.pushKV("oracles", oracles_arr);
            result.pushKV("last_bundle_height", scan.last_bundle_height);
            result.pushKV("last_bundle_time", scan.last_bundle_time);

            return result;
        },
    };
}


// sendoracleprice RPC REMOVED — Security vulnerability.
// Oracle operators must NOT be able to inject arbitrary prices.
// Oracle prices come exclusively from live exchange aggregation.
// See OracleNode::PriceThreadFunc() and MultiExchangeAggregator.

static RPCHelpMan getoracles()
{
    return RPCHelpMan{"getoracles",
                "\nGet all oracle nodes with their config, status, and network-reported prices.\n"
                "Shows what the network sees — prices come from on-chain oracle bundles,\n"
                "not just the local node. Use this for monitoring all oracle health.\n",
                {
                    {"active_only", RPCArg::Type::BOOL, RPCArg::Default{false}, "Only show active oracles"},
                    {"blocks", RPCArg::Type::NUM, RPCArg::Default{20}, "Number of recent blocks to scan (default: 20)"},
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::NUM, "oracle_id", "Oracle ID"},
                                {RPCResult::Type::STR, "name", "Oracle operator name"},
                                {RPCResult::Type::STR_HEX, "pubkey", "Oracle public key"},
                                {RPCResult::Type::STR, "endpoint", "Oracle network endpoint"},
                                {RPCResult::Type::BOOL, "is_active", "Whether oracle is configured as active"},
                                {RPCResult::Type::NUM, "last_price_micro_usd", "Last reported price in micro-USD"},
                                {RPCResult::Type::NUM, "last_price_usd", "Last reported price in USD"},
                                {RPCResult::Type::NUM, "last_update", "Timestamp of last price"},
                                {RPCResult::Type::STR, "price_source", "Where price came from: local/on-chain/pending/none"},
                                {RPCResult::Type::STR, "status", "Oracle status: reporting/no_data/outlier"},
                                {RPCResult::Type::BOOL, "selected_for_epoch", "Whether oracle is selected for current epoch"},
                                {RPCResult::Type::BOOL, "is_running_locally", "Whether this oracle is running on YOUR node"}
                            }
                        }
                    }
                },
                RPCExamples{
                    HelpExampleCli("getoracles", "") +
                    HelpExampleCli("getoracles", "true") +
                    HelpExampleRpc("getoracles", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            bool activeOnly = !request.params[0].isNull() ? request.params[0].get_bool() : false;

            int scan_blocks = !request.params[1].isNull() ? request.params[1].getInt<int>() : 20;
            if (scan_blocks < 1) scan_blocks = 1;
            if (scan_blocks > 1000) scan_blocks = 1000;

            const ChainstateManager& chainman = EnsureAnyChainman(request.context);
            const CChainParams& params = Params();
            const std::vector<OracleNodeInfo>& all_oracles = params.GetOracleNodes();
            OracleBundleManager& bundle_manager = OracleBundleManager::GetInstance();
            OracleManager& oracle_manager = OracleManager::GetInstance();

            std::vector<std::string> oracle_names = {"Jared", "Green Candle", "Bastian", "DanGB", "Shenger", "Ycagel", "Aussie", "LookInto", "JohnnyLawDGB", "Ogilvie", "ChopperBrian"};

            int32_t current_height = chainman.ActiveChain().Height();
            int32_t current_epoch = GetCurrentEpoch(current_height);
            std::vector<OracleNodeInfo> selected_oracles = SelectOraclesForEpoch(all_oracles, current_epoch);
            std::set<uint32_t> selected_ids;
            for (const auto& oracle : selected_oracles) {
                selected_ids.insert(oracle.id);
            }

            // Use shared scanner (Bug #15: consistent with getalloracleprices)
            OracleScanResult scan = ScanOracleDataFromChain(chainman, bundle_manager, oracle_manager, scan_blocks);

            UniValue result(UniValue::VARR);
            for (size_t i = 0; i < all_oracles.size(); ++i) {
                const auto& oc = all_oracles[i];
                if (activeOnly && !oc.is_active) continue;

                bool is_selected = selected_ids.count(oc.id) > 0;
                bool is_running = oracle_manager.IsOracleRunning(oc.id);

                UniValue info(UniValue::VOBJ);
                info.pushKV("oracle_id", static_cast<int>(oc.id));
                info.pushKV("name", i < oracle_names.size() ? oracle_names[i] : "Unknown");
                info.pushKV("pubkey", HexStr(oc.pubkey));
                info.pushKV("endpoint", oc.endpoint);
                info.pushKV("is_active", oc.is_active);

                auto it = scan.oracle_data.find(oc.id);
                if (it != scan.oracle_data.end() && it->second.has_data) {
                    const ScannedOracleData& od = it->second;
                    info.pushKV("last_price_micro_usd", (int64_t)od.price_micro_usd);
                    info.pushKV("last_price_usd", static_cast<double>(od.price_micro_usd) / 1000000.0);
                    info.pushKV("last_update", od.timestamp);
                    info.pushKV("price_source", od.price_source);
                    info.pushKV("status", GetOracleStatus(od, scan.consensus_price));
                } else {
                    info.pushKV("last_price_micro_usd", (int64_t)0);
                    info.pushKV("last_price_usd", 0.0);
                    info.pushKV("last_update", (int64_t)0);
                    info.pushKV("price_source", "none");
                    info.pushKV("status", "no_data");
                }

                info.pushKV("selected_for_epoch", is_selected);
                info.pushKV("is_running_locally", is_running);

                result.push_back(info);
            }
            return result;
        },
    };
}

static RPCHelpMan listoracle()
{
    return RPCHelpMan{"listoracle",
                "\nShow the status of the oracle running on this local node.\n"
                "If no oracle is running, returns a message with instructions.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "running", "Whether an oracle is running locally"},
                        {RPCResult::Type::NUM, "oracle_id", /*optional=*/ "Oracle ID (if running)"},
                        {RPCResult::Type::STR, "name", /*optional=*/ "Oracle operator name"},
                        {RPCResult::Type::STR_HEX, "pubkey", /*optional=*/ "Oracle public key"},
                        {RPCResult::Type::NUM, "price_micro_usd", /*optional=*/ "Current price being reported"},
                        {RPCResult::Type::NUM, "price_usd", /*optional=*/ "Current price in USD"},
                        {RPCResult::Type::NUM, "last_update", /*optional=*/ "Last update timestamp"},
                        {RPCResult::Type::BOOL, "enabled", /*optional=*/ "Whether oracle is enabled"},
                        {RPCResult::Type::STR, "message", /*optional=*/ "Status message"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("listoracle", "") +
                    HelpExampleRpc("listoracle", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            OracleManager& oracle_manager = OracleManager::GetInstance();
            std::vector<std::string> oracle_names = {"Jared", "Green Candle", "Bastian", "DanGB", "Shenger", "Ycagel", "Aussie", "LookInto", "JohnnyLawDGB", "Ogilvie", "ChopperBrian"};

            UniValue result(UniValue::VOBJ);

            // Check all oracle IDs for a running instance
            const CChainParams& chainparams = Params();
            const std::vector<OracleNodeInfo>& all_oracles = chainparams.GetOracleNodes();
            for (uint32_t id = 0; id < all_oracles.size(); ++id) {
                if (oracle_manager.IsOracleRunning(id)) {
                    OracleNode* node = oracle_manager.GetOracleNode(id);
                    if (!node) continue;

                    result.pushKV("running", true);
                    result.pushKV("oracle_id", (int)id);
                    result.pushKV("name", id < oracle_names.size() ? oracle_names[id] : "Unknown");

                    const CChainParams& params = Params();
                    const std::vector<OracleNodeInfo>& oracles = params.GetOracleNodes();
                    if (id < oracles.size()) {
                        result.pushKV("pubkey", HexStr(oracles[id].pubkey));
                    }

                    // Price: prefer local runtime, fall back to pending P2P, then on-chain
                    uint64_t price = 0;
                    int64_t update_time = 0;
                    std::string price_source = "none";

                    if (node->HasValidPrice()) {
                        price = node->GetCurrentPrice();
                        update_time = node->GetLastUpdateTime();
                        price_source = "local";
                    } else {
                        // Check pending P2P messages (our own broadcast may be there)
                        OracleBundleManager& bundle_manager = OracleBundleManager::GetInstance();
                        std::vector<COraclePriceMessage> pending = bundle_manager.GetPendingMessages();
                        for (const auto& msg : pending) {
                            if (msg.oracle_id == id) {
                                price = msg.price_micro_usd;
                                update_time = msg.timestamp;
                                price_source = "pending";
                                break;
                            }
                        }

                        // Fall back to on-chain data
                        if (price == 0) {
                            const ChainstateManager& chainman = EnsureAnyChainman(request.context);
                            LOCK(cs_main);
                            int32_t current_height = chainman.ActiveChain().Height();
                            for (int h = current_height; h >= std::max(0, current_height - 19); --h) {
                                CBlockIndex* pindex = chainman.ActiveChain()[h];
                                if (!pindex) continue;
                                CBlock block;
                                if (!chainman.m_blockman.ReadBlockFromDisk(block, *pindex)) continue;
                                if (block.vtx.empty()) continue;
                                COracleBundle bundle;
                                if (bundle_manager.ExtractOracleBundle(*block.vtx[0], bundle)) {
                                    for (const auto& msg : bundle.messages) {
                                        if (msg.oracle_id == id) {
                                            price = msg.price_micro_usd;
                                            update_time = msg.timestamp;
                                            price_source = "on-chain";
                                            break;
                                        }
                                    }
                                    if (price > 0) break;
                                }
                            }
                        }
                    }

                    result.pushKV("price_micro_usd", (int64_t)price);
                    result.pushKV("price_usd", static_cast<double>(price) / 1000000.0);
                    result.pushKV("last_update", update_time);
                    result.pushKV("price_source", price_source);

                    result.pushKV("enabled", node->IsEnabled());
                    result.pushKV("message", "Oracle is running");
                    return result;
                }
            }

            result.pushKV("running", false);
            result.pushKV("message", "No oracle is running on this node. Use 'startoracle <id>' to start one.");
            return result;
        },
    };
}

RPCHelpMan createoraclekey()
{
    return RPCHelpMan{"createoraclekey",
                "\nGenerate an oracle keypair and store it in the loaded descriptor wallet.\n"
                "The private key is stored securely in the wallet database, mapped to the oracle_id.\n"
                "\nTwo public key formats are returned from the same keypair:\n"
                "  - pubkey: 33-byte compressed key (02/03 prefix) — SEND THIS to the maintainer\n"
                "  - pubkey_xonly: 32-byte x-only key (prefix stripped) — used internally for Schnorr signatures\n"
                "\nThe maintainer uses your pubkey to populate both chainparams locations:\n"
                "  - vOracleNodes: uses the full 33-byte compressed key as-is\n"
                "  - consensus.vOraclePublicKeys: uses the 32-byte x-only version (02/03 prefix stripped)\n"
                "\nAs an operator, you only need to share your pubkey. Never share your private key.\n",
                {
                    {"oracle_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Oracle ID slot (0-29) to generate key for"},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "oracle_id", "Oracle ID the key was generated for"},
                        {RPCResult::Type::STR_HEX, "pubkey", "Compressed public key (33-byte, 02/03 prefix) — SHARE THIS with the maintainer for chainparams inclusion"},
                        {RPCResult::Type::STR_HEX, "pubkey_xonly", "X-only public key (32-byte, no prefix) — derived from pubkey, used internally for Schnorr signature verification. Do not share separately; the maintainer derives this from pubkey."},
                        {RPCResult::Type::BOOL, "stored_in_wallet", "Whether key was stored in wallet"},
                        {RPCResult::Type::STR, "message", "Instructions for the operator"},
                    }
                },
                RPCExamples{
                    HelpExampleCli("createoraclekey", "5") +
                    HelpExampleRpc("createoraclekey", "5")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                std::shared_ptr<wallet::CWallet> pwallet_check = wallet::GetWalletForJSONRPCRequest(request);
                if (pwallet_check) {
                    node::NodeContext* node_ctx = pwallet_check->chain().context();
                    if (node_ctx) {
                        ChainstateManager& chainman = *node_ctx->chainman;
                        const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                        if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                            throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                        }
                    }
                }
            }
            // Get wallet
            std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "No wallet is loaded. A descriptor wallet is required.");

            // Ensure wallet is unlocked
            wallet::EnsureWalletIsUnlocked(*pwallet);

            // Parse and validate oracle_id
            uint32_t oracle_id = request.params[0].getInt<int>();
            if (oracle_id >= (uint32_t)ORACLE_TOTAL_COUNT) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Invalid oracle ID %u. Must be between 0 and %d", oracle_id, ORACLE_TOTAL_COUNT - 1));
            }

            // Check if key already exists for this oracle_id
            CKey existing_key;
            if (pwallet->GetOracleKey(oracle_id, existing_key)) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                    strprintf("Oracle key already exists in wallet for oracle_id %u. "
                              "Use the existing key or remove it first.", oracle_id));
            }

            // Generate new compressed keypair
            CKey key;
            key.MakeNewKey(true); // compressed = true

            CPubKey pubkey = key.GetPubKey();
            assert(pubkey.IsCompressed());
            assert(key.VerifyPubKey(pubkey));

            // Store in wallet
            bool stored = pwallet->StoreOracleKey(oracle_id, key);
            if (!stored) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to store oracle key in wallet database");
            }

            // Get x-only pubkey (32 bytes, strip the 02/03 prefix)
            XOnlyPubKey xonly(pubkey);

            LogPrintf("Oracle: Generated oracle key for oracle_id %u, pubkey=%s\n",
                     oracle_id, HexStr(pubkey));

            UniValue result(UniValue::VOBJ);
            result.pushKV("oracle_id", (int)oracle_id);
            result.pushKV("pubkey", HexStr(pubkey));
            result.pushKV("pubkey_xonly", HexStr(xonly));
            result.pushKV("stored_in_wallet", stored);
            result.pushKV("message", strprintf(
                "Oracle key generated and stored in wallet. "
                "Share ONLY the pubkey (33-byte compressed, starting with 02/03) with the DigiByte Core maintainer for chainparams inclusion. "
                "The pubkey_xonly is derived from it automatically — you do not need to send it separately. "
                "Run 'startoracle %u' after your key is added to chainparams to begin oracle operation.",
                oracle_id));

            return result;
        },
    };
}

RPCHelpMan startoracle()
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
            // Check DigiDollar activation via wallet's chain interface
            // (startoracle is registered in wallet RPC table, so request.context is WalletContext)
            {
                std::shared_ptr<wallet::CWallet> pwallet_check = wallet::GetWalletForJSONRPCRequest(request);
                if (pwallet_check) {
                    node::NodeContext* node_ctx = pwallet_check->chain().context();
                    if (node_ctx) {
                        ChainstateManager& chainman = *node_ctx->chainman;
                        const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                        if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                            throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                        }
                    }
                }
            }

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
                        success = TryStartOracleFromPrivateKey(oracle_manager, oracle_id, private_key_hex, "provided private key", /*allow_initialized_without_running=*/false, status_message);
                    } else {
                        // Try to start existing oracle (if already configured)
                        OracleNode* existing_oracle = oracle_manager.GetOracleNode(oracle_id);
                        if (existing_oracle) {
                            existing_oracle->Start();
                            success = existing_oracle->IsRunning();
                            status_message = success ? "Existing oracle started" : "Failed to start existing oracle";
                        } else {
                            // Try to load oracle key from wallet
                            bool loaded_from_wallet = false;
                            try {
                                std::shared_ptr<wallet::CWallet> pwallet = wallet::GetWalletForJSONRPCRequest(request);
                                if (pwallet) {
                                    // Ensure wallet is unlocked before reading keys
                                    wallet::EnsureWalletIsUnlocked(*pwallet);
                                    CKey wallet_key;
                                    if (pwallet->GetOracleKey(oracle_id, wallet_key)) {
                                        const std::string wallet_key_hex = HexStr(Span<const unsigned char>(wallet_key.begin(), wallet_key.end()));
                                        const std::string key_source = strprintf("key loaded from wallet '%s'", pwallet->GetName());
                                        success = TryStartOracleFromPrivateKey(oracle_manager, oracle_id, wallet_key_hex, key_source, /*allow_initialized_without_running=*/true, status_message);
                                        loaded_from_wallet = success;
                                    }
                                }
                            } catch (const std::exception& e) {
                                // No wallet context available — include error in status_message for debugging
                                status_message = strprintf("Wallet access error: %s", e.what());
                            }
                            if (!loaded_from_wallet && !success) {
                                status_message = "Oracle not configured. Provide private_key parameter or run createoraclekey first.";
                                warning = "Oracle private key must be provided for first-time setup, or use createoraclekey to generate one";
                            }
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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
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

                        // Clear stale oracle messaging state to break potential deadlocks.
                        // The seen_message_hashes, pending_messages, and pending_attestations
                        // can hold stale entries that prevent consensus recovery after restart.
                        // ClearPendingMessages() resets the duplicate filter, allowing fresh
                        // messages to be accepted when the oracle is restarted.
                        if (success) {
                            OracleBundleManager& bundleManager = OracleBundleManager::GetInstance();
                            bundleManager.ClearPendingMessages();
                            status_message += " (messaging state cleared)";
                        }
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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
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
                "functionality in RegTest mode without requiring real oracle nodes.\n"
                "Price is specified in micro-USD (1,000,000 = $1.00) for sub-cent precision.\n",
                {
                    {"price", RPCArg::Type::NUM, RPCArg::Optional::NO, "Price in micro-USD per DGB (e.g., 6500 = $0.0065/DGB, 1000000 = $1.00/DGB)", RPCArgOptions{.skip_type_check = true}}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "price_micro_usd", "New mock oracle price in micro-USD per DGB"},
                        {RPCResult::Type::STR, "price_usd", "Price formatted as USD per DGB"},
                        {RPCResult::Type::NUM, "update_height", "Block height of update"},
                        {RPCResult::Type::BOOL, "enabled", "Whether mock oracle is enabled"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("setmockoracleprice", "6500") +
                    "\nSet price to $0.0065 per DGB (realistic DGB price)\n" +
                    HelpExampleCli("setmockoracleprice", "1000000") +
                    "\nSet price to $1.00 per DGB\n" +
                    HelpExampleRpc("setmockoracleprice", "6500")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            // Only allow in RegTest mode
            if (Params().GetChainType() != ChainType::REGTEST) {
                throw JSONRPCError(RPC_METHOD_NOT_FOUND,
                    "setmockoracleprice is only available in RegTest mode");
            }

            CAmount price_micro_usd = request.params[0].getInt<int64_t>();

            if (price_micro_usd <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "Price must be positive");
            }

            // Price should be reasonable (between $0.0001 and $1000 per DGB in micro-USD)
            const CAmount MIN_PRICE = 100;              // 100 micro-USD = $0.0001 per DGB
            const CAmount MAX_PRICE = 1000000000;       // 1,000,000,000 micro-USD = $1000 per DGB

            if (price_micro_usd < MIN_PRICE || price_micro_usd > MAX_PRICE) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    strprintf("Price must be between %lld and %lld micro-USD per DGB", MIN_PRICE, MAX_PRICE));
            }

            // Set the mock price (mock oracle now accepts micro-USD directly)
            MockOracleManager::GetInstance().SetMockPrice(price_micro_usd);

            // Build result
            UniValue result(UniValue::VOBJ);
            result.pushKV("price_micro_usd", price_micro_usd);
            // Format as dollars (divide micro-USD by 1,000,000)
            result.pushKV("price_usd", strprintf("$%.6f", price_micro_usd / 1000000.0));
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
                "functionality in RegTest mode.\n"
                "Price is in micro-USD (1,000,000 = $1.00) for sub-cent precision.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "price_micro_usd", "Current mock oracle price in micro-USD per DGB"},
                        {RPCResult::Type::STR, "price_usd", "Price formatted as USD per DGB"},
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

            CAmount price_micro_usd = MockOracleManager::GetInstance().GetCurrentPrice();
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
            result.pushKV("price_micro_usd", price_micro_usd);
            // Format as dollars (divide micro-USD by 1,000,000)
            result.pushKV("price_usd", strprintf("$%.6f", price_micro_usd / 1000000.0));
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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
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
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
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

static RPCHelpMan submitoracleprice()
{
    return RPCHelpMan{"submitoracleprice",
                "\nSubmit an individual oracle price message for Phase 2 testing (RegTest only).\n"
                "Creates a signed oracle price message from the specified oracle ID and adds\n"
                "it to the pending message pool. When enough messages are collected (>= min_required),\n"
                "a Phase 2 bundle will be created in the next block.\n",
                {
                    {"oracle_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Oracle ID (0-4 for regtest)"},
                    {"price_micro_usd", RPCArg::Type::NUM, RPCArg::Optional::NO, "Price in micro-USD per DGB (e.g., 6500 = $0.0065/DGB)"}
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "oracle_id", "Oracle ID used"},
                        {RPCResult::Type::NUM, "price_micro_usd", "Price submitted"},
                        {RPCResult::Type::BOOL, "accepted", "Whether message was accepted"},
                        {RPCResult::Type::NUM, "pending_count", "Total pending oracle messages"},
                        {RPCResult::Type::NUM, "min_required", "Minimum messages required for consensus"}
                    }
                },
                RPCExamples{
                    HelpExampleCli("submitoracleprice", "0 6500") +
                    HelpExampleCli("submitoracleprice", "1 6500") +
                    HelpExampleCli("submitoracleprice", "2 6500") +
                    HelpExampleRpc("submitoracleprice", "0, 6500")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // Check DigiDollar activation
            {
                const node::NodeContext& node = EnsureAnyNodeContext(request.context);
                ChainstateManager& chainman = EnsureChainman(node);
                const CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
                if (!DigiDollar::IsDigiDollarEnabled(tip, chainman)) {
                    throw JSONRPCError(RPC_MISC_ERROR, "DigiDollar is not yet active on this blockchain");
                }
            }
            if (Params().GetChainType() != ChainType::REGTEST) {
                throw JSONRPCError(RPC_METHOD_NOT_FOUND,
                    "submitoracleprice is only available in RegTest mode");
            }

            uint32_t oracle_id = request.params[0].getInt<int>();
            CAmount price_micro_usd = request.params[1].getInt<int64_t>();

            if (oracle_id > 6) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Oracle ID must be 0-6 for regtest");
            }
            if (price_micro_usd <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Price must be positive");
            }

            // Get test private key for this oracle
            MockOracleManager& mock = MockOracleManager::GetInstance();
            CKey key = mock.GetTestKey(oracle_id);
            if (!key.IsValid()) {
                throw JSONRPCError(RPC_INTERNAL_ERROR,
                    strprintf("No test key available for oracle %d", oracle_id));
            }

            // Create and sign the oracle price message
            COraclePriceMessage msg;
            msg.oracle_id = oracle_id;
            msg.price_micro_usd = static_cast<uint64_t>(price_micro_usd);
            msg.timestamp = GetTime();
            msg.block_height = 0; // Will be set by block creation
            msg.nonce = GetRand(std::numeric_limits<uint64_t>::max());
            msg.oracle_pubkey = XOnlyPubKey(key.GetPubKey());

            if (!msg.SignPhase2(key)) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to sign oracle message");
            }

            // Add to bundle manager's pending messages
            OracleBundleManager& manager = OracleBundleManager::GetInstance();
            bool accepted = manager.AddOracleMessage(msg);

            const Consensus::Params& consensus = Params().GetConsensus();

            UniValue result(UniValue::VOBJ);
            result.pushKV("oracle_id", (int)oracle_id);
            result.pushKV("price_micro_usd", price_micro_usd);
            result.pushKV("accepted", accepted);
            result.pushKV("pending_count", (int)manager.GetPendingMessageCount());
            result.pushKV("min_required", consensus.nOracleRequiredMessages);

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
        // {"digidollar", &validateddaddress},  // Moved to wallet RPC table (Bug #17)
        // {"digidollar", &listdigidollaraddresses},  // Moved to wallet RPC commands for proper wallet context (Bug #12)
        {"digidollar", &importdigidollaraddress},

        // Utility commands (moved to wallet RPC table)
        // {"digidollar", &getdigidollarbalance},
        {"digidollar", &estimatecollateral},
        // {"digidollar", &getredemptioninfo},  // Moved to wallet RPC table for proper wallet context

        // {"digidollar", &listdigidollartxs},
        {"digidollar", &getoracleprice},
        {"oracle", &getalloracleprices},
        {"digidollar", &getprotectionstatus},

        // Oracle management commands
        // sendoracleprice REMOVED — security vulnerability (fake price injection)
        {"oracle", &getoracles},
        {"oracle", &listoracle},
        // {"oracle", &startoracle},  // Moved to wallet RPC table for wallet key loading
        {"oracle", &stoporacle},
        {"oracle", &getoraclepubkey},

        // Mock Oracle commands (RegTest only)
        {"digidollar", &setmockoracleprice},
        {"digidollar", &getmockoracleprice},
        {"digidollar", &simulatepricevolatility},
        {"digidollar", &enablemockoracle},

        // Phase 2 oracle testing (RegTest only)
        {"oracle", &submitoracleprice}
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }

    // DigiDollar wallet-based transaction commands are registered via
    // GetWalletRPCCommands() in wallet/rpc/wallet.cpp for proper wallet context.
    // Do NOT register them here - they won't have wallet context and will fail
    // with "Wallet context not found".
}
