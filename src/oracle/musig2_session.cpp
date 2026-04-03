// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/musig2_session.h>

#include <chainparams.h>
#include <random.h>
#include <support/cleanse.h>

#include <secp256k1_musig.h>

#include <cassert>
#include <cstring>

MuSig2SigningSession::MuSig2SigningSession(int32_t epoch, uint8_t min_signers)
    : m_epoch(epoch),
      m_min_signers(min_signers),
      m_state(MuSig2SessionState::CREATED),
      m_creation_height(epoch),
      m_timeout_blocks(20)
{
    m_ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    assert(m_ctx != nullptr);
    memset(&m_keyagg_cache, 0, sizeof(m_keyagg_cache));
    memset(&m_aggnonce, 0, sizeof(m_aggnonce));
    memset(&m_session, 0, sizeof(m_session));
}

MuSig2SigningSession::~MuSig2SigningSession()
{
    // Zero all secret nonces in destructor as safety net
    for (auto& [id, nonce] : m_secnonces) {
        memory_cleanse(&nonce, sizeof(nonce));
    }
    if (m_ctx) {
        secp256k1_context_destroy(m_ctx);
        m_ctx = nullptr;
    }
}

MuSig2SigningSession::MuSig2SigningSession(MuSig2SigningSession&& other) noexcept
    : m_epoch(other.m_epoch),
      m_min_signers(other.m_min_signers),
      m_ctx(nullptr)
{
    LOCK(other.m_mutex);
    m_state = other.m_state;
    m_ctx = other.m_ctx;
    other.m_ctx = nullptr;

    m_secnonces = std::move(other.m_secnonces);
    m_secnonces_used = std::move(other.m_secnonces_used);

    m_keyagg_cache = other.m_keyagg_cache;
    m_pubnonces = std::move(other.m_pubnonces);
    m_aggnonce = other.m_aggnonce;
    m_session = other.m_session;
    m_partial_sigs = std::move(other.m_partial_sigs);
    m_creation_height = other.m_creation_height;
    m_timeout_blocks = other.m_timeout_blocks;

    other.m_state = MuSig2SessionState::FAILED;
}

MuSig2SigningSession& MuSig2SigningSession::operator=(MuSig2SigningSession&& other) noexcept
{
    if (this == &other) return *this;

    // Clean up current state
    for (auto& [id, nonce] : m_secnonces) {
        memory_cleanse(&nonce, sizeof(nonce));
    }
    m_secnonces.clear();
    m_secnonces_used.clear();
    if (m_ctx) {
        secp256k1_context_destroy(m_ctx);
        m_ctx = nullptr;
    }

    LOCK(other.m_mutex);
    m_epoch = other.m_epoch;
    m_min_signers = other.m_min_signers;
    m_state = other.m_state;
    m_ctx = other.m_ctx;
    other.m_ctx = nullptr;

    m_secnonces = std::move(other.m_secnonces);
    m_secnonces_used = std::move(other.m_secnonces_used);

    m_keyagg_cache = other.m_keyagg_cache;
    m_pubnonces = std::move(other.m_pubnonces);
    m_aggnonce = other.m_aggnonce;
    m_session = other.m_session;
    m_partial_sigs = std::move(other.m_partial_sigs);
    m_creation_height = other.m_creation_height;
    m_timeout_blocks = other.m_timeout_blocks;

    other.m_state = MuSig2SessionState::FAILED;
    return *this;
}

MuSig2SessionState MuSig2SigningSession::GetState() const
{
    LOCK(m_mutex);
    return m_state;
}

int32_t MuSig2SigningSession::GetEpoch() const
{
    return m_epoch; // immutable, no lock needed
}

// ============================================================================
// Passive initialization (non-oracle nodes)
// ============================================================================

bool MuSig2SigningSession::InitializePassive(const secp256k1_musig_keyagg_cache& cache)
{
    LOCK(m_mutex);
    if (m_state != MuSig2SessionState::CREATED) return false;
    m_keyagg_cache = cache;
    // Passive nodes have no local secret nonces
    m_state = MuSig2SessionState::NONCES_COLLECTING;
    return true;
}

// ============================================================================
// Round 1a: Generate local nonce
// ============================================================================

bool MuSig2SigningSession::GenerateNonce(uint8_t oracle_id,
                                         const CKey& signing_key,
                                         const secp256k1_pubkey& pubkey,
                                         const secp256k1_musig_keyagg_cache& cache,
                                         secp256k1_musig_pubnonce& pubnonce_out)
{
    LOCK(m_mutex);

    // Allow CREATED (first call) or NONCES_COLLECTING (subsequent local oracles)
    if (m_state != MuSig2SessionState::CREATED &&
        m_state != MuSig2SessionState::NONCES_COLLECTING) return false;
    if (!signing_key.IsValid()) return false;
    if (m_secnonces.count(oracle_id)) return false; // already generated for this oracle

    // Store the key aggregation cache (same for all local oracles)
    m_keyagg_cache = cache;

    // Generate session randomness
    unsigned char session_secrand[32];
    GetStrongRandBytes(Span{session_secrand, 32});

    // Generate secnonce + pubnonce
    secp256k1_musig_secnonce secnonce;
    if (!secp256k1_musig_nonce_gen(m_ctx,
                                    &secnonce,
                                    &pubnonce_out,
                                    session_secrand,
                                    signing_key.begin(),
                                    &pubkey,
                                    nullptr,  // msg32 — not known yet
                                    &m_keyagg_cache,
                                    nullptr)) {
        memory_cleanse(&secnonce, sizeof(secnonce));
        return false;
    }

    m_secnonces[oracle_id] = secnonce;
    memory_cleanse(&secnonce, sizeof(secnonce)); // clear stack copy
    m_state = MuSig2SessionState::NONCES_COLLECTING;
    return true;
}

// ============================================================================
// Round 1b: Collect pubnonces
// ============================================================================

bool MuSig2SigningSession::AddPubnonce(uint8_t oracle_id,
                                       const secp256k1_musig_pubnonce& pubnonce)
{
    LOCK(m_mutex);

    if (m_state != MuSig2SessionState::NONCES_COLLECTING &&
        m_state != MuSig2SessionState::NONCES_COMPLETE) {
        return false;
    }

    // Reject duplicate oracle ID
    if (m_pubnonces.count(oracle_id)) return false;

    // Reject oracle IDs outside configured active set to prevent malformed
    // participation bitmaps and invalid signer transitions.
    const uint16_t total_oracles = static_cast<uint16_t>(Params().GetConsensus().nOracleTotalOracles);
    if (total_oracles == 0 || oracle_id >= total_oracles) return false;

    // Validate pubnonce by checking secp256k1 internal magic bytes.
    // secp256k1_musig_pubnonce_serialize calls abort() via ARG_CHECK on
    // invalid data instead of returning false, so check magic directly.
    static const unsigned char pubnonce_magic[4] = {0xf5, 0x7a, 0x3d, 0xa0};
    if (memcmp(pubnonce.data, pubnonce_magic, 4) != 0) {
        return false;
    }

    m_pubnonces[oracle_id] = pubnonce;

    // Check if we have enough nonces
    if (m_pubnonces.size() >= m_min_signers) {
        m_state = MuSig2SessionState::NONCES_COMPLETE;
    }

    return true;
}

bool MuSig2SigningSession::HasEnoughNonces() const
{
    LOCK(m_mutex);
    return m_pubnonces.size() >= m_min_signers;
}

size_t MuSig2SigningSession::GetNonceCount() const
{
    LOCK(m_mutex);
    return m_pubnonces.size();
}

// ============================================================================
// Aggregate nonces + initialize signing session
// ============================================================================

bool MuSig2SigningSession::AggregateNonces(const unsigned char* msg32)
{
    LOCK(m_mutex);

    if (m_state != MuSig2SessionState::NONCES_COMPLETE) return false;
    if (!msg32) return false;

    // Build sorted array of pubnonce pointers (sorted by oracle_id via std::map)
    std::vector<const secp256k1_musig_pubnonce*> pubnonce_ptrs;
    pubnonce_ptrs.reserve(m_pubnonces.size());
    for (const auto& [id, nonce] : m_pubnonces) {
        pubnonce_ptrs.push_back(&nonce);
    }

    // Aggregate all pubnonces
    if (!secp256k1_musig_nonce_agg(m_ctx, &m_aggnonce,
                                    pubnonce_ptrs.data(), pubnonce_ptrs.size())) {
        m_state = MuSig2SessionState::FAILED;
        return false;
    }

    // Initialize the MuSig2 session with the aggregate nonce and message
    if (!secp256k1_musig_nonce_process(m_ctx, &m_session, &m_aggnonce,
                                        msg32, &m_keyagg_cache)) {
        m_state = MuSig2SessionState::FAILED;
        return false;
    }

    m_state = MuSig2SessionState::SIGNING;
    return true;
}

// ============================================================================
// Round 2a: Create local partial signature
// ============================================================================

bool MuSig2SigningSession::CreatePartialSignature(uint8_t oracle_id,
                                                   const CKey& signing_key,
                                                   secp256k1_musig_partial_sig& partial_sig_out)
{
    LOCK(m_mutex);

    if (m_state != MuSig2SessionState::SIGNING) return false;
    if (!signing_key.IsValid()) return false;

    // Check we have a secnonce for this oracle and it hasn't been used
    auto nonce_it = m_secnonces.find(oracle_id);
    if (nonce_it == m_secnonces.end()) return false;
    if (m_secnonces_used.count(oracle_id)) return false;

    // Create keypair from CKey for secp256k1_musig_partial_sign
    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(m_ctx, &keypair, signing_key.begin())) {
        return false;
    }

    // Sign — this ZEROES the secnonce on success (secp256k1 guarantee)
    int ret = secp256k1_musig_partial_sign(m_ctx,
                                            &partial_sig_out,
                                            &nonce_it->second,
                                            &keypair,
                                            &m_keyagg_cache,
                                            &m_session);

    // Zero the keypair regardless of success
    memory_cleanse(&keypair, sizeof(keypair));

    // Mark this oracle's nonce as consumed and cleanse
    memory_cleanse(&nonce_it->second, sizeof(nonce_it->second));
    m_secnonces.erase(nonce_it);
    m_secnonces_used.insert(oracle_id);

    return ret != 0;
}

// ============================================================================
// Round 2b: Collect partial signatures
// ============================================================================

bool MuSig2SigningSession::AddPartialSignature(uint8_t oracle_id,
                                                const secp256k1_musig_partial_sig& partial_sig)
{
    LOCK(m_mutex);

    if (m_state != MuSig2SessionState::SIGNING) return false;

    // Reject duplicate oracle ID
    if (m_partial_sigs.count(oracle_id)) return false;

    // Reject oracle IDs outside configured active set to keep signer-set
    // transitions aligned with on-chain v0x03 bitmap semantics.
    const uint16_t total_oracles = static_cast<uint16_t>(Params().GetConsensus().nOracleTotalOracles);
    if (total_oracles == 0 || oracle_id >= total_oracles) return false;

    m_partial_sigs[oracle_id] = partial_sig;
    return true;
}

bool MuSig2SigningSession::HasEnoughPartialSigs() const
{
    LOCK(m_mutex);
    return m_partial_sigs.size() >= m_min_signers;
}

size_t MuSig2SigningSession::GetPartialSigCount() const
{
    LOCK(m_mutex);
    return m_partial_sigs.size();
}

// ============================================================================
// Aggregate partial signatures → final 64-byte Schnorr signature
// ============================================================================

bool MuSig2SigningSession::AggregateSignature(std::vector<unsigned char>& sig64_out)
{
    LOCK(m_mutex);

    if (m_state != MuSig2SessionState::SIGNING) return false;
    if (m_partial_sigs.size() < m_min_signers) return false;

    // Build sorted array of partial sig pointers (sorted by oracle_id via std::map)
    std::vector<const secp256k1_musig_partial_sig*> psig_ptrs;
    psig_ptrs.reserve(m_partial_sigs.size());
    for (const auto& [id, sig] : m_partial_sigs) {
        psig_ptrs.push_back(&sig);
    }

    sig64_out.resize(64);
    if (!secp256k1_musig_partial_sig_agg(m_ctx, sig64_out.data(),
                                          &m_session, psig_ptrs.data(),
                                          psig_ptrs.size())) {
        m_state = MuSig2SessionState::FAILED;
        return false;
    }

    m_aggregate_sig = sig64_out;
    m_state = MuSig2SessionState::COMPLETE;
    return true;
}

std::vector<unsigned char> MuSig2SigningSession::GetAggregateSig() const
{
    LOCK(m_mutex);
    if (m_state != MuSig2SessionState::COMPLETE) return {};
    return m_aggregate_sig;
}

std::vector<unsigned char> MuSig2SigningSession::GetParticipationBitmap() const
{
    LOCK(m_mutex);
    if (m_state != MuSig2SessionState::COMPLETE) return {};
    if (m_partial_sigs.empty()) return {};
    uint8_t max_id = 0;
    for (const auto& [id, sig] : m_partial_sigs) {
        if (id > max_id) max_id = id;
    }
    size_t bitmap_bytes = (max_id / 8) + 1;
    std::vector<unsigned char> bitmap(bitmap_bytes, 0);
    for (const auto& [id, sig] : m_partial_sigs) {
        bitmap[id / 8] |= (1 << (id % 8));
    }
    return bitmap;
}


// ============================================================================
// Timeout management
// ============================================================================

void MuSig2SigningSession::CheckTimeout(int32_t current_height)
{
    LOCK(m_mutex);

    if (m_state == MuSig2SessionState::COMPLETE ||
        m_state == MuSig2SessionState::FAILED) {
        return;
    }

    if (current_height >= m_creation_height + m_timeout_blocks) {
        // Zero all held secret nonces on timeout
        for (auto& [id, nonce] : m_secnonces) {
            memory_cleanse(&nonce, sizeof(nonce));
        }
        m_secnonces.clear();
        m_state = MuSig2SessionState::FAILED;
    }
}

void MuSig2SigningSession::SetTimeoutBlocks(int32_t blocks)
{
    LOCK(m_mutex);
    m_timeout_blocks = blocks;
}
