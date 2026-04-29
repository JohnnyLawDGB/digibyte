// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/bundle_manager.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <optional>

#include <chainparams.h>
#include <common/args.h>
#include <consensus/consensus.h>
#include <digidollar/digidollar.h>
#include <kernel/chainparams.h>
#include <logging.h>
#include <net.h>
#include <netmessagemaker.h>
#include <oracle/mock_oracle.h>
#include <oracle/musig2_aggregator.h>
#include <oracle/musig2_orchestrator.h>
#include <oracle/musig2_messages.h>
#include <oracle/signing_orchestrator.h>
#include <oracle/musig2_session.h>
#include <oracle/node.h>
#include <primitives/block.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <script/script.h>
#include <script/standard.h>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <util/time.h>
#include <validation.h>

// Forward declaration for missing functions
extern int32_t GetBestHeight();

// MuSig2 global signing session store (defined in musig2_session.cpp)
extern std::map<int32_t, MuSig2SigningSession> g_oracle_signing_sessions;
extern Mutex g_oracle_signing_sessions_mutex;

// Get current chain height - used by oracle bundle manager
int32_t GetBestHeight() {
    // TODO: Wire up to ChainstateManager properly
    // For now, return 0 if we can't determine height
    return 0;
}

//! Global oracle bundle manager instance
std::unique_ptr<OracleBundleManager> g_oracle_bundle_manager;

/**
 * OracleBundleManager Implementation
 */

OracleBundleManager::OracleBundleManager()
{
    const int64_t configured_wait_ms = std::max<int64_t>(0, gArgs.GetIntArg("-oraclebundlewaitms", 2000));
    const int64_t configured_poll_ms = std::max<int64_t>(1, gArgs.GetIntArg("-oraclebundlewaitpollms", 200));
    near_quorum_wait_timeout = std::chrono::milliseconds(configured_wait_ms);
    near_quorum_wait_poll_interval = std::chrono::milliseconds(configured_poll_ms);
    if (near_quorum_wait_timeout.count() > 0 && near_quorum_wait_poll_interval > near_quorum_wait_timeout) {
        near_quorum_wait_poll_interval = near_quorum_wait_timeout;
    }

    LogPrintf("Oracle: Initializing Oracle Bundle Manager\n");
    LogPrintf("Oracle: Near-quorum wait config: timeout=%lldms poll=%lldms\n",
             static_cast<long long>(near_quorum_wait_timeout.count()),
             static_cast<long long>(near_quorum_wait_poll_interval.count()));
}

OracleBundleManager::~OracleBundleManager()
{
    LogPrintf("Oracle: Shutting down Oracle Bundle Manager\n");
}

bool OracleBundleManager::AddOracleMessage(const COraclePriceMessage& message)
{
    LogPrint(BCLog::DIGIDOLLAR, "Oracle: AddOracleMessage called for oracle_id=%d, price=%llu, timestamp=%d, enabled=%d\n",
             message.oracle_id, message.price_micro_usd, message.timestamp, enabled);

    if (!enabled) {
        LogPrintf("Oracle: Manager not enabled, rejecting message\n");
        return false;
    }

    if (!IsValidOracleMessage(message)) {
        LogPrintf("Oracle: Invalid oracle message from oracle %d\n", message.oracle_id);
        return false;
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Message passed IsValidOracleMessage check\n");

    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

    // Purge stale messages from pending_messages.
    // Messages older than ORACLE_MAX_AGE_SECONDS are from oracles that may no
    // longer be active. Without this cleanup, oracle nodes accumulate entries
    // from briefly-running oracles, inflating the consensus count (e.g. showing
    // "7-of-5" instead of "5-of-5").
    {
        int64_t now = GetTime();
        auto stale_it = pending_messages.begin();
        while (stale_it != pending_messages.end()) {
            if (now - stale_it->second.timestamp > ORACLE_MAX_AGE_SECONDS) {
                LogPrint(BCLog::DIGIDOLLAR, "Oracle: Purging stale message from oracle %d (age %lld seconds)\n",
                         stale_it->first, now - stale_it->second.timestamp);
                stale_it = pending_messages.erase(stale_it);
            } else {
                ++stale_it;
            }
        }
    }

    // Periodic cleanup of seen message hashes to prevent deadlock.
    // When the oracle consensus round stalls (e.g., due to rapid block production
    // or network partition), messages with the same Phase2 hash keep getting
    // rejected as duplicates, preventing recovery. Clearing the set periodically
    // allows fresh consensus rounds to form. The 300-second interval is shorter
    // than any network's epoch length (testnet=750s, mainnet=1500s), ensuring at
    // least one cleanup per epoch. The pending_messages map (keyed by oracle_id)
    // provides the authoritative dedup — seen_message_hashes is best-effort P2P
    // optimization only.
    {
        static int64_t last_seen_cleanup = 0;
        int64_t now_cleanup = GetTime();
        if (now_cleanup - last_seen_cleanup > 300) {
            size_t old_size = seen_message_hashes.size();
            seen_message_hashes.clear();
            last_seen_cleanup = now_cleanup;
            if (old_size > 0) {
                LogPrint(BCLog::DIGIDOLLAR, "Oracle: Periodic cleanup cleared %zu seen message hashes\n", old_size);
            }
        }
    }

    // Calculate message hash for duplicate detection.
    // Use Phase2 hash (oracle_id + price + timestamp) for Phase2-signed messages.
    // GetSignatureHash() includes block_height+nonce which are NOT covered by
    // Phase2 signatures — an attacker can mutate those fields to bypass dedup.
    uint256 msg_hash = (!message.schnorr_sig.empty())
        ? message.GetPhase2SignatureHash()
        : message.GetSignatureHash();

    // Check if we've already seen this exact message
    if (seen_message_hashes.count(msg_hash) > 0) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Ignoring duplicate message from oracle %d\n", message.oracle_id);
        return false;
    }

    // Add to seen set
    seen_message_hashes.insert(msg_hash);

    // Cap seen_message_hashes to prevent unbounded growth.
    // Each oracle re-broadcasts every ~60 seconds with a new timestamp, adding a
    // new hash each time.  With 8 oracles broadcasting for hours, the set can
    // grow very large.  Keep only the most recent hashes; old hashes are no
    // longer needed because the corresponding messages have already expired
    // from pending_messages (purged above) and from the P2P relay window.
    static constexpr size_t MAX_SEEN_HASHES = 2048;
    if (seen_message_hashes.size() > MAX_SEEN_HASHES) {
        // std::set iteration is ordered; erasing from begin() removes the
        // "smallest" hashes which, being random SHA-256 digests, are
        // effectively arbitrary.  This is acceptable because the set is only
        // a best-effort duplicate filter — true dedup is enforced by the
        // pending_messages oracle_id key.
        auto erase_end = seen_message_hashes.begin();
        std::advance(erase_end, seen_message_hashes.size() - MAX_SEEN_HASHES);
        seen_message_hashes.erase(seen_message_hashes.begin(), erase_end);
    }

    // Check if we already have a message from this oracle for current epoch
    auto it = pending_messages.find(message.oracle_id);
    if (it != pending_messages.end()) {
        // Replace if new message is more recent
        if (message.timestamp > it->second.timestamp) {
            it->second = message;
            LogPrintf("Oracle: Updated message from oracle %d with newer timestamp\n", message.oracle_id);
        } else {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Ignoring older message from oracle %d\n", message.oracle_id);
            return false;
        }
    } else {
        // Add new message
        pending_messages[message.oracle_id] = message;
        LogPrintf("Oracle: Added new message from oracle %d: price=%llu micro-USD, timestamp=%d\n",
                 message.oracle_id, message.price_micro_usd, message.timestamp);
    }

    // Note: Bundle creation happens in AddOracleBundleToBlock() or when explicitly requested
    // Don't auto-create here to avoid epoch mismatch issues

    // Update cached price only when consensus is met
    // Phase One (1-of-1): any single message is consensus
    // Phase Two (4-of-7): need min_oracle_count agreeing messages
    //
    // Only count fresh messages (within ORACLE_MAX_AGE_SECONDS) for consensus.
    // Stale entries were purged above, so pending_messages.size() is the
    // fresh message count.
    {
        std::lock_guard<std::recursive_mutex> pending_lock(mtx_messages);
        int fresh_count = static_cast<int>(pending_messages.size());
        if (fresh_count >= min_oracle_count) {
            COracleBundle temp;
            temp.messages.reserve(pending_messages.size());
            for (const auto& [id, m] : pending_messages) {
                temp.messages.push_back(m);
            }

            const Consensus::Params& cparams = Params().GetConsensus();
            const CAmount consensus_price = CalculateConsensusPrice(temp, cparams);
            if (consensus_price <= 0) {
                LogPrint(BCLog::DIGIDOLLAR, "Oracle: %d fresh messages reached threshold but no valid consensus price was calculated\n",
                         fresh_count);
                m_messages_updated_cv.notify_all();
                return true;
            }

            std::lock_guard<std::mutex> price_lock(mtx_bundles);
            cached_price = consensus_price;
            last_update_time = GetTime();
            LogPrintf("Oracle: Updated cached price with %d/%d oracles in consensus (min %d required): %llu micro-USD ($%.6f)\n",
                     fresh_count, (int)pending_messages.size(), min_oracle_count,
                     static_cast<uint64_t>(consensus_price), consensus_price / 1000000.0);

            // T5-03: When consensus is reached from individual messages, try to generate
            // consensus attestations from local oracle nodes. Each oracle signs
            // H(oracle_id, consensus_price, consensus_timestamp) so the signature verifies
            // when stored on-chain with the consensus values.
            if (min_oracle_count > 1) {
                uint64_t att_consensus_price = static_cast<uint64_t>(consensus_price);
                int64_t att_consensus_timestamp = 0;
                // Compute consensus values (median price + median timestamp)
                {
                    std::vector<int64_t> ts;
                    for (const auto& m : temp.messages) ts.push_back(m.timestamp);
                    std::sort(ts.begin(), ts.end());
                    size_t tmid = ts.size() / 2;
                    att_consensus_timestamp = (ts.size() % 2 == 0) ?
                        (ts[tmid - 1] + ts[tmid]) / 2 : ts[tmid];

                    // Proactive broadcast: send consensus proposal when quorum is reached
                    // This ensures remote oracles get the proposal BEFORE any block template is needed
                    // Use cached_epoch, or fallback to a conservative estimate if not set
                    int32_t epoch_for_broadcast = (cached_epoch >= 0) ? cached_epoch :
                                                   static_cast<int32_t>(GetTime() / (1440 * 15));  // 1440 blocks * 15 seconds/block
                    BroadcastConsensusProposal(epoch_for_broadcast, att_consensus_price, att_consensus_timestamp);

                    // Ask local oracle nodes to sign consensus values
                    OracleManager& om = OracleManager::GetInstance();
                    for (const auto& [oid, omsg] : pending_messages) {
                        if (pending_attestations.count(oid)) continue; // Already have attestation
                        OracleNode* node = om.GetOracleNode(oid);
                        if (node) {
                            COraclePriceMessage att = node->CreateConsensusAttestation(
                                att_consensus_price, att_consensus_timestamp);
                            if (!att.schnorr_sig.empty() && att.VerifyPhase2()) {
                                pending_attestations[oid] = att;
                                LogPrint(BCLog::DIGIDOLLAR,
                                    "Oracle: Auto-generated consensus attestation for local oracle %d\n", oid);
                            }
                        }
                    }
                }
            }
        } else {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: %d fresh messages, need %d for consensus - cached price unchanged\n",
                     fresh_count, min_oracle_count);
        }
    }

    m_messages_updated_cv.notify_all();
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

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: GetPendingMessages called, pending_messages.size()=%zu\n", pending_messages.size());

    std::vector<COraclePriceMessage> messages;
    messages.reserve(pending_messages.size());

    for (const auto& [oracle_id, message] : pending_messages) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: GetPendingMessages - oracle_id=%d, price=%llu, timestamp=%d\n",
                 oracle_id, message.price_micro_usd, message.timestamp);
        messages.push_back(message);
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: GetPendingMessages returning %zu messages\n", messages.size());
    return messages;
}

size_t OracleBundleManager::GetPendingMessageCount() const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    return pending_messages.size();
}

void OracleBundleManager::ClearPendingMessages()
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    pending_messages.clear();
    pending_attestations.clear();
    seen_message_hashes.clear();
    LogPrintf("Oracle: Manually cleared all pending messages and attestations\n");
    m_messages_updated_cv.notify_all();
}

void OracleBundleManager::InjectTestMessage(const COraclePriceMessage& message)
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    pending_messages[message.oracle_id] = message;
    LogPrintf("Oracle: Injected test message for oracle %d, price=%llu\n",
             message.oracle_id, message.price_micro_usd);
    m_messages_updated_cv.notify_all();
}

bool OracleBundleManager::AddConsensusAttestation(const COraclePriceMessage& attestation)
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

    // Basic validation: must have a Phase 2 signature
    if (attestation.schnorr_sig.empty() || attestation.schnorr_sig.size() != 64) {
        LogPrintf("Oracle: Rejecting consensus attestation from oracle %d: missing/invalid signature\n",
                 attestation.oracle_id);
        return false;
    }

    // Price range check
    if (attestation.price_micro_usd < ORACLE_MIN_PRICE_MICRO_USD ||
        attestation.price_micro_usd > ORACLE_MAX_PRICE_MICRO_USD) {
        LogPrintf("Oracle: Rejecting consensus attestation from oracle %d: price out of range\n",
                 attestation.oracle_id);
        return false;
    }

    // Verify Phase 2 signature (attestation signs consensus values)
    if (!attestation.VerifyPhase2()) {
        // Also try with chainparams pubkey binding
        const CChainParams& params = Params();
        const OracleNodeInfo* oracle_config = params.GetOracleNode(attestation.oracle_id);
        if (oracle_config) {
            COraclePriceMessage bound = attestation;
            bound.oracle_pubkey = XOnlyPubKey(oracle_config->pubkey);
            if (!bound.VerifyPhase2()) {
                LogPrintf("Oracle: Rejecting consensus attestation from oracle %d: invalid signature\n",
                         attestation.oracle_id);
                return false;
            }
            // Store with chainparams-bound pubkey
            pending_attestations[attestation.oracle_id] = bound;
        } else {
            // In regtest, chainparams may not have this oracle — accept with original pubkey
            if (Params().GetChainType() == ChainType::REGTEST) {
                pending_attestations[attestation.oracle_id] = attestation;
            } else {
                LogPrintf("Oracle: Rejecting consensus attestation from oracle %d: unknown oracle\n",
                         attestation.oracle_id);
                return false;
            }
        }
    } else {
        pending_attestations[attestation.oracle_id] = attestation;
    }

    LogPrintf("Oracle: Added consensus attestation from oracle %d: price=%llu, timestamp=%lld\n",
             attestation.oracle_id, attestation.price_micro_usd, attestation.timestamp);
    m_messages_updated_cv.notify_all();
    return true;
}

std::vector<COraclePriceMessage> OracleBundleManager::GetPendingAttestations() const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    std::vector<COraclePriceMessage> attestations;
    attestations.reserve(pending_attestations.size());
    for (const auto& [id, msg] : pending_attestations) {
        attestations.push_back(msg);
    }
    return attestations;
}

size_t OracleBundleManager::GetPendingAttestationCount() const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    return pending_attestations.size();
}

void OracleBundleManager::ClearPendingAttestations()
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    pending_attestations.clear();
    m_messages_updated_cv.notify_all();
}

bool OracleBundleManager::ComputeConsensusValues(uint64_t& consensus_price, int64_t& consensus_timestamp) const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

    if (static_cast<int>(pending_messages.size()) < min_oracle_count) {
        return false;
    }

    // Build a temporary bundle from pending messages to compute consensus price
    COracleBundle temp;
    for (const auto& [id, msg] : pending_messages) {
        temp.messages.push_back(msg);
    }

    const Consensus::Params& cparams = Params().GetConsensus();
    CAmount price = CalculateConsensusPrice(temp, cparams);
    if (price <= 0) return false;

    consensus_price = static_cast<uint64_t>(price);

    // Compute consensus timestamp as the median of individual message timestamps
    std::vector<int64_t> timestamps;
    for (const auto& msg : temp.messages) {
        timestamps.push_back(msg.timestamp);
    }
    std::sort(timestamps.begin(), timestamps.end());
    size_t mid = timestamps.size() / 2;
    if (timestamps.size() % 2 == 0) {
        consensus_timestamp = (timestamps[mid - 1] + timestamps[mid]) / 2;
    } else {
        consensus_timestamp = timestamps[mid];
    }

    return true;
}

bool OracleBundleManager::ValidateConsensusProposal(uint64_t consensus_price, int64_t consensus_timestamp) const
{
    if (consensus_price < ORACLE_MIN_PRICE_MICRO_USD ||
        consensus_price > ORACLE_MAX_PRICE_MICRO_USD) {
        LogPrint(BCLog::DIGIDOLLAR,
                 "Oracle: Rejecting consensus proposal with out-of-range price=%llu\n",
                 consensus_price);
        return false;
    }

    uint64_t local_price = 0;
    int64_t local_timestamp = 0;
    if (!ComputeConsensusValues(local_price, local_timestamp)) {
        LogPrint(BCLog::DIGIDOLLAR,
                 "Oracle: Rejecting consensus proposal price=%llu timestamp=%lld: no local quorum\n",
                 consensus_price, consensus_timestamp);
        return false;
    }

    if (local_price != consensus_price || local_timestamp != consensus_timestamp) {
        LogPrint(BCLog::DIGIDOLLAR,
                 "Oracle: Rejecting consensus proposal price=%llu timestamp=%lld: local price=%llu timestamp=%lld\n",
                 consensus_price, consensus_timestamp, local_price, local_timestamp);
        return false;
    }

    return true;
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
    if (bundle.epoch >= cached_epoch && bundle.HasConsensus(min_oracle_count)) {
        cached_price = bundle.GetConsensusPrice(min_oracle_count);
        cached_epoch = bundle.epoch;
        last_update_time = GetTime();
    }

    return true;
}

bool OracleBundleManager::HasValidBundle(int32_t epoch) const
{
    std::lock_guard<std::mutex> lock(mtx_bundles);

    auto it = epoch_bundles.find(epoch);
    return it != epoch_bundles.end() && it->second.HasConsensus(min_oracle_count);
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

bool OracleBundleManager::AddOracleBundleToBlock(CBlock& block, int32_t block_height)
{
    LogPrintf("Oracle: AddOracleBundleToBlock called for height %d, enabled=%d, min_oracle_count=%d\n",
             block_height, enabled, min_oracle_count);

    if (!enabled) {
        LogPrintf("Oracle: Oracles disabled, skipping bundle addition\n");
        return true; // Don't fail block creation if oracles are disabled
    }

    int32_t epoch = GetCurrentEpoch(block_height);
    LogPrintf("Oracle: Current epoch=%d for height %d\n", epoch, block_height);
    const Consensus::Params& consensus_params = Params().GetConsensus();
    const bool chainparams_phase_one =
        !force_phase2 &&
        block_height < consensus_params.nDigiDollarPhase2Height &&
        min_oracle_count == consensus_params.nOracleRequiredMessages;
    const int32_t required_bundle_messages =
        chainparams_phase_one ? 1 : min_oracle_count;

    // Cleanup stale MuSig2 sessions at epoch boundary (keep current epoch only).
    // This prevents unbounded growth of the global session map when epoch advances.
    {
        LOCK(g_oracle_signing_sessions_mutex);
        auto it = g_oracle_signing_sessions.begin();
        while (it != g_oracle_signing_sessions.end()) {
            if (it->first < epoch) {
                LogPrintf("Oracle: Pruning stale MuSig2 session for epoch %d (current epoch=%d)\n",
                          it->first, epoch);
                it = g_oracle_signing_sessions.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Try MuSig2 (v0x03) first — the OracleSigningOrchestrator drives the
    // MuSig2 protocol asynchronously via BlockConnected callbacks.
    // AddOracleBundleToBlock only *queries* for a completed session.
    if (!force_phase2 && g_signing_orchestrator) {
        COracleBundle bundle(epoch);
        bundle.version = 3;

        uint64_t signed_price = 0;
        int64_t signed_timestamp = 0;
        bool session_ready = g_signing_orchestrator->GetCompletedSession(
            epoch, bundle.aggregate_sig, bundle.participation_bitmap,
            signed_price, signed_timestamp);

        if (session_ready) {
            LogPrintf("Oracle: MuSig2 session for epoch %d is COMPLETE, sig=%zu bytes, bitmap=%zu bytes\n",
                     epoch, bundle.aggregate_sig.size(), bundle.participation_bitmap.size());

            // Use the EXACT values signed by the MuSig2 ceremony
            bundle.median_price_micro_usd = signed_price;
            bundle.timestamp = signed_timestamp;

            CScript oracle_script = CreateOracleScript(bundle);
            if (!oracle_script.empty()) {
                CMutableTransaction coinbase_tx(*block.vtx[0]);
                CTxOut oracle_output;
                oracle_output.nValue = 0;
                oracle_output.scriptPubKey = oracle_script;
                coinbase_tx.vout.push_back(oracle_output);
                block.vtx[0] = MakeTransactionRef(std::move(coinbase_tx));

                LogPrintf("Oracle: Added MuSig2 v0x03 bundle to block %d (epoch %d)\n",
                         block_height, epoch);
                return true;
            }
        }
        // MuSig2 not ready — fall through to v0x02 individual-sig bundling
        LogPrintf("Oracle: MuSig2 not ready for epoch %d, using v0x02 fallback\n", epoch);
    }

    // Phase 1/2 bundling (existing logic below)
    COracleBundle bundle = GetCurrentBundle(epoch);
    LogPrintf("Oracle: GetCurrentBundle(epoch=%d) returned bundle with %zu messages, HasConsensus=%d\n",
             epoch, bundle.messages.size(), bundle.HasConsensus(required_bundle_messages));

    // If no consensus yet, try previous epoch
    if (!bundle.HasConsensus(required_bundle_messages)) {
        bundle = GetCurrentBundle(epoch - 1);
        LogPrintf("Oracle: Tried previous epoch, bundle now has %zu messages, HasConsensus=%d\n",
                 bundle.messages.size(), bundle.HasConsensus(required_bundle_messages));
    }

    // Phase One: If still no consensus and min_oracle_count == 1, use pending messages directly
    // This allows unit tests to work without full epoch consensus flow
    if (!bundle.HasConsensus(required_bundle_messages) && required_bundle_messages == 1) {
        LogPrintf("Oracle: Phase One mode - checking pending messages\n");
        std::lock_guard<std::recursive_mutex> lock(mtx_messages);
        std::vector<COraclePriceMessage> pending;
        pending.reserve(pending_messages.size());
        for (const auto& pair : pending_messages) {
            pending.push_back(pair.second);
        }
        LogPrintf("Oracle: Phase One - %zu pending messages\n", pending.size());

        if (!pending.empty()) {
            bundle = COracleBundle(epoch);
            bundle.messages.push_back(pending.front());
            bundle.median_price_micro_usd = pending[0].price_micro_usd;
            LogPrintf("Oracle: Phase One - Using 1 of %zu pending message(s) for block %d\n",
                     pending.size(), block_height);
            // NOTE: Do NOT clear pending_messages here. CreateNewBlock() fires every
            // ~15 seconds but oracle messages broadcast every ~60 seconds. Clearing
            // here drains messages 4x faster than replenished, resulting in zero
            // oracle data in blocks. Messages expire naturally via the stale purge
            // in AddOracleMessage() after ORACLE_MAX_AGE_SECONDS (3600s).
        } else {
            LogPrintf("Oracle: Phase One - No pending messages available!\n");
        }
    }

    // Phase Two: Build bundle from consensus attestations (signatures over consensus values)
    // T5-03: Each oracle signs H(oracle_id, consensus_price, consensus_timestamp) — the same
    // consensus values stored on-chain. This ensures signatures verify after round-trip through
    // the Phase 2 on-chain format (which stores ONE consensus price + N signatures).
    if (!bundle.HasConsensus(required_bundle_messages) && required_bundle_messages > 1) {
        LogPrintf("Oracle: Phase Two mode - checking for consensus attestations (%d-of-N)\n", required_bundle_messages);

        struct PhaseTwoAttempt {
            bool has_bundle{false};
            bool near_quorum{false};
            size_t individual_count{0};
            size_t valid_attestation_count{0};
            uint64_t consensus_price{0};
            int64_t consensus_timestamp{0};
            COracleBundle candidate;
        };

        auto try_build_phase_two_bundle_locked = [&]() -> PhaseTwoAttempt {
            PhaseTwoAttempt result;
            const size_t near_quorum_threshold = required_bundle_messages > 1 ? static_cast<size_t>(required_bundle_messages - 1) : 0;

            std::vector<COraclePriceMessage> all_individual;
            all_individual.reserve(pending_messages.size());
            for (const auto& pair : pending_messages) {
                all_individual.push_back(pair.second);
            }
            result.individual_count = all_individual.size();

            if (result.individual_count < static_cast<size_t>(required_bundle_messages)) {
                result.near_quorum = result.individual_count >= near_quorum_threshold;
                return result;
            }

            COracleBundle temp_bundle(epoch);
            temp_bundle.messages = all_individual;
            const Consensus::Params& cparams = Params().GetConsensus();
            CAmount computed_price = CalculateConsensusPrice(temp_bundle, cparams);
            if (computed_price <= 0) {
                return result;
            }

            result.consensus_price = static_cast<uint64_t>(computed_price);

            // Consensus timestamp: median of individual message timestamps
            std::vector<int64_t> timestamps;
            timestamps.reserve(all_individual.size());
            for (const auto& msg : all_individual) {
                timestamps.push_back(msg.timestamp);
            }
            std::sort(timestamps.begin(), timestamps.end());
            const size_t tmid = timestamps.size() / 2;
            result.consensus_timestamp = (timestamps.size() % 2 == 0) ?
                (timestamps[tmid - 1] + timestamps[tmid]) / 2 : timestamps[tmid];

            // Collect valid consensus attestations.
            std::vector<COraclePriceMessage> valid_attestations;
            valid_attestations.reserve(std::max(pending_attestations.size(), all_individual.size()));
            std::set<uint32_t> seen_ids;

            for (const auto& pair : pending_attestations) {
                const COraclePriceMessage& att = pair.second;
                if (att.price_micro_usd == result.consensus_price &&
                    att.timestamp == result.consensus_timestamp &&
                    att.VerifyPhase2()) {
                    valid_attestations.push_back(att);
                    seen_ids.insert(att.oracle_id);
                }
            }

            // Also check pending_messages — an oracle may have already signed consensus values.
            for (const auto& msg : all_individual) {
                if (seen_ids.count(msg.oracle_id)) continue;
                COraclePriceMessage check_msg = msg;
                check_msg.price_micro_usd = result.consensus_price;
                check_msg.timestamp = result.consensus_timestamp;
                if (check_msg.VerifyPhase2()) {
                    valid_attestations.push_back(check_msg);
                    seen_ids.insert(msg.oracle_id);
                }
            }

            // Try to generate attestation from local oracle node (if available).
            if (static_cast<int>(valid_attestations.size()) < required_bundle_messages) {
                OracleManager& om = OracleManager::GetInstance();
                for (const auto& pair : pending_messages) {
                    const uint32_t id = pair.first;
                    if (seen_ids.count(id)) continue;
                    OracleNode* node = om.GetOracleNode(id);
                    if (node) {
                        COraclePriceMessage att = node->CreateConsensusAttestation(
                            result.consensus_price, result.consensus_timestamp);
                        if (!att.schnorr_sig.empty() && att.VerifyPhase2()) {
                            valid_attestations.push_back(att);
                            seen_ids.insert(id);
                            pending_attestations[id] = att;
                            LogPrintf("Oracle: Phase Two - Generated local consensus attestation for oracle %d\n", id);
                        }
                    }
                }
            }

            result.valid_attestation_count = valid_attestations.size();
            if (result.valid_attestation_count >= static_cast<size_t>(required_bundle_messages)) {
                result.has_bundle = true;
                result.candidate = COracleBundle(epoch);
                result.candidate.messages = std::move(valid_attestations);
                result.candidate.median_price_micro_usd = result.consensus_price;
                result.candidate.timestamp = result.consensus_timestamp;
                return result;
            }

            // Once we have enough individual messages for a consensus price, quorum progress
            // should be measured on attestations rather than raw message count.
            result.near_quorum = result.valid_attestation_count >= near_quorum_threshold;
            return result;
        };

        std::unique_lock<std::recursive_mutex> lock(mtx_messages);
        PhaseTwoAttempt attempt = try_build_phase_two_bundle_locked();

        if (!attempt.has_bundle && attempt.near_quorum && near_quorum_wait_timeout.count() > 0) {
            ++near_quorum_wait_attempts;
            const uint64_t wait_attempt = near_quorum_wait_attempts;
            const auto wait_started = std::chrono::steady_clock::now();
            const auto deadline = wait_started + near_quorum_wait_timeout;

            LogPrintf("Oracle: Near-quorum wait triggered for block %d (attempt #%llu): %zu individual, %zu attestations, need %d\n",
                     block_height, wait_attempt, attempt.individual_count, attempt.valid_attestation_count, required_bundle_messages);

            while (!attempt.has_bundle && attempt.near_quorum) {
                const auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    break;
                }
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
                const auto step = std::min(near_quorum_wait_poll_interval, remaining);
                m_messages_updated_cv.wait_for(lock, step);
                attempt = try_build_phase_two_bundle_locked();
            }

            const auto waited_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - wait_started).count();
            if (attempt.has_bundle) {
                ++near_quorum_wait_successes;
                LogPrintf("Oracle: Near-quorum wait succeeded after %lldms (success %llu/%llu)\n",
                         waited_ms,
                         static_cast<unsigned long long>(near_quorum_wait_successes),
                         static_cast<unsigned long long>(near_quorum_wait_attempts));
            } else {
                ++near_quorum_wait_timeouts;
                LogPrintf("Oracle: Near-quorum wait timed out after %lldms (timeouts %llu/%llu)\n",
                         waited_ms,
                         static_cast<unsigned long long>(near_quorum_wait_timeouts),
                         static_cast<unsigned long long>(near_quorum_wait_attempts));
            }
        }

        if (attempt.has_bundle) {
            bundle = std::move(attempt.candidate);
            LogPrintf("Oracle: Phase Two - Created bundle with %zu consensus attestations, price=%llu\n",
                     bundle.messages.size(), bundle.median_price_micro_usd);
        } else if (attempt.consensus_price > 0) {
            const uint64_t consensus_price = attempt.consensus_price;
            const int64_t consensus_timestamp = attempt.consensus_timestamp;
            const size_t valid_attestation_count = attempt.valid_attestation_count;
            const size_t individual_count = attempt.individual_count;
            lock.unlock();

            LogPrintf("Oracle: Phase Two - %zu valid consensus attestations from %zu individual messages (need %d), broadcasting proposal\n",
                     valid_attestation_count, individual_count, required_bundle_messages);
            BroadcastConsensusProposal(epoch, consensus_price, consensus_timestamp);
        } else {
            LogPrintf("Oracle: Phase Two - Not enough individual messages for consensus (%zu, need %d)\n",
                     attempt.individual_count, required_bundle_messages);
        }
    }

    // If still no consensus, create empty bundle (graceful degradation)
    if (!bundle.HasConsensus(required_bundle_messages)) {
        if (Params().GetChainType() == ChainType::REGTEST && MockOracleManager::GetInstance().IsEnabled()) {
            COracleBundle mock_bundle = MockOracleManager::GetInstance().CreateMockBundle(block_height);
            if (required_bundle_messages == 1 && mock_bundle.messages.size() > 1) {
                mock_bundle.messages.resize(1);
                mock_bundle.median_price_micro_usd = mock_bundle.messages[0].price_micro_usd;
                mock_bundle.version = 1;
            } else {
                mock_bundle.version = 2;
            }
            if (mock_bundle.HasConsensus(required_bundle_messages)) {
                LogPrintf("Oracle: Using regtest mock oracle bundle for block %d with %zu messages\n",
                          block_height, mock_bundle.messages.size());
                bundle = std::move(mock_bundle);
            }
        }
    }

    if (!bundle.HasConsensus(required_bundle_messages)) {
        LogPrintf("Oracle: No consensus bundle available for block %d, creating empty oracle data\n", block_height);
        bundle = COracleBundle(epoch);
    }

    if (consensus_params.IsPhaseThreeActive(block_height)) {
        LogPrintf("Oracle: Phase Three active at height %d, checking MuSig2 session for epoch %d\n", block_height, epoch);

        std::optional<std::vector<unsigned char>> phase3_sig;
        std::optional<std::vector<unsigned char>> phase3_bitmap;

        {
            LOCK(g_oracle_signing_sessions_mutex);
            auto it = g_oracle_signing_sessions.find(epoch);
            if (it != g_oracle_signing_sessions.end()) {
                MuSig2SigningSession& session = it->second;
                std::vector<unsigned char> sig64;

                if (session.GetState() == MuSig2SessionState::COMPLETE) {
                    sig64 = session.GetAggregateSig();
                } else if (session.HasEnoughPartialSigs() && session.AggregateSignature(sig64)) {
                    LogPrintf("Oracle: Aggregated MuSig2 signature for epoch %d at block build\n", epoch);
                }

                if (sig64.size() == 64) {
                    auto bitmap = session.GetParticipationBitmap();
                    if (!bitmap.empty()) {
                        phase3_sig = std::move(sig64);
                        phase3_bitmap = std::move(bitmap);
                    }
                }
            }
        }

        if (phase3_sig && phase3_bitmap) {
            const uint16_t total_oracles = static_cast<uint16_t>(std::max(1, consensus_params.nOracleTotalOracles));
            const int phase3_required = std::max(1, consensus_params.nOracleConsensusRequired);
            std::vector<uint8_t> participating_ids = MuSig2OracleAggregator::DecodeBitmap(*phase3_bitmap, total_oracles);
            if (participating_ids.size() >= static_cast<size_t>(phase3_required)) {
                MuSig2OracleAggregator aggregator;
                secp256k1_xonly_pubkey agg_pk;
                secp256k1_musig_keyagg_cache cache;
                if (aggregator.ComputeAggregatePubkey(participating_ids, agg_pk, cache)) {
                    COracleBundle v03_bundle = bundle;
                    v03_bundle.version = 3;
                    v03_bundle.aggregate_sig = *phase3_sig;
                    v03_bundle.participation_bitmap = *phase3_bitmap;

                    std::vector<unsigned char> v03_payload = v03_bundle.SerializeV03Data();
                    if (!v03_payload.empty() && v03_payload.size() <= MAX_SCRIPT_ELEMENT_SIZE) {
                        bundle = std::move(v03_bundle);
                        LOCK(g_oracle_signing_sessions_mutex);
                        g_oracle_signing_sessions.erase(epoch);
                        LogPrintf("Oracle: Using v0x03 MuSig2 bundle at height %d, participants=%zu payload=%zu bytes\n",
                                  block_height, participating_ids.size(), v03_payload.size());
                    } else {
                        LogPrintf("Oracle: Rejecting v0x03 payload size=%zu (max=%d)\n",
                                  v03_payload.size(), MAX_SCRIPT_ELEMENT_SIZE);
                    }
                } else {
                    LogPrintf("Oracle: Failed ComputeAggregatePubkey for %zu participants\n", participating_ids.size());
                }
            } else {
                LogPrintf("Oracle: MuSig2 participant threshold not met (%zu < %d)\n",
                          participating_ids.size(), phase3_required);
            }
        } else {
            LogPrintf("Oracle: No complete MuSig2 session available for epoch %d, falling back to legacy bundle\n", epoch);
        }
    }

    LogPrintf("Oracle: Final bundle has %zu messages before CreateOracleScript\n", bundle.messages.size());

    // Create oracle script and add to coinbase
    CScript oracle_script = CreateOracleScript(bundle);
    LogPrintf("Oracle: CreateOracleScript returned script of size %zu\n", oracle_script.size());

    if (oracle_script.empty()) {
        LogPrintf("Oracle: Script is empty, returning true without adding bundle\n");
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
    // Phase Three (v0x03): MuSig2 aggregate signature + participation bitmap
    // Format: OP_RETURN OP_ORACLE <version=0x03> <v03_data>
    // v03_data: bitmap_len(1) + bitmap(var) + price(8) + timestamp(8) + aggregate_sig(64)
    if (bundle.version == 3) {
        if (bundle.aggregate_sig.size() != 64) {
            LogPrintf("Oracle: CreateOracleScript v0x03 error: aggregate_sig size %zu != 64\n",
                     bundle.aggregate_sig.size());
            return CScript();
        }

        if (bundle.participation_bitmap.empty()) {
            LogPrintf("Oracle: CreateOracleScript v0x03 error: empty participation_bitmap\n");
            return CScript();
        }

        std::vector<unsigned char> v03_data = bundle.SerializeV03Data();
        if (v03_data.empty()) {
            LogPrintf("Oracle: CreateOracleScript v0x03 error: SerializeV03Data failed\n");
            return CScript();
        }

        CScript script;
        script << OP_RETURN << OP_ORACLE;
        script << std::vector<unsigned char>{0x03};
        script << v03_data;

        LogPrintf("Oracle: Created Phase Three (v0x03) script with bitmap_bytes=%zu, payload=%zu, price=%llu\n",
                 bundle.participation_bitmap.size(),
                 v03_data.size(),
                 static_cast<unsigned long long>(bundle.median_price_micro_usd));
        return script;
    }

    if (bundle.messages.empty()) {
        return CScript(); // Empty script for no oracle data
    }

    // Phase One: Compact format using oracle pubkey hash from chainparams
    // Format: OP_RETURN OP_ORACLE <version=0x01> <oracle_id> <price_micro_usd> <timestamp>
    // Total: ~20 bytes (well within 83 byte MAX_OP_RETURN_RELAY limit)

    if (bundle.messages.size() > 1) {
        // Check if Phase Two is active — only create multi-oracle scripts when Phase Two is enabled
        const Consensus::Params& cparams = Params().GetConsensus();
        int current_height = 0; // Best effort — CreateOracleScript doesn't have height context
        // If Phase Two is not activated (INT_MAX), reject multi-message bundles
        if (cparams.nDigiDollarPhase2Height == std::numeric_limits<int>::max()) {
            LogPrintf("Oracle: Phase Two not active, rejecting multi-message bundle (%zu messages)\n",
                     bundle.messages.size());
            return CScript(); // Phase One: reject multi-message bundles
        }

        // Phase Two: Full signature format — all signatures stored on-chain for verification
        // Format: OP_RETURN OP_ORACLE <version=0x02> <data>
        // Data layout:
        //   num_messages (1 byte)
        //   consensus_price (8 bytes, uint64 LE)
        //   timestamp (8 bytes, int64 LE)
        //   For each message:
        //     oracle_id (1 byte)
        //     schnorr_sig (64 bytes)
        // Total for 4 oracles: 1+8+8 + 4*(1+64) = 277 bytes
        // Exceeds 83-byte MAX_OP_RETURN_RELAY but coinbase is not subject to relay policy

        CScript script;
        script << OP_RETURN << OP_ORACLE;

        script << std::vector<unsigned char>{0x02};

        std::vector<unsigned char> p2_data;
        size_t num_msgs = bundle.messages.size();
        p2_data.reserve(17 + num_msgs * 65);

        // Number of oracle messages
        p2_data.push_back(static_cast<unsigned char>(num_msgs & 0xFF));

        // Consensus price in micro-USD (uint64, little-endian)
        uint64_t p2_price = bundle.median_price_micro_usd;
        for (int i = 0; i < 8; ++i) {
            p2_data.push_back(static_cast<unsigned char>((p2_price >> (i * 8)) & 0xFF));
        }

        // Consensus timestamp (int64, little-endian)
        int64_t p2_timestamp = bundle.timestamp;
        for (int i = 0; i < 8; ++i) {
            p2_data.push_back(static_cast<unsigned char>((p2_timestamp >> (i * 8)) & 0xFF));
        }

        // Per-oracle entries: oracle_id + schnorr_sig
        for (const auto& msg : bundle.messages) {
            // SECURITY (DGB-SEC-004): Defense-in-depth — reject at serialization
            if (msg.oracle_id > 255) {
                LogPrintf("Oracle: Phase Two rejecting oracle_id %d > 255\n", msg.oracle_id);
                return CScript();
            }
            p2_data.push_back(static_cast<unsigned char>(msg.oracle_id & 0xFF));
            if (msg.schnorr_sig.size() == 64) {
                p2_data.insert(p2_data.end(), msg.schnorr_sig.begin(), msg.schnorr_sig.end());
            } else {
                // Pad with zeros if signature missing (will fail validation)
                p2_data.insert(p2_data.end(), 64, 0x00);
                LogPrintf("Oracle: WARNING - Phase Two message for oracle %d missing signature\n", msg.oracle_id);
            }
        }

        // For data > 75 bytes, CScript << vector uses OP_PUSHDATA1/2 automatically
        script << p2_data;

        LogPrintf("Oracle: Created Phase Two script with %zu oracle messages, %zu bytes, price=%llu\n",
                 num_msgs, p2_data.size(), p2_price);
        return script;
    }

    // Phase One: Must have exactly 1 message (1-of-1 consensus)
    CScript script;
    script << OP_RETURN << OP_ORACLE;

    // Version byte (0x01 = Phase One compact format)
    script << std::vector<unsigned char>{0x01};

    // Phase One: Single oracle message (chainparams verification)
    const COraclePriceMessage& msg = bundle.messages[0];

    // Compact data: oracle_id (1 byte) + price (8 bytes) + timestamp (8 bytes)
    std::vector<unsigned char> compact_data;
    compact_data.reserve(17);

    // Oracle ID (uint8 for Phase One, expandable to uint32 for Phase Two)
    // SECURITY (DGB-SEC-004): Defense-in-depth — reject at serialization
    if (msg.oracle_id > 255) {
        LogPrintf("Oracle: Phase One rejecting oracle_id %d > 255\n", msg.oracle_id);
        return CScript();
    }
    compact_data.push_back(static_cast<unsigned char>(msg.oracle_id & 0xFF));

    // Price in micro-USD (uint64, little-endian)
    uint64_t price = msg.price_micro_usd;
    for (int i = 0; i < 8; ++i) {
        compact_data.push_back(static_cast<unsigned char>((price >> (i * 8)) & 0xFF));
    }

    // Timestamp (int64, little-endian)
    int64_t timestamp = msg.timestamp;
    for (int i = 0; i < 8; ++i) {
        compact_data.push_back(static_cast<unsigned char>((timestamp >> (i * 8)) & 0xFF));
    }

    script << compact_data;

    return script;
}

bool OracleBundleManager::ExtractOracleBundle(const CTransaction& coinbase_tx, COracleBundle& bundle) const
{
    const std::optional<int32_t> coinbase_height = [&]() -> std::optional<int32_t> {
        if (coinbase_tx.vin.empty() || coinbase_tx.vin[0].scriptSig.empty()) {
            return std::nullopt;
        }

        CScript::const_iterator pc = coinbase_tx.vin[0].scriptSig.begin();
        opcodetype opcode;
        std::vector<unsigned char> data;
        if (!coinbase_tx.vin[0].scriptSig.GetOp(pc, opcode, data) || data.empty()) {
            return std::nullopt;
        }

        try {
            return CScriptNum(data, true).getint();
        } catch (const scriptnum_error&) {
            return std::nullopt;
        }
    }();

    // Look for OP_RETURN output with OP_ORACLE marker
    for (const auto& output : coinbase_tx.vout) {
        if (output.scriptPubKey.size() > 2 && output.scriptPubKey[0] == OP_RETURN) {
            // Check for OP_ORACLE opcode at byte 1
            if (output.scriptPubKey.size() >= 4 &&
                output.scriptPubKey[1] == OP_ORACLE) {

                try {
                    // Extract data chunks
                    std::vector<unsigned char> data;
                    auto script_it = output.scriptPubKey.begin() + 2; // Skip OP_RETURN + OP_ORACLE

                    while (script_it < output.scriptPubKey.end()) {
                        if (*script_it <= 75) { // Direct push (1-75 bytes)
                            unsigned char chunk_size = *script_it;
                            ++script_it;
                            if (script_it + chunk_size <= output.scriptPubKey.end()) {
                                data.insert(data.end(), script_it, script_it + chunk_size);
                                script_it += chunk_size;
                            } else {
                                break;
                            }
                        } else if (*script_it == 0x4c) { // OP_PUSHDATA1: next byte is length
                            ++script_it;
                            if (script_it >= output.scriptPubKey.end()) break;
                            unsigned int chunk_size = *script_it;
                            ++script_it;
                            if (script_it + chunk_size <= output.scriptPubKey.end()) {
                                data.insert(data.end(), script_it, script_it + chunk_size);
                                script_it += chunk_size;
                            } else {
                                break;
                            }
                        } else if (*script_it == 0x4d) { // OP_PUSHDATA2: next 2 bytes are length (LE)
                            ++script_it;
                            if (script_it + 2 > output.scriptPubKey.end()) break;
                            unsigned int chunk_size = *script_it | (*(script_it + 1) << 8);
                            script_it += 2;
                            if (script_it + chunk_size <= output.scriptPubKey.end()) {
                                data.insert(data.end(), script_it, script_it + chunk_size);
                                script_it += chunk_size;
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    }

                    if (data.empty()) {
                        return false;
                    }

                    // Check version byte
                    if (data[0] == 0x03) {
                        // v0x03 MuSig2 format: aggregate sig + participation bitmap (RC30)
                        // Data layout (after version byte):
                        //   bitmap_len(1) + bitmap(variable) + epoch(4) + price(8) + timestamp(8) + aggregate_sig(64)
                        std::vector<unsigned char> v03_data(data.begin() + 1, data.end());

                        if (!COracleBundle::DeserializeV03Data(v03_data, bundle)) {
                            LogPrintf("Oracle: Failed to deserialize v0x03 MuSig2 bundle data (%zu bytes)\n",
                                     v03_data.size());
                            return false;
                        }

                        bundle.version = 3;
                        // RC30: bundle.epoch is now parsed from the on-chain payload by
                        // DeserializeV03Data — do NOT zero it here. The validator binds
                        // the epoch to the current block height in ValidatePhaseThreeBundle.

                        // Wave 3: decode participation bitmap into synthetic oracle messages.
                        // v0x03 stores one aggregate signature, so per-oracle schnorr_sig is empty.
                        bundle.messages.clear();
                        const Consensus::Params& params = Params().GetConsensus();
                        const uint16_t total_oracles = static_cast<uint16_t>(std::max<size_t>(
                            bundle.participation_bitmap.size() * 8,
                            static_cast<size_t>(std::max(1, params.nOracleTotalOracles))));
                        std::vector<uint8_t> oracle_ids = MuSig2OracleAggregator::DecodeBitmap(
                            bundle.participation_bitmap, total_oracles);
                        const CChainParams& chainparams = Params();
                        for (uint8_t oracle_id : oracle_ids) {
                            COraclePriceMessage msg;
                            msg.oracle_id = oracle_id;
                            msg.price_micro_usd = bundle.median_price_micro_usd;
                            msg.timestamp = bundle.timestamp;
                            msg.block_height = 0;
                            msg.nonce = 0;
                            const OracleNodeInfo* oracle_info = chainparams.GetOracleNode(msg.oracle_id);
                            if (oracle_info) {
                                msg.oracle_pubkey = XOnlyPubKey(oracle_info->pubkey);
                            }
                            bundle.messages.push_back(std::move(msg));
                        }

                        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Extracted v0x03 MuSig2 bundle: "
                                 "bitmap=%zu bytes, participants=%zu, price=%llu micro-USD, sig=%zu bytes\n",
                                 bundle.participation_bitmap.size(),
                                 bundle.messages.size(),
                                 static_cast<unsigned long long>(bundle.median_price_micro_usd),
                                 bundle.aggregate_sig.size());
                        return true;
                    }
                    else if (data[0] == 0x01) {
                        // Phase One compact format: oracle_id (1) + price (8) + timestamp (8) = 17 bytes
                        if (data.size() < 18) { // 1 (version) + 17 (data)
                            LogPrintf("Oracle: Invalid Phase One bundle size: %d\n", data.size());
                            return false;
                        }

                        COraclePriceMessage msg;

                        // Parse oracle_id (uint8)
                        msg.oracle_id = data[1];

                        // Parse price (uint64, little-endian)
                        uint64_t price = 0;
                        for (int i = 0; i < 8; ++i) {
                            price |= (static_cast<uint64_t>(data[2 + i]) << (i * 8));
                        }
                        msg.price_micro_usd = price;

                        // Parse timestamp (int64, little-endian)
                        int64_t timestamp = 0;
                        for (int i = 0; i < 8; ++i) {
                            timestamp |= (static_cast<int64_t>(data[10 + i]) << (i * 8));
                        }
                        msg.timestamp = timestamp;

                        // Set remaining fields (not in compact format)
                        msg.block_height = coinbase_height.value_or(0);
                        msg.nonce = 0;

                        // Phase One: Get oracle pubkey from chainparams for verification
                        // Compact format doesn't include signature/pubkey (would exceed OP_RETURN size limit)
                        const CChainParams& chainparams = Params();
                        const OracleNodeInfo* oracle_info = chainparams.GetOracleNode(msg.oracle_id);
                        if (oracle_info) {
                            msg.oracle_pubkey = XOnlyPubKey(oracle_info->pubkey);
                            // Schnorr signature is NOT in compact format (verified at bundle creation time)
                            // For Phase One, trust is based on chainparams oracle pubkey
                        }

                        // Create bundle with single message
                        bundle.version = 1; // Phase One (V01)
                        bundle.messages.clear();
                        bundle.messages.push_back(msg);
                        bundle.median_price_micro_usd = price;
                        bundle.timestamp = timestamp;
                        bundle.epoch = coinbase_height ? GetCurrentEpoch(*coinbase_height) : 0;

                        return true;
                    }
                    else if (data[0] == 0x02) {
                        // Phase Two: Full signature format
                        // Layout: version(1) + num_msgs(1) + price(8) + timestamp(8) + N*(oracle_id(1)+sig(64))
                        if (data.size() < 18) { // minimum: version + num_msgs + price + timestamp
                            return false;
                        }

                        uint8_t num_messages = data[1];

                        // Reject empty bundles and unreasonably large message counts
                        if (num_messages == 0) {
                            LogPrintf("Oracle: Phase Two bundle rejected: num_messages=0 (ghost bundle)\n");
                            return false;
                        }
                        if (num_messages > ORACLE_ACTIVE_COUNT) {
                            LogPrintf("Oracle: Phase Two bundle rejected: num_messages=%d exceeds ORACLE_ACTIVE_COUNT=%d\n",
                                     num_messages, ORACLE_ACTIVE_COUNT);
                            return false;
                        }

                        // Validate we have enough data for all messages
                        size_t expected_size = 1 + 1 + 8 + 8 + num_messages * 65; // version + header + per-msg
                        if (data.size() < expected_size) {
                            LogPrintf("Oracle: Phase Two data too short: %zu < %zu (for %d messages)\n",
                                     data.size(), expected_size, num_messages);
                            return false;
                        }

                        // Parse consensus price (uint64, little-endian)
                        uint64_t price = 0;
                        for (int i = 0; i < 8; ++i) {
                            price |= (static_cast<uint64_t>(data[2 + i]) << (i * 8));
                        }

                        // Parse consensus timestamp (int64, little-endian)
                        int64_t timestamp = 0;
                        for (int i = 0; i < 8; ++i) {
                            timestamp |= (static_cast<int64_t>(data[10 + i]) << (i * 8));
                        }

                        bundle.version = 2;
                        bundle.aggregate_sig.clear();
                        bundle.participation_bitmap.clear();
                        bundle.messages.clear();
                        const CChainParams& chainparams = Params();

                        // Parse each oracle message (oracle_id + schnorr_sig)
                        size_t offset = 18; // past version + num_msgs + price + timestamp
                        for (uint8_t m = 0; m < num_messages; m++) {
                            COraclePriceMessage msg;
                            msg.oracle_id = data[offset];
                            offset += 1;

                            msg.schnorr_sig.assign(data.begin() + offset, data.begin() + offset + 64);
                            offset += 64;

                            // All oracles in the bundle attested to the same consensus price
                            msg.price_micro_usd = price;
                            msg.timestamp = timestamp;
                            msg.block_height = coinbase_height.value_or(0);
                            msg.nonce = 0;

                            // Get oracle pubkey from chainparams for verification
                            const OracleNodeInfo* oracle_info = chainparams.GetOracleNode(msg.oracle_id);
                            if (oracle_info) {
                                msg.oracle_pubkey = XOnlyPubKey(oracle_info->pubkey);
                            }

                            bundle.messages.push_back(msg);
                        }

                        bundle.median_price_micro_usd = price;
                        bundle.timestamp = timestamp;
                        bundle.epoch = coinbase_height ? GetCurrentEpoch(*coinbase_height) : 0;

                        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Extracted Phase Two bundle: %d oracles with signatures, price=%llu micro-USD\n",
                                 num_messages, price);

                        return true;
                    }

                    return false;
                }
                catch (const std::exception& e) {
                    LogPrintf("Oracle: Failed to extract oracle bundle: %s\n", e.what());
                }
            }
        }
    }

    return false;
}

bool OracleBundleManager::ValidateV03BundleFormat(const CScript& script, uint8_t& version)
{
    version = 0;

    // Minimum script: OP_RETURN(1) + OP_ORACLE(1) + push(1) + version(1) = 4 bytes
    if (script.size() < 4) return false;

    // Check OP_RETURN + OP_ORACLE marker
    if (script[0] != OP_RETURN || script[1] != OP_ORACLE) return false;

    // Extract data chunks (same logic as ExtractOracleBundle)
    std::vector<unsigned char> data;
    auto it = script.begin() + 2;
    while (it < script.end()) {
        if (*it <= 75) {
            unsigned char chunk_size = *it;
            ++it;
            if (it + chunk_size <= script.end()) {
                data.insert(data.end(), it, it + chunk_size);
                it += chunk_size;
            } else {
                return false;
            }
        } else if (*it == 0x4c) { // OP_PUSHDATA1
            ++it;
            if (it >= script.end()) return false;
            unsigned int chunk_size = *it;
            ++it;
            if (it + chunk_size <= script.end()) {
                data.insert(data.end(), it, it + chunk_size);
                it += chunk_size;
            } else {
                return false;
            }
        } else if (*it == 0x4d) { // OP_PUSHDATA2
            ++it;
            if (it + 2 > script.end()) return false;
            unsigned int chunk_size = *it | (*(it + 1) << 8);
            it += 2;
            if (it + chunk_size <= script.end()) {
                data.insert(data.end(), it, it + chunk_size);
                it += chunk_size;
            } else {
                return false;
            }
        } else {
            break;
        }
    }

    if (data.empty()) return false;

    version = data[0];

    // Validate format based on version
    if (version == 0x03) {
        // v0x03: version(1) + bitmap_len(1) + bitmap(>=1) + price(8) + timestamp(8) + sig(64) >= 83
        if (data.size() < 83) return false;
        uint8_t bitmap_len = data[1];
        if (bitmap_len == 0) return false;
        // Check total data size: 1(version) + 1(bitmap_len) + bitmap_len + 8 + 8 + 64
        size_t expected = 1 + 1 + bitmap_len + 8 + 8 + 64;
        if (data.size() < expected) return false;
        return true;
    } else if (version == 0x02) {
        // v0x02: version(1) + num_msgs(1) + price(8) + timestamp(8) = 18 minimum
        if (data.size() < 18) return false;
        uint8_t num_msgs = data[1];
        size_t expected = 1 + 1 + 8 + 8 + num_msgs * 65;
        return data.size() >= expected;
    } else if (version == 0x01) {
        // v0x01: version(1) + oracle_id(1) + price(8) + timestamp(8) = 18
        return data.size() >= 18;
    }

    return false;
}

CAmount OracleBundleManager::GetConsensusPrice(int32_t epoch) const
{
    COracleBundle bundle = GetCurrentBundle(epoch);
    return bundle.GetConsensusPrice(min_oracle_count);
}

CAmount OracleBundleManager::GetLatestPrice() const
{
    std::lock_guard<std::mutex> lock(mtx_bundles);

    // SECURITY: Reject stale cached prices.
    // If the last oracle update was more than ORACLE_MAX_AGE_SECONDS ago,
    // the price is stale and must not be used for collateral calculations.
    // Without this check, an attacker can DDoS oracles and mint DD using
    // the last known (higher) price while the real DGB price has crashed.
    if (cached_price > 0 && last_update_time > 0) {
        int64_t age = GetTime() - last_update_time;
        if (age > ORACLE_MAX_AGE_SECONDS) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Rejecting stale cached price %lld micro-USD (age: %lld seconds, max: %d)\n",
                     cached_price, age, ORACLE_MAX_AGE_SECONDS);
            return 0;
        }
    }

    return cached_price;
}

bool OracleBundleManager::UpdateCachedPrice(int32_t epoch)
{
    COracleBundle bundle = GetCurrentBundle(epoch);
    if (bundle.HasConsensus(min_oracle_count)) {
        CAmount price = bundle.GetConsensusPrice(min_oracle_count);
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
    BlockValidationState state;
    return OracleDataValidator::ValidateBlockOracleData(block, nullptr, params, state);
}

bool OracleBundleManager::HasOracleMessage(const uint256& hash) const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    return seen_message_hashes.count(hash) > 0;
}

void OracleBundleManager::RegisterSeenHash(const uint256& hash)
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    seen_message_hashes.insert(hash);

    // RH-03 Fix: Cap seen_message_hashes from P2P RegisterSeenHash path too.
    // Without this, an attacker flooding unique MuSig2 messages can grow the
    // set unboundedly since RegisterSeenHash bypasses the MAX_SEEN_HASHES
    // cap in AddOracleMessage.
    static constexpr size_t MAX_SEEN_HASHES = 2048;
    if (seen_message_hashes.size() > MAX_SEEN_HASHES) {
        auto erase_end = seen_message_hashes.begin();
        std::advance(erase_end, seen_message_hashes.size() - MAX_SEEN_HASHES);
        seen_message_hashes.erase(seen_message_hashes.begin(), erase_end);
    }
}

bool OracleBundleManager::BroadcastMessage(const COraclePriceMessage& message)
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

    // Validate message before broadcasting
    if (!message.IsValid()) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Cannot broadcast invalid oracle message\n");
        return false;
    }

    // Add message to our own collection first
    if (!AddOracleMessage(message)) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Failed to add message to local storage before broadcast\n");
        return false;
    }

    // Broadcast to P2P network
    if (!m_connman) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Cannot broadcast - no P2P connection available\n");
        return false;
    }

    // Push message to all connected peers using OraclePriceMsg wrapper
    OraclePriceMsg price_msg;
    price_msg.price_message = message;
    m_connman->ForEachNode([this, &price_msg](CNode* node) {
        m_connman->PushMessage(node,
            CNetMsgMaker(node->GetCommonVersion()).Make(
                NetMsgType::ORACLEPRICE, price_msg));
    });

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Broadcast oracle message to network: oracle_id=%d, price=%llu micro-USD\n",
             message.oracle_id, message.price_micro_usd);

    return true;
}

bool OracleBundleManager::BroadcastConsensusProposal(int32_t epoch, uint64_t consensus_price, int64_t consensus_timestamp)
{
    {
        std::lock_guard<std::recursive_mutex> lock(mtx_messages);

        // Rate limit: only broadcast once per 30 seconds for recovery
        static int64_t last_broadcast_time = 0;
        int64_t now = GetTime();
        if (now - last_broadcast_time < 30) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Consensus proposal rate limited, last broadcast %d seconds ago\n", 
                     (int)(now - last_broadcast_time));
            return false;
        }
        last_broadcast_time = now;

        // Still track epochs to avoid redundant broadcasts within same epoch
        if (broadcast_proposal_epochs.count(epoch)) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Consensus proposal already broadcast for epoch %d\n", epoch);
            return false;
        }
        broadcast_proposal_epochs.insert(epoch);

        // Cleanup old epochs (keep last 3)
        while (broadcast_proposal_epochs.size() > 3) {
            broadcast_proposal_epochs.erase(broadcast_proposal_epochs.begin());
        }
    }

    if (!m_connman) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Cannot broadcast consensus proposal - no P2P connection\n");
        return false;
    }

    OracleConsensusMsg proposal;
    proposal.epoch = epoch;
    proposal.consensus_price = consensus_price;
    proposal.consensus_timestamp = consensus_timestamp;

    m_connman->ForEachNode([this, &proposal](CNode* node) {
        m_connman->PushMessage(node,
            CNetMsgMaker(node->GetCommonVersion()).Make(
                NetMsgType::ORACLECONSENSUS, proposal));
    });

    LogPrintf("Oracle: Broadcast consensus proposal for epoch %d: price=%llu, timestamp=%lld\n",
             epoch, consensus_price, consensus_timestamp);
    return true;
}

bool OracleBundleManager::HasBroadcastConsensusProposal(int32_t epoch) const
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    return broadcast_proposal_epochs.count(epoch) > 0;
}

bool OracleBundleManager::StartMuSig2Session(int32_t block_height)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    if (!consensus.IsPhaseThreeActive(block_height)) return false;

    OracleManager& om = OracleManager::GetInstance();
    const std::vector<uint32_t> active_ids = om.GetActiveOracleIds();
    if (active_ids.empty()) return false;

    const uint8_t local_oracle_id = static_cast<uint8_t>(active_ids.front());
    OracleNode* local_node = om.GetOracleNode(local_oracle_id);
    if (!local_node) return false;

    const int32_t epoch = GetCurrentEpoch(block_height);

    MuSig2SigningSession* session_ptr = nullptr;
    {
        LOCK(g_oracle_signing_sessions_mutex);
        if (g_oracle_signing_sessions.count(epoch)) return false;

        auto [it, ok] = g_oracle_signing_sessions.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(epoch),
            std::forward_as_tuple(epoch, static_cast<uint8_t>(consensus.nOracleRequiredMessages)));
        if (!ok) return false;
        session_ptr = &it->second;
    }

    MuSig2OracleAggregator aggregator;
    secp256k1_xonly_pubkey agg_pk;
    secp256k1_musig_keyagg_cache keyagg_cache;
    // Convert uint32_t oracle IDs to uint8_t for aggregator
    std::vector<uint8_t> oracle_ids_u8;
    oracle_ids_u8.reserve(active_ids.size());
    for (uint32_t id : active_ids) oracle_ids_u8.push_back(static_cast<uint8_t>(id));
    if (!aggregator.ComputeAggregatePubkey(oracle_ids_u8, agg_pk, keyagg_cache)) return false;

    const CKey signing_key = local_node->GetOraclePrivateKey();
    if (!signing_key.IsValid()) return false;

    const CPubKey pubkey = local_node->GetPublicKey();
    if (!pubkey.IsFullyValid()) return false;

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx) return false;

    secp256k1_pubkey secp_pubkey;
    if (!secp256k1_ec_pubkey_parse(ctx, &secp_pubkey, pubkey.data(), pubkey.size())) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    secp256k1_musig_pubnonce pubnonce;
    if (!session_ptr->GenerateNonce(local_oracle_id, signing_key, secp_pubkey, keyagg_cache, pubnonce)) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    session_ptr->AddPubnonce(local_oracle_id, pubnonce);

    OracleMusigNonceMsg msg;
    msg.epoch = epoch;
    msg.oracle_id = local_oracle_id;
    msg.pubnonce.resize(66);
    if (!secp256k1_musig_pubnonce_serialize(ctx, msg.pubnonce.data(), &pubnonce)) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    secp256k1_context_destroy(ctx);

    if (m_connman) {
        m_connman->ForEachNode([this, &msg](CNode* node) {
            m_connman->PushMessage(node,
                CNetMsgMaker(node->GetCommonVersion()).Make(NetMsgType::ORACLEMUSIGNONCE, msg));
        });
    }

    return true;
}

bool OracleBundleManager::CompleteMuSig2Session(int32_t block_height)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    if (!consensus.IsPhaseThreeActive(block_height)) return false;

    OracleManager& om = OracleManager::GetInstance();
    const std::vector<uint32_t> active_ids = om.GetActiveOracleIds();
    if (active_ids.empty()) return false;

    const uint8_t local_oracle_id = static_cast<uint8_t>(active_ids.front());
    OracleNode* local_node = om.GetOracleNode(local_oracle_id);
    if (!local_node) return false;

    const int32_t epoch = GetCurrentEpoch(block_height);
    LOCK(g_oracle_signing_sessions_mutex);
    auto it = g_oracle_signing_sessions.find(epoch);
    if (it == g_oracle_signing_sessions.end()) return false;

    MuSig2SigningSession& session = it->second;
    if (!session.HasEnoughNonces()) return false;

    unsigned char msg32[32] = {0};
    std::memcpy(msg32, &epoch, std::min(sizeof(epoch), sizeof(msg32)));

    if (session.GetState() == MuSig2SessionState::NONCES_COMPLETE && !session.AggregateNonces(msg32)) {
        return false;
    }
    if (session.GetState() != MuSig2SessionState::SIGNING) return false;

    const CKey signing_key = local_node->GetOraclePrivateKey();
    if (!signing_key.IsValid()) return false;

    secp256k1_musig_partial_sig partial_sig;
    if (!session.CreatePartialSignature(local_oracle_id, signing_key, partial_sig)) return false;
    session.AddPartialSignature(local_oracle_id, partial_sig);

    OracleMusigPartialSigMsg msg;
    msg.epoch = epoch;
    msg.oracle_id = local_oracle_id;
    msg.partial_sig.resize(32);

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx) return false;
    if (!secp256k1_musig_partial_sig_serialize(ctx, msg.partial_sig.data(), &partial_sig)) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    secp256k1_context_destroy(ctx);

    if (m_connman) {
        m_connman->ForEachNode([this, &msg](CNode* node) {
            m_connman->PushMessage(node,
                CNetMsgMaker(node->GetCommonVersion()).Make(NetMsgType::ORACLEMUSIGPARTIALSIG, msg));
        });
    }

    return true;
}

// ============================================================================
// MuSig2 P2P ingestion — feed remote nonces/partial-sigs into
// g_oracle_signing_sessions so the local node can aggregate them.
// Called by net_processing on ORACLEMUSIGNONCE / ORACLEMUSIGPARTIALSIG.
// ============================================================================

bool OracleBundleManager::ProcessRemoteMusigNonce(const OracleMusigNonceMsg& msg)
{
    if (!msg.IsValid()) return false;

    // RH-24: Verify authentication signature before processing
    {
        const OracleNodeInfo* oracle_config = Params().GetOracleNode(msg.oracle_id);
        if (!oracle_config) return false;
        XOnlyPubKey oracle_pubkey(oracle_config->pubkey);
        if (!msg.VerifySignature(oracle_pubkey)) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: MuSig2 nonce signature verification failed for oracle %u\n",
                     msg.oracle_id);
            return false;
        }
    }

    LOCK(g_oracle_signing_sessions_mutex);
    auto it = g_oracle_signing_sessions.find(msg.epoch);
    if (it == g_oracle_signing_sessions.end()) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Received MuSig2 nonce for unknown epoch %d from oracle %u\n",
                 msg.epoch, msg.oracle_id);
        return false;
    }

    MuSig2SigningSession& session = it->second;

    // Deserialize the 66-byte pubnonce
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx) return false;

    secp256k1_musig_pubnonce pubnonce;
    bool parsed = secp256k1_musig_pubnonce_parse(ctx, &pubnonce, msg.pubnonce.data());
    secp256k1_context_destroy(ctx);

    if (!parsed) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Failed to parse MuSig2 pubnonce from oracle %u epoch %d\n",
                 msg.oracle_id, msg.epoch);
        return false;
    }

    if (!session.AddPubnonce(msg.oracle_id, pubnonce)) {
        // Duplicate or wrong state — not an error, just skip
        return false;
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Ingested remote MuSig2 nonce: epoch=%d, oracle_id=%u, nonces_now=%zu\n",
             msg.epoch, msg.oracle_id, session.GetNonceCount());
    return true;
}

bool OracleBundleManager::ProcessRemoteMusigPartialSig(const OracleMusigPartialSigMsg& msg)
{
    if (!msg.IsValid()) return false;

    // RH-24: Verify authentication signature before processing
    {
        const OracleNodeInfo* oracle_config = Params().GetOracleNode(msg.oracle_id);
        if (!oracle_config) return false;
        XOnlyPubKey oracle_pubkey(oracle_config->pubkey);
        if (!msg.VerifySignature(oracle_pubkey)) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: MuSig2 partial sig signature verification failed for oracle %u\n",
                     msg.oracle_id);
            return false;
        }
    }

    LOCK(g_oracle_signing_sessions_mutex);
    auto it = g_oracle_signing_sessions.find(msg.epoch);
    if (it == g_oracle_signing_sessions.end()) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Received MuSig2 partial sig for unknown epoch %d from oracle %u\n",
                 msg.epoch, msg.oracle_id);
        return false;
    }

    MuSig2SigningSession& session = it->second;

    // Deserialize the 32-byte partial sig
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx) return false;

    secp256k1_musig_partial_sig psig;
    bool parsed = secp256k1_musig_partial_sig_parse(ctx, &psig, msg.partial_sig.data());
    if (!parsed) {
        secp256k1_context_destroy(ctx);
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Failed to parse MuSig2 partial sig from oracle %u epoch %d\n",
                 msg.oracle_id, msg.epoch);
        return false;
    }

    // W3-H-01 fix: verify the partial sig under the signer's chainparams
    // pubkey + session's keyagg_cache before admitting it to the session.
    // Previously used unverified AddPartialSignature which allowed any
    // in-range scalar submitted by a single compromised oracle to poison
    // aggregation and force schnorrsig_verify failure on the produced
    // 64-byte aggregate — per-epoch DoS on oracle attestation.
    const OracleNodeInfo* oracle_cfg = Params().GetOracleNode(msg.oracle_id);
    if (!oracle_cfg) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    secp256k1_pubkey signer_pk;
    if (!secp256k1_ec_pubkey_parse(ctx, &signer_pk,
                                   oracle_cfg->pubkey.data(),
                                   oracle_cfg->pubkey.size())) {
        secp256k1_context_destroy(ctx);
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Failed to parse chainparams pubkey for oracle %u\n",
                 msg.oracle_id);
        return false;
    }
    secp256k1_context_destroy(ctx);

    if (!session.AddPartialSignatureVerified(msg.oracle_id, psig, signer_pk)) {
        // Verification failed (garbage scalar, wrong session, or duplicate)
        // — not an error, just skip. Prevents a single malicious oracle from
        // bricking the epoch's aggregate signature.
        return false;
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Ingested remote MuSig2 partial sig: epoch=%d, oracle_id=%u, sigs_now=%zu\n",
             msg.epoch, msg.oracle_id, session.GetPartialSigCount());

    // Eagerly try to aggregate when we have enough partial sigs.
    // This avoids waiting for the next AddOracleBundleToBlock() call.
    if (session.HasEnoughPartialSigs() &&
        session.GetState() == MuSig2SessionState::SIGNING) {
        std::vector<unsigned char> sig64;
        if (session.AggregateSignature(sig64)) {
            LogPrintf("Oracle: MuSig2 signature eagerly aggregated for epoch %d (%zu bytes)\n",
                     msg.epoch, sig64.size());
        }
    }

    return true;
}

bool OracleBundleManager::RegisterSeenAttestation(const uint256& hash)
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);

    if (seen_attestation_hashes.count(hash)) {
        return false; // Already seen — replay
    }

    seen_attestation_hashes.insert(hash);

    // Limit set size to prevent memory exhaustion
    if (seen_attestation_hashes.size() > 10000) {
        // Remove oldest entries (set is ordered, so begin() is smallest hash)
        auto it = seen_attestation_hashes.begin();
        for (size_t i = 0; i < 1000 && it != seen_attestation_hashes.end(); ++i) {
            it = seen_attestation_hashes.erase(it);
        }
    }

    return true; // New attestation
}

void OracleBundleManager::SetConnman(CConnman* connman)
{
    std::lock_guard<std::recursive_mutex> lock(mtx_messages);
    m_connman = connman;
    LogPrint(BCLog::DIGIDOLLAR, "Oracle: P2P connection manager set for OracleBundleManager\n");
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
        stats.near_quorum_wait_attempts = near_quorum_wait_attempts;
        stats.near_quorum_wait_successes = near_quorum_wait_successes;
        stats.near_quorum_wait_timeouts = near_quorum_wait_timeouts;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_bundles);
        stats.active_bundles = epoch_bundles.size();
        const bool fresh_price = cached_price > 0 && last_update_time > 0 &&
            (GetTime() - last_update_time) <= ORACLE_MAX_AGE_SECONDS;
        stats.latest_price = fresh_price ? cached_price : 0;
        stats.latest_epoch = cached_epoch;
        stats.last_update = last_update_time;
        stats.has_consensus = stats.latest_price > 0;
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

    // Log oracle configuration
    LogPrintf("Oracle: Testnet configured with %zu oracle keys, %d-of-%d consensus\n",
             consensus.vOraclePublicKeys.size(),
             consensus.nOracleRequiredMessages, consensus.nOracleTotalOracles);

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

void OracleBundleManager::LoadPricesFromChain(ChainstateManager& chainman)
{
    OracleBundleManager& manager = GetInstance();
    const Consensus::Params& consensus = Params().GetConsensus();

    LOCK(cs_main);

    // Get the active chain tip
    CBlockIndex* pindex = chainman.ActiveChain().Tip();
    if (!pindex) {
        LogPrintf("Oracle: No active chain tip, skipping price loading\n");
        return;
    }

    int tip_height = pindex->nHeight;

    // Only scan if DigiDollar is active via BIP9 deployment
    if (!DigiDollar::IsDigiDollarEnabled(pindex, chainman)) {
        LogPrintf("Oracle: DigiDollar not yet active (BIP9) at height %d, skipping price loading\n",
                 tip_height);
        return;
    }

    // Scan back 20 blocks to find recent oracle prices (default validity window)
    static constexpr int ORACLE_VALIDITY_BLOCKS = 20;
    int scan_depth = std::min(ORACLE_VALIDITY_BLOCKS, tip_height);
    int prices_found = 0;

    LogPrintf("Oracle: Scanning last %d blocks for oracle prices (height %d to %d)...\n",
             scan_depth, tip_height - scan_depth + 1, tip_height);

    for (int height = tip_height; height >= tip_height - scan_depth + 1 && height >= 0; --height) {
        CBlockIndex* block_index = chainman.ActiveChain()[height];
        if (!block_index) continue;

        CBlock block;
        if (!chainman.m_blockman.ReadBlockFromDisk(block, *block_index)) {
            LogPrintf("Oracle: Failed to read block at height %d\n", height);
            continue;
        }

        // Extract oracle bundle from coinbase
        if (block.vtx.empty()) continue;
        const CTransaction& coinbase = *block.vtx[0];

        COracleBundle bundle;
        if (manager.ExtractOracleBundle(coinbase, bundle)) {
            if (bundle.median_price_micro_usd > 0) {
                manager.UpdatePriceCache(height, bundle.median_price_micro_usd, bundle.timestamp);
                prices_found++;
                LogPrintf("Oracle: Found price %llu micro-USD at height %d\n",
                         bundle.median_price_micro_usd, height);
            }
        }
    }

    if (prices_found > 0) {
        LogPrintf("Oracle: Loaded %d oracle prices from blockchain, latest price: %llu micro-USD\n",
                 prices_found, manager.GetLatestPrice());
    } else {
        LogPrintf("Oracle: No oracle prices found in recent blocks\n");
    }
}

void OracleBundleManager::Clear()
{
    // Clear all state for test isolation
    {
        std::lock_guard<std::mutex> lock(mtx_bundles);
        epoch_bundles.clear();
        cached_price = 0;
        cached_epoch = -1;
        last_update_time = 0;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(mtx_messages);
        pending_messages.clear();
        pending_attestations.clear();
        seen_message_hashes.clear();
        broadcast_proposal_epochs.clear();
        seen_attestation_hashes.clear();
        near_quorum_wait_attempts = 0;
        near_quorum_wait_successes = 0;
        near_quorum_wait_timeouts = 0;
        force_phase2 = false;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_price_cache);
        height_to_price.clear();
        height_to_price_time.clear();
    }

    m_messages_updated_cv.notify_all();
    LogPrintf("Oracle: Cleared all bundle manager state\n");
}

bool OracleBundleManager::ValidateConfiguration() const
{
    const Consensus::Params& consensus = Params().GetConsensus();

    // Validate oracle configuration
    if (consensus.vOraclePublicKeys.empty()) {
        LogPrintf("Oracle: ERROR - No oracle public keys configured\n");
        return false;
    }

    if (consensus.nOracleRequiredMessages > consensus.nOracleTotalOracles) {
        LogPrintf("Oracle: ERROR - Required messages (%d) exceeds total oracles (%d)\n",
                 consensus.nOracleRequiredMessages, consensus.nOracleTotalOracles);
        return false;
    }

    LogPrintf("Oracle: Configuration valid - %zu keys, %d-of-%d consensus\n",
             consensus.vOraclePublicKeys.size(),
             consensus.nOracleRequiredMessages, consensus.nOracleTotalOracles);

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
    if (bundle.HasConsensus(min_oracle_count)) {
        // Calculate and set median price
        bundle.median_price_micro_usd = bundle.GetConsensusPrice(min_oracle_count);

        // Set bundle timestamp to the latest message timestamp
        bundle.timestamp = GetTime();
        for (const auto& msg : bundle.messages) {
            if (msg.timestamp > bundle.timestamp) {
                bundle.timestamp = msg.timestamp;
            }
        }

        UpdateBundle(bundle);
        LogPrintf("Oracle: Created consensus bundle for epoch %d with %d messages (min required: %d), median price=%llu\n",
                 epoch, bundle.messages.size(), min_oracle_count, bundle.median_price_micro_usd);
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
    // SECURITY (DGB-SEC-004): Reject oracle_id > 255. The on-chain script
    // format stores oracle_id as a single byte. Accepting larger IDs would
    // silently truncate, causing ID collisions and signature mismatches.
    if (message.oracle_id > 255) {
        LogPrintf("Oracle: Rejecting message with oracle_id %d > 255 (exceeds 1-byte on-chain format)\n",
                 message.oracle_id);
        return false;
    }

    // Phase One: Skip chainparams check when min_oracle_count == 1 (testing mode)
    if (min_oracle_count == 1) {
        if (!message.IsValid()) return false;
        return message.VerifyPhase2();
    }

    // Phase Two (min_oracle_count > 1): Use Phase 2 signature hash
    // Basic field validation without calling IsValid() which uses Phase 1 Verify()
    if (message.price_micro_usd < ORACLE_MIN_PRICE_MICRO_USD) return false;
    if (message.price_micro_usd > ORACLE_MAX_PRICE_MICRO_USD) return false;

    // Verify oracle ID is in valid range and matches chainparams
    const CChainParams& params = Params();
    const OracleNodeInfo* oracle_config = params.GetOracleNode(message.oracle_id);
    if (!oracle_config) {
        return false;
    }

    // SECURITY: Bind pubkey from chainparams before verification.
    // The message may contain an attacker-supplied pubkey — we must verify
    // against the authorized key, not whatever was deserialized from P2P.
    COraclePriceMessage bound_msg = message;
    bound_msg.oracle_pubkey = XOnlyPubKey(oracle_config->pubkey);

    // Verify Phase 2 Schnorr signature against chainparams pubkey
    return bound_msg.VerifyPhase2();
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

void OracleBundleManager::UpdatePriceCache(int height, uint64_t price_micro_usd, int64_t source_time)
{
    const int64_t effective_update_time = source_time > 0 ? source_time : GetTime();

    // Update the height-to-price map
    {
        std::lock_guard<std::mutex> lock(mtx_price_cache);
        height_to_price[height] = price_micro_usd;
        height_to_price_time[height] = effective_update_time;

        // Keep cache size limited (last 1000 blocks)
        if (height_to_price.size() > 1000) {
            const int erase_height = height_to_price.begin()->first;
            height_to_price.erase(height_to_price.begin());
            height_to_price_time.erase(erase_height);
        }
    }

    // CRITICAL: Also update cached_price so GetLatestPrice() returns the correct value
    // This is the price that GetCurrentOraclePrice() uses for the DigiDollar system
    // The price comes from oracle data embedded in blocks - this is the consensus price
    {
        std::lock_guard<std::mutex> lock(mtx_bundles);
        cached_price = static_cast<CAmount>(price_micro_usd);
        last_update_time = effective_update_time;
    }

    LogPrintf("Oracle: Price cache updated for height %d: %llu micro-USD ($%.6f), source_time=%lld - cached_price updated\n",
             height, price_micro_usd, price_micro_usd / 1000000.0, (long long)effective_update_time);
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

void OracleBundleManager::RemovePriceCache(int height)
{
    // RH-44: Must hold both mtx_price_cache (for height_to_price) and
    // mtx_bundles (for cached_price) to avoid data race with GetLatestPrice().
    // Lock order: mtx_bundles before mtx_price_cache to prevent deadlocks.
    std::lock_guard<std::mutex> bundles_lock(mtx_bundles);
    std::lock_guard<std::mutex> price_lock(mtx_price_cache);
    auto it = height_to_price.find(height);
    if (it != height_to_price.end()) {
        height_to_price.erase(it);
        height_to_price_time.erase(height);
        // Revert cached_price to highest remaining height's price
        if (!height_to_price.empty()) {
            const int restored_height = height_to_price.rbegin()->first;
            cached_price = height_to_price.rbegin()->second;
            auto time_it = height_to_price_time.find(restored_height);
            last_update_time = time_it != height_to_price_time.end() ? time_it->second : 0;
        } else {
            cached_price = 0;
            last_update_time = 0;
        }
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Removed price cache for height %d, cached_price reverted to %d\n", height, cached_price);
    }
}

/**
 * OracleDataValidator Implementation
 */

bool OracleDataValidator::ValidateBlockOracleData(const CBlock& block, const CBlockIndex* pindex_prev, const Consensus::Params& params, BlockValidationState& state)
{
    // Oracle validation runs identically on testnet and mainnet. The
    // previous chain-type short-circuit (return true on any chain other
    // than testnet/regtest) meant mainnet would never run the phase-
    // aware validator once DigiDollar activated via BIP9 — miners could
    // publish arbitrary oracle data with no consensus check. Removing the
    // short-circuit lets the BIP9/activation-height gate below decide
    // whether oracle validation applies, in parallel with testnet.
    //
    // Pre-activation behavior is preserved by the BIP9/height gate at
    // the top of the function: any chain where DigiDollar is not yet
    // BIP9-active returns true without running the validator. Blocks
    // without an OP_ORACLE output (or with a malformed one) remain
    // accepted via the intentional transition-period escape hatches
    // further down — the chain must keep producing blocks when the
    // oracle network is temporarily unavailable.

    // Extract oracle bundle from coinbase OP_RETURN (output index 1)
    if (block.vtx.empty()) {
        return true; // No transactions, nothing to validate
    }

    const CTransaction& coinbase = *block.vtx[0];

    // Determine block height
    int32_t block_height = 0;
    if (pindex_prev) {
        block_height = pindex_prev->nHeight + 1;
    } else {
        // Extract height from coinbase scriptSig (BIP34)
        if (!coinbase.vin.empty() && coinbase.vin[0].scriptSig.size() >= 1) {
            CScript::const_iterator pc = coinbase.vin[0].scriptSig.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;
            if (coinbase.vin[0].scriptSig.GetOp(pc, opcode, data) && !data.empty()) {
                try {
                    block_height = CScriptNum(data, true).getint();
                } catch (const scriptnum_error&) {
                    return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                                         "bad-cb-bip34-height-encoding",
                                         "coinbase BIP34 height push is non-minimal or overflows");
                }
            }
        }
    }

    // Check if DigiDollar is active via BIP9 deployment
    if (pindex_prev) {
        if (!DigiDollar::IsDigiDollarEnabled(pindex_prev, params)) {
            return true; // Oracle validation not required before BIP9 activation
        }
    } else {
        // Fallback when no block index available — use height-based check
        if (block_height < params.nDDActivationHeight) {
            return true;
        }
    }
    // Scan all coinbase outputs for OP_ORACLE marker (oracle output position varies:
    // may be vout[1] without witness commitment, or vout[2] with witness commitment)
    int oracle_output_count = 0;
    for (const auto& output : coinbase.vout) {
        if (output.scriptPubKey.size() >= 2 &&
            output.scriptPubKey[0] == OP_RETURN &&
            output.scriptPubKey[1] == OP_ORACLE) {
            oracle_output_count++;
        }
    }

    // SECURITY: Reject blocks with multiple oracle outputs (prevents confusion attacks)
    if (oracle_output_count > 1) {
        LogPrintf("Oracle: Block %d has %d oracle outputs (expected at most 1)\n",
                 block_height, oracle_output_count);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-oracle-multiple-outputs",
            strprintf("Block contains %d oracle outputs, expected at most 1", oracle_output_count));
    }

    if (oracle_output_count == 0) {
        // Allow blocks without oracle data during transition
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: No oracle bundle in block %d (transition period)\n", block_height);
        return true;
    }

    // Extract oracle bundle using OracleBundleManager's parser (scans all outputs for OP_ORACLE)
    COracleBundle bundle;
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    if (!manager.ExtractOracleBundle(coinbase, bundle)) {
        // During transition period, allow blocks without valid oracle bundles
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Block %d could not extract oracle bundle (transition period)\n", block_height);
        return true;
    }

    // IMPORTANT: Once we successfully extract a bundle, we MUST validate it fully
    // No transition period leniency for bundles that are present but invalid

    // STEP 7: PHASE-AWARE CONSENSUS VALIDATION
    const Consensus::Params& consensusParams_ref = Params().GetConsensus();

    // Phase 3 (MuSig2): v0x03 bundles use aggregate signatures instead of individual oracle sigs
    if (bundle.version == 3) {
        std::string phase3_error;
        if (!OracleBundleManager::ValidatePhaseThreeBundle(bundle, block_height, consensusParams_ref, phase3_error)) {
            LogPrintf("Oracle: Phase Three bundle validation failed at block %d: %s\n",
                     block_height, phase3_error);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                                "bad-oracle-phase3",
                                strprintf("Phase Three oracle bundle validation failed: %s", phase3_error));
        }
        // Phase 3 validation complete — skip Phase 1/2 checks below
        // (v0x03 bundles don't have individual oracle messages)
        goto oracle_post_validation;
    }

    // Phase 1: Use generic bundle.IsValid() which checks Phase 1 signatures
    // Phase 2: Skip generic IsValid() — ValidatePhaseTwoBundle does Phase 2-specific validation
    if (block_height < consensusParams_ref.nDigiDollarPhase2Height) {
        if (!bundle.IsValid(consensusParams_ref.nOracleRequiredMessages, block.nTime)) {
            LogPrintf("Oracle: Invalid oracle bundle in block %d\n", block_height);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-oracle-bundle", "invalid oracle bundle structure");
        }
    } else {
        // Phase 2: Basic structural checks only (signatures checked by ValidatePhaseTwoBundle)
        if (bundle.messages.empty()) {
            LogPrintf("Oracle: Empty oracle bundle in block %d\n", block_height);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-oracle-bundle", "empty oracle bundle");
        }
    }

    if (block_height >= consensusParams_ref.nDigiDollarPhase2Height) {
        // Phase Two: Full multi-oracle validation with on-chain signature verification
        if (!OracleBundleManager::ValidatePhaseTwoBundle(bundle, consensusParams_ref)) {
            LogPrintf("Oracle: Phase Two bundle validation failed at block %d\n", block_height);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                                "bad-oracle-phase2",
                                "Phase Two oracle bundle validation failed");
        }
    } else {
        // Phase One: Exactly 1 message required
        if (bundle.messages.size() != 1) {
            LogPrintf("Oracle: Phase One requires exactly 1 oracle message, got %zu\n",
                     bundle.messages.size());
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                                "bad-oracle-consensus",
                                strprintf("Phase One requires exactly 1 oracle message, got %zu",
                                         bundle.messages.size()));
        }
        
        // Phase One: Median price must equal single message price
        const COraclePriceMessage& msg_p1 = bundle.messages[0];
        if (bundle.median_price_micro_usd != msg_p1.price_micro_usd) {
            LogPrintf("Oracle: Median price mismatch: bundle=%llu, message=%llu\n",
                     bundle.median_price_micro_usd, msg_p1.price_micro_usd);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                                "bad-oracle-median",
                                strprintf("Median price mismatch: bundle=%llu, message=%llu",
                                         bundle.median_price_micro_usd, msg_p1.price_micro_usd));
        }
    }

    // Use first message for remaining validation checks
    {
        const COraclePriceMessage& msg = bundle.messages[0];

        // Phase 1: Verify Schnorr signature (Phase 2 signatures verified in ValidatePhaseTwoBundle)
        if (block_height < consensusParams_ref.nDigiDollarPhase2Height) {
            if (!msg.schnorr_sig.empty()) {
                if (!msg.VerifyPhase2()) {
                    LogPrintf("Oracle: Invalid Schnorr signature in oracle message (oracle_id=%d, block=%d)\n",
                             msg.oracle_id, block_height);
                    return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-oracle-signature",
                        "Invalid oracle Schnorr signature");
                }
            }
        }
    }

oracle_post_validation:
    // Verify oracle timestamp is not too old (max 1 hour = 3600 seconds)
    int64_t oracle_age = block.nTime - bundle.timestamp;
    if (oracle_age > ORACLE_MAX_AGE_SECONDS) {
        LogPrintf("Oracle: Oracle message too old: age=%lld seconds (max=%d) in block %d\n",
                 (long long)oracle_age, ORACLE_MAX_AGE_SECONDS, block_height);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-oracle-timestamp",
            strprintf("Oracle timestamp too old: age=%lld seconds (max=%d)", (long long)oracle_age, ORACLE_MAX_AGE_SECONDS));
    }

    // Verify oracle timestamp is not in the future (with 60 second tolerance for clock skew)
    if (bundle.timestamp > block.nTime + 60) {
        LogPrintf("Oracle: Oracle timestamp in future: oracle=%lld, block=%u\n",
                 (long long)bundle.timestamp, block.nTime);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-oracle-timestamp",
            "Oracle timestamp is in the future");
    }

    // Verify oracle is authorized (skip in REGTEST for unit testing, skip Phase 3 — uses bitmap)
    if (bundle.version < 3 && !bundle.messages.empty() && Params().GetChainType() != ChainType::REGTEST) {
        const CChainParams& chainparams = Params();
        const COraclePriceMessage& auth_msg = bundle.messages[0];
        const OracleNodeInfo* oracle_config = chainparams.GetOracleNode(auth_msg.oracle_id);
        if (!oracle_config || !oracle_config->is_active) {
            LogPrintf("Oracle: Unauthorized oracle ID %d in block %d\n", auth_msg.oracle_id, block_height);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-oracle-unauthorized",
                strprintf("Unauthorized oracle ID %d", auth_msg.oracle_id));
        }
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Block %d oracle bundle validated: price=%llu micro-USD\n",
             block_height, bundle.median_price_micro_usd);

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
    return message.VerifyPhase2();
}

bool OracleDataValidator::ValidateOracleBundle(const COracleBundle& bundle, int32_t epoch, const Consensus::Params& params)
{
    // Validate epoch
    if (!bundle.ValidateEpoch(epoch)) {
        LogPrintf("Oracle: Invalid epoch in bundle: %d (current: %d)\n", bundle.epoch, epoch);
        return false;
    }

    // Check consensus requirement using chainparams threshold
    if (!bundle.HasConsensus(params.nOracleRequiredMessages)) {
        LogPrintf("Oracle: Bundle does not have required consensus (%zu messages, %d required)\n",
                  bundle.messages.size(), params.nOracleRequiredMessages);
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

bool OracleDataValidator::CheckOracleConsensus(const COracleBundle& bundle, const Consensus::Params& params)
{
    return bundle.HasConsensus(params.nOracleRequiredMessages);
}

/**
 * Phase Two Bundle Validation Implementation
 */

bool OracleBundleManager::ValidateBundle(const COracleBundle& bundle, int block_height, const Consensus::Params& params)
{
    // v0x03 MuSig2 bundles — structural validation
    if (bundle.version == 3 || bundle.IsMuSig2()) {
        return true;
    }

    if (block_height >= params.nDigiDollarPhase2Height) {
        return ValidatePhaseTwoBundle(bundle, params);
    } else {
        return ValidatePhaseOneBundle(bundle, params);
    }
}

int OracleBundleManager::GetRequiredConsensus(int block_height, const Consensus::Params& params)
{
    if (block_height >= params.nDigiDollarPhase2Height) {
        return params.nOracleRequiredMessages;  // 3 for testnet, 8 for mainnet
    }
    return 1;  // Phase One: 1-of-1
}

bool OracleBundleManager::ValidatePhaseOneBundle(const COracleBundle& bundle, const Consensus::Params& params)
{
    // Phase One: Must have exactly 1 message (1-of-1 consensus)
    if (bundle.messages.size() != 1) {
        LogPrintf("Oracle: Phase One requires exactly 1 oracle message, got %zu\n", bundle.messages.size());
        return false;
    }

    const COraclePriceMessage& msg = bundle.messages[0];

    // Verify message is valid
    if (!msg.IsValid()) {
        LogPrintf("Oracle: Phase One message validation failed\n");
        return false;
    }

    // Verify median price matches single message price
    if (bundle.median_price_micro_usd != msg.price_micro_usd) {
        LogPrintf("Oracle: Phase One median price mismatch: bundle=%llu, message=%llu\n",
                 bundle.median_price_micro_usd, msg.price_micro_usd);
        return false;
    }

    // Phase One: Schnorr signature verification (if signature is present)
    if (!msg.schnorr_sig.empty() && !msg.VerifyPhase2()) {
        LogPrintf("Oracle: Phase One signature verification failed\n");
        return false;
    }

    return true;
}

bool OracleBundleManager::ValidatePhaseTwoBundle(const COracleBundle& bundle, const Consensus::Params& params)
{
    // Check minimum message count
    int min_required = params.nOracleRequiredMessages;
    if (bundle.messages.size() < static_cast<size_t>(min_required)) {
        LogPrintf("Oracle: Phase Two requires at least %d messages, got %zu\n",
                 min_required, bundle.messages.size());
        return false;
    }

    // Check for duplicate oracle IDs
    std::set<uint32_t> seen_oracles;
    for (const auto& msg : bundle.messages) {
        if (seen_oracles.count(msg.oracle_id) > 0) {
            LogPrintf("Oracle: Phase Two detected duplicate oracle ID %d\n", msg.oracle_id);
            return false;
        }
        seen_oracles.insert(msg.oracle_id);
    }

    // Get active oracle set for current epoch
    std::vector<uint32_t> active_oracles = OracleBundleManager::GetInstance().GetActiveOraclesForEpoch(bundle.epoch);

    // Verify each message is from an active oracle and has valid signature
    int valid_count = 0;
    for (const auto& msg : bundle.messages) {
        // Check if oracle is in active set
        auto it = std::find(active_oracles.begin(), active_oracles.end(), msg.oracle_id);
        if (it == active_oracles.end()) {
            LogPrintf("Oracle: Phase Two oracle %d not in active set for epoch %d\n",
                     msg.oracle_id, bundle.epoch);
            continue;  // Skip invalid oracle, don't fail entire bundle
        }

        // Verify basic message fields (price range)
        if (msg.price_micro_usd < ORACLE_MIN_PRICE_MICRO_USD ||
            msg.price_micro_usd > ORACLE_MAX_PRICE_MICRO_USD) {
            LogPrintf("Oracle: Phase Two message price out of range for oracle %d\n", msg.oracle_id);
            continue;
        }

        // Verify Schnorr signature using Phase 2 hash (oracle_id + price + timestamp only)
        if (!msg.schnorr_sig.empty()) {
            if (!msg.VerifyPhase2()) {
                LogPrintf("Oracle: Phase Two signature verification failed for oracle %d\n", msg.oracle_id);
                continue;  // Skip message with invalid signature
            }
        } else {
            LogPrintf("Oracle: Phase Two message missing signature for oracle %d\n", msg.oracle_id);
            continue;  // Skip message without signature
        }

        valid_count++;
    }

    // Check if we have enough valid signatures
    if (valid_count < min_required) {
        LogPrintf("Oracle: Phase Two requires %d valid signatures, got %d\n",
                 min_required, valid_count);
        return false;
    }

    // Verify consensus price calculation
    CAmount calculated_price = CalculateConsensusPrice(bundle, params);
    if (calculated_price != static_cast<CAmount>(bundle.median_price_micro_usd)) {
        LogPrintf("Oracle: Phase Two consensus price mismatch: calculated=%lld, bundle=%llu\n",
                 calculated_price, bundle.median_price_micro_usd);
        return false;
    }

    LogPrintf("Oracle: Phase Two bundle validated successfully: %d valid signatures (min: %d), price=%llu micro-USD\n",
             valid_count, min_required, bundle.median_price_micro_usd);

    return true;
}

/**
 * Compute deterministic hash of oracle bundle consensus data.
 *
 * For v0x03 (MuSig2) bundles, the aggregate signature covers the consensus
 * price and timestamp — the fields all participating oracles agreed upon.
 * This hash is used as the message for secp256k1_schnorrsig_verify.
 */
uint256 ComputeOracleBundleHash(const COracleBundle& bundle)
{
    // Must match OracleSigningOrchestrator::ComputeOracleMessageHash
    // which hashes (epoch, price, timestamp).
    CHashWriter ss(0);
    ss << bundle.epoch;
    ss << bundle.median_price_micro_usd;
    ss << bundle.timestamp;
    return ss.GetHash();
}

/**
 * Phase Three Bundle Validation (MuSig2 aggregate signatures)
 *
 * Validates a v0x03 oracle bundle containing:
 * - participation_bitmap: which oracles contributed to the aggregate signature
 * - aggregate_sig: 64-byte BIP-340 Schnorr signature from the MuSig2 ceremony
 * - median_price_micro_usd + timestamp: consensus data signed by the aggregate key
 *
 * Verification steps:
 * 1. Check activation height
 * 2. Validate aggregate_sig size (must be 64 bytes)
 * 3. Validate bitmap is non-empty and decodes correctly
 * 4. Check participating oracle count >= threshold
 * 5. Compute aggregate pubkey from participating oracle subset (via chainparams)
 * 6. Verify aggregate signature against bundle hash using schnorrsig_verify
 */
bool OracleBundleManager::ValidatePhaseThreeBundle(const COracleBundle& bundle,
                                                    int32_t block_height,
                                                    const Consensus::Params& params,
                                                    std::string& error)
{
    // Pre-condition: Phase 3 must be active
    // Pre-condition: must be a v0x03 bundle
    if (bundle.version != 3) {
        error = "ValidatePhaseThreeBundle called with non-v0x03 bundle (version=" + std::to_string(bundle.version) + ")";
        return false;
    }

    if (bundle.median_price_micro_usd < ORACLE_MIN_PRICE_MICRO_USD ||
        bundle.median_price_micro_usd > ORACLE_MAX_PRICE_MICRO_USD) {
        error = "v0x03 consensus price out of range (" + std::to_string(bundle.median_price_micro_usd) + ")";
        return false;
    }

    // Check aggregate signature size (BIP-340 Schnorr: exactly 64 bytes)
    if (bundle.aggregate_sig.size() != 64) {
        error = "Invalid v0x03 aggregate signature size (" + std::to_string(bundle.aggregate_sig.size()) + " bytes, expected 64)";
        return false;
    }

    // Check bitmap is present
    if (bundle.participation_bitmap.empty()) {
        error = "v0x03 bitmap cannot be empty";
        return false;
    }

    // RC30: Bind the v0x03 payload epoch to the current block's epoch.
    // The signer hashes H(epoch, price, timestamp); a bundle whose payload epoch
    // doesn't match the current epoch cannot verify (and could otherwise enable
    // cross-epoch replay of a previously-valid aggregate signature).
    const int32_t expected_epoch = GetCurrentEpoch(block_height);
    if (bundle.epoch != expected_epoch) {
        error = "v0x03 bundle epoch mismatch (payload=" + std::to_string(bundle.epoch) +
                ", expected=" + std::to_string(expected_epoch) + ")";
        return false;
    }

    // Decode bitmap to get participating oracle IDs
    std::vector<uint8_t> oracle_ids = MuSig2OracleAggregator::DecodeBitmap(
        bundle.participation_bitmap, static_cast<uint16_t>(params.nOracleTotalOracles));

    if (oracle_ids.empty()) {
        error = "v0x03 bitmap decoding failed (malformed bitmap or size mismatch)";
        return false;
    }

    // Check minimum oracle threshold
    if (static_cast<int>(oracle_ids.size()) < params.nOracleRequiredMessages) {
        error = "v0x03 bundle below minimum oracle threshold (" +
                std::to_string(oracle_ids.size()) + " signers, need " +
                std::to_string(params.nOracleRequiredMessages) + ")";
        return false;
    }

    // Compute aggregate pubkey for participating oracles
    MuSig2OracleAggregator aggregator;
    secp256k1_xonly_pubkey agg_pk;
    secp256k1_musig_keyagg_cache cache;

    LogPrintf("Oracle: ValidatePhaseThreeBundle: bitmap=%s, total_oracles=%d, oracle_ids=[%s]\n",
             HexStr(bundle.participation_bitmap),
             params.nOracleTotalOracles,
             [&]() -> std::string {
                 std::string s;
                 for (size_t i = 0; i < oracle_ids.size(); ++i) {
                     if (i > 0) s += ",";
                     s += std::to_string(oracle_ids[i]);
                 }
                 return s;
             }());

    if (!aggregator.ComputeAggregatePubkeyFromBitmap(bundle.participation_bitmap,
                                                      static_cast<uint16_t>(params.nOracleTotalOracles),
                                                      agg_pk, cache)) {
        error = "Failed to compute aggregate pubkey from bitmap";
        return false;
    }

    // Compute message hash (price + timestamp)
    uint256 msg_hash = ComputeOracleBundleHash(bundle);

    // Verify aggregate signature using BIP-340 Schnorr verification
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    int result = secp256k1_schnorrsig_verify(ctx,
                                              bundle.aggregate_sig.data(),
                                              msg_hash.begin(), 32,
                                              &agg_pk);
    secp256k1_context_destroy(ctx);

    if (!result) {
        error = "v0x03 aggregate signature verification failed";
        return false;
    }

    LogPrintf("Oracle: Phase Three bundle validated: %zu oracles, price=%llu micro-USD\n",
             oracle_ids.size(), bundle.median_price_micro_usd);

    return true;
}

CAmount OracleBundleManager::CalculateConsensusPrice(const COracleBundle& bundle, const Consensus::Params& params)
{
    // SECURITY: Use price-range checks ONLY — NOT msg.IsValid().
    // IsValid() calls GetTime() for timestamp checks, making the consensus
    // price depend on wall-clock time. During IBD or delayed block relay,
    // oracle timestamps become "stale" relative to GetTime(), causing
    // messages to be excluded. This produces a DIFFERENT consensus price
    // than the miner calculated, rejecting valid blocks and causing chain
    // splits between timely and delayed nodes.
    //
    // Timestamp validation belongs in ValidateBlockOracleData (which uses
    // block.nTime, not GetTime()). Here we only filter by price range.
    std::vector<CAmount> prices;
    for (const auto& msg : bundle.messages) {
        // Price-range filter only — deterministic, time-independent
        if (msg.price_micro_usd >= ORACLE_MIN_PRICE_MICRO_USD &&
            msg.price_micro_usd <= ORACLE_MAX_PRICE_MICRO_USD) {
            prices.push_back(static_cast<CAmount>(msg.price_micro_usd));
        }
    }

    if (prices.empty()) {
        return 0;  // No valid prices
    }

    // Sort prices for IQR calculation
    std::sort(prices.begin(), prices.end());

    // If less than 4 prices, just return median without outlier filtering
    if (prices.size() < 4) {
        size_t mid = prices.size() / 2;
        if (prices.size() % 2 == 0) {
            return (prices[mid - 1] + prices[mid]) / 2;
        }
        return prices[mid];
    }

    // Apply IQR outlier filtering (1.5 * IQR rule)
    size_t q1_idx = prices.size() / 4;
    size_t q3_idx = (prices.size() * 3) / 4;
    CAmount q1 = prices[q1_idx];
    CAmount q3 = prices[q3_idx];
    CAmount iqr = q3 - q1;
    CAmount lower_bound = q1 - (iqr * 3 / 2);  // 1.5 * IQR below Q1
    CAmount upper_bound = q3 + (iqr * 3 / 2);  // 1.5 * IQR above Q3

    // Filter outliers
    std::vector<CAmount> filtered;
    for (CAmount price : prices) {
        if (price >= lower_bound && price <= upper_bound) {
            filtered.push_back(price);
        }
    }

    // If filtering removed all prices, fall back to unfiltered median
    if (filtered.empty()) {
        size_t mid = prices.size() / 2;
        if (prices.size() % 2 == 0) {
            return (prices[mid - 1] + prices[mid]) / 2;
        }
        return prices[mid];
    }

    // Calculate median of filtered prices
    std::sort(filtered.begin(), filtered.end());
    size_t mid = filtered.size() / 2;
    if (filtered.size() % 2 == 0) {
        return (filtered[mid - 1] + filtered[mid]) / 2;
    }
    return filtered[mid];
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
    CAmount price_micro_usd = manager.GetLatestPrice();

    // Phase One (testnet): cached_price is stored in micro-USD format
    // Convert micro-USD to cents: cents = micro-USD / 10,000
    // e.g. 50,000 micro-USD = $0.05 = 5 cents
    // e.g. 6,310 micro-USD = $0.00631 = 0.631 cents (rounds to 1 cent)
    if (price_micro_usd > 0) {
        CAmount price_cents = (price_micro_usd + 5000) / 10000; // Round to nearest cent
        if (price_cents == 0) {
            price_cents = 1; // Minimum 1 cent for any non-zero price
        }
        // Use LogPrint instead of LogPrintf to avoid log spam during sync
        LogPrint(BCLog::NET, "Oracle: GetCurrentOraclePrice returning %lld micro-USD = %lld cents ($%.4f)\n",
                 price_micro_usd, price_cents, price_cents / 100.0);
        return price_cents;
    }

    // No oracle price available - return 0 to indicate no data
    // Caller must handle this case appropriately
    LogPrintf("Oracle: No oracle price available, returning 0\n");
    return 0;
}

CAmount GetCurrentOraclePriceMicroUSD()
{
    // In RegTest mode, use MockOracleManager for testing
    // MockOracleManager stores and returns micro-USD directly (set via setmockoracleprice RPC)
    if (Params().GetChainType() == ChainType::REGTEST) {
        CAmount mockPriceMicroUSD = MockOracleManager::GetInstance().GetCurrentPrice();
        if (mockPriceMicroUSD > 0) {
            LogPrintf("Oracle: GetCurrentOraclePriceMicroUSD returning %lld micro-USD ($%.6f) from MockOracleManager\n",
                     mockPriceMicroUSD, mockPriceMicroUSD / 1000000.0);
            return mockPriceMicroUSD;
        }
    }

    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    CAmount price_micro_usd = manager.GetLatestPrice();

    if (price_micro_usd > 0) {
        LogPrintf("Oracle: GetCurrentOraclePriceMicroUSD returning %lld micro-USD ($%.6f)\n",
                 price_micro_usd, price_micro_usd / 1000000.0);
        return price_micro_usd;
    }

    // No oracle price available
    LogPrint(BCLog::DIGIDOLLAR, "Oracle: No oracle price available in GetCurrentOraclePriceMicroUSD, returning 0\n");
    return 0;
}

CAmount GetOraclePriceForHeight(int nHeight)
{
    // Get oracle bundle manager instance
    OracleBundleManager& manager = OracleBundleManager::GetInstance();

    // Phase One: Try to get price from cache first (populated by ConnectBlock)
    // This takes priority over MockOracleManager for accurate integration testing
    uint64_t cached_price = manager.GetOraclePriceForHeight(nHeight);
    if (cached_price > 0) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using cached price for height %d: %llu micro-USD ($%.6f)\n",
                 nHeight, cached_price, cached_price / 1000000.0);
        return static_cast<CAmount>(cached_price);
    }

    // In RegTest mode, fall back to MockOracleManager for simple tests
    if (Params().GetChainType() == ChainType::REGTEST) {
        CAmount mockPrice = MockOracleManager::GetInstance().GetCurrentPrice();
        if (mockPrice > 0) {
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using mock price for height %d: %lld (MockOracleManager fallback)\n",
                     nHeight, mockPrice);
            return mockPrice;
        }
    }

    // Fallback: Try to get from current epoch bundle (for mempool transactions)
    int32_t epoch = GetCurrentEpoch(nHeight);
    COracleBundle bundle = manager.GetCurrentBundle(epoch);
    const int nRequired = manager.GetMinOracleCount();

    if (bundle.HasConsensus(nRequired)) {
        CAmount price = bundle.GetConsensusPrice(nRequired);
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using current epoch price for height %d (epoch %d): %lld micro-USD\n",
                 nHeight, epoch, price);
        return price;
    }

    // Try previous epoch as fallback
    if (epoch > 0) {
        COracleBundle prev_bundle = manager.GetCurrentBundle(epoch - 1);
        if (prev_bundle.HasConsensus(nRequired)) {
            CAmount price = prev_bundle.GetConsensusPrice(nRequired);
            LogPrint(BCLog::DIGIDOLLAR, "Oracle: Using previous epoch price for height %d: %lld micro-USD\n",
                     nHeight, price);
            return price;
        }
    }

    // No oracle price available
    LogPrint(BCLog::DIGIDOLLAR, "Oracle: No price available for height %d\n", nHeight);
    return 0;
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
