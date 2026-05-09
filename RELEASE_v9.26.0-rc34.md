# DigiByte v9.26.0-rc34 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## SAME TESTNET — NO RESET

**RC34 uses the same `testnet23` chain as RC33.** No reset, no new genesis,
no network-magic change, no port change. Existing chain data, wallets, DD
positions, and oracle keys carry forward.

Oracle operators running RC33 can drop in RC34 binaries and resume. There
are **no oracle key rotations in RC34**.

This release closes Wave 0 audit finding `DD-FA-DOC-001` (release notes
lagged the source/build version, which already reported `9.26.0rc34`).

---

## Local Commit Consolidation

After the fresh Codex Wave 0-26 rerun and final green Wave 26 gate, Jared
requested a clean history for every change since `v9.26.0-rc33`. The branch
was rewritten from the RC33 tag into a logical subsystem stack covering release
baseline, script/consensus, DigiDollar validation, node validation/miner,
oracle, RPC, wallet, Qt, devtools, test coverage, and docs/reports.

Review the current local-only stack with:

```bash
git log --oneline v9.26.0-rc33..HEAD
```

The pre-rewrite state remains available locally at
`backup/digidollar-v1-pre-rc33-consolidation-20260506`. Older granular commit
hashes in audit evidence refer to that preserved backup history.

---

## What's New in RC34

RC34 is the **DigiDollar V1 Final Audit release** on top of RC33. It
captures every confirmed bug, security vulnerability, test gap, and
documentation drift found by the fresh Codex rerun through Wave 26 after
the RC33 Red Hornet hardening release, including the final
backward-compatibility / activation launch evidence. The campaign was run in parallel sub-agent
waves (security/exploit, functional/test, integration/release) under the
discipline laid out in `final_audit.MD`, with full regression gates after
every wave.

The short version: RC33 stabilised the Red Hornet fixes; RC34 closes the
remaining final-launch readiness items the Final Audit surfaced —
including six Critical/High consensus-class fixes, a cross-chain MuSig2
replay fix, an IBD lock-tier-duration consensus split, the miner
oracle-quote freshness parity gap, the current-head deterministic
volatility consensus fix, and a full sweep of test/doc/RPC/Qt hardening.

## RC34 fix summary since RC33

- **Six Critical/High consensus-class hardening fixes** — collateral
  spend detection now uses persisted vault metadata instead of
  process-local state, non-marker spend paths can no longer bypass DD
  burn validation, ERR/overburn redemptions account exactly for DD
  burned, redemption ERR health uses deterministic block context instead
  of stale process state, DD-marked non-redemption transactions are
  prevented from spending collateral vaults without burning the vault
  DD, and unconfirmed mint collateral children cannot miss mandatory
  burn validation.
- **MuSig2 cross-chain replay closed** — v0x03 oracle bundle aggregate
  signatures now bind the chain `params.hashGenesisBlock` plus a domain
  tag, so a mainnet bundle cannot replay onto testnet (or vice versa).
- **IBD/catch-up lock-tier consensus split closed** — IBD nodes now
  enforce the canonical lock-tier-duration check in every code path, so
  an IBD node can no longer be forked by a peer feeding a non-canonical
  mint that the caught-up validator rejects.
- **Mempool / miner / ConnectBlock parity completed** — the miner now
  enforces the same wall-clock oracle quote freshness check as the
  mempool, and `bad-oracle-timestamp` is now in
  `IsRetryableDigiDollarBlockFailure`, so DD txs cannot be mined past
  the freshness window the mempool guards.
- **Volatility consensus split closed** — DD transaction validation is
  now side-effect free. Accepted mint volatility is recorded only after
  a block has fully connected, uses the candidate block `nTime`, is
  removed on disconnect, and is reconstructed at startup from
  deterministic on-chain MuSig2 oracle-bundle data.
- **Current rerun Wave 3 collateral math hardening** — required
  collateral above `MAX_MONEY` now fails closed instead of capping,
  fallback txbuilder change no longer masquerades as collateral, and
  `mintdigidollar` reports the effective DCA-adjusted collateral ratio.
- **Current rerun Wave 4 burn-enforcement hardening** — collateral-child
  validation now retrieves unconfirmed DD mint parents from the mempool, and
  miner package assembly applies the DD block-inclusion gate to every package
  transaction so non-DD children cannot miss mandatory burn validation.
- **Current rerun Wave 5 redemption/miner hardening** — miner package
  assembly now skips descendants of rejected DD package transactions, and
  `getredemptioninfo` no longer advertises locked encrypted wallets as
  currently redeemable.
- **Current rerun Wave 6-9 RPC, lock-tier, and oracle-doc alignment** — quote RPCs
  now expose both the consensus collateral minimum and the wallet mint
  builder's 1% safety margin, `listdigidollarpositions` rejects invalid
  `tier_filter` values outside `-1`/`0..9`, and lock-tier docs now
  consistently describe the ten canonical tiers (1 hour through
  10 years, 1000%-200%). Wave 8 also clarifies `startoracle` status when
  the price fetcher is not running and refreshes V1 oracle setup docs to
  state MuSig2 v0x03-only production behavior. Wave 9 filters mainnet
  reserve slots out of signing and pending-message quorum so only slots 0-16
  participate in the 9-of-17 V1 oracle set.
- **Defense-in-depth alignment between BIP9 and `nDDActivationHeight`
  on regtest** — vault-spend detection is now triggered by the lower of
  the two so the regtest `-digidollaractivationheight=N` knob does not
  drift away from production behaviour.
- **DD address validation tightened** — `validateddaddress`,
  `senddigidollar`, and the `CDigiDollarAddress` constructor reject any
  whitespace anywhere in the input, preventing copy-paste loss-of-funds
  caused by `DecodeBase58Check`'s transparent whitespace stripping.
- **Wallet recovery and persistence hardened** — `wallet_digidollar_persistence_restart`
  was a silent no-op; rewritten to actually exercise the load → mint →
  restart → persist → unload → reload → claim path, plus a new
  `wallet_digidollar_active_restore_redeem.py` matrix covering
  rescan/restart/restore.
- **Wallet spendability/watch-only safety** — `listdigidollarpositions`
  no longer claims `spendable=true` on a locked encrypted wallet,
  `validateddaddress` exposes `solvable`, `listdigidollaraddresses`
  hides empty addresses by default, the Qt positions widget shows a
  watch-only badge, and locked-wallet write RPCs emit
  DigiDollar-flavored hints instead of the generic
  `walletpassphrase` text.
- **RPC schema hardening** — `createoraclekey` rejects negative `oracle_id`
  and any id missing from `Params().GetOracleNode()` (so regtest cannot
  persist unusable wallet keys for slots 7..29), `listdigidollarpositions`
  now bounds its response with `count`/`skip` paging parameters, and 22
  schema/unit/minconf/min-amount/cents-to-DGB invariants are pinned.
- **Qt UX safety** — Qt `mintdigidollar` rejects watch-only wallets up
  front, the redeem widget shows the human-readable lock-tier label
  instead of the opaque "Tier N" string, and Qt translates consensus
  reject reasons through a shared translator helper.
- **MuSig2 P2P replay/dedup pinned** — `seen_message_hashes` cap-of-2048
  is pinned, `ClearPendingMessages` restart contract is pinned,
  cross-epoch isolation is pinned, malformed payload rejection is
  pinned, stale positive MuSig2 relay epochs are dropped before relay
  or session ingestion, and the dedup-hash contract for the MuSig2 P2P
  message families is pinned by Boost and functional cases.
- **DoS and resource caps pinned** — the `OracleBundleManager::UpdatePriceCache`
  1000-entry cap, `COracleBundle::DeserializeV03Data` size guard,
  `OracleBundleManager::AddOracleMessage` reject path, and
  `ClearPendingMessages` operator-restart hot path are pinned by 64,032
  assertions in `digidollar_wave21_dos_resource_tests.cpp`.
- **Fuzz harness completeness** — six historical Wave 22 fuzz harnesses
  (`dd_txbuilder_validate_mint_params`,
  `dd_txbuilder_redeem_consensus_round_trip`,
  `oracle_bundle_hash_domain_sep`,
  `oracle_musig2_session_real`,
  `oracle_id_bitmap_mutations`,
  `oracle_validate_block_data`) plus marathon coverage of 832,596 seeds
  across 51 DD/oracle/MuSig2 targets. The Codex rerun adds
  `oracle_musig2_auth_signature_domain`, registers 10 previously unregistered
  active harness sources, and the current seeded all-target gate passes
  247/247 registered fuzz targets. Zero production crashes, zero sanitizer
  failures, zero OOM.
- **Build/warning cleanup** — duplicate `CheckMinimalPush` decl in
  `src/script/interpreter.h` removed (would have blocked an
  `--enable-werror` build), unused mock variable in `digidollar/health.cpp`
  removed, and `contrib/devtools/security-check.py` now supports current LIEF
  enum namespaces so the release hardening gate runs cleanly.
- **Wave 25 final sweep hardening** — deprecated ignored
  `senddigidollar` / `redeemdigidollar` fee-rate positional arguments remain
  accepted for RC client compatibility, live RH49 OP_RETURN tests are now
  registered, stale source-only bughunt/gui tests were removed, and wallet
  pending-redeem recovery now distinguishes final mempool expiry/size evictions
  from transient `REORG` churn.
- **Wave 26 compatibility and activation proof** — the direct regtest
  `-digidollaractivationheight=N` knob now aligns BIP9 and static DD/oracle
  height gates, DD-touching post-activation blocks are pinned to final v0x03
  MuSig2 oracle rules, hidden Qt positions widgets no longer poll wallet/oracle
  state before display, and startup oracle-cache reconstruction follows the
  same BIP9 activation predicate as block connection. The final gate also
  reconciles reorged-out confirmed DD redeems after restart/reindex while
  keeping live pending redeems reserved, and the wallet reorg fixture now proves
  both the pending-mempool reservation and active-chain restoration states. The
  generic GBT longpoll fixture now disables Dandelion when testing direct
  mempool wakeups.
- **Documentation aligned to code** — DigiDollar wallet integration
  guide gained a wallet-lifecycle / recovery section, the activation
  explainer documents the BIP9 / `IsOracleActive` 1-block boundary and
  the regtest `-digidollaractivationheight` knob, the legacy
  oracle reject-reason matrix matches code, and stale references to a
  separate `test_wallet_digibyte` binary were corrected (wallet tests
  link into the unified `src/test/test_digibyte`).

---

## Final Audit campaign coverage

The fresh Final Audit rerun is green through Wave 26, including the final
backward-compatibility / activation proof. Earlier campaign evidence and
current-head revalidation also closed
the volatility consensus item previously queued as `DD-FA-ARCH-003`.
Generated audit reports are intentionally local-only and ignored by git so the
release history stays focused on code, tests, product docs, and release notes.
The local ignored artifacts used for the final sign-off include:

- `final_audit.MD` (campaign brief)
- `reports/final_audit_ledger.md` (running ledger of every confirmed and
  rejected finding)
- `reports/final_audit_wave_00.md` through `reports/final_audit_wave_13.md`,
  plus later agent-specific wave reports such as
  `reports/final_audit_wave_26_agent_a.md` and
  `reports/final_audit_wave_26_agent_c.md`, and current rerun reports through
  `reports/final_audit_wave_26_rerun.md`

### Confirmed bugs / vulnerabilities fixed in RC34

#### Security (`DD-FA-SEC-###`)

| ID | Severity | Area | Summary |
|----|----------|------|---------|
| `DD-FA-SEC-001` | Critical | Collateral spend detection | Non-DD collateral spend detection depended on process-local metadata; now uses persisted vault metadata. |
| `DD-FA-SEC-002` | Critical | Production gates | Production gates skipped non-marker collateral spends; tightened to inspect every spend. |
| `DD-FA-SEC-003` | High | ERR accounting | ERR/overburn redemptions undercounted DD burned in chain and stats accounting; corrected. |
| `DD-FA-SEC-004` | High | Redemption ERR | Redemption ERR health used stale/default process state instead of deterministic block context; switched to deterministic source. |
| `DD-FA-SEC-005` | Critical | Collateral burn | DD-marked non-redemption transactions could spend collateral vaults without burning the vault DD; rejected. |
| `DD-FA-SEC-006` | High | Timelock | Redemption validation did not compare `nLockTime` with the mint-committed lock height; now compared. |
| `DD-FA-SEC-007` | High | Output indexing | A mint's ordinary DGB change output could masquerade as redemption collateral input 0; rejected. |
| `DD-FA-SEC-008` | High | MuSig2 domain sep | v0x03 MuSig2 bundle aggregate signature lacked chain-identity domain separation, enabling cross-chain replay; binds chain genesis hash and tag. |
| `DD-FA-SEC-009` | Medium | Oracle feeds | Exchange-feed safety cap tightened to $10 to reject malformed-above-sanity outliers. |
| `DD-FA-SEC-010` | Low | Activation gate | `SpendsDigiDollarCollateralVault` skip diverged from BIP9 activation on regtest; aligned to lower of BIP9 / `nDDActivationHeight`. |
| `DD-FA-SEC-011` | High | IBD consensus split | IBD nodes accepted noncanonical lock-tier-duration mints rejected by caught-up nodes; canonical lock-tier-duration check now enforced on every validator path. |
| `DD-FA-SEC-012` | High | Volatility consensus | Shared DD validation mutated global volatility state with wall-clock time; validation is now pure and accepted mint volatility is recorded post-ConnectBlock using block `nTime`. |
| `DD-FA-SEC-013` | High | Volatility consensus | Current rerun removed consensus-visible floating-point thresholding from volatility freeze decisions; integer BPS math is used. |
| `DD-FA-SEC-014` | High | Parser | Current rerun fixed legacy Format 1 DD OP_RETURN amount decoding so malformed high-bit values are parsed through unsigned arithmetic before any `CAmount` cast. |
| `DD-FA-SEC-015` | High | Collateral math | Current rerun fails closed when required collateral exceeds `MAX_MONEY` instead of capping to an under-required boundary value. |
| `DD-FA-SEC-016` | Critical | Collateral burn | Current rerun makes unconfirmed mint collateral children retrieve the mempool parent and requires miner package validation for every package transaction. |
| `DD-FA-SEC-017` | High | Canonical health | Current rerun normalizes cached health oracle prices as micro-USD and converts to millicents for DCA health math, eliminating a volatility price-unit overhealth path. |
| `DD-FA-SEC-018` | High | Oracle quorum | Current rerun rejects reserve oracle slots from off-chain pending-message quorum and consensus attestations, preventing reserve metadata keys from driving the price active MuSig2 signers attest. |
| `DD-FA-SEC-019` | High | MuSig2 P2P auth | Current rerun chain-binds MuSig2 nonce and partial-sig P2P auth hashes to `hashGenesisBlock`, preventing mainnet/testnet replay of authenticated wire messages. |
| `DD-FA-SEC-020` | Medium | MuSig2 P2P relay | Current rerun rejects reserve oracle slots from MuSig2 nonce/partial-sig relay admission before dedup, relay, or session ingestion. |
| `DD-FA-SEC-021` | Medium | Oracle feeds | Current rerun rejects malformed exchange price strings with trailing junk instead of accepting numeric prefixes into local feed aggregation. |
| `DD-FA-SEC-022` | High | Activation boundary | Current rerun gates the active-only DD-marker coinbase ban behind the BIP9 active predicate so pre-activation DD-looking coinbase data cannot change base-chain block validity. |
| `DD-FA-SEC-023` | High | Activation boundary | Current rerun skips DigiDollar body-transaction consensus validation until BIP9 is active, while ordinary block input/script checks still run, so pre-activation DD-looking transactions do not split upgraded/non-upgraded block validity. |
| `DD-FA-SEC-024` | Critical | Activation boundary / supply | Current rerun rejects zero-value DD-looking source UTXOs created before activation before transfer or redemption amount extraction, so ordinary pre-activation data cannot become post-activation DD supply. |
| `DD-FA-SEC-025` | High | Wallet/oracle key storage | Current rerun encrypts wallet-managed oracle signing keys in encrypted wallets, migrates plaintext `ORACLE_KEY` rows, and refuses locked-wallet oracle key reads. |
| `DD-FA-SEC-026` | High | Wallet preset inputs | Current rerun rejects wallet-owned DD mint collateral/token/metadata outputs from ordinary preset-input funding even if DD sidecar lock state is stale or absent. |

#### Functional (`DD-FA-FUNC-###`)

| ID | Severity | Area | Summary |
|----|----------|------|---------|
| `DD-FA-FUNC-003` | Medium | Parser | DD OP_RETURN parser rejected the documented per-output maximum; corrected. |
| `DD-FA-FUNC-004` | High | Collateral release | Separate DGB fee inputs reduced required collateral release; now isolated. |
| `DD-FA-FUNC-005` | Medium | RPC | `estimatecollateral` rounded fractional DCA down and drifted from canonical health; aligned. |
| `DD-FA-FUNC-006` | Low | Builder | Redeem txbuilder divided by zero when cached DD minted amount was zero; guarded. |
| `DD-FA-FUNC-007` | Medium | Reorg | Regtest oracle mock reset corrupted DD reorg mempool resurrection; fixed. |
| `DD-FA-FUNC-008` | Medium | Wallet | Pending DD balance was hidden after valid mint reorg resurrection; restored. |
| `DD-FA-FUNC-009` | Medium | RPC | `getredemptioninfo` displayed a DGB fee haircut instead of full collateral return; corrected. |
| `DD-FA-FUNC-010` | Medium | Qt | Qt ERR burn display used hand-coded floating tiers instead of consensus ERR ceil math; aligned. |
| `DD-FA-FUNC-011` | Low | RPC | `mintdigidollar` ERR pre-check used wallet-local DD positions, not canonical health; switched. |
| `DD-FA-FUNC-012` | Medium | Docs/code | Documentation/code mismatch on legacy oracle reject reason; aligned. |
| `DD-FA-FUNC-013` | Medium | Roster | Roster alignment / oracle network discovery tightened. |
| `DD-FA-FUNC-014` | Medium | RPC | Operators had no programmatic way to inspect MuSig2 signing-session liveness; RPC added. |
| `DD-FA-FUNC-015` | Low | Tests | Multi-node activation, reorg, IBD, and reindex coverage was thin; expanded. |
| `DD-FA-FUNC-016` | Medium | Miner parity | Miner missed wall-clock oracle quote freshness; mirrors mempool gate. |
| `DD-FA-FUNC-017` | Low | Tests | Multi-node mempool/miner/ConnectBlock parity coverage was thin; expanded. |
| `DD-FA-FUNC-018` | Low | Tests | Multi-node IBD/reorg/reindex parity coverage at full DD activation was thin; expanded. |
| `DD-FA-FUNC-019` | Medium | Address | `validateddaddress` and `senddigidollar` accepted whitespace-padded DD addresses; rejected at constructor. |
| `DD-FA-FUNC-020` | Medium | Wallet test | `wallet_digidollar_persistence_restart` silently no-op-passed; rewritten to actually exercise persistence. |
| `DD-FA-FUNC-021` | Low | Wallet test | Wave 16 lacked a real load/unload/rescan/restart matrix; added. |
| `DD-FA-FUNC-022` | Medium | RPC | `listdigidollarpositions` deceived locked encrypted wallets (claimed `spendable=true`/`can_redeem=true`); honors locked wallet. |
| `DD-FA-FUNC-023` | Low | RPC | `validateddaddress` missed `solvable` field; added. |
| `DD-FA-FUNC-024` | Low | RPC | `listdigidollaraddresses` leaked empty addresses; default `include_empty=false`. |
| `DD-FA-FUNC-025` | Low | RPC | Locked redeem hint was not DigiDollar-flavored; added DD context. |
| `DD-FA-FUNC-027` | Low | Qt | Qt positions widget lacked watch-only badge; added. |
| `DD-FA-FUNC-028` | Low | RPC | `createoraclekey` accepted negative or out-of-roster `oracle_id`; tightened. |
| `DD-FA-FUNC-029` | Low | RPC | `createoraclekey` cast `oracle_id` to `uint32_t` before bounds-check; reads into `int` and rejects `< 0`. |
| `DD-FA-FUNC-030` | Low | Qt | Redeem widget rendered "Tier N" instead of human-readable lock-tier label; aligned with mint/positions widgets. |
| `DD-FA-FUNC-031` | Low | Qt | Qt mint widget did not reject watch-only wallets up front; rejected at WalletModel layer. |
| `DD-FA-FUNC-033` | Medium | Tests | Real multi-node oracle P2P functional coverage missing; added. |
| `DD-FA-FUNC-034` | Medium | RPC | `listdigidollarpositions` had no per-call result cap or paging; added `count`/`skip` parameters. |
| `DD-FA-FUNC-035` | Low | Fuzz harness | `dd_collateral_math` strategy 7 stale assertion (harness contract gap; no production bug). |
| `DD-FA-FUNC-036` | Low | Fuzz harness | `dd_price_conversion` strategy 9 stale assertion (harness contract gap; no production bug). |
| `DD-FA-FUNC-037` | Low | Build | Duplicate `CheckMinimalPush` decl in `src/script/interpreter.h`; would block `--enable-werror`; removed. |
| `DD-FA-FUNC-038` | Low | Code hygiene | Unused mock tip pointer in `digidollar/health.cpp`; removed. |
| `DD-FA-FUNC-040` | Medium | Wallet | Current rerun rejects legacy/non-descriptor DigiDollar owner-key and address creation before fallback key derivation. |
| `DD-FA-FUNC-041` | Medium | Redemption | Current rerun rejects multiple DD change outputs from a single redemption OP_RETURN amount to avoid ambiguous future amount extraction/accounting. |
| `DD-FA-FUNC-042` | Medium | Builder | Current rerun keeps fallback mint DGB change non-P2TR so it cannot be classified as a second collateral output. |
| `DD-FA-FUNC-043` | Medium | RPC | Current rerun makes `mintdigidollar.collateral_ratio` report the effective DCA-adjusted ratio used by collateral calculation. |
| `DD-FA-FUNC-044` | High | Miner | Current rerun skips descendants of rejected DD package transactions so block-template validity cannot include a grandchild without its skipped parent. |
| `DD-FA-FUNC-045` | Medium | RPC | Current rerun makes `getredemptioninfo.can_redeem` honor locked encrypted wallets. |
| `DD-FA-FUNC-046` | Medium | RPC | Current rerun exposes `minimum_required_dgb`, `wallet_collateral_dgb`, and `collateral_safety_margin_dgb` so quote RPCs show the wallet's 1% collateral safety margin. |
| `DD-FA-FUNC-047` | Low | RPC | Current rerun rejects `listdigidollarpositions tier_filter` values outside `-1`/`0..9` instead of silently treating invalid tiers as no filter or empty results. |
| `DD-FA-FUNC-048` | Low | RPC/operator | Current rerun clarifies `startoracle` status when an oracle is initialized but its price fetcher is not running. |
| `DD-FA-FUNC-049` | High | Oracle signing | Current rerun keeps mainnet signing key aggregation on slots 0-16 so reserve `vOracleNodes` metadata slots cannot make mainnet oracle sessions fail liveness. |
| `DD-FA-FUNC-050` | High | MuSig2 P2P | Current rerun registers `oramusnonce` / `oramusigpsig` in the protocol command matrix and maps their inventory types to the correct wire commands. |
| `DD-FA-FUNC-051` | Medium | RPC/operator | Current rerun reports aggregate MuSig2 signature validity for v0x03 on-chain oracle participants instead of marking them invalid for missing per-oracle attestations. |
| `DD-FA-FUNC-052` | Medium | RPC/operator | Current rerun splits `getdigidollardeploymentinfo` mandatory oracle/DD activation height from the v0x03 MuSig2 format activation height. |
| `DD-FA-FUNC-053` | Low | Qt activation UI | Current rerun makes the Qt DigiDollar activation banner use the node's VersionBits state instead of height-window heuristics. |
| `DD-FA-FUNC-054` | High | Miner/backcompat | Current rerun keeps ordinary non-DD OP_RETURN `"DD"` datacarrier transactions out of DigiDollar miner policy so upgraded miners can include them without oracle bundles. |
| `DD-FA-FUNC-055` | Medium | VerifyDB/cache | Current rerun keeps `verifychain` checklevel 3/4 memory-only verification from mutating live DigiDollar health, volatility, or oracle price caches. |
| `DD-FA-FUNC-056` | Low | Wallet test state | Current rerun clears DD address-key and encrypted key caches in `ClearWalletData()` so restore/rescan tests cannot pass using stale key material. |
| `DD-FA-FUNC-057` | Medium | Qt locked-wallet UX | Current rerun makes the Qt positions tab render matured DD vaults in locked encrypted wallets as disabled `Wallet Locked`, not enabled `Redeem`. |
| `DD-FA-FUNC-058` | Medium | RPC amounts | Current rerun replaces floating DigiDollar RPC amount parsing with fixed-precision parsing so decimal JSON numbers match decimal strings and sub-cent DD values reject. |
| `DD-FA-FUNC-059` | Low | RPC/operator | Current rerun adds DigiDollar-specific locked-wallet context to `startoracle` when loading wallet-managed oracle keys. |
| `DD-FA-FUNC-060` | Low | RPC/oracle status | Current rerun reports `getdigidollarstats.oracle_price_age` from mock/on-chain update height instead of hardcoding zero. |
| `DD-FA-FUNC-061` | Low | RPC/operator | Current rerun rejects out-of-roster IDs in `stoporacle` and `getoraclepubkey`, matching `createoraclekey`/`startoracle` chainparams boundaries. |
| `DD-FA-FUNC-062` | Medium | Qt watch-only | Current rerun hides pending DD from private-key-disabled Qt wallets, matching confirmed DD balance display. |
| `DD-FA-FUNC-063` | Medium | Qt address validation | Current rerun makes Qt DD address preflight use active-network checksum validation so cross-chain DD addresses reject before confirmation. |
| `DD-FA-FUNC-064` | Medium | Qt oracle health | Current rerun renders missing oracle health as unavailable (`N/A`) instead of a false numeric at-risk state. |
| `DD-FA-FUNC-065` | Medium | Oracle P2P recovery | Current rerun clears the consensus-attestation replay cache in `ClearPendingMessages()` so oracle restart/stale-consensus recovery can accept re-delivered valid attestations. |
| `DD-FA-FUNC-066` | Medium | MuSig2 P2P DoS | Current rerun rejects stale positive MuSig2 nonce and partial-signature epochs; current and current+1 remain relayable for epoch-boundary liveness. |
| `DD-FA-FUNC-067` | Medium | Release tooling | Current rerun restores `make -C src check-security` under current LIEF so binary hardening checks are part of the Wave 23 gate. |
| `DD-FA-FUNC-068` | Low | RPC compatibility | Current rerun preserves deprecated ignored `senddigidollar` / `redeemdigidollar` fee-rate positional arguments for old RC clients. |
| `DD-FA-FUNC-069` | Medium | Wallet pending redeem | Current rerun abandons stale non-mempool DD redeems and rescans DD state after final mempool expiry/size eviction. |
| `DD-FA-FUNC-070` | Medium | Wallet mempool state | Current rerun prevents transient `REORG` mempool removals from reactivating a DD position while the redeem remains live. |
| `DD-FA-FUNC-071` | Low | Regtest activation | Wave 26 aligns the direct `-digidollaractivationheight=N` override so BIP9, DD, and oracle height gates move together. |
| `DD-FA-FUNC-072` | Low | Qt hidden polling | Wave 26 prevents hidden DD positions widgets from polling/loading wallet and oracle state before display. |
| `DD-FA-FUNC-073` | Low | Startup oracle cache | Wave 26 makes startup oracle-price reconstruction follow the same BIP9 activation predicate as block connection. |
| `DD-FA-FUNC-074` | Medium | Wallet pending redeem | Wave 26 deactivates wallet positions when valid DD redeems are imported from mempool/startup/reindex state. |
| `DD-FA-FUNC-075` | Medium | Wallet reorg/reindex | Wave 26 reconciles reorged-out confirmed DD redeems after restart/reindex without reactivating live pending redeems. |

#### Test coverage (`DD-FA-TEST-###`)

| ID | Severity | Area | Summary |
|----|----------|------|---------|
| `DD-FA-TEST-001` | Medium | Fuzz baseline | Full fuzz gate needs generated corpus because `qa-assets` is absent. |
| `DD-FA-TEST-002` | Medium | Fixture | DD validation tests leaked volatility state into later suites; cleared. |
| `DD-FA-TEST-003` | Medium | Fixture | ERR/DCA health state leaked across targeted Boost suite order; cleared. |
| `DD-FA-TEST-004` | Medium | Redteam | Redteam redemption fixtures missed expired locktimes after Wave 5 hardening; updated. |
| `DD-FA-TEST-005` | Medium | Fixture | Health fixture leaked SystemHealthMonitor / Volatility state into rh64 cases; cleared. |
| `DD-FA-TEST-006` | Medium | Coverage | Wave 6 health/DCA/volatility coverage was thin; expanded. |
| `DD-FA-TEST-007` | Medium | Coverage | Wave 7 lock-tier canonicalization coverage was thin; expanded. |
| `DD-FA-TEST-008` | Medium | Coverage | Multi-node propagation of malformed/legacy oracle bundles unproven; pinned. |
| `DD-FA-TEST-009` | Medium | Coverage | Wave 8 oracle bundle reject-reason matrix coverage was thin; pinned. |
| `DD-FA-TEST-010` | Medium | Coverage | Roster alignment functional coverage. |
| `DD-FA-TEST-011` | Medium | Coverage | Wave 9 quorum/roster/aggregate-drift unit and fuzz coverage was thin; expanded. |
| `DD-FA-TEST-012` | Medium | Coverage | Wave 10 session-state, malformed-payload, final-signature-mismatch coverage was thin; expanded. |
| `DD-FA-TEST-013` | Medium | Coverage | Wave 11 oracle-feed staleness/outlier/missing/malformed/precision/mock-gate coverage was thin; expanded. |
| `DD-FA-TEST-014` | Medium | Coverage | Wave 12 activation-boundary unit coverage was thin; expanded. |
| `DD-FA-TEST-015` | Medium | Coverage | Wave 13 mempool/miner/ConnectBlock parity coverage was thin; expanded. |
| `DD-FA-TEST-016` | Medium | Coverage | Wave 14 reorg-replay invariants unpinned; pinned. |
| `DD-FA-TEST-017` | Low | Coverage | Format 1 OP_RETURN above-MAX_DIGIDOLLAR rejection had no direct unit pin; pinned. |
| `DD-FA-TEST-018` | Low | Coverage | Format 2 vs Format 1 OP_RETURN preference had no direct unit pin; pinned. |
| `DD-FA-TEST-026` | Low | Coverage | 22 RPC schema/unit/minconf/min-amount/cents-to-DGB invariants had no direct unit pins; 22 cases pinned. |
| `DD-FA-TEST-027` | Low | Coverage | Mint widget tier dropdown had no direct unit pin; pinned. |
| `DD-FA-TEST-028` | Low | Coverage | Transactions widget filter had no direct unit pin; pinned. |
| `DD-FA-TEST-029` | Low | Coverage | DD coin-control dialog DD-locked filter Qt pin missing; pinned. |
| `DD-FA-TEST-030` | Low | Coverage | MuSig2 P2P message-family dedup-hash contract pinned. |
| `DD-FA-TEST-031` | Low | Coverage | `ClearPendingMessages` restart contract pinned. |
| `DD-FA-TEST-032` | Low | Coverage | Cross-epoch isolation for the bundle-manager pinned. |
| `DD-FA-TEST-033` | Low | Coverage | Malformed P2P payload rejection pinned. |
| `DD-FA-TEST-034` | Low | Coverage | `seen_message_hashes` cap-of-2048 pinned (2560 distinct hashes flooded; 2048 retained). |
| `DD-FA-TEST-035` | Low | Coverage | Direct-replay (same signed message) pinned. |
| `DD-FA-TEST-036` | Low | Coverage | `OracleBundleManager::UpdatePriceCache` 1000-entry cap pinned. |
| `DD-FA-TEST-037` | Low | Coverage | `COracleBundle::DeserializeV03Data` size guard pinned. |
| `DD-FA-TEST-038` | Low | Coverage | `OracleBundleManager::AddOracleMessage` reject path pinned. |
| `DD-FA-TEST-039` | Low | Coverage | `ClearPendingMessages` operator-restart hot path pinned. |
| `DD-FA-TEST-040` | Low | Fuzz | New `dd_txbuilder_validate_mint_params`, `dd_txbuilder_redeem_consensus_round_trip` harnesses pinned. |
| `DD-FA-TEST-041` | Low | Fuzz | New `oracle_bundle_hash_domain_sep` mutation fuzz pinned (DD-FA-SEC-008 invariants). |
| `DD-FA-TEST-042` | Low | Fuzz | New `oracle_musig2_session_real` lifecycle fuzz pinned. |
| `DD-FA-TEST-043` | Low | Fuzz | New `oracle_id_bitmap_mutations` fuzz pinned. |
| `DD-FA-TEST-044` | Low | Fuzz | New `oracle_validate_block_data` CBlock fuzz pinned. |
| `DD-FA-TEST-045` | Low | Fixture | Locktier fixture only reset volatility on construct, not teardown; both ends now cleared. |
| `DD-FA-TEST-046` | Low | Historical reserved ID | No current Wave 23 finding uses this reserved slot; the fresh rerun's fuzz-registration issue is tracked as `DD-FA-TEST-067`. |
| `DD-FA-TEST-047` | Medium | Fixture | Wave 13/14 fixtures leaked volatility history into later suites; teardown now clears state. |
| `DD-FA-TEST-049` | Low | Compatibility | Wave 26 backward-compat invariants lacked direct unit pins; `digidollar_wave26_compat_tests` added. |
| `DD-FA-TEST-050` | Low | Compatibility | End-to-end mixed-node compatibility test added for ordinary DGB transactions across activation. |
| `DD-FA-TEST-051` | Low | Volatility | Deterministic volatility state lacked direct regression pins; RH-41 tests added. |
| `DD-FA-TEST-052` | Low | Transfer | Current rerun strengthened RH12 transfer OP_RETURN amount-count mismatch coverage with explicit reject-reason assertions. |
| `DD-FA-TEST-053` | Low | Fuzz | Current rerun aligned `dd_price_conversion` fuzz expectations with production DCA basis-point ceil rounding. |
| `DD-FA-TEST-054` | Low | Boundary tests | Current rerun aligned Red Team/RH overflow tests with fail-closed collateral math. |
| `DD-FA-TEST-055` | Low | Burn enforcement | Current rerun pins unconfirmed mint collateral-child validation, miner package selection, and RPC/mempool/submitblock collateral-spend guards. |
| `DD-FA-TEST-056` | Low | Fixture | Current rerun seeds explicit micro-USD oracle prices in RH34/health-DCA fixtures that test priced health after missing-price paths were made fail-closed. |
| `DD-FA-TEST-057` | Low | Oracle bundle | Current rerun adds a functional `submitblock` matrix case proving truncated v0x03 oracle bundles reject as `bad-oracle-malformed`. |
| `DD-FA-TEST-058` | Low | MuSig2 P2P | Current rerun registers and extends `musig2_net_processing_tests`, exposing command-registration and reserve-relay regressions. |
| `DD-FA-TEST-059` | Medium | Wallet restore test | Current rerun fixes `wallet_digidollar_restore.py` so mint/state/restore verification errors fail the suite, and the full runner executes it only in descriptor-wallet mode. |
| `DD-FA-TEST-060` | Medium | Encrypted wallet backup | Current rerun adds encrypted DD backup restore coverage that proves locked restore refuses redemption and post-unlock restored wallet can redeem. |
| `DD-FA-TEST-061` | Low | RPC help schema | Current rerun pins runtime help output for DD cent fields as numeric JSON so docs do not drift into DGB amount semantics. |
| `DD-FA-TEST-062` | Medium | MuSig2 fuzz | Current rerun initializes the `musig2_nonce_message` and `musig2_partialsig_message` fuzz targets with regtest chainparams so seeded harness execution no longer aborts on `Params()`. |
| `DD-FA-TEST-063` | Low | Oracle replay cap | Current rerun pins the separate consensus-attestation replay set at 10000 retained hashes. |
| `DD-FA-TEST-064` | Medium | Fuzz runner | Current rerun makes unknown target selections and impossible non-libFuzzer empty-corpus `--empty_min_time` requests fail closed instead of reporting success. |
| `DD-FA-TEST-065` | Low | MuSig2 fuzz | Current rerun adds `oracle_musig2_auth_signature_domain`, proving nonce/partial-sig auth verification succeeds for signed-good messages and fails on epoch/id/payload/signature/wrong-key/cross-chain mutation. |
| `DD-FA-TEST-066` | Low | Oracle P2P | Current rerun pins deprecated `ORACLEBUNDLE` ignore behavior and exact `GETORACLES` epoch boundaries in the live functional P2P test. |
| `DD-FA-TEST-067` | Medium | Fuzz registration | Current rerun registers 10 active fuzz harness sources (`addrdb`, BIP324, Dandelion, Odocrypt, `i2p`, `rbf`) and raises the all-target gate to 247/247. |
| `DD-FA-TEST-068` | Low | Test registration | Current rerun registers the live RH49 OP_RETURN matrix and removes stale source-only DigiDollar bughunt/gui test files. |
| `DD-FA-TEST-069` | Low | Wave 26 oracle proof | Wave 26 strengthens unit/functional compatibility tests to prove DD-touching post-activation blocks use final v0x03 MuSig2 oracle bundles only. |
| `DD-FA-TEST-070` | Low | Functional fixture | Wave 26 isolates the generic abortnode undo-data test from the default DigiDollar stats index. |
| `DD-FA-TEST-071` | Low | Reindex fixture | Wave 26 keeps Wave 14 active-chain reindex parity scoped away from persisted mempool replay. |
| `DD-FA-TEST-072` | Low | Reorg fixture | Wave 26 fixes `wallet_digidollar_reorg.py` stale mempool expectation so reorged-out redeems keep wallet positions reserved while pending and restore active-chain state after the pending spend is gone. |
| `DD-FA-TEST-073` | Low | Functional fixture | Wave 26 disables Dandelion in `mining_getblocktemplate_longpoll.py` so the mempool longpoll wakeup assertion is not masked by stempool embargo under full-suite load. |

#### Documentation (`DD-FA-DOC-###`)

| ID | Severity | Area | Summary |
|----|----------|------|---------|
| `DD-FA-DOC-001` | Medium | Release notes | Release notes lagged source/build version (was rc33 doc / rc34 build); **resolved by this release note**. |
| `DD-FA-DOC-002` | Low | Redemption docs | Stale claims that redemption paths were incomplete/non-V1; corrected. |
| `DD-FA-DOC-004` | Low | Code comment | `musig2_session_mining.h` overstated its role on the miner hot path; corrected. |
| `DD-FA-DOC-006` | Low | Activation explainer | Activation explainer omitted the BIP9 / `IsOracleActive` 1-block off-by-one and the regtest `-digidollaractivationheight` knob; documented. |
| `DD-FA-DOC-008` | Low | Wallet integration | `DIGIDOLLAR_WALLET_INTEGRATION.md` gained a wallet-lifecycle / recovery section. |
| `DD-FA-DOC-011` | Low | Functional test | `feature_oracle_p2p.py` is silently passing despite missing real P2P coverage; documented and superseded. |
| `DD-FA-DOC-012` | Low | Operator guidance | Recurring environmental flakes (`mempool_datacarrier.py`, `feature_coinstatsindex.py`, `p2p_feefilter.py`, `mining_getblocktemplate_longpoll.py`) documented for operators with rerun-in-isolation guidance. |
| `DD-FA-DOC-013` | Low | Build warning | DD-specific compiler warning (unused `tip = nullptr` in `digidollar/health.cpp`) documented and removed. |
| `DD-FA-DOC-014` | Low | CLAUDE.md | CLAUDE.md described non-existent separate `test_wallet_digibyte` binary; corrected (wallet tests link into unified `src/test/test_digibyte`). |
| `DD-FA-DOC-015` | Low | Wave 24 docs refresh | Current rerun refreshes release/final-report/repo-map/operator docs after the Wave 23 target-count and compatibility updates. |
| `DD-FA-DOC-016` | Low | Wallet docs | Architecture docs claimed a legacy-wallet random-key fallback that V1 code does not implement; corrected. |
| `DD-FA-DOC-017` | Low | Final report | Final report and RC34 notes drifted after the Wave 26/current-head volatility fix; refreshed. |
| `DD-FA-DOC-018` | Low | Rerun docs | Codex Wave 0 rerun reconciled stale deleted-test references, report artifact names, and current ledger status. |
| `DD-FA-DOC-019` | Low | Rerun docs | Codex Wave 1/2 rerun refreshed report and release-note drift for volatility, legacy-wallet, and supply-integrity findings. |
| `DD-FA-DOC-020` | Low | Rerun docs | Codex Wave 3 rerun refreshed report and release-note drift for collateral math, txbuilder change, RPC ratio, and fuzz/test expectation findings. |
| `DD-FA-DOC-021` | Low | Rerun docs | Codex Wave 4 rerun refreshed report and release-note drift for collateral burn enforcement, new guard tests, and the 368-entry full functional gate. |
| `DD-FA-DOC-022` | Low | Rerun docs | Codex Wave 5 rerun refreshed report and release-note drift for miner package descendants, locked-wallet redemption info, and the full Wave 5 gate. |
| `DD-FA-DOC-023` | Low | Rerun docs | Codex Wave 6 rerun refreshed report and release-note drift for canonical health price units, RPC collateral-margin fields, RH34 fixture correction, and the full Wave 6 gate. |
| `DD-FA-DOC-024` | Low | Lock-tier docs | Codex Wave 7 rerun refreshed stale lock-tier range/ratio/network/fallback wording across architecture, explainer, repo-map, wallet, presentation, and operator docs. |
| `DD-FA-DOC-025` | Low | Oracle docs | Codex Wave 8 rerun refreshed V1 oracle setup/explainer guidance so production is described as MuSig2 v0x03-only with no v0x01/v0x02 fallback. |
| `DD-FA-DOC-026` | Low | Oracle docs | Codex Wave 9 rerun corrected stale mainnet/testnet roster wording: mainnet has 30 metadata slots with only 0-16 consensus-active, while testnet23 has 17 configured slots and no reserves. |
| `DD-FA-DOC-027` | Low | Operator docs | Codex Wave 10 rerun corrected `stoporacle` / `startoracle` wording so RPC restart is not confused with a full daemon restart that drops in-memory MuSig2 sessions. |
| `DD-FA-DOC-028` | Low | Oracle docs | Codex Wave 11 rerun updated current release wording and clarified oracle price units/ranges for consensus, production feeds, and regtest mock RPCs. |
| `DD-FA-DOC-029` | Low | Testnet docs | Codex Wave 12 rerun marks the historical testnet reset guide as archived and points RC34 operators to current `testnet23` ports/heights. |
| `DD-FA-DOC-030` | Low | Rerun docs | Codex Wave 15 rerun records the pre-activation source-input fix, wallet restore test hardening, and green Wave 15 unit/functional/fuzz gate. |
| `DD-FA-DOC-031` | Low | Rerun docs | Codex Wave 16 rerun records encrypted oracle-key storage, encrypted DD backup restore coverage, key-cache clearing, and the green Wave 16 unit/functional/fuzz gate. |
| `DD-FA-DOC-032` | Low | Rerun docs | Codex Wave 17 rerun records stale DD preset-input rejection, Qt locked-wallet action state, and the green Wave 17 unit/functional/fuzz gate. |
| `DD-FA-DOC-033` | Low | RPC docs/help | Current rerun clarifies `importdigidollaraddress` is a V1 unsupported validation/no-op stub, not watch-only import support. |
| `DD-FA-DOC-034` | Low | Qt docs | Current rerun aligns tier-0 wording so the one-hour DigiDollar tier is canonical on all networks, not a testnet-only tier. |
| `DD-FA-DOC-035` | Low | Qt docs | Current rerun expands DCA/ERR tooltips as Dynamic Collateral Adjustment and Emergency Redemption Ratio. |
| `DD-FA-DOC-036` | Low | Qt docs | Current rerun aligns Qt send invalid-amount text with cents precision and the real send range. |
| `DD-FA-DOC-037` | Low | Oracle roster docs | Current rerun fixes stale testnet reserve-slot wording: mainnet has reserve metadata slots, while testnet23 has only the 17 configured active slots. |
| `DD-FA-DOC-038` | Low | Oracle timing docs | Current rerun updates oracle architecture timing from 15 seconds to the code's 60-second operator price fetch/broadcast loop. |
| `DD-FA-DOC-039` | Low | RPC docs | Current rerun clarifies that `listdigidollarpositions` paging is optional and `count=0` preserves the historical return-all default. |
| `DD-FA-DOC-040` | Low | Oracle P2P docs | Current rerun aligns repo maps/operator guidance with the production P2P anti-abuse location in `src/net_processing.cpp` and the stale-epoch relay window. |
| `DD-FA-DOC-041` | Low | Fuzz evidence docs | Wave 22 reconciled historical marathon evidence at 236 total / 51 DD-oracle-MuSig2 targets with the then-current 237 total / 52 DD-oracle-MuSig2 target gate; Wave 23 supersedes the all-target count with 247 registered targets. |
| `DD-FA-DOC-042` | Low | Wave 23 evidence docs | Current rerun records the 247-target all-fuzz gate, 369-entry functional gate, and LIEF-compatible `check-security` result. |
| `DD-FA-DOC-043` | Low | Wallet/watch-only docs | Current rerun aligns active user docs with the V1 unsupported/no-op `importdigidollaraddress` behavior. |
| `DD-FA-DOC-044` | Low | Oracle docs/scripts | Current rerun removes active-looking v0x02 fallback, mock-testnet, 15-second fetch-loop, and mainnet-bypass claims from current V1 guidance. |
| `DD-FA-DOC-045` | Low | Test/fuzz inventory docs | Current rerun aligns repo maps and historical fuzz docs with the 80 functional-entry and 247 fuzz-target Wave 23 evidence. |
| `DD-FA-DOC-046` | Low | RPC/Qt help docs | Current rerun aligns RPC help schemas, deprecated fee-rate compatibility metadata, and Qt/wallet comments with the implemented fee, amount, tier, and oracle-unit behavior. |
| `DD-FA-DOC-047` | Low | Wave 25 evidence docs | Current rerun records the final exploit-chain sweep, supersedes Red Hornet final reports, and updates RC/final-report status to Wave 25 green / Wave 26 pending at that point. |
| `DD-FA-DOC-048` | Low | Wave 26 evidence docs | Current rerun records the final backward-compatibility/activation proof and refreshes activation docs after the regtest override/cache fixes. |

#### Architecture review (`DD-FA-ARCH-###`) — remaining decisions

These items remain open for end-of-campaign discussion with the project
maintainer per the campaign rule "no architectural redesign during the 25
audit waves":

- `DD-FA-ARCH-001` High — Oracle active roster expansion beyond 17
  remains a protocol decision (governance / activation policy for adding
  or replacing operators). Architecture is in place
  (`nOraclePubkeyCount` / `nOracleTotalOracles` flow through chainparams
  to the validator, and the
  `expanded_roster_accepts_valid_9_sig_bundle_after_activation` test
  exercises `ROSTER_ACTIVE_OPERATORS=20`); the activation-mechanism
  decision is deferred.
- `DD-FA-ARCH-002` Medium — Wallet/RPC position storage assumes
  collateral output index 0 while consensus can validate nonzero
  collateral outputs. Must be resolved or explicitly approved before
  final launch readiness.
- `DD-FA-ARCH-004` High — MuSig2 has no intra-epoch participant reselection
  after a selected signer withholds its partial signature. This is a
  liveness/protocol decision and was not redesigned during the audit waves.
- `DD-FA-ARCH-005` Medium — DD mints are currently valid for one connect
  height because consensus checks canonical lock tiers against remaining
  blocks. Jared must accept one-block mint retry semantics or approve a
  post-audit protocol/wallet lifecycle redesign.

Closed item:

- `DD-FA-ARCH-003` High — Volatility consensus path used wall-clock
  `GetTime()`; closed by `DD-FA-SEC-012` / commit `132e8a82e0`.

---

## Test evidence summary

| Surface | Result | Evidence |
|---------|--------|----------|
| Unit suite | `make check` PASS — full suite green at every wave gate, including the current Codex Wave 26 rerun gate. | `/tmp/dd-fa-rerun-wave26-gate/make-check.log`. |
| Functional suite (standard) | `test/functional/test_runner.py --jobs=4` PASS at every wave gate; current Codex Wave 26 rerun passed 369 entries under `TEST_RUNNER_PORT_MIN=8000`. Four upstream-Bitcoin scripts (`mempool_datacarrier.py`, `feature_coinstatsindex.py`, `p2p_feefilter.py`, `mining_getblocktemplate_longpoll.py`) are classified as environmental flakes per `DD-FA-DOC-012`; none failed in the current Wave 26 rerun. | `/tmp/dd-fa-rerun-wave26-gate/functional.log`. |
| Functional suite (extended / DD/oracle direct) | DD/oracle/wallet direct loops PASS; multi-node oracle P2P scenarios PASS. | Wave 13/14/20 Agent C reports. |
| Fuzz marathon | Historical Wave 22 marathon explored 832,596 seeds across 51 DD/oracle/MuSig2 fuzz targets with no production crashes, sanitizer failures, or OOM. Codex Wave 26 rerun enumerates and passes 247 total registered fuzz targets against the seeded corpus. | `/tmp/dd-fa-rerun-wave26-gate/fuzz-seeded.log`. |
| Build sanity | Configure / Make / `--with-gui` smoke green; no new warnings introduced by DigiDollar surface; `--enable-werror` blocker removed (`DD-FA-FUNC-037`); current LIEF `check-security` compatibility fixed (`DD-FA-FUNC-067`). Wave 26 inherited green `check-symbols`/`check-security` evidence from Wave 23 and reran full unit/functional/fuzz gates. | `/tmp/dd-fa-rerun-wave23-gate/check-symbols-postdevtool.log`, `/tmp/dd-fa-rerun-wave23-gate/check-security-postdevtool-final.log`, `/tmp/dd-fa-rerun-wave26-gate`. |
| Standalone `libdigibyteconsensus.so` | Built; `g_get_oracle_consensus_price` correctly **not** in the dynamic symbol table — `OP_CHECKPRICE` hook is null and fails closed in standalone embedders. | Wave 23 Agent C verification. |
| Live oracle smoke (RC33→RC34 carry) | Oracle slot 15 broadcast prices in the 3831–3842 µUSD range; v0x03 MuSig2 bundles validated with 9-of-17 signatures. | Inherited from RC33 evidence; no oracle key rotation in RC34. |
| Codex rerun Wave 3 gate | `make check` PASS, full functional suite PASS 367/367, all registered fuzz targets PASS 236/236. | `/tmp/dd-fa-rerun-wave03-gate`. |
| Codex rerun Wave 4 gate | `make check` PASS, full functional suite PASS 368/368, all registered fuzz targets PASS 236/236. | `/tmp/dd-fa-rerun-wave04-gate`. |
| Codex rerun Wave 5 gate | `make check` PASS, full functional suite PASS 368/368, all registered fuzz targets PASS 236/236. | `/tmp/dd-fa-rerun-wave05-gate`. |
| Codex rerun Wave 6 gate | `make check` PASS after RH34 fixture correction, full functional suite PASS 368/368, all registered fuzz targets PASS 236/236. | `/tmp/dd-fa-rerun-wave06-gate`. |
| Codex rerun Wave 7 gate | `make check` PASS, full functional suite PASS 368/368, all registered fuzz targets PASS 236/236. | `/tmp/dd-fa-rerun-wave07-gate`. |
| Codex rerun Wave 8 gate | `make check` PASS, full functional suite PASS 368/368 after oracle-keygen status expectation alignment, all registered fuzz targets PASS 238/238. | `/tmp/dd-fa-rerun-wave08-gate-rerun`; targeted logs in `/tmp/dd-fa-rerun-wave08-main`. |
| Codex rerun Wave 9 gate | Signing roster and pending-quorum fixes passed targeted oracle roster/domain unit suites, focused MuSig2 fuzz, `make check`, the full functional suite, and all registered fuzz targets. | `/tmp/dd-fa-rerun-wave09-gate`; targeted unit/fuzz commands are recorded in the local ignored final audit ledger. |
| Codex rerun Wave 10 gate | MuSig2 P2P/session focused unit suites PASS after P2P auth, command-registration, and reserve-relay fixes; `make check`, full functional suite 368/368, and all 236 registered fuzz targets PASS. | `/tmp/dd-fa-rerun-wave10-gate`; targeted commands are recorded in the local ignored final audit ledger. |
| Codex rerun Wave 11 gate | Oracle feed/RPC targeted slices passed after malformed feed parser and v0x03 aggregate signature status fixes; `make check`, full functional suite 368/368, and all 236 registered fuzz targets also PASS. | `/tmp/dd-fa-rerun-wave11-gate`; details are recorded in the local ignored final audit ledger. |
| Codex rerun Wave 12 gate | Activation-boundary fixes passed targeted activation/RPC/Qt slices; full `make check`, full functional suite 368/368, and all 236 registered fuzz targets also PASS. | `/tmp/dd-fa-rerun-wave12-gate`; details are recorded in the local ignored final audit ledger. |
| Codex rerun Wave 17 gate | Wallet/Qt spendability fixes passed RH59 preset-input, Qt locked-wallet, Wave 17 spendability, and encrypted-redeem targeted slices; full `make check`, full functional suite 368/368, and all 236 registered fuzz targets also PASS. | `/tmp/dd-fa-rerun-wave17-gate`; commits `62ab1b36ef`, `886f7fcff5`; full functional evidence uses `TEST_RUNNER_PORT_MIN=8000` to avoid unrelated local port collisions. |
| Codex rerun Wave 18 gate | RPC amount/operator/status fixes passed Wave 18 schema, locked-wallet matrix, oracle keygen, amount filters, display, gating, and staleness slices; full `make check`, full functional suite 368/368, and all 236 registered fuzz targets also PASS. | `/tmp/dd-fa-rerun-wave18-gate`; commits `a6e5e1ef2a`, `2340b845c9`, `49dee7fd49`, `d0dbab0712`, `6eb2b9b873`, `182d7c7b69`; full functional evidence uses `TEST_RUNNER_PORT_MIN=8000`. |
| Codex rerun Wave 19 gate | Qt canonical-tier, acronym, watch-only pending-balance, active-network DD address, amount-bound, and missing-oracle health fixes passed the full Qt binary; full `make check`, full functional suite, and all 236 registered fuzz targets also PASS. | `/tmp/dd-fa-rerun-wave19-gate`; commits `f052ecf88c`, `5013be4780`; targeted Qt logs `/tmp/dd-fa-wave19-tier-green.log` and `/tmp/dd-fa-wave19-qt-green-2.log`; full functional evidence uses `TEST_RUNNER_PORT_MIN=8000`. |
| Codex rerun Wave 20 gate | Oracle P2P restart replay-cache fix and MuSig2 fuzz harness initialization passed targeted P2P/fuzz slices; full `make check`, full functional suite, and all 236 registered fuzz targets also PASS. | `/tmp/dd-fa-rerun-wave20-gate`; commits `8a4a0418b4`, `e3f79abe46`; targeted logs `/tmp/dd-fa-wave20-p2p-pending-green.log`, `/tmp/dd-fa-wave20-functional-green.log`, `/tmp/dd-fa-wave20-nonce-fuzz-green.log`, and `/tmp/dd-fa-wave20-psig-fuzz-green.log`; full functional evidence uses `TEST_RUNNER_PORT_MIN=8000`. |
| Codex rerun Wave 21 gate | MuSig2 stale-epoch P2P relay fix and oracle replay-cap coverage passed targeted DoS/resource unit, functional, and focused fuzz slices; full `make check`, full functional suite, and all 236 registered fuzz targets also PASS. | `/tmp/dd-fa-rerun-wave21-gate`; commits `b65daf9eb7`, `0afbc04389`; targeted logs `/tmp/dd-fa-wave21-musig2-p2p-dos-green.log`, `/tmp/dd-fa-wave21-dos-resource-green.log`, `/tmp/dd-fa-wave21-rh58-green.log`, `/tmp/dd-fa-wave21-rh56-green.log`, and `/tmp/dd-fa-wave21-target-fuzz.log`; full functional evidence uses `TEST_RUNNER_PORT_MIN=8000`, runtime 705 s. |
| Codex rerun Wave 22 gate | Fuzz runner fail-closed behavior, MuSig2 auth-domain fuzz, P2P wire fuzz, and live `ORACLEBUNDLE` / `GETORACLES` boundary tests passed; full `make check`, full functional suite, and all 237 registered fuzz targets also PASS against seeded corpora. | `/tmp/dd-fa-rerun-wave22-gate`; commits `3f08fe7fe5`, `7fd1dd94cf`, `1fecdf91cc`; targeted logs `/tmp/dd-fa-wave22-auth-fuzz-green.log`, `/tmp/dd-fa-wave22-p2p-fuzz-green.log`, `/tmp/dd-fa-wave22-oracle-p2p-boundaries-green.log`; full functional evidence uses `TEST_RUNNER_PORT_MIN=8000`, runtime 665 s. |
| Codex rerun Wave 23 gate | Active fuzz harness registration and current-LIEF security-check compatibility passed; full `make check`, 369-entry functional suite, 247/247 seeded fuzz targets, `check-symbols`, and `check-security` all PASS. | `/tmp/dd-fa-rerun-wave23-gate`; commits `cc7f364a6c`, `84d1f74b7d`; functional runtime 666 s. |
| Codex rerun Wave 24 gate | Documentation, release notes, repo maps, operator guidance, RPC/Qt help metadata, and stale-test inventories reconciled; full `make check`, 369-entry functional suite, and 247/247 seeded fuzz targets all PASS. | `/tmp/dd-fa-rerun-wave24-gate`; commit `5d1ffc63d6`; functional runtime 677 s. |
| Codex rerun Wave 25 gate | Final exploit-chain sweep, wallet pending-redeem fixes, RPC compatibility pin, RH49 registration, and Red Hornet supersession docs passed; full `make check`, 369-entry functional suite, and 247/247 seeded fuzz targets all PASS. | `/tmp/dd-fa-rerun-wave25-gate`; commits `57842c2238`, `61c33961ea`, `8473418ce4`, `64b94d1221`; functional runtime 676 s. |
| Codex rerun Wave 26 gate | Final backward-compatibility/activation proof, regtest activation override, v0x03-only DD-touching block proof, Qt hidden-position polling guard, startup oracle-cache activation parity, generic abortnode isolation, pending-redeem wallet reservation, reorged-out redeem restart reconciliation, chain-scoped reindex parity, mempool-aware wallet reorg fixture, and deterministic GBT longpoll fixture passed; full `make check`, 369-entry functional suite, and 247/247 seeded fuzz targets all PASS. | `/tmp/dd-fa-rerun-wave26-gate`; commits `eea2f22b33`, `e9d10e782c`, `e4a3a4eb8d`, `38816e0dbc`, `9c95b3cd2a`, `0606b02c67`, `eb2290f1ba`, `820c42ce45`, `d7d7dd7f2e`, `bd2532ab1e`. |

Known exploratory limitation, **not** a fixed RC34 vulnerability:

- `digidollar_basic.py --legacy-wallet` cannot generate a bech32m
  DigiDollar address. Legacy wallets remain explicitly unsupported for
  DigiDollar mint/receive; descriptor wallets are the tested DD wallet
  path.

---

## Unsupported boundaries (consensus, wallet, watch-only, legacy)

These boundaries are intentional and must be preserved by integrators
and operators:

| Boundary | Rule | Enforced where |
|----------|------|----------------|
| Watch-only DD | Display/monitor only. Cannot mint, redeem, send, or sign. Locked encrypted wallets are *not* spendable. | `mintdigidollar`/`senddigidollar`/`sendmanydigidollar`/`redeemdigidollar` reject watch-only/locked; `listdigidollarpositions` reports `spendable=false`/`can_redeem=false` (`DD-FA-FUNC-022`). Qt mint widget rejects watch-only at the WalletModel layer (`DD-FA-FUNC-031`), and the Qt positions tab shows locked encrypted matured vaults as disabled `Wallet Locked` (`DD-FA-FUNC-057`). |
| DD address import | V1 does not support watch-only DD address import. `importdigidollaraddress` validates the address and returns an unsupported/no-op warning without wallet mutation or rescan. | `DD-FA-DOC-033`; RPC help and repo maps updated. |
| Legacy (non-descriptor / non-bech32m) wallets | Cannot create or use DigiDollar receive/mint addresses. Will fail with a clear descriptor/bech32m HD-wallet requirement. | `CWallet::GetHDKeyForDigiDollar` rejects non-descriptor wallets before legacy key extraction; pinned by `digidollar_wallet_hd_tests`. |
| Cross-chain DD addresses | DD addresses with the wrong chain prefix (`DD` mainnet, `TD` testnet, `RD` regtest) are rejected. | `validateddaddress`, `CDigiDollarAddress`. |
| Whitespace-padded DD addresses | Rejected at the address constructor — copy-paste with stray whitespace cannot silently succeed. | `DD-FA-FUNC-019`. |
| Custom lock-tier durations | Consensus rejects with `bad-mint-lock-period`, `bad-mint-lock-tier`, or `bad-mint-lock-tier-duration`. Only canonical tiers 0..9 allowed. | `src/digidollar/validation.cpp`; reinforced by `DD-FA-SEC-011` IBD enforcement. |
| Non-marker collateral spends | Rejected with `bad-collateral-spend-missing-dd-burn`. | `DD-FA-SEC-001`/`002`/`005`. |
| Partial DD burn on collateral release | Rejected with `bad-collateral-release-partial-burn`. | `ValidateCollateralReleaseAmount`. |
| Mempool DD txs without recent valid MuSig2 oracle quote | Rejected via `HasRecentValidMuSig2OracleQuote`. | Mempool gate (`src/validation.cpp:154-217, 905-916`). |
| DD-touching block without exactly one v0x03 MuSig2 oracle bundle | Rejected (`bad-oracle-missing` / `bad-oracle-multiple-outputs` / `bad-oracle-malformed` / `bad-oracle-legacy`). | `OracleDataValidator::ValidateBlockOracleData`. |
| Cross-chain MuSig2 bundle replay | Rejected — bundle hash is bound to `params.hashGenesisBlock`. | `DD-FA-SEC-008`. |
| `OP_CHECKPRICE` in standalone `libdigibyteconsensus.so` | Hook is null; pushes `vchFalse` regardless of operand — fails closed. | Wave 23 Agent C verification. |

---

## Operator upgrade notes

RC34 is a drop-in binary replacement on `testnet23`.

1. Stop the RC33 node or Qt wallet.
2. Install the RC34 binary.
3. Start with the same data directory and wallet.
4. Keep `txindex=1` enabled for DD nodes.
5. Oracle operators: load/unlock the same oracle wallet and run
   `startoracle <your_oracle_id>` if auto-start does not resume.

No chain reset or wallet migration is required. No oracle slot changes.

Manual oracle start if needed:

```bash
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

Encrypted wallet operators should unlock first:

```bash
digibyte-cli -testnet -rpcwallet=oracle walletpassphrase "your passphrase" 600
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

### New operator-facing behaviour to know

- **`createoraclekey` is stricter.** Negative `oracle_id` and any id
  missing from `Params().GetOracleNode()` (e.g. regtest slots 7..29 that
  do not exist) are now rejected up front. If you previously persisted
  an unusable wallet key for a slot that `startoracle` then refused, you
  may delete it from the wallet.
- **`listdigidollarpositions` has optional paging.** New `count` and
  `skip` parameters let operators bound each response. The default
  `count=0` preserves the historical "return all matching positions"
  behavior for compatibility.
- **`listdigidollaraddresses` hides empty addresses by default.** Pass
  `include_empty=true` to recover the previous behavior.
- **`listdigidollarpositions` on a locked encrypted wallet** now reports
  `spendable=false`/`can_redeem=false` and emits a DD-flavored
  `walletpassphrase` hint. Unlock the wallet to resume normal display.
- **`validateddaddress`** now exposes `solvable` (matches
  `validateaddress`) and rejects whitespace anywhere in the input.
- **Qt redeem widget** now shows the human-readable lock-tier label
  ("30 days", "10 years") instead of `Tier N`.
- **Qt positions widget** shows a watch-only badge for watch-only
  positions.
- **Locked-wallet write RPCs** (`mintdigidollar`, `senddigidollar`,
  `sendmanydigidollar`, `redeemdigidollar`, `getdigidollaraddress`,
  `createoraclekey`) emit DD-flavored hints instead of the generic
  upstream `walletpassphrase` text. The legacy `walletpassphrase`
  substring is preserved for parity with `digidollar_encrypted_wallet.py`.

### Operator full-suite flake guidance (`DD-FA-DOC-012`)

Four upstream-Bitcoin functional tests are known to flake under heavy
parallel load on busy CI / 32-core boxes. None represent DigiDollar /
oracle / wallet / consensus regressions. If a full-suite run hits one
of these names, **rerun the script in isolation** before treating it
as a launch blocker:

| Script | Class | Standalone PASS time |
|--------|-------|----------------------|
| `mempool_datacarrier.py` | RPC port-bind race during 4-node init | ~5 s |
| `feature_coinstatsindex.py` | Reindex stall under disk/scheduler contention | ~5 s |
| `p2p_feefilter.py` | P2P-handshake race after `restart_node(... -blocksonly)` | ~9 s |
| `mining_getblocktemplate_longpoll.py` | Longpoll mempool-probe vs 80-s budget | ~70 s (test self-documents this baseline) |

Lower parallelism to `--jobs=2` on busy CI boxes if these are recurring.

---

## Mainnet activation reminder

Per `src/kernel/chainparams.cpp`, the mainnet BIP9 deployment for
DigiDollar (bit 23) starts at epoch `1777593600` (2026-05-01) with
minimum activation height `22014720`, a 40,320-block (~1 week) window,
and a 70% threshold (28,224 of 40,320). RC34 remains **TESTNET-ONLY**;
the fresh Codex rerun Wave 26 backward-compatibility/activation proof is
green, and mainnet launch remains
gated by the launch-readiness checklist in
`final_audit.MD` Section 11 plus explicit Jared decisions on the
remaining architecture items.

| Network | Start | Min activation height | Window | Threshold | Status |
|---------|-------|----------------------|--------|-----------|--------|
| Mainnet | 2026-05-01 (epoch 1777593600) | 22,014,720 | 40,320 blocks (~1 week) | 70% | Pending |
| Testnet (`testnet23`) | Already past genesis | 600 | 200 blocks | 70% | Active |
| Regtest | ALWAYS_ACTIVE | 0 | 144 blocks | 75% | Active |

---

## Network Information

| Setting | Value |
|---------|-------|
| Network | Testnet (`testnet23`) |
| Genesis Hash | `0xa19e809bb060f7f50c05a9bec7fdefedd8497aa0bd6ccca6f55c86090963e4ca` |
| Network Magic | `fd d2 b9 e4` |
| Default P2P Port | **12030** |
| Default RPC Port | **14026** |
| Oracle Consensus | **9-of-17** |
| Oracle Bundle Format | **MuSig2 aggregate signing (`v0x03`, 88-byte data on the 17-oracle roster)** |
| Exchange Sources | 6 (Binance, CoinGecko, KuCoin, Gate.io, HTX, Crypto.com) |

---

## Configuration

```ini
testnet=1

[test]
digidollar=1
txindex=1
algo=scrypt
addnode=oracle1.digibyte.io
```

> **Note:** `txindex=1` is enforced at startup for DD-enabled nodes.
> Make sure it is in the correct section (`[test]` for testnet, `[main]`
> for mainnet). Global placement above all sections also works.

### Scrypt CPU mining against local Qt/node

For local testnet scrypt mining, both the node and miner must agree on
scrypt:

```ini
[test]
algo=scrypt
```

Start the miner with the scrypt flag explicitly:

```bash
cd /path/to/cpuminer
./minerd -c dgb-solo-mining.json -a scrypt -t 12
```

Healthy mining output includes:

- `12 miner threads started, using 'scrypt' algorithm.`
- `Long-polling activated for http://127.0.0.1:14026/`
- no repeated HTTP 401/500 errors.

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc34-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc34-win64.zip` |
| macOS Apple Silicon | `digibyte-9.26.0-rc34-arm64-apple-darwin.dmg` |
| macOS Intel | `digibyte-9.26.0-rc34-x86_64-apple-darwin.dmg` |
| Linux x86_64 | `digibyte-9.26.0-rc34-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc34-aarch64-linux-gnu.tar.gz` |

Binaries attached to the GitHub release once Guix builds complete and
are verified.

---

## Troubleshooting

### "My oracle did not start automatically"

Load and unlock the oracle wallet, then start manually:

```bash
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle walletpassphrase "your passphrase" 600
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

### "createoraclekey now refuses an oracle_id I used to use"

Intentional. RC34 rejects negative `oracle_id` and any id outside the active
consensus roster. On regtest only slots 0..6 are valid. On testnet23 only
slots 0..16 are configured. On mainnet, slots 17..29 are reserve
`vOracleNodes` metadata only; they are not in `consensus.vOraclePublicKeys`
and cannot participate in V1 quorum.

### "How do I page through listdigidollarpositions?"

Only calls that pass a positive `count` are paged. RC34 added optional
`count` and `skip` parameters; omitting `count`, or passing `count=0`,
keeps the historical return-all behavior.

### "validateddaddress refuses an address I just copy-pasted"

Likely whitespace. RC34 rejects DD addresses with any whitespace
anywhere in the input (a previous loss-of-funds-confusion path because
`DecodeBase58Check` transparently stripped whitespace before validation).
Trim the input and retry.

### "Locked encrypted wallet shows spendable=false on listdigidollarpositions or getredemptioninfo"

Intentional. RC34 honors locked-wallet state in
`listdigidollarpositions` and `getredemptioninfo` so users do not see
deceptive `spendable=true` or `can_redeem=true` on a locked wallet.
Unlock with `walletpassphrase` to resume normal display and signing.

### "Qt shows Wallet Locked on a matured DigiDollar vault"

Intentional. The vault may be timelock-matured, but a locked encrypted
wallet cannot sign the redemption. Unlock the wallet first; watch-only
wallets remain display-only.

### "estimatecollateral shows two collateral amounts"

Intentional. `required_dgb` and `minimum_required_dgb` are the consensus
minimum. `wallet_collateral_dgb` is the amount the wallet mint builder
will lock after its 1% safety margin, and
`collateral_safety_margin_dgb` is the difference.

### "A full-suite functional run hit one of the four documented flakes"

Not a launch blocker. See **Operator full-suite flake guidance**
(`DD-FA-DOC-012`) above — rerun the affected script in isolation. All
four scripts pass cleanly standalone on the current
`feature/digidollar-v1` working tree.

On busy developer workstations, if a full runner attempt times out during
RPC startup because a test port collides with an ephemeral/local listener,
rerun the full suite with an isolated low test range, for example
`TEST_RUNNER_PORT_MIN=8000 test/functional/test_runner.py --jobs=4`.

### "A legacy wallet cannot create a DigiDollar receive address"

Intentional. Legacy wallet support for DigiDollar bech32m receive/mint
remains a product decision. Descriptor wallets remain the tested DD
wallet path.

### "Node refuses to start: oracle roster alignment mismatch"

Intentional guard. It means local chainparams have inconsistent oracle
public-key/roster data. Run an official RC34 binary and do not hand-edit
chainparams.

---

## Architecture-review items deferred to end-of-campaign review

The following items were classified `ARCHITECTURAL_REVIEW_REQUIRED`
during the audit and are **not** implemented in RC34. They require
explicit approval before final launch readiness:

- `DD-FA-ARCH-001` — Oracle active roster expansion beyond 17
  (governance/activation mechanism for adding/replacing operators).
- `DD-FA-ARCH-002` — Wallet/RPC position storage assumes collateral
  output index 0 while consensus validates nonzero collateral outputs.
- `DD-FA-ARCH-004` — MuSig2 has no intra-epoch participant reselection
  after a selected signer withholds its partial signature.
- `DD-FA-ARCH-005` — DD mints are intentionally valid for one connect
  height because canonical lock tiers are checked against remaining blocks.

`DD-FA-ARCH-003` is no longer in this queue. It was closed by
`DD-FA-SEC-012` / commit `132e8a82e0`.

Red Hornet-era crosswalk:
`DD-RH-037`/`DD-RH-084` map to `DD-FA-ARCH-001`;
`DD-RH-094` maps to `DD-FA-ARCH-004`; `DD-RH-085` is closed by V1
MuSig2-only validation; `DD-RH-097` is closed by `DD-FA-SEC-008`,
`DD-FA-SEC-019`, and Wave 22 auth-domain fuzz; `LEGACY-DD-WALLET` is
closed as the explicit unsupported V1 legacy-wallet boundary.

---

## Feedback & Community

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
