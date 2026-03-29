// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_ORACLE_MUSIG2_SESSION_MINING_H
#define DIGIBYTE_ORACLE_MUSIG2_SESSION_MINING_H

/**
 * MuSig2 Session Mining Extensions (W2-A2)
 *
 * This header declares additions to MuSig2SigningSession needed by
 * AddOracleBundleToBlock for Phase 3 (v0x03) bundle creation:
 *
 * 1. GetAggregateSig() / GetParticipationBitmap() — getters for COMPLETE sessions
 * 2. g_oracle_signing_sessions — global session map keyed by epoch
 * 3. g_oracle_signing_sessions_mutex — mutex protecting the map
 *
 * These are declared in musig2_session.h and defined in musig2_session.cpp
 * when those files are up to date. This header exists as a bridge to ensure
 * the declarations are always available even during concurrent development.
 */

#include <oracle/musig2_session.h>
#include <sync.h>

#include <cstdint>
#include <map>
#include <vector>

// Forward-declare global session map if not already declared in musig2_session.h
#ifndef DIGIBYTE_MUSIG2_SESSION_GLOBALS_DECLARED
#define DIGIBYTE_MUSIG2_SESSION_GLOBALS_DECLARED
extern std::map<int32_t, MuSig2SigningSession> g_oracle_signing_sessions;
extern Mutex g_oracle_signing_sessions_mutex;
#endif

#endif // DIGIBYTE_ORACLE_MUSIG2_SESSION_MINING_H
