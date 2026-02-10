# DigiDollar Oracle Setup Guide

*The single source of truth for oracle operator setup — Phase 2 Multi-Oracle*

---

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [New Oracle Setup](#new-oracle-setup)
- [Upgrading to a New Release](#upgrading-to-a-new-release)
- [Restarting After a Reboot or Crash](#restarting-after-a-reboot-or-crash)
- [What Your Oracle Does](#what-your-oracle-does)
- [Consensus Parameters](#consensus-parameters)
- [Monitoring](#monitoring)
- [Troubleshooting](#troubleshooting)
- [RPC Command Reference](#rpc-command-reference)
  - [Oracle RPCs](#oracle-rpcs)
  - [DigiDollar RPCs](#digidollar-rpcs)
  - [Mock Oracle RPCs (regtest only)](#mock-oracle-rpcs-regtest-only)
- [For the Maintainer](#for-the-maintainer)
- [Server Requirements](#server-requirements)
- [File Locations](#file-locations)
- [Fixing Wallet Name](#fixing-wallet-name)

---

## Overview

DigiDollar requires oracle operators to provide real-time DGB/USD price feeds. Oracle public keys are hardcoded in `src/kernel/chainparams.cpp`. The workflow:

1. Generate an oracle keypair via `createoraclekey` (stored in your wallet)
2. Send your **public key only** to the maintainer
3. Maintainer adds it to chainparams and ships a new release
4. You run `loadwallet` → `startoracle` after every node start

---

## Prerequisites

### Build DigiByte Core

```bash
cd ~/Code/digibyte
./autogen.sh
./configure
make -j$(nproc)
```

Verify curl support (required for exchange price fetching):
```bash
ldd ./src/digibyted | grep curl
# If missing: sudo apt install libcurl4-openssl-dev
```

### Configuration

Create or edit `~/.digibyte/digibyte.conf`:

```ini
testnet=1

[test]
digidollar=1
server=1
listen=1
addnode=oracle1.digibyte.io
debug=digidollar
debug=net
```

> **`testnet=1` goes at the top** (not under any section). Everything else under `[test]`.

Optional:
```ini
[test]
txindex=1
digidollarstatsindex=1
algo=sha256d
```

---

## New Oracle Setup

For first-time oracle operators. You need an assigned oracle ID (0–7 for testnet) — contact the maintainer.

```bash
# 1. Start your node
digibyted -testnet -daemon

# 2. Create a wallet (use just the name, NOT a full file path!)
digibyte-cli -testnet createwallet "oracle"

# 3. Generate your oracle key (one-time only)
digibyte-cli -testnet -rpcwallet=oracle createoraclekey <your_oracle_id>

# 4. Send your pubkey (66-char hex starting with 02/03) to the maintainer
#    NEVER share your private key.

# 5. After the maintainer ships a release with your key, start your oracle:
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>

# 6. Verify
digibyte-cli -testnet getoracles true
```

**Qt wallet users:** Create wallet via **File → Create Wallet**, name it `oracle`. Then **Help → Debug Window → Console** to run `createoraclekey` and `startoracle`.

> ⚠️ **Use just the wallet name** (`"oracle"`), not a full path like `"/home/user/.digibyte/testnet13/wallets/oracle/"`. See [Fixing Wallet Name](#fixing-wallet-name) if you already did this.

---

## Upgrading to a New Release

Your oracle key persists in your wallet across upgrades. You do NOT need to generate a new key.

```bash
# 1. Stop your node
digibyte-cli -testnet stop

# 2. Replace binaries (download new release or rebuild from source)

# 3. Start your node
digibyted -testnet -daemon

# 4. Load your wallet
digibyte-cli -testnet loadwallet "oracle"

# 5. Start your oracle
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>

# 6. Verify
digibyte-cli -testnet getoracles true
```

**Qt wallet users:** Start Qt → **File → Open Wallet → oracle** → **Console** → `startoracle <your_oracle_id>`

> ⚠️ **Steps 4 and 5 are required after EVERY restart.** The wallet does not auto-load and the oracle thread does not auto-start.

---

## Restarting After a Reboot or Crash

Same as upgrading but skip step 2:

```bash
digibyted -testnet -daemon
digibyte-cli -testnet loadwallet "oracle"
digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

---

## What Your Oracle Does

Once running, the oracle automatically:
- Fetches DGB/USD prices from multiple exchanges every 60 seconds
- Calculates median price with outlier filtering (requires minimum 2 valid sources)
- Signs the price with BIP-340 Schnorr using your wallet-stored private key
- Broadcasts the signed message to the P2P network

### Exchange Sources (no API keys required)

Binance, CoinGecko, KuCoin, Crypto.com, Gate.io, HTX

### Price Format

| Format | Unit | Example |
|--------|------|---------|
| Internal (wire) | micro-USD | `50000` = $0.05 |
| RPC input | USD | `0.05` |
| Valid range | $0.0001 – $10.00 | 100 – 10,000,000 micro-USD |

---

## Consensus Parameters

| Parameter | Testnet | Regtest | Mainnet |
|-----------|---------|---------|---------|
| Active Oracles | 8 | 7 | 15 |
| Consensus Required | 5-of-8 | 4-of-7 | 8-of-15 |
| Activation Height | 600 | 650 | BIP9 (TBD) |
| Epoch Length (`nDDOracleEpochBlocks`) | 50 blocks | 10 blocks | 100 blocks |
| Price Update Interval | 2 blocks | 1 block | 4 blocks |
| Oracle Broadcast Interval | 60 seconds | 60 seconds | 60 seconds |
| Max Price Age | 3600 seconds | 3600 seconds | 3600 seconds |

Total oracle slots: 30 (defined in `src/primitives/oracle.h`).

---

## Monitoring

```bash
# Watch oracle activity in real-time
tail -f ~/.digibyte/testnet13/debug.log | grep -i "oracle\|digidollar"

# Check current oracle price
digibyte-cli -testnet getoracleprice

# List all oracles and their status
digibyte-cli -testnet getoracles true

# Check specific oracle details
digibyte-cli -testnet listoracle <oracle_id>

# Get your oracle's public key and status
digibyte-cli -testnet getoraclepubkey <oracle_id>
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `"No wallet is loaded"` | Run `loadwallet "oracle"` first, then add `-rpcwallet=oracle` to commands |
| `"Oracle key already exists"` | Key is already in your wallet — no need to recreate |
| `"Oracle ID not found in chain parameters"` | Your key isn't in chainparams yet — wait for next release |
| `"Oracle not configured"` | Run `createoraclekey` first (new operators) or `loadwallet` (existing) |
| Oracle not running after restart | Must run `loadwallet` + `startoracle` after every restart |
| Price fetch failures | Check `ldd digibyted | grep curl` and internet connectivity |
| Wallet name shows full file path | See [Fixing Wallet Name](#fixing-wallet-name) |

---

## RPC Command Reference

### Oracle RPCs

#### `createoraclekey` *(wallet RPC)*
Generate an oracle Schnorr keypair and store it in your wallet. One-time only.

```
digibyte-cli -testnet -rpcwallet=oracle createoraclekey <oracle_id>
```
Returns `pubkey` (33-byte compressed) and `pubkey_xonly` (32-byte x-only). Send the `pubkey` to the maintainer. Rejects if a key already exists for that ID.

#### `startoracle` *(wallet RPC)*
Start the oracle price feed thread. Loads the private key from your wallet.

```
digibyte-cli -testnet -rpcwallet=oracle startoracle <oracle_id> [private_key_hex]
```
The optional `private_key_hex` overrides the wallet key (not recommended). Must be re-run after every node restart.

#### `stoporacle`
Stop a running oracle.

```
digibyte-cli -testnet stoporacle <oracle_id>
```

#### `getoraclepubkey`
Get an oracle's public key and running status. Oracle must be initialized via `startoracle` first.

```
digibyte-cli -testnet getoraclepubkey <oracle_id>
```
Returns: `oracle_id`, `pubkey`, `pubkey_full`, `valid`, `authorized`, `is_running`.

#### `getoracles`
List all oracles from chainparams with their status.

```
digibyte-cli -testnet getoracles [active_only]
```
Returns array with: `oracle_id`, `pubkey`, `endpoint`, `is_active`, `is_running`, `last_price`, `last_update`, `selected_for_epoch`.

#### `listoracle`
Get detailed info for a specific oracle.

```
digibyte-cli -testnet listoracle <oracle_id>
```

#### `getalloracleprices`
Get price data from all active oracles.

```
digibyte-cli -testnet getalloracleprices
```

#### `sendoracleprice` *(testnet/regtest only)*
Broadcast a signed oracle price message for testing.

```
digibyte-cli -testnet sendoracleprice <price_usd> [oracle_id]
```
Example: `sendoracleprice 0.05 1` broadcasts $0.05 as oracle 1.

#### `submitoracleprice` *(regtest only)*
Submit a Phase 2 oracle price for testing consensus.

```
digibyte-cli -regtest submitoracleprice <oracle_id> <price_usd>
```

### DigiDollar RPCs

#### `getoracleprice`
Get the current consensus DGB/USD oracle price.

```
digibyte-cli -testnet getoracleprice
```
Returns: `price_usd`, `price_micro_usd`, `last_update_height`, `is_stale`, `oracle_count`, `24h_high`, `24h_low`.

#### `mintdigidollar` *(wallet RPC)*
Mint DigiDollars by locking DGB as collateral.

```
digibyte-cli -testnet -rpcwallet=<wallet> mintdigidollar <amount> <lock_tier>
```

#### `senddigidollar` *(wallet RPC)*
Send DigiDollars to an address.

```
digibyte-cli -testnet -rpcwallet=<wallet> senddigidollar <address> <amount>
```

#### `redeemdigidollar` *(wallet RPC)*
Redeem DigiDollars to unlock collateral (after lock period expires).

```
digibyte-cli -testnet -rpcwallet=<wallet> redeemdigidollar <position_txid> [amount]
```

#### `getdigidollarbalance` *(wallet RPC)*
Get your DigiDollar balance (confirmed + pending).

```
digibyte-cli -testnet -rpcwallet=<wallet> getdigidollarbalance
```

#### `listdigidollarpositions` *(wallet RPC)*
List all your active DD positions (minted, locked collateral).

```
digibyte-cli -testnet -rpcwallet=<wallet> listdigidollarpositions
```

#### `listdigidollartxs` *(wallet RPC)*
List DigiDollar transaction history.

```
digibyte-cli -testnet -rpcwallet=<wallet> listdigidollartxs [count]
```

#### `getdigidollaraddress` *(wallet RPC)*
Get a new DigiDollar receiving address.

```
digibyte-cli -testnet -rpcwallet=<wallet> getdigidollaraddress
```

#### `listdigidollaraddresses`
List all DigiDollar addresses in a wallet.

```
digibyte-cli -testnet listdigidollaraddresses
```

#### `importdigidollaraddress`
Import a DigiDollar address for watch-only tracking.

```
digibyte-cli -testnet importdigidollaraddress <address> [label]
```

#### `validateddaddress`
Validate a DigiDollar address.

```
digibyte-cli -testnet validateddaddress <address>
```

#### `getdigidollarstats`
Get network-wide DigiDollar statistics (total supply, collateral locked, etc.).

```
digibyte-cli -testnet getdigidollarstats
```

#### `getdigidollardeploymentinfo`
Get DigiDollar BIP9 deployment status.

```
digibyte-cli -testnet getdigidollardeploymentinfo
```

#### `getdcamultiplier`
Get the current DCA (Dynamic Collateral Adjustment) multiplier.

```
digibyte-cli -testnet getdcamultiplier
```

#### `calculatecollateralrequirement`
Calculate collateral required for a given DD amount.

```
digibyte-cli -testnet calculatecollateralrequirement <dd_amount> <lock_tier>
```

#### `estimatecollateral`
Estimate collateral needed at current oracle price.

```
digibyte-cli -testnet estimatecollateral <dd_amount>
```

#### `getredemptioninfo`
Get redemption details for a position.

```
digibyte-cli -testnet getredemptioninfo <position_txid>
```

#### `getprotectionstatus`
Get the current collateral protection status.

```
digibyte-cli -testnet getprotectionstatus
```

### Mock Oracle RPCs (regtest only)

| Command | Description |
|---------|-------------|
| `setmockoracleprice <micro_usd>` | Set mock price (e.g. 50000 = $0.05) |
| `getmockoracleprice` | Get current mock price |
| `simulatepricevolatility <percent>` | Simulate volatility (e.g. 50 = +50%) |
| `enablemockoracle <true\|false>` | Enable/disable mock oracle |

---

## For the Maintainer

When an operator sends their `pubkey` (33-byte compressed, e.g. `0398720f...eb7b57`), add it to **two locations** in `src/kernel/chainparams.cpp`:

**1. `vOracleNodes`** — use the full 33-byte compressed key:
```cpp
{5, ParsePubKey("0398720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57"), "operator.server.com:12030", true},
```

**2. `consensus.vOraclePublicKeys`** — strip the `02`/`03` prefix to get the 32-byte x-only key:
```cpp
consensus.vOraclePublicKeys.push_back("98720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57");
```

Both locations MUST match the same key. If they don't, `ValidateOracleKey()` will reject the oracle at startup.

---

## Server Requirements

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| Uptime | 95% | 99.9% |
| RAM | 2 GB | 4+ GB |
| Disk | 20 GB | 50+ GB SSD |
| Network | Outbound HTTPS | Static IP or DNS |
| Ports | 12030 (testnet P2P) | Open inbound + outbound |

---

## File Locations

| Component | Path |
|-----------|------|
| Config | `~/.digibyte/digibyte.conf` |
| Testnet data | `~/.digibyte/testnet13/` |
| Debug log | `~/.digibyte/testnet13/debug.log` |
| Wallets | `~/.digibyte/testnet13/wallets/` |
| RPC cookie | `~/.digibyte/testnet13/.cookie` |

---

## Fixing Wallet Name

If `getwalletinfo` shows the full path as wallet name:

```bash
# Unload with the wrong name
digibyte-cli -testnet unloadwallet "/home/user/.digibyte/testnet13/wallets/oracle/"

# Reload with just the name
digibyte-cli -testnet loadwallet "oracle"

# Verify
digibyte-cli -testnet -rpcwallet=oracle getwalletinfo
```

---

*Verified against DigiByte Core RC16 source code.*
