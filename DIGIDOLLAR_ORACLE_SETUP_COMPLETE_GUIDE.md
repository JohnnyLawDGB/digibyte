# DigiDollar Oracle Setup - Complete Technical Guide

## Overview

This document explains everything I did to set up the DigiDollar oracle system for testnet, including the bugs I found and fixed, how the oracle currently runs, and what you need to know going forward.

## What We Built

The DigiDollar oracle system is now operational on testnet with:
- **1-of-1 Oracle Consensus** (Phase One - single oracle requirement)
- **Oracle Activation Height**: Block 650
- **Testnet Oracle Key**: Private key `0x01` (well-known test key)
- **Working RPC Commands**: `sendoracleprice`, `listoracles`

---

## Part 1: Bugs Found and Fixed

### Bug #1: Oracle Manager Never Initialized (src/init.cpp)

**The Problem**: `OracleBundleManager::Initialize()` was never called during node startup. Without this call, the oracle system used default (mainnet) parameters, requiring 8 oracles instead of 1.

**Location**: `src/init.cpp` around line 2050

**The Fix**: Added initialization call before setting the connman:
```cpp
// Initialize Oracle Bundle Manager with consensus parameters
OracleBundleManager::Initialize();
// Initialize oracle P2P connection for broadcasting
OracleBundleManager::GetInstance().SetConnman(node.connman.get());
```

**How I found it**: When running `listoracles`, it showed "min_oracle_count: 8" instead of 1. Traced through the code to find `OracleBundleManager::Initialize()` was implemented but never called.

---

### Bug #2: Price Validation Using Wrong Units (src/primitives/oracle.cpp)

**The Problem**: The price validation was checking against wrong constants. Comments said "cents" but the actual format is "micro-USD" (1,000,000 = $1.00). A price of $0.05 (50,000 micro-USD) was failing validation because MAX_PRICE was only 1000.

**Location**: `src/primitives/oracle.cpp` lines 30-39

**Before (broken)**:
```cpp
// DigiDollar cents format: 100 cents = $1.00
static constexpr uint64_t MIN_PRICE_CENTS = 1;        // $0.01
static constexpr uint64_t MAX_PRICE_CENTS = 1000;     // $10.00
```

**After (fixed)**:
```cpp
// price_micro_usd format: 1,000,000 micro-USD = $1.00
// Realistic DGB price range: $0.0001 to $100.00
static constexpr uint64_t MIN_PRICE_MICRO_USD = 100;         // $0.0001
static constexpr uint64_t MAX_PRICE_MICRO_USD = 100000000;   // $100.00
```

**How I found it**: `sendoracleprice 0.05` was returning "Failed to create valid oracle message". Added debug logging and found `IsValid()` returning false due to price range check.

---

### Bug #3: Oracle Messages Not Signed (src/rpc/digidollar.cpp)

**The Problem**: The `sendoracleprice` RPC created oracle messages but never signed them. When the message was added to the bundle manager, it called `IsValidOracleMessage()` which calls `Verify()`, and unsigned messages always fail verification.

**Location**: `src/rpc/digidollar.cpp` lines 2027-2043

**Before (broken)**:
```cpp
// For testnet, we need to sign with the oracle's private key
// This would normally be done by the oracle operator daemon
// TODO: Add proper key management for testnet oracle operators
```

**After (fixed)**:
```cpp
// Phase One testnet: Sign with hardcoded oracle key
// Private key = 0x01, Public key = G (generator point)
// This is a well-known test key - NEVER use on mainnet
CKey oracle_key;
std::vector<unsigned char> keydata = ParseHex(
    "0000000000000000000000000000000000000000000000000000000000000001"
);
oracle_key.Set(keydata.begin(), keydata.end(), true);

if (!oracle_key.IsValid()) {
    throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to create oracle key");
}

// Sign the message
if (!msg.Sign(oracle_key)) {
    throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to sign oracle message");
}
```

**How I found it**: After fixing Bug #2, `sendoracleprice` still failed with "Failed to add oracle message to bundle manager". Traced through `OracleBundleManager::AddOracleMessage()` which calls `IsValidOracleMessage()` which calls `msg.Verify()`. Empty signature = fail.

---

## Part 2: Current Testnet Configuration

### Config File: `~/.digibyte/digibyte.conf`

```ini
# Main network defaults (if ever used)
[main]
# Nothing special

# Testnet configuration
[test]
server=1
txindex=1

# RPC Settings
rpcuser=digibyterpc
rpcpassword=testnet_oracle_phase1_2025

# Oracle/DigiDollar debugging
debug=digidollar
debug=net

# Network
listen=1
port=12028
rpcport=14024

# Allow local RPC
rpcallowip=127.0.0.1
rpcbind=127.0.0.1

# Increase connections for testnet
maxconnections=40
```

**Important**: Settings must be under `[test]` section for testnet, NOT global.

### Data Directories

| Component | Location |
|-----------|----------|
| Config | `~/.digibyte/digibyte.conf` |
| Testnet Data | `~/.digibyte/testnet4/` |
| Debug Log | `~/.digibyte/testnet4/debug.log` |
| Wallet | `~/.digibyte/testnet4/wallets/testnet_oracle/` |

---

## Part 3: How the Oracle Runs

### Current State (as of this session)

- **Testnet Height**: 7 blocks
- **Wallet Name**: `testnet_oracle`
- **Mining Address**: `dgbt1ql0xcy0q40ed4wanw3667a5rqagtms52992lytm`
- **Oracle Status**: Initialized with 1-of-1 consensus

### Oracle Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    DigiByte Core Node                        │
│                                                              │
│  ┌─────────────────┐    ┌──────────────────────────────┐   │
│  │  OracleManager  │────│   OracleBundleManager        │   │
│  │  (Price Fetch)  │    │   (Message Validation)       │   │
│  └────────┬────────┘    └───────────────┬──────────────┘   │
│           │                             │                   │
│           ▼                             ▼                   │
│  ┌─────────────────┐    ┌──────────────────────────────┐   │
│  │  Exchange APIs  │    │   P2P Network Broadcast      │   │
│  │  (8 sources)    │    │   (via CConnman)             │   │
│  └─────────────────┘    └──────────────────────────────┘   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Oracle Message Flow

1. **Price Creation**: RPC `sendoracleprice 0.05` creates `COraclePriceMessage`
2. **Signing**: Message signed with testnet oracle key (private key `0x01`)
3. **Validation**: `IsValid()` checks price range, timestamp freshness
4. **Bundle Addition**: `OracleBundleManager::AddOracleMessage()` stores and broadcasts
5. **P2P Broadcast**: Message sent to connected peers
6. **Consensus**: Other nodes validate and include in their bundles

### Oracle Key (Phase One Testnet)

| Property | Value |
|----------|-------|
| Private Key | `0x0000000000000000000000000000000000000000000000000000000000000001` |
| Public Key | `0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798` |
| Type | secp256k1 Generator Point (G) |
| Security | **ZERO** - well-known test key, TESTNET ONLY |

---

## Part 4: RPC Commands

### Working Commands

```bash
# List all configured oracles
./src/digibyte-cli -testnet listoracles

# Send a price update ($0.05 per DGB)
./src/digibyte-cli -testnet sendoracleprice 0.05

# Get current block height
./src/digibyte-cli -testnet getblockcount

# Mine blocks (scrypt algorithm, slow on CPU)
./src/digibyte-cli -testnet generatetoaddress 10 "dgbt1ql0xcy0q40ed4wanw3667a5rqagtms52992lytm"

# Check network info
./src/digibyte-cli -testnet getnetworkinfo
```

### Expected Commands (After Activation at Height 650)

```bash
# Get oracle price consensus
./src/digibyte-cli -testnet getoracleprice

# Get DigiDollar system stats
./src/digibyte-cli -testnet getdigidollarstats

# Mint DigiDollars (after oracle active)
./src/digibyte-cli -testnet mintdigidollar <dgb_amount> <tier>
```

---

## Part 5: How to Continue Testing

### Step 1: Mine to Activation Height

The DigiDollar/Oracle features activate at block 650:

```bash
# Check current height
./src/digibyte-cli -testnet getblockcount

# Mine blocks (be patient - scrypt PoW is slow on CPU)
./src/digibyte-cli -testnet generatetoaddress 100 "dgbt1ql0xcy0q40ed4wanw3667a5rqagtms52992lytm"
```

**Note**: Mining uses Scrypt PoW which is intentionally slow on CPU. Each block may take several seconds.

### Step 2: Test Oracle Price Submission

Once at height 650+:

```bash
# Submit a price ($0.05)
./src/digibyte-cli -testnet sendoracleprice 0.05

# Verify it was accepted
./src/digibyte-cli -testnet listoracles
```

### Step 3: Monitor Debug Log

```bash
# Watch oracle activity
tail -f ~/.digibyte/testnet4/debug.log | grep -i "oracle\|digidollar"
```

---

## Part 6: Exchange API Sources

The oracle aggregator is configured to fetch from these exchanges:

| Exchange | Requires API Key | Endpoint |
|----------|------------------|----------|
| Binance | No | `api.binance.com/api/v3/ticker/price?symbol=DGBUSDT` |
| CoinGecko | No | `api.coingecko.com/api/v3/simple/price?ids=digibyte&vs_currencies=usd` |
| Coinbase | No | `api.coinbase.com/v2/prices/DGB-USD/spot` |
| Kraken | No | `api.kraken.com/0/public/Ticker?pair=DGBUSD` |
| KuCoin | No | `api.kucoin.com/api/v1/market/orderbook/level1?symbol=DGB-USDT` |
| Crypto.com | No | `api.crypto.com/v2/public/get-ticker?instrument_name=DGB_USD` |
| Messari | No | `data.messari.io/api/v1/assets/dgb/metrics/market-data` |
| CoinMarketCap | **Yes** | `pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest` |

### Getting a CoinMarketCap API Key (Optional)

1. Go to https://pro.coinmarketcap.com/signup
2. Sign up for free Basic plan (10,000 calls/month)
3. Copy your API key
4. Add to config: `coinmarketcap-api-key=YOUR_KEY`

---

## Part 7: Files Modified Summary

| File | Change | Purpose |
|------|--------|---------|
| `src/init.cpp` | Added `OracleBundleManager::Initialize()` | Enable testnet oracle consensus params |
| `src/primitives/oracle.cpp` | Fixed price validation constants | Allow realistic DGB prices |
| `src/rpc/digidollar.cpp` | Added message signing | Enable valid oracle message creation |
| `~/.digibyte/digibyte.conf` | Created testnet config | Node configuration |

---

## Part 8: Key Concepts

### Oracle Consensus (Phase One)

- **Requirement**: 1-of-1 (single oracle needed)
- **Epoch Length**: 1440 blocks (~6 hours)
- **Price Update Interval**: Every 2 blocks (~30 seconds)

### Price Format

- **Internal Format**: micro-USD (1,000,000 = $1.00)
- **RPC Input**: USD (0.05 = $0.05)
- **Conversion**: `price_micro_usd = usd_price * 1,000,000`

### Activation Heights (Testnet)

| Feature | Activation Height |
|---------|-------------------|
| DigiDollar | 650 |
| Oracle System | 650 |
| DD Operations | 650 |

---

## Troubleshooting

### "Failed to create valid oracle message"
- **Cause**: Price out of range
- **Solution**: Use price between $0.0001 and $100.00

### "Failed to add oracle message to bundle manager"
- **Cause**: Message not properly signed
- **Solution**: This is now fixed in the code

### "min_oracle_count: 8" instead of 1
- **Cause**: `OracleBundleManager::Initialize()` not called
- **Solution**: This is now fixed in init.cpp

### Mining is very slow
- **Cause**: Scrypt PoW is CPU-intensive by design
- **Solution**: Be patient or use multiple `generatetoaddress` calls in background

---

## Security Notes

⚠️ **TESTNET ONLY**: Everything in this setup uses well-known test keys that provide ZERO security. This is intentional for Phase One testing.

For mainnet (future phases):
- Oracle operators will generate unique private keys
- Keys registered through governance process
- Multi-oracle consensus (7-of-15 or higher)
- No hardcoded keys in RPC commands

---

*Document created during DigiDollar Phase One testnet oracle setup session.*
