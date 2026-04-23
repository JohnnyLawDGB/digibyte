# DigiDollar / Oracle Bug Hunt Report

**Branch**: `feature/digidollar-v1`
**Date**: 2026-04-22
**Method**: 20-wave orchestrated audit, 55+ sub-agent investigations, personally code-verified findings
**Baseline**: 2173 unit tests pass · 7.96M assertions · 0 failures · 4 functional tests verified green

---

## Overall Assessment

DigiDollar is in "RED phase" implementation. Multiple consensus-critical paths are stubs or placeholder returns. The ERR (Emergency Redemption Ratio) subsystem is effectively broken — it bricks all redemptions once system health drops below 100%. The mainnet oracle validator is short-circuited. Several historical finds (F1 mainnet roster, F1b regtest roster) are resolved; an F1 residual (pollution of off-chain state by slots 17-29 on mainnet) remains lower-severity than first alarmed.

**Not production-ready for mainnet Phase 3 activation.**

---

## CRITICAL — Mainnet-Breaking / Consensus-Splitting

All five items below read directly from code.

| # | Title | File:Line | Summary |
|---|-------|-----------|---------|
| C1 | Mainnet oracle validation disabled | `src/oracle/bundle_manager.cpp:2230-2232` | `ValidateBlockOracleData` short-circuits `return true` for any chain that isn't TESTNET/REGTEST. Comment: "Oracle validation disabled on mainnet". After DD activates at height 22,014,720 a miner can stamp any oracle price into coinbase — unbounded DD mint at any price. |
| C2 | ERR consensus deadlock | `src/digidollar/validation.cpp:1557` → `:1676` | When `ctx.systemCollateral < 100`, dispatch routes to `ValidateEmergencyRedemptionConditions` which unconditionally returns `state.Invalid(..., "err-validation-incomplete")`. Normal path at `:1604` is also blocked under the same condition. Result: **a single oracle dip to 99% freezes every DD redemption.** Dispatch at `:1557` does NOT respect `skipOracleValidation`, so any historical block with `systemCollateral<100` breaks IBD. |
| C3 | ERR Tapscript leaf unspendable post-activation | `src/digidollar/scripts.cpp:100-103` + `src/script/interpreter.cpp:651` | Script emits `OP_CHECKCOLLATERAL <100> OP_LESSTHAN OP_VERIFY OP_DIGIDOLLAR OP_DDVERIFY ...`. Post-activation, `OP_DIGIDOLLAR` calls `script.GetOp(pc, ...)` and reads the next byte (`OP_DDVERIFY`, 0xbc). `CScriptNum` of empty push is 0 → `amount<=0` → `SCRIPT_ERR_INVALID_DD_AMOUNT`. **Every mint's ERR escape hatch cannot be script-path spent.** |
| C4 | Uncaught `CScriptNum` exception in transfer validator | `src/digidollar/validation.cpp:1199, 1206` | Both `CScriptNum(data, true)` constructions are outside any try/catch. Caller at `src/validation.cpp:2933` (ConnectBlock) also does not wrap. Non-minimal OP_RETURN push throws `scriptnum_error` → exception escapes DD validator → block-validation aborts. DoS / crash vector. |
| C5 | F1b regtest chainparams misalignment — **FIXED Wave 1** | `src/kernel/chainparams.cpp` CRegTestParams | `vOraclePublicKeys` was lexicographically sorted; `vOracleNodes` was ID-ordered. `ValidateOracleNodeAlignment()` aborted every regtest startup. Fixed by reordering `vOraclePublicKeys` to match ID order. Sibling of F1 which was fixed upstream in commit `0be06d5ea8`. |

---

## HIGH — Money-flow Correctness / State Integrity

| # | Title | File:Line | Summary |
|---|-------|-----------|---------|
| H1 | `SCRIPT_VERIFY_DIGIDOLLAR` in `STANDARD_SCRIPT_VERIFY_FLAGS` | `src/policy/policy.h:118` vs `src/validation.cpp:2567-2569` | Policy always-on; consensus gated on BIP9. Policy stricter than consensus → mempool may reject valid-by-consensus txs pre-activation. Direction is soft-fork safe but still a policy/consensus seam. Author added `// CRITICAL FIX` comments for stack effects; pc-advancement difference in `OP_DIGIDOLLAR` (pre: no advance, post: `GetOp` advances) is what enables C3. |
| H2 | DCA multiplier tables disagree | `src/consensus/digidollar.h:87-92` (chainparams) vs `src/consensus/dca.cpp:20-25` (hardcoded) | Chainparams: `{150→1.0x, 120→1.25x, 110→1.5x, <110→2.0x}`. Hardcoded: `{≥150→1.0x, 120-149→1.2x, 100-119→1.5x, <100→2.0x}`. Disagree at health 100-109 (2.0x vs 1.5x) and 120-149 (1.25x vs 1.2x). Callers split — txbuilder uses chainparams, validation uses hardcoded. Builder always more conservative today → no consensus reject, but any retune flips invariant. |
| H3 | `GetCurrentSystemCollateral()` hardcoded 150 | `src/digidollar/txbuilder.cpp:273-280` | Returns `DEFAULT_SYSTEM_COLLATERAL` unconditionally. Comment: "Placeholder implementation". Called at `:146` (mint), `:880` (ERR calc), `:1267` (path selection), `:1333` (ERR verification). Wallet never takes ERR path; DCA never varies in builder. |
| H4 | `GetBestHeight()` returns hardcoded 0 | `src/oracle/bundle_manager.cpp:47-51` | Comment: `// TODO: Wire up to ChainstateManager properly`. Wave 13 confirmed zero live callers today. Latent trap — any caller wired later pins everything to epoch 0. |
| H5 | `GetActiveOracleCount()` hardcoded `return 8` | `src/digidollar/health.cpp:823-829` | Ignores real oracle state. `IsOracleAvailable` → always reports ≥1 oracle active, bypasses real oracle consensus. Drives health tier alerts off fake number. |
| H6 | Volatility unit mismatch | `src/consensus/volatility.h:28` vs `src/digidollar/validation.cpp:1992` | Header declares `PricePoint::price` as "hundredths (50000 = $500.00)". Caller passes `ctx.oraclePriceMicroUSD` (10^6 granularity). 10,000× mismatch. Ratio-based volatility thresholds still work; but `lastOraclePrice` RPC and `CalculateSystemHealth` downstream consumers produce garbage. |
| H7 | `ContextualCheckBlock` Phase-1-only gate will break testnet post-Phase-2 | `src/validation.cpp:4591-4641` | Gates on `TESTNET || REGTEST`; `:4619` enforces `bundle.messages.size() != 1` (Phase-1 rule) regardless of current phase. Testnet has `nDigiDollarPhase2Height = 600` — already active. Masked today because try/catch at `:4635` returns true on deserialize failure; will fire once a real Phase-2 multi-message bundle at `vout[1]` successfully deserializes. |
| H8 | `importdigidollaraddress` is a lying stub | `src/rpc/digidollar.cpp:2292-2365` | Hardcodes `transactionsFound = 3` at `:2347` when `rescan=true`. Imports nothing, rescans nothing, returns `success: true`. Users relying on it for watch-only DD monitoring see no balances. |

---

## MEDIUM — Implementation Debt / Coverage Illusion

| # | Title | File:Line | Summary |
|---|-------|-----------|---------|
| M1 | Stale `DD_TX_VERSION = 0x44440000` | `src/digidollar/validation.h:33` | Canonical value is `0x0D1D0770` in `src/primitives/transaction.h:47`. 9+ test files build txs with `DigiDollar::DD_TX_VERSION \| (type<<16)` — low 16 bits = `0x0000`, fails `HasDigiDollarMarker` which checks `& 0xFFFF == 0x0770`. Those tests silently bypass the DD validation path they claim to test. |
| M2 | Shadow MuSig2 classes unused in production | `MuSig2Orchestrator`, `MuSig2SessionManager`, `MuSig2OracleParticipation` | Zero non-test, non-self references across `src/`. Live path is `OracleSigningOrchestrator`. Parallel implementations with differing timeouts (50 vs 100 blocks) and eviction rules. Risk: future code accidentally resolves `MuSig2SessionManager::GetInstance()` which asserts on uninitialized pointer. |
| M3 | Three fuzz harnesses not built | `oracle_bundle_validation.cpp`, `oracle_price_message.cpp`, `oracle_script_parsing.cpp` | Absent from `src/Makefile.test.include`. The harness designed to catch the `ExtractOracleBundle` bitmap-inflation bug exists but never runs. One-line Makefile fix reclaims coverage. |
| M4 | ~25 DD functional tests not registered | `test/functional/test_runner.py:261-287` | 51 DD-related test files on disk; ~24 registered. Unregistered includes `feature_oracle_p2p.py`, `rpc_getoracles_pending.py`, and all `digidollar_rpc_*.py` (11 files) plus `wallet_digidollar_{backup,descriptors,encryption,rescan,restore}.py`. These files run only if explicitly invoked; never in normal CI. |
| M5 | `nDigiDollarPhase3Height = 0` on all chains | `src/kernel/chainparams.cpp:316, 580, 1124` | Comment says "active immediately" — intentional. Makes `IsPhase3Active(height)` tautologically true from genesis. Intentional, but confusing — worth clarifying in docs or renaming to a sentinel check. |

---

## Resolved During Audit

| # | Title | Status |
|---|-------|--------|
| R1 | F1 mainnet oracle keyset (secp256k1 G at slot 0) | Resolved upstream in commit `0be06d5ea8` (2026-04-22 07:14). Mainnet `vOracleNodes[0..16]` now match `consensus.vOraclePublicKeys[0..16]`. |
| R2 | F1b regtest oracle keyset misalignment | Resolved Wave 1 remediation. `vOraclePublicKeys` reordered to match `vOracleNodes` ID order. Regtest node starts cleanly; 5 previously-blocked functional tests pass. |

## Downgraded / Corrected Findings

- **F1 residual (mainnet `vOracleNodes[17..29]` suspicious keys)**: First alarmed in Wave 8 as consensus-level exploit. Wave 13 re-analysis showed block-consensus is bounded by `DecodeBitmap` exact-size enforcement at `musig2_aggregator.cpp:63-80` and `ValidatePhaseThreeBundle`'s fixed `nOracleTotalOracles=17`. Reclassified to MEDIUM (off-chain MuSig2 session pollution + ExtractOracleBundle cache contamination). Still worth cleanup before mainnet activation.
- **`GetOraclePriceForHeight` fallback unbounded (Wave 14)**: Wave 19 showed the actual fallback is 1-epoch-back, not unbounded. Wave 14 claim was over-stated.
- **DCA hardcoded table "in `consensus/digidollar.cpp`"**: Agent 7A mislocated it. Correct location is `consensus/dca.cpp:20-25`.

---

## Recommended Priority Order

1. **C1** — decide mainnet oracle validation strategy. Remove the short-circuit or prove every downstream mainnet path is safely gated.
2. **C2** — implement ERR validation OR temporarily allow normal redeems when `systemCollateral<100%`. Add `skipOracleValidation` gate to the dispatcher at `validation.cpp:1557`.
3. **C3** — fix ERR Tapscript ABI. Either match opcode semantics or remove the ERR leaf from MAST until ERR is live.
4. **C4** — wrap `CScriptNum` constructions at `validation.cpp:1199, 1206` in try/catch → `state.Invalid("transfer-bad-op-return-encoding")`.
5. **H1** — decide policy flag strategy. Gate `SCRIPT_VERIFY_DIGIDOLLAR` in `STANDARD_SCRIPT_VERIFY_FLAGS` behind BIP9 deployment or document the split.
6. **H2** — unify DCA sources. Make `consensus/dca.cpp` read from chainparams or assert `builder_ratio >= consensus_ratio` as an invariant.
7. **H3/H4/H5** — wire the stubs or mark them unreachable with `assert(false, "stub")`.
8. **H6** — convert units at the `RecordPrice` callsite in `validation.cpp:1992` or update the doc + all consumers.
9. **H7** — gate `ContextualCheckBlock:4591` on phase state or delete in favor of the phase-aware validator in `bundle_manager.cpp`.
10. **H8** — implement or remove `importdigidollaraddress`. Do not ship a handler that lies.
11. **M1-M5** — cleanup items, one-line fixes each (Makefile registration, Deletion of unused classes, etc).

---

## Methodology Notes

- 20 sequential waves × ~3 sub-agents each = 55+ investigations.
- Full per-finding log at `/tmp/dd_bughunt_notes.md`.
- Memory entries updated: `project_red_hornet_f1.md` (RESOLVED), `project_red_hornet_f1b_regtest.md` (RESOLVED).
- Items C1–C5, H1–H8, M1–M5 were re-verified by directly reading the cited `file:line` in current working tree.
- Sub-agent reports produced ~250 additional findings (schema mismatches, RPC validation gaps, Qt UI issues, etc) that were NOT independently code-verified. Treat those as investigation leads, not confirmations.

---

*Generated by 20-wave orchestrated audit on `feature/digidollar-v1`. No mainnet/testnet chainparams modified. F1b fix committed in working tree only during audit; no push performed by the audit itself.*
