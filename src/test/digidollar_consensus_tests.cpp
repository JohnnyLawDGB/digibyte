// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/digidollar.h>
#include <consensus/params.h>
#include <kernel/chainparams.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(digidollar_consensus_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(collateral_ratio_lookup_test)
{
    DigiDollar::ConsensusParams params;

    // Test exact tier matches
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(30 * DigiDollar::BLOCKS_PER_DAY, params), 500);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(90 * DigiDollar::BLOCKS_PER_DAY, params), 400);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(180 * DigiDollar::BLOCKS_PER_DAY, params), 350);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(365 * DigiDollar::BLOCKS_PER_DAY, params), 300);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(2 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 275);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(3 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 250);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(5 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 225);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(7 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 212);
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(10 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 200);

    // Test between tiers (should use higher ratio for conservative approach)
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(15 * DigiDollar::BLOCKS_PER_DAY, params), 500); // Less than 30 days
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(60 * DigiDollar::BLOCKS_PER_DAY, params), 400); // Between 30 and 90 days
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(120 * DigiDollar::BLOCKS_PER_DAY, params), 350); // Between 90 and 180 days

    // Test beyond longest tier
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(15 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 200); // 15 years
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(100 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 200); // 100 years

    // Test very short periods (now map to 1-hour tier = 1000%)
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(1, params), 1000); // 1 block (< 240 blocks)
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(DigiDollar::BLOCKS_PER_DAY, params), 500); // 1 day (> 240 blocks, maps to 30-day tier)
}

BOOST_AUTO_TEST_CASE(dca_multiplier_test)
{
    DigiDollar::ConsensusParams params;

    // Test exact DCA levels
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(150, params), 1.0); // Normal level
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(120, params), 1.25); // 25% increase
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(110, params), 1.5); // 50% increase
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(100, params), 2.0); // 100% increase

    // Test above highest level
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(200, params), 1.0); // Above 150%
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(175, params), 1.0); // Above 150%

    // Test between levels
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(140, params), 1.25); // Between 150 and 120
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(115, params), 1.5); // Between 120 and 110
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(105, params), 2.0); // Between 110 and 100

    // Test below lowest level
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(90, params), 2.0); // Below 100%
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(50, params), 2.0); // Much below 100%
}

BOOST_AUTO_TEST_CASE(mint_amount_validation_test)
{
    DigiDollar::ConsensusParams params;

    // Test valid amounts (amounts are in CENTS, not satoshis)
    // Default params: minMintAmount = 10000 cents ($100), maxMintAmount = 10000000 cents ($100k)
    BOOST_CHECK(DigiDollar::IsValidMintAmount(10000, params)); // Minimum: $100.00
    BOOST_CHECK(DigiDollar::IsValidMintAmount(100000, params)); // Mid-range: $1,000.00
    BOOST_CHECK(DigiDollar::IsValidMintAmount(10000000, params)); // Maximum: $100,000.00

    // Test invalid amounts (too low)
    BOOST_CHECK(!DigiDollar::IsValidMintAmount(9999, params)); // Below minimum ($99.99)
    BOOST_CHECK(!DigiDollar::IsValidMintAmount(100, params)); // Way below ($1.00)
    BOOST_CHECK(!DigiDollar::IsValidMintAmount(1, params)); // Very low ($0.01)
    BOOST_CHECK(!DigiDollar::IsValidMintAmount(0, params)); // Zero

    // Test invalid amounts (too high)
    BOOST_CHECK(!DigiDollar::IsValidMintAmount(10000001, params)); // Above maximum ($100,000.01)
    BOOST_CHECK(!DigiDollar::IsValidMintAmount(100000000, params)); // Much above maximum ($1,000,000.00)
}

BOOST_AUTO_TEST_CASE(minimum_output_test)
{
    DigiDollar::ConsensusParams params;
    BOOST_CHECK_EQUAL(DigiDollar::GetMinimumDDOutput(params), 100); // $1 minimum
}

BOOST_AUTO_TEST_CASE(lock_time_conversion_test)
{
    // Test day to block conversion
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(1), DigiDollar::BLOCKS_PER_DAY); // 1 day = 5760 blocks
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(30), 30 * DigiDollar::BLOCKS_PER_DAY); // 30 days
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(365), 365 * DigiDollar::BLOCKS_PER_DAY); // 1 year

    // Test block to day conversion
    BOOST_CHECK_EQUAL(DigiDollar::BlocksToLockDays(DigiDollar::BLOCKS_PER_DAY), 1); // 1 day
    BOOST_CHECK_EQUAL(DigiDollar::BlocksToLockDays(30 * DigiDollar::BLOCKS_PER_DAY), 30); // 30 days
    BOOST_CHECK_EQUAL(DigiDollar::BlocksToLockDays(365 * DigiDollar::BLOCKS_PER_DAY), 365); // 1 year

    // Test roundtrip conversions
    BOOST_CHECK_EQUAL(DigiDollar::BlocksToLockDays(DigiDollar::LockDaysToBlocks(100)), 100);
    BOOST_CHECK_EQUAL(DigiDollar::LockDaysToBlocks(DigiDollar::BlocksToLockDays(576000)), 576000);
}

BOOST_AUTO_TEST_CASE(parameter_validation_test)
{
    DigiDollar::ConsensusParams validParams;
    std::string strError;

    // Test valid parameters
    BOOST_CHECK(DigiDollar::ValidateConsensusParams(validParams, strError));

    // Test invalid collateral ratios
    DigiDollar::ConsensusParams invalidParams1 = validParams;
    invalidParams1.collateralRatios.clear();
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams1, strError));
    BOOST_CHECK(strError.find("empty") != std::string::npos);

    DigiDollar::ConsensusParams invalidParams2 = validParams;
    invalidParams2.collateralRatios[100] = 50; // Less than 100%
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams2, strError));
    BOOST_CHECK(strError.find("100%") != std::string::npos);

    // Test invalid mint amounts
    DigiDollar::ConsensusParams invalidParams3 = validParams;
    invalidParams3.minMintAmount = 0;
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams3, strError));

    DigiDollar::ConsensusParams invalidParams4 = validParams;
    invalidParams4.maxMintAmount = invalidParams4.minMintAmount; // Equal
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams4, strError));

    // Test invalid oracle configuration
    DigiDollar::ConsensusParams invalidParams5 = validParams;
    invalidParams5.oracleCount = 0;
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams5, strError));

    DigiDollar::ConsensusParams invalidParams6 = validParams;
    invalidParams6.oracleThreshold = invalidParams6.activeOracles + 1; // Exceeds active oracles
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams6, strError));

    // Test invalid DCA levels
    DigiDollar::ConsensusParams invalidParams7 = validParams;
    invalidParams7.dcaLevels.clear();
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams7, strError));

    DigiDollar::ConsensusParams invalidParams8 = validParams;
    invalidParams8.dcaLevels[0].systemCollateral = 100; // Should be descending order
    invalidParams8.dcaLevels[1].systemCollateral = 150; // This breaks descending order
    BOOST_CHECK(!DigiDollar::ValidateConsensusParams(invalidParams8, strError));
}

BOOST_AUTO_TEST_CASE(digidollar_activation_test)
{
    // Test mainnet parameters
    auto mainParams = CChainParams::Main();
    BOOST_CHECK(!DigiDollar::IsDigiDollarActive(0, mainParams->GetConsensus())); // Genesis
    BOOST_CHECK(!DigiDollar::IsDigiDollarActive(21999999, mainParams->GetConsensus())); // Before activation
    BOOST_CHECK(DigiDollar::IsDigiDollarActive(22000000, mainParams->GetConsensus())); // At activation
    BOOST_CHECK(DigiDollar::IsDigiDollarActive(22000001, mainParams->GetConsensus())); // After activation

    // Test regtest parameters - DigiDollar activates at height 650 (after Odocrypt at 600)
    auto regTestParams = CChainParams::RegTest({});
    BOOST_CHECK(!DigiDollar::IsDigiDollarActive(0, regTestParams->GetConsensus())); // Not active at genesis
    BOOST_CHECK(!DigiDollar::IsDigiDollarActive(649, regTestParams->GetConsensus())); // Not active before height 650
    BOOST_CHECK(DigiDollar::IsDigiDollarActive(650, regTestParams->GetConsensus())); // Active at height 650
    BOOST_CHECK(DigiDollar::IsDigiDollarActive(1000, regTestParams->GetConsensus())); // Active after height 650
}

BOOST_AUTO_TEST_CASE(lock_tier_index_test)
{
    DigiDollar::ConsensusParams params;

    // Test tier index lookup (tier 0 is now 1-hour tier at 240 blocks)
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(100, params), 0); // 1-hour tier (< 240 blocks)
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(15 * DigiDollar::BLOCKS_PER_DAY, params), 1); // 30 day tier
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(60 * DigiDollar::BLOCKS_PER_DAY, params), 2); // 90 day tier
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(120 * DigiDollar::BLOCKS_PER_DAY, params), 3); // 180 day tier
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(200 * DigiDollar::BLOCKS_PER_DAY, params), 4); // 365 day tier

    // Test exact tier boundaries
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(240, params), 0); // Exactly 1 hour (240 blocks)
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(30 * DigiDollar::BLOCKS_PER_DAY, params), 1); // Exactly 30 days
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(90 * DigiDollar::BLOCKS_PER_DAY, params), 2); // Exactly 90 days
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(365 * DigiDollar::BLOCKS_PER_DAY, params), 4); // Exactly 365 days

    // Test 2-year tier
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(500 * DigiDollar::BLOCKS_PER_DAY, params), 5); // 2 year tier
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(2 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 5); // Exactly 2 years

    // Test beyond all tiers
    BOOST_CHECK_EQUAL(DigiDollar::GetLockTierIndex(15 * 365 * DigiDollar::BLOCKS_PER_DAY, params), 9); // Last tier index (10 tiers, 0-9)
}

BOOST_AUTO_TEST_CASE(format_lock_period_test)
{
    // Test day formatting
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(DigiDollar::BLOCKS_PER_DAY), "1 days");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(15 * DigiDollar::BLOCKS_PER_DAY), "15 days");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(29 * DigiDollar::BLOCKS_PER_DAY), "29 days");

    // Test month formatting
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(30 * DigiDollar::BLOCKS_PER_DAY), "1 month");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(60 * DigiDollar::BLOCKS_PER_DAY), "2 months");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(90 * DigiDollar::BLOCKS_PER_DAY), "3 months");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(330 * DigiDollar::BLOCKS_PER_DAY), "11 months");

    // Test year formatting
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(360 * DigiDollar::BLOCKS_PER_DAY), "1 year");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(365 * DigiDollar::BLOCKS_PER_DAY), "1 year");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(730 * DigiDollar::BLOCKS_PER_DAY), "2 years");
    BOOST_CHECK_EQUAL(DigiDollar::FormatLockPeriod(3650 * DigiDollar::BLOCKS_PER_DAY), "10 years");
}

BOOST_AUTO_TEST_CASE(chainparams_digidollar_integration_test)
{
    // Test that all chain types have DigiDollar parameters
    auto mainParams = CChainParams::Main();
    auto testParams = CChainParams::TestNet();
    auto regTestParams = CChainParams::RegTest({});

    // Verify parameters are accessible
    const auto& mainDD = mainParams->GetDigiDollarParams();
    const auto& testDD = testParams->GetDigiDollarParams();
    const auto& regTestDD = regTestParams->GetDigiDollarParams();

    // Test that parameters are valid
    std::string strError;
    BOOST_CHECK(DigiDollar::ValidateConsensusParams(mainDD, strError));
    BOOST_CHECK(DigiDollar::ValidateConsensusParams(testDD, strError));
    BOOST_CHECK(DigiDollar::ValidateConsensusParams(regTestDD, strError));

    // Test network-specific differences (amounts are in CENTS, not satoshis)
    BOOST_CHECK_EQUAL(mainDD.minMintAmount, 10000);   // Mainnet: 10000 cents = $100.00 min
    BOOST_CHECK_EQUAL(testDD.minMintAmount, 10000);   // Testnet: 10000 cents = $100.00 min
    BOOST_CHECK_EQUAL(regTestDD.minMintAmount, 1);    // Regtest: 1 cent = $0.01 min

    BOOST_CHECK_EQUAL(mainDD.oracleThreshold, 8);  // Mainnet: 8-of-15 (Phase Two)
    BOOST_CHECK_EQUAL(testDD.oracleThreshold, 4);  // Testnet: 4-of-7 (Phase Two)
    BOOST_CHECK_EQUAL(regTestDD.oracleThreshold, 1); // Regtest: 1-of-1 (Phase One)

    // Test activation heights
    BOOST_CHECK_EQUAL(mainParams->GetConsensus().nDDActivationHeight, 22000000); // Future block
    BOOST_CHECK_EQUAL(testParams->GetConsensus().nDDActivationHeight, 600);      // Testnet: active from block 600 (BIP9 DEFINED→STARTED→LOCKED_IN→ACTIVE)
    BOOST_CHECK_EQUAL(regTestParams->GetConsensus().nDDActivationHeight, 650);   // After Odocrypt at 600
}

BOOST_AUTO_TEST_CASE(edge_cases_test)
{
    DigiDollar::ConsensusParams params;

    // Test zero lock time (maps to 1-hour tier = 1000%)
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(0, params), 1000); // Maps to 1-hour tier

    // Test maximum possible values
    BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(std::numeric_limits<int64_t>::max(), params), 200);

    // Test DCA with extreme values
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(std::numeric_limits<int>::max(), params), 1.0);
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(0, params), 2.0);

    // Test empty DCA levels
    DigiDollar::ConsensusParams emptyDCAParams = params;
    emptyDCAParams.dcaLevels.clear();
    BOOST_CHECK_EQUAL(DigiDollar::GetDCAMultiplier(150, emptyDCAParams), 2.0); // Fallback
}

BOOST_AUTO_TEST_SUITE_END()