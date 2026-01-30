# DigiDollar Oracle Setup Guide
*The single source of truth for oracle operator setup — RC12, Phase 2 Multi-Oracle*

---

## Overview

DigiDollar requires oracle operators to provide real-time DGB/USD price feeds. Oracle public keys are **hardcoded in `src/kernel/chainparams.cpp`**. Each operator:

1. Runs DigiByte Core RC12 with a descriptor wallet
2. Generates an oracle keypair via `createoraclekey`
3. Sends their **public key only** to the DigiByte Core maintainer
4. The maintainer adds the key to `chainparams.cpp` and ships a new release
5. The operator runs `startoracle` — the wallet provides the private key automatically

### Phase 2 Consensus Parameters

| Network | Total Slots | Active Oracles | Consensus Required | Phase 2 Activation |
|---------|-------------|----------------|--------------------|--------------------|
| **Mainnet** | 30 (IDs 0–29) | 15 | 8-of-15 | Disabled (`INT_MAX`) |
| **Testnet** | 10 (IDs 0–9) | 5 (IDs 0–4) | 3-of-5 | Block 100 |
| **Regtest** | 5 (IDs 0–4) | 5 | 3-of-5 | Block 100 |

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
[test]
server=1
txindex=1
debug=digidollar
debug=net
listen=1
port=12030
rpcport=14025
rpcallowip=127.0.0.1
rpcbind=127.0.0.1

# Optional: CoinMarketCap API key for additional price source
# coinmarketcap-api-key=YOUR_CMC_API_KEY
```

> **Important:** Put testnet settings under the `[test]` section header, not globally.

---

## Step-by-Step: Oracle Operator Setup

### Step 1: Start the Node

```bash
./src/digibyted -testnet -daemon
./src/digibyte-cli -testnet getblockchaininfo
```

### Step 2: Create a Descriptor Wallet

RC12 creates descriptor wallets by default:

```bash
./src/digibyte-cli -testnet createwallet "oracle"
```

### Step 3: Generate Your Oracle Key

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

## What Your Oracle Does

Once running, the oracle automatically:
- Fetches DGB/USD prices from multiple exchanges every 15 seconds
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
| CoinMarketCap | **Yes** (optional, add `coinmarketcap-api-key` to config) |

Minimum 2 valid sources required for a price to be accepted.

---

## Multi-Oracle Setup (Phase 2)

Phase 2 enables multi-oracle consensus. On testnet, this means **3-of-5 oracles must agree** on a price before it's accepted.

### Key Consensus Parameters (from `chainparams.cpp`)

| Parameter | Testnet | Regtest | Mainnet |
|-----------|---------|---------|---------|
| `nOracleActivationHeight` | 1 | 1 | Disabled (`INT_MAX`) |
| `nDigiDollarPhase2Height` | 100 | 100 | Disabled (`INT_MAX`) |
| `nOracleRequiredMessages` | 3 | 3 | 8 |
| `nOracleTotalOracles` | 5 | 5 | 15 |
| `nOracleEpochLength` | 1440 blocks | 144 blocks | 1440 blocks |
| `nDDOracleEpochBlocks` | 50 | 10 | 100 |
| `nDDOracleUpdateInterval` | 2 blocks | 1 block | 4 blocks |
| `nDDActivationHeight` | 550 | 650 | 22,000,000 |

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

*This guide consolidates and supersedes the previous `ORACLE_OPERATOR_GUIDE.md`, `DIGIDOLLAR_TESTNET_ORACLE_SETUP.md`, and `DIGIDOLLAR_ORACLE_SETUP_COMPLETE_GUIDE.md`. Verified against DigiByte Core RC12 source code.*
