# DigiDollar Oracle Operator Guide
*How to become an oracle operator and get your key into DigiByte Core*

---

## Overview

DigiDollar requires oracle operators to provide real-time DGB/USD price feeds. Oracle public keys are **hardcoded in `src/kernel/chainparams.cpp`** — every oracle operator must:

1. Run a current DigiByte Core release (RC30 or later; RC33 is the latest at the time of writing) and create a descriptor wallet
2. Run `createoraclekey` to generate their oracle keypair inside the wallet
3. Send their **public key only** to the DigiByte Core maintainer
4. The maintainer adds their key to `chainparams.cpp` and ships a new release
5. The operator runs `startoracle` — the wallet provides the private key automatically

For testnet release/migration mechanics (testnet23, port 12030, RC29→RC30 cutover), follow `DIGIDOLLAR_ORACLE_SETUP.md`.

---

## Step-by-Step: For Oracle Operators

### Step 1: Compile and Run DigiByte Core (RC30+)

```bash
cd ~/Code/digibyte
./autogen.sh
./configure
make -j$(nproc)
```

Start on testnet:
```bash
./src/digibyted -testnet
```

### Step 2: Create a Descriptor Wallet

Current releases create descriptor wallets by default. No special flags needed.

```bash
./src/digibyte-cli -testnet createwallet "oracle"
```

### Step 3: Generate Your Oracle Key

```bash
./src/digibyte-cli -testnet -rpcwallet=oracle createoraclekey 0
```

Replace `0` with the oracle ID slot assigned to you by the maintainer.

The chainparams `vOracleNodes` table allocates 30 mainnet/testnet slots (IDs 0–29), but only slots **0–16** are part of the active 17-of-17 roster (`consensus.vOraclePublicKeys`) participating in 9-of-17 MuSig2 consensus. Slots 17–29 are reserve placeholders and are not validated against MuSig2 quorum. Regtest has 7 slots (IDs 0–6) with 4-of-7 consensus.

**Output:**
```json
{
  "oracle_id": 0,
  "pubkey": "0398720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57",
  "pubkey_xonly": "98720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57",
  "stored_in_wallet": true,
  "message": "Oracle key generated and stored in wallet. Share ONLY the pubkey..."
}
```

**What happened:**
- A new secp256k1 keypair was generated using the wallet's secure random number generator
- The **private key** was stored securely in the wallet database (key: `oraclekey`)
- The **compressed public key** (33 bytes, 02/03 prefix) was returned for you to share
- The **x-only public key** (32 bytes, for Schnorr) was also returned

### Step 4: Send Your Public Key to the Maintainer

Send **only these two things**:
1. Your **pubkey** from the output above (66-char hex starting with `02` or `03`)
2. Your **server endpoint** (e.g., `myserver.com:12028`)

**⚠️ NEVER share your private key. It stays in your wallet.**

### Step 5: Wait for Updated Release

The maintainer adds your key to `chainparams.cpp` and releases an updated binary.

### Step 6: Download the Updated Binary and Start Your Oracle

```bash
# Start the node
./src/digibyted -testnet

# Start your oracle — key loads automatically from wallet!
./src/digibyte-cli -testnet -rpcwallet=oracle startoracle 0
```

**No private key argument needed.** The `startoracle` command automatically retrieves your private key from the wallet where `createoraclekey` stored it.

**Output (testnet):**
```json
{
  "success": true,
  "oracle_id": 0,
  "status": "running",
  "message": "Oracle started with key loaded from wallet 'oracle'"
}
```

You can also provide the key explicitly if needed:
```bash
./src/digibyte-cli -testnet startoracle 0 "raw_hex_private_key"
```

### Step 7: Verify Your Oracle is Running

```bash
# Check oracle status
./src/digibyte-cli -testnet getoraclepubkey 0

# Expected output:
# "authorized": true    ← Your key matches chainparams
# "is_running": true    ← Price thread is active
```

### Step 8: Monitor

```bash
tail -f ~/.digibyte/testnet4/debug.log | grep -i oracle
```

---

## What Your Oracle Does

Once running, your oracle automatically:
- Fetches DGB/USD prices from 7 exchanges every 15 seconds (Binance, KuCoin, Gate.io, HTX, Crypto.com, CoinGecko, CoinMarketCap)
- Calculates median price with MAD outlier filtering
- Signs the price with BIP-340 Schnorr using your wallet-stored private key
- Broadcasts the signed 128-byte message to the P2P network

---

## Important Notes

- **Key persists in wallet** — Your oracle key survives wallet unload/reload. But after restarting `digibyted`, you need to run `startoracle` again.
- **One key per oracle ID** — `createoraclekey` rejects if a key already exists for that ID. This prevents accidental overwrites.
- **Descriptor wallets** are the default in current releases. `dumpprivkey` exists for legacy wallets; descriptor-wallet operators do not need it because the oracle private key is stored under the `oraclekey` record (see `WriteOracleKey/ReadOracleKey` in `src/wallet/walletdb.cpp` and `StoreOracleKey/GetOracleKey` in `src/wallet/wallet.cpp`).
- **Backup your wallet** — `backupwallet` includes your oracle key. Losing the wallet means losing your oracle key.

---

## For the Maintainer: Adding an Operator's Key

When an operator sends you their 33-byte compressed public key, add it to **two places** in `src/kernel/chainparams.cpp`:

### 1. vOracleNodes (33-byte compressed CPubKey)

In `InitializeOracleNodes()`:
```cpp
{5, ParsePubKey("0398720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57"), "operator.server.com:12028", true},
```

### 2. consensus.vOraclePublicKeys (32-byte x-only key — strip the 02/03 prefix)

```cpp
consensus.vOraclePublicKeys.push_back("98720f6d15252fb2c3501107d46129589d8ab56e0f967be2e470f40675eb7b57");
```

**⚠️ Both locations must be updated.** `vOracleNodes` uses 33-byte compressed keys. `consensus.vOraclePublicKeys` uses 32-byte x-only keys. They must match.

Then recompile and distribute the updated binary.

---

## Oracle Slots

| Network | Total Slots | Active (in MuSig2 quorum) | Consensus | Notes |
|---------|------------|---------------------------|-----------|-------|
| Mainnet | 30 (IDs 0–29) | 17 (slots 0–16) | 9-of-17 MuSig2 (RC30) | Slots 17–29 are reserve placeholders, not in `consensus.vOraclePublicKeys`. Active for use only after BIP9 activation at min height 22,014,720. |
| Testnet | 30 (IDs 0–29) | 17 (slots 0–16) | 9-of-17 MuSig2 (RC30) | Active on testnet23 from height 600. |
| Regtest | 7 (IDs 0–6) | 7 | 4-of-7 MuSig2 | Always active. |

---

## Server Requirements

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| Uptime | 95% | 99.9% |
| RAM | 2 GB | 4+ GB |
| Disk | 20 GB | 50+ GB SSD |
| Network | Outbound HTTPS | Static IP or DNS |
| Ports | 12028 (testnet) | Open inbound + outbound |

---

## RPC Command Reference

| Command | Description |
|---------|-------------|
| `createoraclekey <oracle_id>` | Generate oracle keypair in wallet (NEW) |
| `startoracle <id> [privkey_hex]` | Start oracle — loads from wallet if no privkey given |
| `stoporacle <id>` | Stop oracle price thread |
| `getoraclepubkey <id>` | Check oracle key and status |
| `listoracles [active_only]` | List all configured oracles |
| `submitoracleprice <price>` | Submit price (regtest / Phase 2 oracle testing only) |
| `getoracleprice` | Get current oracle price |

---

## Code References

| Component | File | Key Lines |
|-----------|------|-----------|
| `createoraclekey` RPC | `src/rpc/digidollar.cpp` | ~line 4033 |
| `startoracle` RPC (wallet loading) | `src/rpc/digidollar.cpp` | ~line 4137 |
| Wallet DB storage | `src/wallet/walletdb.cpp` | `WriteOracleKey` / `ReadOracleKey` |
| CWallet key methods | `src/wallet/wallet.cpp` | `StoreOracleKey` / `GetOracleKey` |
| OracleNodeInfo struct | `src/primitives/oracle.h` | OracleNodeInfo |
| chainparams oracle slots | `src/kernel/chainparams.cpp` | `InitializeOracleNodes()`, `vOraclePublicKeys` |
| Unit tests | `src/test/oracle_wallet_key_tests.cpp` | Wallet key generation / persistence |
| Functional test | `test/functional/digidollar_oracle_keygen.py` | End-to-end keygen + start |

---

*Verified against the DigiByte Core RC30+ codebase on `feature/digidollar-v1`. All RPC commands tested in regtest.*
