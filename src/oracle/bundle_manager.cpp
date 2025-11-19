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
#include <oracle/mock_oracle.h>
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

    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

    // Calculate message hash for duplicate detection
    uint256 msg_hash = message.GetSignatureHash();

    // Check if we've already seen this exact message
    if (seen_message_hashes.count(msg_hash) > 0) {
        LogPrintf("Oracle: Ignoring duplicate message from oracle %d\n", message.oracle_id);
        return false;
    }

    // Add to seen set
    seen_message_hashes.insert(msg_hash);

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
        LogPrintf("Oracle: Added new message from oracle %d: price=%llu micro-USD, timestamp=%d\n",
                 message.oracle_id, message.price_micro_usd, message.timestamp);
    }

    // Try to create bundle if we have enough messages
    int32_t current_epoch = GetCurrentEpoch(GetBestHeight());
    TryCreateBundle(current_epoch);

    return true;
}

bool OracleBundleManager::RemoveOracleMessage(uint32_t oracle_id)
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

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
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

    std::vector<COraclePriceMessage> messages;
    messages.reserve(pending_messages.size());

    for (const auto& [oracle_id, message] : pending_messages) {
        messages.push_back(message);
    }

    return messages;
}

size_t OracleBundleManager::GetPendingMessageCount() const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
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

bool OracleBundleManager::HasOracleMessage(const uint256& hash) const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    return seen_message_hashes.count(hash) > 0;
}

bool OracleBundleManager::BroadcastMessage(const COraclePriceMessage& message)
{
    // Validate message before broadcasting
    if (!message.IsValid()) {
        LogPrintf("Oracle: Cannot broadcast invalid oracle message\n");
        return false;
    }

    // Add message to our own collection first
    if (!AddOracleMessage(message)) {
        LogPrintf("Oracle: Failed to add message to local storage before broadcast\n");
        return false;
    }

    // Broadcasting will be handled by the P2P layer when sendoracleprice RPC is called
    // or when the oracle operator daemon generates messages
    LogPrintf("Oracle: Broadcasted price message from oracle %d (price=%llu micro-USD)\n",
             message.oracle_id, message.price_micro_usd);

    return true;
}

void OracleBundleManager::ProcessIncomingMessage(const COraclePriceMessage& message)
{
    AddOracleMessage(message);
}

OracleBundleManager::OracleStats OracleBundleManager::GetStats() const
{
    OracleStats stats;

    {
        std::lock_guard<std::recursive_mutex> lock(mtx_messages);
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
    const Consensus::Params& consensus = Params().GetConsensus();

    // Set consensus requirements from chain parameters
    manager.min_oracle_count = consensus.nOracleRequiredMessages;
    manager.total_oracle_count = consensus.nOracleTotalOracles;
    // Note: epoch_length is not a member variable in header, but nOracleEpochLength is in consensus

    LogPrintf("Oracle: Initialized with %d-of-%d consensus, epoch length %d blocks\n",
              manager.min_oracle_count, manager.total_oracle_count, consensus.nOracleEpochLength);

    // Verify oracle public keys are configured
    if (consensus.vOraclePublicKeys.empty()) {
        LogPrintf("Oracle: WARNING - No oracle public keys configured\n");
        manager.SetEnabled(false);
    } else {
        LogPrintf("Oracle: %d oracle public keys configured\n", consensus.vOraclePublicKeys.size());
        manager.SetEnabled(true);
    }

    // Validate configuration
    if (Params().GetChainType() == ChainType::TESTNET) {
        if (consensus.nOracleRequiredMessages != 1) {
            LogPrintf("Oracle: ERROR - Phase One requires 1-of-1 consensus, got %d-of-%d\n",
                     consensus.nOracleRequiredMessages, consensus.nOracleTotalOracles);
        }
        if (consensus.vOraclePublicKeys.size() != 1) {
            LogPrintf("Oracle: ERROR - Phase One requires exactly 1 oracle public key, got %zu\n",
                     consensus.vOraclePublicKeys.size());
        }
    }

    // Check epoch length is reasonable
    if (consensus.nOracleEpochLength < 144 || consensus.nOracleEpochLength > 10080) {
        LogPrintf("Oracle: WARNING - Unusual epoch length: %d blocks\n", consensus.nOracleEpochLength);
    }
}

void OracleBundleManager::Shutdown()
{
    if (g_oracle_bundle_manager) {
        g_oracle_bundle_manager.reset();
        LogPrintf("Oracle: Oracle Bundle Manager shut down\n");
    }
}

bool OracleBundleManager::ValidateConfiguration() const
{
    const Consensus::Params& consensus = Params().GetConsensus();

    // Check Phase One constraints
    if (Params().GetChainType() == ChainType::TESTNET) {
        if (consensus.nOracleRequiredMessages != 1) {
            LogPrintf("Oracle: ERROR - Phase One requires 1-of-1 consensus, got %d-of-%d\n",
                     consensus.nOracleRequiredMessages, consensus.nOracleTotalOracles);
            return false;
        }

        if (consensus.vOraclePublicKeys.size() != 1) {
            LogPrintf("Oracle: ERROR - Phase One requires exactly 1 oracle public key, got %zu\n",
                     consensus.vOraclePublicKeys.size());
            return false;
        }
    }

    // Check epoch length is reasonable
    if (consensus.nOracleEpochLength < 144 || consensus.nOracleEpochLength > 10080) {
        LogPrintf("Oracle: WARNING - Unusual epoch length: %d blocks\n", consensus.nOracleEpochLength);
    }

    // Check min_oracle_count matches consensus
    if (min_oracle_count != consensus.nOracleRequiredMessages) {
        LogPrintf("Oracle: ERROR - min_oracle_count (%d) does not match consensus.nOracleRequiredMessages (%d)\n",
                 min_oracle_count, consensus.nOracleRequiredMessages);
        return false;
    }

    // Check total_oracle_count matches consensus
    if (total_oracle_count != consensus.nOracleTotalOracles) {
        LogPrintf("Oracle: ERROR - total_oracle_count (%d) does not match consensus.nOracleTotalOracles (%d)\n",
                 total_oracle_count, consensus.nOracleTotalOracles);
        return false;
    }

    return true;
}

bool OracleBundleManager::TryCreateBundle(int32_t epoch)
{
    std::lock_guard<std::recursive_mutex> messages_lock(mtx_messages);

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
    return message.Verify();
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

void OracleBundleManager::UpdatePriceCache(int height, uint64_t price_micro_usd)
{
    std::lock_guard<std::mutex> lock(mtx_price_cache);
    height_to_price[height] = price_micro_usd;

    // Keep cache size limited (last 1000 blocks)
    if (height_to_price.size() > 1000) {
        height_to_price.erase(height_to_price.begin());
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Price cache updated for height %d: %llu micro-USD\n",
             height, price_micro_usd);
}

uint64_t OracleBundleManager::GetOraclePriceForHeight(int height) const
{
    std::lock_guard<std::mutex> lock(mtx_price_cache);
    auto it = height_to_price.find(height);
    if (it != height_to_price.end()) {
        return it->second;
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: No price available in cache for height %d\n", height);
    return 0; // No price available
}

/**
 * OracleDataValidator Implementation
 */

bool OracleDataValidator::ValidateBlockOracleData(const CBlock& block, const CBlockIndex* pindex_prev, const Consensus::Params& params)
{
    // Phase One: Oracle validation only on testnet
    if (Params().GetChainType() != ChainType::TESTNET) {
        return true; // Oracle validation disabled on non-testnet chains
    }

    // After oracle activation height, blocks should contain oracle data
    int32_t block_height = pindex_prev ? pindex_prev->nHeight + 1 : 0;

    // Check if oracle system is activated
    if (block_height < params.nDDActivationHeight) {
        return true; // Oracle validation not required before activation
    }

    // Extract oracle bundle from coinbase OP_RETURN (output index 1)
    if (block.vtx.empty()) {
        return true; // No transactions, nothing to validate
    }

    const CTransaction& coinbase = *block.vtx[0];
    if (coinbase.vout.size() < 2) {
        // Allow blocks without oracle data during transition
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: No oracle bundle in block %d (transition period)\n", block_height);
        return true;
    }

    // Check for oracle bundle in output 1 (OP_RETURN)
    const CTxOut& oracle_output = coinbase.vout[1];
    if (!oracle_output.scriptPubKey.IsUnspendable()) {
        // Not an OP_RETURN, allow during transition
        return true;
    }

    // Extract and validate oracle bundle
    if (oracle_output.scriptPubKey.size() <= 2) {
        // Empty OP_RETURN, allow during transition
        return true;
    }

    // Extract data after OP_RETURN opcode
    std::vector<unsigned char> data;
    data.assign(oracle_output.scriptPubKey.begin() + 2, oracle_output.scriptPubKey.end());

    // Deserialize oracle bundle
    try {
        CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
        COracleBundle bundle;
        ss >> bundle;

        // Validate bundle structure
        if (!bundle.IsValid()) {
            LogPrintf("Oracle: Invalid oracle bundle in block %d\n", block_height);
            return false; // Bundle validation failed
        }

        // Phase One: Must have exactly 1 message (1-of-1 consensus)
        if (bundle.messages.size() != 1) {
            LogPrintf("Oracle: Phase One requires exactly 1 oracle message, got %d\n", bundle.messages.size());
            return false;
        }

        // Verify Schnorr signature on oracle message
        const COraclePriceMessage& msg = bundle.messages[0];
        if (!msg.Verify()) {
            LogPrintf("Oracle: Oracle message signature verification failed for oracle %u\n", msg.oracle_id);
            return false;
        }

        // Verify median price matches message price (Phase One: 1 message = median)
        if (bundle.median_price_micro_usd != msg.price_micro_usd) {
            LogPrintf("Oracle: Median price mismatch: bundle=%llu, message=%llu\n",
                     bundle.median_price_micro_usd, msg.price_micro_usd);
            return false;
        }

        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Block %d oracle bundle validated: price=%llu micro-USD\n",
                 block_height, bundle.median_price_micro_usd);

    } catch (const std::exception& e) {
        LogPrintf("Oracle: Failed to deserialize oracle bundle in block %d: %s\n", block_height, e.what());
        return false;
    }

    return true;
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
    return message.Verify();
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
    // In RegTest mode, use MockOracleManager for testing
    if (Params().GetChainType() == ChainType::REGTEST) {
        CAmount mockPrice = MockOracleManager::GetInstance().GetCurrentPrice();
        if (mockPrice > 0) {
            return mockPrice;
        }
    }

    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    CAmount price = manager.GetLatestPrice();

    // Fallback to default price if no oracle data available
    if (price <= 0) {
        price = 5000; // $50.00 per DGB default price
        LogPrintf("Oracle: Using fallback price: %d cents\n", price);
    }

    return price;
}

CAmount GetOraclePriceForHeight(int nHeight)
{
    // In RegTest mode, use MockOracleManager for testing
    if (Params().GetChainType() == ChainType::REGTEST) {
        CAmount mockPrice = MockOracleManager::GetInstance().GetCurrentPrice();
        if (mockPrice > 0) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using mock price for height %d: %lld micro-USD\n",
                     nHeight, mockPrice);
            return mockPrice;
        }
    }

    // Get oracle bundle manager instance
    OracleBundleManager& manager = OracleBundleManager::GetInstance();

    // Phase One: Try to get price from cache first (populated by ConnectBlock)
    uint64_t cached_price = manager.GetOraclePriceForHeight(nHeight);
    if (cached_price > 0) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using cached price for height %d: %llu micro-USD ($%.6f)\n",
                 nHeight, cached_price, cached_price / 1000000.0);
        return static_cast<CAmount>(cached_price);
    }

    // Fallback: Try to get from current epoch bundle (for mempool transactions)
    int32_t epoch = GetCurrentEpoch(nHeight);
    COracleBundle bundle = manager.GetCurrentBundle(epoch);

    if (bundle.HasConsensus()) {
        CAmount price = bundle.GetConsensusPrice();
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using current epoch price for height %d (epoch %d): %lld micro-USD\n",
                 nHeight, epoch, price);
        return price;
    }

    // Try previous epoch as fallback
    if (epoch > 0) {
        COracleBundle prev_bundle = manager.GetCurrentBundle(epoch - 1);
        if (prev_bundle.HasConsensus()) {
            CAmount price = prev_bundle.GetConsensusPrice();
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using previous epoch price for height %d: %lld micro-USD\n",
                     nHeight, price);
            return price;
        }
    }

    // No oracle price available - use fallback for older blocks
    LogPrint(BCLog::DIGIDOLLAR, "Oracle: No price available for height %d, using fallback\n", nHeight);
    return 5000; // Fallback: $50.00 per DGB (5000 cents)
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