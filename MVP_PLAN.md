# MVP_PLAN.md — DigiDollar Mainnet V1 Work Plan

**Branch:** `feature/digidollar-v1`  
**Purpose:** TDD wave-based execution plan for DigiDollar V1 mainnet deployment.  
**Rule:** Planning only. All code work done by separate agents using TDD. No pushes — Jared reviews and pushes.

---

## 0. Non-Negotiable V1 Invariants

1. **No early redemptions.** Collateral cannot unlock before the chosen timelock expires.
2. **DD burn enforcement is mandatory.** A collateral vault spend must require burning the correct DigiDollar amount.
3. **ERR must be finished, not disabled.** If system health drops below 100%, redemption still requires timelock expiry and returns full collateral — but the redeemer must burn extra DD according to the ERR ratio. No haircut on DGB.
4. **Consensus math must be deterministic.** No float/double in consensus-visible DCA/ERR/collateral math. Integer basis points and `__int128` only.
5. **Oracle data must be deterministic.** DCA, ERR, mint validation, and redemption validation must use the same consensus-safe price/health source.
6. **MuSig2 oracle bundles only for V1.** Legacy v0x01/v0x02 formats were development steps — removed from production validation, mining, and fallback paths.
7. **Oracle roster must be expandable.** Launch quorum: minimum 9 signatures. Active oracle set must grow beyond 17 via deterministic consensus-visible roster rules.
8. **Lock tiers are canonical only.** No in-between or custom lock durations for V1.
9. **Watch-only DD is read-only.** Watch-only wallets may display/monitor DD positions but cannot mint, redeem, send, or sign.
10. **Legacy DigiDollar wallets unsupported for V1.** Require descriptor/bech32m-capable wallets. Fail clearly on unsupported types.

---

## 1. Audit Sources

### Primary DigiDollar / Red Hornet
- `DIGIDOLLAR_BUG_HUNT_REPORT.md`, `Z_RED_HORNET.md`, `Z_RED_HORNET_v2.md`
- `doc/RED_HORNET_V3_REPORT.md`, `reports/red_hornet_final_report.md`
- `reports/red_hornet_ledger.md`, `reports/red_hornet_security_final_report.md`
- `reports/red_hornet_security_ledger.md`, `RELEASE_v9.26.0-rc33.md`

### Architecture / repo maps
- `ARCHITECTURE.md`, `REPO_MAP.md`, `DIGIDOLLAR_ARCHITECTURE.md`, `REPO_MAP_DIGIDOLLAR.md`

### Fuzz / regression readiness
- `doc/FUZZ_COMPLETION_REPORT.md`, `doc/FUZZ_MARATHON_COMPLETE.md`, `doc/FUZZ_PHASE4_FINAL_REPORT.md`

---

## 2. Red Hornet Continuation Fixes (Pre-committed by Red Hornet Team)

The Red Hornet team will have committed all campaign fixes before this plan's Wave 1 begins. The Pre-Wave agent's only job is to verify the baseline is clean and the full test suite passes — no commit-splitting work needed.

**Fixes expected in baseline (committed by Red Hornet team):**

| RH ID | Severity | Fix |
|-------|----------|-----|
| DD-RH-001 | Medium | Valid reordered mint now correctly tracked in health accounting |
| DD-RH-003 | Low | DCA fractional multipliers now ceiling-rounded (no undercut) |
| DD-RH-004 | Low | Required collateral uses ceiling division (no one-sat undercount) |
| DD-RH-005 | High | Nonzero-vout mint collateral no longer treated as redemption fee input |
| DD-RH-006 | Low | `getprotectionstatus` no longer reports false emergency on zero DD supply |
| DD-RH-007 | Critical | Transfer OP_RETURN spoof cannot inflate later spends |
| DD-RH-008 | Medium | DD address validators reject corrupted/wrong-shape strings |
| DD-RH-009 | Medium | `importdigidollaraddress` returns explicit failure instead of false success |
| DD-RH-105 | Medium | Transfer rejects unresolved zero-value DD inputs |
| DD-RH-106 | High | Redemption burn accounting prefers authoritative mint tx over poisonable metadata |
| DD-RH-109 | Medium | Transfer builder rejects underfunded DGB fee inputs |
| DD-RH-112 | Medium | Malformed transfer OP_RETURN script numbers reject cleanly |
| DD-RH-113 | High | Mint accounting prefers modern `"DD"` OP_RETURN over legacy spoof marker |
| DD-RH-114 | High | Non-canonical `OP_1` impostor scripts rejected as DD outputs |
| DD-RH-115 | High | Mint price/collateral validation no longer depends on `skipOracleValidation` |
| DD-RH-116 | Low | `getmockoracleprice` is activation-gated |
| DD-RH-118 | Medium | Reorg connect/disconnect now restores DD supply exactly near `MAX_DIGIDOLLAR` |
| DD-RH-120 | Low | Non-final DD transactions now fail cheaply before DD contextual validation |
| DD-RH-121 | Medium | DD transfer descendants no longer survive reorg through stale txindex |

**Additional fixes from waves 6–19** (DD-RH-010 through DD-RH-049) will also be in the baseline. Pre-Wave agent should verify by reading `reports/red_hornet_ledger.md` to confirm all "fixed" items are present in committed history.

**Pre-Wave responsibility:** Verify baseline compiles, all committed RH fixes are present, and the full unit + functional + fuzz suite passes clean. No commit work. No push.

---

## 3. P0 Launch Blockers (Detail + TDD Requirements)

### P0.1 — DD Burn Enforcement on Every Collateral Spend (`DD-RH-069`)

**Problem:** The normal collateral script path is essentially `CLTV + owner sig`. After timelock, a non-DD tx can spend backing DGB without entering DD redemption validation — leaving DD supply live with no collateral.

**Files:**
- `src/script/interpreter.cpp`, `src/script/script.h`
- `src/digidollar/scripts.cpp`, `src/digidollar/validation.cpp`

**Fix direction:**
1. Consensus-level detector: any input spending a known DD collateral vault must enter DD redemption validation, even without a DD marker in the spending tx.
2. Require correct DD burn amount before collateral release.
3. Fix both normal and ERR Taproot leaves so ABI matches `OP_DIGIDOLLAR` / `OP_DDVERIFY` semantics.
4. Timelock mandatory for both normal and ERR leaves.

**TDD requirements:**
- Failing test: post-timelock non-DD collateral spend is rejected.
- Failing test: normal redemption succeeds only when full original DD amount is burned.
- Failing test: partial burn cannot release collateral.
- Failing test: collateral spend with no DD inputs cannot release collateral.
- Failing test: reordered mint outputs still identify the correct collateral output.

---

### P0.2 — ERR End-to-End (`ARCH-RH-004`, `DD-RH-110`)

**Design:** ERR is not early redemption. ERR only changes the DD burn requirement after timelock expiry when system health drops below 100%.

**Current problems:**
- ERR consensus validation incomplete.
- ERR Taproot leaf ABI broken (`OP_DIGIDOLLAR` not followed by positive DD amount).
- RPC/wallet can disagree with consensus on ERR state.

**Files:**
- `src/consensus/err.cpp/h`, `src/digidollar/validation.cpp` (ERR sections)
- `src/digidollar/txbuilder.cpp`, `src/rpc/digidollar.cpp`
- `src/test/digidollar_err*_tests.cpp`, `src/test/digidollar_redeem_tests.cpp`

**Required V1 behavior:**
- Health `≥ 100%`: burn original DD, return full DGB.
- Health `< 100%`: burn `ceil(originalDD * 10000 / errRatioBps)`, return full DGB.
- ERR still requires timelock expiry.
- New minting blocked while ERR active.
- Normal redemption blocked while ERR active unless ERR burn requirement met.
- No DGB haircut. Extra DD burned is the penalty.

**Consensus math:** Replace doubles with integer basis points. `ceil(originalDD * 10000 / errRatioBps)` using `__int128`. Compare against `MAX_MONEY` before casting.

**TDD requirements:**
- Failing test: ERR no longer returns `err-validation-incomplete` for valid ERR redemption.
- Failing test: health `<100%`, original-only burn fails.
- Failing test: health `<100%`, required extra burn succeeds.
- Failing test: ERR before timelock fails.
- Failing test: normal redemption while ERR active fails unless ERR burn requirement met.
- Functional test: regtest price/health crash activates ERR, wallet builds correct ERR redemption, block validates.
- Fuzz: ERR burn boundary fuzz for health 0, 1, 84, 85, 89, 90, 94, 95, 99, 100, overflow values.

---

### P0.3 — Canonical Deterministic Health for DCA + ERR (`DD-RH-108`)

**Problem:** DCA/ERR decisions can depend on cached or placeholder health. `TxBuilder::GetCurrentSystemCollateral()` returns a default healthy value; validation can fall back to `150%` when data is missing.

**Files:**
- `src/digidollar/health.cpp/h`, `src/consensus/dca.cpp`

**Required V1 behavior:**
- One canonical health calculation for consensus validation.
- Inputs: chain-derived DD supply, locked collateral, and the same oracle price used for the block/tx being validated.
- No healthy fallback after activation when oracle/health data is missing. Fail closed.
- Caches OK for performance but consensus validates against deterministic data.
- DCA multiplier doubles → integer multipliers/basis points. Conservative rounding up. `__int128` + `MAX_MONEY` checks.

**TDD requirements:**
- Failing test: stale cached health cannot allow a mint at base collateral when current health requires DCA.
- Failing test: missing post-activation price/health rejects DD validation instead of defaulting to 150%.
- Failing test: DCA, ERR, RPC quote, and txbuilder agree on health source for the same block/price.
- Fuzz: health calculation with extreme collateral, DD supply, and price values.

---

### P0.4 — Lock Tier Canonicalization (`DD-RH-107`)

**Problem:** Non-standard lock duration just above a tier boundary can receive the next tier's lower collateral ratio. E.g., `1-hour tier + 1 block` gets the 30-day ratio.

**Files:**
- `src/consensus/digidollar.cpp/h`, `src/digidollar/validation.cpp` (lock-tier sections), `src/digidollar/txbuilder.cpp`

**Required V1 rule:** Accept only canonical lock tiers. Reject all in-between/custom durations.

**TDD requirements:**
- Failing test: each canonical tier is accepted with correct collateral ratio.
- Failing test: `tier + 1 block`, `tier - 1 block`, and arbitrary custom durations reject.
- Functional test: RPC/Qt cannot create non-canonical lock durations.

---

### P0.5 — Oracle Consensus / Activation Rules (`ARCH-RH-001`, `DD-RH-084/085/086/117`)

**Locked direction:** Non-DD blocks are valid without oracle data. Any block containing a DD transaction or spending a DD collateral vault must include a valid MuSig2 oracle bundle — missing or invalid oracle data makes the DD-touching block invalid for every node.

**Files:**
- `src/oracle/*`, `src/primitives/oracle*`
- `src/kernel/chainparams.cpp`, `src/consensus/params.h`
- `src/node/miner.cpp`, `src/validation.cpp`

**Required V1 rules:**
1. **MuSig2 only.** Remove v0x01/v0x02 acceptance, mining, and fallback paths.
2. **No separate oracle activation maze.** DD V1 starts with MuSig2. Gate DD behavior to the DigiDollar deployment predicate, not stale phase heights.
3. **Avoid chain splits:** Non-DD blocks need no oracle bundle. DD-touching blocks must have one valid MuSig2 oracle bundle.
4. **Expandable roster:** variable-length signer encoding + deterministic height/deployment-indexed active roster. Quorum floor remains 9. Expansion requires a new release with explicit activation rules.
5. **Reserve/inactive operators** listed but cannot count toward quorum.
6. **Domain separation:** signatures commit to network/genesis/deployment, epoch/height, price, timestamp, message type.
7. **Signer liveness:** deterministic reselection/retry rules so nonce withholding cannot stall block construction.
8. **Mempool policy:** may require recent valid MuSig2 oracle quote for DD transactions; block validity is the final consensus rule.

**TDD requirements:**
- Failing test: v0x01/v0x02 oracle formats rejected; not mined as fallback.
- Failing test: non-DD block without oracle data remains valid.
- Failing test: any DD mint/transfer/redeem/collateral-spend block without oracle data is rejected.
- Failing test: malformed/stale/wrong-domain/insufficient-quorum oracle bundle makes DD-touching block invalid.
- Failing test: roster with >17 active operators verifies valid 9-sig MuSig2 bundle after roster activation.
- Failing test: same >17 roster bundle rejected before roster activation.
- Failing test: out-of-roster signer, duplicate signer, wrong-domain sig all reject.
- Functional: 9-of-N signs passes, 8-of-N fails, N can be increased without invalidating historical blocks.

---

### P0.6 — `MAX_DIGIDOLLAR` Supply Cap Clarification (`DD-RH-119`) — RESOLVED

**Severity:** High → RESOLVED as Option B (docs only)

**Decision (locked):** DigiDollar has **no global supply cap**. Total DigiDollars in circulation are theoretically unlimited — the only constraint is available DGB collateral and the per-block minting rate. `MAX_DIGIDOLLAR` is a per-output serialization bound only, not a global cap. No consensus change required.

**Docs already updated:**
- `REPO_MAP_DIGIDOLLAR.md` — removed "hard cap on total DD supply" language; clarified as per-output bound
- `AlertThresholds::MAX_DD_SUPPLY` in `health.h` — monitoring alert threshold only, not a supply cap; rename to `ALERT_DD_SUPPLY` for clarity

**Wave 6 docs agent must also verify:** No remaining "global supply cap" or "total supply limit" language survives in `DIGIDOLLAR_ARCHITECTURE.md`, `DIGIDOLLAR_EXPLAINER.md`, or any whitepaper/spec docs under `digidollar/`.

---

### P0.7 — Qt Mint Owner Key Lifecycle (`DD-RH-034`)

**Severity:** High

**Problem:** Qt mint generates a random non-HD key (`MakeNewKey(true)`) and stores it in the DD wallet only AFTER `commitTransaction()`. The RPC mint path uses the wallet-derived `GetHDKeyForDigiDollar()` helper. A crash between successful broadcast and `StoreOwnerKey()` leaves the collateral vault locked forever — the owner key that controls normal and ERR redemption is lost. This is a **loss-of-funds** risk on mainnet.

**Files:**
- `src/qt/walletmodel.cpp` (lines 866–870, 1111–1138)
- `src/rpc/digidollar.cpp` (HD key helper, lines 97–151)
- `src/wallet/` (key derivation path)

**Required V1 fix:** Move `GetHDKeyForDigiDollar()` to shared wallet code (not private to `rpc/digidollar.cpp`). Both Qt and RPC mint must derive the owner key from the wallet's HD seed. Persist the derived key to wallet database before broadcast. If the wallet is non-HD, fail clearly at mint time with an explicit error.

**TDD requirements:**
- Failing test: Qt mint path derives owner key from HD seed (same derivation as RPC path).
- Failing test: owner key persisted to wallet database before transaction broadcast.
- Failing test: non-HD wallet returns explicit error at Qt mint time.
- Failing test: wallet restore from seed recovers Qt-minted DD position owner key without extra wallet.dat backup.
- Functional test: mint via Qt, crash-simulate between broadcast and StoreOwnerKey, reload wallet — position is still redeemable.

---

### P0.8 — Volatility Precheck (`DD-RH-111`)

**Problem:** First mint that crosses a volatility threshold can pass because freeze state is checked before recording/evaluating the candidate price.

**Files:** `src/digidollar/validation.cpp` (volatility section only)

**Fix:**
- Non-mutating "would this candidate price freeze minting?" check before any state mutation.
- Reject candidate mint if it crosses freeze threshold.

**TDD requirements:**
- Failing test: seeded prior price + candidate mint price crossing threshold is rejected.
- Failing test: invalid candidate tx cannot poison volatility state.
- Fuzz: volatility boundary/cooldown thresholds.

---

### P0.9 — Mempool / Block / IBD / Reorg Consensus Parity

**Problem:** Several audit findings were chain-split shaped: behavior differed between mempool and block validation, IBD and caught-up nodes, stale cache and fresh cache, or pre/post reorg state.

**Files:**
- `src/validation.cpp`, `src/node/miner.cpp`, `src/net_processing.cpp`
- `test/functional/` (functional test scenarios)

**Required V1 behavior:**
- `AcceptToMemoryPool`, miner block assembly, `ConnectBlock`, IBD, reindex, and reorg replay apply identical DD consensus rules.
- Mempool may be stricter for liveness/safety but must never allow a DD tx that block validation rejects.
- `skipOracleValidation` must not bypass post-activation DD requirements.
- Reorg rollback must restore DD supply, collateral, health, oracle cache, wallet positions, and pending redemption state deterministically.
- Historical validation uses the oracle bundle and roster rules at that historical height.

**TDD requirements:**
- Functional: DD block accepted by caught-up node is also accepted by IBD/reindex node.
- Functional: DD mempool acceptance and mined block validity agree on oracle, DCA, ERR, and burn requirements.
- Functional: reorg across DD mint/transfer/redeem restores supply/collateral/health and wallet state exactly.
- Functional: expanded oracle roster validates only after activation height and does not invalidate older blocks.
- Fuzz: validation contexts with `skipOracleValidation`, stale cache, missing cache, and reorg rollback.

---

## 4. P1 Mainnet Readiness Work

### P1.1 — Wallet / RPC / Qt State Consistency
- `getprotectionstatus`, `getredemptioninfo`, wallet redemption, Qt display, and consensus agree on normal vs ERR state.
- RPC reports required DD burn during ERR. Wallet selects enough DD inputs for ERR extra burn.
- Watch-only wallets: display/monitor only. Cannot mint/send/redeem/sign.
- Restore/rescan recovers DD positions with reordered outputs and modern metadata.
- Legacy wallet path fails with explicit unsupported error.

**TR-RH-003 — RPC unit/filter drift (reachable, loss-of-funds adjacent):**
- `listdigidollarpositions min_amount` compared DGB units to DD cents → fix filter to use DD cents.
- `senddigidollar change_amount` reported wallet remaining DGB as DD change → fix to report correct unit.
- `getdigidollarbalance minconf` accepted but ignored `minconf` parameter → apply correctly.
- Fix these in RPC layer; add schema validation tests.

**TR-RH-004 — Qt stale collateral ratios for tiers 5–8:**
- `WalletModel::calculateRequiredCollateral()` had stale hardcoded ratios for higher tiers.
- RPC collateral estimate reported base consensus minimum; `MintTxBuilder` adds 1% safety margin.
- Decide whether RPC should report consensus minimum or builder-padded amount and document clearly.
- Fix Qt to read ratios from consensus parameters, not hardcoded constants.

**TR-RH-005 — `listdigidollaraddresses` hides zero-balance DD addresses:**
- Currently only lists addresses inferred from current DD UTXOs; hides generated zero-balance addresses and hardcodes `iswatchonly=false`.
- Fix: list all generated DD addresses including zero-balance; set `iswatchonly` from wallet metadata.

**TR-RH-006 — `CDigiDollarAddress` accepts any chain's address prefix:**
- A mainnet wallet can accept testnet/regtest-prefixed DD address if version+checksum are valid.
- Fix: validate the address version byte against the currently active chain params. Reject cross-chain addresses.

**ARCH-RH-003 — Watch-only address storage (V1 scope: explicit failure is acceptable):**
- `importdigidollaraddress` now returns `success=false` with an explicit warning (fixed in Wave 9).
- For V1: this explicit failure is sufficient. Full watch-only address import/rescan/listing is a post-V1 feature.
- Ensure the error message is clear and documented in release notes.

**TDD requirements:**
- Functional: health crash changes all RPC/Qt-visible protection state consistently.
- Functional: wallet builds both normal redemption and ERR redemption.
- Functional: legacy wallet path fails with explicit unsupported error.
- Functional: watch-only wallet can view DD metadata but cannot mint/send/redeem/sign.
- Unit: restore/rescan recovers DD positions with reordered outputs.
- Unit: `listdigidollarpositions min_amount` filters in DD cents, not DGB satoshis.
- Unit: Qt collateral estimate matches consensus tier ratios for all tiers including 5–8.
- Unit: `CDigiDollarAddress` rejects testnet address on mainnet and vice versa.
- Functional: `importdigidollaraddress` returns explicit unsupported error (not false success).

### P1.2 — Oracle Operator Key Security
- Oracle operator private keys must not remain plaintext if wallet/node is encrypted.
- Key IDs/slots cannot drift from consensus roster.
- Define backup/rotation procedure.

### P1.3 — Backward Compatibility
- Existing non-DD DigiByte blocks/transactions remain valid.
- No mainnet DD state before activation; rules can be finalized without preserving old mainnet DD positions.
- Testnet reset allowed if final consensus changes require it. Recommendation: reset testnet after final V1 changes.
- Legacy DigiDollar wallets unsupported; document in release notes and RPC errors.

### P1.4 — Documentation Cleanup
Update after code fixes: `ARCHITECTURE.md`, `DIGIDOLLAR_ARCHITECTURE.md`, `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`, `DIGIDOLLAR_EXPLAINER.md`, `REPO_MAP_DIGIDOLLAR.md`, release notes for next RC.

---

## 5. Decisions Locked

| # | Decision |
|---|---------|
| 1 | **DD burn enforcement:** locked. Collateral unlock requires burning DD. |
| 2 | **ERR:** locked. Finish ERR; do not disable it. No DGB haircut. |
| 3 | **No early redemptions:** locked. ERR does not bypass timelock. |
| 4 | **ERR ratio table:** locked. Keep current 95/90/85/80% ratio tiers. |
| 5 | **Oracle strictness:** locked. Non-DD blocks may omit oracle data; any DD-touching block requires valid MuSig2 data and fails deterministically without it. |
| 6 | **Oracle bundle format:** locked. MuSig2 only. Remove v0x01/v0x02 production support and fallback paths. |
| 7 | **Oracle roster:** locked. Min quorum remains 9. Active oracle set expandable beyond 17 via deterministic roster rules. |
| 8 | **Expandable roster mechanism:** locked. Variable-length signer encoding + deterministic height/deployment-indexed roster. New oracles added by release + explicit activation. |
| 9 | **Lock tiers:** locked. Canonical tiers only; no in-between/custom lock durations. |
| 10 | **Legacy DigiDollar wallets:** locked. Unsupported for V1. |
| 11 | **Watch-only DD:** locked. Read-only display/monitoring allowed; spend/sign requires private keys. |
| 12 | **Qt mint owner key:** locked. Must derive from HD seed via shared wallet helper. Non-HD wallet fails clearly at mint time. Key persisted before broadcast. |
| 13 | **Cross-chain DD address:** locked. `CDigiDollarAddress` must validate version byte against active chain params. Testnet addresses rejected on mainnet. |
| 14 | **No global DD supply cap:** locked. DigiDollar total supply is theoretically unlimited. `MAX_DIGIDOLLAR` is a per-output serialization bound only. No consensus-enforced aggregate cap. The only rate limit is per-block minting. |

---

## 6. TDD Wave Execution Model

**Rules for all agents:**
1. Write the failing test first. Confirm it fails for the expected reason.
2. Implement the minimum safe fix.
3. Run targeted tests to confirm green.
4. Commit immediately after targeted tests pass — one commit per logical fix (not one per file, not one giant dump).
5. **Commit message standard:** Subject line (≤72 chars) states what was broken and what was fixed in plain English. No ticket jargon, no vague "fix bug" messages. Body optional — only add it if the why isn't obvious from the subject. Examples:
   - `digidollar: collateral spend could bypass DD burn check — add consensus-level vault detector`
   - `digidollar/err: ERR validation returned incomplete for valid redemption — wire integer burn calc`
   - `digidollar/health: stale cached health could allow mint under DCA — fail closed when data missing`
6. At end of each wave: run **all unit tests** (`make check`), **all functional tests** (`test/functional/test_runner.py`), **all fuzz tests**. These are the regression gate — no exceptions.
7. Wave does not close until: full test gate is green AND working tree is clean (all changes committed, no pending diffs).
8. Never touch a file owned by a sibling agent in the same wave.
9. Local commits only. No pushing.

---

### Pre-Wave — Verify Clean Baseline (1 agent, sequential)

**Goal:** Confirm the Red Hornet team's commits are all present and the full test suite passes before Wave 1 begins. No new features, no new files, no commit work.

**Agent 1 tasks:**
- Read `reports/red_hornet_ledger.md` — confirm every "fixed" RH ID has a corresponding commit in git log
- Run full unit + functional + fuzz suite: `make check && test/functional/test_runner.py --jobs=4`
- Confirm clean working tree (no uncommitted diffs)
- Report any missing RH fixes or test failures before Wave 1 begins

**File scope:** Read-only git history verification + test runner only.

**Gate:** Full unit + functional + fuzz suite passes cleanly. All Section 2 RH IDs confirmed committed.

---

### Wave 1 — TDD Red Phase: Write All Failing Tests (3 agents, parallel)

**Goal:** Write all failing tests for the P0 blockers before touching any production code. Zero production code changes in this wave. All agents write to different test files/directories.

**Agent A — Burn + Tiers + Address** (test files only, no conflicts with B/C)
- Write failing unit tests for P0.1 (DD burn enforcement, partial burn, no-DD-input collateral spend)
- Write failing unit tests for P0.4 (lock tier boundary above/below, canonical tier acceptance)
- Write failing unit tests for TR-RH-006 (mainnet rejects testnet DD address, regtest rejects mainnet address)
- Target files: `src/test/digidollar_burn_enforcement_tests.cpp` (new), `src/test/digidollar_locktier_tests.cpp` (new/modify), `src/test/digidollar_address_tests.cpp` (new/modify)

**Agent B — Oracle** (test files only, no conflicts with A/C)
- Write failing unit tests for P0.5 (v0x01/v0x02 rejection, non-DD block without oracle, DD block requires oracle, malformed/stale/wrong-domain rejection)
- Write failing unit tests for expandable roster (>17 operators, pre/post activation)
- Target files: `src/test/digidollar_oracle_musig2_tests.cpp` (new), `src/test/digidollar_oracle_roster_tests.cpp` (new or modify existing)

**Agent C — ERR + Health + Volatility + Wallet + RPC** (test files only, no conflicts with A/B)
- Write failing unit tests for P0.2 (ERR: extra burn success/fail, ERR before timelock, normal while ERR active)
- Write failing unit tests for P0.3 (stale health rejection, missing-price fail-closed, DCA/ERR/RPC health agreement)
- Write failing unit tests for P0.7 (Qt mint derives HD key, persists before broadcast, non-HD wallet fails clearly)
- Write failing unit tests for P0.8 (volatility threshold candidate rejection, no state poison)
- Write failing unit tests for TR-RH-003/004 (listdigidollarpositions DD-cent filter, Qt stale tier ratios)
- Target files: `src/test/digidollar_err_tests.cpp` (new/modify), `src/test/digidollar_health_dca_tests.cpp` (new), `src/test/digidollar_volatility_tests.cpp` (new/modify), `src/test/digidollar_wallet_hd_tests.cpp` (new), `src/test/digidollar_rpc_unit_tests.cpp` (new/modify)

**Gate after Wave 1:**
- `make check` passes — zero pre-existing test regressions
- New tests compile and fail for the correct expected reason (not compile error, not wrong-failure message)
- All new test files committed; working tree clean
- Commit messages explain what each test is covering and why it must fail at this stage

---

### Wave 2 — Script / Oracle / Lock Layer (3 agents, parallel)

**Goal:** Fix the foundational script, oracle, and lock-tier layers. Each agent owns separate file domains.

**Agent A — Script/Burn** (`src/script/`, `src/digidollar/scripts.cpp` only)
- Implement DD burn enforcement (P0.1)
- Add consensus-level detector for DD collateral vault spends
- Fix normal + ERR Taproot leaf ABI to match `OP_DIGIDOLLAR` / `OP_DDVERIFY` semantics
- Files: `src/script/interpreter.cpp`, `src/script/script.h`, `src/digidollar/scripts.cpp`

**Agent B — Oracle Core** (`src/oracle/`, `src/primitives/oracle*`, `src/kernel/chainparams.cpp`, `src/consensus/params.h`)
- Remove v0x01/v0x02 oracle bundle acceptance, mining, and fallback paths
- Gate DD oracle behavior to DigiDollar deployment predicate (not stale phase heights)
- Implement expandable roster: variable-length signer encoding, deterministic activation rules
- Implement MuSig2 domain separation (network/genesis/deployment, epoch/height, price, timestamp, message type)
- Implement signer liveness / deterministic reselection rules
- Files: `src/oracle/*`, `src/primitives/oracle*`, `src/kernel/chainparams.cpp`, `src/consensus/params.h`

**Agent C — Lock Tiers + Volatility + Address Routing** (`src/consensus/digidollar.cpp/h`, specific validation.cpp sections, `src/digidollar/address.cpp`)
- Canonicalize lock tiers: reject all non-canonical durations (P0.4)
- Fix volatility candidate precheck: non-mutating freeze check before state mutation (P0.8)
- Fix `CDigiDollarAddress` network routing: validate version byte against active chain params, reject cross-chain addresses (TR-RH-006)
- Rename `AlertThresholds::MAX_DD_SUPPLY` → `AlertThresholds::ALERT_DD_SUPPLY` in `health.h` and all callers — clarify it is a monitoring alert threshold, not a supply cap (P0.6 docs-only resolution)
- Files: `src/consensus/digidollar.cpp`, `src/consensus/digidollar.h`, `src/digidollar/txbuilder.cpp` (lock-tier input validation only), `src/digidollar/address.cpp` or equivalent, `src/digidollar/health.h`, `src/digidollar/health.cpp` (rename only)
- validation.cpp scope: lock-tier validation section, volatility section only — do not touch mint/redemption/ERR sections; no supply cap check needed (no global cap)

**Gate after Wave 2:**
- All Wave 1 failing tests now pass (burn, lock tiers, address routing, oracle, alert rename)
- `make check` — zero unit test regressions vs Wave 1 baseline
- `test/functional/test_runner.py` — zero functional test regressions
- Fuzz suite — zero new crashes or errors
- All changes committed in clean, logical commits; working tree clean
- Each commit subject line explains what was broken and what was fixed

---

### Wave 3 — Health / ERR Core (3 agents, parallel)

**Goal:** Replace doubles with integer math, implement canonical health, finish ERR consensus. Each agent owns distinct files.

**Agent A — Canonical Health + DCA Math** (`src/digidollar/health.cpp/h`, `src/consensus/dca.cpp`)
- Implement one canonical health calculation for consensus (P0.3)
- Replace DCA consensus doubles with integer basis points
- Conservative rounding up for collateral requirements
- `__int128` + `MAX_MONEY` checks before casting
- No fallback to 150% after activation when data is missing — fail closed
- Files: `src/digidollar/health.cpp`, `src/digidollar/health.h`, `src/consensus/dca.cpp`

**Agent B — ERR Consensus** (`src/consensus/err.cpp/h`)
- Finish ERR validation logic (P0.2 part 1)
- Replace ERR consensus doubles with integer basis points
- Implement `ceil(originalDD * 10000 / errRatioBps)` using `__int128`
- Gate: health `≥ 100%` = normal burn, health `< 100%` = ERR burn
- Block new minting while ERR active
- Files: `src/consensus/err.cpp`, `src/consensus/err.h`

**Agent C — Oracle Roster + Miner Gating** (`src/oracle/musig2*`, `src/node/miner.cpp`)
- Wire expandable roster into MuSig2 verification path
- Enforce that miner only includes/mines valid MuSig2 bundles for DD-touching blocks
- Non-DD blocks: no oracle bundle required
- Files: `src/oracle/musig2*.cpp`, `src/oracle/musig2*.h`, `src/node/miner.cpp` (oracle bundle gating only)

**Gate after Wave 3:**
- Wave 1 Agent C failing tests now pass (ERR, health, DCA, volatility)
- Oracle roster unit tests pass (expandable roster, quorum, domain separation)
- `make check` — zero unit test regressions vs Wave 2 baseline
- `test/functional/test_runner.py` — zero functional test regressions
- Fuzz suite — zero new crashes or errors
- All changes committed in clean, logical commits; working tree clean
- Each commit subject line explains what was broken and what was fixed

---

### Wave 4 — Integration Wiring (3 agents, parallel)

**Goal:** Wire the fixed subsystems (health, ERR, oracle) into validation, txbuilder, and RPC. Each agent owns separate call sites in shared files.

**Agent A — Mint path wiring** (validation.cpp mint sections, txbuilder.cpp mint/collateral)
- Wire canonical health + DCA into DD mint validation (P0.3 wiring)
- Wire canonical health into txbuilder collateral requirement calculations
- Remove all `skipOracleValidation` bypasses in mint and DCA paths
- Ensure mint rejects without oracle data post-activation (fail closed)
- Files: `src/digidollar/validation.cpp` (mint + DCA sections), `src/digidollar/txbuilder.cpp` (mint/collateral sections)

**Agent B — Redemption/ERR wiring** (validation.cpp redemption sections, txbuilder.cpp redemption, rpc/digidollar.cpp ERR status)
- Wire ERR into redemption validation path (P0.2 wiring)
- Wire burn enforcement into redemption path (must call P0.1 collateral detector)
- Update txbuilder redemption to select correct DD burn amount based on ERR state
- RPC: report ERR state and required DD burn amount correctly
- Files: `src/digidollar/validation.cpp` (redemption + ERR sections), `src/digidollar/txbuilder.cpp` (redemption sections), `src/rpc/digidollar.cpp` (ERR status RPCs)

**Agent C — Wallet + Qt + RPC unit fixes + mempool oracle policy**
- Move HD key helper to shared wallet code; both Qt and RPC mint derive owner key from HD seed (P0.7 / DD-RH-034)
- Persist derived owner key to wallet DB before transaction broadcast
- Non-HD wallet returns explicit error at mint time
- Wire wallet normal redemption and ERR redemption with correct DD input selection (P1.1)
- Enforce watch-only wallet cannot mint/send/redeem/sign
- Legacy wallet path fails with explicit unsupported error
- Fix Qt collateral ratios for tiers 5–8 to read from consensus params (TR-RH-004)
- Fix `listdigidollarpositions min_amount` to filter in DD cents (TR-RH-003)
- Fix `senddigidollar change_amount` unit (TR-RH-003)
- Fix `getdigidollarbalance minconf` application (TR-RH-003)
- Fix `listdigidollaraddresses` to list all generated DD addresses including zero-balance (TR-RH-005)
- Confirm `importdigidollaraddress` explicit failure message is clear and release-note worthy (ARCH-RH-003)
- Mempool oracle policy: require recent valid MuSig2 oracle quote before accepting DD txs
- Files: `src/qt/walletmodel.cpp`, `src/wallet/` (shared HD helper), `src/rpc/digidollar.cpp` (RPC unit fixes, oracle policy), wallet DD paths, `src/validation.cpp` (mempool DD oracle gating), `src/net_processing.cpp` (mempool policy only)

**Gate after Wave 4:**
- All Wave 1 failing tests now pass across all three agent domains
- RPC, wallet, and Qt state visibly consistent with consensus for normal redemption, ERR, watch-only, legacy wallet failure
- `make check` — zero unit test regressions vs Wave 3 baseline
- `test/functional/test_runner.py` — zero functional test regressions
- Fuzz suite — zero new crashes or errors
- All changes committed in clean, logical commits; working tree clean
- Each commit subject line explains what was broken and what was fixed

---

### Wave 5 — Mempool / IBD / Reorg Consensus Parity (3 agents, parallel)

**Goal:** Close all chain-split-shaped audit findings. Prove identical behavior across all validation contexts.

**Agent A — Mempool/ConnectBlock parity** (validation.cpp connect path, miner.cpp)
- Ensure `AcceptToMemoryPool` + `ConnectBlock` + miner block assembly apply identical DD consensus rules (P0.7)
- Fix any remaining `skipOracleValidation` bypasses in post-activation paths
- Historical validation must use oracle bundle/roster rules committed at that historical height
- Files: `src/validation.cpp` (ConnectBlock + AcceptToMemoryPool sections), `src/node/miner.cpp` (final oracle validation check)

**Agent B — IBD / reindex / reorg rollback** (validation.cpp disconnect path, net_processing.cpp)
- Ensure reindex, IBD, and reorg rollback restore DD supply, collateral, health, oracle cache deterministically (P0.7)
- Expanded oracle roster validates only after activation height; does not invalidate older blocks
- Files: `src/validation.cpp` (DisconnectBlock + IBD path), `src/net_processing.cpp` (reorg chain state)

**Agent C — Functional test coverage** (test/functional/ only)
- Write/run functional test scenarios:
  - Caught-up node accepts same DD block as IBD/reindex node
  - Mempool + mined block validity agree on oracle, DCA, ERR, burn requirements
  - Reorg across DD mint/transfer/redeem restores supply/collateral/health and wallet state exactly
  - Expanded roster validates only after activation height
- Files: `test/functional/digidollar_ibdreorg_tests.py` (new), `test/functional/digidollar_consensus_parity_tests.py` (new), existing functional tests

**Gate after Wave 5:**
- All P0.9 consensus parity scenarios covered and passing (mempool/ConnectBlock/IBD/reindex/reorg)
- New functional tests pass: caught-up node and IBD node agree; reorg restores all DD state exactly; roster activation height enforced
- `make check` — zero unit test regressions vs Wave 4 baseline
- `test/functional/test_runner.py` — zero functional test regressions (full suite, not just new tests)
- Fuzz suite — zero new crashes or errors
- All changes committed in clean, logical commits; working tree clean
- Each commit subject line explains what was broken and what was fixed

---

### Wave 6 — Final Regression Gates + Release Prep (3 agents, parallel)

**Goal:** Green on everything. Then docs + RC cut.

**Agent A — Full unit test suite**
- Run `make check` (all unit tests)
- Fix any remaining unit test failures
- Target: zero failures
- Files: targeted fixes only, no new features

**Agent B — Full functional test suite**
- Run full `test/functional/` suite
- Fix any remaining functional test failures
- Target: zero failures
- Files: targeted fixes only, no new features

**Agent C — Full fuzz suite**
- Run all 208 registered fuzz targets per `doc/FUZZ_MARATHON_COMPLETE.md`
- Fix any remaining fuzz failures or OOM issues (use per-target RSS config, not global)
- Target: zero crashes/errors

**After all three agents green:**
- Update `ARCHITECTURE.md`, `DIGIDOLLAR_ARCHITECTURE.md`, `DIGIDOLLAR_ORACLE_ARCHITECTURE.md`
- Update `DIGIDOLLAR_EXPLAINER.md`, `REPO_MAP_DIGIDOLLAR.md`
- Remove stale claims that ERR is incomplete or disabled
- Write release notes for next RC
- Cut RC tag locally. Jared reviews and pushes.

**Gate after Wave 6:**
- `make check` — zero failures across all unit tests
- `test/functional/test_runner.py` — zero failures across all functional tests
- All 208 fuzz targets — zero crashes or errors
- All fixes committed in clean, logical commits with clear commit messages; working tree clean except doc/release artifacts
- Doc artifacts (updated architecture docs, release notes) committed separately from code fixes
- RC tag created locally; Jared reviews and pushes

---

## 7. Wave Summary

| Phase | Agents | Focus | Files Touched |
|-------|--------|-------|---------------|
| Pre-Wave | 1 | Verify Red Hornet team commits + full test suite baseline | Read-only — git log verification + test runner only |
| Wave 1 | 3 (parallel) | Write all failing tests (red phase, no prod code) | `src/test/`, `test/functional/` only |
| Wave 2 | 3 (parallel) | Script/oracle/lock layer + address routing + alert rename (no supply cap) | `src/script/*`, `src/oracle/*`, `src/primitives/oracle*`, `src/kernel/chainparams.cpp`, `src/consensus/digidollar.*`, `src/digidollar/scripts.cpp`, `src/digidollar/address.cpp`, `src/digidollar/txbuilder.cpp` (lock input only), `src/digidollar/validation.cpp` (lock/volatility sections), `src/digidollar/health.h/cpp` (alert rename only) |
| Wave 3 | 3 (parallel) | Health/ERR core + oracle roster wiring | `src/digidollar/health.*`, `src/consensus/dca.cpp`, `src/consensus/err.*`, `src/oracle/musig2*`, `src/node/miner.cpp` (oracle gating) |
| Wave 4 | 3 (parallel) | Integration wiring — mint path, redemption/ERR path, wallet/Qt/RPC | `src/digidollar/validation.cpp` (by section), `src/digidollar/txbuilder.cpp` (by section), `src/rpc/digidollar.cpp`, `src/qt/walletmodel.cpp`, `src/wallet/` (HD helper), `src/validation.cpp` (mempool policy), `src/net_processing.cpp` (mempool) |
| Wave 5 | 3 (parallel) | Mempool/IBD/reorg consensus parity + functional tests | `src/validation.cpp` (connect/disconnect/IBD), `src/node/miner.cpp`, `src/net_processing.cpp`, `test/functional/` |
| Wave 6 | 3 (parallel) | Full regression gates (unit + functional + fuzz) + docs + RC | Targeted fixes only; docs; release notes |

**Total:** 7 phases (1 pre-wave + 6 waves)  
**Sub-agent executions:** 19 (1 + 3×6)  
**After every wave — mandatory before the next wave starts:**
1. `make check` — zero unit test failures (no regressions)
2. `test/functional/test_runner.py` — zero functional test failures (no regressions)
3. Fuzz suite — zero new crashes or errors
4. All changes committed; working tree clean
5. Every commit subject line explains what was broken and what was fixed — plain English, ≤72 chars

---

## 8. Pre-Coding Checklist

Before Wave 2 coding begins, the following must be confirmed:

- [x] **DD-RH-119 supply cap decision:** LOCKED — Option B. No global DD supply cap. `MAX_DIGIDOLLAR` is per-output bound only. Docs updated. No consensus change.
- [ ] **Pre-Wave complete:** Red Hornet team commits verified in git log, full test suite passes clean.
- [ ] **Wave 1 complete:** all failing tests written, existing suite still green.

---

## 9. MVP Summary

**7 phases, 19 sub-agent executions, mandatory full-suite test gate after every wave.**

DigiDollar V1 ships when:
- All Red Hornet fixes committed by the Red Hornet team, verified by Pre-Wave baseline check
- Collateral spends cannot bypass DD burn (P0.1)
- ERR is fully implemented with integer math and no DGB haircut (P0.2 + P0.3)
- DCA/ERR health uses one canonical oracle-backed source (P0.3)
- Lock tiers are canonical only (P0.4)
- Oracle handling is MuSig2-only with expandable roster, DD-touching blocks require oracle data (P0.5)
- No global DD supply cap enforced in consensus; `MAX_DIGIDOLLAR` is per-output bound only; all supply-cap language removed from docs (P0.6)
- Qt mint owner keys derived from HD seed, persisted before broadcast (P0.7)
- Volatility precheck is non-mutating (P0.8)
- All validation contexts (mempool/IBD/reorg) are provably identical (P0.9)
- Wallet/RPC/Qt state is consistent with consensus including ERR, watch-only, legacy, and unit/filter correctness (P1.1)
- Cross-chain DD addresses rejected at validation (TR-RH-006)
- Full unit + functional + fuzz gate green, clean working tree, docs updated, RC cut for Jared review
