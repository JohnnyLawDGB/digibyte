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

std::unique_ptr<OracleSigningOrchestrator> g_signing_orchestrator;

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
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_signing_sessions.find(msg.epoch);
    if (it == m_signing_sessions.end() || !it->second) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Ignoring remote nonce for unknown epoch %d\n", msg.epoch);
        return;
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
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_signing_sessions.find(msg.epoch);
    if (it == m_signing_sessions.end() || !it->second) {
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Ignoring remote partial sig for unknown epoch %d\n", msg.epoch);
        return;
    }

    // Deserialize partial sig
    if (msg.partial_sig.size() != 32) return;
    secp256k1_musig_partial_sig partial_sig;
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (secp256k1_musig_partial_sig_parse(ctx, &partial_sig, msg.partial_sig.data())) {
        if (it->second->AddPartialSignature(msg.oracle_id, partial_sig)) {
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
        }
    }
    secp256k1_context_destroy(ctx);
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
    session->SetCreationHeight(block_height > 0 ? block_height : epoch * 50);
    session->SetTimeoutBlocks(100);

    MuSig2SigningSession* ptr = session.get();
    m_signing_sessions[epoch] = std::move(session);

    LogPrintf("Oracle: Created MuSig2 signing session for epoch %d (creation_height=%d)\n",
             epoch, block_height);
    return ptr;
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
    const std::shared_ptr<const CBlock>& block,
    int32_t block_height)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    if (block_height < consensus.nDigiDollarPhase3Height) {
        return;
    }

    int32_t current_epoch = GetCurrentEpoch(block_height);

    CleanupOldSessions(current_epoch);

    MuSig2SigningSession* session = GetOrCreateSigningSession(current_epoch, block_height);
    if (!session) return;

    MuSig2SessionState state = session->GetState();
    bool is_oracle = IsOracleNode();

    LogPrintf("Oracle: OnBlockConnected h=%d epoch=%d state=%d is_oracle=%d\n",
             block_height, current_epoch, static_cast<int>(state), is_oracle);

    if (!m_aggregator) {
        m_aggregator = std::make_unique<MuSig2OracleAggregator>();
    }

    // ── Step 1: Oracle generates nonces for ALL local oracle IDs on new epoch ──
    if (is_oracle && (state == MuSig2SessionState::CREATED || state == MuSig2SessionState::NONCES_COLLECTING)) {
        OracleManager& om = OracleManager::GetInstance();
        const std::vector<uint32_t> local_ids = om.GetActiveOracleIds();

        LogPrintf("Oracle: Step 1 - local_ids.size()=%zu for epoch %d\n", local_ids.size(), current_epoch);

        // Build full oracle ID list for key aggregation
        std::vector<uint8_t> all_oracle_ids;
        const auto& nodes = Params().GetOracleNodes();
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].is_active) all_oracle_ids.push_back(static_cast<uint8_t>(i));
        }

        LogPrintf("Oracle: Step 1 - all_oracle_ids.size()=%zu\n", all_oracle_ids.size());

        secp256k1_xonly_pubkey agg_pk;
        secp256k1_musig_keyagg_cache cache;
        if (m_aggregator->ComputeAggregatePubkey(all_oracle_ids, agg_pk, cache)) {
            LogPrintf("Oracle: Step 1 - key aggregation succeeded for %zu oracle IDs\n", all_oracle_ids.size());
            secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

            for (uint32_t oid : local_ids) {
                uint8_t oid8 = static_cast<uint8_t>(oid);
                // Skip if already generated nonce for this oracle
                if (m_nonce_broadcast_tracker[current_epoch].count(oid8)) {
                    LogPrintf("Oracle: Skipping oracle %d epoch %d - already broadcast\n", oid8, current_epoch);
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
                        nonce_msg.epoch = current_epoch;
                        nonce_msg.oracle_id = oid8;
                        nonce_msg.pubnonce.assign(ser_nonce, ser_nonce + 66);

                        BroadcastMusigNonce(nonce_msg);
                        m_nonce_broadcast_tracker[current_epoch].insert(oid8);

                        LogPrintf("Oracle: Generated and broadcast nonce for epoch %d (oracle_id=%d)\n",
                                 current_epoch, oid8);
                    }
                } else {
                    LogPrintf("Oracle: GenerateNonce FAILED for oracle %d epoch %d\n", oid8, current_epoch);
                }
            }
            secp256k1_context_destroy(ctx);
        } else {
            LogPrintf("Oracle: Failed to compute aggregate pubkey for epoch %d\n", current_epoch);
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
            ComputeOracleMessageHash(current_epoch, consensus_price, consensus_timestamp, msg32);

            if (session->AggregateNonces(msg32)) {
                LogPrintf("Oracle: Nonces aggregated for epoch %d, SIGNING\n", current_epoch);

                OracleManager& om = OracleManager::GetInstance();
                const std::vector<uint32_t> local_ids = om.GetActiveOracleIds();
                secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

                for (uint32_t oid : local_ids) {
                    uint8_t oid8 = static_cast<uint8_t>(oid);
                    if (m_partialsig_broadcast_tracker[current_epoch].count(oid8)) continue;

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
                            psig_msg.epoch = current_epoch;
                            psig_msg.oracle_id = oid8;
                            psig_msg.partial_sig.assign(ser_psig, ser_psig + 32);

                            BroadcastMusigPartialSig(psig_msg);
                            m_partialsig_broadcast_tracker[current_epoch].insert(oid8);

                            LogPrintf("Oracle: Broadcast partial sig for epoch %d (oracle_id=%d)\n",
                                     current_epoch, oid8);
                        }
                    }
                }
                secp256k1_context_destroy(ctx);
            }
        }
    }

    state = session->GetState();

    // ── Step 3: All nodes aggregate when enough partial sigs ──
    if (state == MuSig2SessionState::SIGNING && session->HasEnoughPartialSigs()) {
        std::vector<unsigned char> final_sig;
        if (session->AggregateSignature(final_sig)) {
            LogPrintf("Oracle: MuSig2 COMPLETE for epoch %d, sig size=%zu\n",
                     current_epoch, final_sig.size());
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
    std::vector<unsigned char>& participation_bitmap_out) const
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_signing_sessions.find(epoch);
    if (it == m_signing_sessions.end()) return false;

    const MuSig2SigningSession& session = *it->second;
    if (session.GetState() != MuSig2SessionState::COMPLETE) return false;

    aggregate_sig_out = session.GetAggregateSig();
    participation_bitmap_out = session.GetParticipationBitmap();
    return !aggregate_sig_out.empty();
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
    CHashWriter hasher(0);
    hasher << epoch;
    hasher << price;
    hasher << timestamp;
    uint256 result = hasher.GetHash();
    memcpy(hash32, result.data(), 32);
}
