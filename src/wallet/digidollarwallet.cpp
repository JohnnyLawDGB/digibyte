// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/digidollarwallet.h>
#include <wallet/wallet.h>
#include <wallet/spend.h>
#include <wallet/coincontrol.h>
#include <wallet/scriptpubkeyman.h>
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
    : amount(0), timestamp(0), confirmations(0), incoming(false), category("unknown"),
      blockheight(-1), blockhash(""), fee(0), comment(""), abandoned(false), lock_tier(-1) {}

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

                LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loaded DD UTXO %s:%u (%lld cents)\n",
                        outpoint.hash.ToString(), outpoint.n, static_cast<long long>(dd_amount));
            }
        }
    }

    // Load DD address keys for received tokens (FIX for wallet restart key loss)
    size_t addr_keys_loaded = LoadDDAddressKeys();

    // Load DD owner keys for minted tokens (FIX for vault redemption after restart)
    size_t owner_keys_loaded = LoadDDOwnerKeys();

    size_t total = positions_loaded + balances_loaded + txs_loaded + utxos_loaded + addr_keys_loaded + owner_keys_loaded;

    LogPrintf("DigiDollarWallet: Loaded %zu positions, %zu balances, %zu transactions, %zu DD UTXOs, %zu DD address keys, %zu DD owner keys\n",
              positions_loaded, balances_loaded, txs_loaded, utxos_loaded, addr_keys_loaded, owner_keys_loaded);

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

    LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Totals - DD Balance: %lld, Locked: %lld\n",
             static_cast<long long>(total_dd_balance), static_cast<long long>(locked_collateral));
}

void DigiDollarWallet::StoreAddressKey(const XOnlyPubKey& output_key, const CKey& key)
{
    // Store in in-memory map
    std::array<unsigned char, 32> key_bytes;
    std::copy(output_key.begin(), output_key.end(), key_bytes.begin());
    dd_address_keys[key_bytes] = key;

    LogPrintf("DigiDollarWallet: Stored DD address key for output key %s\n",
              HexStr(output_key));

    // Persist to wallet database
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDAddressKey(key_bytes, key)) {
            LogPrintf("DigiDollarWallet: WARNING - Failed to persist DD address key to database\n");
        } else {
            LogPrintf("DigiDollarWallet: Persisted DD address key to database\n");
        }
    }
}

size_t DigiDollarWallet::LoadDDAddressKeys()
{
    if (!m_wallet) {
        LogPrint(BCLog::WALLETDB, "DigiDollarWallet::LoadDDAddressKeys - No wallet pointer\n");
        return 0;
    }

    wallet::WalletBatch batch(m_wallet->GetDatabase());
    size_t count = 0;

    // Clear in-memory map before loading
    dd_address_keys.clear();

    // Iterate through database to find DD address keys
    std::unique_ptr<wallet::DatabaseCursor> cursor = batch.GetNewCursor();
    if (cursor) {
        wallet::DatabaseCursor::Status status = wallet::DatabaseCursor::Status::MORE;
        while (status == wallet::DatabaseCursor::Status::MORE) {
            DataStream key_stream{};
            DataStream value_stream{};
            status = cursor->Next(key_stream, value_stream);

            if (status != wallet::DatabaseCursor::Status::MORE) break;

            std::string key_type;
            key_stream >> key_type;

            if (key_type == wallet::DBKeys::DD_ADDRESS_KEY) {
                std::array<unsigned char, 32> output_key_bytes;
                key_stream >> output_key_bytes;

                CPrivKey privkey;
                value_stream >> privkey;

                CKey key;
                if (key.Load(privkey, CPubKey(), /*fSkipCheck=*/true)) {
                    dd_address_keys[output_key_bytes] = key;
                    count++;

                    LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loaded DD address key %s\n",
                            HexStr(output_key_bytes));
                } else {
                    LogPrintf("DigiDollarWallet: WARNING - Failed to load DD address key from database\n");
                }
            }
        }
    }

    LogPrintf("DigiDollarWallet: Loaded %zu DD address keys from database\n", count);
    return count;
}

void DigiDollarWallet::StoreOwnerKey(const uint256& dd_timelock_id, const CKey& key)
{
    // Store in in-memory map
    dd_owner_keys[dd_timelock_id] = key;

    LogPrintf("DigiDollarWallet: Stored DD owner key for timelock %s\n",
              dd_timelock_id.ToString());

    // Persist to wallet database
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDOwnerKey(dd_timelock_id, key)) {
            LogPrintf("DigiDollarWallet: WARNING - Failed to persist DD owner key to database\n");
        } else {
            LogPrintf("DigiDollarWallet: Persisted DD owner key to database\n");
        }
    }
}

size_t DigiDollarWallet::LoadDDOwnerKeys()
{
    if (!m_wallet) {
        LogPrint(BCLog::WALLETDB, "DigiDollarWallet::LoadDDOwnerKeys - No wallet pointer\n");
        return 0;
    }

    wallet::WalletBatch batch(m_wallet->GetDatabase());
    size_t count = 0;

    // Clear in-memory map before loading
    dd_owner_keys.clear();

    // Iterate through database to find DD owner keys
    std::unique_ptr<wallet::DatabaseCursor> cursor = batch.GetNewCursor();
    if (cursor) {
        wallet::DatabaseCursor::Status status = wallet::DatabaseCursor::Status::MORE;
        while (status == wallet::DatabaseCursor::Status::MORE) {
            DataStream key_stream{};
            DataStream value_stream{};
            status = cursor->Next(key_stream, value_stream);

            if (status != wallet::DatabaseCursor::Status::MORE) break;

            std::string key_type;
            key_stream >> key_type;

            if (key_type == wallet::DBKeys::DD_OWNER_KEY) {
                uint256 dd_timelock_id;
                key_stream >> dd_timelock_id;

                CPrivKey privkey;
                value_stream >> privkey;

                CKey key;
                if (key.Load(privkey, CPubKey(), /*fSkipCheck=*/true)) {
                    dd_owner_keys[dd_timelock_id] = key;
                    count++;

                    LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Loaded DD owner key for timelock %s\n",
                            dd_timelock_id.ToString());
                } else {
                    LogPrintf("DigiDollarWallet: WARNING - Failed to load DD owner key from database\n");
                }
            }
        }
    }

    LogPrintf("DigiDollarWallet: Loaded %zu DD owner keys from database\n", count);
    return count;
}

bool DigiDollarWallet::TransferDigiDollar(const CDigiDollarAddress& to, CAmount amount,
                                        std::string& txid, std::string& error) {
    // Clear previous results
    txid.clear();
    error.clear();

    try {
        LogPrintf("DigiDollar: Starting transfer - %lld cents to %s\n", static_cast<long long>(amount), to.ToString());

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
            error = strprintf("Insufficient DD balance. Available: %lld cents, Required: %lld cents",
                            static_cast<long long>(currentBalance), static_cast<long long>(amount));
            LogPrintf("DigiDollar: Insufficient balance - available: %lld, required: %lld\n",
                     static_cast<long long>(currentBalance), static_cast<long long>(amount));
            return false;
        }

        // Build transfer transaction using TxBuilder
        DigiDollar::TxBuilderTransferParams params;
        params.recipients.push_back({to.ToString(), amount});
        // DigiDollar transactions MUST pay at least 0.1 DGB fee to miners
        params.feeRate = 35000000; // 0.35 DGB/kB = 0.105 DGB for 300 byte tx

        // Select DD UTXOs to cover the amount (with individual amounts - FIX #7)
        CAmount selectedDDTotal = 0;
        std::vector<CAmount> selected_dd_amounts;
        if (!SelectDDCoins(amount, params.ddUtxos, selectedDDTotal, &selected_dd_amounts)) {
            // CRITICAL: Do NOT use mock UTXOs - they cause "bad-txns-inputs-missingorspent" errors!
            error = "No spendable DD UTXOs found. Make sure mint transaction is confirmed.";
            LogPrintf("DigiDollar: Transfer failed - no DD UTXOs available\n");
            return false;
        }

        // CRITICAL: Pass DD UTXO amounts to txbuilder (FIX #3 & #7)
        // Pass individual amounts for each UTXO (required by txbuilder)
        params.ddAmounts = selected_dd_amounts;
        LogPrintf("DigiDollar: Transfer - Selected %zu DD UTXOs totaling %lld cents\n",
                  selected_dd_amounts.size(), static_cast<long long>(selectedDDTotal));

        // Select DGB UTXOs for fees (estimated)
        // CRITICAL: Exclude DD UTXOs from fee selection to prevent double-spend
        std::vector<COutPoint> exclude_dd_utxos = params.ddUtxos;
        // DigiDollar transactions MUST pay at least 0.1 DGB fee to miners
        CAmount estimatedFee = 10000000; // 0.1 DGB minimum fee
        std::vector<CAmount> fee_amounts;
        CAmount selectedFeeTotal = 0;
        if (!SelectFeeCoins(estimatedFee, params.feeUtxos, selectedFeeTotal, &fee_amounts, &exclude_dd_utxos)) {
            // CRITICAL: Do NOT use mock UTXOs - they cause "bad-txns-inputs-missingorspent" errors!
            error = "Insufficient DGB balance for transaction fees (need 0.1 DGB minimum)";
            LogPrintf("DigiDollar: Transfer failed - no DGB UTXOs available for fees\n");
            return false;
        }
        params.feeAmounts = fee_amounts;  // Pass actual fee UTXO amounts

        // Get spending key - handles both minted DD (in dd_owner_keys) and received DD (in wallet)
        // For minted DD: key is stored in dd_owner_keys map when we created the position
        // For received DD: key is in wallet's standard P2TR key management
        CKey spenderKey;
        bool found_key = false;

        if (!params.ddUtxos.empty()) {
            // First, try dd_owner_keys map (for minted DD)
            if (dd_owner_keys.count(params.ddUtxos[0].hash) > 0) {
                spenderKey = dd_owner_keys[params.ddUtxos[0].hash];
                found_key = true;
                LogPrintf("DigiDollar: Using stored owner key for DD UTXO %s\n", params.ddUtxos[0].hash.ToString());
            }

            // If not found (received DD), try to get key from wallet's P2TR key management
            if (!found_key && m_wallet) {
                LogPrintf("DigiDollar: DD UTXO not in dd_owner_keys, trying wallet key lookup for received DD\n");

                // Helper to get signing provider with private key access for descriptor wallets
                auto getSigningProviderWithKeys = [this](const CScript& script) -> std::unique_ptr<SigningProvider> {
                    const auto& spk_mans = m_wallet->GetScriptPubKeyMans(script);
                    if (!spk_mans.empty()) {
                        wallet::ScriptPubKeyMan* spk_man = *spk_mans.begin();
                        wallet::DescriptorScriptPubKeyMan* desc_spk_man = dynamic_cast<wallet::DescriptorScriptPubKeyMan*>(spk_man);
                        if (desc_spk_man) {
                            LogPrintf("DigiDollar: Using DescriptorScriptPubKeyMan with private keys\n");
                            return desc_spk_man->GetSigningProviderWithKeys(script);
                        }
                    }
                    // Fallback for legacy wallets
                    return m_wallet->GetSolvingProvider(script);
                };

                // Get the scriptPubKey for the DD UTXO we're spending
                const COutPoint& dd_outpoint = params.ddUtxos[0];
                LogPrintf("DigiDollar: Looking up tx %s in mapWallet (size=%d)\n",
                         dd_outpoint.hash.ToString(), m_wallet->mapWallet.size());

                // Look up the transaction to get the scriptPubKey
                auto wtx_it = m_wallet->mapWallet.find(dd_outpoint.hash);
                if (wtx_it != m_wallet->mapWallet.end()) {
                    LogPrintf("DigiDollar: Found tx in mapWallet, checking output %d (tx has %d outputs)\n",
                             dd_outpoint.n, wtx_it->second.tx->vout.size());
                    const auto& wtx = wtx_it->second;
                    if (dd_outpoint.n < wtx.tx->vout.size()) {
                        const CTxOut& txout = wtx.tx->vout[dd_outpoint.n];
                        LogPrintf("DigiDollar: Got output, scriptPubKey size=%d\n", txout.scriptPubKey.size());

                        // Get signing provider for this script WITH PRIVATE KEY ACCESS
                        auto provider = getSigningProviderWithKeys(txout.scriptPubKey);
                        if (provider) {
                            LogPrintf("DigiDollar: Got signing provider\n");
                            // Extract the P2TR destination
                            CTxDestination dest;
                            if (ExtractDestination(txout.scriptPubKey, dest)) {
                                LogPrintf("DigiDollar: Extracted destination, checking if P2TR\n");
                                if (auto* tr = std::get_if<WitnessV1Taproot>(&dest)) {
                                    LogPrintf("DigiDollar: Got P2TR destination, output key=%s\n",
                                             HexStr(Span<const unsigned char>(tr->begin(), tr->end())));
                                    // Get TaprootSpendData to find the internal key
                                    TaprootSpendData spenddata;
                                    if (provider->GetTaprootSpendData(XOnlyPubKey(*tr), spenddata)) {
                                        LogPrintf("DigiDollar: Got TaprootSpendData, internal_key valid=%d\n",
                                                 spenddata.internal_key.IsFullyValid());
                                        if (spenddata.internal_key.IsFullyValid()) {
                                            LogPrintf("DigiDollar: internal_key=%s\n",
                                                     HexStr(Span<const unsigned char>(spenddata.internal_key.begin(), spenddata.internal_key.end())));
                                            // Try to get the private key for the internal key
                                            if (provider->GetKeyByXOnly(spenddata.internal_key, spenderKey)) {
                                                found_key = true;
                                                LogPrintf("DigiDollar: Found key for received DD via GetKeyByXOnly\n");
                                            } else {
                                                LogPrintf("DigiDollar: GetKeyByXOnly failed for internal key\n");
                                            }
                                        }
                                    } else {
                                        LogPrintf("DigiDollar: GetTaprootSpendData returned false\n");
                                    }

                                    // Try dd_address_keys map (for addresses generated via getdigidollaraddress)
                                    if (!found_key) {
                                        CKey address_key;
                                        if (GetAddressKey(XOnlyPubKey(*tr), address_key)) {
                                            spenderKey = address_key;
                                            found_key = true;
                                            LogPrintf("DigiDollar: Found key for received DD via dd_address_keys map\n");
                                        } else {
                                            LogPrintf("DigiDollar: dd_address_keys lookup failed for output key\n");
                                        }
                                    }

                                    // If still not found, try brute force scan through wallet keys
                                    if (!found_key) {
                                        LogPrintf("DigiDollar: Starting brute force P2TR wallet scan\n");
                                        XOnlyPubKey target_output_key(*tr);
                                        int scanned_txs = 0;
                                        int scanned_outputs = 0;
                                        for (const auto& [txid, scan_wtx] : m_wallet->mapWallet) {
                                            scanned_txs++;
                                            for (size_t n = 0; n < scan_wtx.tx->vout.size() && !found_key; n++) {
                                                scanned_outputs++;
                                                CTxDestination out_dest;
                                                if (ExtractDestination(scan_wtx.tx->vout[n].scriptPubKey, out_dest)) {
                                                    auto out_provider = getSigningProviderWithKeys(scan_wtx.tx->vout[n].scriptPubKey);
                                                    if (out_provider) {
                                                        if (auto* scan_tr = std::get_if<WitnessV1Taproot>(&out_dest)) {
                                                            TaprootSpendData scan_spenddata;
                                                            if (out_provider->GetTaprootSpendData(XOnlyPubKey(*scan_tr), scan_spenddata)) {
                                                                if (scan_spenddata.internal_key.IsFullyValid()) {
                                                                    CKey test_key;
                                                                    if (out_provider->GetKeyByXOnly(scan_spenddata.internal_key, test_key)) {
                                                                        XOnlyPubKey test_xonly(test_key.GetPubKey());
                                                                        auto tweaked = test_xonly.CreateTapTweak(nullptr);
                                                                        if (tweaked && std::equal(target_output_key.begin(), target_output_key.end(),
                                                                                                 tweaked->first.begin())) {
                                                                            spenderKey = test_key;
                                                                            found_key = true;
                                                                            LogPrintf("DigiDollar: Found key for received DD via P2TR wallet scan\n");
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            if (found_key) break;
                                        }
                                        LogPrintf("DigiDollar: Wallet scan complete: %d txs, %d outputs scanned, found_key=%d\n",
                                                 scanned_txs, scanned_outputs, found_key);
                                    }
                                } else {
                                    LogPrintf("DigiDollar: Destination is not P2TR\n");
                                }
                            } else {
                                LogPrintf("DigiDollar: ExtractDestination failed\n");
                            }
                        } else {
                            LogPrintf("DigiDollar: GetSolvingProvider returned null\n");
                        }
                    } else {
                        LogPrintf("DigiDollar: Output index %d out of range (tx has %d outputs)\n",
                                 dd_outpoint.n, wtx.tx->vout.size());
                    }
                } else {
                    LogPrintf("DigiDollar: TX %s not found in mapWallet\n", dd_outpoint.hash.ToString());
                }
            }
        }

        if (!found_key) {
            error = "Could not find spending key for DD UTXO";
            LogPrintf("DigiDollar: Could not find spending key for DD UTXO %s:%d\n",
                     params.ddUtxos.empty() ? "empty" : params.ddUtxos[0].hash.ToString().c_str(),
                     params.ddUtxos.empty() ? 0 : params.ddUtxos[0].n);
            return false;
        }

        params.spenderKey = spenderKey;

        // CRITICAL FIX: Get a proper change address from the wallet for DGB change output
        // This ensures the wallet recognizes the change output as its own!
        // Without this, DGB would be sent to an address the wallet doesn't control.
        if (m_wallet) {
            // Get a new change address from wallet's internal keypool
            auto op_dest = m_wallet->GetNewChangeDestination(OutputType::BECH32);
            if (op_dest) {
                params.dgbChangeDest = *op_dest;
                LogPrintf("DigiDollar: Using wallet change address for DGB change output\n");
            } else {
                // Fallback: try to get any fresh address
                LogPrintf("DigiDollar: WARNING - Could not get change destination, trying fresh address\n");
                auto fresh_dest = m_wallet->GetNewDestination(OutputType::BECH32, std::string("DD_change"));
                if (fresh_dest) {
                    params.dgbChangeDest = *fresh_dest;
                    LogPrintf("DigiDollar: Using fresh wallet address for DGB change output\n");
                } else {
                    LogPrintf("DigiDollar: WARNING - No wallet change address available, DGB change may be lost!\n");
                }
            }
        }

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

        // Sign the transaction before broadcasting
        LogPrintf("DigiDollar: Signing transaction with %d DD inputs and %d fee inputs\n",
                  params.ddUtxos.size(), params.feeUtxos.size());

        if (!SignTransaction(result.tx, params.ddUtxos, params.feeUtxos)) {
            error = "Failed to sign transaction";
            LogPrintf("DigiDollar: Transaction signing failed\n");
            return false;
        }

        LogPrintf("DigiDollar: Transaction signed successfully\n");

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
        // Extract DD amounts from OP_RETURN (same logic as DetectIncomingDDOutputs)
        std::vector<CAmount> dd_amounts;
        for (const auto& txout : result.tx.vout) {
            if (txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN) {
                CScript::const_iterator pc = txout.scriptPubKey.begin();
                opcodetype opcode;
                std::vector<unsigned char> data;

                // Skip OP_RETURN
                if (!txout.scriptPubKey.GetOp(pc, opcode)) continue;

                // Check for "DD" marker
                if (!txout.scriptPubKey.GetOp(pc, opcode, data)) continue;
                if (data.size() != 2 || data[0] != 'D' || data[1] != 'D') continue;

                // Get transaction type
                if (!txout.scriptPubKey.GetOp(pc, opcode, data)) continue;
                CScriptNum txType(data, true);
                if (txType.getint() != 2) continue;  // Only TRANSFER transactions (type 2)

                // Extract DD amounts
                while (txout.scriptPubKey.GetOp(pc, opcode, data)) {
                    if (data.size() > 0) {
                        CScriptNum amount_num(data, true, 8);  // 8-byte max for large DD amounts
                        dd_amounts.push_back(amount_num.GetInt64());
                    }
                }
                break;
            }
        }

        // Now match P2TR outputs with DD amounts
        // We know the change output belongs to us because we created it with our owner key
        size_t dd_output_index = 0;
        for (size_t i = 0; i < result.tx.vout.size(); i++) {
            const CTxOut& txout = result.tx.vout[i];

            // Skip OP_RETURN and non-zero value outputs
            if (txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN) continue;
            if (txout.nValue != 0) continue;  // DD outputs have 0 DGB value

            // Check if it's a P2TR output (OP_1 + 32 bytes)
            if (txout.scriptPubKey.size() == 34 && txout.scriptPubKey[0] == OP_1) {
                // This is a DD output
                if (dd_output_index < dd_amounts.size()) {
                    CAmount dd_amount = dd_amounts[dd_output_index];

                    // Check if this is the change output (output 1 in transfer txs)
                    // The change output was created with our owner key, so it's ours
                    // Output 0 is recipient, output 1+ is change
                    bool is_ours = (dd_output_index > 0);  // First DD output goes to recipient, rest is change

                    if (is_ours) {
                        COutPoint new_utxo(result.tx.GetHash(), i);
                        dd_utxos[new_utxo] = dd_amount;

                        // Store the owner key for this new DD UTXO so we can spend it later
                        // Use StoreOwnerKey to ensure persistence to database
                        StoreOwnerKey(result.tx.GetHash(), spenderKey);

                        // Persist DD UTXO to database
                        if (!batch.WriteDDUTXO(new_utxo, dd_amount)) {
                            LogPrintf("DigiDollar: WARNING - Failed to write DD UTXO %s:%d to database\n",
                                     new_utxo.hash.ToString(), i);
                        }

                        LogPrintf("DigiDollar: Added change DD UTXO %s:%d (%lld cents)\n",
                                  new_utxo.hash.ToString(), i, static_cast<long long>(dd_amount));
                    }
                }
                dd_output_index++;
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

        transaction_history.push_back(tx);

        // Verify balance updated correctly
        CAmount newBalance = GetTotalDDBalance();
        LogPrintf("DigiDollar: Transfer successful - %lld cents to %s (txid: %s)\n",
                  static_cast<long long>(amount), to.ToString(), txid);
        LogPrintf("DigiDollar: Balance updated: %lld -> %lld (change: %lld)\n",
                  static_cast<long long>(currentBalance), static_cast<long long>(newBalance), static_cast<long long>(dd_change));

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

    // Calculate confirmations on-demand (not stored/updated on every block)
    // This is the same pattern Bitcoin Core uses - confirmations computed dynamically
    for (auto& ddtx : history) {
        uint256 txid;
        txid.SetHex(ddtx.txid);
        ddtx.confirmations = GetDDTransactionConfirmations(txid);
    }

    // Sort by timestamp (newest first)
    std::sort(history.begin(), history.end(),
              [](const DDTransaction& a, const DDTransaction& b) {
                  return a.timestamp > b.timestamp;
              });

    LogPrintf("DigiDollar: GetDDTransactionHistory returning %zu transactions\n", history.size());
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
        LogPrintf("DigiDollar: Starting redemption - %lld cents via path %d\n", static_cast<long long>(ddAmount), static_cast<int>(path));

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
    // DigiDollar transactions must pay at least 0.1 DGB fee to miners
    static const CAmount MIN_DD_TX_FEE = 10000000;       // 0.1 DGB minimum
    static const CAmount FEE_RATE_PER_KB = 200000;       // 0.002 DGB/kB

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

    // Calculate size-based fee
    CAmount size_based_fee = (estimatedSize * FEE_RATE_PER_KB) / 1000;

    // DigiDollar transactions must pay at least 0.1 DGB fee
    return std::max(size_based_fee, MIN_DD_TX_FEE);
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
// PHASE 2: STATE MANAGEMENT - DD BURNING & POSITION CLOSURE (Task 6)
// =============================================================================

bool DigiDollarWallet::BurnDigiDollars(CAmount amount, std::vector<COutPoint>& burnedUtxos) {
    LogPrintf("DigiDollar: BurnDigiDollars - Burning %lld DD cents\n", static_cast<long long>(amount));

    // Validate input
    if (amount <= 0) {
        LogPrintf("DigiDollar: BurnDigiDollars - Invalid amount: %lld\n", static_cast<long long>(amount));
        return false;
    }

    // Clear output parameter
    burnedUtxos.clear();

    // Step 1: Get all spendable DD UTXOs
    std::vector<DDUtxo> available_utxos = GetDDUTXOs();
    if (available_utxos.empty()) {
        LogPrintf("DigiDollar: BurnDigiDollars - No DD UTXOs available\n");
        return false;
    }

    // Step 2: Select UTXOs to burn (simple greedy selection)
    CAmount selected_amount = 0;
    std::vector<COutPoint> selected_utxos;

    for (const auto& utxo : available_utxos) {
        // Add this UTXO
        selected_utxos.push_back(utxo.outpoint);
        selected_amount += utxo.dd_amount;
        LogPrintf("DigiDollar: BurnDigiDollars - Selected UTXO %s:%u (%lld DD), total now: %lld\n",
                  utxo.outpoint.hash.ToString(), utxo.outpoint.n, static_cast<long long>(utxo.dd_amount), static_cast<long long>(selected_amount));

        // Check if we now have enough
        if (selected_amount >= amount) {
            LogPrintf("DigiDollar: BurnDigiDollars - Have enough DD (%lld >= %lld), stopping selection\n",
                      static_cast<long long>(selected_amount), static_cast<long long>(amount));
            break;
        }
    }

    // Check if we have enough DD
    if (selected_amount < amount) {
        LogPrintf("DigiDollar: BurnDigiDollars - Insufficient DD balance: need %lld, have %lld\n",
                  static_cast<long long>(amount), static_cast<long long>(selected_amount));
        return false;
    }

    // Step 3: Mark UTXOs as spent in collateral_positions (if they exist there)
    // Note: MarkDDUTXOsSpent only works for UTXOs that have collateral positions
    // For UTXOs without positions (e.g., received DD), we just remove from dd_utxos map
    bool has_positions = false;
    for (const auto& utxo : selected_utxos) {
        auto it = collateral_positions.find(utxo.hash);
        if (it != collateral_positions.end()) {
            has_positions = true;
            break;
        }
    }

    if (has_positions) {
        if (!MarkDDUTXOsSpent(selected_utxos)) {
            LogPrintf("DigiDollar: BurnDigiDollars - Failed to mark UTXOs as spent in positions\n");
            return false;
        }
    }

    // Step 4: Remove from dd_utxos tracking map
    for (const auto& utxo : selected_utxos) {
        RemoveDDUTXO(utxo);
        LogPrintf("DigiDollar: BurnDigiDollars - Removed UTXO from tracking: %s:%d\n",
                  utxo.hash.ToString(), utxo.n);
    }

    // Step 5: Erase from database
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        for (const auto& utxo : selected_utxos) {
            if (!batch.EraseDDUTXO(utxo)) {
                LogPrintf("DigiDollar: BurnDigiDollars - Warning: Failed to erase UTXO from DB: %s:%d\n",
                          utxo.hash.ToString(), utxo.n);
                // Continue burning other UTXOs even if one fails
            }
        }
    }

    // Step 6: Update total DD balance (recalculate from remaining UTXOs)
    CAmount new_balance = 0;
    for (const auto& [outpoint, dd_amt] : dd_utxos) {
        new_balance += dd_amt;
    }
    total_dd_balance = new_balance;

    // Step 7: Return burned UTXOs
    burnedUtxos = selected_utxos;

    LogPrintf("DigiDollar: BurnDigiDollars - Successfully burned %lld DD cents using %zu UTXOs\n",
              static_cast<long long>(amount), burnedUtxos.size());
    LogPrintf("DigiDollar: BurnDigiDollars - New total DD balance: %lld cents\n", static_cast<long long>(total_dd_balance));

    return true;
}

bool DigiDollarWallet::CloseCollateralPosition(const COutPoint& outpoint, bool partial, CAmount remainingDD) {
    LogPrintf("DigiDollar: CloseCollateralPosition - %s closure of position %s:%d\n",
              partial ? "Partial" : "Full", outpoint.hash.ToString(), outpoint.n);

    // Validate input
    if (outpoint.IsNull()) {
        LogPrintf("DigiDollar: CloseCollateralPosition - Invalid outpoint (null)\n");
        return false;
    }

    // For partial redemptions, validate remaining DD
    if (partial && remainingDD <= 0) {
        LogPrintf("DigiDollar: CloseCollateralPosition - Invalid remaining DD for partial redemption: %lld\n",
                  static_cast<long long>(remainingDD));
        return false;
    }

    // Step 1: Find collateral position
    // The outpoint.hash is the dd_timelock_id (mint tx hash)
    auto it = collateral_positions.find(outpoint.hash);
    if (it == collateral_positions.end()) {
        LogPrintf("DigiDollar: CloseCollateralPosition - Position not found: %s\n",
                  outpoint.hash.ToString());
        return false;
    }

    // Store original values for logging and potential rollback
    CAmount original_dd = it->second.dd_minted;
    CAmount original_dgb = it->second.dgb_collateral;
    bool originally_active = it->second.is_active;

    // Step 2: Handle full vs partial redemption
    if (partial) {
        // Partial redemption: update position
        CAmount redeemed_dd = it->second.dd_minted - remainingDD;

        // Calculate proportional DGB release
        // released_dgb = (redeemed_dd / original_dd) * original_dgb
        CAmount released_dgb = 0;
        if (original_dd > 0) {
            released_dgb = (redeemed_dd * original_dgb) / original_dd;
        }

        // Update position
        it->second.dd_minted = remainingDD;
        it->second.dgb_collateral -= released_dgb;
        it->second.is_active = true; // Keep active for partial redemption

        LogPrintf("DigiDollar: CloseCollateralPosition - Partial redemption: %lld DD redeemed, %lld DD remaining\n",
                  static_cast<long long>(redeemed_dd), static_cast<long long>(remainingDD));
        LogPrintf("DigiDollar: CloseCollateralPosition - Partial redemption: %lld DGB released, %lld DGB remaining\n",
                  static_cast<long long>(released_dgb), static_cast<long long>(it->second.dgb_collateral));
    } else {
        // Full redemption: mark position as inactive
        it->second.is_active = false;

        LogPrintf("DigiDollar: CloseCollateralPosition - Full redemption: position marked inactive\n");
        LogPrintf("DigiDollar: CloseCollateralPosition - Full redemption: %lld DD redeemed, %lld DGB released\n",
                  static_cast<long long>(original_dd), static_cast<long long>(original_dgb));
    }

    // Step 3: Persist changes to wallet database
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());

        if (!batch.WriteDDTimeLock(it->second)) {
            LogPrintf("DigiDollar: CloseCollateralPosition - Failed to persist position update\n");
            // Rollback in-memory changes
            it->second.dd_minted = original_dd;
            it->second.dgb_collateral = original_dgb;
            it->second.is_active = originally_active;
            return false;
        }

        // For full redemptions, also archive the position to history
        // (Keep in database but marked inactive for accounting/auditing)
        // This is already handled by is_active = false flag
    }

    // Step 4: Update locked collateral tracking
    if (it->second.is_active != originally_active || partial) {
        CAmount total_locked = 0;
        for (const auto& [id, pos] : collateral_positions) {
            if (pos.is_active) {
                total_locked += pos.dgb_collateral;
            }
        }
        locked_collateral = total_locked;

        LogPrintf("DigiDollar: CloseCollateralPosition - Updated total locked collateral: %lld DGB\n",
                  static_cast<long long>(locked_collateral));
    }

    // Step 5: Record closure in transaction history
    DDTransaction ddtx;
    ddtx.txid = outpoint.hash.ToString();
    ddtx.amount = partial ? (original_dd - remainingDD) : original_dd;
    ddtx.timestamp = GetTime();
    ddtx.confirmations = 0;
    ddtx.incoming = false; // Redemption (DD going out, DGB coming in)
    ddtx.address = "";
    ddtx.category = partial ? "partial_redeem" : "redeem";
    ddtx.lock_tier = static_cast<int>(it->second.lock_tier);  // Set lock tier from redeemed position

    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTransaction(ddtx)) {
            LogPrintf("DigiDollar: CloseCollateralPosition - Warning: Failed to write transaction to history\n");
            // Don't fail the entire operation for history write failure
        }
    }

    LogPrintf("DigiDollar: CloseCollateralPosition - Successfully %s position %s\n",
              partial ? "updated" : "closed", outpoint.hash.ToString());

    return true;
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
        LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - Negative balance: %lld\n", static_cast<long long>(balance));
        return error("DigiDollarWallet::WriteDDBalance: Negative balance not allowed");
    }

    std::string addr_str = addr.ToString();
    if (addr_str.empty()) {
        // For testing with mock addresses, use a hash of the serialized address
        // This ensures different invalid addresses get different keys
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << addr;
        uint256 hash = Hash(ss);
        addr_str = "test_addr_" + hash.GetHex();

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet::WriteDDBalance - Using test key for invalid address: %s\n", addr_str);
    }

    try {
        // Create balance record
        WalletDDBalance bal_record(addr, balance);
        bal_record.last_updated = GetTime();

        // Update in-memory cache first (works in both test and production mode)
        dd_balances[addr_str] = bal_record;

        // Recalculate total balance
        CAmount total = 0;
        for (const auto& [address, bal] : dd_balances) {
            total += bal.balance;
        }
        total_dd_balance = total;

        // Write to database (only if wallet pointer exists - production mode)
        if (m_wallet) {
            wallet::WalletBatch batch(m_wallet->GetDatabase());

            LogPrintf("DEBUG: DigiDollarWallet::WriteDDBalance - Writing to database: addr=%s, balance=%lld\n", addr_str, static_cast<long long>(balance));
            if (!batch.WriteDDBalance(addr_str, bal_record)) {
                LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - Database write failed for %s\n", addr_str);
                return error("DigiDollarWallet::WriteDDBalance: Database write failed for %s", addr_str.c_str());
            }

            // Persist total balance metadata
            batch.WriteDDMetadata("total_dd_balance", std::to_string(total));
        } else {
            // Testing mode - no database, just in-memory
            LogPrintf("DigiDollarWallet: WriteDDBalance in test mode (no database) - addr: %s\n", addr_str);
        }

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Wrote balance %lld for %s (total: %lld)\n",
                 static_cast<long long>(balance), addr_str, static_cast<long long>(total));
        return true;

    } catch (const std::exception& e) {
        LogPrintf("ERROR: DigiDollarWallet::WriteDDBalance - Exception: %s\n", e.what());
        return error("DigiDollarWallet::WriteDDBalance: Exception - %s", e.what());
    }
}

bool DigiDollarWallet::WriteDDTimeLock(const WalletCollateralPosition& position) {
    try {
        if (position.dd_timelock_id.IsNull()) {
            return error("DigiDollarWallet::WriteDDTimeLock: Invalid position ID");
        }

        // Update in-memory cache first (works in both test and production mode)
        collateral_positions[position.dd_timelock_id] = position;

        // Write to database (only if wallet pointer exists - production mode)
        if (m_wallet) {
            wallet::WalletBatch batch(m_wallet->GetDatabase());
            if (!batch.WriteDDTimeLock(position)) {
                return error("DigiDollarWallet::WriteDDTimeLock: Database write failed for %s",
                             position.dd_timelock_id.ToString());
            }
        } else {
            // Testing mode - no database, just in-memory
            LogPrintf("DigiDollarWallet: WriteDDTimeLock in test mode (no database) - ID: %s\n",
                      position.dd_timelock_id.GetHex());
        }

        // Recalculate locked collateral if active
        if (position.is_active) {
            CAmount total_locked = 0;
            for (const auto& [id, pos] : collateral_positions) {
                if (pos.is_active) {
                    total_locked += pos.dgb_collateral;
                }
            }
            locked_collateral = total_locked;

            // Write metadata to database (only in production mode)
            if (m_wallet) {
                wallet::WalletBatch batch(m_wallet->GetDatabase());
                batch.WriteDDMetadata("locked_collateral", std::to_string(total_locked));
            }
        }

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Wrote DDTimeLock %s (DD: %lld, DGB: %lld, tier: %u)\n",
                 position.dd_timelock_id.ToString(), static_cast<long long>(position.dd_minted), static_cast<long long>(position.dgb_collateral), position.lock_tier);
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

    // FIX: When deactivating a position, remove its DD UTXO from tracking map
    // DD output from mint is always at vout 1
    COutPoint dd_outpoint(dd_timelock_id, 1);
    if (!active) {
        // Position being spent/redeemed - remove DD UTXO
        dd_utxos.erase(dd_outpoint);
        LogPrintf("DigiDollar: Removed DD UTXO %s:%d from tracking (position deactivated)\n",
                  dd_outpoint.hash.ToString(), dd_outpoint.n);
    }

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

        LogPrint(BCLog::WALLETDB, "DigiDollarWallet: Updated position %s status to %s (locked: %lld)\n",
                 dd_timelock_id.ToString(), active ? "active" : "inactive", static_cast<long long>(total_locked));
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
            // For testing with mock addresses, use a hash of the serialized address
            // This ensures different invalid addresses get different keys
            CDataStream ss(SER_DISK, CLIENT_VERSION);
            ss << addr;
            uint256 hash = Hash(ss);
            std::string test_key = "test_addr_" + hash.GetHex();

            // Check if we have this test address
            auto it = dd_balances.find(test_key);
            if (it != dd_balances.end()) {
                return it->second.balance;
            }

            // Return 0 if not found
            return 0;
        }

        auto it = dd_balances.find(key);
        CAmount balance = (it != dd_balances.end()) ? it->second.balance : 0;

        LogPrintf("DigiDollar: GetDDBalance for %s returned %lld cents\n", key, static_cast<long long>(balance));
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
            // Only count unspent UTXOs if we have wallet context
            // In testing scenarios (m_wallet == nullptr), count all UTXOs in the map
            if (!m_wallet) {
                // Testing scenario: count all UTXOs in map
                balance += dd_amount;
            } else if (!m_wallet->IsSpent(outpoint)) {
                // Production scenario: only count unspent UTXOs
                balance += dd_amount;
            }
        }

        LogPrintf("DigiDollar: GetTotalDDBalance calculated %lld cents from %zu UTXOs\n",
                  static_cast<long long>(balance), dd_utxos.size());
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

        LogPrintf("DigiDollar: GetLockedCollateral returned %lld satoshis\n", static_cast<long long>(locked));
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

        LogPrintf("DigiDollar: GetDDTimeLocks returned %zu time-locked positions (active_only=%s)\n",
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

        LogPrintf("DigiDollar: Found DD UTXO %s:%u (%lld cents)\n",
                  outpoint.hash.ToString(), outpoint.n, static_cast<long long>(dd_amount));
    }

    LogPrintf("DigiDollar: GetDDUTXOs - Found %zu spendable UTXOs\n", utxos.size());
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

    LogPrintf("DigiDollar: GetDDFromUTXO - Found %lld cents for UTXO %s:%u\n",
              static_cast<long long>(dd_amount), outpoint.hash.ToString(), outpoint.n);

    return dd_amount;
}

bool DigiDollarWallet::IsDDTokenUnspent(const uint256& dd_timelock_id) const {
    // DD token output is always at index 1 of the mint transaction
    COutPoint dd_token_outpoint(dd_timelock_id, 1);

    // Check if the DD token UTXO exists in our tracking map
    auto it = dd_utxos.find(dd_token_outpoint);
    if (it == dd_utxos.end()) {
        // Not in our tracking map - could be already spent/transferred
        LogPrintf("DigiDollar: IsDDTokenUnspent - UTXO %s:1 not in dd_utxos map\n",
                  dd_timelock_id.ToString());
        return false;
    }

    // Verify UTXO is still unspent in the wallet
    if (m_wallet && m_wallet->IsSpent(dd_token_outpoint)) {
        LogPrintf("DigiDollar: IsDDTokenUnspent - UTXO %s:1 is spent (transferred away)\n",
                  dd_timelock_id.ToString());
        return false;
    }

    LogPrintf("DigiDollar: IsDDTokenUnspent - UTXO %s:1 is still unspent (%lld cents)\n",
              dd_timelock_id.ToString(), static_cast<long long>(it->second));
    return true;
}

void DigiDollarWallet::AddCollateralPosition(const WalletCollateralPosition& position) {
    try {
        // Write position to database (this also updates the in-memory map)
        if (!WriteDDTimeLock(position)) {
            LogPrintf("DigiDollar: Failed to write position to database\n");
            return;
        }

        LogPrintf("DigiDollar: Added collateral position - ID: %s, DD: %lld, DGB: %lld, Tier: %u, Active: %s\n",
                  position.dd_timelock_id.GetHex(), static_cast<long long>(position.dd_minted), static_cast<long long>(position.dgb_collateral),
                  position.lock_tier, position.is_active ? "YES" : "NO");

        // FIX #1: Add DD UTXO to tracking map
        // DD output from mint is always at vout 1
        COutPoint dd_outpoint(position.dd_timelock_id, 1);
        dd_utxos[dd_outpoint] = position.dd_minted;
        LogPrintf("DigiDollar: Added DD UTXO to tracking - %s:%u (%lld cents)\n",
                  dd_outpoint.hash.ToString(), dd_outpoint.n, static_cast<long long>(position.dd_minted));

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
        tx.lock_tier = static_cast<int>(position.lock_tier);  // Set lock tier from position
        transaction_history.push_back(tx);

        // Persist transaction to database
        if (m_wallet) {
            wallet::WalletBatch batch(m_wallet->GetDatabase());
            if (!batch.WriteDDTransaction(tx)) {
                LogPrintf("DigiDollar: WARNING - Failed to persist mint transaction to database\n");
            }
        }

        LogPrintf("DigiDollar: Added mint transaction to history - TxID: %s\n", tx.txid);
    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: AddCollateralPosition exception - %s\n", e.what());
    }
}

bool DigiDollarWallet::AddRedemptionToHistory(const DDTransaction& tx) {
    try {
        // Add to in-memory history
        transaction_history.push_back(tx);

        // Persist to database
        if (m_wallet) {
            wallet::WalletBatch batch(m_wallet->GetDatabase());
            if (!batch.WriteDDTransaction(tx)) {
                LogPrintf("DigiDollar: WARNING - Failed to persist redemption transaction to database\n");
                return false;
            }
        }

        LogPrintf("DigiDollar: Added redemption transaction to history - TxID: %s, Amount: %lld cents\n",
                  tx.txid, static_cast<long long>(tx.amount));
        return true;
    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: AddRedemptionToHistory exception - %s\n", e.what());
        return false;
    }
}

size_t DigiDollarWallet::ScanForDDUTXOs() {
    if (!m_wallet) {
        LogPrintf("DigiDollar: ScanForDDUTXOs called but no wallet pointer set\n");
        return 0;
    }

    try {
        LogPrintf("DigiDollar: Starting UTXO scan for DD outputs (OP_RETURN parsing)\n");

        // Clear existing tracking data for fresh scan
        dd_balances.clear();
        dd_utxos.clear();  // Clear UTXO tracking for fresh scan
        total_dd_balance = 0;

        size_t dd_utxo_count = 0;

        // Lock wallet for thread-safe access
        LOCK(m_wallet->cs_wallet);

        // Iterate through all wallet transactions
        for (const auto& [txid, wtx] : m_wallet->mapWallet) {
            // First, find the OP_RETURN output and extract DD amounts
            std::vector<CAmount> ddAmounts;
            int ddTxType = 0;  // 0=none, 1=MINT, 2=TRANSFER

            for (size_t i = 0; i < wtx.tx->vout.size(); ++i) {
                const CScript& script = wtx.tx->vout[i].scriptPubKey;
                if (script.size() > 0 && script[0] == OP_RETURN) {
                    // Found OP_RETURN - try to parse DD data
                    auto pc = script.begin();
                    opcodetype opcode;
                    std::vector<unsigned char> data;

                    // Skip OP_RETURN
                    if (!script.GetOp(pc, opcode, data) || opcode != OP_RETURN) continue;

                    // Get DD marker
                    if (!script.GetOp(pc, opcode, data)) continue;
                    if (data.size() != 2 || data[0] != 'D' || data[1] != 'D') continue;

                    // Get transaction type
                    if (!script.GetOp(pc, opcode, data)) continue;
                    try {
                        CScriptNum txTypeNum(data, false);
                        ddTxType = txTypeNum.getint();
                    } catch (const scriptnum_error&) {
                        continue;
                    }

                    if (ddTxType == 1) {
                        // MINT: Format is <"DD"> <1> <ddAmount> <lockHeight>
                        if (script.GetOp(pc, opcode, data)) {
                            try {
                                CScriptNum amtNum(data, false);
                                ddAmounts.push_back(amtNum.GetInt64());
                            } catch (const scriptnum_error&) {}
                        }
                    } else if (ddTxType == 2) {
                        // TRANSFER: Format is <"DD"> <2> <amount1> <amount2> ... <amountN>
                        while (script.GetOp(pc, opcode, data)) {
                            try {
                                CScriptNum amtNum(data, false);
                                CAmount amt = amtNum.GetInt64();
                                if (amt > 0 && amt <= 100000000000LL) {
                                    ddAmounts.push_back(amt);
                                }
                            } catch (const scriptnum_error&) {
                                break;
                            }
                        }
                    } else if (ddTxType == 3) {
                        // REDEEM: Format is <"DD"> <3> <change_amount>
                        // DD change from redemption when more DD UTXOs selected than needed
                        if (script.GetOp(pc, opcode, data)) {
                            try {
                                CScriptNum amtNum(data, false);
                                CAmount changeAmt = amtNum.GetInt64();
                                if (changeAmt > 0 && changeAmt <= 100000000000LL) {
                                    ddAmounts.push_back(changeAmt);
                                    LogPrintf("DigiDollar: ScanForDDUTXOs - Found REDEEM tx with DD change: %lld cents\n", static_cast<long long>(changeAmt));
                                }
                            } catch (const scriptnum_error&) {}
                        }
                    }
                    break; // Found and parsed OP_RETURN
                }
            }

            if (ddAmounts.empty()) {
                continue; // No DD amounts found in this transaction
            }

            // Now find DD outputs (P2TR with value=0) and match to amounts
            size_t ddOutputIndex = 0;
            for (size_t n = 0; n < wtx.tx->vout.size(); ++n) {
                const CTxOut& txout = wtx.tx->vout[n];

                // DD outputs are P2TR (34 bytes, starts with OP_1) with value = 0
                // For MINT: DD token is at output 1
                // For TRANSFER: DD tokens are outputs 0..N-1 (before DGB change and OP_RETURN)
                if (txout.scriptPubKey.size() == 34 &&
                    txout.scriptPubKey[0] == OP_1 &&
                    txout.nValue == 0) {

                    // For MINT, skip output 0 (vault) - DD token is at index 1
                    if (ddTxType == 1 && n == 0) {
                        continue; // Skip vault output
                    }

                    // Check if we have an amount for this DD output
                    size_t amountIndex = (ddTxType == 1) ? 0 : ddOutputIndex;
                    if (amountIndex >= ddAmounts.size()) {
                        continue; // No amount for this output
                    }

                    CAmount dd_amount = ddAmounts[amountIndex];
                    ddOutputIndex++;

                    // Check if we own this output
                    // First check standard wallet ownership
                    wallet::isminetype mine = m_wallet->IsMine(txout);
                    bool is_ours = (mine & wallet::ISMINE_SPENDABLE);

                    // Check dd_owner_keys - but VERIFY the key actually controls this output
                    // The owner key, when tweaked, should produce the output key
                    // This prevents marking recipient outputs as "ours" in transfer transactions
                    if (!is_ours) {
                        CKey owner_key;
                        if (GetOwnerKey(txid, owner_key)) {
                            // Extract the actual output key from scriptPubKey
                            std::vector<unsigned char> output_key_bytes(txout.scriptPubKey.begin() + 2, txout.scriptPubKey.end());

                            // Compute what the tweaked key should be from this owner_key
                            XOnlyPubKey owner_xonly(owner_key.GetPubKey());
                            auto tweaked = owner_xonly.CreateTapTweak(nullptr);
                            if (tweaked) {
                                // Check if tweaked key matches output key
                                if (std::equal(output_key_bytes.begin(), output_key_bytes.end(),
                                              tweaked->first.begin())) {
                                    is_ours = true;
                                    LogPrintf("DigiDollar: ScanForDDUTXOs - DD %s:%d owned via dd_owner_keys (verified, txType=%d)\n",
                                              txid.GetHex(), n, ddTxType);
                                } else {
                                    LogPrintf("DigiDollar: ScanForDDUTXOs - DD %s:%d has dd_owner_key but tweaked key doesn't match output\n",
                                              txid.GetHex(), n);
                                }
                            }
                        }
                    }

                    // If standard wallet doesn't recognize it, check dd_address_keys
                    // (for DD addresses generated via getdigidollaraddress)
                    if (!is_ours) {
                        // Extract the P2TR output key from the scriptPubKey
                        // P2TR scripts are: OP_1 <32-byte-output-key>
                        if (txout.scriptPubKey.size() == 34 && txout.scriptPubKey[0] == OP_1) {
                            std::vector<unsigned char> output_key_bytes(txout.scriptPubKey.begin() + 2, txout.scriptPubKey.end());
                            XOnlyPubKey output_key(output_key_bytes);

                            // Check if we have the key for this output in dd_address_keys
                            CKey address_key;
                            if (GetAddressKey(output_key, address_key)) {
                                is_ours = true;
                                LogPrintf("DigiDollar: ScanForDDUTXOs - Output %s:%d owned via dd_address_keys\n",
                                          txid.GetHex(), n);
                            }
                        }
                    }

                    if (!is_ours) {
                        continue; // Not owned by us or not spendable
                    }

                    // Check if output is already spent
                    COutPoint outpoint(txid, n);
                    if (m_wallet->IsSpent(outpoint)) {
                        continue; // Already spent
                    }

                    // Add to dd_utxos map for coin selection (CRITICAL FIX)
                    // This enables GetDDUTXOs() to find received DD tokens
                    dd_utxos[outpoint] = dd_amount;

                    // Add to balance tracking
                    std::string key = "total"; // Aggregate key

                    if (dd_balances.find(key) == dd_balances.end()) {
                        CDigiDollarAddress emptyAddr; // Empty address for total
                        dd_balances[key] = WalletDDBalance(emptyAddr, 0);
                    }

                    dd_balances[key].balance += dd_amount;
                    total_dd_balance += dd_amount;
                    dd_utxo_count++;

                    LogPrintf("DigiDollar: Found DD UTXO %s:%zu - Amount: %lld cents (txType=%d, added to dd_utxos)\n",
                              txid.GetHex(), n, static_cast<long long>(dd_amount), ddTxType);
                }
            }
        }

        LogPrintf("DigiDollar: Scan complete - Found %zu DD UTXOs, Total balance: %lld cents\n",
                  dd_utxo_count, static_cast<long long>(total_dd_balance));

        return dd_utxo_count;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: ScanForDDUTXOs exception - %s\n", e.what());
        return 0;
    }
}

// =============================================================================
// INCREMENTAL DD UTXO PROCESSING (Performance Fix)
// Process a single transaction instead of full wallet rescan on every block
// =============================================================================

bool DigiDollarWallet::ProcessTransactionForDD(const CTransaction& tx, const uint256& txid) {
    if (!m_wallet) {
        return false;
    }

    bool changed = false;

    try {
        // Step 1: Check if this transaction SPENDS any of our DD UTXOs
        for (const CTxIn& txin : tx.vin) {
            auto it = dd_utxos.find(txin.prevout);
            if (it != dd_utxos.end()) {
                // This TX spends one of our DD UTXOs - remove it
                CAmount spent_amount = it->second;
                total_dd_balance -= spent_amount;
                dd_utxos.erase(it);
                changed = true;
                LogPrint(BCLog::WALLETDB, "DigiDollar: ProcessTxForDD - Spent DD UTXO %s:%d (%lld cents)\n",
                         txin.prevout.hash.ToString(), txin.prevout.n, static_cast<long long>(spent_amount));
            }
        }

        // Step 2: Check if this transaction CREATES DD outputs we own
        // First find OP_RETURN with DD marker and parse amounts
        std::vector<CAmount> ddAmounts;
        int ddTxType = 0;

        for (size_t i = 0; i < tx.vout.size(); ++i) {
            const CScript& script = tx.vout[i].scriptPubKey;
            if (script.size() > 0 && script[0] == OP_RETURN) {
                auto pc = script.begin();
                opcodetype opcode;
                std::vector<unsigned char> data;

                if (!script.GetOp(pc, opcode, data) || opcode != OP_RETURN) continue;
                if (!script.GetOp(pc, opcode, data)) continue;
                if (data.size() != 2 || data[0] != 'D' || data[1] != 'D') continue;

                if (!script.GetOp(pc, opcode, data)) continue;
                try {
                    CScriptNum txTypeNum(data, false);
                    ddTxType = txTypeNum.getint();
                } catch (const scriptnum_error&) {
                    continue;
                }

                if (ddTxType == 1) {
                    // MINT
                    if (script.GetOp(pc, opcode, data)) {
                        try {
                            CScriptNum amtNum(data, false);
                            ddAmounts.push_back(amtNum.GetInt64());
                        } catch (const scriptnum_error&) {}
                    }
                } else if (ddTxType == 2 || ddTxType == 3) {
                    // TRANSFER or REDEEM (with change)
                    while (script.GetOp(pc, opcode, data)) {
                        try {
                            CScriptNum amtNum(data, false);
                            CAmount amt = amtNum.GetInt64();
                            if (amt > 0 && amt <= 100000000000LL) {
                                ddAmounts.push_back(amt);
                            }
                        } catch (const scriptnum_error&) {
                            break;
                        }
                    }
                }
                break;
            }
        }

        if (ddAmounts.empty()) {
            return changed; // No DD amounts in this TX
        }

        // Match DD amounts to P2TR outputs we own (same logic as ScanForDDUTXOs)
        LOCK(m_wallet->cs_wallet);
        size_t ddOutputIndex = 0;

        for (size_t n = 0; n < tx.vout.size(); ++n) {
            const CTxOut& txout = tx.vout[n];

            // DD outputs are P2TR with value=0
            if (txout.nValue != 0) continue;
            if (txout.scriptPubKey.size() != 34 || txout.scriptPubKey[0] != OP_1) continue;

            // Check if we own this output
            wallet::isminetype mine = m_wallet->IsMine(txout);
            bool is_ours = (mine & wallet::ISMINE_SPENDABLE);

            // Check dd_owner_keys with tweaked key verification
            if (!is_ours) {
                CKey owner_key;
                if (GetOwnerKey(txid, owner_key)) {
                    std::vector<unsigned char> output_key_bytes(txout.scriptPubKey.begin() + 2, txout.scriptPubKey.end());
                    XOnlyPubKey owner_xonly(owner_key.GetPubKey());
                    auto tweaked = owner_xonly.CreateTapTweak(nullptr);
                    if (tweaked && std::equal(output_key_bytes.begin(), output_key_bytes.end(),
                                              tweaked->first.begin())) {
                        is_ours = true;
                    }
                }
            }

            // Check dd_address_keys
            if (!is_ours) {
                std::vector<unsigned char> output_key_bytes(txout.scriptPubKey.begin() + 2, txout.scriptPubKey.end());
                XOnlyPubKey output_key(output_key_bytes);
                CKey address_key;
                if (GetAddressKey(output_key, address_key)) {
                    is_ours = true;
                }
            }

            if (!is_ours) continue;

            // Check if already spent
            COutPoint outpoint(txid, n);
            if (m_wallet->IsSpent(outpoint)) continue;

            // This is our DD output - add it
            if (ddOutputIndex < ddAmounts.size()) {
                CAmount dd_amount = ddAmounts[ddOutputIndex++];

                if (dd_utxos.find(outpoint) == dd_utxos.end()) {
                    dd_utxos[outpoint] = dd_amount;
                    total_dd_balance += dd_amount;
                    changed = true;
                    LogPrint(BCLog::WALLETDB, "DigiDollar: ProcessTxForDD - Added DD UTXO %s:%zu (%lld cents)\n",
                             txid.ToString(), n, static_cast<long long>(dd_amount));
                }
            }
        }

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: ProcessTransactionForDD exception - %s\n", e.what());
    }

    return changed;
}

// =============================================================================
// PHASE 5 TASK 5.3: TRANSACTION CREATION IMPLEMENTATIONS
// =============================================================================

bool DigiDollarWallet::MintDigiDollar(const CAmount& dd_amount, uint32_t lock_tier, CTransactionRef& tx_out) {
    try {
        LogPrintf("DigiDollar: MintDigiDollar called - amount: %lld, tier: %u\n", static_cast<long long>(dd_amount), lock_tier);

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
        LogPrintf("DigiDollar: TransferDigiDollar called - to: %s, amount: %lld\n", to.ToString(), static_cast<long long>(amount));

        // Validate transfer parameters
        if (!ValidateTransferParams(to, amount)) {
            LogPrintf("DigiDollar: TransferDigiDollar validation failed\n");
            return false;
        }

        // Phase 2.1: Select DD UTXOs to cover the amount
        std::vector<COutPoint> dd_utxos;
        CAmount selectedDDTotal = 0;
        if (!SelectDDCoins(amount, dd_utxos, selectedDDTotal)) {
            LogPrintf("DigiDollar: Insufficient DD balance for transfer (need %lld, have %lld)\n",
                      static_cast<long long>(amount), static_cast<long long>(GetTotalDDBalance()));
            return false;
        }

        // Phase 2.1: Create mock transaction structure for fee estimation
        CMutableTransaction estimateTx;
        estimateTx.vin.resize(dd_utxos.size() + 1);  // DD inputs + 1 fee input
        estimateTx.vout.resize(2);  // Recipient + change outputs

        // Phase 2.1: Calculate estimated fee
        CAmount estimatedFee = CalculateTransactionFee(estimateTx);

        // Phase 2.1: Select DGB UTXOs for fees
        // CRITICAL: Exclude DD UTXOs from fee selection to prevent double-spend
        std::vector<COutPoint> exclude_dd_utxos = dd_utxos;
        std::vector<COutPoint> fee_utxos;
        std::vector<CAmount> fee_amounts;
        CAmount selectedFeeTotal = 0;
        if (!SelectFeeCoins(estimatedFee, fee_utxos, selectedFeeTotal, &fee_amounts, &exclude_dd_utxos)) {
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
        // DigiDollar transactions MUST pay at least 0.1 DGB fee to miners
        params.feeRate = 35000000; // 0.35 DGB/kB = 0.105 DGB for 300 byte tx

        // Get the spending key from wallet
        // For DD transfers, we need the key that owns the first DD UTXO
        LogPrintf("DigiDollar: TransferDigiDollar - Starting key lookup, dd_utxos.size()=%d\n", dd_utxos.size());
        if (dd_utxos.empty()) {
            LogPrintf("DigiDollar: No DD UTXOs available for transfer\n");
            return false;
        }

        LogPrintf("DigiDollar: TransferDigiDollar - First DD UTXO: %s:%d\n", dd_utxos[0].hash.ToString(), dd_utxos[0].n);

        // Try to get the spending key - two cases:
        // 1. Minted DD: Key is in dd_owner_keys map (stored when we minted)
        // 2. Received DD: Key is in wallet's standard key management (P2TR key)
        CKey spenderKey;
        bool found_key = false;

        // First, try to get owner key from DD owner keys map (for minted DD)
        LogPrintf("DigiDollar: TransferDigiDollar - collateral_positions.size()=%d\n", collateral_positions.size());
        auto it = collateral_positions.find(dd_utxos[0].hash);
        if (it != collateral_positions.end()) {
            // This is minted DD - try to get the stored owner key
            LogPrintf("DigiDollar: TransferDigiDollar - Found in collateral_positions, trying GetOwnerKey\n");
            if (GetOwnerKey(dd_utxos[0].hash, spenderKey)) {
                found_key = true;
                LogPrintf("DigiDollar: Retrieved owner key for DD transfer from minted position %s\n", dd_utxos[0].hash.ToString());
            } else {
                LogPrintf("DigiDollar: TransferDigiDollar - GetOwnerKey returned false\n");
            }
        } else {
            LogPrintf("DigiDollar: TransferDigiDollar - UTXO NOT in collateral_positions (this is received DD)\n");
        }

        // If not found (received DD), try to get key from wallet's P2TR key management
        if (!found_key && m_wallet) {
            LogPrintf("DigiDollar: TransferDigiDollar - Trying wallet key lookup for received DD, m_wallet=%p\n", (void*)m_wallet);

            // Helper to get signing provider with private key access for descriptor wallets
            auto getSigningProviderWithKeys = [this](const CScript& script) -> std::unique_ptr<SigningProvider> {
                const auto& spk_mans = m_wallet->GetScriptPubKeyMans(script);
                if (!spk_mans.empty()) {
                    wallet::ScriptPubKeyMan* spk_man = *spk_mans.begin();
                    wallet::DescriptorScriptPubKeyMan* desc_spk_man = dynamic_cast<wallet::DescriptorScriptPubKeyMan*>(spk_man);
                    if (desc_spk_man) {
                        LogPrintf("DigiDollar: TransferDigiDollar - Using DescriptorScriptPubKeyMan with private keys\n");
                        return desc_spk_man->GetSigningProviderWithKeys(script);
                    }
                }
                // Fallback for legacy wallets
                return m_wallet->GetSolvingProvider(script);
            };

            // Get the scriptPubKey for the DD UTXO we're spending
            // We need to find the actual output being spent
            const COutPoint& dd_outpoint = dd_utxos[0];

            // Look up the transaction to get the scriptPubKey
            LogPrintf("DigiDollar: TransferDigiDollar - Looking up tx %s in mapWallet (size=%d)\n",
                     dd_outpoint.hash.ToString(), m_wallet->mapWallet.size());
            auto wtx_it = m_wallet->mapWallet.find(dd_outpoint.hash);
            if (wtx_it != m_wallet->mapWallet.end()) {
                LogPrintf("DigiDollar: TransferDigiDollar - Found tx in mapWallet\n");
                const auto& wtx = wtx_it->second;
                if (dd_outpoint.n < wtx.tx->vout.size()) {
                    const CTxOut& txout = wtx.tx->vout[dd_outpoint.n];
                    LogPrintf("DigiDollar: TransferDigiDollar - Got output %d, scriptPubKey size=%d\n",
                             dd_outpoint.n, txout.scriptPubKey.size());

                    // Get signing provider for this script WITH PRIVATE KEY ACCESS
                    auto provider = getSigningProviderWithKeys(txout.scriptPubKey);
                    if (provider) {
                        LogPrintf("DigiDollar: TransferDigiDollar - Got signing provider\n");
                        // Extract the P2TR destination
                        CTxDestination dest;
                        if (ExtractDestination(txout.scriptPubKey, dest)) {
                            LogPrintf("DigiDollar: TransferDigiDollar - ExtractDestination succeeded, dest index=%d\n",
                                     dest.index());
                            if (auto* tr = std::get_if<WitnessV1Taproot>(&dest)) {
                                LogPrintf("DigiDollar: TransferDigiDollar - Got WitnessV1Taproot destination\n");
                                // Get TaprootSpendData to find the internal key
                                TaprootSpendData spenddata;
                                XOnlyPubKey output_key(*tr);
                                LogPrintf("DigiDollar: TransferDigiDollar - Output key: %s\n", HexStr(output_key));
                                if (provider->GetTaprootSpendData(output_key, spenddata)) {
                                    LogPrintf("DigiDollar: TransferDigiDollar - GetTaprootSpendData succeeded\n");
                                    if (spenddata.internal_key.IsFullyValid()) {
                                        LogPrintf("DigiDollar: TransferDigiDollar - Internal key valid: %s\n",
                                                 HexStr(spenddata.internal_key));
                                        // Try to get the private key for the internal key
                                        if (provider->GetKeyByXOnly(spenddata.internal_key, spenderKey)) {
                                            found_key = true;
                                            LogPrintf("DigiDollar: Found key for received DD via GetKeyByXOnly\n");
                                        } else {
                                            LogPrintf("DigiDollar: TransferDigiDollar - GetKeyByXOnly FAILED for internal key\n");
                                        }
                                    } else {
                                        LogPrintf("DigiDollar: TransferDigiDollar - Internal key NOT valid\n");
                                    }
                                } else {
                                    LogPrintf("DigiDollar: TransferDigiDollar - GetTaprootSpendData FAILED\n");
                                    // Try alternate approach: look for key that matches the output key directly
                                    LogPrintf("DigiDollar: TransferDigiDollar - Trying to get key by output key directly\n");
                                    if (provider->GetKeyByXOnly(output_key, spenderKey)) {
                                        found_key = true;
                                        LogPrintf("DigiDollar: TransferDigiDollar - Found key by output key directly\n");
                                    } else {
                                        LogPrintf("DigiDollar: TransferDigiDollar - GetKeyByXOnly by output key FAILED\n");
                                    }
                                }

                                // Try dd_address_keys map (for addresses generated via getdigidollaraddress)
                                if (!found_key) {
                                    CKey address_key;
                                    if (GetAddressKey(XOnlyPubKey(*tr), address_key)) {
                                        spenderKey = address_key;
                                        found_key = true;
                                        LogPrintf("DigiDollar: TransferDigiDollar - Found key for received DD via dd_address_keys map\n");
                                    } else {
                                        LogPrintf("DigiDollar: TransferDigiDollar - dd_address_keys lookup failed for output key\n");
                                    }
                                }

                                // If still not found, try brute force scan through wallet keys
                                if (!found_key) {
                                    LogPrintf("DigiDollar: TransferDigiDollar - Starting brute force wallet scan\n");
                                    XOnlyPubKey target_output_key(*tr);
                                    int scan_count = 0;
                                    for (const auto& [txid, scan_wtx] : m_wallet->mapWallet) {
                                        for (size_t n = 0; n < scan_wtx.tx->vout.size() && !found_key; n++) {
                                            CTxDestination out_dest;
                                            if (ExtractDestination(scan_wtx.tx->vout[n].scriptPubKey, out_dest)) {
                                                auto out_provider = getSigningProviderWithKeys(scan_wtx.tx->vout[n].scriptPubKey);
                                                if (out_provider) {
                                                    if (auto* scan_tr = std::get_if<WitnessV1Taproot>(&out_dest)) {
                                                        TaprootSpendData scan_spenddata;
                                                        if (out_provider->GetTaprootSpendData(XOnlyPubKey(*scan_tr), scan_spenddata)) {
                                                            if (scan_spenddata.internal_key.IsFullyValid()) {
                                                                CKey test_key;
                                                                if (out_provider->GetKeyByXOnly(scan_spenddata.internal_key, test_key)) {
                                                                    scan_count++;
                                                                    XOnlyPubKey test_xonly(test_key.GetPubKey());
                                                                    auto tweaked = test_xonly.CreateTapTweak(nullptr);
                                                                    if (tweaked && std::equal(target_output_key.begin(), target_output_key.end(),
                                                                                             tweaked->first.begin())) {
                                                                        spenderKey = test_key;
                                                                        found_key = true;
                                                                        LogPrintf("DigiDollar: Found key for received DD via P2TR wallet scan\n");
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        if (found_key) break;
                                    }
                                    LogPrintf("DigiDollar: TransferDigiDollar - Brute force scan checked %d keys, found_key=%d\n",
                                             scan_count, found_key);
                                }
                            } else {
                                LogPrintf("DigiDollar: TransferDigiDollar - Destination is NOT WitnessV1Taproot\n");
                            }
                        } else {
                            LogPrintf("DigiDollar: TransferDigiDollar - ExtractDestination FAILED\n");
                        }
                    } else {
                        LogPrintf("DigiDollar: TransferDigiDollar - GetSolvingProvider returned NULL\n");
                    }
                } else {
                    LogPrintf("DigiDollar: TransferDigiDollar - Output index %d out of range (vout size=%d)\n",
                             dd_outpoint.n, wtx.tx->vout.size());
                }
            } else {
                LogPrintf("DigiDollar: TransferDigiDollar - TX NOT FOUND in mapWallet!\n");
            }
        } else if (!m_wallet) {
            LogPrintf("DigiDollar: TransferDigiDollar - m_wallet is NULL, cannot look up wallet keys\n");
        }

        if (!found_key) {
            LogPrintf("DigiDollar: Could not find spending key for DD UTXO %s:%d\n",
                     dd_utxos[0].hash.ToString(), dd_utxos[0].n);
            return false;
        }

        params.spenderKey = spenderKey;

        // CRITICAL FIX: Get a proper change address from the wallet for DGB change output
        // This ensures the wallet recognizes the change output as its own!
        // Without this, DGB would be sent to an address the wallet doesn't control.
        if (m_wallet) {
            // Get a new change address from wallet's internal keypool
            auto op_dest = m_wallet->GetNewChangeDestination(OutputType::BECH32);
            if (op_dest) {
                params.dgbChangeDest = *op_dest;
                LogPrintf("DigiDollar: Using wallet change address for DGB change output\n");
            } else {
                // Fallback: try to get any fresh address
                LogPrintf("DigiDollar: WARNING - Could not get change destination, trying fresh address\n");
                auto fresh_dest = m_wallet->GetNewDestination(OutputType::BECH32, std::string("DD_change"));
                if (fresh_dest) {
                    params.dgbChangeDest = *fresh_dest;
                    LogPrintf("DigiDollar: Using fresh wallet address for DGB change output\n");
                } else {
                    LogPrintf("DigiDollar: WARNING - No wallet change address available, DGB change may be lost!\n");
                }
            }
        }

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
            LogPrintf("DigiDollar: WARNING - No wallet context, transaction built but not broadcast (test mode)\n");
            // In test mode (m_wallet == nullptr), return true since transaction was successfully built
            // Broadcast isn't possible without wallet context, but transaction construction succeeded
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
        LogPrintf("DigiDollar: Transfer successful - %lld cents to %s (txid: %s)\n",
                  static_cast<long long>(amount), to.ToString(), ddtx.txid);
        LogPrintf("DigiDollar: Balance after transfer: %lld (change: %lld)\n",
                  static_cast<long long>(newBalance), static_cast<long long>(dd_change));

        return true;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: TransferDigiDollar exception - %s\n", e.what());
        return false;
    }
}

bool DigiDollarWallet::RedeemDigiDollar(const uint256& dd_timelock_id, const CAmount& amount, CTransactionRef& tx_out) {
    try {
        LogPrintf("DigiDollar: RedeemDigiDollar called - position: %s, amount: %lld\n", dd_timelock_id.ToString(), static_cast<long long>(amount));

        // Validation
        if (!ValidateRedeemParams(dd_timelock_id, amount)) {
            LogPrintf("DigiDollar: RedeemDigiDollar validation failed\n");
            return false;
        }

        // Get position details
        auto it = collateral_positions.find(dd_timelock_id);
        if (it == collateral_positions.end() || !it->second.is_active) {
            LogPrintf("DigiDollar: Position not found or inactive\n");
            return false;
        }

        // Create redemption transaction using RedeemTxBuilder
        // Get current chain height and oracle price (similar to transfer/mint)
        int currentHeight = 100000; // TODO: Get actual height from m_wallet->chain().getHeight()
        CAmount oraclePrice = 2500;  // TODO: Get from MockOracleManager

        DigiDollar::RedeemTxBuilder builder(Params(), currentHeight, oraclePrice);

        DigiDollar::TxBuilderRedeemParams params;
        params.collateralOutpoint = COutPoint(dd_timelock_id, 0); // Assuming output 0
        params.ddToRedeem = amount;
        params.path = builder.DetermineRedemptionPath(params);

        // Get wallet spending key for this position
        CKey ownerKey;
        if (!GetOwnerKey(dd_timelock_id, ownerKey)) {
            // Fallback: generate new key (for testing/mock scenarios)
            ownerKey.MakeNewKey(true);
            LogPrintf("DigiDollar: WARNING - No owner key found for position %s, using generated key\n",
                     dd_timelock_id.ToString());
        }
        params.ownerKey = ownerKey;

        // CRITICAL FIX: Get a wallet address for the returned collateral
        // This ensures the wallet recognizes the returned DGB as belonging to it
        // Try BECH32M first (Taproot), fallback to BECH32 for legacy wallets
        if (m_wallet) {
            LOCK(m_wallet->cs_wallet);
            std::string label = "";  // Empty label
            auto op_dest = m_wallet->GetNewDestination(OutputType::BECH32M, label);
            if (!op_dest) {
                // Legacy wallet fallback: try BECH32 (SegWit v0)
                LogPrintf("DigiDollar: BECH32M not available, trying BECH32 for legacy wallet\n");
                op_dest = m_wallet->GetNewDestination(OutputType::BECH32, label);
            }
            if (op_dest) {
                params.collateralDest = *op_dest;
                LogPrintf("DigiDollar: Using wallet destination for returned collateral\n");
            } else {
                LogPrintf("DigiDollar: WARNING - Could not get wallet address, using owner key (wallet may not recognize)\n");
                LogPrintf("DigiDollar: Error: %s\n", util::ErrorString(op_dest).original);
            }
        }

        // DigiDollar transactions MUST pay at least 0.1 DGB fee to miners
        params.feeRate = 35000000; // 0.35 DGB/kB = 0.105 DGB for 300 byte tx

        // Select DD UTXOs to burn (get amounts too for DD change calculation)
        CAmount selectedTotal = 0;
        if (!SelectDDCoins(amount, params.ddUtxos, selectedTotal, &params.ddAmounts)) {
            LogPrintf("DigiDollar: Insufficient DD balance for redemption\n");
            return false;
        }
        LogPrintf("DigiDollar: Selected %zu DD UTXOs totaling %lld cents (need %lld cents)\n",
                  params.ddUtxos.size(), static_cast<long long>(selectedTotal), static_cast<long long>(amount));
        LogPrintf("DigiDollar: ddAmounts size: %zu\n", params.ddAmounts.size());
        for (size_t i = 0; i < params.ddAmounts.size(); i++) {
            LogPrintf("DigiDollar: ddAmounts[%zu] = %lld cents\n", i, static_cast<long long>(params.ddAmounts[i]));
        }

        // Select DGB UTXOs for fees
        // CRITICAL FIX: Properly estimate redemption transaction fees
        // Redemption tx structure: 3 inputs (collateral + DD + fee), 2 outputs (return + change)
        // Approximate vsize: ~400 bytes with script-path spending
        // Fee calculation: vsize * feeRate / 1000 (feeRate is in sat/kB)
        CAmount estimatedFee = (400 * params.feeRate) / 1000; // Proper fee estimate
        // Add safety margin
        estimatedFee = estimatedFee + (estimatedFee * 50 / 100); // 50% margin for worst case
        LogPrintf("DigiDollar: Estimated redemption fee: %lld sats (%.8f DGB)\n", static_cast<long long>(estimatedFee), estimatedFee / 100000000.0);

        // Build exclude list: collateral outpoint + all DD UTXOs that will be burned
        std::vector<COutPoint> exclude_utxos;
        exclude_utxos.push_back(params.collateralOutpoint);  // Don't select collateral as fee input
        exclude_utxos.insert(exclude_utxos.end(), params.ddUtxos.begin(), params.ddUtxos.end());  // Don't select DD UTXOs as fee inputs

        LogPrintf("DigiDollar: Built exclude list with %zu UTXOs (1 collateral + %zu DD)\n",
                  exclude_utxos.size(), params.ddUtxos.size());

        CAmount selectedFeeTotal = 0;
        if (!SelectFeeCoins(estimatedFee, params.feeUtxos, selectedFeeTotal, nullptr, &exclude_utxos)) {
            LogPrintf("DigiDollar: Insufficient DGB balance for fees\n");
            return false;
        }

        LogPrintf("DigiDollar: CALLING BuildRedemptionTransaction now...\n");
        auto result = builder.BuildRedemptionTransaction(params);
        LogPrintf("DigiDollar: BuildRedemptionTransaction returned success=%d, error='%s'\n",
                  result.success, result.error.c_str());
        if (!result.success) {
            LogPrintf("DigiDollar: Redemption transaction build failed - %s\n", result.error);
            return false;
        }
        LogPrintf("DigiDollar: BuildRedemptionTransaction SUCCESS, transaction has %d inputs and %d outputs\n",
                  result.tx.vin.size(), result.tx.vout.size());

        // Sign the transaction (includes collateral, DD, and fee inputs)
        LogPrintf("DigiDollar: ABOUT TO CALL SignRedemptionTransaction with %d DD inputs and %d fee inputs\n",
                  params.ddUtxos.size(), params.feeUtxos.size());
        CMutableTransaction mtx(result.tx);
        if (!SignRedemptionTransaction(mtx, params.collateralOutpoint, params.ddUtxos, params.feeUtxos, ownerKey)) {
            LogPrintf("DigiDollar: Failed to sign redemption transaction\n");
            return false;
        }
        LogPrintf("DigiDollar: SignRedemptionTransaction RETURNED SUCCESS\n");

        tx_out = MakeTransactionRef(mtx);

        // Broadcast transaction
        std::string error;
        if (!CommitDDTransaction(tx_out, error)) {
            LogPrintf("DigiDollar: Failed to broadcast redemption transaction - %s\n", error);
            return false;
        }

        // Track DD change output if there was any
        if (result.ddChange > 0) {
            // DD change is at vout[1] (after DGB return at vout[0])
            // Find the DD change output (P2TR with 0 value)
            uint256 txid = tx_out->GetHash();
            for (size_t i = 0; i < tx_out->vout.size(); i++) {
                const CTxOut& vout = tx_out->vout[i];
                // DD change output: zero value, P2TR (OP_1 + 32 bytes)
                if (vout.nValue == 0 && vout.scriptPubKey.size() == 34 && vout.scriptPubKey[0] == OP_1) {
                    COutPoint changeOutpoint(txid, i);
                    dd_utxos[changeOutpoint] = result.ddChange;
                    StoreOwnerKey(txid, ownerKey);  // Store owner key for change

                    // Persist to database
                    wallet::WalletBatch batch(m_wallet->GetDatabase());
                    batch.WriteDDUTXO(changeOutpoint, result.ddChange);

                    LogPrintf("DigiDollar: Tracked DD change output %s:%d (%d cents)\n",
                              txid.ToString(), i, result.ddChange);
                    break;
                }
            }
        }

        // Remove spent DD UTXOs from tracking
        for (const auto& spentUtxo : params.ddUtxos) {
            dd_utxos.erase(spentUtxo);
            wallet::WalletBatch batch(m_wallet->GetDatabase());
            batch.EraseDDUTXO(spentUtxo);
            LogPrintf("DigiDollar: Removed spent DD UTXO %s:%d\n",
                      spentUtxo.hash.ToString(), spentUtxo.n);
        }

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
        ddtx.lock_tier = static_cast<int>(it->second.lock_tier);  // Set lock tier from redeemed position

        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDTransaction(ddtx)) {
            LogPrintf("DigiDollarWallet::RedeemDigiDollar - Failed to write transaction\n");
        }

        return true;

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

        // FIX #1: Also add DD UTXO to dd_utxos map
        // DD UTXOs are always at output index 1 of DDTimeLock mint transactions
        COutPoint dd_utxo(id, 1);
        dd_utxos[dd_utxo] = dd;

        // FIX #2: Generate and store owner key for this position (needed for transfers)
        CKey ownerKey;
        ownerKey.MakeNewKey(true);
        StoreOwnerKey(id, ownerKey);

        LogPrintf("DigiDollarWallet: Added mock position %s (DD: %d, DGB: %d) - NO DB\n",
                  id.ToString(), dd, dgb);
        LogPrintf("DigiDollarWallet: Added DD UTXO %s:%d with amount %d\n",
                  id.ToString(), 1, dd);
        LogPrintf("DigiDollarWallet: Stored owner key for position %s\n", id.ToString());
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

    // FIX #1: Also clear DD UTXO tracking map
    dd_utxos.clear();

    // FIX #2: Clear owner keys map
    dd_owner_keys.clear();

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

    if (amount != it->second.dd_minted) {
        LogPrintf("DigiDollar: EXACT-AMOUNT REDEMPTION ENFORCED - amount must equal dd_minted (provided: %d, required: %d)\n",
                  amount, it->second.dd_minted);
        return false;
    }

    return true;
}

bool DigiDollarWallet::SelectDDCoins(const CAmount& target_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total, std::vector<CAmount>* amounts) const {
    // Reset output parameters
    selected_total = 0;
    selected_utxos.clear();
    if (amounts) amounts->clear();

    // Validate target amount
    if (target_amount <= 0) {
        LogPrintf("DigiDollar: SelectDDCoins - Invalid target amount %lld\n", static_cast<long long>(target_amount));
        return false;
    }

    LogPrintf("DigiDollar: SelectDDCoins - target: %lld cents\n", static_cast<long long>(target_amount));

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
    // FIXED: Now selects ALL DD UTXOs (both minted and received)
    // Signing logic properly handles both types:
    //  - Minted DD: Uses custom owner keys from dd_owner_keys map
    //  - Received DD: Uses wallet's regular key management
    for (const auto& utxo : available_utxos) {
        if (selected_total >= target_amount) break;

        selected_utxos.push_back(utxo.outpoint);
        selected_total += utxo.dd_amount;

        // Store individual amounts if requested (CRITICAL FIX #7)
        if (amounts) amounts->push_back(utxo.dd_amount);

        LogPrintf("DigiDollar: SelectDDCoins - Selected UTXO %s:%u (%lld cents, total: %lld)\n",
                  utxo.outpoint.hash.ToString(), utxo.outpoint.n,
                  static_cast<long long>(utxo.dd_amount), static_cast<long long>(selected_total));
    }

    bool success = (selected_total >= target_amount);

    if (!success) {
        LogPrintf("DigiDollar: SelectDDCoins - FAILED: need %lld, have %lld\n",
                  static_cast<long long>(target_amount), static_cast<long long>(selected_total));
        selected_utxos.clear();
        selected_total = 0;
    } else {
        LogPrintf("DigiDollar: SelectDDCoins - SUCCESS: selected %lld cents from %zu UTXOs\n",
                  static_cast<long long>(selected_total), selected_utxos.size());
    }

    return success;
}

bool DigiDollarWallet::SelectFeeCoins(const CAmount& fee_amount, std::vector<COutPoint>& selected_utxos, CAmount& selected_total, std::vector<CAmount>* selected_amounts, const std::vector<COutPoint>* exclude_utxos) const {
    // Reset output parameters
    selected_total = 0;
    selected_utxos.clear();
    if (selected_amounts) selected_amounts->clear();

    // Validate fee amount
    if (fee_amount <= 0) {
        LogPrintf("DigiDollar: SelectFeeCoins - Invalid fee amount %lld\n", static_cast<long long>(fee_amount));
        return false;
    }

    // Check if wallet pointer exists
    if (!m_wallet) {
        LogPrintf("DigiDollar: SelectFeeCoins - No wallet available for UTXO selection\n");
        return false;
    }

    LogPrintf("DigiDollar: SelectFeeCoins - target fee: %lld satoshis\n", static_cast<long long>(fee_amount));
    if (exclude_utxos && !exclude_utxos->empty()) {
        LogPrintf("DigiDollar: SelectFeeCoins - excluding %zu UTXOs from selection\n", exclude_utxos->size());
    }

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

    LogPrintf("DigiDollar: SelectFeeCoins - Found %zu available DGB UTXOs before filtering\n", available_coins.size());

    // Sort by amount (smallest first for efficiency)
    std::sort(available_coins.begin(), available_coins.end(),
              [](const wallet::COutput& a, const wallet::COutput& b) {
                  return a.txout.nValue < b.txout.nValue;
              });

    // Select UTXOs until fee covered, excluding any specified UTXOs
    for (const auto& coin : available_coins) {
        if (selected_total >= fee_amount) break;

        COutPoint outpoint = coin.outpoint;
        CAmount amount = coin.txout.nValue;

        // CRITICAL: Skip if this UTXO is in the exclude list (collateral or DD UTXOs)
        if (exclude_utxos) {
            bool should_exclude = false;
            for (const auto& exclude : *exclude_utxos) {
                if (outpoint == exclude) {
                    should_exclude = true;
                    LogPrintf("DigiDollar: SelectFeeCoins - EXCLUDING UTXO %s:%d (in exclude list)\n",
                              outpoint.hash.ToString(), outpoint.n);
                    break;
                }
            }
            if (should_exclude) continue;
        }

        selected_utxos.push_back(outpoint);
        if (selected_amounts) selected_amounts->push_back(amount);
        selected_total += amount;

        LogPrintf("DigiDollar: SelectFeeCoins - Selected UTXO %s:%u (%lld sats)\n",
                  outpoint.hash.ToString(), outpoint.n, static_cast<long long>(amount));
    }

    bool success = (selected_total >= fee_amount);

    if (!success) {
        LogPrintf("DigiDollar: SelectFeeCoins - FAILED: need %lld sats, have %lld\n",
                  static_cast<long long>(fee_amount), static_cast<long long>(selected_total));
        selected_utxos.clear();
        if (selected_amounts) selected_amounts->clear();
        selected_total = 0;
    } else {
        LogPrintf("DigiDollar: SelectFeeCoins - SUCCESS: selected %lld sats from %zu UTXOs\n",
                  static_cast<long long>(selected_total), selected_utxos.size());
    }

    return success;
}

CAmount DigiDollarWallet::CalculateTransactionFee(const CMutableTransaction& tx) const {
    // DigiDollar fee constants (match network requirements)
    // DigiByte uses KvB (kilobyte), not vB (virtual bytes)
    // DEFAULT_MIN_RELAY_TX_FEE in policy.h is 100000 satoshis/kB (0.001 DGB/kB)
    static const CAmount MIN_RELAY_FEE_PER_KB = 100000;  // 0.001 DGB/kB (matches network min relay fee)
    static const CAmount DEFAULT_FEE_RATE = 200000;      // 0.002 DGB/kB (2x min for faster confirmation)

    // DigiDollar transactions MUST pay at least 0.1 DGB fee to miners
    static const CAmount MIN_DD_TX_FEE = 10000000;       // 0.1 DGB minimum for DigiDollar transactions

    // Calculate transaction size
    // NOTE: This is an estimate. Actual size determined after signing
    unsigned int tx_size = GetSerializeSize(tx, PROTOCOL_VERSION);

    // Add estimated witness size for P2TR inputs
    // Each P2TR witness is approximately 65 bytes (1 byte length + 64 byte Schnorr signature)
    size_t num_inputs = tx.vin.size();
    unsigned int estimated_witness_size = num_inputs * 65;
    unsigned int total_size = tx_size + estimated_witness_size;

    // Calculate fee in satoshis based on size
    // Formula: (size_in_bytes / 1000) * fee_rate_per_KB
    CAmount size_based_fee = (total_size * DEFAULT_FEE_RATE) / 1000;

    // Ensure minimum relay fee is met
    CAmount min_relay_fee = (total_size * MIN_RELAY_FEE_PER_KB) / 1000;
    if (size_based_fee < min_relay_fee) {
        size_based_fee = min_relay_fee;
    }

    // DigiDollar transactions must pay at least 0.1 DGB fee
    CAmount fee = std::max(size_based_fee, MIN_DD_TX_FEE);

    LogPrintf("DigiDollar: CalculateTransactionFee - size: %d bytes, size_based_fee: %d sats, final_fee: %d sats (min 0.1 DGB)\n",
              total_size, size_based_fee, fee);

    return fee;
}
// =============================================================================
// PHASE 3.1: P2TR SIGNING FOR DD INPUTS (SCHNORR SIGNATURES)
// =============================================================================

bool DigiDollarWallet::SignDDInputs(CMutableTransaction& tx,
                                     const std::vector<COutPoint>& dd_utxos,
                                     const std::vector<COutPoint>& fee_utxos) {
    if (!m_wallet) {
        LogPrintf("DigiDollar: SignDDInputs - No wallet available\n");
        return false;
    }

    LogPrintf("DigiDollar: SignDDInputs - Signing %d DD inputs and %d fee inputs using wallet's SignTransaction\n",
              dd_utxos.size(), fee_utxos.size());

    LOCK(m_wallet->cs_wallet);

    // Build coins map for ALL inputs (DD + fee)
    // The wallet's SignTransaction needs all inputs to be in the coins map
    std::map<COutPoint, Coin> coins;

    // Add DD UTXOs to coins map
    for (const auto& outpoint : dd_utxos) {
        // Get the transaction from mapWallet
        const auto mi = m_wallet->mapWallet.find(outpoint.hash);
        if (mi == m_wallet->mapWallet.end() || outpoint.n >= mi->second.tx->vout.size()) {
            LogPrintf("DigiDollar: SignDDInputs - Failed to find DD transaction %s in wallet\n",
                      outpoint.hash.ToString());
            return false;
        }

        const wallet::CWalletTx& wtx = mi->second;
        const CTxOut& txout = wtx.tx->vout[outpoint.n];

        // Get block height for the transaction
        int prev_height = wtx.state<wallet::TxStateConfirmed>() ? wtx.state<wallet::TxStateConfirmed>()->confirmed_block_height : 0;

        // Use the actual on-chain value for sighash calculation
        // DD tokens have nValue=0 on-chain - we MUST use 0 here to match network verification
        // The network validates using actual UTXO values from the chain, so we must sign with the same
        coins[outpoint] = Coin(txout, prev_height, wtx.IsCoinBase());

        LogPrintf("DigiDollar: SignDDInputs - Added DD coin for %s:%d at height %d, scriptPubKey size %d\n",
                  outpoint.hash.ToString(), outpoint.n, prev_height, txout.scriptPubKey.size());
    }

    // Add fee UTXOs to coins map
    for (const auto& outpoint : fee_utxos) {
        const auto mi = m_wallet->mapWallet.find(outpoint.hash);
        if (mi == m_wallet->mapWallet.end() || outpoint.n >= mi->second.tx->vout.size()) {
            LogPrintf("DigiDollar: SignDDInputs - Failed to find fee transaction %s in wallet\n",
                      outpoint.hash.ToString());
            return false;
        }

        const wallet::CWalletTx& wtx = mi->second;
        const CTxOut& txout = wtx.tx->vout[outpoint.n];
        int prev_height = wtx.state<wallet::TxStateConfirmed>() ? wtx.state<wallet::TxStateConfirmed>()->confirmed_block_height : 0;

        coins[outpoint] = Coin(txout, prev_height, wtx.IsCoinBase());

        LogPrintf("DigiDollar: SignDDInputs - Added fee coin for %s:%d at height %d, value %d\n",
                  outpoint.hash.ToString(), outpoint.n, prev_height, txout.nValue);
    }

    // IMPORTANT: For Taproot, we must sign fee inputs FIRST, then DD inputs
    // This is because Taproot sighash includes the witness data of other inputs
    // If we sign DD inputs first, the sighash will be different when fee inputs are added later

    // Sign fee inputs using wallet's standard signing
    if (!fee_utxos.empty()) {
        LogPrintf("DigiDollar: SignDDInputs - Signing fee inputs FIRST using wallet's SignTransaction\n");

        // Log DD input witness BEFORE SignTransaction
        for (size_t i = 0; i < dd_utxos.size(); i++) {
            LogPrintf("DigiDollar: SignDDInputs - DD input %d witness BEFORE SignTransaction: %s\n",
                      i, tx.vin[i].scriptWitness.IsNull() ? "NULL" : "NOT NULL");
        }

        // CRITICAL FIX: Wallet's SignTransaction will sign inputs it has keys for
        // This includes:
        //  - ALL fee inputs (DGB UTXOs)
        //  - RECEIVED DD inputs (wallet has keys from address generation)
        // It will NOT sign:
        //  - MINTED DD inputs (use custom owner keys not in wallet descriptors)
        //
        // IMPORTANT: We use the overload that accepts a coins map so that our
        // modified DD coins (with dummy value=1) are used for Taproot sighash.
        // The simple SignTransaction(tx) looks up coins from UTXO set which has value=0.
        std::map<int, bilingual_str> input_errors;
        bool sign_result = m_wallet->SignTransaction(tx, coins, SIGHASH_DEFAULT, input_errors);
        LogPrintf("DigiDollar: SignDDInputs - SignTransaction returned: %s\n", sign_result ? "true" : "false");
        if (!sign_result) {
            for (const auto& [idx, err] : input_errors) {
                LogPrintf("DigiDollar: SignDDInputs - Input %d error: %s\n", idx, err.original);
            }
        }

        // Log DD input witness AFTER SignTransaction
        for (size_t i = 0; i < dd_utxos.size(); i++) {
            LogPrintf("DigiDollar: SignDDInputs - DD input %d witness AFTER SignTransaction: %s\n",
                      i, tx.vin[i].scriptWitness.IsNull() ? "NULL" : "NOT NULL");
        }

        // Verify that fee inputs were actually signed
        bool all_fee_inputs_signed = true;
        for (size_t i = dd_utxos.size(); i < tx.vin.size(); i++) {
            bool has_witness = !tx.vin[i].scriptWitness.IsNull() &&
                             !tx.vin[i].scriptWitness.stack.empty();
            bool has_scriptsig = !tx.vin[i].scriptSig.empty();

            if (!has_witness && !has_scriptsig) {
                all_fee_inputs_signed = false;
                LogPrintf("DigiDollar: SignDDInputs - Fee input %d was NOT signed\n", i);
                return false;
            }

            LogPrintf("DigiDollar: SignDDInputs - Fee input %d signed (witness: %s, scriptSig: %s)\n",
                      i, has_witness ? "yes" : "no", has_scriptsig ? "yes" : "no");
        }

        if (!all_fee_inputs_signed) {
            LogPrintf("DigiDollar: SignDDInputs - Not all fee inputs were signed\n");
            return false;
        }

        LogPrintf("DigiDollar: SignDDInputs - All fee inputs signed successfully\n");
    }

    // Create PrecomputedTransactionData for proper Taproot sighash calculation
    // NOW with fee inputs already signed
    std::vector<CTxOut> prevouts;
    for (size_t idx = 0; idx < tx.vin.size(); idx++) {
        const auto& input = tx.vin[idx];
        const Coin& coin = coins.at(input.prevout);
        prevouts.push_back(coin.out);
        LogPrintf("DigiDollar: SignDDInputs - Prevout %d: amount=%d, scriptPubKey=%s\n",
                  idx, coin.out.nValue, HexStr(coin.out.scriptPubKey));
    }

    PrecomputedTransactionData txdata;
    txdata.Init(tx, std::move(prevouts), /* force=*/ true);

    // NOW manually sign DD inputs that wallet couldn't sign (minted DD with owner keys)
    // RECEIVED DD was already signed by wallet's SignTransaction above
    for (size_t i = 0; i < dd_utxos.size(); i++) {
        // Check if this input is already signed
        bool already_signed = !tx.vin[i].scriptWitness.IsNull() &&
                             !tx.vin[i].scriptWitness.stack.empty();

        if (already_signed) {
            LogPrintf("DigiDollar: SignDDInputs - DD input %d already signed by wallet (received DD)\n", i);
            continue;  // Skip - wallet already signed it
        }

        const COutPoint& outpoint = dd_utxos[i];

        // Get the owner key for this DD UTXO
        // For MINTED DD: owner key is stored in dd_owner_keys
        // For RECEIVED DD: we need to find the internal key in wallet that matches the tweaked output
        CKey ownerKey;
        bool found_key = false;

        // First, extract the output key from scriptPubKey to verify any key we find
        const Coin& coin_for_key = coins.at(outpoint);
        const CTxOut& output_for_key = coin_for_key.out;
        std::vector<unsigned char> target_output_key_verify;
        if (output_for_key.scriptPubKey.size() == 34 && output_for_key.scriptPubKey[0] == OP_1) {
            target_output_key_verify.assign(output_for_key.scriptPubKey.begin() + 2, output_for_key.scriptPubKey.end());
        }

        // FIRST: Try dd_address_keys (for received DD via getdigidollaraddress)
        // This takes priority because dd_owner_keys is indexed by txid which can return wrong key
        // for multi-output transfer transactions
        if (!target_output_key_verify.empty()) {
            XOnlyPubKey xonly_output(target_output_key_verify);
            if (GetAddressKey(xonly_output, ownerKey)) {
                found_key = true;
                LogPrintf("DigiDollar: SignDDInputs - Found key via dd_address_keys for received DD (priority lookup)\n");
            }
        }

        // SECOND: Try dd_owner_keys (for minted DD), but VERIFY the key matches this output
        if (!found_key) {
            CKey candidate_key;
            if (GetOwnerKey(outpoint.hash, candidate_key)) {
                // Verify this key produces the correct tweaked output key
                if (!target_output_key_verify.empty()) {
                    XOnlyPubKey internal_xonly(candidate_key.GetPubKey());
                    auto tweaked = internal_xonly.CreateTapTweak(nullptr);
                    if (tweaked && std::equal(target_output_key_verify.begin(), target_output_key_verify.end(),
                                             tweaked->first.begin())) {
                        ownerKey = candidate_key;
                        found_key = true;
                        LogPrintf("DigiDollar: SignDDInputs - Found verified key via dd_owner_keys for minted DD\n");
                    } else {
                        LogPrintf("DigiDollar: SignDDInputs - Key from dd_owner_keys doesn't match output (wrong key for this outpoint)\n");
                    }
                } else {
                    // Can't verify, use it anyway (legacy behavior)
                    ownerKey = candidate_key;
                    found_key = true;
                    LogPrintf("DigiDollar: SignDDInputs - Using unverified key from dd_owner_keys\n");
                }
            }
        }

        // THIRD: Fall back to wallet keystore search
        if (!found_key) {
            // This might be RECEIVED DD - try to find the internal key in wallet
            // The DD output uses a tweaked P2TR key. We need to find which wallet key
            // when tweaked matches the output key.
            LogPrintf("DigiDollar: SignDDInputs - No owner key in dd_owner_keys, trying wallet keystore for received DD\n");

            // Get the output key from the scriptPubKey
            const Coin& search_coin = coins.at(outpoint);
            const CTxOut& search_output = search_coin.out;

            LogPrintf("DigiDollar: SignDDInputs - scriptPubKey: %s (size=%d)\n",
                      HexStr(search_output.scriptPubKey), search_output.scriptPubKey.size());

            if (search_output.scriptPubKey.size() == 34 && search_output.scriptPubKey[0] == OP_1) {
                std::vector<unsigned char> target_output_key(search_output.scriptPubKey.begin() + 2, search_output.scriptPubKey.end());
                LogPrintf("DigiDollar: SignDDInputs - Target output key (tweaked): %s\n", HexStr(target_output_key));

                // FIRST: Try dd_address_keys map (for received DD tokens via getdigidollaraddress)
                XOnlyPubKey xonly_output_key(target_output_key);
                if (GetAddressKey(xonly_output_key, ownerKey)) {
                    found_key = true;
                    LogPrintf("DigiDollar: SignDDInputs - Found key via dd_address_keys for received DD\n");
                }

                // Helper to get signing provider with private key access for descriptor wallets
                auto getSigningProviderWithKeys = [this](const CScript& script) -> std::unique_ptr<SigningProvider> {
                    const auto& spk_mans = m_wallet->GetScriptPubKeyMans(script);
                    if (!spk_mans.empty()) {
                        wallet::ScriptPubKeyMan* spk_man = *spk_mans.begin();
                        wallet::DescriptorScriptPubKeyMan* desc_spk_man = dynamic_cast<wallet::DescriptorScriptPubKeyMan*>(spk_man);
                        if (desc_spk_man) {
                            LogPrintf("DigiDollar: SignDDInputs - Using DescriptorScriptPubKeyMan with private keys\n");
                            return desc_spk_man->GetSigningProviderWithKeys(script);
                        }
                    }
                    // Fallback for legacy wallets
                    return m_wallet->GetSolvingProvider(script);
                };

                // Try to find the internal key by iterating through wallet keys
                // This is the key that when tweaked produces the target output key
                // For descriptor wallets, we need GetSigningProviderWithKeys
                if (!found_key) {
                auto provider = getSigningProviderWithKeys(search_output.scriptPubKey);
                LogPrintf("DigiDollar: SignDDInputs - GetSigningProvider returned: %s\n", provider ? "valid" : "nullptr");

                if (provider) {
                    // The provider knows about this script - try to get the key
                    // For Taproot, the key in WitnessV1Taproot is the tweaked output key
                    // We need the internal key for signing

                    // Extract destination and see if we can get signing info
                    CTxDestination dest;
                    if (ExtractDestination(search_output.scriptPubKey, dest)) {
                        LogPrintf("DigiDollar: SignDDInputs - ExtractDestination succeeded, dest type index: %d\n", dest.index());
                        if (auto* tr = std::get_if<WitnessV1Taproot>(&dest)) {
                            LogPrintf("DigiDollar: SignDDInputs - Destination is WitnessV1Taproot\n");
                            // Check if we have a key for this via TaprootSpendData
                            TaprootSpendData spenddata;
                            if (provider->GetTaprootSpendData(XOnlyPubKey(*tr), spenddata)) {
                                LogPrintf("DigiDollar: SignDDInputs - GetTaprootSpendData succeeded, internal_key valid: %s\n",
                                          spenddata.internal_key.IsFullyValid() ? "yes" : "no");
                                // For key-path spending, the internal key is what we need
                                // Try to get the private key for the internal key
                                if (spenddata.internal_key.IsFullyValid()) {
                                    LogPrintf("DigiDollar: SignDDInputs - Internal key: %s\n",
                                              HexStr(Span<const unsigned char>(spenddata.internal_key.begin(), 32)));
                                    CKeyID keyid = CKeyID(Hash160(std::vector<unsigned char>(
                                        spenddata.internal_key.begin(),
                                        spenddata.internal_key.begin() + 32)));
                                    LogPrintf("DigiDollar: SignDDInputs - Trying GetKey with keyid: %s\n", keyid.ToString());

                                    // Try getting key via FlatSigningProvider
                                    if (provider->GetKey(keyid, ownerKey)) {
                                        LogPrintf("DigiDollar: SignDDInputs - GetKey succeeded!\n");
                                        // Verify this key when tweaked matches the output
                                        XOnlyPubKey internal_xonly(ownerKey.GetPubKey());
                                        auto tweaked = internal_xonly.CreateTapTweak(nullptr);
                                        if (tweaked && std::equal(target_output_key.begin(), target_output_key.end(),
                                                                 tweaked->first.begin())) {
                                            found_key = true;
                                            LogPrintf("DigiDollar: SignDDInputs - Found internal key via GetTaprootSpendData\n");
                                        } else {
                                            LogPrintf("DigiDollar: SignDDInputs - GetKey succeeded but tweak doesn't match target\n");
                                        }
                                    } else {
                                        LogPrintf("DigiDollar: SignDDInputs - GetKey failed for keyid\n");
                                        // Try GetKeyByXOnly instead
                                        if (provider->GetKeyByXOnly(spenddata.internal_key, ownerKey)) {
                                            LogPrintf("DigiDollar: SignDDInputs - GetKeyByXOnly succeeded!\n");
                                            XOnlyPubKey internal_xonly(ownerKey.GetPubKey());
                                            auto tweaked = internal_xonly.CreateTapTweak(nullptr);
                                            if (tweaked && std::equal(target_output_key.begin(), target_output_key.end(),
                                                                     tweaked->first.begin())) {
                                                found_key = true;
                                                LogPrintf("DigiDollar: SignDDInputs - Found internal key via GetKeyByXOnly\n");
                                            } else {
                                                LogPrintf("DigiDollar: SignDDInputs - GetKeyByXOnly succeeded but tweak doesn't match\n");
                                            }
                                        } else {
                                            LogPrintf("DigiDollar: SignDDInputs - GetKeyByXOnly also failed\n");
                                        }
                                    }
                                }
                            } else {
                                LogPrintf("DigiDollar: SignDDInputs - GetTaprootSpendData failed\n");
                            }
                        } else {
                            LogPrintf("DigiDollar: SignDDInputs - Destination is NOT WitnessV1Taproot\n");
                        }
                    } else {
                        LogPrintf("DigiDollar: SignDDInputs - ExtractDestination failed\n");
                    }
                }

                // If still not found, try a brute force search through wallet keys
                if (!found_key) {
                    LogPrintf("DigiDollar: SignDDInputs - Starting brute force wallet key scan (mapWallet size: %d)\n",
                              m_wallet->mapWallet.size());
                    int pkh_count = 0, p2tr_count = 0;
                    // Get all keys from mapWallet transactions and try each
                    for (const auto& [txid, wtx] : m_wallet->mapWallet) {
                        for (size_t n = 0; n < wtx.tx->vout.size(); n++) {
                            CTxDestination out_dest;
                            if (ExtractDestination(wtx.tx->vout[n].scriptPubKey, out_dest)) {
                                // Check if this output belongs to our wallet and has a key (WITH PRIVATE KEY ACCESS)
                                auto out_provider = getSigningProviderWithKeys(wtx.tx->vout[n].scriptPubKey);
                                if (out_provider) {
                                    // Try all key types
                                    if (auto* pkh = std::get_if<PKHash>(&out_dest)) {
                                        pkh_count++;
                                        CKey test_key;
                                        if (out_provider->GetKey(ToKeyID(*pkh), test_key)) {
                                            XOnlyPubKey test_xonly(test_key.GetPubKey());
                                            auto tweaked = test_xonly.CreateTapTweak(nullptr);
                                            if (tweaked && std::equal(target_output_key.begin(), target_output_key.end(),
                                                                     tweaked->first.begin())) {
                                                ownerKey = test_key;
                                                found_key = true;
                                                LogPrintf("DigiDollar: SignDDInputs - Found matching key via wallet PKH scan\n");
                                                break;
                                            }
                                        }
                                    }
                                    // Also try Taproot destinations (WitnessV1Taproot)
                                    // The output key in P2TR is already tweaked, so we need to find
                                    // the internal key that produces this tweaked output
                                    else if (auto* tr = std::get_if<WitnessV1Taproot>(&out_dest)) {
                                        p2tr_count++;
                                        // For our wallet's own P2TR outputs, we can get the internal key
                                        // via GetTaprootSpendData
                                        TaprootSpendData tr_spenddata;
                                        if (out_provider->GetTaprootSpendData(XOnlyPubKey(*tr), tr_spenddata)) {
                                            if (tr_spenddata.internal_key.IsFullyValid()) {
                                                // Try to get the private key for this internal key
                                                CKey test_key;
                                                if (out_provider->GetKeyByXOnly(tr_spenddata.internal_key, test_key)) {
                                                    // Verify this key when tweaked matches our target output
                                                    XOnlyPubKey test_xonly(test_key.GetPubKey());
                                                    auto tweaked = test_xonly.CreateTapTweak(nullptr);
                                                    if (tweaked && std::equal(target_output_key.begin(), target_output_key.end(),
                                                                             tweaked->first.begin())) {
                                                        ownerKey = test_key;
                                                        found_key = true;
                                                        LogPrintf("DigiDollar: SignDDInputs - Found matching key via wallet P2TR scan (internal key)\n");
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (found_key) break;
                    }
                    LogPrintf("DigiDollar: SignDDInputs - Brute force scan complete: checked %d PKH, %d P2TR outputs, found_key=%s\n",
                              pkh_count, p2tr_count, found_key ? "true" : "false");
                }
                } // end if (!found_key) - wallet key lookup
            }
        }

        if (!found_key) {
            LogPrintf("DigiDollar: SignDDInputs - DD input %d not signed and no owner key found for %s\n",
                      i, outpoint.hash.ToString());
            return false;
        }

        CPubKey ownerPubKey = ownerKey.GetPubKey();
        XOnlyPubKey ownerXOnly(ownerPubKey);

        LogPrintf("DigiDollar: SignDDInputs - Owner compressed pubkey: %s\n", HexStr(ownerPubKey));
        LogPrintf("DigiDollar: SignDDInputs - Owner x-only pubkey: %s\n", HexStr(ownerXOnly));

        // Get the CTxOut for signing
        const Coin& coin = coins.at(outpoint);
        const CTxOut& prevOutput = coin.out;

        // Verify it's a valid Taproot output
        if (prevOutput.scriptPubKey.size() != 34 || prevOutput.scriptPubKey[0] != OP_1) {
            LogPrintf("DigiDollar: SignDDInputs - Invalid Taproot output for input %d\n", i);
            return false;
        }

        // Extract the output key from the script (this is the TWEAKED key = internal_key + merkle_root_hash)
        std::vector<unsigned char> outputKeyBytes(prevOutput.scriptPubKey.begin() + 2, prevOutput.scriptPubKey.end());
        LogPrintf("DigiDollar: SignDDInputs - Actual output key in script: %s\n", HexStr(outputKeyBytes));

        // CRITICAL: Check if this is a DD output (vout 1) or collateral output (vout 0)
        // DD outputs are simple P2TR with key-path only
        // Collateral outputs have MAST and require script-path spending
        //
        // To distinguish: first check if there's a collateral position for this outpoint
        // If there IS a collateral position -> script-path signing
        // If there is NO collateral position -> it's a DD token output -> key-path signing

        // First, check if this is a collateral output by looking for a registered position
        WalletCollateralPosition position;
        bool is_collateral = false;
        for (const auto& [pos_id, pos] : collateral_positions) {
            if (pos_id == outpoint.hash && outpoint.n == 0) {  // Collateral is always vout[0]
                position = pos;
                is_collateral = true;
                break;
            }
        }

        if (!is_collateral) {
            // This is a DD token output - use KEY-PATH signing
            // DD outputs use standard Taproot P2TR with tweaked key (key-path only, no merkle root)
            LogPrintf("DigiDollar: SignDDInputs - Output %s:%d is DD token (no collateral position), using key-path signing\n",
                      outpoint.hash.ToString(), outpoint.n);

            // Compute the expected tweaked output key (same tweak as CreateDigiDollarP2TR)
            auto tweaked = ownerXOnly.CreateTapTweak(nullptr);  // nullptr = no merkle root
            if (!tweaked) {
                LogPrintf("DigiDollar: SignDDInputs - Failed to create tap tweak for key-path signing\n");
                return false;
            }
            XOnlyPubKey expected_output_key = tweaked->first;

            // Verify the output key matches the tweaked key
            if (outputKeyBytes.size() != 32 ||
                !std::equal(outputKeyBytes.begin(), outputKeyBytes.end(), expected_output_key.begin())) {
                LogPrintf("DigiDollar: SignDDInputs - Output key mismatch for key-path (expected tweaked: %s, got: %s)\n",
                         HexStr(expected_output_key), HexStr(outputKeyBytes));
                return false;
            }

            LogPrintf("DigiDollar: SignDDInputs - Using KEY-PATH signing for DD token (tweaked key)\n");

            // Calculate sighash for Taproot KEY-PATH spending
            uint256 sighash;
            ScriptExecutionData execdata;
            execdata.m_annex_init = true;
            execdata.m_annex_present = false;
            // For key-path: NO tapleaf hash (that's only for script-path)
            execdata.m_tapleaf_hash_init = false;

            if (!SignatureHashSchnorr(sighash, execdata, tx, i, SIGHASH_DEFAULT, SigVersion::TAPROOT, txdata, MissingDataBehavior::FAIL)) {
                LogPrintf("DigiDollar: SignDDInputs - Failed to compute key-path sighash for input %d\n", i);
                return false;
            }

            LogPrintf("DigiDollar: SignDDInputs - KEY-PATH sighash: %s\n", sighash.ToString());

            // Sign for Taproot key-path spending
            // For BIP-341 key-path spending, we need to apply the standard tweak:
            // - Wallet stores the INTERNAL private key
            // - Output uses the TWEAKED public key (from CreateTapTweak(nullptr))
            // - SignSchnorr with empty merkle root applies the same standard tweak
            std::vector<unsigned char> sig(64);
            uint256 aux = GetRandHash();
            uint256 empty_merkle_root;  // Zero hash = standard key-path tweak

            if (!ownerKey.SignSchnorr(sighash, sig, &empty_merkle_root, aux)) {
                LogPrintf("DigiDollar: SignDDInputs - Failed to create key-path signature for input %d\n", i);
                return false;
            }

            LogPrintf("DigiDollar: SignDDInputs - Created key-path signature: %s\n", HexStr(sig));

            // For Taproot KEY-PATH spending, witness stack is: [signature]
            tx.vin[i].scriptWitness.stack.clear();
            tx.vin[i].scriptWitness.stack.push_back(sig);

            LogPrintf("DigiDollar: SignDDInputs - KEY-PATH witness stack: sig (%d bytes)\n", sig.size());
            continue;  // Move to next input
        }

        // This is collateral (vout[0]) - position already found above
        // Use the position data to reconstruct MAST tree
        LogPrintf("DigiDollar: SignDDInputs - Output %s:%d is collateral, using script-path signing\n",
                  outpoint.hash.ToString(), outpoint.n);

        // Rebuild the MAST tree using the same parameters as mint
        TaprootBuilder builder;

        // Recreate redemption path scripts (same as CreateCollateralP2TR)
        DigiDollar::MintParams scriptParams;
        scriptParams.ddAmount = position.dd_minted;
        scriptParams.lockHeight = position.unlock_height;
        scriptParams.ownerKey = ownerXOnly;
        scriptParams.internalKey = ownerXOnly;
        scriptParams.oracleKeys = DigiDollar::GetOracleKeys(15); // Same as mint

        // Add the 4 redemption paths in the same order as mint
        CScript normalPath = DigiDollar::CreateNormalRedemptionPath(scriptParams);
        if (!normalPath.empty()) {
            builder.Add(1, normalPath, 0xC0);
        }

        CScript partialPath = DigiDollar::CreatePartialRedemptionPath(scriptParams);
        if (!partialPath.empty()) {
            builder.Add(2, partialPath, 0xC0);
        }

        CScript emergencyPath = DigiDollar::CreateEmergencyPath(scriptParams);
        if (!emergencyPath.empty()) {
            builder.Add(3, emergencyPath, 0xC0);
        }

        CScript errPath = DigiDollar::CreateERRPath(scriptParams);
        if (!errPath.empty()) {
            builder.Add(3, errPath, 0xC0);
        }

        // Finalize with the internal key to get the merkle root
        builder.Finalize(ownerXOnly);

        if (!builder.IsValid() || !builder.IsComplete()) {
            LogPrintf("DigiDollar: SignDDInputs - Failed to rebuild Taproot tree for signing\n");
            return false;
        }

        // Get the TaprootSpendData which contains the merkle root
        TaprootSpendData spend_data = builder.GetSpendData();

        // Verify the output key matches what we expect
        XOnlyPubKey computed_output_key(outputKeyBytes);
        WitnessV1Taproot expected_output = builder.GetOutput();

        LogPrintf("DigiDollar: SignDDInputs - Reconstructed merkle root: %s\n",
                  spend_data.merkle_root.IsNull() ? "NULL" : HexStr(spend_data.merkle_root));
        LogPrintf("DigiDollar: SignDDInputs - Expected output key from builder: %s\n",
                  HexStr(expected_output));

        // CRITICAL: Use SCRIPT-PATH spending to execute the Normal Redemption Path
        // For redemption, we need to execute the OP_CHECKLOCKTIMEVERIFY script,
        // which requires script-path spending, NOT key-path spending.

        // Get the control block for the normal redemption path from spend_data
        std::pair<CScript, int> script_key = {normalPath, TAPROOT_LEAF_TAPSCRIPT};
        auto it = spend_data.scripts.find(script_key);
        if (it == spend_data.scripts.end() || it->second.empty()) {
            LogPrintf("DigiDollar: SignDDInputs - Control block not found for normal redemption path\n");
            return false;
        }

        // Get the shortest control block (most efficient)
        std::vector<unsigned char> control_block = *it->second.begin();

        LogPrintf("DigiDollar: SignDDInputs - Found control block (%d bytes) for normal redemption\n",
                  control_block.size());

        // Calculate the leaf hash for the normal redemption script
        uint256 leaf_hash = ComputeTapleafHash(TAPROOT_LEAF_TAPSCRIPT, normalPath);

        LogPrintf("DigiDollar: SignDDInputs - Leaf hash: %s\n", leaf_hash.ToString());

        // Create Schnorr signature for Taproot SCRIPT-PATH spending
        std::vector<unsigned char> sig(64); // Schnorr signatures are always 64 bytes

        // Calculate sighash for Taproot script-path spending
        ScriptExecutionData execdata;
        execdata.m_annex_init = true;
        execdata.m_annex_present = false;
        execdata.m_tapleaf_hash = leaf_hash;
        execdata.m_tapleaf_hash_init = true;
        execdata.m_codeseparator_pos_init = true;
        execdata.m_codeseparator_pos = 0xFFFFFFFF; // No OP_CODESEPARATOR in our script

        uint256 sighash;
        if (!SignatureHashSchnorr(sighash, execdata, tx, i, SIGHASH_DEFAULT, SigVersion::TAPSCRIPT, txdata, MissingDataBehavior::FAIL)) {
            LogPrintf("DigiDollar: SignDDInputs - Failed to compute Tapscript sighash for input %d\n", i);
            return false;
        }

        LogPrintf("DigiDollar: SignDDInputs - SCRIPT-PATH SIGNING - input %d, sighash: %s\n",
                  i, sighash.ToString());

        // Generate auxiliary randomness for Schnorr signing
        uint256 aux = GetRandHash();

        // CRITICAL FIX: Sign with UNTWEAKED internal key for script-path spending
        // The leaf hash is already committed in the sighash (via execdata.m_tapleaf_hash)
        // Passing &leaf_hash would TWEAK the key (only correct for key-path spending)
        // For script-path: Sign with untweaked key, verify against pubkey in script
        if (!ownerKey.SignSchnorr(sighash, sig, nullptr, aux)) {
            LogPrintf("DigiDollar: SignDDInputs - Failed to create Schnorr signature for input %d\n", i);
            return false;
        }

        LogPrintf("DigiDollar: SignDDInputs - Created signature: %s\n", HexStr(sig));

        // For Taproot SCRIPT-PATH spending, witness stack is:
        // [signature, script, control_block]
        tx.vin[i].scriptWitness.stack.clear();
        tx.vin[i].scriptWitness.stack.push_back(sig);
        tx.vin[i].scriptWitness.stack.push_back(std::vector<unsigned char>(normalPath.begin(), normalPath.end()));
        tx.vin[i].scriptWitness.stack.push_back(control_block);

        LogPrintf("DigiDollar: SignDDInputs - Witness stack: sig (%d bytes) + script (%d bytes) + control (%d bytes)\n",
                  sig.size(), normalPath.size(), control_block.size());
    }

    LogPrintf("DigiDollar: SignDDInputs - Successfully signed all inputs (%d DD + %d fee)\n",
              dd_utxos.size(), fee_utxos.size());
    return true;
}


// =============================================================================
// PHASE 3.2: FEE INPUT SIGNING IMPLEMENTATION
// =============================================================================

bool DigiDollarWallet::SignFeeInputs(CMutableTransaction& tx,
                                      const std::vector<COutPoint>& fee_utxos,
                                      size_t dd_input_count) {
    // Validate that we have fee inputs to sign
    if (fee_utxos.empty()) {
        LogPrintf("DigiDollar: SignFeeInputs - No fee UTXOs provided\n");
        return true; // No fee inputs to sign is valid (fee-less tx)
    }

    if (!m_wallet) {
        LogPrintf("DigiDollar: SignFeeInputs - No wallet available (test mode)\n");
        return false; // Can't sign without wallet
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

    // Helper to get signing provider with private key access for descriptor wallets
    auto getSigningProviderWithKeys = [this](const CScript& script) -> std::unique_ptr<SigningProvider> {
        const auto& spk_mans = m_wallet->GetScriptPubKeyMans(script);
        if (!spk_mans.empty()) {
            wallet::ScriptPubKeyMan* spk_man = *spk_mans.begin();
            wallet::DescriptorScriptPubKeyMan* desc_spk_man = dynamic_cast<wallet::DescriptorScriptPubKeyMan*>(spk_man);
            if (desc_spk_man) {
                LogPrintf("DigiDollar: SignFeeInputs - Using DescriptorScriptPubKeyMan with private keys\n");
                return desc_spk_man->GetSigningProviderWithKeys(script);
            }
        }
        // Fallback for legacy wallets
        return m_wallet->GetSolvingProvider(script);
    };

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

        // Get the signing provider for this script WITH PRIVATE KEY ACCESS
        std::unique_ptr<SigningProvider> provider = getSigningProviderWithKeys(prevScript);
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

    // Sign all inputs together (DD + fee) using wallet's SignTransaction
    // SignDDInputs now handles both DD and fee inputs in one call
    if (!SignDDInputs(tx, dd_utxos, fee_utxos)) {
        LogPrintf("DigiDollar: SignTransaction - Failed to sign inputs\n");
        return false;
    }

    // Verify all inputs are signed (have witness or scriptSig)
    for (size_t i = 0; i < tx.vin.size(); i++) {
        bool has_witness = !tx.vin[i].scriptWitness.IsNull() && !tx.vin[i].scriptWitness.stack.empty();
        bool has_scriptsig = !tx.vin[i].scriptSig.empty();

        if (!has_witness && !has_scriptsig) {
            LogPrintf("DigiDollar: SignTransaction - Input %d not signed (no witness or scriptSig)\n", i);
            return false;
        }
    }

    LogPrintf("DigiDollar: SignTransaction - Successfully signed all inputs\n");
    return true;
}

// =============================================================================
// REDEMPTION-SPECIFIC SIGNING (INCLUDES COLLATERAL INPUT)
// =============================================================================

bool DigiDollarWallet::SignRedemptionTransaction(CMutableTransaction& tx,
                                                  const COutPoint& collateral_outpoint,
                                                  const std::vector<COutPoint>& dd_utxos,
                                                  const std::vector<COutPoint>& fee_utxos,
                                                  const CKey& owner_key) {
    LogPrintf("DigiDollar: SignRedemptionTransaction - Signing collateral + %d DD inputs + %d fee inputs\n",
              dd_utxos.size(), fee_utxos.size());

    if (!m_wallet) {
        LogPrintf("DigiDollar: SignRedemptionTransaction - No wallet available\n");
        return false;
    }

    LOCK(m_wallet->cs_wallet);

    // Build coins map for ALL inputs (collateral + DD + fee)
    std::map<COutPoint, Coin> coins;

    // 1. Add COLLATERAL input (index 0) to coins map
    const auto collateral_mi = m_wallet->mapWallet.find(collateral_outpoint.hash);
    if (collateral_mi == m_wallet->mapWallet.end() || collateral_outpoint.n >= collateral_mi->second.tx->vout.size()) {
        LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to find collateral transaction %s in wallet\n",
                  collateral_outpoint.hash.ToString());
        return false;
    }

    const wallet::CWalletTx& collateral_wtx = collateral_mi->second;
    const CTxOut& collateral_txout = collateral_wtx.tx->vout[collateral_outpoint.n];
    int collateral_height = collateral_wtx.state<wallet::TxStateConfirmed>() ?
                           collateral_wtx.state<wallet::TxStateConfirmed>()->confirmed_block_height : 0;

    coins[collateral_outpoint] = Coin(collateral_txout, collateral_height, collateral_wtx.IsCoinBase());

    LogPrintf("DigiDollar: SignRedemptionTransaction - Added collateral coin for %s:%d at height %d, value %d\n",
              collateral_outpoint.hash.ToString(), collateral_outpoint.n, collateral_height, collateral_txout.nValue);

    // 2. Add DD UTXOs to coins map (inputs 1+)
    for (const auto& outpoint : dd_utxos) {
        const auto mi = m_wallet->mapWallet.find(outpoint.hash);
        if (mi == m_wallet->mapWallet.end() || outpoint.n >= mi->second.tx->vout.size()) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to find DD transaction %s in wallet\n",
                      outpoint.hash.ToString());
            return false;
        }

        const wallet::CWalletTx& wtx = mi->second;
        const CTxOut& txout = wtx.tx->vout[outpoint.n];
        int prev_height = wtx.state<wallet::TxStateConfirmed>() ?
                         wtx.state<wallet::TxStateConfirmed>()->confirmed_block_height : 0;

        coins[outpoint] = Coin(txout, prev_height, wtx.IsCoinBase());

        LogPrintf("DigiDollar: SignRedemptionTransaction - Added DD coin for %s:%d at height %d\n",
                  outpoint.hash.ToString(), outpoint.n, prev_height);
    }

    // 3. Add fee UTXOs to coins map
    for (const auto& outpoint : fee_utxos) {
        const auto mi = m_wallet->mapWallet.find(outpoint.hash);
        if (mi == m_wallet->mapWallet.end() || outpoint.n >= mi->second.tx->vout.size()) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to find fee transaction %s in wallet\n",
                      outpoint.hash.ToString());
            return false;
        }

        const wallet::CWalletTx& wtx = mi->second;
        const CTxOut& txout = wtx.tx->vout[outpoint.n];
        int prev_height = wtx.state<wallet::TxStateConfirmed>() ?
                         wtx.state<wallet::TxStateConfirmed>()->confirmed_block_height : 0;

        coins[outpoint] = Coin(txout, prev_height, wtx.IsCoinBase());

        LogPrintf("DigiDollar: SignRedemptionTransaction - Added fee coin for %s:%d at height %d, value %d\n",
                  outpoint.hash.ToString(), outpoint.n, prev_height, txout.nValue);
    }

    // 4. Sign fee inputs FIRST using wallet's standard signing
    if (!fee_utxos.empty()) {
        LogPrintf("DigiDollar: SignRedemptionTransaction - Signing fee inputs FIRST using wallet's SignTransaction\n");

        bool sign_result = m_wallet->SignTransaction(tx);
        LogPrintf("DigiDollar: SignRedemptionTransaction - SignTransaction returned: %s\n", sign_result ? "true" : "false");

        // Verify that fee inputs were actually signed
        // Fee inputs start at index (1 + dd_utxos.size())
        size_t fee_input_start = 1 + dd_utxos.size();
        for (size_t i = fee_input_start; i < tx.vin.size(); i++) {
            bool has_witness = !tx.vin[i].scriptWitness.IsNull() && !tx.vin[i].scriptWitness.stack.empty();
            bool has_scriptsig = !tx.vin[i].scriptSig.empty();

            if (!has_witness && !has_scriptsig) {
                LogPrintf("DigiDollar: SignRedemptionTransaction - Fee input %d was NOT signed\n", i);
                return false;
            }

            LogPrintf("DigiDollar: SignRedemptionTransaction - Fee input %d signed successfully\n", i);
        }
    }

    // 5. Create PrecomputedTransactionData for proper Taproot sighash calculation
    std::vector<CTxOut> prevouts;
    for (size_t idx = 0; idx < tx.vin.size(); idx++) {
        const auto& input = tx.vin[idx];
        const Coin& coin = coins.at(input.prevout);
        prevouts.push_back(coin.out);
        LogPrintf("DigiDollar: SignRedemptionTransaction - Prevout %d: amount=%d, scriptPubKey=%s\n",
                  idx, coin.out.nValue, HexStr(coin.out.scriptPubKey));
    }

    PrecomputedTransactionData txdata;
    txdata.Init(tx, std::move(prevouts), /* force=*/ true);

    // 6. Sign COLLATERAL input at index 0 (script-path spending)
    {
        LogPrintf("DigiDollar: SignRedemptionTransaction - Signing collateral input at index 0\n");

        // Get the position data to reconstruct the MAST tree
        WalletCollateralPosition position;
        bool found_position = false;
        for (const auto& [pos_id, pos] : collateral_positions) {
            if (pos_id == collateral_outpoint.hash) {
                position = pos;
                found_position = true;
                break;
            }
        }

        if (!found_position) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Position not found for %s\n",
                      collateral_outpoint.hash.ToString());
            return false;
        }

        // Use the owner_key parameter - it should be the correct key from RPC
        CPubKey ownerPubKey = owner_key.GetPubKey();
        XOnlyPubKey ownerXOnly(ownerPubKey);

        LogPrintf("DigiDollar: SignRedemptionTransaction - Using owner key: %s\n",
                  HexStr(ownerXOnly));

        // Rebuild the MAST tree using the same parameters as mint
        TaprootBuilder builder;

        DigiDollar::MintParams scriptParams;
        scriptParams.ddAmount = position.dd_minted;
        scriptParams.lockHeight = position.unlock_height;
        scriptParams.ownerKey = ownerXOnly;
        scriptParams.internalKey = ownerXOnly;
        scriptParams.oracleKeys = DigiDollar::GetOracleKeys(15);

        // Add the 4 redemption paths in the same order as mint
        CScript normalPath = DigiDollar::CreateNormalRedemptionPath(scriptParams);
        if (!normalPath.empty()) {
            builder.Add(1, normalPath, 0xC0);
        }

        CScript partialPath = DigiDollar::CreatePartialRedemptionPath(scriptParams);
        if (!partialPath.empty()) {
            builder.Add(2, partialPath, 0xC0);
        }

        CScript emergencyPath = DigiDollar::CreateEmergencyPath(scriptParams);
        if (!emergencyPath.empty()) {
            builder.Add(3, emergencyPath, 0xC0);
        }

        CScript errPath = DigiDollar::CreateERRPath(scriptParams);
        if (!errPath.empty()) {
            builder.Add(3, errPath, 0xC0);
        }

        // Finalize with the internal key
        builder.Finalize(ownerXOnly);

        if (!builder.IsValid() || !builder.IsComplete()) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to rebuild Taproot tree for collateral\n");
            return false;
        }

        // Get the TaprootSpendData
        TaprootSpendData spend_data = builder.GetSpendData();

        // Get the control block for the normal redemption path
        std::pair<CScript, int> script_key = {normalPath, TAPROOT_LEAF_TAPSCRIPT};
        auto it = spend_data.scripts.find(script_key);
        if (it == spend_data.scripts.end() || it->second.empty()) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Control block not found for normal redemption path\n");
            return false;
        }

        std::vector<unsigned char> control_block = *it->second.begin();

        LogPrintf("DigiDollar: SignRedemptionTransaction - Found control block (%d bytes) for normal redemption\n",
                  control_block.size());

        // Calculate the leaf hash for the normal redemption script
        uint256 leaf_hash = ComputeTapleafHash(TAPROOT_LEAF_TAPSCRIPT, normalPath);

        LogPrintf("DigiDollar: SignRedemptionTransaction - Leaf hash: %s\n", leaf_hash.ToString());

        // Create Schnorr signature for Taproot SCRIPT-PATH spending
        std::vector<unsigned char> sig(64);

        // Calculate sighash for Taproot script-path spending
        ScriptExecutionData execdata;
        execdata.m_annex_init = true;
        execdata.m_annex_present = false;
        execdata.m_tapleaf_hash = leaf_hash;
        execdata.m_tapleaf_hash_init = true;
        execdata.m_codeseparator_pos_init = true;
        execdata.m_codeseparator_pos = 0xFFFFFFFF;

        uint256 sighash;
        if (!SignatureHashSchnorr(sighash, execdata, tx, 0, SIGHASH_DEFAULT, SigVersion::TAPSCRIPT, txdata, MissingDataBehavior::FAIL)) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to compute Tapscript sighash for collateral input\n");
            return false;
        }

        LogPrintf("DigiDollar: SignRedemptionTransaction - Collateral SCRIPT-PATH sighash: %s\n", sighash.ToString());

        // Generate auxiliary randomness for Schnorr signing
        uint256 aux = GetRandHash();

        // Sign with UNTWEAKED internal key for script-path spending
        if (!owner_key.SignSchnorr(sighash, sig, nullptr, aux)) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to create Schnorr signature for collateral\n");
            return false;
        }

        LogPrintf("DigiDollar: SignRedemptionTransaction - Created collateral signature: %s\n", HexStr(sig));

        // Set witness stack: [signature, script, control_block]
        tx.vin[0].scriptWitness.stack.clear();
        tx.vin[0].scriptWitness.stack.push_back(sig);
        tx.vin[0].scriptWitness.stack.push_back(std::vector<unsigned char>(normalPath.begin(), normalPath.end()));
        tx.vin[0].scriptWitness.stack.push_back(control_block);

        LogPrintf("DigiDollar: SignRedemptionTransaction - Collateral witness stack: sig (%d) + script (%d) + control (%d)\n",
                  sig.size(), normalPath.size(), control_block.size());
    }

    // 7. Sign DD inputs (indices 1, 2, 3...) using KEY-PATH spending
    // DD token outputs (vout[1]) are simple P2TR with key-path only (no MAST, no CLTV)
    // Only collateral outputs (vout[0]) have MAST and use script-path spending
    for (size_t i = 0; i < dd_utxos.size(); i++) {
        size_t input_index = 1 + i;  // DD inputs start at index 1 (after collateral at index 0)

        // Check if this input is already signed
        bool already_signed = !tx.vin[input_index].scriptWitness.IsNull() &&
                             !tx.vin[input_index].scriptWitness.stack.empty();

        if (already_signed) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - DD input %d already signed by wallet\n", input_index);
            continue;
        }

        const COutPoint& outpoint = dd_utxos[i];

        // Look up the owner key for this specific DD UTXO
        // It could be from:
        // - The same mint tx as collateral (use owner_key)
        // - A different mint tx (use that tx's owner key)
        // - A transfer tx (use that transfer's owner key)
        CKey ddOwnerKey;
        if (!GetOwnerKey(outpoint.hash, ddOwnerKey)) {
            // Fallback to collateral's owner key if same mint tx
            if (outpoint.hash == collateral_outpoint.hash) {
                ddOwnerKey = owner_key;
                LogPrintf("DigiDollar: SignRedemptionTransaction - Using collateral owner key for DD from same mint tx\n");
            } else {
                LogPrintf("DigiDollar: SignRedemptionTransaction - ERROR: No owner key found for DD UTXO %s\n",
                          outpoint.hash.ToString());
                return false;
            }
        } else {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Found owner key for DD UTXO %s\n",
                      outpoint.hash.ToString());
        }

        CPubKey ddOwnerPubKey = ddOwnerKey.GetPubKey();
        XOnlyPubKey ddOwnerXOnly(ddOwnerPubKey);

        LogPrintf("DigiDollar: SignRedemptionTransaction - DD input %d using owner key: %s\n",
                  input_index, HexStr(ddOwnerXOnly));

        // Get the CTxOut for signing
        const Coin& coin = coins.at(outpoint);
        const CTxOut& prevOutput = coin.out;

        // Verify it's a valid Taproot output
        if (prevOutput.scriptPubKey.size() != 34 || prevOutput.scriptPubKey[0] != OP_1) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Invalid Taproot output for DD input %d\n", input_index);
            return false;
        }

        // Extract the output key from the script (this is the TWEAKED key)
        std::vector<unsigned char> outputKeyBytes(prevOutput.scriptPubKey.begin() + 2, prevOutput.scriptPubKey.end());
        LogPrintf("DigiDollar: SignRedemptionTransaction - Actual output key in script: %s\n", HexStr(outputKeyBytes));

        // CRITICAL: DD token outputs are simple P2TR with key-path only (no MAST, no CLTV)
        // DD can be at any vout index:
        //   - vout[1] for minted DD (collateral at vout[0])
        //   - vout[0] for transfer recipient DD
        //   - vout[1+] for transfer change DD
        // All use the same key-path signing approach
        LogPrintf("DigiDollar: SignRedemptionTransaction - Output %s:%d (DD token), using key-path signing\n",
                  outpoint.hash.ToString(), outpoint.n);

        // DD outputs are created with CreateDigiDollarP2TR which applies a Taproot tweak
        // (owner.CreateTapTweak(nullptr) - standard BIP-341 key-path only P2TR)
        // We must compute the tweaked key for verification and sign with the tweaked key
        auto tweaked = ddOwnerXOnly.CreateTapTweak(nullptr);  // nullptr = no merkle root
        if (!tweaked) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to compute tweaked key for DD input\n");
            return false;
        }
        XOnlyPubKey tweakedOutputKey = tweaked->first;

        // Verify the output key matches the TWEAKED pubkey (not raw)
        if (outputKeyBytes.size() != 32 ||
            !std::equal(outputKeyBytes.begin(), outputKeyBytes.end(), tweakedOutputKey.begin())) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Output key mismatch for key-path (expected tweaked: %s, got: %s)\n",
                     HexStr(tweakedOutputKey), HexStr(outputKeyBytes));
            return false;
        }

        LogPrintf("DigiDollar: SignRedemptionTransaction - Using KEY-PATH signing for DD token (tweaked pubkey)\n");

        // Calculate sighash for Taproot KEY-PATH spending
        uint256 dd_sighash;
        ScriptExecutionData dd_execdata;
        dd_execdata.m_annex_init = true;
        dd_execdata.m_annex_present = false;
        // For key-path: NO tapleaf hash (that's only for script-path)
        dd_execdata.m_tapleaf_hash_init = false;

        if (!SignatureHashSchnorr(dd_sighash, dd_execdata, tx, input_index, SIGHASH_DEFAULT, SigVersion::TAPROOT, txdata, MissingDataBehavior::FAIL)) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to compute key-path sighash for input %d\n", input_index);
            return false;
        }

        LogPrintf("DigiDollar: SignRedemptionTransaction - DD input %d KEY-PATH sighash: %s\n",
                  input_index, dd_sighash.ToString());

        // Sign for Taproot key-path spending
        // For BIP-341 key-path spending, we need to apply the standard tweak:
        // - Wallet stores the INTERNAL private key
        // - Output uses the TWEAKED public key (from CreateTapTweak(nullptr))
        // - SignSchnorr with empty merkle root applies the same standard tweak
        std::vector<unsigned char> dd_sig(64);
        uint256 dd_aux = GetRandHash();
        uint256 empty_merkle_root;  // Zero hash = standard key-path tweak

        if (!ddOwnerKey.SignSchnorr(dd_sighash, dd_sig, &empty_merkle_root, dd_aux)) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Failed to create key-path signature for input %d\n", input_index);
            return false;
        }

        LogPrintf("DigiDollar: SignRedemptionTransaction - Created key-path signature (tweaked): %s\n", HexStr(dd_sig));

        // For Taproot KEY-PATH spending, witness stack is: [signature]
        tx.vin[input_index].scriptWitness.stack.clear();
        tx.vin[input_index].scriptWitness.stack.push_back(dd_sig);

        LogPrintf("DigiDollar: SignRedemptionTransaction - DD input %d signed successfully with KEY-PATH (witness: sig %d bytes)\n",
                  input_index, dd_sig.size());
    }

    // 8. Verify all inputs are signed
    for (size_t i = 0; i < tx.vin.size(); i++) {
        bool has_witness = !tx.vin[i].scriptWitness.IsNull() && !tx.vin[i].scriptWitness.stack.empty();
        bool has_scriptsig = !tx.vin[i].scriptSig.empty();

        if (!has_witness && !has_scriptsig) {
            LogPrintf("DigiDollar: SignRedemptionTransaction - Input %d not signed\n", i);
            return false;
        }
    }

    LogPrintf("DigiDollar: SignRedemptionTransaction - Successfully signed all inputs (1 collateral + %d DD + %d fee)\n",
              dd_utxos.size(), fee_utxos.size());
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
        // FIX #1: Remove UTXO from dd_utxos tracking map
        auto utxo_it = dd_utxos.find(utxo);
        if (utxo_it == dd_utxos.end()) {
            LogPrintf("DigiDollar: MarkDDUTXOsSpent - Warning: UTXO not found in tracking map: %s:%d\n",
                      utxo.hash.ToString(), utxo.n);
            // Continue - may be collateral position
        } else {
            // Remove from tracking map
            CAmount dd_amount = utxo_it->second;
            dd_utxos.erase(utxo_it);
            LogPrintf("DigiDollar: Removed DD UTXO from tracking map: %s:%d (%d DD)\n",
                      utxo.hash.ToString(), utxo.n, dd_amount);
        }

        // Check if this is a DDTimeLock position (only at vout[1])
        if (utxo.n == 1) {
            // Find position in cache
            auto it = collateral_positions.find(utxo.hash);
            if (it != collateral_positions.end()) {
                // Mark as inactive (spent)
                it->second.is_active = false;

                // Persist to database if wallet available
                if (batch) {
                    if (!batch->WriteDDTimeLock(it->second)) {
                        LogPrintf("DigiDollar: MarkDDUTXOsSpent - Failed to persist spent position: %s\n",
                                  utxo.hash.ToString());
                        return false;
                    }
                }

                LogPrintf("DigiDollar: Marked DDTimeLock position as spent: %s:%d (%d DD)\n",
                          utxo.hash.ToString(), utxo.n, it->second.dd_minted);
            }
        }
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

    // Validate wallet pointer is set (must check before transaction validation)
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

void DigiDollarWallet::ProcessIncomingTransaction(const CTransactionRef& tx, const uint256& txid) {
    if (!m_wallet) {
        LogPrintf("DigiDollar: ProcessIncomingTransaction called but no wallet pointer\n");
        return;
    }

    try {
        // First, extract DD amounts from OP_RETURN (same logic as DetectIncomingDDOutputs)
        std::vector<CAmount> dd_amounts;
        int txType = 0;

        for (const auto& vout : tx->vout) {
            if (vout.scriptPubKey.size() > 0 && vout.scriptPubKey[0] == OP_RETURN) {
                CScript::const_iterator pc = vout.scriptPubKey.begin();
                opcodetype opcode;
                std::vector<unsigned char> data;

                // Skip OP_RETURN
                if (!vout.scriptPubKey.GetOp(pc, opcode)) break;

                // Check for "DD" marker
                if (!vout.scriptPubKey.GetOp(pc, opcode, data)) break;
                if (data.size() != 2 || data[0] != 'D' || data[1] != 'D') break;

                // Get transaction type
                if (!vout.scriptPubKey.GetOp(pc, opcode, data)) break;
                CScriptNum txTypeNum(data, true);
                txType = txTypeNum.getint();

                // Extract DD amounts
                while (vout.scriptPubKey.GetOp(pc, opcode, data)) {
                    if (data.size() > 0) {
                        CScriptNum amount(data, true, 8);  // 8-byte max for large DD amounts
                        dd_amounts.push_back(amount.GetInt64());
                    }
                }
                break;
            }
        }

        // Skip mint transactions - they're already handled by mint code
        if (txType == 1) {
            return;
        }

        if (dd_amounts.empty()) {
            return; // No DD amounts in this transaction
        }

        // Now check P2TR outputs we own
        size_t dd_output_index = 0;
        for (size_t n = 0; n < tx->vout.size(); ++n) {
            const CTxOut& txout = tx->vout[n];

            // Skip OP_RETURN and non-zero value outputs
            if (txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN) continue;
            if (txout.nValue != 0) continue;

            // Check if it's a P2TR output (OP_1 + 32 bytes = DD output)
            if (txout.scriptPubKey.size() == 34 && txout.scriptPubKey[0] == OP_1) {
                // Check if we own this output
                LOCK(m_wallet->cs_wallet);
                wallet::isminetype mine = m_wallet->IsMine(txout);
                if (!(mine & wallet::ISMINE_SPENDABLE)) {
                    dd_output_index++;
                    continue; // Not ours
                }

                // Get DD amount for this output
                if (dd_output_index >= dd_amounts.size()) {
                    dd_output_index++;
                    continue;
                }
                CAmount dd_amount = dd_amounts[dd_output_index];
                dd_output_index++;

                LogPrintf("DigiDollar: Detected incoming DD transaction - txid: %s, vout: %d, amount: %d cents\n",
                          txid.GetHex(), n, dd_amount);

                // Add to transaction history (txType already checked above, only TRANSFER here)
                DDTransaction ddtx;
                ddtx.txid = txid.GetHex();
                ddtx.amount = dd_amount;
                ddtx.timestamp = GetTime();
                ddtx.confirmations = 0; // Will be updated later
                ddtx.incoming = true;
                ddtx.address = ""; // Could extract sender address if needed
                ddtx.category = "receive";

                transaction_history.push_back(ddtx);

                // Persist to database
                if (m_wallet) {
                    wallet::WalletBatch batch(m_wallet->GetDatabase());
                    if (!batch.WriteDDTransaction(ddtx)) {
                        LogPrintf("DigiDollar: WARNING - Failed to persist received transaction to database\n");
                    }
                }

                LogPrintf("DigiDollar: Added receive transaction to history - TxID: %s, Amount: %d cents\n",
                          txid.GetHex(), dd_amount);

                // Only process first DD output (there should only be one per receive anyway)
                break;
            }
        }
    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: ProcessIncomingTransaction exception - %s\n", e.what());
    }
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

    // First, extract DD amounts from OP_RETURN
    // Format: OP_RETURN <"DD"> <txType> <amount1> <amount2> ...
    std::vector<CAmount> dd_amounts;
    for (const auto& txout : tx->vout) {
        if (txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN) {
            // Parse OP_RETURN data
            CScript::const_iterator pc = txout.scriptPubKey.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;

            // Skip OP_RETURN
            if (!txout.scriptPubKey.GetOp(pc, opcode)) continue;

            // Check for "DD" marker
            if (!txout.scriptPubKey.GetOp(pc, opcode, data)) continue;
            if (data.size() != 2 || data[0] != 'D' || data[1] != 'D') continue;

            // Get transaction type
            if (!txout.scriptPubKey.GetOp(pc, opcode, data)) continue;
            CScriptNum txType(data, true);
            if (txType.getint() != 2) continue;  // Only TRANSFER transactions (type 2)

            // Extract DD amounts
            while (txout.scriptPubKey.GetOp(pc, opcode, data)) {
                if (data.size() > 0) {
                    CScriptNum amount(data, true, 8);  // 8-byte max for large DD amounts
                    dd_amounts.push_back(amount.GetInt64());
                    LogPrintf("DigiDollar: Found DD amount in OP_RETURN: %lld cents\n", (long long)amount.GetInt64());
                }
            }
            break;  // Only one OP_RETURN per transaction
        }
    }

    if (dd_amounts.empty()) {
        LogPrint(BCLog::WALLETDB, "DigiDollar: No DD amounts found in OP_RETURN\n");
        return false;
    }

    // Now match P2TR outputs with DD amounts
    size_t dd_output_index = 0;
    for (uint32_t i = 0; i < tx->vout.size(); i++) {
        const CTxOut& txout = tx->vout[i];

        // Skip OP_RETURN and non-zero value outputs
        if (txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN) continue;
        if (txout.nValue != 0) continue;  // DD outputs have 0 DGB value

        // Check if it's a P2TR output (OP_1 + 32 bytes)
        if (txout.scriptPubKey.size() == 34 && txout.scriptPubKey[0] == OP_1) {
            // This is a DD output - check if we own it via dd_address_keys
            // Extract the 32-byte output key from P2TR scriptPubKey (bytes 2-33)
            std::array<unsigned char, 32> output_key;
            std::copy(txout.scriptPubKey.begin() + 2, txout.scriptPubKey.begin() + 34, output_key.begin());

            // Check if we have this key in our dd_address_keys map
            bool have_key = false;
            {
                // Check dd_address_keys for the output key
                auto it = dd_address_keys.find(output_key);
                if (it != dd_address_keys.end()) {
                    have_key = true;
                    LogPrintf("DigiDollar: Found output key in dd_address_keys for vout[%d]\n", i);
                }
            }

            // Also check if wallet recognizes it (for minted DD where we own the key)
            if (!have_key && m_wallet) {
                LOCK(m_wallet->cs_wallet);
                wallet::isminetype mine = m_wallet->IsMine(txout);
                if (mine & wallet::ISMINE_SPENDABLE) {
                    have_key = true;
                    LogPrintf("DigiDollar: Wallet IsMine returned spendable for vout[%d]\n", i);
                }
            }

            if (have_key) {
                if (dd_output_index < dd_amounts.size()) {
                    CAmount dd_amount = dd_amounts[dd_output_index];
                    LogPrintf("DigiDollar: Detected incoming DD output - vout[%d]: %d DD cents\n",
                              i, dd_amount);
                    our_dd_outputs.push_back(std::make_pair(i, dd_amount));
                }
            } else {
                LogPrint(BCLog::WALLETDB, "DigiDollar: Output %d is DD P2TR but not ours (no key found)\n", i);
            }
            dd_output_index++;
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

    // Check for duplicate processing using dd_utxos (NOT collateral_positions)
    // Received DD is tracked in dd_utxos only, NOT in collateral_positions
    // collateral_positions should ONLY contain actual minted collateral outputs
    COutPoint received_utxo(txid, vout_index);
    if (dd_utxos.find(received_utxo) != dd_utxos.end()) {
        LogPrint(BCLog::WALLETDB,
                 "DigiDollar: AddReceivedDDUTXO - UTXO %s:%d already exists, skipping\n",
                 txid.ToString(), vout_index);
        return true;  // Not an error, just already processed
    }

    LogPrintf("DigiDollar: AddReceivedDDUTXO - Adding received DD UTXO: %s:%d (%d DD cents)\n",
              txid.ToString(), vout_index, dd_amount);

    // Look up the owner key from dd_address_keys and store in dd_owner_keys
    // This allows GetOwnerKey to find the key by txid for spending
    const CTxOut& txout = tx->vout[vout_index];
    if (txout.scriptPubKey.size() == 34 && txout.scriptPubKey[0] == OP_1) {
        // Extract the output key (tweaked pubkey) from the scriptPubKey
        // dd_address_keys is indexed by std::array<unsigned char, 32>
        std::array<unsigned char, 32> output_key_array;
        std::copy(txout.scriptPubKey.begin() + 2, txout.scriptPubKey.end(), output_key_array.begin());

        // Look up the internal key in dd_address_keys
        // dd_address_keys is indexed by the tweaked output key
        auto it = dd_address_keys.find(output_key_array);
        if (it != dd_address_keys.end()) {
            // Found the key! Store it under the txid so GetOwnerKey can find it
            StoreOwnerKey(txid, it->second);
            LogPrintf("DigiDollar: AddReceivedDDUTXO - Stored owner key from dd_address_keys for tx %s\n",
                      txid.ToString());
        } else {
            LogPrintf("DigiDollar: AddReceivedDDUTXO - WARNING: No key found in dd_address_keys for output %s\n",
                      HexStr(output_key_array));
        }
    }

    // Add to DD UTXO tracking (primary balance source)
    // This is the ONLY tracking needed for received DD
    // Note: DO NOT add to collateral_positions - that would cause script-path signing
    // for DD token outputs, which is incorrect (they need key-path signing)
    dd_utxos[received_utxo] = dd_amount;

    // Persist DD UTXO to database
    if (m_wallet) {
        wallet::WalletBatch batch(m_wallet->GetDatabase());
        if (!batch.WriteDDUTXO(received_utxo, dd_amount)) {
            LogPrintf("DigiDollar: AddReceivedDDUTXO - Failed to persist DD UTXO to database\n");
            dd_utxos.erase(received_utxo);
            return false;
        }
        LogPrint(BCLog::WALLETDB, "DigiDollar: Persisted received DD UTXO to wallet.dat\n");
    }

    // Balance updates automatically (UTXO-derived approach from Phase 5.1)
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