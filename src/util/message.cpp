// Copyright (c) 2009-2010 Satoshi Nakamoto
<<<<<<< HEAD
// Copyright (c) 2009-2020 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <hash.h>            // For CHashWriter
#include <key.h>             // For CKey
#include <key_io.h>          // For DecodeDestination()
#include <pubkey.h>          // For CPubKey
#include <script/standard.h> // For CTxDestination, IsValidDestination(), PKHash
#include <serialize.h>       // For SER_GETHASH
#include <util/message.h>
#include <util/strencodings.h> // For DecodeBase64()

#include <string>
=======
// Copyright (c) 2009-2022 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <hash.h>
#include <key.h>
#include <key_io.h>
#include <pubkey.h>
#include <uint256.h>
#include <util/message.h>
#include <util/strencodings.h>

#include <cassert>
#include <optional>
#include <string>
#include <variant>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <vector>

/**
 * Text used to signify that a signed message follows and to prevent
 * inadvertently signing a transaction.
 */
const std::string MESSAGE_MAGIC = "DigiByte Signed Message:\n";

MessageVerificationResult MessageVerify(
    const std::string& address,
    const std::string& signature,
    const std::string& message)
{
    CTxDestination destination = DecodeDestination(address);
    if (!IsValidDestination(destination)) {
        return MessageVerificationResult::ERR_INVALID_ADDRESS;
    }

    if (std::get_if<PKHash>(&destination) == nullptr) {
        return MessageVerificationResult::ERR_ADDRESS_NO_KEY;
    }

<<<<<<< HEAD
    bool invalid = false;
    std::vector<unsigned char> signature_bytes = DecodeBase64(signature.c_str(), &invalid);
    if (invalid) {
=======
    auto signature_bytes = DecodeBase64(signature);
    if (!signature_bytes) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
    }

    CPubKey pubkey;
<<<<<<< HEAD
    if (!pubkey.RecoverCompact(MessageHash(message), signature_bytes)) {
        return MessageVerificationResult::ERR_PUBKEY_NOT_RECOVERED;
    }

    if (!(CTxDestination(PKHash(pubkey)) == destination)) {
=======
    if (!pubkey.RecoverCompact(MessageHash(message), *signature_bytes)) {
        return MessageVerificationResult::ERR_PUBKEY_NOT_RECOVERED;
    }

    if (!(PKHash(pubkey) == *std::get_if<PKHash>(&destination))) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        return MessageVerificationResult::ERR_NOT_SIGNED;
    }

    return MessageVerificationResult::OK;
}

bool MessageSign(
    const CKey& privkey,
    const std::string& message,
    std::string& signature)
{
    std::vector<unsigned char> signature_bytes;

    if (!privkey.SignCompact(MessageHash(message), signature_bytes)) {
        return false;
    }

    signature = EncodeBase64(signature_bytes);

    return true;
}

uint256 MessageHash(const std::string& message)
{
<<<<<<< HEAD
    CHashWriter hasher(SER_GETHASH, 0);
=======
    HashWriter hasher{};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    hasher << MESSAGE_MAGIC << message;

    return hasher.GetHash();
}

std::string SigningResultString(const SigningResult res)
{
    switch (res) {
        case SigningResult::OK:
            return "No error";
        case SigningResult::PRIVATE_KEY_NOT_AVAILABLE:
            return "Private key not available";
        case SigningResult::SIGNING_FAILED:
            return "Sign failed";
        // no default case, so the compiler can warn about missing cases
    }
    assert(false);
}
