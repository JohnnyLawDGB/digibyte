// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <digidollar/scripts.h>
#include <script/standard.h>
#include <script/script.h>
#include <key.h>
#include <logging.h>
#include <util/strencodings.h>

#include <algorithm>

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
        std::vector<unsigned char> seed(32, 0);
        seed[0] = static_cast<unsigned char>(i);
        seed[1] = static_cast<unsigned char>(i >> 8);
        key.Set(seed.begin(), seed.end(), true);

        keys.emplace_back(XOnlyPubKey(key.GetPubKey()));
    }

    LogPrintf("DigiDollar: Generated %d oracle keys for testing\n", count);
    return keys;
}

CScript CreateNormalRedemptionPath(const MintParams& params)
{
    if (params.ddAmount <= 0 || params.lockHeight < 0) {
        LogPrintf("DigiDollar: Invalid parameters for normal redemption path\n");
        return CScript();
    }

    CScript script;

    // Normal redemption after timelock
    script << params.lockHeight << OP_CHECKLOCKTIMEVERIFY << OP_DROP;

    // Verify DigiDollar amount
    script << OP_DIGIDOLLAR << params.ddAmount << OP_EQUALVERIFY;

    // Owner signature verification
    script << ToByteVector(params.ownerKey) << OP_CHECKSIG;

    LogPrintf("DigiDollar: Created normal redemption path for %d DD at height %d\n",
              params.ddAmount, params.lockHeight);

    return script;
}

CScript CreateEmergencyPath(const MintParams& params)
{
    if (params.ddAmount <= 0 || params.oracleKeys.empty()) {
        LogPrintf("DigiDollar: Invalid parameters for emergency path\n");
        return CScript();
    }

    CScript script;

    // Verify DigiDollar amount first
    script << OP_DIGIDOLLAR << params.ddAmount << OP_EQUALVERIFY;

    // Add oracle multisig (8-of-15 or 8-of-N)
    size_t oracleCount = std::min(params.oracleKeys.size(), size_t(15));
    for (size_t i = 0; i < oracleCount; i++) {
        script << ToByteVector(params.oracleKeys[i]) << OP_CHECKSIGADD;
    }

    // Require 8 signatures
    script << OP_8 << OP_EQUAL;

    LogPrintf("DigiDollar: Created emergency path with %d oracles for %d DD\n",
              oracleCount, params.ddAmount);

    return script;
}

CScript CreatePartialRedemptionPath(const MintParams& params)
{
    if (params.ddAmount <= 0) {
        LogPrintf("DigiDollar: Invalid parameters for partial redemption path\n");
        return CScript();
    }

    CScript script;

    // DigiDollar verification (allows partial amounts)
    script << OP_DIGIDOLLAR << OP_DDVERIFY;

    // Owner signature verification (must sign before price check)
    script << ToByteVector(params.ownerKey) << OP_CHECKSIGVERIFY;

    // Verify current price from oracles
    script << OP_CHECKPRICE;

    LogPrintf("DigiDollar: Created partial redemption path for %d DD\n", params.ddAmount);

    return script;
}

CScript CreateERRPath(const MintParams& params)
{
    if (params.ddAmount <= 0) {
        LogPrintf("DigiDollar: Invalid parameters for ERR path\n");
        return CScript();
    }

    CScript script;

    // Check if system collateral ratio < 100%
    script << OP_CHECKCOLLATERAL << CScriptNum(100) << OP_LESSTHAN << OP_VERIFY;

    // DigiDollar verification
    script << OP_DIGIDOLLAR << OP_DDVERIFY;

    // Owner signature
    script << ToByteVector(params.ownerKey) << OP_CHECKSIG;

    LogPrintf("DigiDollar: Created ERR path for %d DD\n", params.ddAmount);

    return script;
}

CScript CreateCollateralP2TR(const MintParams& params)
{
    if (params.ddAmount <= 0 || params.lockHeight < 0 || !params.internalKey.IsFullyValid()) {
        LogPrintf("DigiDollar: Invalid parameters for P2TR collateral script\n");
        return CScript();
    }

    try {
        // Use TaprootBuilder to create MAST
        TaprootBuilder builder;

        // Add redemption paths with weights (depth determines probability)
        // Lower depth = higher probability = more frequent use

        // Normal path is most likely (depth 2, weight ~64)
        CScript normalPath = CreateNormalRedemptionPath(params);
        if (!normalPath.empty()) {
            builder.Add(2, normalPath, 0xC0);  // Leaf version 0xC0 for Tapscript
        }

        // Emergency path is rare (depth 4, weight ~4)
        CScript emergencyPath = CreateEmergencyPath(params);
        if (!emergencyPath.empty()) {
            builder.Add(4, emergencyPath, 0xC0);
        }

        // Partial path is medium probability (depth 3, weight ~16)
        CScript partialPath = CreatePartialRedemptionPath(params);
        if (!partialPath.empty()) {
            builder.Add(3, partialPath, 0xC0);
        }

        // ERR path is very rare (depth 5, weight ~2)
        CScript errPath = CreateERRPath(params);
        if (!errPath.empty()) {
            builder.Add(5, errPath, 0xC0);
        }

        // Finalize with internal key
        builder.Finalize(params.internalKey);

        if (!builder.IsValid() || !builder.IsComplete()) {
            LogPrintf("DigiDollar: TaprootBuilder failed to create valid tree\n");
            return CScript();
        }

        // Create P2TR output script
        CScript scriptPubKey;
        WitnessV1Taproot output = builder.GetOutput();

        // P2TR format: OP_1 + 32-byte taproot output
        scriptPubKey << OP_1 << ToByteVector(output);

        LogPrintf("DigiDollar: Created P2TR collateral script for %d DD (size: %d bytes)\n",
                  params.ddAmount, scriptPubKey.size());

        return scriptPubKey;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: Exception creating P2TR script: %s\n", e.what());
        return CScript();
    }
}

CScript CreateDigiDollarP2TR(const XOnlyPubKey& owner, CAmount ddAmount)
{
    if (ddAmount <= 0 || !owner.IsFullyValid()) {
        LogPrintf("DigiDollar: Invalid parameters for DD P2TR script\n");
        return CScript();
    }

    try {
        TaprootBuilder builder;

        // Simple transfer path with DD amount verification
        CScript transferPath;
        transferPath << OP_DIGIDOLLAR << ddAmount << OP_EQUALVERIFY;
        transferPath << ToByteVector(owner) << OP_CHECKSIG;

        // Single script tree (depth 0)
        builder.Add(0, transferPath, 0xC0);
        builder.Finalize(owner);

        if (!builder.IsValid() || !builder.IsComplete()) {
            LogPrintf("DigiDollar: Failed to create DD P2TR script\n");
            return CScript();
        }

        // Create P2TR output
        CScript scriptPubKey;
        WitnessV1Taproot output = builder.GetOutput();

        scriptPubKey << OP_1 << ToByteVector(output);

        LogPrintf("DigiDollar: Created DD P2TR script for %d DD (size: %d bytes)\n",
                  ddAmount, scriptPubKey.size());

        return scriptPubKey;

    } catch (const std::exception& e) {
        LogPrintf("DigiDollar: Exception creating DD P2TR script: %s\n", e.what());
        return CScript();
    }
}

} // namespace DigiDollar