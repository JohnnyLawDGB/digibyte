// Copyright (c) 2014-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/odocrypt.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>

#include <cassert>
#include <cstdint>
#include <cstring>

// --------------------------------------------------------------------------
// fuzz_odocrypt_cipher — Fuzz OdoCrypt encrypt/decrypt roundtrip.
// Verifies that Decrypt(Encrypt(plain)) == plain for all fuzzed keys and data.
// --------------------------------------------------------------------------
FUZZ_TARGET(fuzz_odocrypt_cipher)
{
    FuzzedDataProvider fdp(buffer.data(), buffer.size());

    if (fdp.remaining_bytes() < sizeof(uint32_t) + OdoCrypt::DIGEST_SIZE)
        return;

    uint32_t key = fdp.ConsumeIntegral<uint32_t>();

    // Consume exactly DIGEST_SIZE bytes for plaintext
    auto plain_bytes = fdp.ConsumeBytes<uint8_t>(OdoCrypt::DIGEST_SIZE);
    if (plain_bytes.size() < (size_t)OdoCrypt::DIGEST_SIZE) return;

    char plain[OdoCrypt::DIGEST_SIZE];
    char cipher[OdoCrypt::DIGEST_SIZE];
    char decrypted[OdoCrypt::DIGEST_SIZE];

    std::memcpy(plain, plain_bytes.data(), OdoCrypt::DIGEST_SIZE);

    // Construct cipher with fuzzed key — exercises S-box/P-box generation
    OdoCrypt odo(key);

    // Encrypt
    odo.Encrypt(cipher, plain);

    // Decrypt
    odo.Decrypt(decrypted, cipher);

    // Roundtrip must reproduce original plaintext
    assert(std::memcmp(plain, decrypted, OdoCrypt::DIGEST_SIZE) == 0);

    // Additional: encrypting different data should (almost certainly) produce
    // different ciphertext — not asserted because it's probabilistic, but
    // we exercise the path
    if (fdp.remaining_bytes() >= OdoCrypt::DIGEST_SIZE) {
        auto alt_bytes = fdp.ConsumeBytes<uint8_t>(OdoCrypt::DIGEST_SIZE);
        if (alt_bytes.size() == (size_t)OdoCrypt::DIGEST_SIZE) {
            char alt_plain[OdoCrypt::DIGEST_SIZE];
            char alt_cipher[OdoCrypt::DIGEST_SIZE];
            std::memcpy(alt_plain, alt_bytes.data(), OdoCrypt::DIGEST_SIZE);
            odo.Encrypt(alt_cipher, alt_plain);
            // Just exercise it — no assertion on distinctness
        }
    }
}

// --------------------------------------------------------------------------
// fuzz_odocrypt_keygen — Fuzz OdoCrypt constructor with many different keys.
// Verifies no crash/UB during S-box/P-box/rotation/round-key generation.
// Also does a basic encrypt to exercise the generated tables.
// --------------------------------------------------------------------------
FUZZ_TARGET(fuzz_odocrypt_keygen)
{
    FuzzedDataProvider fdp(buffer.data(), buffer.size());

    LIMITED_WHILE(fdp.remaining_bytes() >= sizeof(uint32_t), 1000) {
        uint32_t key = fdp.ConsumeIntegral<uint32_t>();

        // Construct — exercises all key-dependent generation
        OdoCrypt odo(key);

        // Do a basic encrypt with zero data to exercise the generated tables
        char plain[OdoCrypt::DIGEST_SIZE] = {};
        char cipher[OdoCrypt::DIGEST_SIZE] = {};
        odo.Encrypt(cipher, plain);

        // Decrypt back to verify roundtrip even with zero input
        char roundtrip[OdoCrypt::DIGEST_SIZE] = {};
        odo.Decrypt(roundtrip, cipher);
        assert(std::memcmp(plain, roundtrip, OdoCrypt::DIGEST_SIZE) == 0);
    }
}
