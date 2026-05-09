// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/signing_orchestrator.h>

#include <chainparams.h>
#include <hash.h>
#include <logging.h>
#include <net.h>
#include <netmessagemaker.h>
#include <oracle/bundle_manager.h>
#include <oracle/node.h>
#include <primitives/oracle.h>
#include <protocol.h>
#include <serialize.h>

#include <secp256k1.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

std::unique_ptr<OracleSigningOrchestrator> g_signing_orchestrator;

namespace {
int32_t GetEpochStartHeight(int32_t epoch)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    int32_t epoch_length = consensus.nDDOracleEpochBlocks;
    if (epoch_length <= 0) epoch_length = 1440;
    return epoch * epoch_length;
}
} // namespace

std::vector<uint8_t> OracleSigningOrchestrator::GetConsensusOracleIdsForSigning()
{
    std::vector<uint8_t> oracle_ids;
    const Consensus::Params& consensus = Params().GetConsensus();
    const int active_pubkeys = consensus.nOraclePubkeyCount;
    if (active_pubkeys <= 0) return oracle_ids;

    const auto& nodes = Params().GetOracleNodes();
    oracle_ids.reserve(nodes.size());
    for (const auto& node : nodes) {
        if (!node.is_active) continue;
        // vOracleNodes may include reserve metadata beyond the consensus keyset.
        if (node.id >= static_cast<uint32_t>(active_pubkeys)) continue;
        if (node.id > std::numeric_limits<uint8_t>::max()) continue;
        oracle_ids.push_back(static_cast<uint8_t>(node.id));
    }
    return oracle_ids;
}

// ============================================================================
// Construction / lifecycle
// ============================================================================

OracleSigningOrchestrator::OracleSigningOrchestrator() = default;

OracleSigningOrchestrator::~OracleSigningOrchestrator()
{
    Stop();
}

void OracleSigningOrchestrator::Start()
{
    if (m_started) return;
    RegisterValidationInterface(this);
    m_started = true;
    LogPrintf("Oracle: MuSig2 signing orchestrator started\n");
}

void OracleSigningOrchestrator::Stop()
{
    if (!m_started) return;
    UnregisterValidationInterface(this);
    m_started = false;
    LogPrintf("Oracle: MuSig2 signing orchestrator stopped\n");
}

void OracleSigningOrchestrator::Clear()
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_signing_sessions.clear();
    m_nonce_broadcast_tracker.clear();
    m_partialsig_broadcast_tracker.clear();
    m_pending_partialsigs.clear();
    m_aggregator.reset();
    m_cached_oracle_key.reset();
    m_cached_oracle_id = 255;
    m_oracle_key_cached = false;
}

void OracleSigningOrchestrator::InjectSession(int32_t epoch, std::unique_ptr<MuSig2SigningSession> session)
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_signing_sessions[epoch] = std::move(session);
}

void OracleSigningOrchestrator::IngestRemoteNonce(const OracleMusigNonceMsg& msg)
{
    // RC30: auto-create a session for this epoch if we don't have one yet.
    // Early-arriving remote nonces used to be dropped as "unknown epoch",
    // which prevented threshold sessions from ever assembling enough nonces
    // when mining is fast. Sessions are tiny so lazy creation is cheap.
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_signing_sessions.find(msg.epoch);
    if (it == m_signing_sessions.end() || !it->second) {
        const Consensus::Params& consensus = Params().GetConsensus();
        const uint8_t min_signers = static_cast<uint8_t>(std::max(1, consensus.nOracleConsensusRequired));
        auto session = std::make_unique<MuSig2SigningSession>(msg.epoch, min_signers);
        session->SetCreationHeight(GetEpochStartHeight(msg.epoch));
        session->SetTimeoutBlocks(100);
        LogPrintf("Oracle: Lazily created MuSig2 session for epoch %d on remote nonce arrival\n", msg.epoch);
        it = m_signing_sessions.emplace(msg.epoch, std::move(session)).first;
    }
    if (it == m_signing_sessions.end() || !it->second) return;

    if (it->second->GetState() == MuSig2SessionState::CREATED) {
        std::vector<uint8_t> all_oracle_ids = GetConsensusOracleIdsForSigning();

        MuSig2OracleAggregator aggregator;
        secp256k1_xonly_pubkey agg_pk;
        secp256k1_musig_keyagg_cache cache;
        if (!aggregator.ComputeAggregatePubkey(all_oracle_ids, agg_pk, cache) ||
            !it->second->InitializePassive(cache)) {
            LogPrint(BCLog::DIGIDOLLAR,
                     "Oracle: Failed to initialize passive MuSig2 session for epoch %d on remote nonce arrival\n",
                     msg.epoch);
            return;
        }
    }

    // Deserialize pubnonce
    if (msg.pubnonce.size() != 66) return;
    secp256k1_musig_pubnonce pubnonce;
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (secp256k1_musig_pubnonce_parse(ctx, &pubnonce, msg.pubnonce.data())) {
        if (it->second->AddPubnonce(msg.oracle_id, pubnonce)) {
            LogPrintf("Oracle: Ingested remote nonce for epoch %d from oracle %d\n",
                     msg.epoch, msg.oracle_id);
        }
    }
    secp256k1_context_destroy(ctx);
}

void OracleSigningOrchestrator::IngestRemotePartialSig(const OracleMusigPartialSigMsg& msg)
{
    // RC30: auto-create the session so partial sigs arriving from faster peers
    // (who already progressed past NONCES_COMPLETE) aren't lost. Session is
    // also needed to hold a pending buffer if local side is still in
    // CREATED/NONCES_COLLECTING.
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_signing_sessions.find(msg.epoch);
    if (it == m_signing_sessions.end() || !it->second) {
        const Consensus::Params& consensus = Params().GetConsensus();
        const uint8_t min_signers = static_cast<uint8_t>(std::max(1, consensus.nOracleConsensusRequired));
        auto session = std::make_unique<MuSig2SigningSession>(msg.epoch, min_signers);
        session->SetCreationHeight(GetEpochStartHeight(msg.epoch));
        session->SetTimeoutBlocks(100);
        LogPrintf("Oracle: Lazily created MuSig2 session for epoch %d on remote partial sig arrival\n", msg.epoch);
        it = m_signing_sessions.emplace(msg.epoch, std::move(session)).first;
    }
    if (it == m_signing_sessions.end() || !it->second) return;

    if (TryApplyRemotePartialSig(msg, *it->second)) {
        LogPrintf("Oracle: Ingested remote partial sig for epoch %d from oracle %d\n",
                 msg.epoch, msg.oracle_id);

        // Auto-aggregate if threshold met
        if (it->second->HasEnoughPartialSigs()) {
            std::vector<unsigned char> final_sig;
            if (it->second->AggregateSignature(final_sig)) {
                LogPrintf("Oracle: MuSig2 auto-aggregated for epoch %d after remote partial sig, sig size=%zu\n",
                         msg.epoch, final_sig.size());
            }
        }
    } else if (it->second->GetState() != MuSig2SessionState::SIGNING &&
               it->second->GetState() != MuSig2SessionState::COMPLETE &&
               it->second->GetState() != MuSig2SessionState::FAILED) {
        BufferPendingPartialSig(msg);
    }
}

bool OracleSigningOrchestrator::TryApplyRemotePartialSig(const OracleMusigPartialSigMsg& msg,
                                                         MuSig2SigningSession& session) const
{
    if (msg.partial_sig.size() != 32) return false;

    secp256k1_musig_partial_sig partial_sig;
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx) return false;

    bool verified_and_added = false;
    if (secp256k1_musig_partial_sig_parse(ctx, &partial_sig, msg.partial_sig.data())) {
        const OracleNodeInfo* oracle_cfg = Params().GetOracleNode(msg.oracle_id);
        if (oracle_cfg) {
            secp256k1_pubkey signer_pk;
            if (secp256k1_ec_pubkey_parse(ctx, &signer_pk,
                                          oracle_cfg->pubkey.data(),
                                          oracle_cfg->pubkey.size())) {
                verified_and_added = session.AddPartialSignatureVerified(
                    msg.oracle_id, partial_sig, signer_pk);
            }
        }
    }

    secp256k1_context_destroy(ctx);
    return verified_and_added;
}

void OracleSigningOrchestrator::BufferPendingPartialSig(const OracleMusigPartialSigMsg& msg)
{
    // Keep early partial sig replay bounded. Honest epochs need at most one
    // message per oracle; malformed overflow is dropped until the next epoch.
    constexpr size_t MAX_PENDING_PER_EPOCH = 32;   // > any honest oracle count
    constexpr size_t MAX_PENDING_EPOCHS    = 8;    // ±4 epochs around current

    auto& epoch_buf = m_pending_partialsigs[msg.epoch];
    auto existing = std::find_if(epoch_buf.begin(), epoch_buf.end(),
                                 [&msg](const OracleMusigPartialSigMsg& pending) {
                                     return pending.oracle_id == msg.oracle_id;
                                 });
    if (existing != epoch_buf.end()) {
        *existing = msg;
        LogPrint(BCLog::DIGIDOLLAR,
                 "Oracle: Replaced buffered partial sig for epoch %d oracle %d (session not SIGNING yet)\n",
                 msg.epoch, msg.oracle_id);
        return;
    }

    if (epoch_buf.size() < MAX_PENDING_PER_EPOCH) {
        epoch_buf.push_back(msg);
    }
    if (m_pending_partialsigs.size() > MAX_PENDING_EPOCHS) {
        m_pending_partialsigs.erase(m_pending_partialsigs.begin());
    }
    LogPrint(BCLog::DIGIDOLLAR,
             "Oracle: Buffered partial sig for epoch %d oracle %d (session not SIGNING yet)\n",
             msg.epoch, msg.oracle_id);
}

size_t OracleSigningOrchestrator::DrainPendingPartialSigsForEpoch(int32_t epoch,
                                                                  MuSig2SigningSession& session)
{
    if (session.GetState() != MuSig2SessionState::SIGNING) return 0;

    std::vector<OracleMusigPartialSigMsg> pending;
    {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        auto it = m_pending_partialsigs.find(epoch);
        if (it == m_pending_partialsigs.end()) return 0;
        pending = std::move(it->second);
        m_pending_partialsigs.erase(it);
    }

    size_t accepted = 0;
    for (const OracleMusigPartialSigMsg& msg : pending) {
        if (TryApplyRemotePartialSig(msg, session)) {
            ++accepted;
        }
    }

    if (accepted > 0) {
        LogPrintf("Oracle: Replayed %zu pending partial sigs for epoch %d\n", accepted, epoch);
    }
    return accepted;
}

OracleSigningOrchestrator& OracleSigningOrchestrator::GetInstance()
{
    assert(g_signing_orchestrator);
    return *g_signing_orchestrator;
}

void OracleSigningOrchestrator::Initialize()
{
    assert(!g_signing_orchestrator);
    g_signing_orchestrator = std::make_unique<OracleSigningOrchestrator>();
    g_signing_orchestrator->Start();
    LogPrintf("Oracle: MuSig2 signing orchestrator initialized and started\n");
}

void OracleSigningOrchestrator::Shutdown()
{
    if (g_signing_orchestrator) {
        g_signing_orchestrator->Stop();
        g_signing_orchestrator.reset();
    }
}

// ============================================================================
// Oracle detection
// ============================================================================

bool OracleSigningOrchestrator::IsOracleNode() const
{
    if (!g_oracle_manager) return false;
    OracleManager& om = OracleManager::GetInstance();
    return om.GetActiveOracleCount() > 0;
}

const CKey* OracleSigningOrchestrator::GetOracleSigningKey() const
{
    if (m_oracle_key_cached) {
        return m_cached_oracle_key ? m_cached_oracle_key.get() : nullptr;
    }

    m_oracle_key_cached = true;
    if (!g_oracle_manager) {
        m_cached_oracle_key.reset();
        return nullptr;
    }

    OracleManager& om = OracleManager::GetInstance();
    auto ids = om.GetActiveOracleIds();
    if (ids.empty()) {
        m_cached_oracle_key.reset();
        return nullptr;
    }

    OracleNode* node = om.GetOracleNode(ids[0]);
    if (!node) {
        m_cached_oracle_key.reset();
        return nullptr;
    }

    CKey key = node->GetOraclePrivateKey();
    if (!key.IsValid()) {
        m_cached_oracle_key.reset();
        return nullptr;
    }

    m_cached_oracle_id = static_cast<uint8_t>(ids[0]);
    m_cached_oracle_key = std::make_unique<CKey>(key);
    return m_cached_oracle_key.get();
}

uint8_t OracleSigningOrchestrator::GetOracleId() const
{
    if (!m_oracle_key_cached) {
        GetOracleSigningKey();
    }
    return m_cached_oracle_id;
}

// ============================================================================
// Session management
// ============================================================================

MuSig2SigningSession* OracleSigningOrchestrator::GetOrCreateSigningSession(int32_t epoch, int32_t block_height)
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);

    auto it = m_signing_sessions.find(epoch);
    if (it != m_signing_sessions.end()) {
        return it->second.get();
    }

    const Consensus::Params& consensus = Params().GetConsensus();
    const uint8_t min_signers = static_cast<uint8_t>(std::max(1, consensus.nOracleConsensusRequired));
    auto session = std::make_unique<MuSig2SigningSession>(
        epoch, min_signers);
    session->SetCreationHeight(block_height > 0 ? block_height : GetEpochStartHeight(epoch));
    session->SetTimeoutBlocks(100);

    MuSig2SigningSession* ptr = session.get();
    m_signing_sessions[epoch] = std::move(session);

    LogPrintf("Oracle: Created MuSig2 signing session for epoch %d (creation_height=%d)\n",
             epoch, block_height);
    return ptr;
}

bool OracleSigningOrchestrator::HasSession(int32_t epoch) const
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    return m_signing_sessions.find(epoch) != m_signing_sessions.end();
}

void OracleSigningOrchestrator::CleanupOldSessions(int32_t current_epoch)
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);

    for (auto it = m_signing_sessions.begin(); it != m_signing_sessions.end(); ) {
        if (it->first < current_epoch - 2) {
            it = m_signing_sessions.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_nonce_broadcast_tracker.begin(); it != m_nonce_broadcast_tracker.end(); ) {
        it = (it->first < current_epoch - 2) ? m_nonce_broadcast_tracker.erase(it) : std::next(it);
    }
    for (auto it = m_partialsig_broadcast_tracker.begin(); it != m_partialsig_broadcast_tracker.end(); ) {
        it = (it->first < current_epoch - 2) ? m_partialsig_broadcast_tracker.erase(it) : std::next(it);
    }
    // W6-H-01: prune stale buffered partial sigs along with sessions.
    for (auto it = m_pending_partialsigs.begin(); it != m_pending_partialsigs.end(); ) {
        it = (it->first < current_epoch - 2) ? m_pending_partialsigs.erase(it) : std::next(it);
    }
}

// ============================================================================
// ValidationInterface callback
// ============================================================================

void OracleSigningOrchestrator::BlockConnected(
    ChainstateRole role,
    const std::shared_ptr<const CBlock>& block,
    const CBlockIndex* pindex)
{
    if (role != ChainstateRole::NORMAL) return;
    if (!pindex) return;
    OnBlockConnected(block, pindex->nHeight);
}

// ============================================================================
// Block-tick orchestration
// ============================================================================

void OracleSigningOrchestrator::OnBlockConnected(
    const std::shared_ptr<const CBlock>& /*block*/,
    int32_t block_height)
{
    const int32_t current_epoch = GetCurrentEpoch(block_height);

    CleanupOldSessions(current_epoch);

    TickEpochSession(current_epoch, block_height);

    // Pre-start next epoch's ceremony in the tail of the current one.
    //
    // The MuSig2 ceremony needs ~3 block ticks to reach COMPLETE
    // (nonce -> partial-sig -> aggregate). Without pre-start, the
    // session for epoch N+1 is only created when block (N+1)*L
    // first connects, which can leave the next block template without
    // the mandatory v0x03 bundle. Starting the ceremony K blocks early
    // lets it reach COMPLETE before the first block of the next epoch
    // is templated.
    const Consensus::Params& p = Params().GetConsensus();
    int32_t epoch_length = p.nDDOracleEpochBlocks;
    if (epoch_length <= 0) epoch_length = 1440;
    const int32_t pos_in_epoch = block_height % epoch_length;
    const int32_t blocks_until_next = epoch_length - pos_in_epoch;
    constexpr int32_t kPreStartWindow = 5;
    if (blocks_until_next > 0 && blocks_until_next <= kPreStartWindow) {
        TickEpochSession(current_epoch + 1, block_height);
    }
}

void OracleSigningOrchestrator::TickEpochSession(int32_t epoch, int32_t block_height)
{
    MuSig2SigningSession* session = GetOrCreateSigningSession(epoch, block_height);
    if (!session) return;

    MuSig2SessionState state = session->GetState();
    bool is_oracle = IsOracleNode();

    LogPrintf("Oracle: TickEpochSession h=%d epoch=%d state=%d is_oracle=%d\n",
             block_height, epoch, static_cast<int>(state), is_oracle);

    if (!m_aggregator) {
        m_aggregator = std::make_unique<MuSig2OracleAggregator>();
    }

    // ── Step 1: Oracle generates nonces for ALL local oracle IDs on new epoch ──
    if (is_oracle && (state == MuSig2SessionState::CREATED || state == MuSig2SessionState::NONCES_COLLECTING)) {
        OracleManager& om = OracleManager::GetInstance();
        const std::vector<uint32_t> local_ids = om.GetActiveOracleIds();

        LogPrintf("Oracle: Step 1 - local_ids.size()=%zu for epoch %d\n", local_ids.size(), epoch);

        // Build full oracle ID list for key aggregation
        std::vector<uint8_t> all_oracle_ids = GetConsensusOracleIdsForSigning();

        LogPrintf("Oracle: Step 1 - all_oracle_ids.size()=%zu\n", all_oracle_ids.size());

        secp256k1_xonly_pubkey agg_pk;
        secp256k1_musig_keyagg_cache cache;
        if (m_aggregator->ComputeAggregatePubkey(all_oracle_ids, agg_pk, cache)) {
            LogPrintf("Oracle: Step 1 - key aggregation succeeded for %zu oracle IDs\n", all_oracle_ids.size());
            secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

            for (uint32_t oid : local_ids) {
                uint8_t oid8 = static_cast<uint8_t>(oid);
                // Skip if already generated nonce for this oracle
                if (m_nonce_broadcast_tracker[epoch].count(oid8)) {
                    LogPrintf("Oracle: Skipping oracle %d epoch %d - already broadcast\n", oid8, epoch);
                    continue;
                }

                OracleNode* onode = om.GetOracleNode(oid);
                if (!onode) {
                    LogPrintf("Oracle: Skipping oracle %d - GetOracleNode returned null\n", oid8);
                    continue;
                }
                CKey key = onode->GetOraclePrivateKey();
                if (!key.IsValid()) {
                    LogPrintf("Oracle: Skipping oracle %d - invalid private key\n", oid8);
                    continue;
                }
                CPubKey cpk = key.GetPubKey();
                LogPrintf("Oracle: oracle %d pubkey size=%d hex=%s\n", oid8, cpk.size(), HexStr(cpk));

                secp256k1_pubkey secp_pk;
                if (!secp256k1_ec_pubkey_parse(ctx, &secp_pk, cpk.data(), cpk.size())) {
                    LogPrintf("Oracle: Skipping oracle %d - secp256k1_ec_pubkey_parse failed\n", oid8);
                    continue;
                }

                secp256k1_musig_pubnonce pubnonce;
                if (session->GenerateNonce(oid8, key, secp_pk, cache, pubnonce)) {
                    session->AddPubnonce(oid8, pubnonce);

                    unsigned char ser_nonce[66];
                    if (secp256k1_musig_pubnonce_serialize(ctx, ser_nonce, &pubnonce)) {
                        OracleMusigNonceMsg nonce_msg;
                        nonce_msg.epoch = epoch;
                        nonce_msg.oracle_id = oid8;
                        nonce_msg.pubnonce.assign(ser_nonce, ser_nonce + 66);

                        // RC30: sign the nonce message so peers accept it.
                        // RH-24 added Schnorr auth on the receive side; the
                        // sender must also sign or every peer drops the msg
                        // as "invalid MuSig2 nonce signature".
                        if (!nonce_msg.Sign(key)) {
                            LogPrintf("Oracle: Sign() failed for MuSig2 nonce oracle=%d epoch=%d\n",
                                     oid8, epoch);
                            continue;
                        }

                        BroadcastMusigNonce(nonce_msg);
                        m_nonce_broadcast_tracker[epoch].insert(oid8);

                        LogPrintf("Oracle: Generated and broadcast nonce for epoch %d (oracle_id=%d)\n",
                                 epoch, oid8);
                    }
                } else {
                    LogPrintf("Oracle: GenerateNonce FAILED for oracle %d epoch %d\n", oid8, epoch);
                }
            }
            secp256k1_context_destroy(ctx);
        } else {
            LogPrintf("Oracle: Failed to compute aggregate pubkey for epoch %d\n", epoch);
        }
    }

    state = session->GetState();

    // ── Step 2: Oracle creates partial sigs for ALL local oracle IDs when nonces complete ──
    if (is_oracle && state == MuSig2SessionState::NONCES_COMPLETE) {
        uint64_t consensus_price = 0;
        int64_t consensus_timestamp = 0;
        bool have_consensus = false;

        if (g_oracle_bundle_manager) {
            have_consensus = OracleBundleManager::GetInstance().ComputeConsensusValues(
                consensus_price, consensus_timestamp);
        }

        if (have_consensus) {
            unsigned char msg32[32];
            ComputeOracleMessageHash(epoch, consensus_price, consensus_timestamp, msg32);

            // Store the exact values we're signing so the miner embeds
            // them in the bundle (must match for verification).
            session->SetSignedValues(consensus_price, consensus_timestamp);

            // Threshold MuSig2: trim to exactly threshold nonces, then
            // recompute key aggregation for ONLY those oracles. The session,
            // partial sigs, and aggregate signature are all bound to this
            // threshold-sized participant set. This ensures the validator can
            // reconstruct the same aggregate key from the bitmap.
            session->TrimNoncesToThreshold();
            std::vector<uint8_t> participant_ids = session->GetNonceParticipants();
            secp256k1_xonly_pubkey part_agg_pk;
            secp256k1_musig_keyagg_cache part_cache;
            if (!m_aggregator->ComputeAggregatePubkey(participant_ids, part_agg_pk, part_cache)) {
                LogPrintf("Oracle: Step 2 - failed to compute participants-only aggregate for epoch %d (%zu participants)\n",
                         epoch, participant_ids.size());
            } else {
                session->SetKeyAggCache(part_cache);
                LogPrintf("Oracle: Step 2 - recomputed keyagg for %zu participants (epoch %d)\n",
                         participant_ids.size(), epoch);
            }

            if (session->AggregateNonces(msg32)) {
                LogPrintf("Oracle: Nonces aggregated for epoch %d, SIGNING\n", epoch);

                OracleManager& om = OracleManager::GetInstance();
                const std::vector<uint32_t> local_ids = om.GetActiveOracleIds();
                secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

                for (uint32_t oid : local_ids) {
                    uint8_t oid8 = static_cast<uint8_t>(oid);
                    if (m_partialsig_broadcast_tracker[epoch].count(oid8)) continue;

                    OracleNode* onode = om.GetOracleNode(oid);
                    if (!onode) continue;
                    CKey key = onode->GetOraclePrivateKey();
                    if (!key.IsValid()) continue;

                    secp256k1_musig_partial_sig partial_sig;
                    if (session->CreatePartialSignature(oid8, key, partial_sig)) {
                        session->AddPartialSignature(oid8, partial_sig);

                        unsigned char ser_psig[32];
                        if (secp256k1_musig_partial_sig_serialize(ctx, ser_psig, &partial_sig)) {
                            OracleMusigPartialSigMsg psig_msg;
                            psig_msg.epoch = epoch;
                            psig_msg.oracle_id = oid8;
                            psig_msg.partial_sig.assign(ser_psig, ser_psig + 32);

                            // RC30: sign the partial-sig message so peers accept it
                            // (same pattern as nonce_msg — RH-24 auth requirement).
                            if (!psig_msg.Sign(key)) {
                                LogPrintf("Oracle: Sign() failed for MuSig2 partial sig oracle=%d epoch=%d\n",
                                         oid8, epoch);
                                continue;
                            }

                            BroadcastMusigPartialSig(psig_msg);
                            m_partialsig_broadcast_tracker[epoch].insert(oid8);

                            LogPrintf("Oracle: Broadcast partial sig for epoch %d (oracle_id=%d)\n",
                                     epoch, oid8);
                        }
                    }
                }
                secp256k1_context_destroy(ctx);
            }
        }
    }

    state = session->GetState();
    if (state == MuSig2SessionState::SIGNING) {
        DrainPendingPartialSigsForEpoch(epoch, *session);
    }

    // ── Step 3: All nodes aggregate when enough partial sigs ──
    if (state == MuSig2SessionState::SIGNING && session->HasEnoughPartialSigs()) {
        std::vector<unsigned char> final_sig;
        if (session->AggregateSignature(final_sig)) {
            LogPrintf("Oracle: MuSig2 COMPLETE for epoch %d, sig size=%zu\n",
                     epoch, final_sig.size());
        }
    }

    // ── Step 4: Timeout check ──
    session->CheckTimeout(block_height);
}

// ============================================================================
// P2P broadcast
// ============================================================================

bool OracleSigningOrchestrator::BroadcastMusigNonce(const OracleMusigNonceMsg& msg)
{
    if (!m_connman) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Cannot broadcast MuSig2 nonce - no P2P\n");
        return false;
    }

    m_connman->ForEachNode([this, &msg](CNode* node) {
        m_connman->PushMessage(node,
            CNetMsgMaker(node->GetCommonVersion()).Make(
                NetMsgType::ORACLEMUSIGNONCE, msg));
    });

    return true;
}

bool OracleSigningOrchestrator::BroadcastMusigPartialSig(const OracleMusigPartialSigMsg& msg)
{
    if (!m_connman) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Cannot broadcast MuSig2 partial sig - no P2P\n");
        return false;
    }

    m_connman->ForEachNode([this, &msg](CNode* node) {
        m_connman->PushMessage(node,
            CNetMsgMaker(node->GetCommonVersion()).Make(
                NetMsgType::ORACLEMUSIGPARTIALSIG, msg));
    });

    return true;
}

// ============================================================================
// Query completed session for block assembly
// ============================================================================

bool OracleSigningOrchestrator::GetCompletedSession(
    int32_t epoch,
    std::vector<unsigned char>& aggregate_sig_out,
    std::vector<unsigned char>& participation_bitmap_out,
    uint64_t& signed_price_out,
    int64_t& signed_timestamp_out) const
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_signing_sessions.find(epoch);
    if (it == m_signing_sessions.end()) return false;

    const MuSig2SigningSession& session = *it->second;
    if (session.GetState() != MuSig2SessionState::COMPLETE) return false;

    aggregate_sig_out = session.GetAggregateSig();
    participation_bitmap_out = session.GetParticipationBitmap();
    signed_price_out = session.GetSignedPrice();
    signed_timestamp_out = session.GetSignedTimestamp();

    // Some nodes can complete aggregation from received partial signatures even
    // if their local signing tick did not persist the signed values before the
    // miner queries the completed session. Recover the same consensus values the
    // signing round used. The resulting bundle is still verified against the
    // aggregate signature before mining/acceptance, so a mismatched recovery
    // cannot create an invalid-but-accepted block; it simply fails validation.
    if ((signed_price_out == 0 || signed_timestamp_out == 0) && g_oracle_bundle_manager) {
        uint64_t consensus_price = 0;
        int64_t consensus_timestamp = 0;
        if (OracleBundleManager::GetInstance().ComputeConsensusValues(consensus_price, consensus_timestamp)) {
            signed_price_out = consensus_price;
            signed_timestamp_out = consensus_timestamp;
            LogPrint(BCLog::DIGIDOLLAR,
                     "Oracle: Recovered missing signed MuSig2 values for completed epoch %d: price=%llu timestamp=%lld\n",
                     epoch,
                     static_cast<unsigned long long>(signed_price_out),
                     static_cast<long long>(signed_timestamp_out));
        }
    }

    return !aggregate_sig_out.empty() && signed_price_out > 0 && signed_timestamp_out > 0;
}

std::optional<OracleSigningOrchestrator::SessionStatus>
OracleSigningOrchestrator::GetSessionStateForEpoch(int32_t epoch) const
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_signing_sessions.find(epoch);
    if (it == m_signing_sessions.end() || !it->second) return std::nullopt;
    const MuSig2SigningSession& session = *it->second;
    SessionStatus status;
    status.state = session.GetState();
    status.nonce_count = session.GetNonceCount();
    status.partial_sig_count = session.GetPartialSigCount();
    status.creation_height = session.GetCreationHeight();
    return status;
}

// ============================================================================
// Utilities
// ============================================================================

int32_t OracleSigningOrchestrator::ComputeEpoch(int32_t block_height, int32_t epoch_length)
{
    if (epoch_length <= 0) return 0;
    return block_height / epoch_length;
}

void OracleSigningOrchestrator::ComputeOracleMessageHash(
    int32_t epoch, uint64_t price, int64_t timestamp,
    unsigned char hash32[32])
{
    // DD-FA-SEC-008 — domain-separate the v0x03 MuSig2 message so that the
    // same (epoch, price, timestamp) signed for one DigiByte chain cannot
    // be replayed on a different chain that shares the oracle roster.
    // The hash binds:
    //   tag           — labeled prefix for defense-in-depth
    //   chain_hash    — Params().GetConsensus().hashGenesisBlock (chain id)
    //   epoch         — RC30 cross-epoch replay binding
    //   price         — consensus DGB/USD price (micro-USD)
    //   timestamp     — bundle timestamp
    //
    // Validator computes the same hash via ComputeOracleBundleHash, which
    // MUST stay byte-identical to this construction for verification to
    // succeed.
    CHashWriter hasher(0);
    hasher << std::string{"DigiDollar/OracleBundle"};
    hasher << Params().GetConsensus().hashGenesisBlock;
    hasher << epoch;
    hasher << price;
    hasher << timestamp;
    uint256 result = hasher.GetHash();
    memcpy(hash32, result.data(), 32);
}
