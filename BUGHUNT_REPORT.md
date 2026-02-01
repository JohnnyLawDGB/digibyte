# DigiDollar Bug Hunt Report

**Date:** 2026-01-31  
**Branch:** `feature/digidollar-v1`  
**Auditor:** Irene (AI Agent)  
**Test File:** `src/test/digidollar_bughunt_tests.cpp`

## Summary

| # | Severity | Bug | File(s) | Line(s) | Test Case | Confirmed |
|---|----------|-----|---------|---------|-----------|-----------|
| 1 | 🔴 HIGH | Oracle $10 price ceiling | oracle.cpp, net_processing.cpp | 36, 323, 5381 | `bughunt_1a_*`, `bughunt_1b_*` | ✅ Yes |
| 2 | 🔴 HIGH | RPC tier mapping mismatch | rpc/digidollar.cpp, consensus/digidollar.h, wallet/digidollarwallet.cpp | 51-63, 55-63, 6249-6267 | `bughunt_2_*` | ✅ Yes |
| 3 | 🟡 MEDIUM | ERR redemption deadlock | digidollar/validation.cpp | 1218-1221, 1529-1533 | `bughunt_3_*` | ✅ Yes |
| 4 | 🟡 MEDIUM | ValidateCollateralReleaseAmount no-op | digidollar/validation.cpp | 1227-1240 | `bughunt_4_*` | ✅ Yes |
| 5 | 🟡 MEDIUM | Hardcoded height/price in wallet | wallet/digidollarwallet.cpp | 889-890 | `bughunt_5_*` | ✅ Yes (code review) |
| 6 | 🟡 MEDIUM | GUI mock price 10,000x error | qt/digidollaroverviewwidget.cpp | 599 | `bughunt_6_*` | ✅ Yes |
| 7 | 🟡 MEDIUM | Volatility + ERR state ephemeral | consensus/err.cpp, consensus/volatility.cpp | static members | `bughunt_7_*`, `bughunt_7b_*` | ✅ Yes |
| 8 | 🟢 LOW | Transfer DD conservation placeholder | digidollar/validation.cpp | 885 | `bughunt_8_*` | ✅ Yes |
| 9 | 🟢 LOW | Static metadata map unbounded growth | digidollar/scripts.cpp | 215 | `bughunt_9_*` | ✅ Yes |

## Detailed Findings

### Bug #1: Oracle $10 Price Ceiling (HIGH) — FIX BEFORE MAINNET
**Problem:** Three different MAX_PRICE constants:
- `oracle.cpp:36` → `MAX_PRICE_MICRO_USD = 100,000,000` ($100) — in `IsValid()`
- `oracle.cpp:323` → `MAX_REALISTIC_PRICE = 10,000,000` ($10) — in `FilterOutliersAdvanced()`
- `net_processing.cpp:5381` → `MAX_PRICE_MICRO_USD = 10,000,000` ($10) — in P2P layer

**Impact:** If DGB exceeds $10, P2P rejects all oracle messages, outlier filter rejects all prices, oracle system completely stops, minting breaks.

**Fix:** Create single `ORACLE_MAX_PRICE_MICRO_USD` in shared header, set all three to $100 (or $1000).

---

### Bug #2: RPC Tier Mapping Mismatch (HIGH) — FIX BEFORE MAINNET
**Problem:** RPC `GetLockDaysForTier()` has 10 tiers (0-9) including a "2 years" tier at index 5. Consensus has 9 tiers (0-8) with no 2-year tier. Wallet correctly matches consensus.

**Impact:** Users minting via RPC with tiers 5-8 get shorter lock periods than intended:
- RPC tier 5 = 730d (2yr), consensus tier 5 = 1095d (3yr)
- RPC tier 6 = 1095d (3yr), consensus tier 6 = 1825d (5yr)

**Fix:** Remove tier 5 (730d) from RPC `GetLockDaysForTier`, align with consensus 9 tiers.

---

### Bug #3: ERR Redemption Deadlock (MEDIUM)
**Problem:** During ERR events:
1. Normal redemptions blocked by `ShouldBlockNormalRedemptionsDuringERR()` → returns true
2. ERR redemptions always rejected by `ValidateEmergencyRedemptionConditions()` → returns `Invalid("err-validation-incomplete")`

Result: **No redemption path available.** Users are stuck.

**Fix:** Either implement ERR redemption validation, or remove the normal-redemption block during ERR until ERR path is ready.

---

### Bug #4: ValidateCollateralReleaseAmount No-Op (MEDIUM)
**Problem:** Always returns `true` with log "simplified for Phase 1". Accepts any collateral release amount, including absurd values.

**Fix:** Implement proportional validation: `released ≤ (dd_burned / original_dd) × locked_collateral`

---

### Bug #5: Hardcoded Height/Price in Wallet (MEDIUM)
**Problem:** `TransferDigiDollar()` and `RedeemDigiDollar()` use:
- `int currentHeight = 100000;`
- `CAmount oraclePrice = 2500;`

**Fix:** Use `chain().getHeight()` and oracle price cache.

---

### Bug #6: GUI Mock Price 10,000x (MEDIUM)
**Problem:** Mock oracle returns micro-USD (6500 = $0.0065). GUI divides by 100 (thinks cents), shows $65.00. Affects overview, mint, and send widgets.

**Fix:** Change `priceCents / 100.0` to `priceMicroUSD / 1000000.0` in all 3 widgets.

---

### Bug #7: Volatility + ERR State Ephemeral (MEDIUM)
**Problem:** Both systems use static class members with no disk persistence. Node restart loses all state — active ERR events, volatility freezes, price history.

**Fix:** Derive state from chain data (oracle prices in blocks) on startup, or persist to LevelDB.

---

### Bug #8: Transfer DD Conservation Placeholder (LOW)
**Problem:** Line 885: `inputDD = outputDD;` before the conservation check. Makes the check always pass. Transfers could create DD from nothing.

**Fix:** Implement UTXO lookup to calculate actual inputDD from spent outputs.

---

### Bug #9: Static Metadata Map Unbounded Growth (LOW)
**Problem:** `g_scriptMetadataMap` in scripts.cpp grows forever. No eviction, no cleanup, no size limit.

**Fix:** Add LRU eviction or periodic cleanup. Phase 2 replaces this with UTXO DB.

---

## Recommended Fix Priority

1. **Bug #1** (Oracle $10 ceiling) — Immediate. System-breaking above $10.
2. **Bug #2** (RPC tier mismatch) — Immediate. User-facing data corruption.
3. **Bug #3** (ERR deadlock) — Before ERR activation. Currently no ERR on testnet.
4. **Bug #4** (Collateral release no-op) — Before mainnet. Allows collateral theft.
5. **Bug #6** (GUI 10,000x) — Quick fix. Display only, no consensus impact.
6. **Bug #5** (Hardcoded values) — Before mainnet. Incorrect fee/collateral calc.
7. **Bug #7** (Ephemeral state) — Before mainnet. Consensus divergence risk.
8. **Bug #8** (Conservation placeholder) — Before mainnet. DD inflation risk.
9. **Bug #9** (Memory leak) — Low priority. Phase 2 resolves this.
