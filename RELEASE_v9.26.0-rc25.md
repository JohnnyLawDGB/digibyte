# DigiByte v9.26.0-rc25 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## ⚠️ SAME TESTNET — NO RESET

**RC25 uses the same testnet19 chain as RC19–RC24.** No data migration needed.

- **No chain reset** — your blockchain data, wallets, and oracle keys all carry over
- **Same ports:** P2P **12033**, RPC **14025**
- **Same oracle consensus:** **5-of-9** Schnorr threshold
- **Just update the binary** and restart your node

### 🔑 Oracle Operators — NEW: Auto-Start!

**RC25 eliminates the #1 operator complaint: you no longer need to manually restart your oracle after every node restart.**

- **Unencrypted wallets:** Oracle starts automatically when the wallet loads — zero manual steps.
- **Encrypted wallets:** Oracle starts automatically after you unlock with `walletpassphrase`. A log message on startup tells you exactly what to do.

If auto-start doesn't activate (e.g., changed wallet files), you can still manually start:

```bash
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

Qt users: **File → Open Wallet → oracle**, then **Help → Debug Window → Console** → `startoracle <your_oracle_id>`

---

## What's New in RC25

RC25 is a **stability and reliability release** targeting five bugs reported by testnet operators, plus two community-contributed fixes for Dandelion++ and UTXO management. The headline fix is Bug #16 — a critical block production halt that could stop the entire chain.

**If you experienced block production stalls, wallet rescan errors, oracle restart issues, or UTXO fragmentation preventing mints, upgrade to RC25.**

---

## 🐛 Bug Fix #1: Block Production Halt from Stale DD Transactions (Bug #16 — Critical)

**The problem:** When a DigiDollar mint transaction sat in the mempool while the oracle price changed, `TestBlockValidity()` would fail with `insufficient-collateral` and throw a fatal `std::runtime_error` — halting block production entirely. No recovery was possible without restarting the node and manually clearing the mempool.

**Root cause:** Three-stage failure:
1. `addPackageTxs()` included DD transactions with zero DD-specific validation
2. `TestBlockValidity()` re-validated with a potentially different oracle price
3. Failure threw `std::runtime_error` — no recovery, no retry, no skip

**Fix:** Three-part approach:
- **Part A:** DD transactions are now pre-validated against the current oracle price inside `addPackageTxs()` before block inclusion. Invalid DD transactions are skipped (added to `failedTx`) instead of included.
- **Part B:** `TestBlockValidity()` now catches `insufficient-collateral` failures, removes the offending DD transactions, and retries block template creation.
- **Part C:** Mints now include a 1% collateral safety margin at creation time, preventing knife-edge price changes from invalidating transactions.

**Effect:** Block production is now resilient to oracle price changes between mint and block inclusion. The chain can no longer be halted by a stale DD transaction.

---

## 🐛 Bug Fix #2: Rescan False Positive on Matured Mints (Bug #22)

**The problem:** Running a wallet rescan or revalidation on 3+ testnet nodes triggered `bad-mint-lock-height-mismatch` errors for historical mint transactions, blocking rescan completion.

**Root cause:** The lock-height validation compared `lockTime` against `ctx.nHeight` (the current chain tip during rescan), not the block height when the mint was originally confirmed. For any mint whose lock period had already expired — e.g., `lockTime=104041` vs `currentHeight=123779` — the subtraction produced a negative result, triggering a false rejection.

**Fix:** Added a maturity check before the lock-height comparison. If `lockTime <= currentHeight`, the lock has already expired/matured — the tier consistency check is skipped entirely, since it's only relevant at acceptance time.

**Effect:** Wallet rescans and chain revalidations complete successfully for all historical mints.

---

## 🐛 Bug Fix #3: txindex Not Enforced for DD-Enabled Nodes (Bug #21)

**The problem:** DigiDollar requires `txindex=1` for correct operation, but nodes could start without it and then fail unpredictably during reindex or initial block download. Some operators had `txindex=1` in their config but placed it in the wrong section (e.g., global instead of `[test]`), causing it to be ignored for testnet.

**Root cause:** No startup check verified that txindex was enabled when DigiDollar was active. The block-db fallback path (`ExtractDDAmountFromBlockDb`) exists but can fail for certain transaction formats on some platforms.

**Fix:** Startup now checks for `txindex=1` when DigiDollar is enabled. If missing, the node exits with a clear error message:
```
Error: DigiDollar requires -txindex=1. Please restart with -txindex=1 or add txindex=1 to your config.
```
Documentation added for correct config section placement (`[test]` for testnet, `[main]` for mainnet).

**Effect:** No more silent failures from missing txindex. Clear guidance for operators.

---

## 🐛 Bug Fix #4: Oracle Keys Not Loading After Restart (Bug #2)

**The problem:** After every node restart, oracle operators had to manually run `loadwallet "oracle"` then `startoracle <id>` before their oracle would begin submitting prices. If they forgot (or the node crashed overnight), their oracle was silently offline until the next manual intervention.

**Root cause:** `startoracle` calls `EnsureWalletIsUnlocked()` before `GetOracleKey()`. After restart with an encrypted wallet, the wallet is locked — the oracle can't read its key. No auto-start hook existed in the wallet load or unlock paths.

**Fix:** Two paths, respecting wallet security:
- **Unencrypted wallets:** After wallet load completes, `TryAutoStartOracles()` scans for stored oracle keys and auto-starts any that aren't already running. No security risk — unencrypted wallets have keys in plaintext already.
- **Encrypted wallets:** The `walletpassphrase` RPC success path now calls `TryAutoStartOracles()`. After manual unlock, oracles auto-start. On startup, a log message guides operators: `"Oracle: Key stored for oracle X but wallet is locked. Run 'walletpassphrase' to enable oracle operation."`

**⚠️ Security:** Encrypted wallets are NEVER auto-unlocked. That would defeat the purpose of encryption.

**Effect:** Unencrypted wallet operators have zero-touch oracle restart. Encrypted wallet operators get clear guidance and auto-start after unlock.

---

## 🐛 Bug Fix #5: Attestation Quorum Timing Failures (Bug #4)

**The problem:** Block templates could be built before the oracle attestation quorum formed, resulting in zero-price blocks and Emergency status on the network. This was most common during rapid block production or when one oracle was slightly slower than the others.

**Root cause:** `AddOracleBundleToBlock()` checked for quorum exactly once. If the quorum wasn't formed at that instant — even if 4 of 5 required oracles had already reported — the block template was built without an oracle bundle.

**Fix:** A bounded wait window now pauses block template creation for up to 2 seconds when the bundle has ≥ `min_oracle_count - 1` attestations (i.e., quorum is one oracle away). If the final attestation arrives within the window, the block includes the valid bundle. If the timeout expires, it falls back to the previous behavior. Configurable via `-oraclequorumwaitms` (default: 2000ms) and `-oraclequorumminpct` (default: 80%).

**Effect:** Near-quorum situations now resolve correctly instead of producing zero-price blocks.

---

## 🔧 Community Contributions

### Dandelion Stempool Ancestor Leak (PR #392 — JohnnyLawDGB)

**The problem:** When a block was connected, `removeForBlock()` was called on the mempool but not the Dandelion stempool. Confirmed transactions remained in the stempool indefinitely as phantom ancestors, inflating ancestor counts for new transactions until they hit the 25-ancestor limit and were rejected.

**Fix:** `removeForBlock()` now also purges confirmed transactions from the Dandelion stempool on block connect.

### UTXO Fragmentation Blocks Minting (PR #391 — JohnnyLawDGB)

**The problem:** Wallets with many small UTXOs (>400) couldn't mint DigiDollar because no single transaction could cover the collateral requirement — the input set exceeded transaction limits.

**Fix:** `mintdigidollar` now detects fragmented wallets and automatically creates a consolidation transaction using standard coin selection, then retries the mint with the consolidated output.

---

## 🧪 Testing

### New Tests Added in RC25

| Test | What It Verifies |
|------|-----------------|
| `block_with_stale_dd_mint_skips_gracefully` | Stale DD mints skipped during block template creation |
| `block_without_dd_succeeds_after_dd_failure` | Non-DD txs produce valid block after DD removal |
| `mint_includes_safety_margin` | Mint collateral includes 1% safety margin |
| `test_block_validity_retry_on_collateral_failure` | TestBlockValidity retries without DD tx on failure |
| `dd_and_large_dgb_transfer_coexist` | DD mint + large DGB transfer → valid block |
| `rescan_mature_mint_passes` | Matured mint passes validation during rescan |
| `rescan_immature_mint_passes` | Immature mint within lock period passes |
| `fresh_mint_tier_mismatch_fails` | Fresh mint with wrong tier is rejected |
| `dd_startup_requires_txindex` | DD-enabled node without txindex → startup error |
| `dd_startup_with_txindex_succeeds` | DD-enabled node with txindex → starts normally |
| `oracle_autostart_unencrypted_wallet` | Unencrypted wallet → oracle auto-starts on load |
| `oracle_no_autostart_locked_encrypted_wallet` | Encrypted+locked → oracle not started, guidance logged |
| `oracle_autostart_after_walletpassphrase` | Encrypted → unlock → oracle auto-starts |
| `bundle_waits_for_near_quorum` | 4/5 oracles → wait → 5th arrives → bundle created |
| `bundle_gives_up_after_timeout` | 3/5 oracles → timeout → no bundle |
| `bundle_immediate_when_quorum_met` | 5/5 oracles → no wait, immediate bundle |

### Test Results
- **C++ unit tests** — all passing (25+ new tests for this release)
- **DigiDollar functional tests** — all passing
- **Live testnet verification** — all DigiDollar/Oracle RPCs verified on testnet19

---

## 📊 All Changes: RC24 → RC25

| Category | Count | Summary |
|----------|-------|---------|
| 🐛 Bug fixes | 5 | Block halt, rescan validation, txindex, oracle auto-start, quorum timing |
| 🔧 Community | 2 | Dandelion stempool, UTXO consolidation |
| 🧪 Tests | 16+ | Block template, rescan, txindex, oracle lifecycle, quorum timing |
| 📦 Version | 1 | Bump to v9.26.0-rc25, update wallet image |

---

## Technical Changes

| File | Change |
|------|--------|
| `src/node/miner.cpp` | DD pre-validation in `addPackageTxs()`, retry logic in `CreateNewBlock()` |
| `src/node/miner.h` | DD block validation helpers |
| `src/digidollar/validation.cpp` | Maturity check before lock-height comparison |
| `src/digidollar/validation.h` | Maturity check declaration |
| `src/init.cpp` | txindex enforcement for DD-enabled chains |
| `src/init.h` | txindex check declaration |
| `src/wallet/wallet.cpp` | `TryAutoStartOracles()` on wallet load |
| `src/wallet/wallet.h` | Auto-start declaration |
| `src/wallet/walletdb.cpp` | Oracle key metadata storage |
| `src/wallet/walletdb.h` | Oracle key metadata declaration |
| `src/wallet/rpc/encrypt.cpp` | Hook `TryAutoStartOracles()` into `walletpassphrase` |
| `src/rpc/digidollar.cpp` | Refactored oracle start logic into reusable function |
| `src/oracle/bundle_manager.cpp` | Bounded wait window for near-quorum bundles |
| `src/oracle/bundle_manager.h` | Wait window configuration |
| `src/digidollar/txbuilder.cpp` | 1% collateral safety margin |
| `src/test/digidollar_lock_height_tests.cpp` | 4 rescan maturity tests |
| `src/test/miner_dd_validation_tests.cpp` | 5 block template DD validation tests |
| `src/test/digidollar_txindex_tests.cpp` | 3 txindex enforcement tests |
| `src/test/oracle_wallet_autostart_tests.cpp` | 5 oracle auto-start lifecycle tests |
| `src/test/oracle_bundle_timing_tests.cpp` | 4 quorum wait window tests |
| `src/test/digidollar_mint_tests.cpp` | Updated for safety margin |
| `configure.ac` | Version bump RC24 → RC25 |
| `src/qt/res/icons/digibyte_wallet.png` | Updated wallet splash image |

---

## Commits Since RC24

```
fd9135af23 fix: add bounded wait window for near-quorum oracle bundles (Bug #4)
a927e2e556 fix: auto-start oracles from wallet keys on load/unlock (Bug #2)
77a4c584ae fix: enforce txindex=1 for DigiDollar-enabled chains at startup (Bug #21)
ff2d8d5b17 fix: prevent block production halt from stale DD transactions (Bug #16)
641e0f8698 fix: skip tier consistency check for matured mints during rescan (Bug #22)
2aed101e68 Merge pull request #392 from JohnnyLawDGB/fix/dandelion-stempool-ancestor-leak
eb07fb7244 Merge pull request #391 from JohnnyLawDGB/fix/utxo-consolidation-mint
13f98b4a63 fix: purge confirmed txs from Dandelion stempool on block connect
41e3f66969 fix: auto-consolidate fragmented UTXOs when minting DigiDollar
```

---

## What is DigiDollar?

DigiDollar is a USD-pegged stablecoin built natively into DigiByte. It uses an over-collateralized model where users lock DGB to mint DUSD at the current oracle price of DGB.

The world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

DGB becomes the strategic reserve asset (21B max, only ~1.94 DGB per person on Earth). Everything happens inside DigiByte Core wallet. You never give up custody of your private keys. No centralized company, fund or pool. Pure decentralization.

**Learn more:** https://digibyte.io/digidollar

---

## Oracle Operator Setup

### Upgrading from RC24

```bash
digibyte-cli -testnet stop
# Replace binary
digibyted -testnet -daemon
# Oracle auto-starts from wallet! No manual steps needed for unencrypted wallets.
# Encrypted wallets: run walletpassphrase to trigger auto-start.
```

### Upgrading from RC23 or Earlier

```bash
digibyte-cli -testnet stop
# Replace binary
digibyted -testnet -daemon
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
# After this first manual start, future restarts will auto-start.
```

### New Oracle Setup

```bash
digibyted -testnet -daemon
digibyte-cli -testnet createwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle createoraclekey <your_oracle_id>
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
# Future restarts will auto-start your oracle.
```

For the complete guide including RC18-and-earlier migration, see **`DIGIDOLLAR_ORACLE_SETUP.md`**.

### Current Oracle Operators (Testnet)

| ID | Operator | Status |
|----|----------|--------|
| 0 | Jared | ✅ Active |
| 1 | Green Candle | ✅ Active |
| 2 | Bastian | ✅ Active |
| 3 | DanGB | ✅ Active |
| 4 | Shenger | ✅ Active |
| 5 | Ycagel | ✅ Active |
| 6 | Aussie Epic | ✅ Active |
| 7 | LookIntoMyEyes | ✅ Active |
| 8 | JohnnyLawDGB | ✅ Active |

---

## Complete RPC Command Reference

### DigiDollar Commands (Wallet)

| Command | Description |
|---------|-------------|
| `mintdigidollar` | Mint DigiDollars by locking DGB as collateral |
| `senddigidollar` | Send DigiDollars to another address |
| `redeemdigidollar` | Redeem DigiDollars to unlock DGB collateral |
| `getdigidollarbalance` | Show your DigiDollar balance |
| `listdigidollarpositions` | List your active collateral positions |
| `listdigidollartxs` | List your DigiDollar transaction history |
| `getdigidollaraddress` | Get or create a DigiDollar receive address |
| `validateddaddress` | Validate a DigiDollar address |
| `listdigidollaraddresses` | List all DigiDollar addresses in your wallet |
| `importdigidollaraddress` | Import a DigiDollar address for watch-only |
| `getdigidollarstats` | Get network-wide DigiDollar statistics |
| `getdigidollardeploymentinfo` | Get DigiDollar activation/deployment status |
| `calculatecollateralrequirement` | Calculate DGB collateral needed for a DD mint |
| `estimatecollateral` | Estimate collateral requirement by tier |
| `getdcamultiplier` | Get the current DCA multiplier for collateral |
| `getredemptioninfo` | Get info about redeeming a specific position |
| `getprotectionstatus` | Check if liquidation protection is active |

### Oracle Commands

| Command | Description |
|---------|-------------|
| `createoraclekey <id>` | Generate oracle Schnorr keypair (one-time) |
| `getoraclepubkey <id>` | Show oracle public key from wallet |
| `startoracle <id>` | Start running as an oracle operator |
| `stoporacle <id>` | Stop your oracle |
| `getoracleprice` | Get the consensus price |
| `getalloracleprices` | Per-oracle price breakdown |
| `getoracles` | Network-wide oracle status |
| `listoracle` | Show local oracle status |
| `sendoracleprice` | Manually submit a price (testing) |

---

## Configuration

```ini
testnet=1

[test]
digidollar=1
txindex=1
addnode=oracle1.digibyte.io
```

> **⚠️ New in RC25:** `txindex=1` is now enforced at startup for DD-enabled nodes. Make sure it's in the correct section (`[test]` for testnet, `[main]` for mainnet). Global placement (above all sections) also works.

---

## Known Issues

- DigiDollar features disabled until BIP9 activation (~block 600 with continuous mining)
- Oracle prices show as 0 until sufficient operators restart after upgrading
- Stale redeem buttons for already-redeemed positions when restoring via descriptor import (cosmetic)

---

## Network Information

| Setting | Value |
|---------|-------|
| Network | Testnet (testnet19) |
| Default P2P Port | **12033** |
| Default RPC Port | **14025** |
| Oracle Node | oracle1.digibyte.io |
| Oracle Consensus | **5-of-9** Schnorr threshold |
| Exchange Sources | 6 (Binance, CoinGecko, KuCoin, Gate.io, HTX, Crypto.com) |

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc25-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc25-win64.zip` |
| macOS Apple Silicon | `digibyte-9.26.0-rc25-arm64-apple-darwin.dmg` |
| macOS Intel | `digibyte-9.26.0-rc25-x86_64-apple-darwin.dmg` |
| Linux x86_64 | `digibyte-9.26.0-rc25-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc25-aarch64-linux-gnu.tar.gz` |

---

## Troubleshooting

### "Block production halted with insufficient-collateral" (FIXED in RC25)
Stale DD transactions in the mempool could crash `CreateNewBlock()`. RC25 pre-validates DD transactions and retries without them on failure.

### "bad-mint-lock-height-mismatch during rescan" (FIXED in RC25)
Historical mints with expired lock heights triggered false validation errors. RC25 skips tier checks for matured mints.

### "Node fails silently without txindex" (FIXED in RC25)
Startup now requires `txindex=1` for DD-enabled nodes with a clear error message.

### "startoracle fails after restart" (FIXED in RC25)
Oracle keys now auto-start from the wallet. Unencrypted wallets need zero manual steps; encrypted wallets auto-start after `walletpassphrase`.

### "Zero-price blocks during rapid block production" (FIXED in RC25)
Near-quorum attestation bundles now wait up to 2 seconds for the final oracle before giving up.

### "Insufficient fee inputs for calculated fee" on redemption (FIXED in RC24)
### "listdigidollaraddresses returns mock data" (FIXED in RC24)
### "getredemptioninfo shows same data for every position" (FIXED in RC24)
### "startoracle fails with 'Oracle not configured' after restart" (FIXED in RC23)
### "Oracle consensus stalled and never recovered" (FIXED in RC23)
### "Oracle prices stale or missing from blocks" (FIXED in RC22)
### "Sync stuck at block 7586" (FIXED in RC21)

---

## Feedback & Community

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues

When reporting bugs include: what happened, steps to reproduce, platform, and error messages from debug.log.
