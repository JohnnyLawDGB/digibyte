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

OracleSigningOrchestrator& OracleSigningOrchestrator::GetInstance()
{
    assert(g_signing_orchestrator);
    return *g_signing_orchestrator;
}

void OracleSigningOrchestrator::Initialize()
{
    assert(!g_signing_orchestrator);
    g_signing_orchestrator = std::make_unique<OracleSigningOrchestrator>();
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

MuSig2SigningSession* OracleSigningOrchestrator::GetOrCreateSigningSession(int32_t epoch)
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);

    auto it = m_signing_sessions.find(epoch);
    if (it != m_signing_sessions.end()) {
        return it->second.get();
    }

    auto session = std::make_unique<MuSig2SigningSession>(
        epoch, static_cast<uint8_t>(ORACLE_CONSENSUS_REQUIRED));
    session->SetTimeoutBlocks(50);

    MuSig2SigningSession* ptr = session.get();
    m_signing_sessions[epoch] = std::move(session);

    LogPrintf("Oracle: Created MuSig2 signing session for epoch %d\n", epoch);
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

    MuSig2SigningSession* session = GetOrCreateSigningSession(current_epoch);
    if (!session) return;

    MuSig2SessionState state = session->GetState();
    bool is_oracle = IsOracleNode();

    if (!m_aggregator) {
        m_aggregator = std::make_unique<MuSig2OracleAggregator>();
    }

    // ── Step 1: Oracle generates nonce on new epoch ──
    if (is_oracle && state == MuSig2SessionState::CREATED) {
        const CKey* our_key = GetOracleSigningKey();
        uint8_t our_id = GetOracleId();

        if (our_key && our_key->IsValid()) {
            std::vector<uint8_t> oracle_ids;
            const auto& nodes = Params().GetOracleNodes();
            for (size_t i = 0; i < nodes.size() && i < ORACLE_ACTIVE_COUNT; ++i) {
                oracle_ids.push_back(static_cast<uint8_t>(i));
            }

            secp256k1_xonly_pubkey agg_pk;
            secp256k1_musig_keyagg_cache cache;
            if (m_aggregator->ComputeAggregatePubkey(oracle_ids, agg_pk, cache)) {
                CPubKey cpk = our_key->GetPubKey();
                secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
                secp256k1_pubkey our_pubkey;
                bool parsed = secp256k1_ec_pubkey_parse(ctx, &our_pubkey, cpk.data(), cpk.size());

                if (parsed) {
                    secp256k1_musig_pubnonce pubnonce;
                    if (session->GenerateNonce(*our_key, our_pubkey, cache, pubnonce)) {
                        session->AddPubnonce(our_id, pubnonce);

                        unsigned char ser_nonce[66];
                        if (secp256k1_musig_pubnonce_serialize(ctx, ser_nonce, &pubnonce)) {
                            OracleMusigNonceMsg nonce_msg;
                            nonce_msg.epoch = current_epoch;
                            nonce_msg.oracle_id = our_id;
                            nonce_msg.pubnonce.assign(ser_nonce, ser_nonce + 66);

                            BroadcastMusigNonce(nonce_msg);
                            m_nonce_broadcast_tracker[current_epoch].insert(our_id);

                            LogPrintf("Oracle: Generated and broadcast nonce for epoch %d (oracle_id=%d)\n",
                                     current_epoch, our_id);
                        }
                    }
                }
                secp256k1_context_destroy(ctx);
            } else {
                LogPrintf("Oracle: Failed to compute aggregate pubkey for epoch %d\n", current_epoch);
            }
        }
    }

    state = session->GetState();

    // ── Step 2: Oracle creates partial sig when nonces complete ──
    if (is_oracle && state == MuSig2SessionState::NONCES_COMPLETE) {
        const CKey* our_key = GetOracleSigningKey();
        uint8_t our_id = GetOracleId();

        if (our_key && our_key->IsValid() &&
            m_partialsig_broadcast_tracker[current_epoch].count(our_id) == 0) {

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

                    secp256k1_musig_partial_sig partial_sig;
                    if (session->CreatePartialSignature(*our_key, partial_sig)) {
                        session->AddPartialSignature(our_id, partial_sig);

                        secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
                        unsigned char ser_psig[32];
                        if (secp256k1_musig_partial_sig_serialize(ctx, ser_psig, &partial_sig)) {
                            OracleMusigPartialSigMsg psig_msg;
                            psig_msg.epoch = current_epoch;
                            psig_msg.oracle_id = our_id;
                            psig_msg.partial_sig.assign(ser_psig, ser_psig + 32);

                            BroadcastMusigPartialSig(psig_msg);
                            m_partialsig_broadcast_tracker[current_epoch].insert(our_id);

                            LogPrintf("Oracle: Broadcast partial sig for epoch %d (oracle_id=%d)\n",
                                     current_epoch, our_id);
                        }
                        secp256k1_context_destroy(ctx);
                    }
                }
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
    CHashWriter hasher(SER_GETHASH, 0);
    hasher << epoch;
    hasher << price;
    hasher << timestamp;
    uint256 result = hasher.GetHash();
    memcpy(hash32, result.data(), 32);
}
