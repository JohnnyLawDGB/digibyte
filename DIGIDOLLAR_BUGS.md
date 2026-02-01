# DigiDollar Bug Report

**Audit Date:** January 31, 2026  
**Branch:** `feature/digidollar-v1`  
**All Tests Passing:** ✅ 1,446 C++ unit tests, all Python functional tests  
**Test File:** `src/test/digidollar_bughunt_tests.cpp`

---

## Quick Reference

| #  | Sev  | Bug | Fix Status |
|----|------|-----|------------|
| 1  | 🔴  | Oracle rejects prices above $10/DGB (should be $100) | ✅ Fixed |
| 2  | 🔴  | RPC lock tier mapping doesn't match consensus | ✅ Fixed |
| 3  | 🟡  | ERR + normal redemption both blocked = deadlock | ✅ Fixed |
| 4  | 🟡  | Collateral release validation is a no-op | ❌ Not fixed (needs design) |
| 5  | 🟡  | `senddigidollar` uses hardcoded height=100000, price=2500 | ✅ Fixed |
| 6  | 🟡  | GUI shows oracle price 10,000x too high | ✅ Fixed |
| 7  | 🟡  | ERR and volatility state lost on node restart | ❌ Not fixed (complex) |
| 8  | 🟢  | Transfer DD conservation check is a placeholder | ❌ Not fixed (needs UTXO lookup) |
| 9  | 🟢  | Script metadata map grows forever (memory leak) | ✅ Fixed |
| 10 | 🟢  | Wasted HD key generation in `redeemdigidollar` RPC | ✅ Fixed |

---

## Bug #1 — Oracle $10 Price Ceiling

**Severity:** 🔴 HIGH — System breaks if DGB exceeds $10  
**Impact:** All oracle messages rejected, minting stops, no new blocks with oracle data

Three different max price constants exist:

| File | Line | Constant | Value | Used In |
|------|------|----------|-------|---------|
| `src/primitives/oracle.cpp` | 36 | `MAX_PRICE_MICRO_USD` | 100,000,000 ($100) | `IsValid()` |
| `src/primitives/oracle.cpp` | 323 | `MAX_REALISTIC_PRICE` | 10,000,000 ($10) | `FilterOutliersAdvanced()` |
| `src/net_processing.cpp` | 5381 | `MAX_PRICE_MICRO_USD` | 10,000,000 ($10) | P2P message validation |

**What happens at $10+/DGB:**
1. P2P layer rejects oracle messages → peers get misbehavior penalty
2. Outlier filter rejects all prices → no bundles created
3. No oracle data in blocks → minting breaks

**Fix:** Single shared constant in `src/primitives/oracle.h`, set to $100, used everywhere.

---

## Bug #2 — RPC Tier Mapping Mismatch

**Severity:** 🔴 HIGH — Users get wrong lock periods  
**Impact:** Tiers 5-8 via RPC create shorter locks than documented

RPC inserts a "2 years" tier that doesn't exist in consensus:

| Tier | RPC (`rpc/digidollar.cpp:56`) | Consensus (`consensus/digidollar.h`) | Wallet (correct) |
|------|------|-----------|--------|
| 0 | 1 hour | 1 hour (240 blocks) | 1 hour |
| 1 | 30 days | 30 days | 30 days |
| 2 | 90 days | 90 days | 90 days |
| 3 | 180 days | 180 days | 180 days |
| 4 | 1 year | 1 year | 1 year |
| **5** | **730 days (2yr)** ❌ | **3 years** | **3 years** ✅ |
| 6 | 1095 days (3yr) | 5 years | 5 years |
| 7 | 1825 days (5yr) | 7 years | 7 years |
| 8 | 2555 days (7yr) | 10 years | 10 years |
| 9 | 3650 days (10yr) | — | — |

**Fix:** Remove tier 5 (2yr) from RPC, align with consensus 9 tiers (0-8).

---

## Bug #3 — ERR Redemption Deadlock

**Severity:** 🟡 MEDIUM — Users can't redeem during ERR events  
**Impact:** Both redemption paths blocked simultaneously

When ERR activates (`s_currentState.isActive = true`):
- `ShouldBlockNormalRedemptionsDuringERR()` → blocks normal redemptions
- `ValidateEmergencyRedemptionConditions()` → always returns `Invalid("err-validation-incomplete")`

Result: no way to redeem. Deadlock.

**Mitigating factor:** ERR requires oracle consensus (8-of-15) which isn't implemented. Node restart clears ephemeral ERR state.

**Files:** `src/digidollar/validation.cpp` lines 1218-1221 and 1529-1533

**Fix:** Remove the normal-redemption block until ERR path is implemented.

---

## Bug #4 — Collateral Release Validation No-Op

**Severity:** 🟡 MEDIUM — No consensus check on collateral amounts  
**Impact:** Crafted raw transactions could release more collateral than entitled

```cpp
// src/digidollar/validation.cpp — ValidateCollateralReleaseAmount()
LogPrintf("DigiDollar: Collateral release validation passed (simplified for Phase 1)\n");
return true;  // Always passes, regardless of amounts
```

**Fix:** Implement proportional check: `released ≤ (dd_burned / original_dd) × locked_collateral`

---

## Bug #5 — Hardcoded Height/Price in senddigidollar

**Severity:** 🟡 MEDIUM — Wrong fee and collateral calculations  
**Impact:** Transfer transactions built with wrong chain height and oracle price

```cpp
// src/wallet/digidollarwallet.cpp lines 889-890 (called by senddigidollar RPC)
int currentHeight = 100000;  // TODO: Get actual height
CAmount oraclePrice = 2500;   // TODO: Get from MockOracleManager
```

Same pattern at lines 3632-3633 and 3753-3754.

**Fix:** Use `m_wallet->chain().getHeight()` and `MockOracleManager::GetInstance().GetCurrentPrice()`.

---

## Bug #6 — GUI Oracle Price 10,000x Too High

**Severity:** 🟡 MEDIUM — Display shows $65 instead of $0.0065  
**Impact:** Collateral requirements shown 10,000x too low in GUI

`MockOracleManager::GetCurrentPrice()` returns micro-USD (6500 = $0.0065).  
GUI divides by 100 (thinks it's cents) → shows $65.00.

| Widget | File | Line | Code |
|--------|------|------|------|
| Overview | `digidollaroverviewwidget.cpp` | 599 | `priceCents / 100.0` |
| Mint | `digidollarmintwidget.cpp` | 456 | `priceCents / 100.0` |
| Send | `digidollarsendwidget.cpp` | 502 | `priceCents / 100.0` |

RPC path at line 608 correctly uses `/ 1000000.0`.

**Fix:** Change all three to `priceMicroUSD / 1000000.0`, rename variable.

---

## Bug #7 — ERR and Volatility State Lost on Restart

**Severity:** 🟡 MEDIUM — Consensus inconsistency between nodes  
**Impact:** Nodes disagree on ERR/volatility state after restarts

Both use `static` class members with no persistence:
- `src/consensus/err.cpp:25` — `ERRState EmergencyRedemptionRatio::s_currentState`
- `src/consensus/volatility.h:81` — `static VolatilityState currentState`

After restart: ERR deactivates, volatility cooldowns reset, price history gone.

**Fix:** Derive state from oracle prices in blocks (which ARE persisted) on startup.

---

## Bug #8 — Transfer DD Conservation Placeholder

**Severity:** 🟢 LOW — DD could be created from nothing in transfers  
**Impact:** No actual verification that input DD equals output DD

```cpp
// src/digidollar/validation.cpp line ~916
inputDD = outputDD;  // Assume conservation for basic testing
```

The check after this line always passes because inputs are set equal to outputs.

**Fix:** Implement UTXO lookup to calculate actual inputDD from spent outputs.

---

## Bug #9 — Script Metadata Map Unbounded Growth

**Severity:** 🟢 LOW — Memory leak over time  
**Impact:** `g_scriptMetadataMap` grows forever, no eviction

`src/digidollar/scripts.cpp` lines 246-262: `RegisterScriptMetadata()` adds entries, nothing removes them.

**Fix:** Add LRU eviction or max size limit. Phase 2 replaces this with UTXO DB.

---

## Bug #10 — Wasted HD Key in redeemdigidollar

**Severity:** 🟢 LOW — Wastes wallet keypool addresses  
**Impact:** Each redemption burns one HD address for nothing

```cpp
// src/rpc/digidollar.cpp lines 1184-1212
CKey redemptionKey = GetHDKeyForDigiDollar(pwallet.get(), "dd-redeem");  // Generated
redeemParams.ownerKey = redemptionKey;  // Set
// ... then immediately overwritten:
redeemParams.ownerKey = ownerKey;  // Correct key replaces it
```

**Fix:** Remove lines 1182-1203.
