// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/musig2_session.h>

#include <random.h>
#include <support/cleanse.h>

#include <secp256k1_musig.h>

#include <cassert>
#include <cstring>

MuSig2SigningSession::MuSig2SigningSession(int32_t epoch, uint8_t min_signers)
    : m_epoch(epoch),
      m_min_signers(min_signers),
      m_state(MuSig2SessionState::CREATED),
      m_has_secnonce(false),
      m_secnonce_used(false),
      m_creation_height(epoch),
      m_timeout_blocks(20)
{
    m_ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    assert(m_ctx != nullptr);
    memset(&m_secnonce, 0, sizeof(m_secnonce));
    memset(&m_keyagg_cache, 0, sizeof(m_keyagg_cache));
    memset(&m_aggnonce, 0, sizeof(m_aggnonce));
    memset(&m_session, 0, sizeof(m_session));
}

MuSig2SigningSession::~MuSig2SigningSession()
{
    // Always zero the secret nonce in destructor as safety net
    memory_cleanse(&m_secnonce, sizeof(m_secnonce));
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

    m_secnonce = other.m_secnonce;
    memory_cleanse(&other.m_secnonce, sizeof(other.m_secnonce));
    m_has_secnonce = other.m_has_secnonce;
    other.m_has_secnonce = false;
    m_secnonce_used = other.m_secnonce_used;

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
    memory_cleanse(&m_secnonce, sizeof(m_secnonce));
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

    m_secnonce = other.m_secnonce;
    memory_cleanse(&other.m_secnonce, sizeof(other.m_secnonce));
    m_has_secnonce = other.m_has_secnonce;
    other.m_has_secnonce = false;
    m_secnonce_used = other.m_secnonce_used;

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
// Round 1a: Generate local nonce
// ============================================================================

bool MuSig2SigningSession::GenerateNonce(const CKey& signing_key,
                                         const secp256k1_pubkey& pubkey,
                                         const secp256k1_musig_keyagg_cache& cache,
                                         secp256k1_musig_pubnonce& pubnonce_out)
{
    LOCK(m_mutex);

    if (m_state != MuSig2SessionState::CREATED) return false;
    if (!signing_key.IsValid()) return false;
    if (m_has_secnonce) return false; // already generated

    // Store the key aggregation cache for later use
    m_keyagg_cache = cache;

    // Generate session randomness
    unsigned char session_secrand[32];
    GetStrongRandBytes(Span{session_secrand, 32});

    // Generate secnonce + pubnonce
    // Passing seckey provides "misuse resistance" per secp256k1 docs
    if (!secp256k1_musig_nonce_gen(m_ctx,
                                    &m_secnonce,
                                    &pubnonce_out,
                                    session_secrand,
                                    signing_key.begin(), // 32-byte raw secret key
                                    &pubkey,
                                    nullptr,  // msg32 — not known yet
                                    &m_keyagg_cache,
                                    nullptr)) { // extra_input32
        memory_cleanse(&m_secnonce, sizeof(m_secnonce));
        return false;
    }

    m_has_secnonce = true;
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

    // Validate pubnonce by attempting to serialize it
    // A zeroed/invalid pubnonce will fail serialization
    unsigned char ser[66];
    if (!secp256k1_musig_pubnonce_serialize(m_ctx, ser, &pubnonce)) {
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

bool MuSig2SigningSession::CreatePartialSignature(const CKey& signing_key,
                                                   secp256k1_musig_partial_sig& partial_sig_out)
{
    LOCK(m_mutex);

    if (m_state != MuSig2SessionState::SIGNING) return false;
    if (!signing_key.IsValid()) return false;
    if (!m_has_secnonce || m_secnonce_used) return false;

    // Create keypair from CKey for secp256k1_musig_partial_sign
    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(m_ctx, &keypair, signing_key.begin())) {
        return false;
    }

    // Sign — this ZEROES m_secnonce on success (secp256k1 guarantee)
    int ret = secp256k1_musig_partial_sign(m_ctx,
                                            &partial_sig_out,
                                            &m_secnonce,
                                            &keypair,
                                            &m_keyagg_cache,
                                            &m_session);

    // Zero the keypair regardless of success
    memory_cleanse(&keypair, sizeof(keypair));

    if (!ret) {
        // Explicitly zero secnonce on failure too
        memory_cleanse(&m_secnonce, sizeof(m_secnonce));
        m_secnonce_used = true;
        return false;
    }

    // secnonce was zeroed by secp256k1_musig_partial_sign, but belt-and-suspenders
    memory_cleanse(&m_secnonce, sizeof(m_secnonce));
    m_secnonce_used = true;
    return true;
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

    m_partial_sigs[oracle_id] = partial_sig;
    return true;
}

bool MuSig2SigningSession::HasEnoughPartialSigs() const
{
    LOCK(m_mutex);
    return m_partial_sigs.size() >= m_min_signers;
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

    m_state = MuSig2SessionState::COMPLETE;
    return true;
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
        // Zero secnonce if still held
        if (m_has_secnonce && !m_secnonce_used) {
            memory_cleanse(&m_secnonce, sizeof(m_secnonce));
            m_secnonce_used = true;
        }
        m_state = MuSig2SessionState::FAILED;
    }
}

void MuSig2SigningSession::SetTimeoutBlocks(int32_t blocks)
{
    LOCK(m_mutex);
    m_timeout_blocks = blocks;
}
