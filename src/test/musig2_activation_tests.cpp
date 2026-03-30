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
 * - Phase 3 activation logic via IsPhaseThreeActive()
 * - ValidateOracleConfiguration() on all networks
 * - Bundle version gating at Phase 3 boundary
 */

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <kernel/chainparams.h>
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
        return params.IsPhaseThreeActive(block_height);
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
    // Mainnet Phase 3 is not yet activated — stays at default max
    // until testnet validation is complete. Oracle config is pre-staged.
    BOOST_CHECK_EQUAL(params.nDigiDollarPhase3Height, 9999999);
}

BOOST_AUTO_TEST_CASE(test_phase3_activation_testnet)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    // Testnet Phase 3 activates at block 1000 (early for integration testing)
    BOOST_CHECK_EQUAL(params.nDigiDollarPhase3Height, 50000);
    BOOST_CHECK_GE(params.nDigiDollarPhase3Height, params.nDigiDollarPhase2Height);
}

BOOST_AUTO_TEST_CASE(test_phase3_activation_regtest)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    // Regtest Phase 3 activates at block 10 (very low for unit tests)
    BOOST_CHECK_EQUAL(params.nDigiDollarPhase3Height, 1000);
}

// ============================================================================
// PART 2: Oracle Configuration Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_oracle_pubkey_count_is_15)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    // 15 oracle pubkeys configured for Phase 3 MuSig2
    BOOST_CHECK_EQUAL(params.nOraclePubkeyCount, 15);
    BOOST_CHECK_EQUAL(static_cast<int>(params.vOraclePublicKeys.size()), params.nOraclePubkeyCount);
}

BOOST_AUTO_TEST_CASE(test_oracle_consensus_required_is_9)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    // 9-of-15 MuSig2 quorum for Phase 3
    BOOST_CHECK_EQUAL(params.nOracleConsensusRequired, 9);
    BOOST_CHECK_GT(params.nOracleConsensusRequired, params.nOraclePubkeyCount / 2);
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

BOOST_AUTO_TEST_CASE(test_oracle_config_sorted_by_pubkey)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    BOOST_REQUIRE_GE(params.vOraclePublicKeys.size(), 2u);
    for (size_t i = 1; i < params.vOraclePublicKeys.size(); ++i) {
        BOOST_CHECK_MESSAGE(params.vOraclePublicKeys[i - 1] < params.vOraclePublicKeys[i],
            "Not sorted at " + std::to_string(i - 1) + ": " +
            params.vOraclePublicKeys[i - 1].substr(0, 8) + " >= " +
            params.vOraclePublicKeys[i].substr(0, 8));
    }
}

// ============================================================================
// PART 3: Phase Transition Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_phase2_still_works_before_phase3)
{
    // Use testnet where Phase 2 < Phase 3
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK_LT(params.nDigiDollarPhase2Height, params.nDigiDollarPhase3Height);
    // At Phase 2 height: oracle active, Phase 3 NOT
    BOOST_CHECK(Consensus::IsOracleActive(params, params.nDigiDollarPhase2Height));
    BOOST_CHECK(!Consensus::IsPhase3Active(params, params.nDigiDollarPhase2Height));
    // One block before Phase 3: still Phase 2 only
    BOOST_CHECK(!Consensus::IsPhase3Active(params, params.nDigiDollarPhase3Height - 1));
}

BOOST_AUTO_TEST_CASE(test_phase3_active_after_height)
{
    SelectParams(ChainType::TESTNET);
    const auto& params = Params().GetConsensus();
    int phase3 = params.nDigiDollarPhase3Height;
    BOOST_CHECK(Consensus::IsPhase3Active(params, phase3));
    BOOST_CHECK(Consensus::IsPhase3Active(params, phase3 + 1));
    BOOST_CHECK(Consensus::IsPhase3Active(params, phase3 + 10000));
    BOOST_CHECK(!Consensus::IsPhase3Active(params, phase3 - 1));
    BOOST_CHECK(!Consensus::IsPhase3Active(params, 0));
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
    BOOST_CHECK(IsBundleVersionAccepted(2, params.nDigiDollarPhase3Height - 1, params));
}

BOOST_AUTO_TEST_CASE(test_v03_bundle_before_phase3)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(!IsBundleVersionAccepted(3, params.nDigiDollarPhase3Height - 1, params));
}

BOOST_AUTO_TEST_CASE(test_v03_bundle_at_activation)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(IsBundleVersionAccepted(3, params.nDigiDollarPhase3Height, params));
}

BOOST_AUTO_TEST_CASE(test_v02_bundle_after_phase3)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(IsBundleVersionAccepted(2, params.nDigiDollarPhase3Height + 1, params));
}

BOOST_AUTO_TEST_CASE(test_v03_bundle_after_phase3)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    BOOST_CHECK(IsBundleVersionAccepted(3, params.nDigiDollarPhase3Height + 1, params));
}

BOOST_AUTO_TEST_CASE(test_off_by_one_activation)
{
    SelectParams(ChainType::REGTEST);
    const auto& params = Params().GetConsensus();
    int32_t N = params.nDigiDollarPhase3Height;
    BOOST_CHECK(!params.IsPhaseThreeActive(N - 1));
    BOOST_CHECK(params.IsPhaseThreeActive(N));
    BOOST_CHECK(!IsBundleVersionAccepted(3, N - 1, params));
    BOOST_CHECK(IsBundleVersionAccepted(3, N, params));
}

// ============================================================================
// PART 6: Cross-Network Consistency
// ============================================================================

BOOST_AUTO_TEST_CASE(test_regtest_earliest_activation)
{
    SelectParams(ChainType::REGTEST);
    int32_t regtest = Params().GetConsensus().nDigiDollarPhase3Height;
    SelectParams(ChainType::TESTNET);
    int32_t testnet = Params().GetConsensus().nDigiDollarPhase3Height;
    SelectParams(ChainType::MAIN);
    int32_t mainnet = Params().GetConsensus().nDigiDollarPhase3Height;
    BOOST_CHECK_LE(regtest, testnet);
    BOOST_CHECK_LE(testnet, mainnet);
}

BOOST_AUTO_TEST_SUITE_END()
