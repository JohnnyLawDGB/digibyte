// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * MuSig2 Phase 3 Activation & Oracle Configuration Tests
 *
 * Tests Phase 3 (MuSig2 aggregate signatures) activation heights,
 * oracle pubkey configuration, and ValidateOracleConfiguration():
 * - Per-network activation heights (mainnet, testnet, regtest)
 * - Oracle pubkey count, consensus threshold, uniqueness, validity, sort order
 * - Phase 2 backward compatibility before Phase 3
 * - Phase 3 activation logic via IsMuSig2OracleActive()
 * - ValidateOracleConfiguration() on all networks
 * - Bundle version gating at Phase 3 boundary
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <kernel/chainparams.h>
#include <primitives/oracle.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string>

namespace {

/**
 * Bundle version acceptance per phase-aware gating rules:
 * - v0x03 only accepted at/after Phase 3 activation
 * - v0x01/v0x02 always accepted (backward compatibility)
 */
static bool IsBundleVersionAccepted(uint8_t bundle_version, int32_t block_height, const Consensus::Params& params)
{
    if (bundle_version == 3) {
        return params.IsMuSig2OracleActive(block_height);
    }
    return true;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(musig2_activation_tests, BasicTestingSetup)

// ============================================================================
// PART 1: Phase 3 Activation Height Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_phase3_activation_mainnet)
{
    SelectParams(ChainType::MAIN);
    const auto& params = Params().GetConsensus();
    // Mainnet uses MuSig2 immediately on top of 7-signature oracle consensus.
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 0);
}

BOOST_AUTO_TEST_CASE(test_phase3_activation_testnet)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    // Testnet also switches immediately to MuSig2 (7 signatures).
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 0);
}

BOOST_AUTO_TEST_CASE(test_phase3_activation_regtest)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    // Regtest now uses MuSig2 immediately, too.
    BOOST_CHECK_EQUAL(params.nDigiDollarMuSig2Height, 0);
}

// ============================================================================
// PART 2: Oracle Configuration Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_oracle_pubkey_count_and_total_slots)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    // Testnet uses the active oracle pubkey prefix inside a 35-slot roster.
    BOOST_CHECK_EQUAL(params.nOraclePubkeyCount, 22);
    BOOST_CHECK_EQUAL(params.nOracleTotalOracles, 35);
    BOOST_CHECK_EQUAL(static_cast<int>(params.vOraclePublicKeys.size()), params.nOraclePubkeyCount);
}

BOOST_AUTO_TEST_CASE(test_oracle_consensus_required_is_7)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    // 7 signatures are required from the active consensus keyset.
    BOOST_CHECK_EQUAL(params.nOracleConsensusRequired, 7);
    BOOST_CHECK_LE(params.nOracleConsensusRequired, params.nOraclePubkeyCount);
}

BOOST_AUTO_TEST_CASE(test_oracle_pubkey_uniqueness)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    std::set<std::string> unique_keys(params.vOraclePublicKeys.begin(), params.vOraclePublicKeys.end());
    BOOST_CHECK_EQUAL(unique_keys.size(), params.vOraclePublicKeys.size());
}

BOOST_AUTO_TEST_CASE(test_oracle_pubkey_validity)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    for (size_t i = 0; i < params.vOraclePublicKeys.size(); ++i) {
        const std::string& hex = params.vOraclePublicKeys[i];
        // X-only pubkeys are 32 bytes = 64 hex characters
        BOOST_CHECK_MESSAGE(hex.size() == 64,
            "Oracle pubkey " + std::to_string(i) + " wrong length: " + std::to_string(hex.size()));
        // Must parse as valid hex
        std::vector<unsigned char> data = ParseHex(hex);
        BOOST_CHECK_EQUAL(data.size(), 32u);
        // Construct compressed pubkey (prepend 0x02) and validate
        std::vector<unsigned char> compressed(33);
        compressed[0] = 0x02;
        std::copy(data.begin(), data.end(), compressed.begin() + 1);
        CPubKey pubkey(compressed);
        BOOST_CHECK_MESSAGE(pubkey.IsValid(),
            "Oracle pubkey " + std::to_string(i) + " invalid: " + hex);
    }
}

BOOST_AUTO_TEST_CASE(test_oracle_config_unique_by_pubkey)
{
    // RC30: vOraclePublicKeys is ordered by oracle slot (0..N-1) — matches
    // vOracleNodes and the MuSig2 participation bitmap. BIP-327 key aggregation
    // sorts internally, so consensus pubkey ordering is slot-based, not
    // lexicographic. We still require per-slot uniqueness.
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    BOOST_REQUIRE_GE(params.vOraclePublicKeys.size(), 2u);
    std::set<std::string> seen;
    for (size_t i = 0; i < params.vOraclePublicKeys.size(); ++i) {
        auto [it, inserted] = seen.insert(params.vOraclePublicKeys[i]);
        BOOST_CHECK_MESSAGE(inserted,
            "Duplicate oracle pubkey at slot " + std::to_string(i) + ": " +
            params.vOraclePublicKeys[i].substr(0, 8));
    }
}

BOOST_AUTO_TEST_CASE(testnet_rc43_slots_17_to_20_are_active)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    const auto& nodes = Params().GetOracleNodes();

    constexpr const char* DIGIBYTE_MAXI_COMPRESSED =
        "03649d750bcad5b42b3dd0f11c8d98d62ed5afd515cd986663f81c35f086e58d47";
    constexpr const char* DIGIBYTE_MAXI_XONLY =
        "649d750bcad5b42b3dd0f11c8d98d62ed5afd515cd986663f81c35f086e58d47";
    constexpr const char* ANTHONY_COMPRESSED =
        "0345f8cb22dfde6aff8f18552c338256e0df551ca2df007f6449d6da1dbb7f4d89";
    constexpr const char* MBAH_JAMBON_COMPRESSED =
        "031758a6d7f1f87c95d1a4a38415608d41463a504ea28da7c6129e2a9d654add42";
    constexpr const char* CAMDEN_COMPRESSED =
        "03018c81746d6ddc326c993d9f2e7f2015554e97a261f3b5fe637ac5098f421a4c";
    constexpr const char* ANTHONY_XONLY =
        "45f8cb22dfde6aff8f18552c338256e0df551ca2df007f6449d6da1dbb7f4d89";
    constexpr const char* MBAH_JAMBON_XONLY =
        "1758a6d7f1f87c95d1a4a38415608d41463a504ea28da7c6129e2a9d654add42";
    constexpr const char* CAMDEN_XONLY =
        "018c81746d6ddc326c993d9f2e7f2015554e97a261f3b5fe637ac5098f421a4c";

    BOOST_REQUIRE_EQUAL(params.nOraclePubkeyCount, 22);
    BOOST_REQUIRE_EQUAL(params.nOracleConsensusRequired, 7);
    BOOST_REQUIRE_EQUAL(params.vOraclePublicKeys.size(), 21U);
    BOOST_REQUIRE_GE(nodes.size(), 21U);

    BOOST_CHECK_EQUAL(params.vOraclePublicKeys[17], DIGIBYTE_MAXI_XONLY);
    BOOST_CHECK_EQUAL(params.vOraclePublicKeys[18], ANTHONY_XONLY);
    BOOST_CHECK_EQUAL(params.vOraclePublicKeys[19], MBAH_JAMBON_XONLY);
    BOOST_CHECK_EQUAL(params.vOraclePublicKeys[20], CAMDEN_XONLY);
    BOOST_CHECK_EQUAL(nodes[17].id, 17U);
    BOOST_CHECK(nodes[17].is_active);
    BOOST_CHECK_EQUAL(HexStr(Span<const unsigned char>(
                          nodes[17].pubkey.data(), nodes[17].pubkey.size())),
                      DIGIBYTE_MAXI_COMPRESSED);
    BOOST_CHECK_EQUAL(HexStr(Span<const unsigned char>(
                          nodes[18].pubkey.data(), nodes[18].pubkey.size())),
                      ANTHONY_COMPRESSED);
    BOOST_CHECK_EQUAL(HexStr(Span<const unsigned char>(
                          nodes[19].pubkey.data(), nodes[19].pubkey.size())),
                      MBAH_JAMBON_COMPRESSED);
    BOOST_CHECK_EQUAL(HexStr(Span<const unsigned char>(
                          nodes[20].pubkey.data(), nodes[20].pubkey.size())),
                      CAMDEN_COMPRESSED);
}

// ============================================================================
// PART 3: Phase Transition Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_phase2_and_phase3_can_be_active_together_on_regtest)
{
    // With RC27 cleanup, regtest also has immediate Phase 3 activation.
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(Consensus::IsOracleActive(params, params.nDDActivationHeight));
    BOOST_CHECK(Consensus::IsMuSig2Active(params, params.nDDActivationHeight));
    BOOST_CHECK(Consensus::IsMuSig2Active(params, params.nDDActivationHeight - 1));
    BOOST_CHECK(params.IsMuSig2OracleActive(0));
    BOOST_CHECK(!params.IsMuSig2OracleActive(-1));
}

BOOST_AUTO_TEST_CASE(test_phase3_active_after_height)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    int phase3 = params.nDigiDollarMuSig2Height;
    BOOST_CHECK(Consensus::IsMuSig2Active(params, phase3));
    BOOST_CHECK(Consensus::IsMuSig2Active(params, phase3 + 1));
    BOOST_CHECK(Consensus::IsMuSig2Active(params, phase3 + 10000));
    BOOST_CHECK(!Consensus::IsMuSig2Active(params, phase3 - 1));
    BOOST_CHECK(!Consensus::IsMuSig2Active(params, phase3 - 1000));
}

// ============================================================================
// PART 4: ValidateOracleConfiguration Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_validate_oracle_config_testnet)
{
    SelectParams(ChainType::TESTNET);
    BOOST_CHECK(Consensus::ValidateOracleConfiguration(Params().GetConsensus()));
}

BOOST_AUTO_TEST_CASE(test_validate_oracle_config_regtest)
{
    SelectParams(ChainType::REGTEST);
    BOOST_CHECK(Consensus::ValidateOracleConfiguration(Params().GetConsensus()));
}

BOOST_AUTO_TEST_CASE(test_validate_oracle_config_mainnet)
{
    SelectParams(ChainType::MAIN);
    BOOST_CHECK(Consensus::ValidateOracleConfiguration(Params().GetConsensus()));
}

// ============================================================================
// PART 5: Bundle Version Gating
// ============================================================================

BOOST_AUTO_TEST_CASE(test_v02_bundle_before_phase3)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(IsBundleVersionAccepted(2, params.nDigiDollarMuSig2Height - 1, params));
}

BOOST_AUTO_TEST_CASE(test_v03_bundle_before_phase3)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(!IsBundleVersionAccepted(3, params.nDigiDollarMuSig2Height - 1, params));
}

BOOST_AUTO_TEST_CASE(test_v03_bundle_at_activation)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(IsBundleVersionAccepted(3, params.nDigiDollarMuSig2Height, params));
}

BOOST_AUTO_TEST_CASE(test_v02_bundle_after_phase3)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(IsBundleVersionAccepted(2, params.nDigiDollarMuSig2Height + 1, params));
}

BOOST_AUTO_TEST_CASE(test_v03_bundle_after_phase3)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(IsBundleVersionAccepted(3, params.nDigiDollarMuSig2Height + 1, params));
}

BOOST_AUTO_TEST_CASE(test_off_by_one_activation)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    int32_t N = params.nDigiDollarMuSig2Height;
    BOOST_CHECK(!params.IsMuSig2OracleActive(N - 1));
    BOOST_CHECK(params.IsMuSig2OracleActive(N));
    BOOST_CHECK(!IsBundleVersionAccepted(3, N - 1, params));
    BOOST_CHECK(IsBundleVersionAccepted(3, N, params));
}

// ============================================================================
// PART 6: Cross-Network Consistency
// ============================================================================

BOOST_AUTO_TEST_CASE(test_regtest_earliest_activation)
{
    SelectParams(ChainType::REGTEST);
    int32_t regtest = Params().GetConsensus().nDigiDollarMuSig2Height;
    SelectParams(ChainType::TESTNET);
    int32_t testnet = Params().GetConsensus().nDigiDollarMuSig2Height;
    SelectParams(ChainType::MAIN);
    int32_t mainnet = Params().GetConsensus().nDigiDollarMuSig2Height;
    // All RC27 networks should use MuSig2 immediately.
    BOOST_CHECK_EQUAL(regtest, 0);
    BOOST_CHECK_EQUAL(testnet, 0);
    BOOST_CHECK_EQUAL(mainnet, 0);
}

BOOST_AUTO_TEST_SUITE_END()
