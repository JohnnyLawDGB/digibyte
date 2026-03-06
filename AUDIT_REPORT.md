# Audit Report: Bug #11 and Bug #13 Fixes

**Date:** 2026-03-06
**Auditor:** Irene (DigiSwarm)
**Commits:** `d5b0427115` (Bug #11), `6faee8374c` (Bug #13)

---

## Bug #11: Mint Amount Validation (`d5b0427115`)

### Summary
**Verdict: ✅ Fix is correct and solves the bug.**

The fix adds RPC-level validation of `ddAmount` against `ConsensusParams::minMintAmount` and `maxMintAmount` before the transaction is built. Previously, invalid amounts passed RPC parsing and failed at mempool broadcast with confusing errors.

### What the Fix Does
- Calls `DigiDollar::IsValidMintAmount(ddAmount, ddParams)` after the existing `ddAmount > 0` check
- On failure, throws `RPC_INVALID_PARAMETER` with a human-readable message: `"Minimum mint amount is $X (Y cents)"` or `"Maximum mint amount is $X (Y cents)"`
- Uses `Params().GetDigiDollarParams()` (chain-specific params), not hardcoded values

### Consensus Limits (chain-specific)
| Chain    | Min (cents) | Max (cents) | Min ($) | Max ($)   |
|----------|-------------|-------------|---------|-----------|
| Mainnet  | 10,000      | 1,000,000   | $100    | $10,000   |
| Regtest  | 1           | 100,000     | $0.01   | $1,000    |

### Integer Math Assessment
- ✅ DD amounts are in **cents** (CAmount = int64_t), not float/double
- ✅ `IsValidMintAmount()` uses simple integer comparison: `amount >= min && amount <= max`
- ✅ Error message formatting uses integer division (`/ 100`) — correct for cents→dollars
- ✅ No floating point anywhere in the validation path

### Edge Cases
- ✅ Boundary values (exact min, exact max) are correctly handled by `>=` and `<=`
- ✅ Zero caught by pre-existing `ddAmount <= 0` check before the new validation
- ✅ Negative values caught by `ddAmount <= 0` check
- ⚠️ **Minor note:** On regtest (min=1 cent), a user can never trigger the "Minimum mint amount" error message because the `ddAmount <= 0` check catches everything ≤0 first, and 1 passes. This is fine — not a bug, just means the min message path is only exercisable on mainnet/testnet.

### Could It Break Existing Functionality?
**No.** The fix is purely additive — it adds an early-exit error before the existing flow. Any amount that previously succeeded will still succeed. Only previously-confusing-failures now get clear error messages.

---

## Bug #13: Blockheight in DDTransaction (`6faee8374c`)

### Summary
**Verdict: ✅ Fix is correct and solves the bug.**

The fix populates `DDTransaction::blockheight` and `DDTransaction::blockhash` from `TxStateConfirmed` state when iterating wallet transactions in `GetDDTransactionHistory()`.

### What the Fix Does
- After looking up `CWalletTx* wtx`, checks `wtx->state<wallet::TxStateConfirmed>()`
- If confirmed: sets `ddtx.blockheight = conf->confirmed_block_height` and `ddtx.blockhash = conf->confirmed_block_hash.GetHex()`
- If not confirmed: explicitly sets `blockheight = -1` and `blockhash = ""`
- Follows the same `std::variant<TxState>` pattern used throughout Bitcoin Core wallet code

### Code Quality
- ✅ Clean 10-line addition, well-placed before the `isAbandoned()` check
- ✅ Uses the canonical `state<TxStateConfirmed>()` accessor (not manual variant inspection)
- ✅ Handles both confirmed and unconfirmed branches explicitly
- ✅ No performance impact — `state<>()` is a simple variant check

### Edge Cases
- ✅ Unconfirmed transactions correctly retain `blockheight = -1`
- ✅ Reorged transactions that become unconfirmed will correctly show `-1` (TxStateConfirmed won't match)
- ✅ Blockheight is stable after additional confirmations (it records the block height, not tip height)
- ⚠️ **Abandoned transactions:** The fix runs before the `isAbandoned()` check. An abandoned tx that was once confirmed could briefly show a non-(-1) blockheight before the abandoned flag overrides display. However, `isAbandoned()` is checked separately and the UI/RPC should handle this. Not a real issue.

### Could It Break Existing Functionality?
**No.** Previously `blockheight` was always -1 (the default). The fix only adds data that was missing. No existing code path relied on `blockheight == -1` for confirmed transactions.

---

## Regression Test File

**Location:** `test/functional/digidollar_bug11_bug13_regression.py`

### Bug #11 Tests (8 tests)
1. `test_bug11_mint_below_minimum` — 0 cents rejected ("must be positive")
2. `test_bug11_mint_negative` — Negative amount rejected
3. `test_bug11_mint_above_maximum` — 1 cent above max rejected ("Maximum mint amount")
4. `test_bug11_mint_way_above_maximum` — 10M cents rejected
5. `test_bug11_mint_at_exact_minimum` — Exact min (1 cent regtest) passes amount validation
6. `test_bug11_mint_at_exact_maximum` — Exact max (100000 cents regtest) passes amount validation
7. `test_bug11_mint_valid_midrange` — $50 passes amount validation
8. `test_bug11_error_message_contains_limit` — Error text includes dollar/cent values

### Bug #13 Tests (4 tests)
1. `test_bug13_confirmed_tx_has_blockheight` — Confirmed tx: blockheight > 0 and blockhash non-empty
2. `test_bug13_unconfirmed_tx_has_blockheight` — Unconfirmed tx: blockheight == -1
3. `test_bug13_blockheight_matches_actual_block` — blockheight matches the block it was mined in
4. `test_bug13_multiple_confirmations` — blockheight stable as confirmations increase

---

## Overall Assessment

Both fixes are clean, minimal, correct, and non-breaking. They follow existing codebase patterns and use proper integer math throughout. No security concerns identified.
