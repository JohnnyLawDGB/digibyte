// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <oracle/musig2_messages.h>
#include <primitives/oracle.h>
#include <protocol.h>
#include <test/util/setup_common.h>

#include <algorithm>

namespace {

struct ScopedParamsRestore {
    ChainType original{Params().GetChainType()};
    ~ScopedParamsRestore() { SelectParams(original); }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(musig2_net_processing_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(musig2_netmsg_types_are_registered)
{
    const auto& msg_types = getAllNetMessageTypes();

    BOOST_CHECK(std::find(msg_types.begin(), msg_types.end(), NetMsgType::ORACLEMUSIGNONCE) != msg_types.end());
    BOOST_CHECK(std::find(msg_types.begin(), msg_types.end(), NetMsgType::ORACLEMUSIGPARTIALSIG) != msg_types.end());
}

BOOST_AUTO_TEST_CASE(musig2_inv_types_map_to_wire_commands)
{
    const CInv inv_nonce{MSG_ORACLE_MUSIG_NONCE, uint256::ZERO};
    BOOST_CHECK(inv_nonce.IsOracleMsg());
    BOOST_CHECK_EQUAL(inv_nonce.GetCommand(), NetMsgType::ORACLEMUSIGNONCE);

    const CInv inv_psig{MSG_ORACLE_MUSIG_PARTIALSIG, uint256::ZERO};
    BOOST_CHECK(inv_psig.IsOracleMsg());
    BOOST_CHECK_EQUAL(inv_psig.GetCommand(), NetMsgType::ORACLEMUSIGPARTIALSIG);
}

BOOST_AUTO_TEST_CASE(musig2_messages_validate_expected_field_lengths)
{
    // RH-24: IsValid() now requires a 64-byte signature field
    OracleMusigNonceMsg nonce_msg;
    nonce_msg.epoch = 123;
    nonce_msg.oracle_id = 1;
    nonce_msg.pubnonce.assign(66, 0x22);
    nonce_msg.signature.assign(64, 0x00); // signature present (validity of sig checked separately)
    BOOST_CHECK(nonce_msg.IsValid());

    nonce_msg.pubnonce.assign(65, 0x22);
    BOOST_CHECK(!nonce_msg.IsValid());

    // Missing signature must fail
    OracleMusigNonceMsg no_sig_msg;
    no_sig_msg.epoch = 123;
    no_sig_msg.oracle_id = 1;
    no_sig_msg.pubnonce.assign(66, 0x22);
    BOOST_CHECK(!no_sig_msg.IsValid());

    OracleMusigPartialSigMsg psig_msg;
    psig_msg.epoch = 123;
    psig_msg.oracle_id = 1;
    psig_msg.partial_sig.assign(32, 0x33);
    psig_msg.signature.assign(64, 0x00);
    BOOST_CHECK(psig_msg.IsValid());

    psig_msg.partial_sig.assign(31, 0x33);
    BOOST_CHECK(!psig_msg.IsValid());

    // Missing signature must fail
    OracleMusigPartialSigMsg no_sig_psig;
    no_sig_psig.epoch = 123;
    no_sig_psig.oracle_id = 1;
    no_sig_psig.partial_sig.assign(32, 0x33);
    BOOST_CHECK(!no_sig_psig.IsValid());
}

BOOST_AUTO_TEST_CASE(musig2_relay_authorization_rejects_reserve_slots)
{
    ScopedParamsRestore restore;

    SelectParams(ChainType::MAIN);
    BOOST_REQUIRE_EQUAL(Params().GetConsensus().nOraclePubkeyCount, 21);
    BOOST_CHECK(IsAuthorizedMuSig2OracleIdForRelay(Params(), 20));
    BOOST_CHECK_MESSAGE(!IsAuthorizedMuSig2OracleIdForRelay(Params(), 21),
        "mainnet reserve slot 21 must not be accepted for MuSig2 relay");
    BOOST_CHECK_MESSAGE(!IsAuthorizedMuSig2OracleIdForRelay(Params(), 29),
        "mainnet reserve slot 29 must not be accepted for MuSig2 relay");
    BOOST_CHECK(!IsAuthorizedMuSig2OracleIdForRelay(Params(), ORACLE_TOTAL_COUNT));

    SelectParams(ChainType::TESTNET);
    BOOST_REQUIRE_EQUAL(Params().GetConsensus().nOraclePubkeyCount, 21);
    BOOST_CHECK(IsAuthorizedMuSig2OracleIdForRelay(Params(), 20));
    BOOST_CHECK(!IsAuthorizedMuSig2OracleIdForRelay(Params(), 21));

    SelectParams(ChainType::REGTEST);
    BOOST_REQUIRE_EQUAL(Params().GetConsensus().nOraclePubkeyCount, 7);
    BOOST_CHECK(IsAuthorizedMuSig2OracleIdForRelay(Params(), 6));
    BOOST_CHECK(!IsAuthorizedMuSig2OracleIdForRelay(Params(), 7));
}

BOOST_AUTO_TEST_SUITE_END()
