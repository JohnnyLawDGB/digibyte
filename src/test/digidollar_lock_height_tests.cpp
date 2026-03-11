// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/digidollar.h>
#include <consensus/volatility.h>
#include <digidollar/digidollar.h>
#include <digidollar/scripts.h>
#include <digidollar/validation.h>
#include <key.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(digidollar_lock_height_tests)

struct DigiDollarLockHeightTestSetup : public TestingSetup {
    DigiDollarLockHeightTestSetup() : TestingSetup(ChainType::REGTEST)
    {
        testKey.MakeNewKey(true);
        testPubKey = testKey.GetPubKey();
        testXOnlyKey = XOnlyPubKey(testPubKey);

        mockOraclePrice = 500000;       // $0.50 per DGB in micro-USD
        mockSystemCollateral = 150;     // 150% system collateralization

        DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();
    }

    DigiDollar::ValidationContext MakeContext(int height, bool skip_oracle_validation) const
    {
        return DigiDollar::ValidationContext(
            height,
            mockOraclePrice,
            mockSystemCollateral,
            Params(),
            nullptr,
            skip_oracle_validation);
    }

    void ResetVolatilityState(int height) const
    {
        DigiDollar::Volatility::VolatilityMonitor::ReconstructFromBlockData({}, height);
        DigiDollar::Volatility::VolatilityMonitor::ClearFreeze();
    }

    CTransaction CreateMintTx(int64_t lock_height, int64_t lock_tier, CAmount dd_amount, CAmount collateral_amount) const
    {
        CMutableTransaction mtx;
        mtx.nVersion = 0x01000770; // DD_TX_MINT

        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(uint256S("0x1234"), 0);

        DigiDollar::MintParams params;
        params.ddAmount = dd_amount;
        params.lockHeight = lock_height;
        params.ownerKey = testXOnlyKey;
        params.internalKey = DigiDollar::GetCollateralNUMSKey();
        params.oracleKeys = DigiDollar::GetOracleKeys(15);

        const CScript collateralScript = DigiDollar::CreateCollateralP2TR(params);
        const CScript opReturn = CScript() << OP_RETURN
                                           << std::vector<unsigned char>{'D', 'D'}
                                           << CScriptNum(1)
                                           << CScriptNum(dd_amount)
                                           << CScriptNum(lock_height)
                                           << CScriptNum(lock_tier)
                                           << std::vector<unsigned char>(testXOnlyKey.begin(), testXOnlyKey.end());
        const CScript ddScript = DigiDollar::CreateDigiDollarP2TR(testXOnlyKey, dd_amount);

        mtx.vout.resize(3);
        mtx.vout[0] = CTxOut(0, opReturn);
        mtx.vout[1] = CTxOut(collateral_amount, collateralScript);
        mtx.vout[2] = CTxOut(0, ddScript);

        return CTransaction(mtx);
    }

    CKey testKey;
    CPubKey testPubKey;
    XOnlyPubKey testXOnlyKey;
    CAmount mockOraclePrice;
    int mockSystemCollateral;
};

BOOST_FIXTURE_TEST_CASE(rescan_mature_mint_passes, DigiDollarLockHeightTestSetup)
{
    // Mint created at height 100 with lockHeight 340 (1-hour tier / 240 blocks).
    // During rescan/revalidation at height 500, it must still validate.
    const int64_t lockHeight = 340;
    const CTransaction tx = CreateMintTx(lockHeight, /*lock_tier=*/0, /*dd_amount=*/10000, /*collateral=*/1000 * COIN);
    DigiDollar::ValidationContext ctx = MakeContext(/*height=*/500, /*skip_oracle_validation=*/true);
    TxValidationState state;

    ResetVolatilityState(ctx.nHeight);
    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(rescan_immature_mint_still_valid, DigiDollarLockHeightTestSetup)
{
    // Same historical mint, but validation context is height 200 (lock not yet matured).
    // Rescan/revalidation must not reject it for lock tier mismatch.
    const int64_t lockHeight = 340;
    const CTransaction tx = CreateMintTx(lockHeight, /*lock_tier=*/0, /*dd_amount=*/10000, /*collateral=*/1000 * COIN);
    DigiDollar::ValidationContext ctx = MakeContext(/*height=*/200, /*skip_oracle_validation=*/true);
    TxValidationState state;

    ResetVolatilityState(ctx.nHeight);
    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_FIXTURE_TEST_CASE(fresh_mint_tier_mismatch_fails, DigiDollarLockHeightTestSetup)
{
    // Fresh mint claims tier 1 (30-day lock) but lockHeight is only 10 blocks ahead.
    // This must be rejected.
    const int freshHeight = 100;
    const int64_t badLockHeight = freshHeight + 10;
    const CTransaction tx = CreateMintTx(badLockHeight, /*lock_tier=*/1, /*dd_amount=*/10000, /*collateral=*/1000 * COIN);
    DigiDollar::ValidationContext ctx = MakeContext(freshHeight, /*skip_oracle_validation=*/false);
    TxValidationState state;

    ResetVolatilityState(ctx.nHeight);
    BOOST_CHECK(!DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-mint-lock-height-mismatch");
}

BOOST_FIXTURE_TEST_CASE(fresh_mint_tier_correct_passes, DigiDollarLockHeightTestSetup)
{
    // Fresh mint with tier 1 and the expected ~172,800 block lock period must pass.
    const int freshHeight = 100;
    const int64_t tier1LockBlocks = DigiDollar::LockDaysToBlocks(30);
    const int64_t lockHeight = freshHeight + tier1LockBlocks;
    DigiDollar::ValidationContext ctx = MakeContext(freshHeight, /*skip_oracle_validation=*/false);
    const CAmount ddAmount = 10000;
    const CAmount requiredCollateral = DigiDollar::CalculateRequiredCollateral(ddAmount, tier1LockBlocks, ctx);
    BOOST_REQUIRE(requiredCollateral > 0);

    const CTransaction tx = CreateMintTx(lockHeight, /*lock_tier=*/1, ddAmount, requiredCollateral);
    TxValidationState state;

    ResetVolatilityState(ctx.nHeight);
    BOOST_CHECK(DigiDollar::ValidateDigiDollarTransaction(tx, ctx, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_AUTO_TEST_SUITE_END()
