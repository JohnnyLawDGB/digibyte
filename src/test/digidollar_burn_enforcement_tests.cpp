// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
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

static CScript MakeP2TR(const XOnlyPubKey& key)
{
    return CScript() << OP_1 << std::vector<unsigned char>(key.begin(), key.end());
}

static CScript MakeMintOpReturn(CAmount dd_amount, int64_t lock_height, int64_t lock_tier, const XOnlyPubKey& owner)
{
    return CScript() << OP_RETURN
                     << std::vector<unsigned char>{'D', 'D'}
                     << CScriptNum(1)
                     << CScriptNum(dd_amount)
                     << CScriptNum(lock_height)
                     << CScriptNum(lock_tier)
                     << std::vector<unsigned char>(owner.begin(), owner.end());
}

static CScript MakeRedeemOpReturn(CAmount dd_amount)
{
    return CScript() << OP_RETURN
                     << std::vector<unsigned char>{'D', 'D'}
                     << CScriptNum(3)
                     << CScriptNum(dd_amount);
}

struct MintData {
    XOnlyPubKey owner;
    CTransactionRef tx;
    uint32_t collateral_index;
    uint32_t dd_index;
};

enum class MintOutputOrder {
    CollateralThenDDThenOpReturn,
    DDThenOpReturnThenCollateral,
};

struct DigiDollarBurnEnforcementTestSetup : public TestingSetup {
    DigiDollarBurnEnforcementTestSetup()
        : TestingSetup(ChainType::REGTEST)
    {
    }

    static constexpr int MINT_HEIGHT = 1000;
    static constexpr int REDEEM_HEIGHT = 2000;
    static constexpr CAmount ORACLE_PRICE_MICRO_USD = 1000000;
    static constexpr int SYSTEM_COLLATERAL = 150;
    static constexpr CAmount DD_AMOUNT = 10000;
    static constexpr CAmount LOCKED_COLLATERAL = 500 * COIN;
    static constexpr int64_t LOCK_HEIGHT = 1240;

    MintData CreateMint(MintOutputOrder order = MintOutputOrder::CollateralThenDDThenOpReturn) const
    {
        CKey owner_key;
        owner_key.MakeNewKey(true);
        const XOnlyPubKey owner(owner_key.GetPubKey());

        DigiDollar::MintParams params;
        params.ddAmount = DD_AMOUNT;
        params.lockHeight = LOCK_HEIGHT;
        params.ownerKey = owner;
        params.internalKey = DigiDollar::GetCollateralNUMSKey();
        params.oracleKeys = DigiDollar::GetOracleKeys(15);

        const CScript collateral_script = DigiDollar::CreateCollateralP2TR(params);
        const CScript dd_script = DigiDollar::CreateDigiDollarP2TR(owner, DD_AMOUNT);
        const CScript op_return = MakeMintOpReturn(DD_AMOUNT, LOCK_HEIGHT, /*lock_tier=*/0, owner);

        CMutableTransaction mtx;
        mtx.nVersion = 0x01000770;
        mtx.vin.push_back(CTxIn(COutPoint(uint256S("0101010101010101010101010101010101010101010101010101010101010101"), 0)));

        MintData mint;
        mint.owner = owner;

        if (order == MintOutputOrder::CollateralThenDDThenOpReturn) {
            mint.collateral_index = 0;
            mint.dd_index = 1;
            mtx.vout.push_back(CTxOut(LOCKED_COLLATERAL, collateral_script));
            mtx.vout.push_back(CTxOut(0, dd_script));
            mtx.vout.push_back(CTxOut(0, op_return));
        } else {
            mint.dd_index = 0;
            mint.collateral_index = 2;
            mtx.vout.push_back(CTxOut(0, dd_script));
            mtx.vout.push_back(CTxOut(0, op_return));
            mtx.vout.push_back(CTxOut(LOCKED_COLLATERAL, collateral_script));
        }

        mint.tx = MakeTransactionRef(mtx);
        return mint;
    }

    COutPoint CollateralOutpoint(const MintData& mint) const
    {
        return COutPoint(mint.tx->GetHash(), mint.collateral_index);
    }

    COutPoint DDOutpoint(const MintData& mint) const
    {
        return COutPoint(mint.tx->GetHash(), mint.dd_index);
    }

    void AddMintCoins(CCoinsViewCache& coins, const MintData& mint, bool include_dd = true) const
    {
        coins.AddCoin(CollateralOutpoint(mint), Coin(mint.tx->vout[mint.collateral_index], MINT_HEIGHT, false), false);
        if (include_dd) {
            coins.AddCoin(DDOutpoint(mint), Coin(mint.tx->vout[mint.dd_index], MINT_HEIGHT, false), false);
        }
    }

    DigiDollar::TxLookupFn TxLookupFor(const MintData& mint) const
    {
        return [mint_ref = mint.tx](const uint256& txid, uint32_t, CTransactionRef& out) -> bool {
            if (txid == mint_ref->GetHash()) {
                out = mint_ref;
                return true;
            }
            return false;
        };
    }

    DigiDollar::ValidationContext MakeContext(const CCoinsViewCache& coins, DigiDollar::TxLookupFn tx_lookup) const
    {
        return DigiDollar::ValidationContext(REDEEM_HEIGHT,
                                             ORACLE_PRICE_MICRO_USD,
                                             SYSTEM_COLLATERAL,
                                             Params(),
                                             &coins,
                                             /*skip_oracle=*/false,
                                             tx_lookup);
    }

    CTransaction MakeRedeemTx(const MintData& mint, bool include_dd_input, CAmount collateral_release, CAmount dd_change = 0) const
    {
        CMutableTransaction mtx;
        mtx.nVersion = 0x03000770;
        mtx.nLockTime = LOCK_HEIGHT;
        mtx.vin.push_back(CTxIn(CollateralOutpoint(mint), CScript(), 0xfffffffe));
        if (include_dd_input) {
            mtx.vin.push_back(CTxIn(DDOutpoint(mint), CScript(), 0xfffffffe));
        }

        mtx.vout.push_back(CTxOut(collateral_release, MakeP2TR(mint.owner)));

        if (dd_change > 0) {
            mtx.vout.push_back(CTxOut(0, DigiDollar::CreateDigiDollarP2TR(mint.owner, dd_change)));
            mtx.vout.push_back(CTxOut(0, MakeRedeemOpReturn(dd_change)));
        }

        return CTransaction(mtx);
    }

    CTransaction MakeNonDDCollateralSpend(const MintData& mint) const
    {
        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.nLockTime = LOCK_HEIGHT;
        mtx.vin.push_back(CTxIn(CollateralOutpoint(mint), CScript(), 0xfffffffe));
        mtx.vout.push_back(CTxOut(LOCKED_COLLATERAL, MakeP2TR(mint.owner)));
        return CTransaction(mtx);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(digidollar_burn_enforcement_tests, DigiDollarBurnEnforcementTestSetup)

BOOST_AUTO_TEST_CASE(post_timelock_non_dd_collateral_spend_is_rejected)
{
    const MintData mint = CreateMint();
    CCoinsView base_view;
    CCoinsViewCache coins(&base_view);
    AddMintCoins(coins, mint);

    TxValidationState state;
    const DigiDollar::ValidationContext ctx = MakeContext(coins, TxLookupFor(mint));
    const CTransaction spend = MakeNonDDCollateralSpend(mint);

    const bool valid = DigiDollar::ValidateDigiDollarTransaction(spend, ctx, state);
    BOOST_CHECK_MESSAGE(!valid,
        "Post-timelock collateral spends must remain DD redemptions; non-DD spend was accepted. "
        "Reject reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(normal_redemption_succeeds_when_full_original_dd_is_burned)
{
    const MintData mint = CreateMint();
    CCoinsView base_view;
    CCoinsViewCache coins(&base_view);
    AddMintCoins(coins, mint);

    TxValidationState state;
    const DigiDollar::ValidationContext ctx = MakeContext(coins, TxLookupFor(mint));
    const CTransaction redeem = MakeRedeemTx(mint, /*include_dd_input=*/true, LOCKED_COLLATERAL);

    BOOST_CHECK_MESSAGE(DigiDollar::ValidateRedemptionTransaction(redeem, ctx, state),
        "Full-burn redemption should release the full collateral. Reject reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(partial_burn_cannot_release_collateral)
{
    const MintData mint = CreateMint();
    CCoinsView base_view;
    CCoinsViewCache coins(&base_view);
    AddMintCoins(coins, mint);

    TxValidationState state;
    const DigiDollar::ValidationContext ctx = MakeContext(coins, TxLookupFor(mint));
    const CAmount dd_change = DD_AMOUNT / 2;
    const CTransaction redeem = MakeRedeemTx(mint, /*include_dd_input=*/true, LOCKED_COLLATERAL, dd_change);

    BOOST_CHECK_MESSAGE(!DigiDollar::ValidateRedemptionTransaction(redeem, ctx, state),
        "Partial DD burn must not release collateral");
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-collateral-release-partial-burn");
}

BOOST_AUTO_TEST_CASE(collateral_spend_with_no_dd_inputs_cannot_release_collateral)
{
    const MintData mint = CreateMint();
    CCoinsView base_view;
    CCoinsViewCache coins(&base_view);
    AddMintCoins(coins, mint, /*include_dd=*/false);

    TxValidationState state;
    const DigiDollar::ValidationContext ctx = MakeContext(coins, TxLookupFor(mint));
    const CTransaction redeem = MakeRedeemTx(mint, /*include_dd_input=*/false, LOCKED_COLLATERAL);

    BOOST_CHECK_MESSAGE(!DigiDollar::ValidateCollateralReleaseAmount(redeem, ctx, /*ddBurned=*/0, state),
        "Collateral spend with no DD burn must not release locked DGB");
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-collateral-release-partial-burn");
}

BOOST_AUTO_TEST_CASE(reordered_mint_outputs_identify_correct_collateral_output)
{
    const MintData mint = CreateMint(MintOutputOrder::DDThenOpReturnThenCollateral);
    CAmount dd_amount = 0;
    CAmount collateral_amount = 0;

    BOOST_REQUIRE(DigiDollar::ExtractMintAccountingAmounts(*mint.tx, dd_amount, collateral_amount));
    BOOST_CHECK_EQUAL(dd_amount, DD_AMOUNT);
    BOOST_CHECK_EQUAL(collateral_amount, LOCKED_COLLATERAL);

    CCoinsView base_view;
    CCoinsViewCache coins(&base_view);
    AddMintCoins(coins, mint);

    TxValidationState state;
    const DigiDollar::ValidationContext ctx = MakeContext(coins, TxLookupFor(mint));
    const CTransaction redeem = MakeRedeemTx(mint, /*include_dd_input=*/true, LOCKED_COLLATERAL);

    BOOST_CHECK_MESSAGE(DigiDollar::ValidateCollateralReleaseAmount(redeem, ctx, DD_AMOUNT, state),
        "Collateral vout index should come from the spent outpoint, not fixed output order. "
        "Reject reason: " + state.GetRejectReason());
}

BOOST_AUTO_TEST_SUITE_END()
