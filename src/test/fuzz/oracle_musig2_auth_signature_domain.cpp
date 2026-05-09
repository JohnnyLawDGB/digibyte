// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <key.h>
#include <oracle/musig2_messages.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <uint256.h>
#include <util/chaintype.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void initialize_oracle_musig2_auth_signature_domain()
{
    ECC_Start();
    SelectParams(ChainType::REGTEST);
}

CKey FixedOracleAuthKey(unsigned char fill)
{
    std::array<unsigned char, 32> key_bytes;
    key_bytes.fill(fill);
    CKey key;
    key.Set(key_bytes.begin(), key_bytes.end(), true);
    assert(key.IsValid());
    return key;
}

std::vector<unsigned char> ConsumeFixedBytes(FuzzedDataProvider& fdp, size_t len)
{
    std::vector<unsigned char> out = fdp.ConsumeBytes<unsigned char>(len);
    out.resize(len, 0);
    return out;
}

template <typename Msg, typename MutatePayload>
void AssertAuthDomain(Msg msg, const CKey& key, const XOnlyPubKey& pubkey, MutatePayload&& mutate_payload)
{
    SelectParams(ChainType::REGTEST);
    assert(msg.Sign(key));
    assert(msg.IsValid());
    assert(msg.VerifySignature(pubkey));

    Msg mutated_epoch = msg;
    mutated_epoch.epoch ^= 1;
    assert(!mutated_epoch.VerifySignature(pubkey));

    Msg mutated_id = msg;
    mutated_id.oracle_id ^= 1;
    assert(!mutated_id.VerifySignature(pubkey));

    Msg mutated_payload = msg;
    mutate_payload(mutated_payload);
    assert(!mutated_payload.VerifySignature(pubkey));

    Msg mutated_sig = msg;
    mutated_sig.signature[0] ^= 0x01;
    assert(!mutated_sig.VerifySignature(pubkey));

    const CKey wrong_key = FixedOracleAuthKey(0x22);
    const XOnlyPubKey wrong_pubkey(wrong_key.GetPubKey());
    assert(!msg.VerifySignature(wrong_pubkey));

    SelectParams(ChainType::TESTNET);
    assert(!msg.VerifySignature(pubkey));

    SelectParams(ChainType::REGTEST);
    assert(msg.VerifySignature(pubkey));
}

} // namespace

FUZZ_TARGET(oracle_musig2_auth_signature_domain, .init = initialize_oracle_musig2_auth_signature_domain)
{
    FuzzedDataProvider fdp(buffer.data(), buffer.size());

    const CKey key = FixedOracleAuthKey(0x11);
    const XOnlyPubKey pubkey(key.GetPubKey());

    OracleMusigNonceMsg nonce_msg;
    nonce_msg.epoch = fdp.ConsumeIntegral<int32_t>();
    nonce_msg.oracle_id = fdp.ConsumeIntegralInRange<uint8_t>(0, 254);
    nonce_msg.pubnonce = ConsumeFixedBytes(fdp, 66);
    AssertAuthDomain(nonce_msg, key, pubkey, [](OracleMusigNonceMsg& msg) {
        msg.pubnonce[0] ^= 0x01;
    });

    OracleMusigPartialSigMsg partial_sig_msg;
    partial_sig_msg.epoch = fdp.ConsumeIntegral<int32_t>();
    partial_sig_msg.oracle_id = fdp.ConsumeIntegralInRange<uint8_t>(0, 254);
    partial_sig_msg.partial_sig = ConsumeFixedBytes(fdp, 32);
    AssertAuthDomain(partial_sig_msg, key, pubkey, [](OracleMusigPartialSigMsg& msg) {
        msg.partial_sig[0] ^= 0x01;
    });
}
