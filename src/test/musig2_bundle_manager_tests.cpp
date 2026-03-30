// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <consensus/amount.h>
#include <oracle/bundle_manager.h>
#include <primitives/oracle.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>

#include <vector>

namespace {

COracleBundle MakeV03Bundle()
{
    COracleBundle bundle;
    bundle.version = 3;
    bundle.median_price_micro_usd = 51000;
    bundle.timestamp = 1700000000;
    bundle.participation_bitmap = {0xFF, 0x01};

    bundle.aggregate_sig.resize(64);
    for (size_t i = 0; i < 64; ++i) {
        bundle.aggregate_sig[i] = static_cast<unsigned char>(i ^ 0x5A);
    }

    return bundle;
}

CTransaction MakeCoinbaseTx(const CScript& oracle_script)
{
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 50 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CTxOut oracle_out;
    oracle_out.nValue = 0;
    oracle_out.scriptPubKey = oracle_script;
    coinbase.vout.push_back(oracle_out);

    return CTransaction(coinbase);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(musig2_bundle_manager_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(create_oracle_script_v03)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();

    COracleBundle bundle = MakeV03Bundle();

    CScript script = manager.CreateOracleScript(bundle);
    BOOST_REQUIRE(!script.empty());

    // OP_RETURN OP_ORACLE <push:0x03>
    BOOST_CHECK_EQUAL(script[0], OP_RETURN);
    BOOST_CHECK_EQUAL(script[1], OP_ORACLE);
    BOOST_CHECK_EQUAL(script[2], 0x01);
    BOOST_CHECK_EQUAL(script[3], 0x03);
}

BOOST_AUTO_TEST_CASE(extract_oracle_bundle_v03)
{
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    manager.Clear();

    const COracleBundle bundle = MakeV03Bundle();
    const CScript script = manager.CreateOracleScript(bundle);
    BOOST_REQUIRE(!script.empty());

    const CTransaction tx = MakeCoinbaseTx(script);

    COracleBundle extracted;
    BOOST_REQUIRE(manager.ExtractOracleBundle(tx, extracted));

    BOOST_CHECK_EQUAL(extracted.version, 3);
    BOOST_CHECK(extracted.IsMuSig2());
    BOOST_CHECK_EQUAL(extracted.messages.size(), 9);
    BOOST_CHECK_EQUAL(extracted.messages.front().oracle_id, 0);
    BOOST_CHECK_EQUAL(extracted.messages.back().oracle_id, 8);
    BOOST_CHECK_EQUAL(extracted.median_price_micro_usd, bundle.median_price_micro_usd);
    BOOST_CHECK_EQUAL(extracted.timestamp, bundle.timestamp);
    BOOST_CHECK(extracted.participation_bitmap == bundle.participation_bitmap);
    BOOST_CHECK(extracted.aggregate_sig == bundle.aggregate_sig);
}

BOOST_AUTO_TEST_CASE(v03_round_trip_serialization)
{
    COracleBundle original = MakeV03Bundle();

    const std::vector<unsigned char> encoded = original.SerializeV03Data();
    BOOST_REQUIRE(!encoded.empty());

    COracleBundle decoded;
    BOOST_REQUIRE(COracleBundle::DeserializeV03Data(encoded, decoded));

    BOOST_CHECK_EQUAL(decoded.median_price_micro_usd, original.median_price_micro_usd);
    BOOST_CHECK_EQUAL(decoded.timestamp, original.timestamp);
    BOOST_CHECK(decoded.participation_bitmap == original.participation_bitmap);
    BOOST_CHECK(decoded.aggregate_sig == original.aggregate_sig);
}

BOOST_AUTO_TEST_SUITE_END()
