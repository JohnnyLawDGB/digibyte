// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/digidollar.h>
#include <consensus/volatility.h>
#include <digidollar/scripts.h>
#include <digidollar/validation.h>
#include <key.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

struct LockTierCase {
    int tier;
    int days;
    int ratio;
};

static const std::vector<LockTierCase>& CanonicalLockTiers()
{
    static const std::vector<LockTierCase> tiers{
        {0, 0, 1000},
        {1, 30, 500},
        {2, 90, 400},
        {3, 180, 350},
        {4, 365, 300},
        {5, 730, 275},
        {6, 1095, 250},
        {7, 1825, 225},
        {8, 2555, 212},
        {9, 3650, 200},
    };
    return tiers;
}

struct DigiDollarLockTierTestSetup : public TestingSetup {
    DigiDollarLockTierTestSetup()
        : TestingSetup(ChainType::REGTEST)
    {
        owner_key.MakeNewKey(true);
        owner_xonly = XOnlyPubKey(owner_key.GetPubKey());
        ResetVolatilityState();
    }

    static constexpr int CURRENT_HEIGHT = 1000;
    static constexpr CAmount ORACLE_PRICE_MICRO_USD = 1000000;
    static constexpr int SYSTEM_COLLATERAL = 150;
    static constexpr CAmount DD_AMOUNT = 10000;

    DigiDollar::ValidationContext MakeContext() const
    {
        return DigiDollar::ValidationContext(CURRENT_HEIGHT,
                                             ORACLE_PRICE_MICRO_USD,
                                             SYSTEM_COLLATERAL,
                                             Params());
    }

    void ResetVolatilityState() const
    {
        DigiDollar::Volatility::VolatilityMonitor::ReconstructFromBlockData({}, CURRENT_HEIGHT);
        DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();
    }

    CAmount RequiredCollateralAtRatio(int ratio) const
    {
        const __int128 numerator = static_cast<__int128>(DD_AMOUNT) *
                                   static_cast<__int128>(COIN) *
                                   static_cast<__int128>(ratio) *
                                   static_cast<__int128>(100);
        const __int128 denominator = ORACLE_PRICE_MICRO_USD;
        return static_cast<CAmount>((numerator + denominator - 1) / denominator);
    }

    CTransaction CreateMintTx(int64_t lock_blocks, int64_t lock_tier, CAmount collateral_amount) const
    {
        const int64_t lock_height = CURRENT_HEIGHT + lock_blocks;

        DigiDollar::MintParams params;
        params.ddAmount = DD_AMOUNT;
        params.lockHeight = lock_height;
        params.ownerKey = owner_xonly;
        params.internalKey = DigiDollar::GetCollateralNUMSKey();
        params.oracleKeys = DigiDollar::GetOracleKeys(15);

        const CScript collateral_script = DigiDollar::CreateCollateralP2TR(params);
        const CScript dd_script = DigiDollar::CreateDigiDollarP2TR(owner_xonly, DD_AMOUNT);
        const CScript op_return = CScript() << OP_RETURN
                                            << std::vector<unsigned char>{'D', 'D'}
                                            << CScriptNum(1)
                                            << CScriptNum(DD_AMOUNT)
                                            << CScriptNum(lock_height)
                                            << CScriptNum(lock_tier)
                                            << std::vector<unsigned char>(owner_xonly.begin(), owner_xonly.end());

        CMutableTransaction mtx;
        mtx.nVersion = 0x01000770;
        mtx.vin.push_back(CTxIn(COutPoint(uint256S("0202020202020202020202020202020202020202020202020202020202020202"), 0)));
        mtx.vout.push_back(CTxOut(collateral_amount, collateral_script));
        mtx.vout.push_back(CTxOut(0, dd_script));
        mtx.vout.push_back(CTxOut(0, op_return));
        return CTransaction(mtx);
    }

    CKey owner_key;
    XOnlyPubKey owner_xonly;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(digidollar_locktier_tests, DigiDollarLockTierTestSetup)

BOOST_AUTO_TEST_CASE(canonical_lock_tiers_accept_exact_collateral_ratios)
{
    const auto& params = Params().GetDigiDollarParams();

    for (const LockTierCase& tier : CanonicalLockTiers()) {
        const int64_t lock_blocks = DigiDollar::LockDaysToBlocks(tier.days);
        BOOST_CHECK_EQUAL(DigiDollar::GetCollateralRatioForLockTime(lock_blocks, params), tier.ratio);

        const CAmount collateral = RequiredCollateralAtRatio(tier.ratio);
        const CTransaction tx = CreateMintTx(lock_blocks, tier.tier, collateral);
        DigiDollar::ValidationContext ctx = MakeContext();
        TxValidationState state;

        ResetVolatilityState();
        BOOST_CHECK_MESSAGE(DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state),
            "Canonical tier " + std::to_string(tier.tier) +
            " should accept lock_blocks=" + std::to_string(lock_blocks) +
            " at ratio=" + std::to_string(tier.ratio) +
            ". Reject reason: " + state.GetRejectReason());
    }
}

BOOST_AUTO_TEST_CASE(tier_plus_one_and_minus_one_durations_reject)
{
    for (const LockTierCase& tier : CanonicalLockTiers()) {
        const int64_t canonical_blocks = DigiDollar::LockDaysToBlocks(tier.days);
        const CAmount collateral = RequiredCollateralAtRatio(tier.ratio);

        for (const int64_t lock_blocks : {canonical_blocks - 1, canonical_blocks + 1}) {
            BOOST_REQUIRE(lock_blocks > 0);
            const CTransaction tx = CreateMintTx(lock_blocks, tier.tier, collateral);
            DigiDollar::ValidationContext ctx = MakeContext();
            TxValidationState state;

            ResetVolatilityState();
            const bool valid = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
            BOOST_CHECK_MESSAGE(!valid,
                "Non-canonical tier " + std::to_string(tier.tier) +
                " duration lock_blocks=" + std::to_string(lock_blocks) +
                " should reject; accepted with reject reason: " + state.GetRejectReason());
        }
    }
}

BOOST_AUTO_TEST_CASE(arbitrary_custom_lock_duration_rejects)
{
    const int64_t forty_five_days = DigiDollar::LockDaysToBlocks(45);
    const int claimed_tier = 1;
    const CAmount tier_one_collateral = RequiredCollateralAtRatio(/*ratio=*/500);
    const CTransaction tx = CreateMintTx(forty_five_days, claimed_tier, tier_one_collateral);
    DigiDollar::ValidationContext ctx = MakeContext();
    TxValidationState state;

    ResetVolatilityState();
    const bool valid = DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state);
    BOOST_CHECK_MESSAGE(!valid,
        "Arbitrary custom lock duration should reject; accepted with reject reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_SUITE_END()
