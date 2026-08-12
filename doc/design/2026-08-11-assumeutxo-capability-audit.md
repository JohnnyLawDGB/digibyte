# AssumeUTXO capability audit — DigiByte Core

**Audited tree:** `/home/polloloco/dgb-audit` (detached worktree of the DigiByte Core repo)
**Branch/ref:** `origin/develop` · **Commit:** `16159311b34449cd970871bcd1532561b0d92cdd`
**Declared version:** 9.26.5 (`configure.ac:2-7`)
**Closest upstream:** **Bitcoin Core v26.2** — rebase merges named `DGB v8.22 merge with BTC v26.2 Fixes Part 10/20/21`; upstream release notes present through `doc/release-notes/release-notes-26.1.md`; autotools (no `CMakeLists.txt`, i.e. pre-v29).
**Live nodes probed:** Adam VPS mainnet + JohnnyLaw oracle #8, both `/DigiByte:9.26.4/`, mainnet, unpruned.
**Audit date:** 2026-08-11 / 12 UTC. **Read-only:** no source modified, no state-changing RPC issued.

---

## 0. Source ≠ deployment: version equivalence proof

The audit reads `develop` (9.26.5); production runs 9.26.4. Before any finding transfers, the surface must be shown identical. `git diff --shortstat v9.26.4 HEAD -- <path>`:

| File | v9.26.4 → develop |
|---|---|
| `src/crypto/muhash.{h,cpp}` | identical |
| `src/node/utxo_snapshot.{h,cpp}` | identical |
| `src/index/coinstatsindex.cpp` | identical |
| `src/coins.h` | identical |
| `src/rpc/blockchain.cpp` | identical |
| `src/validation.{h,cpp}` | identical |
| `src/node/chainstate.cpp` | identical |
| `src/rpc/txoutproof.cpp` | identical |
| `src/kernel/chainparams.cpp` | 69+/76− — **no `assumeutxo` lines in the diff** (BIP90 burial work) |
| `src/oracle/bundle_manager.cpp` | 36+/11− — see below |

`bundle_manager.cpp` is the only audited file with real changes, and it is where the §5.2 blocker lives. Verified directly against the deployed tag: `git show v9.26.4:src/oracle/bundle_manager.cpp` contains the **same** `ORACLE_VALIDITY_BLOCKS = 20` / `VOLATILITY_HISTORY_BLOCKS = 30*24*60*4` constants and the **same** single fail-closed `restart with -reindex` branch. The PR #429 hunks change activation-gate evaluation, not scan depth or the failure path.

**Every finding below applies to the deployed 9.26.4 fleet, not merely to `develop`.**

---

## 1. Headline

**The backport set for both Sequence 0 goals is empty**, and this is now confirmed at runtime rather than inferred from source. The audit did not find a porting problem. It found (a) an **integration blocker specific to DigiDollar** (§5.2) that gates snapshot *consumption*, and (b) a **cost problem for publication** (§3a) caused by an index that is not enabled in production.

---

## 2. Capability matrix

Status column: **S** = confirmed in source, **L** = confirmed live on the 9.26.4 fleet.

| # | Primitive | Status | Evidence | Backport | Notes |
|---|---|---|---|---|---|
| 1a | MuHash | present **S** | `src/crypto/muhash.{h,cpp}`; `MuHash3072` | none | Diff vs upstream v26.2 = **0 non-cosmetic lines** (§0b) |
| 1b | `gettxoutsetinfo hash_type` | present **S+L** | `src/rpc/blockchain.cpp:878` `ParseHashType`, `:880-887`; live `help gettxoutsetinfo` → `'hash_serialized_3' \| 'muhash' \| 'none'`, plus `hash_or_height` and `use_index` args | none | Muhash queryable per-height; `hash_serialized_3` rejected for a specific block (`:1001`) |
| 1c | Coin stats index | code present **S** / **NOT ENABLED L** | `src/index/coinstatsindex.{h,cpp}`; `-coinstatsindex` `src/init.cpp:484`. Live `getindexinfo` on **both** nodes returns only `txindex` + `basic block filter index` | none | **Cost blocker for publication — see §3a** |
| 2a | `dumptxoutset` | present **S+L** | `src/rpc/blockchain.cpp:2652`, registered `:3007`; live `help` lists `dumptxoutset "path"` | none | No DigiByte-specific tokens in body/help |
| 2b | `loadtxoutset` | present **S+L** | `src/rpc/blockchain.cpp:2780`, registered `:3008`; live help returns the full upstream description incl. background-validation semantics | none | **Task expected absent; present and real** |
| 2c | `SnapshotMetadata` | present **S** | `src/node/utxo_snapshot.h:23`, `:41` `READWRITE(obj.m_base_blockhash, obj.m_coins_count)` | none | 2-field v26 form — no base height, no network magic (§3b) |
| 2d | Chainparams hooks | present **S** | `src/kernel/chainparams.h:40,49,131` (`AssumeutxoHash`, `AssumeutxoData`, `AssumeutxoForHeight`) | none | Hook exists; data is separate — 2e |
| 2e | Populated heights | **mainnet ABSENT S** | `src/kernel/chainparams.cpp:292` `m_assumeutxo_data.clear()` (CMainParams @ `:85`) | n/a | Testnet empty + TODO (`:583`); signet h=160'000 (`:997`) **suspect inherited** (§6); regtest h=110 (hash marked `TODO: Generate actual UTXO hash`) + h=299 (`:1190`) |
| 3 | Multi-chainstate machinery | present **S** | `validation.h:890,902,1071,1081,1094,1117,1261,1272`; impls `validation.cpp:6238,6390,6619`; `node/chainstate.cpp:227,232` | none | The set flagged "largest and most invasive" is **entirely present** |
| 3b | Tests | present, **PASSES** | `test/functional/feature_assumeutxo.py`, registered `test_runner.py:449`; `src/test/validation_chainstatemanager_tests.cpp` | none | Run on a pure `develop` build: exit 0 (§3d). ⚠ Structurally cannot catch §5.2 (§3e) |
| 3c | Design doc | present **S** | `doc/design/assumeutxo.md` (175 lines) | none | Upstream doc, retained. States snapshots are "checked against a hash that's been **hardcoded in source code**" — the exact anchor this project replaces |
| 4a | Merkle proof RPCs | present **S+L** | `src/rpc/txoutproof.cpp:21,122`, registered `:173`; live `help` lists both | none | — |
| 4b | Headers + multi-algo | diverged **by design** | `validation.cpp:4563,4757` → `CheckProofOfWork(header.GetPoWAlgoHash(params), ...)`; `primitives/block.h:124,128` | n/a | §5.1 — affects external verifiers only |
| 5 | Coin serialization | upstream-identical **S** | `src/coins.h:31`; `code = nHeight*2 + fCoinBase`, `VARINT`, `TxOutCompression` | none | `dumptxoutset` determinism + muhash compatibility preserved |

### 0b. Upstream-identity check

`git log` on the snapshot surface shows only copyright bumps, the Bitcoin→DigiByte rename, and rebase fix batches — no feature-level local patches. Verified by direct diff against `raw.githubusercontent.com/bitcoin/bitcoin/v26.2`:

| File | Changed lines | Non-cosmetic |
|---|---|---|
| `src/crypto/muhash.h` | 13 | **0** |
| `src/node/utxo_snapshot.h` | 10 | **0** |

Sole real local deviation, cosmetic: `src/node/chainstate.cpp:226,228` add non-upstream `LogPrintf` debug lines around `DetectSnapshotChainstate`.

---

## 3. Sequence 0 scope

### (a) Oracle attestation **publication** — needs #1 only

**Backport required: none. But it is not free in production today.**

`getindexinfo` on both mainnet nodes returns only `txindex` and `basic block filter index`. **`-coinstatsindex` is not enabled anywhere in the fleet.** Consequences:

- `gettxoutsetinfo muhash` currently performs a **full UTXO-set scan holding `cs_main`**, not an index lookup. **Measured: 93 s** on the live Adam VPS node (§3c). Every attestation would cost that stall on whatever node produces it.
- Enabling `-coinstatsindex` requires a restart, and on the Adam VPS a restart costs **~2.5 h of oracle-dark time** (independent, previously measured). The index also has to build from genesis across ~24.0 M blocks — cost unmeasured.
- So the cheap-attestation story assumed by the design **does not hold on current infrastructure**, and the fix is not free either.

Recommendation: attest from a **dedicated non-oracle node** with `-coinstatsindex` enabled. Publication then costs an index lookup and never touches a node in consensus. This should be treated as an infrastructure prerequisite of Sequence 0, not an afterthought.

Design notes that survive the above:
- **Attest the muhash, not `hash_serialized_3`** — the latter cannot be queried for a specific block (`blockchain.cpp:1001-1002`) and is not index-backed.
- Coin serialization is upstream-identical, so a given UTXO set's muhash is reproducible across implementations with no DigiByte-specific rules.
- Publication is independent of every §5 blocker and can ship long before consumption.

### (b) Snapshot **consumption** — needs #1 + #2 + #3

**Backport required: none.** All three groups present; `loadtxoutset` confirmed live.

Blocked on DigiDollar integration, in severity order:

1. **`LoadPricesFromChain` fail-closed (§5.2) — hard blocker on mainnet, present in deployed 9.26.4.**
2. **`m_assumeutxo_data` empty on mainnet** (`chainparams.cpp:292`). Arguably correct for this project: the hook must be **re-pointed at attestation lookup**, not populated. `AssumeutxoForHeight` (`chainparams.h:131`) is the single chokepoint.
3. **`SnapshotMetadata` is the 2-field v26 form** (`utxo_snapshot.h:41`) — base blockhash + coin count only. Binding height or network requires a serialization change with file-format implications. **Decide before Set 2 freezes the format.**

---

### (c) Measured chain reality — snapshot sizing

`gettxoutsetinfo muhash` run live on the Adam VPS mainnet node (9.26.4), 2026-08-12, with explicit go-ahead to stall a consensus oracle:

| Metric | Value |
|---|---|
| Height | 24,016,523 |
| Best block | `00000000000000020534f10a8263698e7a78255e466a7d82b2eb4dd1dcd6dfad` |
| **UTXOs (`txouts`)** | **15,864,852** |
| **muhash** | `72e1abfeb7c45e48142c2dc0749e5d2f37575c8dcf379d8689632189780c372f` |
| `bogosize` | 1,186,843,502 (~1.19 GB) |
| `disk_size` (chainstate) | 850,007,445 (~850 MB) |
| Txs with unspent outputs | 4,357,881 |
| Total amount | 18,422,744,829.085 DGB |
| **Scan wall-clock** | **93 s**, holding `cs_main` |

**Snapshot file estimate: ~0.95 GB.** 15,864,852 coins × ~60 B/coin (the rate measured on Bitcoin's own `dumptxoutset`, ~176.9 M coins → ~10.6 GB). Cross-checked by `bogosize` 1.19 GB as a loose upper bound.

**This is the single most encouraging number in the audit.** DigiByte's UTXO set is ~11× smaller than Bitcoin's, so a snapshot is **under a gigabyte** — plain HTTP distribution is viable, no torrent infrastructure required, and the download is a rounding error against the 38 GB of block data it replaces.

Node health after the scan: tip advanced 24,016,523 → 24,016,529 during/after the call, and `listoracle` confirmed oracle #9 still `running: true`, `authorized: true` with a fresh price. No lasting effect — but see the cost note in §3a.

---

### (d) Behavioural confirmation — `feature_assumeutxo.py` passes

Built pure `develop` (`16159311b3`, `--with-gui=no`) and ran the suite: **`Tests successful`, exit 0.** The "complete" claims in §2 are now behavioural, not merely structural. What it exercised end-to-end:

- snapshot creation (`dumptxoutset` at height 299) and load into a second node
- rejection of malformed snapshots: unknown base block, wrong coin count (±1), altered UTXO data
- rejection of an invalid snapshot chainstate already in the datadir
- **restart before background validation completes**, then resume
- background validation to completion, then `CheckBlockIndex`/`LoadBlockIndex` on restart
- all indexes + `-reindex` and `-reindex-chainstate` of an assumeutxo-synced node

### ⚠ (e) The test suite structurally cannot catch §5.2

`feature_assumeutxo.py` explicitly restarts a node mid-validation — *"Restarted node before snapshot validation completed, reloading…"* — which is precisely the scenario that bricks a mainnet node under §5.2. It passes, and the reason is that **the guard cannot fire on regtest.**

The fail-closed branch is `if (dd_floor > 0 && height >= dd_floor)` (`oracle/bundle_manager.cpp:1848`), and `EarliestActivationFloor` (`consensus/digidollar.cpp:17-29`) documents its own result in-comment: *"default regtest min(650, 0) = 0"*. With `dd_floor == 0` the branch is unreachable, so every restart path in the suite runs with the check disabled.

**Consequences:**
- §5.2 is **mainnet/testnet-only by construction** — no amount of running the existing suite would ever have surfaced it, which explains why it shipped.
- A regression test for §5.2 must force `dd_floor > 0` on regtest via **`-digidollaractivationheight=N`** (N > 0), then restart a snapshot-booted node with an incomplete trailing window. That is the missing test, and it is cheap to write.

---

## 4. Task expectation vs. finding

| Expectation | Finding |
|---|---|
| `loadtxoutset` "absence is likely" | **Present in source and live** |
| Background validation is the big port | **Fully present**, zero port |
| Timeline gates on backport effort | Backport ≈ 0; gates on §5.2 and the §3a index gap |

---

## 5. DigiByte-specific divergence risks

### 5.1 Multi-algo headers — affects external verifiers only

`CheckProofOfWork(header.GetPoWAlgoHash(consensusParams), header.nBits, consensusParams)` (`validation.cpp:4563`, `:4757`), algo carried in the header (`primitives/block.h:124`).

A verifier checking attestation transactions by merkle proof against headers alone must implement DigiByte's five-algo PoW hashing and DigiShield retargeting. **A stock Bitcoin SPV/header client cannot validate a DigiByte header chain.** In-node verification unaffected; `gettxoutproof`/`verifytxoutproof` are unmodified and live.

### 5.2 ⚠ Consensus state outside the UTXO set — the blocker

`OracleBundleManager::LoadPricesFromChain` (`src/oracle/bundle_manager.cpp:1786`) rebuilds the oracle price cache **and volatility state** at every startup from raw blocks:

- Scan depth `min(max(ORACLE_VALIDITY_BLOCKS, VOLATILITY_HISTORY_BLOCKS), tip_height)` (`:1835`)
- `ORACLE_VALIDITY_BLOCKS = 20` (`:1818`); `VOLATILITY_HISTORY_BLOCKS = 30*24*60*4` = **172,800 blocks** ≈ 30 days at 15 s (`:1819`)
- Each height read via `ReadBlockFromDisk` (`:1847`); on failure at or above the floor it **returns false and the node does not start** (`:1848-1858`)
- `dd_floor = DigiDollar::EarliestActivationFloor(consensus)` = **23,627,520** mainnet

The comment at `:1859-1861` names "assumeutxo gaps" — but only as a *below-floor* case, skipped as legitimate pruning.

**✅ EMPIRICALLY REPRODUCED 2026-08-12.** No longer inferred from code. Regression test added at `test/functional/feature_digidollar_snapshot_startup.py`: it forces `dd_floor = 250` via `-digidollaractivationheight`, snapshots at height 299, loads it into a node that has headers but no blocks beneath the base, and restarts. Result:

```
AssertionError: snapshot-bootstrapped node failed to restart while DigiDollar is
active … [node 1] digibyted exited with status 1 during initialization.
Error: DigiDollar-era block data is incomplete or unreadable. Restart with
-reindex to rebuild it (a pruned node will redownload and re-prune).
```

The test asserts the **post-fix** behaviour (node restarts successfully), so it is RED today and turns GREEN when the blocker is resolved.

**Two fail-closed sites, not one.** The startup path has *two* chain reconstructions, both wired to `InitError` with the same message:

| Site | Call |
|---|---|
| `src/init.cpp:2229` | `OracleBundleManager::LoadPricesFromChain(chainman)` |
| `src/init.cpp:2238` | `DigiDollar::SystemHealthMonitor::ReconstructFromChain(chainman)` |

The identical error string means the message alone cannot tell you which fired. `ReconstructFromChain` rebuilds DD supply/collateral **from the UTXO set** (which a snapshot supplies) rather than from blocks, so it may well be satisfied — but **any fix must clear both gates**, and whichever is addressed first will simply expose the other. Determine which one actually fires before designing the fix. A test hook or distinct error strings would be a cheap upstream improvement in their own right.

**Method note:** the test only works with `setmocktime` pinning block times (`feature_assumeutxo.py:140`, *"Mock time for a deterministic chain"*). Without it, block hashes vary per run and never match the regtest `m_assumeutxo_data` entry — three runs produced three different height-299 hashes before this was spotted.

**Consequence, with live numbers.** Mainnet tip is **24,016,497** (Adam VPS, live). A snapshot at tip has a scan window of ~23,843,697 → tip, **entirely above `dd_floor`**. A node bootstrapped from it has no block data there, hits the fail-closed branch, and **refuses to start**. True for every height above ~23,800,320 (`dd_floor` + 172,800) — i.e. for the last ~5 weeks and permanently from here on.

Candidate resolutions for the spec:

- **(A) Ship the trailing window** — distribute the last 172,800 blocks with the snapshot. No consensus change; inflates download and partly defeats the purpose.
- **(B) Attest the derived state** — carry the price cache + volatility state in the attestation and load instead of rescanning. Aligns with the project thesis; widens the attestation format and its trust surface.
- **(C) Defer the gate for snapshot chainstates** — allow deferral while background sync is in progress. Smallest format impact; leaves a window where DD consensus state is incomplete.

DD transactions additionally require a recent valid MuSig2 oracle quote at mempool acceptance, so a node in state (C) would not process DD activity until the window fills regardless.

`DigiDollarStatsIndex` (`src/index/digidollarstatsindex.h:47`, a `BaseIndex`) is **not** a blocker — indexes rebuild independently.

---

## 6. Landmines

- **⚠ CONFIRMED: chainparams carries Bitcoin assumeutxo data.** `src/node/blockstorage.cpp:520` is a **DigiByte-local patch** whose error string reads: *"This is a DigiByte-specific issue where the snapshot was created with DigiByte blocks but chainparams has Bitcoin assumeutxo data."* Someone has already hit this and left the diagnosis in-tree. This upgrades the signet entry (h=160'000, hash `fe0a4430…928a`, `chainparams.cpp:997-1003`, on a chain with its own genesis at `:992`) from "suspect" to **almost certainly inherited Bitcoin values**, and it means the failure mode is a `fatalError()` at startup, not a graceful refusal.
- **Regtest h=110 entry carries `// TODO: Generate actual UTXO hash`** (`:1194`). `feature_assumeutxo.py` appears to use the h=299 entry (`:1200`), so it may be inert — but it ships.
- **`src/crypto/muhash.h`** comment URL mechanically rewritten `bitcoin-dev` → `digibyte-dev`, producing a dead link. Cosmetic, but shows the rename pass edited comment bodies.
- **Upstream PR numbers deliberately omitted.** They cannot be established from this tree and are not cited from memory. Version-era attributions rest on in-tree release notes and rebase commit names only. Look them up before the maintainer pitch.

---

## 7. Open questions

| # | Question | Needs | Status |
|---|---|---|---|
| 1 | UTXO set size, muhash value, snapshot file size | — | **RESOLVED (§3c):** 15,864,852 UTXOs; muhash `72e1abfe…372f` @ h=24,016,523; **snapshot ≈ 0.95 GB**; scan cost 93 s |
| 1b | Actual `dumptxoutset` file size and write time | A node you're willing to stall again + ~1 GB free disk | Open — the ~0.95 GB figure is derived, not measured. Worth confirming before distribution planning hardens |
| 2 | Does `feature_assumeutxo.py` pass on this tree? | — | **RESOLVED (§3d): PASSES**, exit 0, on a pure `develop` build |
| 3 | Is the signet entry real or inherited Bitcoin data? | Upstream v26.2 chainparams diff | Open |
| 4 | ~~Is `-coinstatsindex` enabled?~~ | — | **RESOLVED: no, on neither node.** Cost of building it from genesis at 24.0 M blocks still unmeasured |
| 5 | Does dump→load at a DD-active height yield a node that validates DD txs? | Regtest with `-digidollaractivationheight` | Open — depends on §5.2 choice; testable without touching mainnet |

---

## 8. Appendix — the core-integration surface, sketched

Design sketch only. **No code was written or modified in this session.**

### The whole surface is four non-test call sites

```
src/validation.cpp:6412        PopulateAndValidateSnapshot   AssumeutxoForHeight(base_height)
src/validation.cpp:6694        MaybeCompleteSnapshotValidation AssumeutxoForHeight(curr_height)
src/rpc/blockchain.cpp:2834    loadtxoutset (pre-check)      AssumeutxoForBlockhash(base_blockhash)
src/node/blockstorage.cpp:518  BlockManager::LoadBlockIndex  AssumeutxoForBlockhash(*snapshot_blockhash)
```

Everything downstream — deserializing coins, staging the second chainstate, background validation, cleanup — never consults the anchor again. It only ever needs `{height, hash_serialized, nChainTx, blockhash}`.

### Constraint that shapes the design

`CChainParams` has **zero** access to chain state (`grep -c "ChainstateManager\|CCoinsView" src/kernel/chainparams.h` → `0`). It is a pure data holder. **The re-point therefore cannot live inside `AssumeutxoForHeight()`** — an on-chain attestation lookup has nothing to read from there.

The shape that fits: leave chainparams alone as the *static* provider, and introduce a resolver the call sites consult.

```cpp
// Conceptual — an anchor source, satisfied today by chainparams and
// tomorrow by oracle attestations.
struct SnapshotAnchorProvider {
    virtual std::optional<AssumeutxoData> ForHeight(int height) const = 0;
    virtual std::optional<AssumeutxoData> ForBlockhash(const uint256&) const = 0;
};
```

- **`ChainparamsAnchor`** — wraps today's `m_assumeutxo_data`. Preserves existing behaviour exactly; regtest/functional tests keep working unchanged.
- **`OracleAttestationAnchor`** — resolves from on-chain attestations, verifying the 7-of-35 MuSig2 threshold before returning. This is the new component, and it is the *only* genuinely new consensus-relevant code.

Three of the four call sites already have the context to reach a resolver: `validation.cpp:6412` and `:6694` are `ChainstateManager` members; `rpc/blockchain.cpp:2834` already holds `chainman`.

### The one hard call site

`src/node/blockstorage.cpp:518` runs inside `BlockManager::LoadBlockIndex()` — **at startup, before the chain is usable**. An anchor resolver that reads attestations *from the chain* cannot be consulted there: the chain isn't loaded yet. Chicken-and-egg.

Two workable escapes, both cheap:
- **Persist the resolved anchor** next to the snapshot chainstate directory when `loadtxoutset` first accepts it, and read that file here. The trust decision is made once, at load time, where the chain *is* available.
- **Thread it through** from the `loadtxoutset` invocation, since that path already knows the anchor it validated against.

Note this site only needs `au_data.height` and `nChainTx` (for progress estimation and `nChainTx` population) — not the hash. So the persisted record can be minimal.

### ⚠ Hash-type mismatch — decide before Set 2 freezes

Snapshot validation compares **`hash_serialized_3`**, not muhash:

```
src/validation.cpp:6533   ComputeUTXOStats(CoinStatsHashType::HASH_SERIALIZED, ...)
src/validation.cpp:6544   if (AssumeutxoHash{maybe_stats->hashSerialized} != au_data.hash_serialized)
src/validation.cpp:6733   same comparison again, in background validation
```

This cuts against §3a's recommendation to attest the muhash, and the trade is now explicit:

| Attest | Validation change | Publication cost |
|---|---|---|
| `hash_serialized_3` | **none** — comparison already matches | Cannot use coinstatsindex; cannot be queried for a specific block (`blockchain.cpp:1001`). Full scan, always |
| `muhash` | 2 extra edits (`:6533`/`:6544` and `:6733`) to compute and compare muhash | Index-backed once `-coinstatsindex` is enabled; queryable per height |

Attesting muhash is still probably right — it makes publication cheap and repeatable — but it is **not free**, and §3a understated it. Either way the decision belongs to Set 2, because it determines what the attestation carries.

### Honest size estimate

- Anchor resolver + chainparams-backed default: small, mechanical.
- Four call-site swaps: trivial, three of them one-liners.
- Persisted-anchor escape for `LoadBlockIndex`: small.
- Optional muhash switch: 3 lines.
- **`OracleAttestationAnchor` + the attestation format + publication tooling: this is the real project.**
- **§5.2 DigiDollar startup rescan: this is the real risk.**

Nothing here is a Bitcoin backport. All of it is DigiByte-side integration.

---

## 9. Sequence 0 architecture

Added 2026-08-12 after design discussion. Design only — no code written.

### 9.1 Trust invariant

> **The chain distributes trust. The host distributes bytes.**

The oracle-signed `{height, utxo_hash}` is read **from the chain**. The ~1 GB snapshot blob is fetched from any mirror and checked against that hash. A compromised, malicious, or simply wrong mirror can therefore only cause a **failed hash check** — a denial of service — never acceptance of a forged UTXO set.

Two consequences worth stating explicitly in the spec, because they are the strongest arguments in the maintainer pitch:
- Mirrors need no trust, no vetting, and no coordination. Anyone may mirror.
- The blob is independently reproducible: `Coin` serialization is deterministic and upstream-identical (`src/coins.h:31`), so any third party can run `dumptxoutset` at height H and confirm the published file matches what the oracles signed.

**Anti-pattern to avoid:** publishing the *attestation* to GitHub and having the node read it there. That would swap a hash hardcoded at release time for a hash on a mutable third-party service — strictly worse than the status quo, and the first thing a reviewer will attack.

### 9.1b The trust root already ships in the binary

`consensus.vOraclePublicKeys` is a compiled-in table of 35 x-only pubkeys (`kernel/chainparams.cpp:350-351`; declared at `consensus/params.h:212` as *"Hardcoded oracle public keys"*), with `nOraclePubkeyCount = 35` and `nOracleConsensusRequired = 7`. **A Core binary already contains everything needed to verify an attestation** — it reads the signed payload from the chain and checks it against keys it shipped with. No external input, no network trust.

**This is the central argument of the proposal:**

> Upstream assumeutxo hardcodes **the answer**. This design hardcodes **the verifier**.

| | Upstream assumeutxo | Oracle-attested |
|---|---|---|
| Compiled into the binary | a specific hash for a specific height | 35 oracle pubkeys + a 7-of-35 threshold |
| Covers | only heights the release knew about | **any height, indefinitely** |
| New snapshot requires | a new Core release | nothing |
| Staleness | immediate | none |

A hardcoded hash is a *value* and is stale on arrival — which is precisely why `m_assumeutxo_data` is empty on mainnet (§2e) and why upstream's tables carry so few entries. Hardcoded pubkeys are a *capability*: any attestation those keys sign, at any future height, validates against a binary shipped today. **One release supports every future snapshot.**

**No new trust is introduced.** A node running DigiByte Core with DigiDollar already trusts this exact 7-of-35 quorum for consensus-critical price data — mint and redeem validation depend on it. Attesting a UTXO hash is a *weaker* ask than what the node already accepts, and unlike the price data it is **temporary**, because background validation re-derives the hash from genesis and compares (`validation.cpp:6733`). To the reviewer question *"why should users trust these oracles?"*: they already do, for something more consequential.

**Roster rotation — design for it now.** The roster is compiled in, and `chainparams.cpp:341-346` documents rotation as *"deploy via coordinated software upgrade."* An older binary therefore cannot verify attestations signed by a rotated roster. That must **not** be a hard failure: the node should detect that it does not recognise the signing set and **fall back to conventional full sync**, telling the user why. Old releases then degrade gracefully to today's behaviour instead of breaking. Cheap to specify now, painful to retrofit.

### 9.1c Locked design decisions

Resolved 2026-08-12 following the adversarial review (`2026-08-12-attested-snapshot-threat-model.md`). These feed directly into Set 2.

#### D1 · Threshold: **18-of-35** for snapshot attestations

Deliberately **distinct from the 7-of-35 used for price data**, because the blast radius is not comparable: a distorted price is bounded and self-correcting, a forged UTXO set is unbounded until background validation completes days later (review H-1).

18 is a **majority of 35**, and that is not a round-number aesthetic — it buys a structural property:

> **Two contradictory attestations for the same height cannot both exist without provable double-signing.** Two disjoint 18-signer sets would require 36 signatures from 35 keys. Any conflicting pair therefore shares at least one signer, and that oracle has demonstrably signed two different UTXO hashes for one height — attributable, detectable misbehaviour that identifies exactly which key to remove.

Below a majority this fails: an attacker holding 10 keys could mint a competing attestation alongside the honest one, and the node would face genuine ambiguity with no way to attribute fault. At 18 the ambiguity is cryptographically impossible and any attempt is self-incriminating.

**Liveness cost is near zero.** The constraint that normally caps thresholds is missing an epoch; snapshot attestations are quarterly with days of slack. 18 of 35 leaves 17 oracles' worth of margin for absent, broken, or upgrading nodes. If collection is slow, wait — nothing is time-critical.

*Document the reasoning alongside the number.* "Why 18 and not 7?" is the first question a reviewer will ask, and the double-signing property is the answer.

#### D2 · Attestation payload — the signed tuple

The signature MUST cover the whole tuple, not the hash alone (review H-2). This is **parity with what already exists**: `AssumeutxoData` (`kernel/chainparams.h:49`) already binds `{height, hash_serialized, nChainTx, blockhash}` as one unit, and that binding is what makes the current mechanism safe. Moving the anchor on-chain must not lose it.

| Field | Purpose |
|---|---|
| `network_magic` | Cheap belt-and-braces; mainnet/testnet oracle keys already differ, but costs nothing |
| `height` | Parity with `AssumeutxoData::height` |
| `blockhash` | Parity; **node MUST verify this block sits at `height` in its own header chain** — defeats fork replay |
| `utxo_hash` | The attested value |
| `hash_type` | **`muhash`** (decided 2026-08-12) — carried explicitly so a later change is unambiguous |
| `nChainTx` | Parity with `AssumeutxoData::nChainTx` |
| `coins_count` | Cross-check against file metadata **before** parsing (review H-3) |
| `file_size` | Download cap (review H-3) |

#### D2a · Attested hash type: **muhash** — and it is not free

Chosen for publication economics: muhash is index-backed via `-coinstatsindex` and queryable at a specific height, whereas `hash_serialized_3` is neither (`blockchain.cpp:1001-1002` rejects it for a specific block). With the index enabled, each oracle's independent computation (D3) becomes a lookup instead of a 93 s scan — which is what makes an 18-signer ceremony practical at all.

**Required consequence:** snapshot validation currently compares `hash_serialized_3`, so choosing muhash means changing the comparison in three places:

| Site | Current |
|---|---|
| `validation.cpp:6533` | `ComputeUTXOStats(CoinStatsHashType::HASH_SERIALIZED, ...)` — snapshot load |
| `validation.cpp:6544` | `if (AssumeutxoHash{maybe_stats->hashSerialized} != au_data.hash_serialized)` |
| `validation.cpp:6733` | same comparison again, in background validation |

All three must move to `CoinStatsHashType::MUHASH` and the corresponding stats field. Small and mechanical, but it is a change to the **security-critical comparison path**, so it wants its own review and its own test rather than riding along with the resolver work.

#### D3 · Oracles compute independently — MUST

Each participating oracle derives `utxo_hash` **from its own chainstate** at the target height and signs only a value it computed itself. An oracle that cannot compute it **abstains**; it never signs a value supplied by a coordinator or generation node (review C-1 — the critical finding).

Workload per oracle, per snapshot: one UTXO-set hash computation at a deterministic height. **Measured 93 s** as a full scan (§3c), or an index lookup with `-coinstatsindex` — the strongest practical reason to enable that index fleet-wide before this ships. Determinism guarantees honest oracles agree (`src/coins.h:31`).

#### D4 · The attestation is also a moving header-chain checkpoint — first-class goal

Not a side effect; a stated design objective. Mainnet ships `nMinimumChainWork = 0x00` (`chainparams.cpp:197`) with its highest checkpoint at 23,500,000 — roughly 516 K blocks with no compiled anchor above them, which is precisely where attestations live.

Because the attestation binds `{height → blockhash}` under an 18-of-35 majority, a node may **reject any header chain that lacks that blockhash at that height**. That is a checkpoint refreshed quarterly, requiring no software release — strictly stronger than compiled checkpoints for every height above 23.5 M.

**This makes the network more eclipse-resistant than it is today, for every node, whether or not it ever loads a snapshot.** It is the strongest single argument in the maintainer pitch: the proposal is not asking for a trust concession in exchange for speed, it is closing an existing gap.

#### D5 · Verify before parse

The node MUST, before entering the coin-read loop: compare file metadata against the attestation (`coins_count`, `blockhash`), cap the transfer at the attested `file_size`, and confirm free disk. `m_coins_count` is attacker-supplied and uncapped in the current code (`validation.cpp:6432`), and the hash is only checkable after the entire file is consumed (review H-3). Any mismatch means *this mirror is bad* → rotate; never *this snapshot is bad*.

#### D6 · Trailing window over P2P

Resolution (A′) (§9.4), with (C) retained as a safety net so a mid-bootstrap restart degrades gracefully instead of producing an unstartable node (review M-4).

#### D7 · Attested checkpoints are enforced **during IBD only**

The moving checkpoint (D4) would feed the same machinery DigiByte already uses for compiled checkpoints — `validation.cpp:4844-4849`, *"Don't accept any forks from the main chain prior to last checkpoint"*, rejecting with `bad-fork-prior-to-checkpoint`. That is a consensus-relevant rule, so its **scope** matters as much as its existence.

**Rule:** enforce attested checkpoints while `ChainstateManager::IsInitialBlockDownload()` (`validation.h:1126`) is true — i.e. against chain a node is adopting for the first time. Once a node has independently validated past an attested height, the attestation is **advisory**: a mismatch raises a loud alarm (log + a field in `getblockchaininfo`) but never reorgs or halts an established node.

**Why this is the right scope:**

- **The benefit is entirely in IBD.** A fully-synced node validated every block itself and gains nothing from a checkpoint. Eclipse risk during initial sync is where the exposure actually lives — and where `nMinimumChainWork = 0x00` above height 23.5 M leaves the gap (D4).
- **It bounds oracle authority.** This is the sharpest objection to D4: a compiled checkpoint only changes what your node accepts *when you install a release*, whereas an oracle-driven one changes it **with no user action**. Restricting enforcement to IBD means oracles can guide nodes that have not yet formed their own view, and **cannot retroactively constrain a node that has.** The consent problem largely dissolves.
- **A mismatch on an established node is information, not an instruction.** If a synced node disagrees with an attestation, something is badly wrong — a compromised quorum, or the node on a fork. Both deserve a klaxon; neither is improved by that node silently reorganising itself on oracle say-so.

**Honest edge case:** a long-offline node re-entering IBD becomes subject to enforcement again. That is arguably correct — it is re-syncing and genuinely eclipse-vulnerable — but it does mean "established" is a property of *current* state, not history. Say so in the docs rather than letting an operator discover it.

**Pairs with the buried-height rule:** attest height H only once it is deeply confirmed (tip ≥ H + ~10,000, roughly 42 h at 15 s). Enforcing a checkpoint on a block that could still be reorged out would split the network; the quarterly cadence makes the wait free.

### 9.2 Attestations ride existing rails

DigiByte oracles **already** publish threshold-signed data in coinbase outputs, and consensus already validates it:

| Step | Existing symbol |
|---|---|
| Write to coinbase | `bundle_manager.cpp:816` `add_bundle_to_coinbase()`, `:826` `coinbase_tx.vout.push_back(oracle_output)` |
| Read back | `bundle_manager.cpp:944` `ExtractOracleBundle(const CTransaction& coinbase_tx, ...)` |
| Consensus validation | `bundle_manager.cpp:2201` `OracleDataValidator::ValidateBlockOracleData()` |

A snapshot attestation is a new *payload* on a proven mechanism, not new machinery. Publication cadence (~500 K blocks ≈ 87 days ≈ quarterly) means the target height is **deterministic** — `floor(tip / 500'000) × 500'000` — so a bootstrapping node needs no discovery protocol to know which block to look in.

### 9.3 Fully-internal bootstrap sequence

No external trust at any step; the only external fetch is bulk data verified against a chain-derived hash.

| # | Step | Trust | Cost |
|---|---|---|---|
| 1 | DNS seeds → peers → **headers sync** | P2P (existing) | **~1.83 GB** |
| 2 | Compute target height (deterministic) | none | — |
| 3 | `getdata` that single block; header chain proves its PoW | P2P (existing) | ~1 KB |
| 4 | `ExtractOracleBundle()` on its coinbase; verify 7-of-35 MuSig2 → **trusted {height, hash}** | **chain only** | — |
| 5 | Resolve mirror (DNS TXT / `-snapshoturl`), fetch blob via libcurl | **untrusted** | ~0.95 GB |
| 6 | Hash-check blob against step 4 | verified | — |
| 7 | Hand to existing `loadtxoutset` internal path | existing | — |
| 8 | **Fetch trailing 172,800 blocks over P2P** (§5.2) | P2P (existing) | ~0.06 GB |
| 9 | Background validation genesis → H | existing | ~30 GB, background |

**Download budget to usable node: ~2.9 GB** (headers 1.83 + snapshot 0.95 + trailing 0.06 + catch-up ≤0.18), vs ~36 GB for full sync. **~12×.** Note the floor is set by *headers*, not by the snapshot — the header chain is nearly 2× the snapshot's size. Any UX must show header progress or it will look hung.

**⚠ Ordering post-condition.** Step 8 must complete **before bootstrap reports success**. The DigiDollar rescan (§5.2) runs at *startup*: on the first run the tip is still genesis, DigiDollar is inactive, and the scan skips — harmless. The failure appears on the **next restart**, when the tip is at H with no history beneath it. A bootstrap that skips step 8 produces a node that works until the user restarts it, then refuses to start. Write this into the spec as an explicit post-condition.

### 9.4 §5.2 resolutions, re-ranked

Supersedes the ranking in §5.2, which was written assuming Bitcoin-sized blocks.

| Option | Cost | Verdict |
|---|---|---|
| **(A′) Fetch trailing window over P2P during bootstrap** | ~60 MB of ordinary block download; no new format, nothing to host | **Recommended.** Node already has headers and peers at that point |
| (A) Ship trailing window inside the bundle | +~60 MB to a ~950 MB bundle (6–12%); new bundle format | Viable fallback; strictly more work than A′ |
| (B) Attest the derived price/volatility state | Widens attestation format and its trust surface | Only if A′/A prove insufficient |
| (C) Defer the gate during background sync | Smallest format impact; leaves a window with incomplete DD consensus state | Needs careful thought about what the node may do meanwhile |

At measured block sizes the trailing window is **~60 MB** (172,800 × ~350 B), not the multi-GB burden the original ranking assumed.

### 9.5 User-facing options

All three converge on an identical fully-validated node. Option 2/3 differ only in *when* the wallet becomes usable.

| Mode | Usable in | Disk | Trust |
|---|---|---|---|
| Archival + full sync | days | ~36 GB | none required |
| Archival + snapshot | ~1 hour | ~36 GB | 7-of-35, **temporary** |
| Pruned + snapshot | ~1 hour | a few GB | 7-of-35, **temporary** |

The trust is a **bridge, not a destination**: background validation re-derives the UTXO hash from genesis and compares (`validation.cpp:6733`). The prompt must say what happens when they disagree — a fatal error — before the user opts in.

Pruning is safe here: `ChainstateManager::GetPruneRange` (`validation.cpp:7071-7074`) explicitly leaves background-IBD-chain blocks alone while pruning the snapshot chain, so the bridge still closes for pruned users. **But see §10.2** for a latent pruning bug that is independent of this project.

### 9.6 Hosting and generation

Given §9.1, hosting is a bandwidth/availability problem, not a security one.

**Generation** — the higher-value use of existing VPS capacity. Needs a **dedicated non-oracle archival node** with `-coinstatsindex` enabled, which:
- runs `dumptxoutset` on the cadence, holding `cs_main` (never do this on a node in consensus — measured 93 s just for `gettxoutsetinfo`, §3c);
- lets oracles compute the attested hash as an **index lookup** rather than a 93 s full scan each.

This node is a prerequisite of Sequence 0, not an afterthought.

**Distribution** — ~1 GB per snapshot, ~4/year, retained indefinitely (on-chain attestations are permanent, so any past attested height stays bootstrappable; a decade of them is ~40 GB).

| Role | Choice | Rationale |
|---|---|---|
| Blob origin | Zero-egress object store (Cloudflare R2 / B2+Cloudflare) | Bandwidth is the entire cost. 10 k bootstraps/yr ≈ 10 TB egress — free there, metered nearly everywhere else. Storage ≈ $0.30/mo at 20 GB |
| Mirror | GitHub Releases | Free, discoverable, versioned; 2 GB asset limit fits. Mirror, not primary — Releases-as-CDN is discouraged at volume |
| Resilience | Torrent + magnet in release notes | Zero marginal cost, community seeding, magnet self-verifies |
| Manifest / discovery | **DNS TXT**, mirroring the existing DNS-seed pattern, plus `-snapshoturl=` override | Updateable without shipping a Core release. Never compile a URL list in |
| Long term | Serve snapshots over DigiByte P2P | Eliminates external hosting; real DoS design work; **not Sequence 0** |

Self-hosting the blob on existing VPS capacity is viable given the untrusted-host property, but check the provider's transfer allowance against 1 GB × expected installs before making it the origin; a zero-egress store in front removes the question entirely.

**Redundancy is free, and this is a direct consequence of §9.1.** Because every mirror's output is checked against a chain-derived hash, mirrors need no trust, no vetting, no coordination, and no operational agreement. Practical implications:

- **Run as many as you like**, across providers and regions, on spare VPS capacity. There is no quorum to maintain and no consistency protocol — a stale mirror serving an older attested height is still perfectly valid, because that height's attestation is permanent on-chain.
- **Third parties may mirror without asking.** Exchanges, pools, community members, even an adversary — the hash check makes a hostile mirror indistinguishable in outcome from a broken one. Publish the expected hash and let the ecosystem host.
- **Failure is retry, not compromise.** A bad or unreachable mirror produces a failed hash check or a timeout; the node moves to the next entry in the list. Client behaviour should therefore be: try mirrors in randomised order, verify, fall back on mismatch — and treat a hash mismatch as *this mirror is bad*, never as *this snapshot is bad*, since the attestation is the authority.
- **A DNS TXT manifest (§9.6) makes the mirror set live** — add or drop mirrors without shipping a Core release. Keep a small compiled-in fallback list for the case where DNS is unavailable.
- **Torrent/P2P is redundancy of a different kind** — it survives the loss of *all* HTTP mirrors, since seeders are the network. Worth having precisely because it fails independently of the DNS+HTTP path.

The one thing redundancy does **not** protect against is a bad attestation, and nothing at this layer can. That risk sits entirely with the 7-of-35 oracle threshold and the background validation that re-checks it.

### 9.7 New code vs. existing

| Component | Status |
|---|---|
| Snapshot create/load/background-validate | **exists**, upstream-identical |
| Coinbase threshold-signed publication rail | **exists**, in production use |
| HTTP client (libcurl) | **exists** — `configure.ac:1620-1625` `HAVE_LIBCURL`, used by `src/oracle/exchange.cpp` |
| Anchor resolver + 4 call-site swap | new, small (§8) |
| Persisted anchor for `LoadBlockIndex` | new, small (§8) |
| Optional muhash comparison switch | new, ~3 lines (§8) |
| **Attestation format + oracle publication** | **new — this is Set 2** |
| **Bootstrap orchestrator (§9.3)** | **new — the main node-side work** |

### 9.8 Anticipated maintainer objections

| Objection | Response |
|---|---|
| "The daemon shouldn't fetch over HTTP" | libcurl is already linked and already used for outbound HTTPS (`oracle/exchange.cpp`). Weakens but does not answer it — oracle fetching is opt-in, this faces ordinary users. Posture: **off by default, explicit consent, hash-checked, URL-overridable** |
| "This is trust-on-first-use" | It is trust-until-verified. Background validation re-derives and compares (`validation.cpp:6733`); disagreement is fatal |
| "Upstream keeps assumeutxo manual/RPC-only" | True, and DigiByte going further is a deliberate choice justified by 24 M blocks and a real sync-time problem. Name it as a choice rather than letting a reviewer discover it |
| "Privacy: the mirror learns you're a new node" | Real. Document it; torrent/P2P paths mitigate |

---

## 10. Independent bugs found during this audit

Neither is caused by, nor blocks, the AssumeUTXO project. Both are shipped in 9.26.4 and worth reporting on their own.

### 10.1 chainparams carries Bitcoin assumeutxo data
`src/node/blockstorage.cpp:520` is a DigiByte-local patch whose error string reads: *"This is a DigiByte-specific issue where the snapshot was created with DigiByte blocks but chainparams has Bitcoin assumeutxo data."* Someone hit this and left the diagnosis in-tree. The signet entry (h=160'000, hash `fe0a4430…928a`, `chainparams.cpp:997-1003`) sits on a chain with its own genesis (`:992`). Failure mode is `fatalError()` at startup.

### 10.2 ~~`MIN_BLOCKS_TO_KEEP` never rescaled~~ — **RETRACTED 2026-08-12**

**This finding was wrong. Do not cite it.**

I claimed that because `MIN_BLOCKS_TO_KEEP = 288` (`validation.h:72`) means ~72 minutes at 15 s blocks rather than Bitcoin's ~2 days, minimally-pruned nodes would begin failing to start once average block size exceeded ~3,183 B — the point at which a 550 MB prune target stops holding the 172,800 blocks §5.2 demands.

**The premise is false: DigiDollar-era blocks cannot be pruned at all.** A dedicated prune lock is registered at startup (`src/node/chainstate.cpp:169-176`):

```cpp
if (options.prune && dd_floor > 0) {
    PruneLockInfo dd_lock;
    dd_lock.height_first = dd_floor;
    chainman.m_blockman.UpdatePruneLock("digidollar", dd_lock);
```

and it is enforced in the prune-range computation (`validation.cpp:3477-3485`), which clamps `last_prune` to `dd_floor - PRUNE_LOCK_BUFFER - 1`. Every block at or above the DigiDollar activation floor is retained regardless of prune target or block size. The same code path also refuses to start if DD-era data is *already* missing on a pruned node, with a specific error rather than the generic one.

`MIN_BLOCKS_TO_KEEP` is therefore not load-bearing here, and the break-even figure was meaningless. The developers evidently reasoned this through — the comment at `chainstate.cpp:160-167` lays out the argument explicitly.

**What is actually true, and worth noting instead:** a pruned DigiByte node's minimum retention is `[dd_floor, tip]` and **grows without bound**. Today that is ~390 K blocks ≈ 137 MB; at 15 s blocks it accrues ~2.1 M blocks/year, so in five years it is ~3.7 GB of mandatory retention on top of whatever prune target the operator set. Not a bug — a design property that should be documented so pruned-node operators are not surprised.

*Method note: this was flagged "derived, not reproduced" when written, and reproducing it is what disproved it. Retained here rather than deleted so the correction is visible.*

---

## 11. Bottom line

- **No upstream backport is required** — verified in source against Bitcoin v26.2 (0 non-cosmetic diff on the sampled surface) and confirmed live on 9.26.4. Lead the maintainer pitch with this; it inverts the expected cost model.
- **Publication: unblocked in code, gated on infrastructure.** Needs a dedicated non-oracle archival node with `-coinstatsindex`, or every attestation costs a measured **93 s `cs_main` stall** on a node in consensus. Treat that node as a Sequence 0 deliverable.
- **Consumption: unblocked in AssumeUTXO, gated by §5.2** — DigiDollar's 172,800-block startup rescan fails closed. Resolution **(A′) fetch the trailing ~60 MB over P2P during bootstrap** (§9.4) is the cheapest path and needs no format change. Settle it **before** the attestation format freezes.
- **Distribution and redundancy are non-problems.** ~0.95 GB per snapshot (15.86 M UTXOs), ~11× smaller than Bitcoin's, ~4/year. Because trust is on-chain, mirrors are untrusted and unlimited (§9.6).
- **The download floor is headers, not the snapshot** — ~1.83 GB of header chain vs ~0.95 GB of snapshot. Total to usable node ~2.9 GB against ~36 GB for full sync (~12×). Design the UX around header progress.
- **The real work is Set 2 and the bootstrap orchestrator**, not core plumbing (§9.7).
- **Two shipped bugs found in passing** (§10), neither blocking this project: Bitcoin assumeutxo data in chainparams, and `MIN_BLOCKS_TO_KEEP` never rescaled for 15-second blocks.

### Immediate next steps

1. Run `feature_assumeutxo.py` on this tree — needs a build, not a node. Converts every "complete" claim here from structural to behavioural. **Highest value per hour.**
2. Stand up the generation node (non-oracle, archival, `-coinstatsindex`); measure index build time from genesis at 24 M blocks.
3. Reproduce §5.2 on regtest via `-digidollaractivationheight` — confirms the blocker and validates whichever resolution you pick.
4. Decide the attested hash type (`hash_serialized_3` vs muhash, §8) — it determines what Set 2 carries.
