// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <digidollar/scripts.h>
#include <digidollar/validation.h>
#include <script/standard.h>
#include <script/script.h>
#include <key.h>
#include <logging.h>
#include <util/strencodings.h>
#include <util/hasher.h>
#include <sync.h>

#include <algorithm>
#include <map>

namespace DigiDollar {

std::vector<XOnlyPubKey> GetOracleKeys(size_t count)
{
    std::vector<XOnlyPubKey> keys;
    keys.reserve(count);

    // Generate deterministic keys for testing (Phase 1)
    // In Phase 2, this will connect to real oracle infrastructure
    for (size_t i = 0; i < count; i++) {
        CKey key;
        // Use deterministic seed based on index for consistent testing
        // Initialize with a valid base seed and modify it to make it unique per index
        std::vector<unsigned char> seed(32);
        // Start with a valid seed base (0x01 repeated) to ensure validity
        for (size_t j = 0; j < 32; j++) {
            seed[j] = static_cast<unsigned char>((i + 1 + j) % 256);
        }
        // Ensure the key is non-zero and within the valid secp256k1 range
        seed[31] = static_cast<unsigned char>((i + 1) % 255 + 1);

        key.Set(seed.begin(), seed.end(), true);

        // Verify the key was initialized successfully
        if (!key.IsValid()) {
            // Fallback: use MakeNewKey with deterministic seed
            key.MakeNewKey(true);
        }

        keys.emplace_back(XOnlyPubKey(key.GetPubKey()));
    }

    // Don't log during test setup to avoid logging initialization issues
    // // LogPrintf("DigiDollar: Generated %d oracle keys for testing\n", count);
    return keys;
}

CScript CreateNormalRedemptionPath(const MintParams& params)
{
    if (params.ddAmount <= 0 || params.lockHeight < 0) {
        // // LogPrintf("DigiDollar: Invalid parameters for normal redemption path\n");
        return CScript();
    }

    CScript script;

    // Normal redemption after timelock
    script << params.lockHeight << OP_CHECKLOCKTIMEVERIFY << OP_DROP;

    // Owner signature verification
    // DD amount validation happens at transaction validation layer, not in script
    script << ToByteVector(params.ownerKey) << OP_CHECKSIG;

    // // LogPrintf("DigiDollar: Created normal redemption path for %d DD at height %d\n",
    //           params.ddAmount, params.lockHeight);

    return script;
}

CScript CreateEmergencyPath(const MintParams& params)
{
    if (params.ddAmount <= 0 || params.oracleKeys.empty()) {
        // LogPrintf("DigiDollar: Invalid parameters for emergency path\n");
        return CScript();
    }

    CScript script;

    // Verify DigiDollar amount first
    // Use CScriptNum to ensure proper minimal encoding without OP_SUCCESSx bytes
    script << OP_DIGIDOLLAR << CScriptNum(params.ddAmount) << OP_EQUALVERIFY;

    // Add oracle multisig (8-of-15 or 8-of-N)
    size_t oracleCount = std::min(params.oracleKeys.size(), size_t(15));
    for (size_t i = 0; i < oracleCount; i++) {
        script << ToByteVector(params.oracleKeys[i]) << OP_CHECKSIGADD;
    }

    // Require 8 signatures
    script << OP_8 << OP_EQUAL;

    // LogPrintf("DigiDollar: Created emergency path with %d oracles for %d DD\n",
    //           oracleCount, params.ddAmount);

    return script;
}

CScript CreatePartialRedemptionPath(const MintParams& params)
{
    if (params.ddAmount <= 0) {
        // LogPrintf("DigiDollar: Invalid parameters for partial redemption path\n");
        return CScript();
    }

    CScript script;

    // DigiDollar verification (allows partial amounts)
    script << OP_DIGIDOLLAR << OP_DDVERIFY;

    // Owner signature verification (must sign before price check)
    script << ToByteVector(params.ownerKey) << OP_CHECKSIGVERIFY;

    // Verify current price from oracles
    script << OP_CHECKPRICE;

    // LogPrintf("DigiDollar: Created partial redemption path for %d DD\n", params.ddAmount);

    return script;
}

CScript CreateERRPath(const MintParams& params)
{
    if (params.ddAmount <= 0) {
        // LogPrintf("DigiDollar: Invalid parameters for ERR path\n");
        return CScript();
    }

    CScript script;

    // Check if system collateral ratio < 100%
    script << OP_CHECKCOLLATERAL << CScriptNum(100) << OP_LESSTHAN << OP_VERIFY;

    // DigiDollar verification
    script << OP_DIGIDOLLAR << OP_DDVERIFY;

    // Owner signature
    script << ToByteVector(params.ownerKey) << OP_CHECKSIG;

    // LogPrintf("DigiDollar: Created ERR path for %d DD\n", params.ddAmount);

    return script;
}

CScript CreateCollateralP2TR(const MintParams& params)
{
    if (params.ddAmount <= 0 || params.lockHeight < 0 || !params.internalKey.IsFullyValid()) {
        // LogPrintf("DigiDollar: Invalid parameters for P2TR collateral script\n");
        return CScript();
    }

    try {
        // Use TaprootBuilder to create MAST
        TaprootBuilder builder;

        // Add redemption paths with valid depths that form a proper binary tree
        // For a 4-leaf tree, valid depth combinations are:
        // - All at depth 2: (2,2,2,2) - balanced tree
        // - Mixed: (1,2,3,3) or (2,2,2,2) - valid structures

        // Normal path (most common) - depth 1
        CScript normalPath = CreateNormalRedemptionPath(params);
        if (!normalPath.empty()) {
            builder.Add(1, normalPath, 0xC0);  // Leaf version 0xC0 for Tapscript
        }

        // Partial path (medium) - depth 2
        CScript partialPath = CreatePartialRedemptionPath(params);
        if (!partialPath.empty()) {
            builder.Add(2, partialPath, 0xC0);
        }

        // Emergency path (rare) - depth 3
        CScript emergencyPath = CreateEmergencyPath(params);
        if (!emergencyPath.empty()) {
            builder.Add(3, emergencyPath, 0xC0);
        }

        // ERR path (very rare) - depth 3
        CScript errPath = CreateERRPath(params);
        if (!errPath.empty()) {
            builder.Add(3, errPath, 0xC0);
        }

        // Finalize with internal key
        builder.Finalize(params.internalKey);

        if (!builder.IsValid() || !builder.IsComplete()) {
            // LogPrintf("DigiDollar: TaprootBuilder failed to create valid tree\n");
            return CScript();
        }

        // Create P2TR output script
        CScript scriptPubKey;
        WitnessV1Taproot output = builder.GetOutput();

        // P2TR format: OP_1 + 32-byte taproot output
        scriptPubKey << OP_1 << ToByteVector(output);

        // LogPrintf("DigiDollar: Created P2TR collateral script for %d DD (size: %d bytes)\n",
        //           params.ddAmount, scriptPubKey.size());

        // Phase 1: Register metadata for testing
        RegisterScriptMetadata(scriptPubKey, DigiDollar::ScriptType::COLLATERAL_LOCK, params.ddAmount, params.lockHeight);

        return scriptPubKey;

    } catch (const std::exception& e) {
        // LogPrintf("DigiDollar: Exception creating P2TR script: %s\n", e.what());
        return CScript();
    }
}

CScript CreateDigiDollarP2TR(const XOnlyPubKey& owner, CAmount ddAmount)
{
    if (ddAmount <= 0 || !owner.IsFullyValid()) {
        // LogPrintf("DigiDollar: Invalid parameters for DD P2TR script\n");
        return CScript();
    }

    try {
        // Standard Taproot P2TR output with tweaked key
        // The owner's x-only pubkey is tweaked with nullptr merkle root (key-path only)
        // This is standard BIP-341 behavior for simple P2TR outputs.
        //
        // When signing, the wallet must apply the same tweak to the private key.
        // For minted DD: owner key is stored, tweak is applied during signing
        // For received DD: wallet already knows the tweaked key from the output
        auto tweaked = owner.CreateTapTweak(nullptr);  // nullptr = no merkle root, key-path only
        if (!tweaked) {
            return CScript();
        }
        XOnlyPubKey output_key = tweaked->first;

        // Create P2TR output with the TWEAKED key (standard Taproot)
        CScript scriptPubKey;
        scriptPubKey << OP_1 << ToByteVector(output_key);

        // LogPrintf("DigiDollar: Created DD P2TR script for %d DD (size: %d bytes)\n",
        //           ddAmount, scriptPubKey.size());

        // Phase 1: Register metadata for testing
        RegisterScriptMetadata(scriptPubKey, DigiDollar::ScriptType::DD_TOKEN_OUTPUT, ddAmount, 0);

        return scriptPubKey;

    } catch (const std::exception& e) {
        // LogPrintf("DigiDollar: Exception creating DD P2TR script: %s\n", e.what());
        return CScript();
    }
}

// ============================================================================
// Phase 1 Script Metadata Tracking
// ============================================================================
// IMPORTANT: This is a Phase 1 testing workaround. In production (Phase 2),
// DD amounts and script types should be tracked in the UTXO database.
// This global map allows tests to identify scripts created by Create*P2TR functions.

static std::map<uint256, ScriptMetadata> g_scriptMetadataMap;
static RecursiveMutex g_scriptMetadataMutex;

void RegisterScriptMetadata(const CScript& script, DigiDollar::ScriptType type, CAmount ddAmount, int64_t lockHeight) {
    uint256 scriptHash = Hash(script);
    LOCK(g_scriptMetadataMutex);
    g_scriptMetadataMap[scriptHash] = {type, ddAmount, lockHeight};
}

bool GetScriptMetadata(const CScript& script, ScriptMetadata& metadata) {
    uint256 scriptHash = Hash(script);
    LOCK(g_scriptMetadataMutex);
    auto it = g_scriptMetadataMap.find(scriptHash);
    if (it != g_scriptMetadataMap.end()) {
        metadata = it->second;
        return true;
    }
    return false;
}

} // namespace DigiDollar