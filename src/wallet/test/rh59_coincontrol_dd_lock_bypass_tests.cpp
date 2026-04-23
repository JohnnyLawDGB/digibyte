// Copyright (c) 2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// =============================================================================
// RH-59: Coin-control / fundrawtransaction bypasses IsLockedByDD + IsLockedCoin
// =============================================================================
//
// Red Hornet Wave-7 sub-agent 7B (angle C) --
// IsLockedByDD bypass via pre-selected coin-control inputs.
//
// Attack: `IsLockedByDD()` (src/wallet/digidollarwallet.cpp:1033) and
// `IsLockedCoin()` (src/wallet/wallet.cpp:2787) are the two wallet-level
// gates that keep DD collateral / DD-token UTXOs out of ordinary DGB
// transactions. Prior tests (rh08-ISMINE in
// digidollar_wallet_security_tests.cpp:669 and rh28_11 in
// digidollar_rh28_wallet_chains_tests.cpp:754) verify the helpers return
// `true` for locked outpoints but never verify that coin selection
// actually respects it.
//
// Finding: `FetchSelectedInputs` at src/wallet/spend.cpp:258-303 accepts
// any outpoint the caller passes via `CCoinControl::Select()` /
// `SelectExternal()`, without consulting *either* `CWallet::IsLockedCoin()`
// *or* `DigiDollarWallet::IsLockedByDD()`. Compare that with
// `AvailableCoins` at spend.cpp:394, which does honour `IsLockedCoin` when
// `params.skip_locked=true` (default), and has the DD-MINT vout-0/1/2
// heuristic at spend.cpp:407-416.
//
// Any RPC that exposes user-controlled prevouts --
// `fundrawtransaction` (src/wallet/rpc/spend.cpp:743),
// `walletcreatefundedpsbt`, `send`, `sendall` --
// threads the outpoint through `FetchSelectedInputs` and then into
// `CreateTransaction`, silently pulling a DD-locked UTXO into a regular
// transaction.
//
// Concrete harm (HIGH, wallet-state integrity + consensus-reject stepping
// stone -- not direct peg break, since consensus still enforces CLTV on the
// collateral and OP_DIGIDOLLAR on the token):
//
//   * The wallet constructs and optionally signs a tx spending DD
//     collateral pre-timelock. The RPC returns success and, if
//     `lockUnspents=true`, the preset prevout is added to
//     `setLockedCoins` again (src/wallet/spend.cpp:1414). DD bookkeeping
//     is never touched.
//   * Broadcast-time consensus rejection bounces back, but the user has
//     already lost: the wallet believes the coin is spent-pending until
//     `abandontransaction`. In the DD-token case (0-value output, simple
//     key-path P2TR), a malicious script can spend the token without the
//     DD validator noticing because `DigiDollarWallet::RemoveDDUTXO` is
//     only called by the DD-aware transfer/redeem builders, not by
//     `CreateTransaction`. Result: `getdigidollarbalance` over-reports the
//     user's DD holdings.
//   * Combined with prior finding C4 (unhandled `CScriptNum` exception in
//     DD transfer validator, src/digidollar/validation.cpp:1199,1206) the
//     attacker can craft an input whose scriptSig throws during ATMP DD
//     validation, turning the wallet-level bypass into a block-validation
//     abort once a miner includes the tx.

#include <boost/test/unit_test.hpp>

#include <consensus/amount.h>
#include <script/solver.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/digidollarwallet.h>
#include <wallet/spend.h>
#include <wallet/test/util.h>
#include <wallet/test/wallet_test_fixture.h>
#include <random.h>
#include <script/script.h>
#include <script/standard.h>

namespace wallet {

BOOST_FIXTURE_TEST_SUITE(rh59_coincontrol_dd_lock_bypass_tests, WalletTestingSetup)

// =============================================================================
// RH-59-01: FetchSelectedInputs ignores regular `setLockedCoins`
// =============================================================================
//
// Primary exploit. A UTXO placed in `setLockedCoins` via LockCoin() (which
// is what the DD wallet does at init for every collateral / DD-token output
// it knows about -- see src/wallet/digidollarwallet.cpp:135,142 and
// :2136,2141) is still accepted by `FetchSelectedInputs`. Any RPC that
// exposes preset inputs therefore bypasses the lock entirely.
//
// The test drives `FetchSelectedInputs` directly with a locked external
// outpoint and asserts it is returned in the result set.
// =============================================================================
BOOST_AUTO_TEST_CASE(rh59_01_fetch_selected_inputs_ignores_locked_coin)
{
    // Fabricate an outpoint and mark it locked, then hand it to the preset
    // input path via SelectExternal. SelectExternal is exactly what
    // fundrawtransaction calls at src/wallet/spend.cpp:1384 when the input
    // isn't in the wallet's mapWallet.
    uint256 fake_txid;
    GetRandBytes(fake_txid);
    COutPoint locked_outpoint(fake_txid, 0);

    CKey sigkey;
    sigkey.MakeNewKey(true);
    const CPubKey pub = sigkey.GetPubKey();
    CScript script_pubkey = GetScriptForDestination(WitnessV0KeyHash(pub.GetID()));

    CTxOut txout;
    txout.nValue = 500 * COIN;
    txout.scriptPubKey = script_pubkey;

    {
        LOCK(m_wallet.cs_wallet);
        BOOST_REQUIRE(m_wallet.LockCoin(locked_outpoint));
        BOOST_REQUIRE(m_wallet.IsLockedCoin(locked_outpoint));
    }

    CCoinControl coin_control;
    coin_control.SelectExternal(locked_outpoint, txout);
    // Attacker-supplied `input_weights` field of fundrawtransaction maps
    // straight to SetInputWeight -- spend.cpp:698. This is how a real
    // fundrawtransaction call skips the "Not solvable" rejection at
    // spend.cpp:293 when the wallet doesn't know the input's descriptor.
    // We use the same trick so the test isolates the LOCK check, not the
    // solvability check.
    coin_control.SetInputWeight(locked_outpoint, 272); // ~P2WPKH input weight

    FastRandomContext rng_fast;
    CoinSelectionParams csp{rng_fast};
    csp.m_effective_feerate = CFeeRate(1000);
    csp.m_long_term_feerate = CFeeRate(1000);
    csp.m_discard_feerate   = CFeeRate(1000);

    util::Result<PreSelectedInputs> res = [&]() EXCLUSIVE_LOCKS_REQUIRED(m_wallet.cs_wallet) {
        LOCK(m_wallet.cs_wallet);
        return FetchSelectedInputs(m_wallet, coin_control, csp);
    }();

    BOOST_REQUIRE_MESSAGE(res.has_value(),
        "Expected FetchSelectedInputs to accept the outpoint. Error: "
        << (res.has_value() ? std::string{} : util::ErrorString(res).original));

    const PreSelectedInputs& preset = *res;
    std::set<COutPoint> accepted;
    for (const std::shared_ptr<COutput>& out : preset.coins) {
        accepted.insert(out->outpoint);
    }

    BOOST_CHECK_MESSAGE(accepted.count(locked_outpoint) == 1,
        "VULN CONFIRMED (RH-59-01): FetchSelectedInputs accepted a UTXO "
        "that is in setLockedCoins. No IsLockedCoin() / IsLockedByDD() "
        "check exists in src/wallet/spend.cpp:258-303. Any RPC exposing "
        "preset prevouts (fundrawtransaction, walletcreatefundedpsbt, "
        "send, sendall) bypasses the DD lock.");

    BOOST_TEST_MESSAGE("RH-59-01: preset-input path accepted locked "
                       "outpoint. Fix direction: reject outpoints for "
                       "which IsLockedCoin(outpoint)==true AND "
                       "(!IsDDInternalFlow) at spend.cpp:264.");
}

// =============================================================================
// RH-59-02: DD-specific bypass via IsLockedByDD
// =============================================================================
//
// Repeats the preset-input probe but with a DD-aware wallet underneath so
// `IsLockedByDD()` also reports the outpoint as locked. The gate is still
// not consulted.
//
// NOTE: WalletTestingSetup does not wire a DigiDollarWallet onto m_wallet
// (CWallet::Create at src/wallet/wallet.cpp:3043 does, but the fixture
// skips Create). The existing digidollar_wallet_security_tests construct
// a stack DigiDollarWallet tied to &m_wallet; we do the same to populate
// dd_utxos and verify IsLockedByDD, then point out that even if the
// wallet WERE wired, FetchSelectedInputs never asks.
// =============================================================================
BOOST_AUTO_TEST_CASE(rh59_02_dd_wallet_locked_outpoint_also_bypassable)
{
    // Populate a DigiDollarWallet with a collateral position.
    DigiDollarWallet dd_wallet(&m_wallet);

    uint256 mint_txid;
    GetRandBytes(mint_txid);

    WalletCollateralPosition pos;
    pos.dd_timelock_id = mint_txid;
    pos.dd_minted = 50000;           // $500 in cents
    pos.dgb_collateral = 500 * COIN;
    pos.lock_tier = 4;
    pos.unlock_height = 1000000;     // still locked on any reasonable regtest tip
    pos.is_active = true;
    dd_wallet.AddCollateralPosition(pos);

    COutPoint collateral(mint_txid, 0);
    COutPoint dd_token(mint_txid, 1);

    BOOST_REQUIRE(dd_wallet.IsLockedByDD(collateral));
    BOOST_REQUIRE(dd_wallet.IsLockedByDD(dd_token));

    // Also install the regular setLockedCoins entry that init-time DD
    // scanning (src/wallet/digidollarwallet.cpp:135) would create.
    {
        LOCK(m_wallet.cs_wallet);
        BOOST_REQUIRE(m_wallet.LockCoin(collateral));
        BOOST_REQUIRE(m_wallet.LockCoin(dd_token));
    }

    // Build fake P2WPKH txouts so CalculateMaximumSignedInputSize succeeds.
    CKey sigkey;
    sigkey.MakeNewKey(true);
    const CPubKey pub = sigkey.GetPubKey();
    CScript script_pubkey = GetScriptForDestination(WitnessV0KeyHash(pub.GetID()));

    CTxOut coll_txout;
    coll_txout.nValue = 500 * COIN;
    coll_txout.scriptPubKey = script_pubkey;

    CTxOut token_txout;
    token_txout.nValue = 0;          // DD-token outputs are 0 DGB
    token_txout.scriptPubKey = script_pubkey;

    CCoinControl coin_control;
    coin_control.SelectExternal(collateral, coll_txout);
    coin_control.SelectExternal(dd_token, token_txout);
    // Supply input weights to skip the solvability gate -- mirrors the
    // fundrawtransaction `input_weights` argument at
    // src/wallet/rpc/spend.cpp:698. Without this the test would hit the
    // "Not solvable" error at spend.cpp:293 before the lock-check gap has
    // a chance to manifest. Real attackers control this parameter.
    coin_control.SetInputWeight(collateral, 272);
    coin_control.SetInputWeight(dd_token, 272);

    FastRandomContext rng_fast;
    CoinSelectionParams csp{rng_fast};
    csp.m_effective_feerate = CFeeRate(1000);
    csp.m_long_term_feerate = CFeeRate(1000);
    csp.m_discard_feerate   = CFeeRate(1000);

    util::Result<PreSelectedInputs> res = [&]() EXCLUSIVE_LOCKS_REQUIRED(m_wallet.cs_wallet) {
        LOCK(m_wallet.cs_wallet);
        return FetchSelectedInputs(m_wallet, coin_control, csp);
    }();

    BOOST_REQUIRE_MESSAGE(res.has_value(),
        "Preset input path produced an error; exploit assumes it succeeds. Error: "
        << (res.has_value() ? std::string{} : util::ErrorString(res).original));

    std::set<COutPoint> accepted;
    for (const std::shared_ptr<COutput>& out : res->coins) {
        accepted.insert(out->outpoint);
    }
    BOOST_CHECK_MESSAGE(accepted.count(collateral) == 1,
        "VULN CONFIRMED (RH-59-02a): DD-collateral outpoint accepted "
        "despite IsLockedByDD()==true AND IsLockedCoin()==true.");
    BOOST_CHECK_MESSAGE(accepted.count(dd_token) == 1,
        "VULN CONFIRMED (RH-59-02b): DD-token outpoint accepted despite "
        "IsLockedByDD()==true AND IsLockedCoin()==true. The DD-token "
        "case is the dangerous one: the owner's key-path P2TR sig makes "
        "the tx consensus-valid even though DigiDollarWallet::dd_utxos "
        "still lists the token -- this is the primary path to an "
        "out-of-sync DD balance.");

    // Tightens the finding: even after we forcibly deactivate the
    // position (so IsLockedByDD now returns false for collateral), the
    // token stays in dd_utxos and the regular lock remains. The bypass
    // is stable.
    dd_wallet.UpdatePositionStatus(mint_txid, false);
    BOOST_CHECK(!dd_wallet.IsLockedByDD(collateral));
    BOOST_CHECK(dd_wallet.IsLockedByDD(dd_token)); // token still in dd_utxos

    BOOST_TEST_MESSAGE("RH-59-02: preset-input path drives both DD "
                       "collateral and DD token through unchecked.");
}

// =============================================================================
// RH-59-03: Regression anchor for the AvailableCoins defence
// =============================================================================
//
// The non-preset ("auto-select") path does consult IsLockedCoin when
// `skip_locked` is true. Pin the default so a future refactor that weakens
// this gate is caught. Does not hit the exploit; purely defensive.
// =============================================================================
BOOST_AUTO_TEST_CASE(rh59_03_auto_select_skip_locked_default)
{
    CoinFilterParams params;
    BOOST_CHECK_MESSAGE(params.skip_locked == true,
        "CoinFilterParams::skip_locked default must remain true -- it is "
        "the only thing keeping DD-locked UTXOs out of auto coin "
        "selection. If this flips to false, the preset-input bypass "
        "described in RH-59-01/02 becomes the default behaviour for "
        "ordinary `sendtoaddress` calls too.");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace wallet
