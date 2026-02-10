# DigiByte v9.26.0-rc16 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## What's New in RC16

RC16 is a critical stability release with **3 critical bug fixes, 2 wallet improvements, and comprehensive BIP9 activation gating** since RC15. **Upgrade strongly recommended** — RC15 has a critical oracle P2P bug that causes cascading peer disconnections.

---

### 🔴 Critical: Oracle P2P Rate Limiter Was Banning Peers

**This was the root cause of DD transactions not confirming on testnet.**

Each oracle broadcasts a signed price message every 15 seconds (once per block). Each message has a unique timestamp → unique hash → passes the P2P duplicate check as "novel." With 8 testnet oracles, that's 1,920 novel messages per peer per hour. On mainnet with 30 oracles, it would be 7,200.

The rate limiter only allowed 50 novel oracle messages per peer per hour. After about 12 minutes, every additional oracle message added +5 Misbehaving points to the peer. Within minutes, peers accumulated 500+ misbehavior points, got disconnected, oracle data was lost, the oracle count dropped below the 5-of-8 consensus threshold, no oracle bundles could be created, and every DigiDollar transaction became unmineable.

Essentially, we were banning our own peers for doing their job correctly — like firing a mail carrier for delivering too much mail.

**Fixes:**
- Oracle broadcast interval changed from 15s to 60s. Exchange APIs don't update faster than once per minute. 60 seconds still gives 12x redundancy per testnet epoch and 25x per mainnet epoch.
- Rate limit increased from 50 to 3,600 messages per peer per hour (30 oracles × 60/hr × 2x headroom for mainnet).
- `Misbehaving()` penalty completely REMOVED from the rate limiter. Oracle relay is normal P2P gossip behavior. Excess messages are now silently dropped.

---

### 🔴 Critical: Dandelion++ P2P Transaction Relay Was Broken

Two bugs in the Dandelion++ privacy relay caused transactions to get stuck with 0 confirmations:

**1. Stem relay spam.** Transactions were being re-routed through the Dandelion stem phase on every `SendMessages()` cycle instead of just once. This wasted bandwidth and confused the relay logic. Fixed by tracking already-routed transactions in a `m_dandelion_stem_routed` set.

**2. New peers never learned about existing transactions.** When peers disconnected and new ones connected, the new peers had empty inventory sets. Transactions that were already `RelayTransaction()`'d before the new peer connected were invisible to them — the wallet wouldn't rebroadcast for 12-36 hours. Fixed by seeding new peers with the full mempool on their first INV cycle.

---

### 🔴 Critical: Wallet Was Erasing DD UTXOs Before Confirmation

When you minted, transferred, or redeemed DigiDollars, the wallet immediately erased the DD UTXO from its tracking database — before the transaction confirmed on-chain. If the transaction got delayed (which happened a lot due to the Dandelion and oracle bugs above), your wallet would "forget" about the DD you owned.

**Fix:** DD UTXOs are now only erased upon block confirmation (1-conf), matching the exact same pattern mainnet DGB uses for `IsSpent()`. Removed premature erasure from 5 transaction paths: TransferDigiDollar, UpdatePositionStatus, BurnDigiDollars, RedeemDigiDollar, and MarkDDUTXOsSpent.

---

### 💡 Wallet: Available vs Pending DD Balance

Previously, your DD balance showed everything lumped together — confirmed and unconfirmed. Now the wallet shows "Available" (confirmed, 1+ confirmations) separately from "Pending" (waiting for confirmation), just like it does for regular DGB. No more confusion about what you can actually spend.

---

### 💡 Qt: Oracle Price Drift Detection

If the DGB price changed between when you opened the mint screen and when you clicked "Mint," you could end up locking more (or less) collateral than expected. Now the wallet re-checks the oracle price at the moment you click Mint and warns you if it changed, showing old vs. new collateral amounts. The redeem widget also auto-refreshes the oracle price every 15 seconds. Oracle poll timer reduced from 30s to 12s.

---

### 🔧 BIP9 Activation Gating (6 commits)

Multiple fixes to ensure DigiDollar features are properly hidden, disabled, and safe on nodes that haven't activated DD yet. This prevents issues on mainnet where DD activates via miner voting (BIP9):

- `SCRIPT_VERIFY_DIGIDOLLAR` flag now properly activated after BIP9 deployment
- Oracle P2P handlers gated by activation height (prevents pre-activation peer banning)
- DD RPC commands (mint, transfer, burn, redeem) gated by BIP9 activation
- Pre-activation DD validation removed from `CheckTransaction`
- DigiDollar tab hidden in Qt when not activated
- `nDDActivationHeight` aligned with BIP9 `min_activation_height` (22,014,720)

---

### 🔧 Other Fixes

- **startoracle RPC crash fix** — `startoracle` was using the wrong internal context (node instead of wallet), which could crash the wallet when called from the Qt console.
- **Updated Aussie Epic (Oracle 6) pubkey** — Key mismatch preventing oracle recognition.
- **Fixed pre-existing test failures** and added BIP9 activation boundary tests.

---

## Technical Changes

| File | Change |
|------|--------|
| `configure.ac` | Version bump RC15 → RC16 |
| `src/net_processing.cpp` | Oracle rate limiter: 50→3600 limit, removed Misbehaving penalty; Dandelion++ stem routing dedup; new peer mempool seeding |
| `src/net.h` | Added `m_dandelion_stem_routed` tracking set |
| `src/dandelion.cpp` | Clear stem-routed set on peer disconnect/shuffle |
| `src/oracle/node.cpp` | Broadcast interval 15s → 60s, price update interval 15s → 60s |
| `src/oracle/node.h` | Default broadcast interval 300 → 60, added `GetBroadcastInterval()` accessor |
| `src/oracle/bundle_manager.cpp` | Updated comments for 60s broadcast interval |
| `src/wallet/digidollarwallet.cpp` | Removed premature UTXO erasure from 5 TX paths; added `GetPendingDDBalance()` |
| `src/wallet/digidollarwallet.h` | Added `GetPendingDDBalance()` declaration |
| `src/qt/digidollarmintwidget.cpp/.h` | Oracle price re-check on mint click with drift warning |
| `src/qt/digidollarredeemwidget.cpp` | 15s auto-refresh of oracle price |
| `src/qt/digidollaroverviewwidget.cpp/.h` | Separate Available/Pending DD balance display |
| `src/kernel/chainparams.cpp` | Updated Aussie oracle 6 pubkey; BIP9 activation gating |
| `src/validation.cpp` | `SCRIPT_VERIFY_DIGIDOLLAR` flag activation; pre-activation DD validation removal |
| `src/rpc/digidollar.cpp` | Gate DD RPCs by BIP9 activation; startoracle wallet context fix |
| `src/qt/res/icons/digibyte_wallet.png` | Update wallet splash to RC16 |
| `src/test/digidollar_oracle_tests.cpp` | 4 new tests for broadcast interval and rate limit |
| `src/test/digidollar_utxo_lifecycle_tests.cpp` | 8 new tests for UTXO lifecycle |
| `src/test/dandelion_tests.cpp` | 2 new tests for Dandelion++ fixes |

---

## What is DigiDollar?

DigiDollar is a USD-pegged stablecoin built natively into DigiByte. It uses an over-collateralized model where users lock DGB to mint DUSD at the current oracle price of DGB.

The world's first truly decentralized stablecoin native on a UTXO blockchain, enabling stable value transactions without centralized control.

DGB becomes the strategic reserve asset (21B max, only ~1.94 DGB per person on Earth). Everything happens inside DigiByte Core wallet. You never give up control of your private keys. No centralized company, fund or pool. Pure decentralization.

**Learn more:** https://digibyte.io/digidollar

---

## Wallet GUI Guide

### How to Use DigiDollar in the Wallet

1. **Open the DigiDollar Tab** — Click "DigiDollar" in the top navigation bar
2. **Mint DigiDollars** — Lock DGB as collateral to create DUSD. Enter the amount and confirm. Your DGB remains locked for the duration of the lock tier you select.
3. **Send DigiDollars** — Click "Send DGB" but use a DD address (starts with `dgbt1...`). Or use the DigiDollar tab's send function.
4. **Receive DigiDollars** — Click "Receive DGB" to get your DD-capable address, or use the DigiDollar tab.
5. **View History** — DD Transactions tab shows complete transaction history (auto-refreshes)
6. **Redeem DigiDollars** — Burn DUSD to unlock your DGB collateral after the lock period expires
7. **Coin Control** — Use manual DD input selection for advanced redemptions
8. **Address Book** — Save frequently used DD addresses for quick sending
9. **Export History** — Export DD transactions to CSV for record keeping

---

## Oracle Operator Setup

Want to run an oracle node? Here's the simple version:

### Prerequisites
- DigiByte Core RC16 built from source with curl support (or download the binary)
- An assigned oracle ID (0–7 for testnet, contact the maintainer)

### Two-Command Setup

```bash
# Step 1: Create wallet and generate oracle key (one-time)
digibyte-cli -testnet createwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle createoraclekey <your_oracle_id>

# Step 2: Start your oracle (after every node restart)
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

**Step 1** generates a Schnorr keypair, stores the private key in your wallet, and returns the public key. Send the **X-only public key** (32-byte hex) to the maintainer for inclusion in `chainparams.cpp`.

**Step 2** loads the private key from your wallet and starts the oracle price feed thread. Your node will automatically fetch DGB/USD prices from multiple exchanges (minimum 2 sources required) and broadcast signed price messages to the network.

> **⚠️ After restarting `digibyted`, you must run `startoracle` again.** The key persists in the wallet, but the oracle thread does not auto-start.

For the complete guide, see **`DIGIDOLLAR_ORACLE_SETUP.md`**.

### Current Oracle Operators (Testnet)

| ID | Operator | Status |
|----|----------|--------|
| 0 | Jared | ✅ Active |
| 1 | Green Candle | ✅ Active |
| 2 | Bastian | ✅ Active |
| 3 | DanGB | ✅ Active |
| 4 | Shenger | ✅ Active |
| 5 | Ycagel | ✅ Active |
| 6 | Aussie Epic | 🔑 New key in RC16 |
| 7 | LookIntoMyEyes | ✅ Active |

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
| `createoraclekey <id>` | Generate a new oracle Schnorr keypair in your wallet (one-time) |
| `getoraclepubkey <id>` | Show the oracle public key stored in your wallet |
| `startoracle <id>` | Start running as an oracle operator (must re-run after restart) |
| `stoporacle <id>` | Stop your oracle |
| `getoracleprice` | Get the consensus price DigiDollar uses for minting/redemption |
| `getalloracleprices` | Per-oracle breakdown: price, deviation, signature validity |
| `getoracles` | Network-wide view of all oracle operators and their status |
| `listoracle` | Show your local oracle status |
| `sendoracleprice` | Manually submit a price (testing/debugging) |

### Usage Examples
```bash
# Check the consensus price DigiDollar uses
digibyte-cli -testnet getoracleprice

# View all oracle operators network-wide
digibyte-cli -testnet getoracles

# See what each oracle reported (forensics)
digibyte-cli -testnet getalloracleprices

# Check your DD balance
digibyte-cli -testnet -rpcwallet=default getdigidollarbalance

# Mint 10 DUSD
digibyte-cli -testnet -rpcwallet=default mintdigidollar 1000

# Send 5 DUSD to someone
digibyte-cli -testnet -rpcwallet=default senddigidollar dgbt1... 500
```

---

## Commits Since RC15

### Critical Fixes
```
81d9f69ea2 fix: oracle P2P rate limiter causing cascading peer disconnections
0caa0e84a1 fix: resolve two critical Dandelion++ P2P transaction relay bugs
d591861158 wallet: prevent premature DD UTXO erasure — match mainnet DGB pattern
```

### Wallet & Qt Improvements
```
6e7fce9099 wallet/qt: separate DigiDollar available and pending balances
12ecfd9a13 qt: detect oracle price drift in mint/redeem widgets before TX submission
209d99b7f7 fix: startoracle RPC uses wallet context, not node context
```

### BIP9 Activation Gating
```
32b4b9c160 fix: activate SCRIPT_VERIFY_DIGIDOLLAR flag after BIP9 deployment
49ac3375a0 fix: gate oracle P2P handlers by activation height
14374cd2d9 fix: gate mutating DD RPC commands by BIP9 activation
c086fd94e1 consensus: remove pre-activation DD validation from CheckTransaction
5bf1dbf8a8 ui: hide DigiDollar tab when not activated
ec53a4347d consensus: align nDDActivationHeight with BIP9 min_activation_height (22014720)
```

### Other
```
883be119fa chainparams: update Aussie (oracle ID 6) pubkey for RC16
52544cd173 test: fix pre-existing test failures
c107d61040 test: add activation boundary test for DigiDollar BIP9 deployment
fc2420859d test: add activation boundary test for DigiDollar BIP9 deployment
```

---

## Upgrade Notes

**RC16 uses the same testnet13 network as RC12–RC15 (port 12030). Your existing testnet data and wallets will work — no migration needed.**

If you're upgrading from RC15, simply replace the binaries and restart. **Upgrade strongly recommended** — RC15 has a critical P2P bug that bans peers for relaying oracle data, causing oracle consensus failure.

### If Upgrading from RC11 or Earlier:
1. Close your old wallet
2. Delete old testnet data:
   - **Windows:** Delete `%APPDATA%\DigiByte\testnet10\` and `testnet11\`
   - **macOS:** Delete `~/Library/Application Support/DigiByte/testnet10/` and `testnet11/`
   - **Linux:** Delete `~/.digibyte/testnet10/` and `~/.digibyte/testnet11/`
3. Download and install RC16
4. Launch with `-testnet` flag

**Aussie Epic (Oracle 6):** Your new key is active in this release. After upgrading, run `startoracle 6` to begin reporting prices.

**Oracle Operators:** After upgrading, your oracle will broadcast prices every 60 seconds instead of 15 seconds. This is intentional — exchange APIs don't update faster than once per minute, and the reduced frequency prevents the P2P rate limiter bug from RC15.

---

## Configuration

### Minimum digibyte.conf for DigiDollar Testing:
```ini
testnet=1

[test]
digidollar=1
addnode=oracle1.digibyte.io
```

### Optional Settings:
```ini
[test]
# Enable transaction index (useful for debugging, not required)
txindex=1

# Enable DigiDollar stats index for network-wide supply tracking
digidollarstatsindex=1

# For mining (SHA256d recommended for fastest CPU mining)
algo=sha256d
```

---

## Known Issues

- Fixed testnet mining difficulty causes slower-than-normal block times on some algorithms
- `startoracle` must be re-run after every `digibyted` restart

---

## Testing

All tests validated:
- ✅ 1,515 / 1,515 C++ unit tests pass (zero failures)
- ✅ 8 / 8 DigiDollar functional tests pass

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

### Oracle Operators (Testnet)

| ID | Name | Public Key (X-only, first 16 hex) |
|----|------|-----------------------------------|
| 0 | Jared | Lead maintainer |
| 1 | Green Candle | Community |
| 2 | Bastian | Community |
| 3 | DanGB | Community |
| 4 | Shenger | Community |
| 5 | Ycagel | Community |
| 6 | Aussie Epic | Community (new key in RC16) |
| 7 | LookIntoMyEyes | Community |

---

## Downloads

| Platform | File |
|----------|------|
| Windows 64-bit (Installer) | `digibyte-9.26.0-rc16-win64-setup.exe` |
| Windows 64-bit (Portable) | `digibyte-9.26.0-rc16-win64.zip` |
| macOS Apple Silicon (M1/M2/M3/M4) | `digibyte-9.26.0-rc16-arm64-apple-darwin.dmg` |
| macOS Intel | `digibyte-9.26.0-rc16-x86_64-apple-darwin.dmg` |
| Linux x86_64 | `digibyte-9.26.0-rc16-x86_64-linux-gnu.tar.gz` |
| Linux ARM64 (Raspberry Pi) | `digibyte-9.26.0-rc16-aarch64-linux-gnu.tar.gz` |

---

## Quick Start

### New to DigiDollar?

1. Download the binary for your platform (see Downloads above)
2. Create your config file (see Configuration section)
3. Launch with `-testnet` flag: `./digibyte-qt -testnet`
4. Wait for the blockchain to sync (should be quick on testnet)
5. Once synced, the DigiDollar tab will appear
6. You need DGB to mint — ask in Gitter and someone can send you testnet DGB

### Want to Run an Oracle?

See the **Oracle Operator Setup** section above, or read `DIGIDOLLAR_ORACLE_SETUP.md` for the complete guide.

---

## Troubleshooting

### "Self-minted DigiDollars not showing"
- **If you deleted testnet13 and resynced:** This was fixed in RC16. Update to RC16 and resync — your self-minted DDs will now appear correctly.
- **If balances appear in DD-Transaction/DD-Vault but not in the main overview:** Same issue, fixed in RC16.

### "Failed to create or send the transaction" when sending multiple DDs
- This was fixed in RC16. Update and retry. You can now chain unlimited consecutive sends.

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
- **If you upgraded from RC13:** This was a known bug fixed in RC15 where minting DD could cause subsequent DGB sends to get stuck. After upgrading, your wallet will automatically re-lock the correct UTXOs on startup.
- If still stuck, try: `abandontransaction <txid>` in the console

### "No wallet is loaded" when running oracle commands
- Add `-rpcwallet=oracle` to your `createoraclekey` and `startoracle` commands

### "Oracle not configured" from `startoracle`
- Run `createoraclekey` first to generate and store the key in your wallet

### "Mining not working"
- Ensure `algo=sha256d` is in your config under `[test]`
- SHA256d is recommended for fastest CPU mining

### "Old testnet data causing crashes"
- Delete your testnet10 and testnet11 folders completely (see Upgrade Notes above)
- RC16 uses testnet13 blockchain

---

## Feedback & Community

Please report issues and feedback to help us prepare for mainnet launch.

When reporting bugs, start your message with **BUG:** and include: what happened, steps to reproduce, platform (Windows/Linux/Mac), and any error messages from your debug.log file.

- **Developer Chat (Gitter):** https://app.gitter.im/#/room/#digidollar:gitter.im
- **GitHub Issues:** https://github.com/DigiByte-Core/digibyte/issues
