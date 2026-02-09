# DigiByte v9.26.0-rc15 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## What's New in RC15

### 🔴 Critical Security Fix

- **Collateral Theft Prevention (CVE-grade)** — Fixed a critical vulnerability where the owner of a DigiDollar could bypass CLTV timelocks on their collateral and withdraw locked DGB immediately after minting, creating unbacked stablecoins. The collateral Taproot output now uses a NUMS (Nothing Up My Sleeve) point as the internal key, forcing all spends through the MAST script-path where timelocks are enforced. This is a consensus-level fix.

### 🔒 Security Hardening (5 fixes)

- **Fake Oracle Price Injection Blocked** — The P2P ORACLEPRICE handler was verifying Schnorr signatures against the pubkey embedded in the message itself, not against authorized keys from chainparams. An attacker could craft fake oracle messages with arbitrary prices. Now all oracle messages are verified against the authorized keys hardcoded in the software before acceptance.

- **TLS Security Hardened** — Removed a dangerous fallback in the exchange price fetcher that disabled SSL verification entirely when no CA bundle was found. Also removed retry-without-TLS on curl failures. Added curl hardening (redirect limits, HTTPS-only, connect timeout, max file size, NaN/infinity guards).

- **Collateral Protected from Accidental Unlocking** — `UnlockAllCoins()` and the `lockunspent` RPC could accidentally unlock DigiDollar collateral, allowing it to be spent while DigiDollars backed by it still exist. DD-locked outpoints are now preserved.

- **Validation Tightened** — Three code paths in validation.cpp silently bypassed critical validation when txindex or CCoinsViewCache was unavailable (transfer conservation, redemption burn check, collateral release). All three now reject the transaction instead of assuming validity.

- **Reorg Protection** — When a blockchain reorganization disconnected a block containing a DigiDollar redemption, the collateral UTXO was unlocked but never re-locked, creating a window to spend collateral before the redemption re-confirms. Now re-locks immediately on block disconnection.

### 🐛 Bug Fixes

- **Wallet Rescan Now Detects Self-Minted DigiDollars** — Fixed a bug where deleting testnet13 and resyncing (or loading a wallet after sync) caused all self-minted DigiDollars to disappear from the balance. The mint rescan path used `IsMine()` which fails on 0-value P2TR DD token outputs. Now falls back to `IsDDOutputMine()` which correctly identifies DD ownership. Received DigiDollars were unaffected. (Reported by shenger)

- **Universal Transaction Lookup (no more -txindex requirement)** — Previously, nodes needed `-txindex` enabled to look up DigiDollar amounts during validation. Without it, nodes could reject valid blocks containing DD redemptions, causing chain splits. Now uses a block-database lookup that works for every full node — no optional indexes needed, survives restarts, works everywhere.

- **Oracle Staleness False Alarm Fixed** — `getoracleprice` was reporting `is_stale: true` during the first epoch before any oracle prices were committed on-chain, even though oracles were actively submitting fresh data via P2P. Now correctly uses current chain height when pending messages exist.

- **Test Failures Resolved** — Fixed NUMS internal key mismatch in DD signing functions, corrected oracle price unit comparisons (micro-USD vs cents), and fixed oracle P2P tests using mainnet chainparams instead of regtest.

- **Transfer Validation Order Fixed** — DD amount lookups now prefer authoritative sources (txindex, block-db) over the in-memory metadata registry, preventing edge cases where stale cached data from script hash collisions could cause issues.

### 🔑 Oracle Key Updates

- **Aussie Epic (Oracle 6)** — New pubkey after RC13 wallet was corrupted. Old key no longer accessible.

---

## Commits Since RC14

```
d0c9f008e5 update: Aussie Epic oracle 6 pubkey for RC15
91c854d08c fix: wallet rescan missing self-minted DigiDollars on fresh sync
a5a752ac29 fix: universal DD amount extraction via block database lookup
e1df0849f2 fix(oracle): use current height for staleness when pending messages exist
1411390658 fix: resolve 3 root causes of DigiDollar test failures
704e4fce72 fix: oracle_p2p_tests use wrong chainparams (mainnet vs regtest)
3559008524 security: re-lock DD collateral on block disconnection (reorg handler)
f659604ce0 security: protect DigiDollar locks from UnlockAllCoins and lockunspent RPC
cc21f0063d security: reject DD transactions when validation data unavailable instead of bypassing
d2be3ccbaa security: remove TLS fallback in exchange fetcher, harden curl settings
a55ff1a54b security: bind oracle pubkey from chainparams before P2P signature verification
3a5101133c security: use NUMS point for collateral Taproot internal key (CVE-grade)
```

---

## Technical Changes

| File | Change |
|------|--------|
| `configure.ac` | Version bump RC14 → RC15 |
| `src/kernel/chainparams.cpp` | Updated oracle key for Aussie Epic (ID 6) |
| `src/digidollar/scripts.h` | Added COLLATERAL_NUMS_POINT_BYTES constant and GetCollateralNUMSKey() |
| `src/digidollar/scripts.cpp` | Implemented GetCollateralNUMSKey() |
| `src/digidollar/txbuilder.cpp` | Changed collateral internalKey from owner pubkey to NUMS point |
| `src/wallet/digidollarwallet.cpp` | Fixed wallet rescan mint detection with IsDDOutputMine(), fixed signing for NUMS key |
| `src/wallet/wallet.cpp` | DD-aware block disconnection handler re-locks collateral on reorg |
| `src/net_processing.cpp` | Bind oracle pubkey from chainparams before P2P signature verification |
| `src/oracle/bundle_manager.cpp` | Verify oracle messages against chainparams keys, not embedded keys |
| `src/oracle/exchange.cpp` | Removed TLS fallback, added curl hardening, NaN/infinity guards |
| `src/validation.cpp` | Reject DD transactions when validation data unavailable; universal block-db lookup |
| `src/wallet/spend.cpp` | Protect DD locks from UnlockAllCoins and lockunspent RPC |
| `src/test/digidollar_scripts_tests.cpp` | Added NUMS point verification tests |
| `src/test/oracle_p2p_tests.cpp` | Fixed test fixture from mainnet to regtest chainparams |
| `src/qt/res/icons/digibyte_wallet.png` | Update wallet splash to RC15 |

---

## Upgrade Notes

**RC15 uses the same testnet13 network as RC12–RC14 (port 12030). Your existing testnet data and wallets will work — no migration needed.**

If you're upgrading from RC14, simply replace the binaries and restart.

**Aussie Epic (Oracle 6):** Your new key is active in this release. After upgrading, run `startoracle 6` to begin reporting prices.

**Important:** The NUMS collateral key change means any DigiDollars minted on RC15 will use the new collateral format. Existing mints from RC14 and earlier are unaffected — they continue to work normally.

---

## Known Issues

- `digidollar_network_relay.py` functional test has a pre-existing getrawtransaction assertion failure — does not affect runtime behavior
- Windows users may experience oracle connectivity issues after ~24 hours of continuous operation
- Fixed testnet mining difficulty causes slower-than-normal block times on some algorithms

---

## Testing

All tests validated:
- ✅ 1,493 / 1,493 C++ unit tests pass (zero failures)
- ✅ 7 / 8 DigiDollar functional tests pass (1 pre-existing `digidollar_network_relay.py` failure)

---

## Network Information

| Setting | Value |
|---------|-------|
| Network | Testnet (testnet13) |
| Default P2P Port | 12030 |
| Default RPC Port | 14025 |
| Oracle Node | oracle1.digibyte.io:12030 |
| Address Prefix | dgbt1... (bech32) |
| Phase Two Activation | Block 100 |
| Oracle Consensus | 5-of-8 Schnorr threshold |

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc15-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc15-win64.zip` |
| macOS Apple Silicon (M1/M2/M3/M4) | `digibyte-9.26.0-rc15-arm64-apple-darwin.dmg` |
| macOS Intel | `digibyte-9.26.0-rc15-x86_64-apple-darwin.dmg` |
| Linux x86_64 | `digibyte-9.26.0-rc15-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc15-aarch64-linux-gnu.tar.gz` |

---

## Quick Start

If you're new to DigiDollar testing, see the complete setup instructions in the [RC12 Release Notes](./RELEASE_v9.26.0-rc12.md) — the setup process is identical.

---

## Troubleshooting

### "Self-minted DigiDollars not showing"
- **If you deleted testnet13 and resynced:** This was fixed in RC15. Update to RC15 and resync — your self-minted DDs will now appear correctly.
- **If balances appear in DD-Transaction/DD-Vault but not in the main overview:** Same issue, fixed in RC15.

### "DigiDollar tab not appearing"
- Verify `digidollar=1` is under `[test]` section in config
- Verify `testnet=1` is at the top of config (not under any section)
- Restart the wallet after config changes

### "Not connecting to network"
- Check your firewall allows port 12030
- Verify `addnode=oracle1.digibyte.io` is under `[test]` in config

### "Oracle price shows 0 or N/A"
- Wait for sync to complete
- The oracle broadcasts price updates every few minutes
- Check Window > Console: `getoracleprice`

### "Transaction stuck / unconfirmed"
- Try: `abandontransaction <txid>` in the console

### "No wallet is loaded" when running oracle commands
- Add `-rpcwallet=oracle` to your `createoraclekey` and `startoracle` commands

### "Oracle not configured" from `startoracle`
- Run `createoraclekey` first to generate and store the key in your wallet

For complete troubleshooting, see [RC12 Release Notes](./RELEASE_v9.26.0-rc12.md).

---

## Feedback & Community

Please report issues and feedback to help us prepare for mainnet launch.

When reporting bugs, start your message with **BUG:** and include: what happened, steps to reproduce, platform (Windows/Linux/Mac), and any error messages from your debug.log file.

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
