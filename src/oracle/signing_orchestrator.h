// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_ORACLE_SIGNING_ORCHESTRATOR_H
#define DIGIBYTE_ORACLE_SIGNING_ORCHESTRATOR_H

#include <chain.h>
#include <key.h>
#include <oracle/musig2_aggregator.h>
#include <oracle/musig2_messages.h>
#include <oracle/musig2_session.h>
#include <validationinterface.h>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>

class CBlock;
class CConnman;

/**
 * MuSig2 Signing Orchestrator (Phase 3)
 *
 * Drives the MuSig2 signing protocol on every block tick via
 * CValidationInterface::BlockConnected.
 *
 * Oracle nodes:  generate nonces, create partial sigs, broadcast.
 * Non-oracle:    collect nonces/sigs, aggregate final signature.
 */
class OracleSigningOrchestrator : public CValidationInterface
{
public:
    OracleSigningOrchestrator();
    ~OracleSigningOrchestrator();

    OracleSigningOrchestrator(const OracleSigningOrchestrator&) = delete;
    OracleSigningOrchestrator& operator=(const OracleSigningOrchestrator&) = delete;

    // Oracle detection
    bool IsOracleNode() const;
    const CKey* GetOracleSigningKey() const;
    uint8_t GetOracleId() const;

    // Session management
    MuSig2SigningSession* GetOrCreateSigningSession(int32_t epoch);
    void CleanupOldSessions(int32_t current_epoch);

    // Block-tick orchestration
    void OnBlockConnected(const std::shared_ptr<const CBlock>& block, int32_t block_height);

    // P2P broadcast
    bool BroadcastMusigNonce(const OracleMusigNonceMsg& msg);
    bool BroadcastMusigPartialSig(const OracleMusigPartialSigMsg& msg);
    void SetConnman(CConnman* connman) { m_connman = connman; }

    // Query completed session for block assembly
    bool GetCompletedSession(int32_t epoch,
                             std::vector<unsigned char>& aggregate_sig_out,
                             std::vector<unsigned char>& participation_bitmap_out) const;

    // Utilities
    static int32_t ComputeEpoch(int32_t block_height, int32_t epoch_length);
    static void ComputeOracleMessageHash(int32_t epoch, uint64_t price,
                                         int64_t timestamp,
                                         unsigned char hash32[32]);

    // Lifecycle
    void Start();
    void Stop();
    void Clear();

    /** Inject a pre-built session (test use only). Takes ownership. */
    void InjectSession(int32_t epoch, std::unique_ptr<MuSig2SigningSession> session);

    static OracleSigningOrchestrator& GetInstance();
    static void Initialize();
    static void Shutdown();

protected:
    void BlockConnected(ChainstateRole role,
                        const std::shared_ptr<const CBlock>& block,
                        const CBlockIndex* pindex) override;

private:
    CConnman* m_connman{nullptr};
    std::map<int32_t, std::unique_ptr<MuSig2SigningSession>> m_signing_sessions;
    mutable std::mutex m_sessions_mutex;
    std::unique_ptr<MuSig2OracleAggregator> m_aggregator;
    std::map<int32_t, std::set<uint8_t>> m_nonce_broadcast_tracker;
    std::map<int32_t, std::set<uint8_t>> m_partialsig_broadcast_tracker;
    mutable std::unique_ptr<CKey> m_cached_oracle_key;
    mutable uint8_t m_cached_oracle_id{255};
    mutable bool m_oracle_key_cached{false};
    bool m_started{false};
};

extern std::unique_ptr<OracleSigningOrchestrator> g_signing_orchestrator;

#endif // DIGIBYTE_ORACLE_SIGNING_ORCHESTRATOR_H
