// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_ORACLE_MUSIG2_MESSAGES_H
#define DIGIBYTE_ORACLE_MUSIG2_MESSAGES_H

#include <hash.h>
#include <key.h>
#include <pubkey.h>
#include <serialize.h>
#include <uint256.h>

#include <cstdint>
#include <vector>

class CChainParams;

bool IsAuthorizedMuSig2OracleIdForRelay(const CChainParams& params, uint32_t oracle_id);

/**
 * MuSig2 Nonce Message for P2P Network (Round 1)
 * Carries an oracle's public nonce for the MuSig2 signing protocol.
 */
class OracleMusigNonceMsg
{
public:
    int32_t epoch{0};
    uint8_t oracle_id{0};
    std::vector<unsigned char> pubnonce;  // 66 bytes serialized secp256k1_musig_pubnonce
    std::vector<unsigned char> signature; // 64 bytes Schnorr signature (RH-24)

    SERIALIZE_METHODS(OracleMusigNonceMsg, obj)
    {
        READWRITE(obj.epoch, obj.oracle_id, obj.pubnonce, obj.signature);
    }

    uint256 GetHash() const;

    /** Hash of fields covered by the authentication signature (excludes signature itself). */
    uint256 GetSignatureHash() const;

    /** Sign this message with the oracle's private key. */
    bool Sign(const CKey& key);

    /** Verify the authentication signature against the given oracle pubkey. */
    bool VerifySignature(const XOnlyPubKey& pubkey) const;

    bool IsValid() const { return pubnonce.size() == 66 && oracle_id < 255 && signature.size() == 64; }
};

/**
 * MuSig2 Partial Signature Message for P2P Network (Round 2)
 * Carries an oracle's partial signature for the MuSig2 signing protocol.
 */
class OracleMusigPartialSigMsg
{
public:
    int32_t epoch{0};
    uint8_t oracle_id{0};
    std::vector<unsigned char> partial_sig;  // 32 bytes serialized secp256k1_musig_partial_sig
    std::vector<unsigned char> signature;    // 64 bytes Schnorr signature (RH-24)

    SERIALIZE_METHODS(OracleMusigPartialSigMsg, obj)
    {
        READWRITE(obj.epoch, obj.oracle_id, obj.partial_sig, obj.signature);
    }

    uint256 GetHash() const;

    /** Hash of fields covered by the authentication signature (excludes signature itself). */
    uint256 GetSignatureHash() const;

    /** Sign this message with the oracle's private key. */
    bool Sign(const CKey& key);

    /** Verify the authentication signature against the given oracle pubkey. */
    bool VerifySignature(const XOnlyPubKey& pubkey) const;

    bool IsValid() const { return partial_sig.size() == 32 && oracle_id < 255 && signature.size() == 64; }
};

#endif // DIGIBYTE_ORACLE_MUSIG2_MESSAGES_H
