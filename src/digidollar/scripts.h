// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_DIGIDOLLAR_SCRIPTS_H
#define DIGIBYTE_DIGIDOLLAR_SCRIPTS_H

#include <script/script.h>
#include <key.h>
#include <consensus/amount.h>
#include <pubkey.h>
#include <script/standard.h>
#include <uint256.h>

#include <vector>
#include <cstdint>

namespace DigiDollar {

/**
 * Parameters for minting DigiDollars with P2TR collateral locking
 */
struct MintParams {
    CAmount ddAmount;                    //!< Amount of DD to mint (in cents)
    int64_t lockHeight;                  //!< Block height when normally redeemable
    XOnlyPubKey ownerKey;               //!< Owner's taproot public key
    XOnlyPubKey internalKey;            //!< Taproot internal key for MAST construction
    std::vector<XOnlyPubKey> oracleKeys; //!< Oracle public keys (typically 15)

    //! Constructor
    MintParams() : ddAmount(0), lockHeight(0) {}
};

/**
 * Create P2TR collateral locking script with MAST redemption paths
 *
 * This creates a Taproot output with 2 spending conditions:
 * 1. Normal redemption after timelock (system health >= 100%)
 * 2. ERR redemption after timelock when system under-collateralized (health < 100%)
 *
 * CRITICAL: Both paths REQUIRE the timelock (CLTV) to expire first.
 * There is NO early redemption, NO forced liquidation, NO exceptions.
 *
 * NOTE: Partial redemption and Emergency oracle override are NOT supported.
 *
 * @param params Parameters including DD amount, lock period, keys, etc.
 * @return CScript P2TR script (OP_1 + 32-byte taproot output)
 */
CScript CreateCollateralP2TR(const MintParams& params);

/**
 * Create simple P2TR script for DigiDollar token outputs
 *
 * This creates a simple Taproot output for transferring DigiDollars
 * with amount verification and key path spending.
 *
 * @param owner Owner's taproot public key
 * @param ddAmount DigiDollar amount in cents
 * @return CScript P2TR script for DD token
 */
CScript CreateDigiDollarP2TR(const XOnlyPubKey& owner, CAmount ddAmount);

/**
 * Generate oracle public keys (mock implementation for Phase 1)
 *
 * In Phase 1, this generates deterministic keys for testing.
 * In Phase 2, this will connect to real oracle infrastructure.
 *
 * @param count Number of oracle keys to generate (default 15)
 * @return Vector of oracle public keys
 */
std::vector<XOnlyPubKey> GetOracleKeys(size_t count = 15);

/**
 * Create individual redemption path scripts
 * These are combined into the MAST tree by CreateCollateralP2TR
 */

/**
 * Normal redemption path - redeemable after timelock expires
 * Script: <lockHeight> OP_CHECKLOCKTIMEVERIFY OP_DROP OP_DIGIDOLLAR <amount> OP_EQUALVERIFY <ownerKey> OP_CHECKSIG
 */
CScript CreateNormalRedemptionPath(const MintParams& params);

/**
 * Emergency redemption path - 8-of-15 oracle override
 * Script: OP_DIGIDOLLAR <amount> OP_EQUALVERIFY <15 oracle keys with OP_CHECKSIGADD> OP_8 OP_EQUAL
 */
CScript CreateEmergencyPath(const MintParams& params);


/**
 * ERR (Emergency Redemption Ratio) path - when system < 100% collateralized
 * CRITICAL: ERR path REQUIRES timelock expiry first (same as Normal path)
 * Script: <lockHeight> OP_CHECKLOCKTIMEVERIFY OP_DROP OP_CHECKCOLLATERAL <100> OP_LESSTHAN OP_VERIFY OP_DIGIDOLLAR OP_DDVERIFY <ownerKey> OP_CHECKSIG
 */
CScript CreateERRPath(const MintParams& params);

// ============================================================================
// Phase 1 Script Metadata Tracking (internal testing use only)
// ============================================================================
// INTERNAL USE ONLY - not part of public API
// These functions support Phase 1 testing by tracking script metadata.
// Phase 2 will track this in the UTXO database properly.

enum class ScriptType;  // Forward declaration from validation.h

struct ScriptMetadata {
    ScriptType type;
    CAmount ddAmount;
    int64_t lockHeight;
};

void RegisterScriptMetadata(const CScript& script, ScriptType type, CAmount ddAmount, int64_t lockHeight);
bool GetScriptMetadata(const CScript& script, ScriptMetadata& metadata);

} // namespace DigiDollar

#endif // DIGIBYTE_DIGIDOLLAR_SCRIPTS_H