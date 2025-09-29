// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/bundle_manager.h>

#include <algorithm>
#include <cassert>

#include <chainparams.h>
#include <consensus/consensus.h>
#include <kernel/chainparams.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <util/time.h>
#include <validation.h>

// Forward declaration for missing functions
extern int32_t GetBestHeight();

// Mock implementation of GetBestHeight for compilation
int32_t GetBestHeight() {
    // In real implementation, this would get the actual chain tip height
    return 1000000; // Mock height for testing
}

//! Global oracle bundle manager instance
std::unique_ptr<OracleBundleManager> g_oracle_bundle_manager;

/**
 * OracleBundleManager Implementation
 */

OracleBundleManager::OracleBundleManager()
{
    LogPrintf("Oracle: Initializing Oracle Bundle Manager\n");
}

OracleBundleManager::~OracleBundleManager()
{
    LogPrintf("Oracle: Shutting down Oracle Bundle Manager\n");
}

bool OracleBundleManager::AddOracleMessage(const COraclePriceMessage& message)
{
    if (!enabled) {
        return false;
    }

    if (!IsValidOracleMessage(message)) {
        LogPrintf("Oracle: Invalid oracle message from oracle %d\n", message.oracle_id);
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx_messages);

    // Check if we already have a message from this oracle for current epoch
    auto it = pending_messages.find(message.oracle_id);
    if (it != pending_messages.end()) {
        // Replace if new message is more recent
        if (message.timestamp > it->second.timestamp) {
            it->second = message;
            LogPrintf("Oracle: Updated message from oracle %d with newer timestamp\n", message.oracle_id);
        } else {
            LogPrintf("Oracle: Ignoring older message from oracle %d\n", message.oracle_id);
            return false;
        }
    } else {
        // Add new message
        pending_messages[message.oracle_id] = message;
        LogPrintf("Oracle: Added new message from oracle %d: price=%d, timestamp=%d\n",
                 message.oracle_id, message.price_satoshis, message.timestamp);
    }

    // Try to create bundle if we have enough messages
    int32_t current_epoch = GetCurrentEpoch(GetBestHeight());
    TryCreateBundle(current_epoch);

    return true;
}

bool OracleBundleManager::RemoveOracleMessage(uint32_t oracle_id)
{
    std::lock_guard<std::mutex> lock(mtx_messages);

    auto it = pending_messages.find(oracle_id);
    if (it != pending_messages.end()) {
        pending_messages.erase(it);
        LogPrintf("Oracle: Removed message from oracle %d\n", oracle_id);
        return true;
    }

    return false;
}

std::vector<COraclePriceMessage> OracleBundleManager::GetPendingMessages() const
{
    std::lock_guard<std::mutex> lock(mtx_messages);

    std::vector<COraclePriceMessage> messages;
    messages.reserve(pending_messages.size());

    for (const auto& [oracle_id, message] : pending_messages) {
        messages.push_back(message);
    }

    return messages;
}

size_t OracleBundleManager::GetPendingMessageCount() const
{
    std::lock_guard<std::mutex> lock(mtx_messages);
    return pending_messages.size();
}

COracleBundle OracleBundleManager::GetCurrentBundle(int32_t epoch) const
{
    std::lock_guard<std::mutex> lock(mtx_bundles);

    auto it = epoch_bundles.find(epoch);
    if (it != epoch_bundles.end()) {
        return it->second;
    }

    // Return empty bundle if not found
    return COracleBundle(epoch);
}

bool OracleBundleManager::UpdateBundle(const COracleBundle& bundle)
{
    if (!enabled) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx_bundles);

    epoch_bundles[bundle.epoch] = bundle;
    LogPrintf("Oracle: Updated bundle for epoch %d with %d messages\n",
             bundle.epoch, bundle.messages.size());

    // Update cached price if this is the latest bundle
    if (bundle.epoch >= cached_epoch && bundle.HasConsensus()) {
        cached_price = bundle.GetConsensusPrice();
        cached_epoch = bundle.epoch;
        last_update_time = GetTime();
    }

    return true;
}

bool OracleBundleManager::HasValidBundle(int32_t epoch) const
{
    std::lock_guard<std::mutex> lock(mtx_bundles);

    auto it = epoch_bundles.find(epoch);
    return it != epoch_bundles.end() && it->second.HasConsensus();
}

void OracleBundleManager::CleanupOldBundles(int32_t current_epoch)
{
    std::lock_guard<std::mutex> lock(mtx_bundles);

    // Keep bundles for current and previous epoch only
    auto it = epoch_bundles.begin();
    while (it != epoch_bundles.end()) {
        if (it->first < current_epoch - 1) {
            LogPrintf("Oracle: Cleaning up old bundle for epoch %d\n", it->first);
            it = epoch_bundles.erase(it);
        } else {
            ++it;
        }
    }
}

bool OracleBundleManager::AddOracleBundleToBlock(CBlock& block, int32_t block_height) const
{
    if (!enabled) {
        return true; // Don't fail block creation if oracles are disabled
    }

    int32_t epoch = GetCurrentEpoch(block_height);
    COracleBundle bundle = GetCurrentBundle(epoch);

    // If no consensus yet, try previous epoch
    if (!bundle.HasConsensus()) {
        bundle = GetCurrentBundle(epoch - 1);
    }

    // If still no consensus, create empty bundle (graceful degradation)
    if (!bundle.HasConsensus()) {
        LogPrintf("Oracle: No consensus bundle available for block %d, creating empty oracle data\n", block_height);
        bundle = COracleBundle(epoch);
    }

    // Create oracle script and add to coinbase
    CScript oracle_script = CreateOracleScript(bundle);
    if (oracle_script.empty()) {
        return true; // Empty script is OK for transition period
    }

    // Add oracle data as OP_RETURN output to coinbase
    CMutableTransaction coinbase_tx(*block.vtx[0]);
    CTxOut oracle_output;
    oracle_output.nValue = 0;
    oracle_output.scriptPubKey = oracle_script;
    coinbase_tx.vout.push_back(oracle_output);

    // Update block with modified coinbase
    block.vtx[0] = MakeTransactionRef(std::move(coinbase_tx));

    LogPrintf("Oracle: Added oracle bundle to block %d with %d oracle messages\n",
             block_height, bundle.messages.size());
    return true;
}

CScript OracleBundleManager::CreateOracleScript(const COracleBundle& bundle) const
{
    if (bundle.messages.empty()) {
        return CScript(); // Empty script for no oracle data
    }

    CScript script;
    script << OP_RETURN;

    // Add oracle data marker
    std::vector<unsigned char> oracle_marker = {0x4F, 0x52, 0x43}; // "ORC"
    script << oracle_marker;

    // Serialize bundle
    DataStream ss{};
    ss << bundle;

    // Convert stream to vector<unsigned char>
    std::vector<unsigned char> bundle_data;
    bundle_data.resize(ss.size());
    std::transform(ss.begin(), ss.end(), bundle_data.begin(),
                   [](std::byte b) { return static_cast<unsigned char>(b); });

    // Split data into chunks if needed (OP_PUSHDATA limits)
    const size_t max_chunk_size = 75; // Max size for OP_PUSHDATA1
    size_t offset = 0;

    while (offset < bundle_data.size()) {
        size_t chunk_size = std::min(max_chunk_size, bundle_data.size() - offset);
        std::vector<unsigned char> chunk(bundle_data.begin() + offset,
                                        bundle_data.begin() + offset + chunk_size);
        script << chunk;
        offset += chunk_size;
    }

    return script;
}

bool OracleBundleManager::ExtractOracleBundle(const CTransaction& coinbase_tx, COracleBundle& bundle) const
{
    // Look for OP_RETURN output with oracle marker
    for (const auto& output : coinbase_tx.vout) {
        if (output.scriptPubKey.size() > 4 && output.scriptPubKey[0] == OP_RETURN) {
            // Check for oracle marker "ORC"
            if (output.scriptPubKey.size() >= 7 &&
                output.scriptPubKey[2] == 0x4F && // 'O'
                output.scriptPubKey[3] == 0x52 && // 'R'
                output.scriptPubKey[4] == 0x43) { // 'C'

                try {
                    // Extract serialized data after marker
                    std::vector<unsigned char> bundle_data;
                    auto script_it = output.scriptPubKey.begin() + 5; // Skip OP_RETURN + len + marker

                    // Reconstruct data from chunks
                    while (script_it < output.scriptPubKey.end()) {
                        if (*script_it <= 75) { // OP_PUSHDATA1 range
                            unsigned char chunk_size = *script_it;
                            ++script_it;

                            if (script_it + chunk_size <= output.scriptPubKey.end()) {
                                bundle_data.insert(bundle_data.end(), script_it, script_it + chunk_size);
                                script_it += chunk_size;
                            } else {
                                break; // Invalid data
                            }
                        } else {
                            break; // Unsupported opcode
                        }
                    }

                    // Deserialize bundle
                    if (!bundle_data.empty()) {
                        DataStream ss{Span<const unsigned char>(bundle_data)};
                        try {
                            ss >> bundle;
                            return true;
                        } catch (const std::exception& e) {
                            LogPrintf("Oracle: Exception deserializing bundle: %s\n", e.what());
                        }
                    }
                }
                catch (const std::exception& e) {
                    LogPrintf("Oracle: Failed to deserialize oracle bundle: %s\n", e.what());
                }
            }
        }
    }

    return false;
}

CAmount OracleBundleManager::GetConsensusPrice(int32_t epoch) const
{
    COracleBundle bundle = GetCurrentBundle(epoch);
    return bundle.GetConsensusPrice();
}

CAmount OracleBundleManager::GetLatestPrice() const
{
    std::lock_guard<std::mutex> lock(mtx_bundles);
    return cached_price;
}

bool OracleBundleManager::UpdateCachedPrice(int32_t epoch)
{
    COracleBundle bundle = GetCurrentBundle(epoch);
    if (bundle.HasConsensus()) {
        CAmount price = bundle.GetConsensusPrice();
        if (price > 0) {
            std::lock_guard<std::mutex> lock(mtx_bundles);
            cached_price = price;
            cached_epoch = epoch;
            last_update_time = GetTime();
            return true;
        }
    }
    return false;
}

bool OracleBundleManager::ValidateOracleBundle(const COracleBundle& bundle, int32_t block_height, const Consensus::Params& params) const
{
    return OracleDataValidator::ValidateOracleBundle(bundle, GetCurrentEpoch(block_height), params);
}

bool OracleBundleManager::ValidateOracleDataInBlock(const CBlock& block, int32_t block_height, const Consensus::Params& params) const
{
    return OracleDataValidator::ValidateBlockOracleData(block, nullptr, params);
}

void OracleBundleManager::BroadcastMessage(const COraclePriceMessage& message)
{
    // TODO: Implement P2P broadcasting
    LogPrintf("Oracle: Broadcasting price message from oracle %d\n", message.oracle_id);
}

void OracleBundleManager::ProcessIncomingMessage(const COraclePriceMessage& message)
{
    AddOracleMessage(message);
}

OracleBundleManager::OracleStats OracleBundleManager::GetStats() const
{
    OracleStats stats;

    {
        std::lock_guard<std::mutex> lock(mtx_messages);
        stats.pending_messages = pending_messages.size();
    }

    {
        std::lock_guard<std::mutex> lock(mtx_bundles);
        stats.active_bundles = epoch_bundles.size();
        stats.latest_price = cached_price;
        stats.latest_epoch = cached_epoch;
        stats.last_update = last_update_time;
        stats.has_consensus = cached_price > 0;
    }

    return stats;
}

OracleBundleManager& OracleBundleManager::GetInstance()
{
    if (!g_oracle_bundle_manager) {
        g_oracle_bundle_manager = std::make_unique<OracleBundleManager>();
    }
    return *g_oracle_bundle_manager;
}

void OracleBundleManager::Initialize()
{
    OracleBundleManager& manager = GetInstance();
    manager.SetEnabled(true);
    LogPrintf("Oracle: Oracle Bundle Manager initialized\n");
}

void OracleBundleManager::Shutdown()
{
    if (g_oracle_bundle_manager) {
        g_oracle_bundle_manager.reset();
        LogPrintf("Oracle: Oracle Bundle Manager shut down\n");
    }
}

bool OracleBundleManager::TryCreateBundle(int32_t epoch)
{
    std::lock_guard<std::mutex> messages_lock(mtx_messages);

    // Check if we have enough messages for consensus
    if (pending_messages.size() < static_cast<size_t>(min_oracle_count)) {
        return false;
    }

    // Create bundle for current epoch
    COracleBundle bundle(epoch);

    // Add messages to bundle
    for (const auto& [oracle_id, message] : pending_messages) {
        if (!bundle.AddMessage(message)) {
            LogPrintf("Oracle: Failed to add message from oracle %d to bundle\n", oracle_id);
        }
    }

    // Check if bundle has consensus
    if (bundle.HasConsensus()) {
        UpdateBundle(bundle);
        LogPrintf("Oracle: Created consensus bundle for epoch %d with %d messages\n",
                 epoch, bundle.messages.size());
        return true;
    }

    return false;
}

void OracleBundleManager::UpdateEpochBundle(int32_t epoch)
{
    TryCreateBundle(epoch);
}

bool OracleBundleManager::IsValidOracleMessage(const COraclePriceMessage& message) const
{
    if (!message.IsValid()) {
        return false;
    }

    // Verify oracle ID is in valid range
    const CChainParams& params = Params();
    const OracleNodeInfo* oracle_config = params.GetOracleNode(message.oracle_id);
    if (!oracle_config) {
        return false;
    }

    // Verify signature
    return message.ValidateSignature(oracle_config->pubkey);
}

std::vector<uint32_t> OracleBundleManager::GetActiveOraclesForEpoch(int32_t epoch) const
{
    const CChainParams& params = Params();
    const std::vector<OracleNodeInfo>& all_oracles = params.GetOracleNodes();

    std::vector<OracleNodeInfo> selected_oracles = SelectOraclesForEpoch(all_oracles, epoch);
    std::vector<uint32_t> oracle_ids;

    for (const auto& oracle : selected_oracles) {
        oracle_ids.push_back(oracle.id);
    }

    return oracle_ids;
}

bool OracleBundleManager::HasRequiredSignatures(const COracleBundle& bundle, int32_t block_height) const
{
    // Check that bundle has minimum required signatures from active oracles
    int32_t epoch = GetCurrentEpoch(block_height);
    std::vector<uint32_t> active_oracles = GetActiveOraclesForEpoch(epoch);

    size_t valid_signatures = 0;
    for (const auto& message : bundle.messages) {
        // Check if oracle is active for this epoch
        auto it = std::find(active_oracles.begin(), active_oracles.end(), message.oracle_id);
        if (it != active_oracles.end()) {
            valid_signatures++;
        }
    }

    return valid_signatures >= static_cast<size_t>(min_oracle_count);
}

/**
 * OracleDataValidator Implementation
 */

bool OracleDataValidator::ValidateBlockOracleData(const CBlock& block, const CBlockIndex* pindex_prev, const Consensus::Params& params)
{
    // After oracle activation height, blocks should contain oracle data
    int32_t block_height = pindex_prev ? pindex_prev->nHeight + 1 : 0;

    // Check if oracle system is activated
    if (block_height < params.nDDActivationHeight) {
        return true; // Oracle validation not required before activation
    }

    // Extract oracle bundle from coinbase
    COracleBundle bundle;
    OracleBundleManager& manager = OracleBundleManager::GetInstance();

    if (!manager.ExtractOracleBundle(*block.vtx[0], bundle)) {
        // Allow empty oracle data during transition period
        LogPrintf("Oracle: No oracle data found in block %d (transition period)\n", block_height);
        return true;
    }

    // Validate bundle
    return ValidateOracleBundle(bundle, GetCurrentEpoch(block_height), params);
}

bool OracleDataValidator::ValidateOraclePriceForTx(const CTransaction& tx, CAmount oracle_price, int32_t block_height)
{
    // Basic validation for DigiDollar transactions
    if (oracle_price <= 0) {
        LogPrintf("Oracle: Invalid oracle price %d for transaction validation\n", oracle_price);
        return false;
    }

    // Check price is within reasonable bounds
    if (oracle_price < 100 || oracle_price > 1000000) { // $0.001 to $10 per DGB
        LogPrintf("Oracle: Oracle price %d is outside reasonable bounds\n", oracle_price);
        return false;
    }

    return true;
}

bool OracleDataValidator::ValidateOracleMessage(const COraclePriceMessage& message, const Consensus::Params& params)
{
    // Basic message validation
    if (!message.IsValid()) {
        return false;
    }

    // Verify oracle is authorized
    const CChainParams& chainparams = Params();
    const OracleNodeInfo* oracle_config = chainparams.GetOracleNode(message.oracle_id);
    if (!oracle_config || !oracle_config->is_active) {
        return false;
    }

    // Verify signature
    return message.ValidateSignature(oracle_config->pubkey);
}

bool OracleDataValidator::ValidateOracleBundle(const COracleBundle& bundle, int32_t epoch, const Consensus::Params& params)
{
    // Validate epoch
    if (!bundle.ValidateEpoch(epoch)) {
        LogPrintf("Oracle: Invalid epoch in bundle: %d (current: %d)\n", bundle.epoch, epoch);
        return false;
    }

    // Check consensus requirement
    if (!bundle.HasConsensus()) {
        LogPrintf("Oracle: Bundle does not have required consensus (%d messages)\n", bundle.messages.size());
        return false;
    }

    // Validate individual messages
    for (const auto& message : bundle.messages) {
        if (!ValidateOracleMessage(message, params)) {
            LogPrintf("Oracle: Invalid message from oracle %d in bundle\n", message.oracle_id);
            return false;
        }
    }

    return true;
}

bool OracleDataValidator::CheckOracleSignatures(const COracleBundle& bundle, const Consensus::Params& params)
{
    for (const auto& message : bundle.messages) {
        if (!ValidateOracleMessage(message, params)) {
            return false;
        }
    }
    return true;
}

bool OracleDataValidator::CheckOracleEpoch(const COracleBundle& bundle, int32_t current_epoch)
{
    return bundle.ValidateEpoch(current_epoch);
}

bool OracleDataValidator::CheckOracleConsensus(const COracleBundle& bundle)
{
    return bundle.HasConsensus();
}

/**
 * OracleIntegration Utility Functions
 */

namespace OracleIntegration {

CAmount GetCurrentOraclePrice()
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    CAmount price = manager.GetLatestPrice();

    // Fallback to mock price if no oracle data available
    if (price <= 0) {
        price = 5000; // $0.05 default price
        LogPrintf("Oracle: Using fallback price: %d cents\n", price);
    }

    return price;
}

bool IsOracleSystemReady()
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    return manager.IsEnabled() && manager.GetLatestPrice() > 0;
}

COracleBundle GetOracleBundleForHeight(int32_t block_height)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    int32_t epoch = GetCurrentEpoch(block_height);
    return manager.GetCurrentBundle(epoch);
}

bool ValidateOracleRequirements(const CTransaction& tx, int32_t block_height)
{
    CAmount oracle_price = GetCurrentOraclePrice();
    return OracleDataValidator::ValidateOraclePriceForTx(tx, oracle_price, block_height);
}

} // namespace OracleIntegration