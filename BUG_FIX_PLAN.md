# DigiDollar Bug Fix Plan — TDD Approach
## Generated: March 11, 2026 by Irene (code-verified)

---

## Priority 1: Bug #22 — `bad-mint-lock-height-mismatch` False Positive on Rescan
**Status:** CONFIRMED IN CODE — `src/digidollar/validation.cpp:821`
**Severity:** High — blocks rescan/revalidation on 3+ nodes
**Root Cause:** `actualLockBlocks = lockTime - ctx.nHeight` where `ctx.nHeight` is current chain tip during rescan, not the block height when the tx was originally mined. Historical mints always fail because `lockTime` (e.g., 104041) < `currentHeight` (e.g., 123779) → negative result → rejected.

### Fix
Add a maturity check before the lock-height comparison. If `lockTime < currentHeight`, the lock has already expired/matured — skip the mismatch check entirely.

**File:** `src/digidollar/validation.cpp` ~line 819
```cpp
// BEFORE the actualLockBlocks calculation:
// If the lockHeight is in the past, this mint has already matured —
// skip the tier consistency check (only relevant at acceptance time)
if (lockTime <= ctx.nHeight) {
    LogPrint(BCLog::DIGIDOLLAR, "DigiDollar: Lock height %lld <= current height %d — mint already matured, skipping tier check\n",
             static_cast<long long>(lockTime), ctx.nHeight);
} else {
    int64_t actualLockBlocks = lockTime - ctx.nHeight;
    // ... existing check ...
}
```

### TDD Tests (write FIRST)
1. `rescan_mature_mint_passes` — Create a mint at height 100 with lockHeight=340, validate at height 500 → PASS (matured)
2. `rescan_immature_mint_passes` — Create a mint at height 100 with lockHeight=340, validate at height 200 → PASS (still within lock)
3. `fresh_mint_tier_mismatch_fails` — Create a mint claiming tier 1 (30 days) but lockHeight only 10 blocks ahead → REJECT
4. `fresh_mint_tier_correct_passes` — Create a mint with tier 1 and correct 172,800 block lock → PASS

**Test file:** `src/test/digidollar_lock_height_tests.cpp`
**Estimated effort:** 2 hours

---

## Priority 2: Bug #16 — Block Production Halt (insufficient-collateral)
**Status:** CONFIRMED IN CODE — `src/node/miner.cpp:201-203`
**Severity:** CRITICAL — can halt entire chain
**Root Cause:** Three-stage failure:
1. `addPackageTxs()` (line 321) includes DD txs with zero DD-specific validation
2. `TestBlockValidity()` (line 201) re-validates with potentially different oracle price
3. Failure `throw std::runtime_error` (line 203) — no recovery, no retry, no skip

### Fix (3 parts)

**Part A: DD pre-validation in addPackageTxs (skip, don't throw)**
Add a DD validity check inside `addPackageTxs()` before `AddToBlock()`. If a DD tx fails collateral validation against current oracle price, skip it (add to `failedTx`) instead of including it.

**File:** `src/node/miner.cpp` ~line 450 (before `AddToBlock`)
```cpp
// Check DD transactions against current oracle price before inclusion
if (IsDDTransaction(iter->GetTx())) {
    if (!ValidateDDForBlockInclusion(iter->GetTx(), pindexPrev)) {
        failedTx.insert(iter);
        continue;
    }
}
```

**Part B: Catch insufficient-collateral in CreateNewBlock (rebuild, don't throw)**
Wrap `TestBlockValidity` in a try-catch. On `insufficient-collateral`, identify the offending DD tx, remove it, and rebuild.

**File:** `src/node/miner.cpp` ~line 201
```cpp
try {
    if (!TestBlockValidity(state, ...)) {
        if (state.GetRejectReason() == "insufficient-collateral") {
            // Remove offending DD txs and retry
            RemoveDDTransactionsFromBlock(pblock);
            // Retry TestBlockValidity without DD txs
        } else {
            throw std::runtime_error(...);
        }
    }
} catch (...) { ... }
```

**Part C: Add 1% collateral safety margin at mint time**
**File:** `src/digidollar/txbuilder.cpp` ~line 165
Add 1% extra collateral at mint time so knife-edge price changes don't invalidate.

### TDD Tests (write FIRST)
1. `block_with_stale_dd_mint_skips_gracefully` — Mempool has DD mint at price P₁, oracle price drops to P₂, block template skips the mint
2. `block_without_dd_succeeds_after_dd_failure` — After DD tx removed, non-DD txs still produce valid block
3. `mint_includes_safety_margin` — Mint collateral is 1% above minimum requirement
4. `test_block_validity_retry_on_collateral_failure` — TestBlockValidity fails once, retries without DD tx, succeeds
5. `dd_and_large_dgb_transfer_coexist` — DD mint + large DGB transfer in mempool → valid block (with margin)

**Test file:** `src/test/miner_dd_validation_tests.cpp`
**Estimated effort:** 6-8 hours (most complex fix)

---

## Priority 3: Bug #21 — txindex Required But Not Enforced for DD Nodes
**Status:** PARTIALLY CONFIRMED — Fallback path exists but may fail on specific tx types
**Severity:** High — blocks reindex/IBD for some nodes
**Root Cause:** `ExtractDDAmountFromBlockDb` (line 318) exists as universal fallback, but gonzoboards' node still stalls. Two hypotheses: (A) txindex config outside `[test]` section doesn't apply to testnet, or (B) `ExtractDDAmountFromBlockDb` fails for certain tx formats. JohnnyLaw's Linux tests pass — may be Mac-specific.

### Fix (2 parts)

**Part A: Enforce txindex=1 for DD-enabled nodes**
At startup, if DigiDollar is enabled (or will be enabled based on deployment params), require `txindex=1` or error with a clear message.

**File:** `src/init.cpp` (during startup validation)
```cpp
if (dd_enabled && !gArgs.GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
    return InitError("DigiDollar requires -txindex=1. Please restart with -txindex=1 or add txindex=1 to your config.");
}
```

**Part B: Improve config section documentation**
Add a warning in the config template that `txindex=1` must be under the correct `[test]`/`[main]` section.

### TDD Tests
1. `dd_startup_requires_txindex` — Node with DD enabled + no txindex → startup error
2. `dd_startup_with_txindex_succeeds` — Node with DD enabled + txindex=1 → starts normally
3. `block_db_fallback_reads_correct_amount` — Without txindex, block-db lookup returns correct DD amount for MINT, TRANSFER, REDEEM tx types

**Test file:** `src/test/digidollar_txindex_tests.cpp`
**Estimated effort:** 3-4 hours

---

## Priority 4: Bug #19 — Fractional Cent Mints + "Partial Redeem Done" Log
**Status:** NEEDS MORE INVESTIGATION
**Severity:** High (if partial redeems are actually executing)
**Analysis:** The partial redeem guard exists at `validation.cpp:1710` (`ddBurned < originalDDMinted`). Fractional dollar amounts ($100.50 = 10050 cents) are valid per consensus rules (min 10000 cents = $100). The "partial redeem done" log message needs to be located — it's not in the current codebase grep, suggesting it may be coming from a different build or debug output.

### Investigation Steps
1. `grep -rn "partial redeem" src/` — find the exact log message source
2. Check if fractional cents create rounding issues in collateral calculation (integer math)
3. Verify `ddBurned == originalDDMinted` for fractional-cent mints (e.g., 10050 == 10050)

### TDD Tests (write during investigation)
1. `fractional_cent_mint_valid` — Mint 10050 cents ($100.50) → accepted
2. `fractional_cent_full_redeem_valid` — Redeem full 10050 cents → accepted
3. `fractional_cent_partial_redeem_rejected` — Burn 5025 of 10050 cents → rejected
4. `collateral_calculation_no_rounding` — Verify integer math produces same collateral for 10000 and 10050

**Estimated effort:** 4-6 hours (investigation + fixes if needed)

---

## Priority 5: Bug #2 — Oracle Keys Not Loading After Restart
**Status:** CONFIRMED IN CODE — `src/rpc/digidollar.cpp:3867`
**Severity:** High
**Root Cause:** `startoracle` calls `EnsureWalletIsUnlocked()` before `GetOracleKey()`. After node restart with encrypted wallet, wallet is locked → oracle can't load key → prompts for new key. No auto-start hook exists.

### Fix — TWO PATHS (encrypted vs unencrypted wallets)

**⚠️ SECURITY: NEVER auto-unlock encrypted wallets. That defeats the entire purpose of encryption.**

**Path A — Unencrypted wallets:** After wallet loads (`CWallet::Create()` or `LoadWallet()`), scan for stored oracle keys. If found and DD is active, auto-start the oracle. No security risk — unencrypted wallets have keys in plaintext already.

**Path B — Encrypted wallets:** Hook into `walletpassphrase` RPC. After the user manually unlocks, check for oracle keys and auto-start. Add a log message on startup: "Oracle key found for ID X but wallet is locked. Run 'walletpassphrase' to enable oracle." This gives clear guidance without compromising security.

**Files:**
- `src/wallet/wallet.cpp` — Add `TryAutoStartOracles()` called after wallet load (unencrypted only)
- `src/wallet/rpc/encrypt.cpp` — Hook into `walletpassphrase` success path to call `TryAutoStartOracles()`
- `src/rpc/digidollar.cpp` — Extract oracle start logic into reusable `TryStartOracleFromWallet(wallet, oracle_id)`

```cpp
// In wallet.cpp after wallet load completes:
void CWallet::TryAutoStartOracles() {
    if (IsCrypted() && IsLocked()) {
        // Encrypted wallet is locked — log guidance, don't touch keys
        for (int id = 0; id < ORACLE_TOTAL_COUNT; id++) {
            if (HasOracleKey(id)) {  // metadata check only, no key access
                LogPrintf("Oracle: Key stored for oracle %d but wallet is locked. "
                         "Run 'walletpassphrase <pass> <timeout>' then 'startoracle %d' "
                         "to enable oracle operation.\n", id, id);
            }
        }
        return;
    }
    // Unencrypted or already-unlocked wallet — safe to read keys
    for (int id = 0; id < ORACLE_TOTAL_COUNT; id++) {
        CKey key;
        if (GetOracleKey(id, key)) {
            OracleManager& om = OracleManager::GetInstance();
            if (!om.IsOracleRunning(id)) {
                std::string key_hex = HexStr(Span<const unsigned char>(key.begin(), key.end()));
                if (om.AddOracleNode(id, key_hex)) {
                    om.EnableOracle(id, true);
                    OracleNode* node = om.GetOracleNode(id);
                    if (node) node->Start();
                    LogPrintf("Oracle: Auto-started oracle %d from wallet key\n", id);
                }
            }
        }
    }
}
```

### TDD Tests
1. `oracle_autostart_unencrypted_wallet` — Unencrypted wallet with oracle key → oracle starts automatically on load
2. `oracle_no_autostart_locked_encrypted_wallet` — Encrypted+locked wallet → oracle NOT started, guidance logged
3. `oracle_autostart_after_walletpassphrase` — Encrypted wallet → `walletpassphrase` → oracle auto-starts
4. `oracle_no_key_no_autostart` — Wallet with no oracle keys → nothing happens, no errors
5. `oracle_already_running_no_duplicate` — Oracle already running → `TryAutoStartOracles()` is a no-op

**Test file:** `src/test/oracle_wallet_autostart_tests.cpp`
**Estimated effort:** 4-5 hours

---

## Priority 6: Bug #4 — Phase 2 Attestation Quorum Timing
**Status:** CONFIRMED — architectural timing issue
**Severity:** High (causes Emergency status, zero-price blocks)
**Root Cause:** Attestation generation is reactive (happens when quorum is reached in `ProcessOracleMessages`). If mining is faster than the attestation cycle, block templates request bundles before quorum forms. No retry or wait mechanism.

### Fix
Add a brief attestation wait window in `AddOracleBundleToBlock()`. If quorum hasn't formed yet but we have ≥ `min_oracle_count - 1` messages, wait up to N seconds for the final attestation before giving up.

**File:** `src/oracle/bundle_manager.cpp` ~line 497
```cpp
if (!bundle.HasConsensus(min_oracle_count)) {
    // Brief wait for stragglers if close to quorum
    if (bundle.messages.size() >= min_oracle_count - 1) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        // Re-check after wait
        // ... rebuild bundle from pending messages ...
    }
}
```

### TDD Tests
1. `bundle_waits_for_near_quorum` — 4/5 oracles reported, 5th arrives within 2s → bundle created
2. `bundle_gives_up_after_timeout` — 3/5 oracles reported, timeout expires → no bundle, zero-price fallback
3. `bundle_immediate_when_quorum_met` — 5/5 oracles → no wait, immediate bundle
4. `block_template_valid_with_late_quorum` — Quorum forms during wait → block includes valid bundle

**Test file:** `src/test/oracle_bundle_timing_tests.cpp`
**Estimated effort:** 5-6 hours

---

## Priority 7: Bug #3 — Persistent "Invalid Price" Messages
**Status:** LIKELY related to Bug #4 (stale oracle state)
**Severity:** Medium
**Root Cause:** Needs investigation of price fetch thread error recovery. Likely the price thread enters an error state and doesn't recover until restart.

### Investigation Steps
1. Trace the price fetch thread lifecycle in `OracleNode::Start()` / price thread
2. Check for error states that don't auto-recover
3. Add health monitoring / auto-restart for price threads

**Estimated effort:** 3-4 hours (investigation)

---

## Execution Order

| # | Bug | Priority | Effort | Dependencies |
|---|-----|----------|--------|-------------|
| 1 | #22 (lock height) | P1 | 2h | None — easiest win |
| 2 | #16 (block halt) | P2 | 6-8h | None — most critical |
| 3 | #21 (txindex) | P3 | 3-4h | None |
| 4 | #19 (fractional) | P4 | 4-6h | Investigation first |
| 5 | #2 (oracle keys) | P5 | 4-5h | None |
| 6 | #4 (quorum timing) | P6 | 5-6h | None |
| 7 | #3 (invalid price) | P7 | 3-4h | Bug #4 may fix this |

**Total estimated effort: 27-35 hours**

## TDD Workflow Per Bug
1. Write failing test(s) FIRST
2. Run full test suite — confirm new tests FAIL
3. Implement minimal fix
4. Run full test suite — confirm ALL tests PASS (new + existing)
5. Commit test + fix separately (test first, then fix)
6. Do NOT push — Jared reviews

## Sub-Agent Strategy
Use Codex 5.3 sub-agents for:
- Writing test scaffolding (test files, fixtures, mocks)
- Implementing fixes after tests are written
- Running test suites

Keep Opus (me) for:
- Code review and verification
- Architecture decisions
- Coordination and progress tracking
