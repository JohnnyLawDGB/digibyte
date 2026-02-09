# DigiByte v9.26.0-rc15 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## What's New in RC15

RC15 is a major stability and security release with **1 critical security fix, 5 security hardening improvements, 10 bug fixes, and 2 quality-of-life improvements** since RC14.

---

### 🔴 Critical Security Fix

**Your locked DGB collateral is now truly locked.**

Previously, someone who minted DigiDollars could immediately withdraw the DGB they locked as collateral — before the timelock expired. This would create DigiDollars backed by nothing. We fixed this by making the collateral output mathematically impossible to spend via the fast path. All spending must now go through the timelock script. This is the single most important fix in RC15.

---

### 🔒 Security Hardening (5 fixes)

**1. Oracle price messages are now verified against authorized keys.**
Before, anyone could broadcast fake oracle price messages to the network because the signature was checked against whatever key was in the message itself — not against the real authorized oracle keys. Now every oracle message is verified against the keys hardcoded in the software.

**2. Encrypted connections to price feeds can no longer silently fall back to unencrypted.**
If the oracle couldn't find SSL certificates on the system, it would quietly disable encryption and fetch prices over plain HTTP. An attacker on the network could intercept and modify prices. That fallback is gone — if encryption fails, the fetch fails. We also added limits on redirects, file sizes, and timeouts.

**3. Your collateral can no longer be accidentally unlocked.**
The "unlock all coins" function and the `lockunspent` RPC command could accidentally unlock DGB that was locked as DigiDollar collateral. If you then spent that DGB, your DigiDollars would become unbacked. DD-locked coins are now permanently protected from these commands.

**4. Validation no longer silently skips checks when data is missing.**
Three places in the code would assume a DigiDollar transaction was valid if they couldn't look up the data to verify it. Now they reject the transaction instead. This prevents invalid transactions from sneaking through on nodes with incomplete data.

**5. Collateral is re-locked after blockchain reorganizations.**
If the network experienced a chain reorganization that reversed a DD redemption, the collateral was left unlocked — creating a brief window where it could be spent. Now it's immediately re-locked when a block is disconnected.

---

### 🐛 Bug Fixes (10 fixes)

**1. Self-minted DigiDollars no longer disappear after wallet rescan.** *(Reported by shenger)*
If you deleted your testnet data and resynced, or loaded your wallet after syncing, all DigiDollars you minted yourself would vanish from your balance. The wallet couldn't recognize its own DD outputs during rescan. Fixed — your minted DD now survives resync.

**2. Nodes no longer need `-txindex` to validate DigiDollar transactions.**
Without the transaction index enabled, nodes could reject perfectly valid blocks containing DD redemptions — potentially causing the network to split. Now uses a direct block database lookup that works on every node, no special configuration needed.

**3. Oracle price no longer shows "stale" when oracles are actively reporting.**
The `getoracleprice` command was incorrectly reporting the price as stale during the first epoch, even though oracles were actively submitting fresh prices. Fixed to use the correct reference point.

**4. Fixed 3 test failures from the security fixes.**
The NUMS collateral key change broke some signing paths and price unit comparisons in tests. All resolved.

**5. DD amount lookups now use the most reliable data source first.**
Previously, the code could use stale cached data instead of the authoritative blockchain data, causing edge-case validation failures.

**6. Windows oracles no longer lose network connectivity after 6–24 hours.** *(Reported by DanGB)*
The price fetcher was creating and destroying a network connection for every single HTTP request — about 40,000 times per day. On Windows, this exhausted available network sockets. Now reuses a single persistent connection. Also fixed a memory leak.

**7. Freshly minted DigiDollars no longer appear spendable before confirmation.** *(Reported by shenger)*
Right after minting DD, the full amount appeared in your balance immediately. If you tried to send it before it confirmed, the transaction would fail. Now minted DD only appears in your balance after at least 1 confirmation. Your own transfer change is still immediately available.

**8. You can now send multiple DD transactions in rapid succession.** *(Reported by Bastian)*
Sending 5 DD transfers in a row would fail after about 4 sends. Two causes: the wallet wouldn't reuse its own unconfirmed DGB change for fees, and DD change from your own transfers was incorrectly filtered out. Both fixed — you can now chain as many sends as you want.

**9. All DigiDollar and Oracle commands now appear in Help → Command-line options.** *(Reported by shenger)*
The Help dialog was missing all DD and Oracle RPC commands. Now shows all 17 DigiDollar commands and 9 Oracle commands with descriptions. Also fixed the dialog layout — the left panel was rendering as an empty box.

**10. Removed CoinMarketCap price fetcher.**
CoinMarketCap requires a paid API key, which is fundamentally incompatible with a decentralized oracle protocol. You can't hardcode a shared API key in open source software. Removed entirely. Oracles now use 6 free public exchange APIs: Binance, CoinGecko, KuCoin, Gate.io, HTX, and Crypto.com.

---

### 🔑 Oracle Key Updates

**Aussie Epic (Oracle 6)** — New pubkey after RC13 wallet was corrupted by a folder rename. Old key no longer accessible. New key is active in this release.

---

## Commits Since RC14

```
ca2d5cafdb docs: update RC15 release notes with bug fixes and CMC removal
7b909ef23d fix: add all DD/Oracle RPC commands to Help dialog, fix empty left pane
918c809572 remove: CoinMarketCap fetcher (paid API key incompatible with decentralized oracle)
c2408fcc5f fix: allow rapid consecutive DD transfers (fee + change chaining)
49315c6218 fix: add DigiDollar and Oracle categories to command-line help
2d1032b456 fix: exclude unconfirmed DD UTXOs from spendable balance
ab0b8fd60c fix: reuse persistent CURL handle to prevent Windows socket exhaustion
ffa46cbb0d release: bump version to v9.26.0-rc15
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

## Upgrade Notes

**RC15 uses the same testnet13 network as RC12–RC14 (port 12030). Your existing testnet data and wallets will work — no migration needed.**

If you're upgrading from RC14, simply replace the binaries and restart.

**Aussie Epic (Oracle 6):** Your new key is active in this release. After upgrading, run `startoracle 6` to begin reporting prices.

**Important:** DigiDollars minted on RC15 use a new collateral format (NUMS internal key). Existing mints from RC14 and earlier are unaffected and continue to work normally.

---

## Known Issues

- `digidollar_network_relay.py` functional test has a pre-existing getrawtransaction assertion failure — does not affect runtime behavior
- Fixed testnet mining difficulty causes slower-than-normal block times on some algorithms

---

## Testing

All tests validated:
- ✅ 1,501 / 1,501 C++ unit tests pass (zero failures)
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
| Exchange Sources | 6 (Binance, CoinGecko, KuCoin, Gate.io, HTX, Crypto.com) |

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

### "Failed to create or send the transaction" when sending multiple DDs
- This was fixed in RC15. Update and retry.

For complete troubleshooting, see [RC12 Release Notes](./RELEASE_v9.26.0-rc12.md).

---

## Feedback & Community

Please report issues and feedback to help us prepare for mainnet launch.

When reporting bugs, start your message with **BUG:** and include: what happened, steps to reproduce, platform (Windows/Linux/Mac), and any error messages from your debug.log file.

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
