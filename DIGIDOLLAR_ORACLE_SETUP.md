# DigiDollar Oracle Setup Guide
*The single source of truth for oracle operator setup — Phase 2 Multi-Oracle*

---

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [Step-by-Step: Oracle Operator Setup](#step-by-step-oracle-operator-setup)
  - [Step 1: Start the Node](#step-1-start-the-node)
  - [Step 2: Create or Load Your Wallet](#step-2-create-or-load-your-wallet)
  - [Step 3: Generate Your Oracle Key (New Operators Only)](#step-3-generate-your-oracle-key-new-operators-only)
  - [Step 4: Send Your Public Key to the Maintainer](#step-4-send-your-public-key-to-the-maintainer)
  - [Step 5: Wait for Updated Release](#step-5-wait-for-updated-release)
  - [Step 6: Start Your Oracle](#step-6-start-your-oracle)
  - [Step 7: Verify Your Oracle](#step-7-verify-your-oracle)
- [Upgrading Your Oracle to a New Release](#upgrading-your-oracle-to-a-new-release)
- [Restarting Your Oracle (After a Reboot or Crash)](#restarting-your-oracle-after-a-reboot-or-crash)
- [What Your Oracle Does](#what-your-oracle-does)
- [Multi-Oracle Setup (Phase 2)](#multi-oracle-setup-phase-2)
- [Monitoring & Troubleshooting](#monitoring--troubleshooting)
- [RPC Command Reference](#rpc-command-reference)
- [For the Maintainer: Adding an Operator's Key](#for-the-maintainer-adding-an-operators-key)
- [Server Requirements](#server-requirements)
- [File Locations](#file-locations)
- [Code References](#code-references)
- [Fixing Wallet Name](#fixing-wallet-name)

---

## Overview

DigiDollar requires oracle operators to provide real-time DGB/USD price feeds. Oracle public keys are **hardcoded in `src/kernel/chainparams.cpp`**. Each operator:

1. Runs DigiByte Core with a descriptor wallet
2. Generates an oracle keypair via `createoraclekey`
3. Sends their **public key only** to the DigiByte Core maintainer
4. The maintainer adds the key to `chainparams.cpp` and ships a new release
5. The operator runs `loadwallet` then `startoracle` — the wallet provides the private key automatically

### Phase 2 Consensus Parameters

| Network | Total Slots | Active Oracles | Consensus Required | Phase 2 Activation |
|---------|-------------|----------------|--------------------|--------------------|
| **Mainnet** | 30 (IDs 0–29) | 15 | 8-of-15 | Disabled (`INT_MAX`) |
| **Testnet** | 8 (IDs 0–7) | 8 | 5-of-8 | Block 600 |
| **Regtest** | 7 (IDs 0–6) | 7 | 4-of-7 | Block 650 |

> **Note:** `ORACLE_TOTAL_COUNT = 30` is defined in `src/primitives/oracle.h`. Oracle IDs are always 0–29.

---

## Prerequisites

### Build DigiByte Core

```bash
cd ~/Code/digibyte
./autogen.sh
./configure    # libcurl auto-detected via pkg-config for exchange price fetching
make -j$(nproc)
```

Verify curl support (required for oracle price fetching):
```bash
ldd ./src/digibyted | grep curl
# If missing: sudo apt install libcurl4-openssl-dev (Debian/Ubuntu)
```

### Testnet Configuration

Create or edit `~/.digibyte/digibyte.conf`:

```ini
testnet=1

[test]
digidollar=1
server=1
listen=1
addnode=oracle1.digibyte.io

# Debugging (optional but recommended for oracle operators)
debug=digidollar
debug=net
```

### Optional Settings:
```ini
[test]
# Transaction index (useful for debugging)
txindex=1

# DigiDollar stats index for network-wide supply tracking
digidollarstatsindex=1

# For mining (SHA256d recommended for fastest CPU mining)
algo=sha256d
```

> **Important:** `testnet=1` goes at the **top** of the file (not under any section). All other settings go under the `[test]` section header.

---

## Step-by-Step: Oracle Operator Setup

### Step 1: Start the Node

```bash
./src/digibyted -testnet -daemon
./src/digibyte-cli -testnet getblockchaininfo
```

### Step 2: Create or Load Your Wallet

**New operators** — create a wallet using **just the name** (not a file path):

```bash
./src/digibyte-cli -testnet createwallet "oracle"
```

> **⚠️ IMPORTANT: Use just the name, not a full path!**
>
> ✅ Correct: `createwallet "oracle"`
> ❌ Wrong: `createwallet "/home/user/.digibyte/testnet13/wallets/oracle/"`
>
> Using a full path causes the wallet name to display incorrectly. If you already did this, see [Fixing Wallet Name](#fixing-wallet-name) below.

**Existing operators** (upgrading from a previous RC) — load your existing wallet:

```bash
./src/digibyte-cli -testnet loadwallet "oracle"
```

Your oracle key from the previous RC is still in your wallet. You do NOT need to run `createoraclekey` again. Skip to [Step 6: Start Your Oracle](#step-6-start-your-oracle).

**Qt wallet users:** Go to **File → Open Wallet → oracle**

> **⚠️ After every node restart, you must load your wallet again.** Wallets are not auto-loaded. Then start your oracle (Step 4).

### Step 3: Generate Your Oracle Key (New Operators Only)

```bash
./src/digibyte-cli -testnet -rpcwallet=oracle createoraclekey <oracle_id>
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `oracle_id` | number | Yes | Oracle ID slot (0–29) assigned by the maintainer |

**Example:**
```bash
./src/digibyte-cli -testnet -rpcwallet=oracle createoraclekey 5
```

**Output:**
```json
{
  "oracle_id": 5,
  "pubkey": "0398720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57",
  "pubkey_xonly": "98720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57",
  "stored_in_wallet": true,
  "message": "Oracle key generated and stored in wallet. Share ONLY the pubkey..."
}
```

**What happens internally:**
- A new secp256k1 compressed keypair is generated
- The **private key** is stored in the wallet database (key: `oraclekey`, mapped by oracle_id)
- If a key already exists for that oracle_id, the command **rejects** (prevents accidental overwrites)
- Requires the wallet to be **unlocked** if encrypted

> ⚠️ **Wallet RPC:** `createoraclekey` is a wallet RPC — you MUST specify `-rpcwallet=<name>`.

#### Understanding the Two Public Key Formats

`createoraclekey` returns **two representations of the same public key:**

| Field | Format | Size | Example |
|-------|--------|------|---------|
| `pubkey` | Compressed (with `02`/`03` prefix) | 33 bytes (66 hex chars) | `0398720f...eb7b57` |
| `pubkey_xonly` | X-only (prefix stripped) | 32 bytes (64 hex chars) | `98720f...eb7b57` |

They are the **same key**. `pubkey_xonly` is literally `pubkey` with the first byte (`02` or `03`) removed.

**Why two formats?** Because chainparams stores the key in **two locations**, each requiring a different format:

| Chainparams Location | Format Used | Purpose |
|---------------------|-------------|---------|
| `vOracleNodes` | `pubkey` (33-byte compressed) | Node identity and peer connections |
| `consensus.vOraclePublicKeys` | `pubkey_xonly` (32-byte x-only) | Schnorr signature verification at consensus level |

> **As an operator, you only share `pubkey`** (the 33-byte compressed key). The maintainer derives `pubkey_xonly` from it by stripping the prefix byte.

### Step 4: Send Your Public Key to the Maintainer

Send **only**:
1. Your **`pubkey`** from the output (66-char hex, starts with `02` or `03` — the 33-byte compressed public key)
2. Your **server endpoint** (e.g., `myserver.com:12030`)

The maintainer uses your single `pubkey` to populate **both** chainparams locations:
- **`vOracleNodes`** → your `pubkey` as-is (33-byte compressed, with `02`/`03` prefix)
- **`consensus.vOraclePublicKeys`** → your `pubkey_xonly` (32-byte, `02`/`03` prefix stripped)

You do **not** need to send `pubkey_xonly` separately — it's derived from `pubkey` by removing the first byte.

**⚠️ NEVER share your private key. It stays in your wallet.**

### Step 5: Wait for Updated Release

The maintainer adds your key to two locations in `src/kernel/chainparams.cpp`:

1. **`vOracleNodes`** — your full 33-byte compressed `pubkey` + endpoint (via `ParsePubKey()`)
2. **`consensus.vOraclePublicKeys`** — your 32-byte `pubkey_xonly` (the `02`/`03` prefix is stripped)

**⚠️ Both locations MUST be updated and MUST correspond to the same key.** See [Maintainer Section](#for-the-maintainer-adding-an-operators-key) below.

### Step 6: Start Your Oracle

After updating to the release with your key in chainparams:

```bash
./src/digibyte-cli -testnet -rpcwallet=oracle startoracle <oracle_id>
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `oracle_id` | number | Yes | Oracle ID (0–29) |
| `private_key` | hex string | No | Raw private key hex (only if NOT using wallet key) |

The command tries key sources in this order:
1. Explicit `private_key` parameter (if provided)
2. Existing oracle already configured in OracleManager
3. Wallet-stored key from `createoraclekey`

**Output:**
```json
{
  "success": true,
  "oracle_id": 5,
  "status": "running",
  "message": "Oracle started with key loaded from wallet 'oracle'",
  "was_already_running": false
}
```

> ⚠️ **Wallet RPC:** `startoracle` is a wallet RPC — you MUST specify `-rpcwallet=<name>`.
>
> **After restarting `digibyted`**, you must run `startoracle` again. The key persists in the wallet, but the oracle thread does not auto-start.

### Step 7: Verify Your Oracle

```bash
./src/digibyte-cli -testnet getoraclepubkey <oracle_id>
```

**Output:**
```json
{
  "oracle_id": 5,
  "pubkey": "98720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57",
  "pubkey_full": "0398720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57",
  "valid": true,
  "authorized": true,
  "is_running": true
}
```

- `authorized: true` — your key matches what's in chainparams
- `is_running: true` — price fetching thread is active

---

## Upgrading Your Oracle to a New Release

When a new RC is released, follow these steps. Your oracle key is stored in your wallet and persists across upgrades — you do NOT need to generate a new key.

```bash
# Step 1: Stop your node
./src/digibyte-cli -testnet stop

# Step 2: Replace binaries with the new release
#   - Download new release from GitHub, OR
#   - Build from source: git pull && make -j$(nproc)

# Step 3: Start your node
./src/digibyted -testnet -daemon

# Step 4: Wait for sync (check progress)
./src/digibyte-cli -testnet getblockchaininfo

# Step 5: Load your wallet
./src/digibyte-cli -testnet loadwallet "oracle"

# Step 6: Start your oracle
./src/digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>

# Step 7: Verify it's running
./src/digibyte-cli -testnet getoracles true
```

**Qt wallet users:** Start DigiByte Qt → **File → Open Wallet → oracle** → **Help → Debug Window → Console** → type `startoracle <your_oracle_id>`

> **⚠️ Steps 5 and 6 are required after EVERY restart.** The wallet does not auto-load and the oracle thread does not auto-start. Your key is safe in the wallet — you just need to load it and tell the oracle to start.

---

## Restarting Your Oracle (After a Reboot or Crash)

Same as upgrading, but skip Step 2 (no new binaries needed):

```bash
./src/digibyted -testnet -daemon
# Wait for sync...
./src/digibyte-cli -testnet loadwallet "oracle"
./src/digibyte-cli -testnet -rpcwallet=oracle startoracle <your_oracle_id>
```

---

## What Your Oracle Does

Once running, the oracle automatically:
- Fetches DGB/USD prices from multiple exchanges every 60 seconds
- Calculates median price with percentage-threshold outlier filtering
- Signs the price with BIP-340 Schnorr using your wallet-stored private key
- Broadcasts the signed message to the P2P network

### Price Format

| Format | Unit | Example |
|--------|------|---------|
| Internal (wire) | micro-USD | `50000` = $0.05 |
| RPC input | USD | `0.05` |
| Conversion | `price_micro_usd = usd * 1,000,000` | |
| Valid range | $0.0001 – $10.00 | 100 – 10,000,000 micro-USD |

### Exchange Sources

| Exchange | API Key Required |
|----------|------------------|
| Binance | No |
| CoinGecko | No |
| KuCoin | No |
| Crypto.com | No |
| Gate.io | No |
| HTX | No |

Minimum 2 valid sources required for a price to be accepted.

---

## Multi-Oracle Setup (Phase 2)

Phase 2 enables multi-oracle consensus. On testnet, this means **5-of-8 oracles must agree** on a price before it's accepted.

### Key Consensus Parameters (from `chainparams.cpp`)

| Parameter | Testnet | Regtest | Mainnet |
|-----------|---------|---------|---------|
| `nOracleActivationHeight` | 600 | 650 | Disabled (`INT_MAX`) |
| `nDigiDollarPhase2Height` | 600 | 650 | Disabled (`INT_MAX`) |
| `nOracleRequiredMessages` | 5 | 4 | 8 |
| `nOracleTotalOracles` | 8 | 7 | 15 |
| `nOracleEpochLength` | 1440 blocks | 144 blocks | 1440 blocks |
| `nDDOracleEpochBlocks` | 50 | 10 | 100 |
| `nDDOracleUpdateInterval` | 2 blocks | 1 block | 4 blocks |
| `nDDActivationHeight` | 600 | 650 | 22,014,720 |

### Running Multiple Oracles (Testing)

Each oracle needs its own wallet and oracle ID:

```bash
# Oracle operator A
./src/digibyte-cli -testnet createwallet "oracle_a"
./src/digibyte-cli -testnet -rpcwallet=oracle_a createoraclekey 0
./src/digibyte-cli -testnet -rpcwallet=oracle_a startoracle 0

# Oracle operator B
./src/digibyte-cli -testnet createwallet "oracle_b"
./src/digibyte-cli -testnet -rpcwallet=oracle_b createoraclekey 1
./src/digibyte-cli -testnet -rpcwallet=oracle_b startoracle 1

# Oracle operator C
./src/digibyte-cli -testnet createwallet "oracle_c"
./src/digibyte-cli -testnet -rpcwallet=oracle_c createoraclekey 2
./src/digibyte-cli -testnet -rpcwallet=oracle_c startoracle 2
```

### Epoch-Based Oracle Selection

Oracles are selected per-epoch using `SelectOraclesForEpoch()`. The `listoracles` command shows which oracles are selected for the current epoch via the `selected_for_epoch` field.

---

## Monitoring & Troubleshooting

### Monitor Oracle Activity

```bash
tail -f ~/.digibyte/testnet13/debug.log | grep -i "oracle\|digidollar"
```

### Check Oracle Price

```bash
./src/digibyte-cli -testnet getoracleprice
```

### List All Oracles

```bash
./src/digibyte-cli -testnet listoracles        # All oracles
./src/digibyte-cli -testnet listoracles true    # Active only
```

### Stop an Oracle

```bash
./src/digibyte-cli -testnet stoporacle <oracle_id>
```

### Common Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| `"No wallet is loaded"` | Missing `-rpcwallet` flag | Add `-rpcwallet=oracle` to `createoraclekey` / `startoracle` |
| `"Oracle key already exists"` | `createoraclekey` called twice for same ID | Use the existing key; it's already in the wallet |
| `"Oracle ID not found in chain parameters"` | Key not yet in chainparams | Wait for updated release with your key |
| `"Oracle not configured"` from `startoracle` | No wallet key and no explicit key | Run `createoraclekey` first, or pass `private_key` param |
| `"Oracle N not found"` from `getoraclepubkey` | Oracle not initialized | Run `startoracle` first |
| Price fetch failures | No curl / network issues | Verify `ldd digibyted | grep curl`; check connectivity |
| Stale prices after restart | `startoracle` not re-run | Must run `startoracle` after every `digibyted` restart |

---

## RPC Command Reference

Oracle management RPCs (`createoraclekey`, `startoracle`, `stoporacle`, `getoraclepubkey`, `listoracles`, `sendoracleprice`) are in the `"oracle"` category. `getoracleprice` and the mock oracle RPCs are in the `"digidollar"` category.

### `createoraclekey` *(wallet RPC)*

Generate oracle keypair and store in wallet.

```
createoraclekey <oracle_id>
```
- **oracle_id** (required): 0–29
- **Returns:** `{ oracle_id, pubkey, pubkey_xonly, stored_in_wallet, message }`
- **Errors if:** key already exists for that oracle_id, wallet locked, no wallet loaded

### `startoracle` *(wallet RPC)*

Start a local oracle node.

```
startoracle <oracle_id> [private_key_hex]
```
- **oracle_id** (required): 0–29
- **private_key** (optional): hex private key; if omitted, loads from wallet
- **Returns:** `{ success, oracle_id, status, message, was_already_running, [warning] }`

### `stoporacle`

Stop a running oracle.

```
stoporacle <oracle_id>
```
- **oracle_id** (required): 0–29
- **Returns:** `{ success, oracle_id, status, message, was_running }`

### `getoraclepubkey`

Get oracle's public key and status. Requires oracle to be initialized via `startoracle` first.

```
getoraclepubkey <oracle_id>
```
- **oracle_id** (required): 0–29
- **Returns:** `{ oracle_id, pubkey, pubkey_full, valid, authorized, is_running }`

### `listoracles`

List all configured oracle nodes from chainparams.

```
listoracles [active_only]
```
- **active_only** (optional, default=false): filter to active oracles only
- **Returns:** Array of `{ oracle_id, pubkey, endpoint, is_active, is_running, is_enabled, last_price, last_update, status, selected_for_epoch }`

### `getoracleprice`

Get current DGB/USD oracle price.

```
getoracleprice
```
- **Returns:** `{ price_micro_usd, price_cents, price_usd, last_update_height, last_update_time, validity_blocks, is_stale, oracle_count, status, 24h_high, 24h_low, volatility }`

### `sendoracleprice` *(testnet/regtest only)*

Broadcast a signed oracle price message using the hardcoded testnet key (`0x01`).

```
sendoracleprice <price_usd> [oracle_id]
```
- **price_usd** (required): price in USD (e.g., `0.05`)
- **oracle_id** (optional, default=1): oracle ID to sign as
- **Returns:** `{ hash, oracle_id, price_micro_usd, price_usd, timestamp, broadcasted }`
- **Only available on testnet/regtest** — uses hardcoded key `0x01` (G point)

### Mock Oracle RPCs *(regtest only)*

| Command | Description |
|---------|-------------|
| `setmockoracleprice <price_micro_usd>` | Set mock oracle price (in micro-USD, e.g. 6500 = $0.0065) |
| `getmockoracleprice` | Get current mock price |
| `simulatepricevolatility <percent>` | Simulate price volatility (e.g. 50 = +50%, -80 = -80%) |
| `enablemockoracle <true\|false>` | Enable/disable mock oracle |

---

## For the Maintainer: Adding an Operator's Key

When an operator sends their `pubkey` (33-byte compressed, e.g. `0398720f...eb7b57`), you must add it to **two locations** in `src/kernel/chainparams.cpp`:

### 1. Add to `vOracleNodes` — Full 33-byte compressed key

Use the operator's `pubkey` exactly as they sent it (with the `02`/`03` prefix):

```cpp
// pubkey goes here as-is (33-byte compressed, 02/03 prefix included)
{5, ParsePubKey("0398720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57"), "operator.server.com:12030", true},
```

### 2. Add to `consensus.vOraclePublicKeys` — 32-byte x-only key

Strip the first byte (`02` or `03`) from the operator's `pubkey` to get the x-only format:

```
Operator sends:  0398720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57
                 ^^
                 Strip this prefix byte

X-only result:   98720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57
```

```cpp
// pubkey_xonly goes here (32-byte, 02/03 prefix REMOVED)
consensus.vOraclePublicKeys.push_back("98720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57");
```

**⚠️ Both locations MUST be updated for the same oracle ID and MUST correspond to the same key.** If they don't match, the oracle will fail key validation at startup (`ValidateOracleKey()` compares the wallet key against the chainparams x-only key).

Recompile and distribute the updated binary.

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

## Code References

| Component | File |
|-----------|------|
| All oracle RPCs | `src/rpc/digidollar.cpp` |
| `createoraclekey` | `src/rpc/digidollar.cpp` ~line 2680 |
| `startoracle` | `src/rpc/digidollar.cpp` ~line 2763 |
| Wallet RPC registration | `src/wallet/rpc/wallet.cpp` (lines 969–970) |
| Non-wallet RPC registration | `src/rpc/digidollar.cpp` ~line 3310 |
| Wallet DB: oracle key storage | `src/wallet/walletdb.cpp` (WriteOracleKey/ReadOracleKey) |
| CWallet key methods | `src/wallet/wallet.cpp` (StoreOracleKey/GetOracleKey) |
| Oracle constants | `src/primitives/oracle.h` (ORACLE_TOTAL_COUNT=30) |
| Chainparams (mainnet oracles) | `src/kernel/chainparams.cpp` ~line 320 |
| Chainparams (testnet oracles) | `src/kernel/chainparams.cpp` ~line 576 |
| Chainparams (regtest oracles) | `src/kernel/chainparams.cpp` ~line 997 |

---

## Fixing Wallet Name

If you created your wallet using the full file path instead of just the name, `getwalletinfo` will show the path as the wallet name (e.g., `"/home/user/.digibyte/testnet13/wallets/oracle/"`).

To fix this:

```bash
# 1. Unload using the full path name it was created with
./src/digibyte-cli -testnet unloadwallet "/home/user/.digibyte/testnet13/wallets/oracle/"

# 2. Reload using just the short name
./src/digibyte-cli -testnet loadwallet "oracle"

# 3. Verify
./src/digibyte-cli -testnet -rpcwallet=oracle getwalletinfo
```

The `walletname` field should now show just `oracle`. Your wallet data is unchanged — this only fixes how it's referenced.

---

*This guide consolidates and supersedes the previous `ORACLE_OPERATOR_GUIDE.md`, `DIGIDOLLAR_TESTNET_ORACLE_SETUP.md`, and `DIGIDOLLAR_ORACLE_SETUP_COMPLETE_GUIDE.md`. Verified against DigiByte Core RC16 source code.*
