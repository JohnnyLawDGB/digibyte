# DigiDollar Oracle System - Complete Architecture Documentation
**DigiByte v8.26 - Oracle Phase One Implementation with Phase Two Preparation**
*Updated: 2026-02-01*
*Implementation Status: ~70% Complete Phase One (Testnet/Regtest Only), Mainnet DISABLED*
*Document Version: 7.0 - Validated against actual codebase*
*Validation Status: ⚠️ DEVELOPMENT BUILD - NOT PRODUCTION READY*

> **⚠️ CRITICAL NOTICE**: This document reflects the ACTUAL state of the code as of March 2026.
> Mainnet oracle validation is completely disabled (returns true at bundle_manager.cpp:2229).
> Several components have stubs, TODOs, and mock implementations that leak into production code paths.

---

## Table of Contents

### Part I: Executive Summary & Overview
1. [Executive Summary](#1-executive-summary)
2. [Quick Start Guide](#2-quick-start-guide)
3. [System Architecture Overview](#3-system-architecture-overview)

### Part II: Core Components (Deep Dive)
4. [Data Structures & Serialization](#4-data-structures--serialization)
5. [Block Validation & Consensus Rules](#5-block-validation--consensus-rules)
6. [P2P Networking & Message Broadcasting](#6-p2p-networking--message-broadcasting)
7. [Exchange API Integration](#7-exchange-api-integration)

### Part III: Testing & Quality Assurance
8. [Test Suite Documentation](#8-test-suite-documentation)
9. [Validation Flows](#9-validation-flows)

### Part IV: Operational Guide
10. [Configuration & Deployment](#10-configuration--deployment)
11. [Monitoring & Troubleshooting](#11-monitoring--troubleshooting)
12. [Performance & Security](#12-performance--security)

### Part V: Reference
13. [API Reference](#13-api-reference)
14. [Phase Two Roadmap](#14-phase-two-roadmap)
15. [Glossary](#15-glossary)

---

# Part I: Executive Summary & Overview

## 1. Executive Summary

### 1.1 What is the Oracle System?

The Oracle System provides **decentralized price feeds** for the DigiByte blockchain, enabling DigiDollar's collateralized stablecoin functionality. It aggregates DGB/USD prices from 6 working exchanges (out of 11 fetcher classes defined) and embeds this data directly into the blockchain.

> **Note**: 5 exchange fetchers are currently broken or removed (Coinbase, Kraken, Messari, Bittrex/Poloniex - broken; CoinMarketCap - removed as paid API incompatible with decentralized design).

**Real-World Analogy**: Like a trusted appraiser network that provides gold prices for a bank's collateral system - but decentralized, cryptographically signed, and embedded in every block.

### 1.2 Phase One Design Philosophy

**Core Principle: Simplicity First**

Phase One implements a **streamlined, testnet-ready system** with:
- **Single Oracle** (1-of-1 consensus) for testing
- **Compact Format** (22 bytes) fitting in OP_RETURN
- **No Embedded Signatures** (trust based on chainparams)
- **6 Working Exchange APIs** with median aggregation (Binance, KuCoin, Gate.io, HTX, Crypto.com, CoinGecko)
- **5 Broken/Removed Exchange APIs** (Coinbase, Kraken, Messari, Bittrex/Poloniex - broken; CoinMarketCap - removed)
- **15-second updates** (aligned with DigiByte block time)

**Trade-off Analysis:**
```
✅ BENEFITS:
- Fits in 83-byte OP_RETURN limit (26.5% utilization)
- Fast block validation (< 1ms overhead)
- Simple testnet deployment
- Clean upgrade path to Phase Two

⚠️ TRADE-OFFS:
- No on-chain signature verification (compact format)
- Single point of failure (1-of-1 consensus)
- Limited to testnet/regtest (mainnet requires Phase Two)

🚨 CRITICAL ISSUES (Current State) - ALL VERIFIED:
- Mainnet validation DISABLED (bundle_manager.cpp:2229 returns true)
- MockOracleManager singleton instantiated globally (err.cpp:403 guarded by regtest check at line 402)
- ERR system health hardcoded to 150% (txbuilder.cpp:29 DEFAULT_SYSTEM_COLLATERAL=150)
- sendoracleprice RPC REMOVED (security vulnerability - fake price injection, digidollar.cpp:3642)
- GetBestHeight() returns hardcoded 0 (bundle_manager.cpp:47-51)
```

### 1.3 Implementation Status

**⚠️ PHASE ONE: ~70% COMPLETE (TESTNET/REGTEST ONLY)**

**What's Working:**
- ✅ OP_ORACLE opcode (0xbf) integrated
- ✅ Compact 22-byte oracle format
- ✅ P2P message handling (ORACLEPRICE, ORACLEBUNDLE, GETORACLES)
- ✅ 6 working exchange APIs (Binance, KuCoin, Gate.io, HTX, Crypto.com, CoinGecko)
- ✅ Block validation on TESTNET (activation height 600) and REGTEST (activation height 650)
- ✅ Price cache (ConnectBlock/DisconnectBlock)
- ✅ Schnorr signatures (BIP-340)
- ✅ Timestamp validation (1-hour window)
- ✅ Data structures (COraclePriceMessage, COracleBundle, OracleNodeInfo)

**What's NOT Working / Incomplete:**
- ❌ **MAINNET VALIDATION DISABLED** - bundle_manager.cpp:2229 returns true immediately
- ❌ 5 broken/removed exchange APIs (Coinbase, Kraken, Messari, Bittrex/Poloniex; CoinMarketCap removed)
- ❌ `sendoracleprice` RPC REMOVED (security vulnerability - fake price injection)
- ❌ `GetBestHeight()` returns hardcoded 0 (bundle_manager.cpp:47-51)
- ❌ MockOracleManager singleton instantiated globally (err.cpp:403 guarded by regtest check at line 402, but singleton available on all networks)
- ❌ ERR system health hardcoded to 150% - ERR ratio can never activate
- ❌ Empty schnorr_sig accepted without verification (signature bypass)

**Test Coverage:**
```
Unit Tests:        826 tests across 38 test files ✅
  - digidollar_*_tests.cpp:            Multiple test suites
  - oracle_*_tests.cpp:                Oracle-specific tests
  - (Many more than originally documented)

Functional Tests:    36 digidollar-related test files ✅
  - test/functional/digidollar_*.py:   Full integration testing

TOTAL: 826+ unit tests + 36 functional tests
```

**Implementation Status: ~70% Complete - TESTNET/REGTEST ONLY**

---

## 2. Quick Start Guide

### 2.1 For Users: What Does This Mean?

**If you're minting DigiDollars:**
- The oracle tells the blockchain how much your DGB collateral is worth
- You need 200% collateral (e.g., $200 of DGB to mint $100 DigiDollar)
- The oracle updates every 15 seconds with fresh exchange prices

**If you're running a node:**
- Your node validates oracle data in every block (after activation height)
- No setup needed - validation happens automatically
- Enable `-debug=digidollar` to see oracle activity

### 2.2 For Developers: Integration Points

```cpp
// 1. Get current oracle price
OracleBundleManager& manager = OracleBundleManager::GetInstance();
CAmount price_micro_usd = manager.GetLatestPrice();
// Returns: 50000 for $0.05/DGB

// 2. Get price at specific height
CAmount historical_price = manager.GetOraclePriceForHeight(block_height);

// 3. Check oracle system status
bool enabled = manager.IsEnabled();
int min_oracles = manager.GetMinOracleCount(); // Phase One: 1
```

### 2.3 Key Files Quick Reference

```
Core Implementation:
├── src/script/script.h                    [OP_ORACLE definition]
├── src/primitives/oracle.{h,cpp}          [Data structures - WORKING]
├── src/oracle/bundle_manager.{h,cpp}      [Bundle logic - GetBestHeight() STUB]
├── src/oracle/exchange.{h,cpp}            [6 active exchange APIs: Binance, KuCoin, Gate.io, HTX, Crypto.com, CoinGecko]
├── src/oracle/mock_oracle.{h,cpp}         [Regtest helper only — OP_CHECKPRICE no longer falls back to mock prices in production (commit f77678cd0f); fails closed when no live oracle consensus]
├── src/oracle/musig2_*.{h,cpp}            [Phase 3 MuSig2 aggregator/session/orchestrator/messages — WORKING]
├── src/oracle/signing_orchestrator.{h,cpp}[CValidationInterface; Phase 3 P2P round 1/2 driver — WORKING with rh58 partialsig DoS cap]
├── src/validation.cpp                     [Block + mempool validation, UpdatePriceCache gated on DEPLOYMENT_DIGIDOLLAR (rh61)]
├── src/net_processing.cpp                 [P2P handlers — ORACLEPRICE/BUNDLE/CONSENSUS/ATTESTATION/MUSIGNONCE/MUSIGPARTIALSIG/GETORACLES, rate limited, gated on IsOracleActive]
├── src/rpc/digidollar.cpp                 [18 base RPCs; sendoracleprice REMOVED (security vuln); submitoracleprice = regtest/Phase 2 testing]
├── src/wallet/rpc/wallet.cpp              [13 wallet-context DD/oracle RPCs incl. createoraclekey, startoracle, sendmanydigidollar]
├── src/consensus/err.cpp                  [ERR system]
└── src/kernel/chainparams.cpp             [Oracle authorization: mainnet/testnet 17 active slots (0–16), regtest 7 slots; testnet/mainnet validator parity — short-circuit removed (commit f0d9a7b2c7)]

Test Suite (826 unit tests + 36 functional):
├── src/test/digidollar_*_tests.cpp             [Multiple test suites]
├── src/test/oracle_*_tests.cpp                 [Oracle-specific tests]
├── src/test/err_*_tests.cpp                    [ERR system tests]
└── test/functional/digidollar_*.py             [36 functional tests]

Known Stubs/TODOs (VERIFIED):
├── bundle_manager.cpp:2229: Mainnet validation returns true (DISABLED)
├── bundle_manager.cpp:47-51: GetBestHeight() returns hardcoded 0
├── txbuilder.cpp:29,273: GetCurrentSystemCollateral() returns 150%
├── err.cpp:403: MockOracleManager guarded by regtest check at line 402, but singleton instantiated globally
└── digidollar.cpp:3642: sendoracleprice REMOVED (security vulnerability)
```

---

## 3. System Architecture Overview

### 3.1 The Complete Data Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ORACLE SYSTEM: COMPLETE DATA FLOW                     │
└─────────────────────────────────────────────────────────────────────────┘

PHASE 1: PRICE DISCOVERY (Every 15 seconds)
═══════════════════════════════════════════

Exchange APIs (6 working exchanges, parallel fetching - 5 broken/removed not shown):
┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│  Binance    │  │   KuCoin    │  │  Gate.io    │  │    HTX      │
│ DGB/USDT    │  │  DGB/USDT   │  │ DGB_USDT    │  │ dgbusdt     │
│ $0.05023    │  │  $0.05017   │  │  $0.05021   │  │  $0.05019   │
└──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘
       └─────────────────┴────────────┬────────────────────┘
                                      │
      ┌─────────────┐  ┌─────────────┐
      │ Crypto.com  │  │  CoinGecko  │
      │  DGB/USD    │  │ (aggregator)│
      │ $0.05020    │  │  $0.05022   │
      └──────┬──────┘  └──────┬──────┘
             └────────────────┘
                                      │
                                      ▼
                    MultiExchangeAggregator
                    ┌─────────────────────────────┐
                    │ 1. Filter failures          │
                    │ 2. Remove outliers          │
                    │ 3. Calculate median         │
                    │ 4. Convert to micro-USD     │
                    └──────────────┬──────────────┘
                                   │
                            Median: $0.05020
                              (50,200 micro-USD)

PHASE 2: MESSAGE CREATION & SIGNING
═══════════════════════════════════

Oracle Node (Authorized, oracle_id=0)
├─► Create COraclePriceMessage:
│     oracle_id:        0
│     price_micro_usd:  50200
│     timestamp:        1732204800
│     block_height:     700
│     nonce:            0x123456789ABCDEF0
│     oracle_pubkey:    XOnlyPubKey (32 bytes)
│
├─► Sign with Schnorr (BIP-340):
│     msg_hash = SHA256d(oracle_id || price || timestamp || ...)
│     schnorr_sig = Sign(msg_hash, oracle_privkey)  [64 bytes]
│
└─► Full Format Message: 128 bytes
      (Used for P2P transmission, NOT stored on-chain)


PHASE 3: BUNDLE AGGREGATION
════════════════════════════

OracleBundleManager
├─► AddOracleMessage(message)
│     - Verify Schnorr signature ✓
│     - Check duplicate (hash-based) ✓
│     - Store in pending_messages[oracle_id=0]
│
├─► TryCreateBundle(epoch)
│     - Phase One: Require exactly 1 message ✓
│     - Set median_price = message.price (trivial for 1 message)
│     - Bundle timestamp = current time
│
└─► COracleBundle Created:
      messages: [message]              [1 message, Phase One]
      median_price_micro_usd: 50200
      timestamp: 1732204800
      epoch: 0


PHASE 4: COMPACT FORMAT ENCODING
═════════════════════════════════

CreateOracleScript(bundle) → Produces 22-byte compact format:

┌──────────────────────────────────────────────────────────┐
│  Byte 0:      0x6a (OP_RETURN)                          │
│  Byte 1:      0xbf (OP_ORACLE)                          │
│  Byte 2:      0x01 (VERSION: Phase One compact)         │
│  Byte 3:      0x00 (ORACLE_ID: 0)                       │
│  Bytes 4-11:  0xD8C40000 00000000 (50200 little-endian) │
│  Bytes 12-19: 0x8080AB67 00000000 (timestamp LE)        │
└──────────────────────────────────────────────────────────┘

Total: 22 bytes (26.5% of 83-byte OP_RETURN limit) ✅

KEY DESIGN DECISION:
- Schnorr signature (64 bytes) NOT included → saves space
- Trust model: Signature verified at creation time
- On-chain: Trust chainparams oracle authorization


PHASE 5: BLOCK INCLUSION
═════════════════════════

Miner (BlockAssembler::CreateNewBlock)
├─► AddOracleBundleToBlock(block, height)
│     - Get latest bundle from OracleBundleManager
│     - CreateOracleScript(bundle) → 22-byte OP_RETURN
│     - Add as coinbase output 1 (output 0 = miner reward)
│
└─► Coinbase Transaction:
      vout[0]: 72000 DGB → miner address (OP_DUP OP_HASH160 ...)
      vout[1]: 0 DGB → OP_RETURN OP_ORACLE <compact_data>


PHASE 6: BLOCK VALIDATION
══════════════════════════

CheckBlock(block, state, params)  [validation.cpp:4373]
├─► OracleDataValidator::ValidateBlockOracleData()
│     │
│     ├─► STEP 1: Extract compact format (22 bytes)
│     │     - Find OP_RETURN output
│     │     - Check byte 1 == OP_ORACLE (0xbf)
│     │     - Parse: version, oracle_id, price, timestamp
│     │     - Populate oracle_pubkey from chainparams ✓
│     │
│     ├─► STEP 2: Validate bundle structure
│     │     - bundle.IsValid() checks:
│     │       • Price range: 100 - 100,000,000 micro-USD ✓
│     │       • Timestamp not in future (+60s tolerance) ✓
│     │       • Signature verification (skip for compact format) ✓
│     │
│     ├─► STEP 3: Enforce Phase One consensus
│     │     - Require exactly 1 message ✓
│     │     - Reject if messages.size() != 1
│     │
│     ├─► STEP 4: Verify median price
│     │     - Phase One: median must equal message.price ✓
│     │
│     ├─► STEP 5: Timestamp validation
│     │     - Age check: block.nTime - msg.timestamp ≤ 3600s ✓
│     │     - Future check: msg.timestamp ≤ block.nTime + 60s ✓
│     │
│     └─► STEP 6: Oracle authorization
│           - Verify oracle_id in chainparams ✓
│           - Check oracle is active ✓
│
└─► Block accepted ✅


PHASE 7: PRICE CACHE UPDATE
════════════════════════════

ConnectBlock(block, state, pindex)  [validation.cpp:~2805]
├─► ExtractOracleBundle(coinbase_tx, bundle)
│     - Parse compact format from OP_RETURN
│     - Reconstruct COracleBundle
│
├─► UpdatePriceCache(height, median_price)
│     - height_to_price[700] = 50200
│     - Keep last 1000 blocks in cache
│     - Thread-safe (mutex-protected)
│
└─► DigiDollar Access:
      OracleIntegration::GetCurrentOraclePrice()
      → Returns: 50200 micro-USD ($0.05020/DGB)


PHASE 8: P2P BROADCASTING (Parallel to Block Inclusion)
═══════════════════════════════════════════════════════

OracleBundleManager::BroadcastMessage(message)  [Full 128-byte format]
├─► Validate message.IsValid() ✓
├─► AddOracleMessage() to local storage ✓
├─► CConnman::ForEachNode([&](CNode* node) {
│       m_connman->PushMessage(node,
│         CNetMsgMaker(version).Make(NetMsgType::ORACLEPRICE, message));
│     });
│
└─► Message propagates to all peers (~2-5 seconds for 95th percentile)

Receiving Node:
├─► ProcessMessage(NetMsgType::ORACLEPRICE)
│     - Rate limit: 3600 msg/hour per peer ✓
│     - Deserialize message ✓
│     - Validate structure, timestamp, signature ✓
│     - Check duplicate (hash-based) ✓
│     - AddOracleMessage() to local storage ✓
│     - Relay to other peers (except sender) ✓
│
└─► Message stored, ready for bundle creation
```

### 3.2 Component Interaction Map

```
┌──────────────────────────────────────────────────────────────────┐
│                    COMPONENT RELATIONSHIPS                        │
└──────────────────────────────────────────────────────────────────┘

External World:
  ┌─────────────────────────────────────────────────────────┐
  │    Exchange APIs (6 WORKING + 5 BROKEN/REMOVED)          │
  │  ✅ Binance • KuCoin • Gate.io • HTX • Crypto.com       │
  │  ✅ CoinGecko                                            │
  │  ❌ Coinbase • Kraken • Messari • Bittrex/Poloniex      │
  │  ❌ CoinMarketCap (removed - paid API)                  │
  └──────────────────┬──────────────────────────────────────┘
                     │ HTTP/HTTPS (libcurl)
                     ▼
Core Oracle Layer:
  ┌──────────────────────────────────────────┐
  │   MultiExchangeAggregator                │
  │   - FetchAllPrices()                     │
  │   - FilterOutliers() [%-threshold]       │
  │   - CalculateMedianPrice()               │
  └──────────────────┬───────────────────────┘
                     │ Median price (micro-USD)
                     ▼
  ┌──────────────────────────────────────────┐
  │   OracleNode (if running oracle)         │
  │   - FetchMedianPrice()                   │
  │   - CreatePriceMessage()                 │
  │   - Sign(oracle_privkey) [Schnorr]      │
  └──────────────────┬───────────────────────┘
                     │ COraclePriceMessage (128B)
                     ▼
Bundle Management:
  ┌──────────────────────────────────────────┐
  │   OracleBundleManager (Singleton)        │
  │   ├─ AddOracleMessage()                  │
  │   ├─ TryCreateBundle()                   │
  │   ├─ CreateOracleScript() [compact 22B]  │
  │   ├─ ExtractOracleBundle()               │
  │   ├─ BroadcastMessage() ◄──┐             │
  │   └─ UpdatePriceCache()     │             │
  └──────────────┬────────────┬─┘             │
                 │            │               │
        ┌────────┘            └───────┐       │
        │                             │       │
        ▼                             ▼       │
P2P Layer:                   Block Layer:     │
┌─────────────────┐         ┌──────────────────────┐
│   CConnman      │         │  BlockAssembler      │
│   - ForEachNode │         │  - CreateNewBlock()  │
│   - PushMessage │         │  - AddOracleBundle   │
└────────┬────────┘         └──────────┬───────────┘
         │                              │
         │ NetMsgType::ORACLEPRICE     │ Coinbase vout[1]
         │                              │
         ▼                              ▼
┌──────────────────┐         ┌──────────────────────┐
│  P2P Network     │         │   CBlock             │
│  (All peers)     │         │   - vtx[0] coinbase  │
└──────────────────┘         │   - merkle root      │
                             └──────────┬───────────┘
                                        │
                                        ▼
Validation Layer:              CheckBlock(block, state)
┌──────────────────────────────────────────────────────┐
│  OracleDataValidator::ValidateBlockOracleData()      │
│  ├─ ExtractOracleBundle() from coinbase             │
│  ├─ Validate structure, consensus, timestamp         │
│  ├─ Check oracle authorization (chainparams)         │
│  └─ Accept/Reject block                              │
└──────────────────┬───────────────────────────────────┘
                   │ Valid block
                   ▼
         ConnectBlock(block, pindex)
┌──────────────────────────────────────────────────────┐
│  UpdatePriceCache(height, price)                     │
│  ├─ height_to_price[700] = 50200                     │
│  ├─ MockOracleManager (⚠️ LEAKS to non-regtest!)     │
│  └─ Log: "Oracle: Updated price cache at height 700" │
└──────────────────┬───────────────────────────────────┘
                   │
                   ▼
DigiDollar Integration:
┌──────────────────────────────────────────────────────┐
│  OracleIntegration::GetCurrentOraclePrice()          │
│  └─ Returns cached price for collateral calculation  │
│                                                       │
│  DigiDollarManager::MintDigiDollar()                 │
│  ├─ Get DGB/USD from oracle                          │
│  ├─ Calculate required collateral (200%)             │
│  └─ Create mint transaction                          │
└──────────────────────────────────────────────────────┘
```

---

# Part II: Core Components (Deep Dive)

## 4. Data Structures & Serialization

### 4.1 COraclePriceMessage - Complete Specification

**Location**: `/home/jared/Code/digibyte/src/primitives/oracle.h` (lines 31-107)

#### 4.1.1 Field-by-Field Breakdown

```cpp
class COraclePriceMessage
{
public:
    uint32_t oracle_id{0};                      // 4 bytes  - Oracle identifier
    uint64_t price_micro_usd{0};                // 8 bytes  - Price in micro-USD
    int64_t timestamp{0};                       // 8 bytes  - Unix timestamp
    int32_t block_height{0};                    // 4 bytes  - Block height at creation
    uint64_t nonce{0};                          // 8 bytes  - Random nonce
    XOnlyPubKey oracle_pubkey;                  // 32 bytes - BIP-340 Schnorr pubkey
    std::vector<unsigned char> schnorr_sig;     // 64 bytes - BIP-340 Schnorr signature

    // Total: 128 bytes (full format)
};
```

**Field Details:**

| Field | Size | Type | Range/Constraint | Purpose |
|-------|------|------|------------------|---------|
| **oracle_id** | 4 bytes | uint32_t | 0-29 (Phase One: always 0) | Identifies oracle node |
| **price_micro_usd** | 8 bytes | uint64_t | 100 - 100,000,000 ($0.0001-$100.00) | DGB price in micro-USD (1,000,000 = $1.00) |
| **timestamp** | 8 bytes | int64_t | Unix timestamp, ≤1 hour old | Message creation time |
| **block_height** | 4 bytes | int32_t | Current chain height | Context for message |
| **nonce** | 8 bytes | uint64_t | Random value | Ensures hash uniqueness |
| **oracle_pubkey** | 32 bytes | XOnlyPubKey | Valid secp256k1 x-coordinate | BIP-340 pubkey |
| **schnorr_sig** | 64 bytes | vector<uchar> | Valid BIP-340 signature | Message authentication |

#### 4.1.2 Micro-USD Price Format (Detailed)

**CRITICAL: Actual Price Format in Code**
- **Field name**: `price_micro_usd`
- **Actual format**: **Micro-USD** where `1,000,000 micro-USD = $1.00 USD`
- **NOT cents**: The code does NOT use 100 = $1.00 format for oracle prices

**Definition**: `1,000,000 micro-USD = $1.00 USD`

**Why Micro-USD?**
1. **Precision**: 6 decimal places (sufficient for extremely small DGB prices)
2. **Integer Arithmetic**: No floating-point rounding errors
3. **Future-Proof**: Handles prices from $0.000001 to $100+ per DGB
4. **Standard**: Aligns with common financial data precision

**Conversion Examples:**
```
Price (USD/DGB)  →  Micro-USD        →  Hex (LE)
$0.0001          →  100              →  0x6400000000000000
$0.001           →  1,000            →  0xE803000000000000
$0.01            →  10,000           →  0x1027000000000000
$0.0065          →  6,500            →  0x6419000000000000  (realistic DGB price)
$0.05            →  50,000           →  0x50C3000000000000
$1.00            →  1,000,000        →  0x40420F0000000000
$10.00           →  10,000,000       →  0x8096980000000000
$100.00          →  100,000,000      →  0x00E1F50500000000
```

**Validation Constraints** (`IsValid()` implementation at oracle.cpp:35-36):
```cpp
static constexpr uint64_t MIN_PRICE_MICRO_USD = 100;        // $0.0001 per DGB (minimum)
static constexpr uint64_t MAX_PRICE_MICRO_USD = 100000000;  // $100.00 per DGB (maximum)

if (price_micro_usd < MIN_PRICE_MICRO_USD) return false;
if (price_micro_usd > MAX_PRICE_MICRO_USD) return false;
```

**Rationale**:
- **Lower bound ($0.0001)**: Prevents oracle spam with near-zero prices
- **Upper bound ($100.00)**: Reasonable max for DGB; prevents data corruption bugs

**Mock Oracle Default Price**:
- Default: `6500 micro-USD = $0.0065/DGB` (realistic DGB price)

#### 4.1.3 XOnlyPubKey (BIP-340 Schnorr)

**Implementation**: `/home/jared/Code/digibyte/src/pubkey.h` (lines 230-300)

```cpp
class XOnlyPubKey
{
private:
    uint256 m_keydata;  // 32 bytes - x-coordinate only

public:
    // Construct from CPubKey (extracts x-coordinate)
    explicit XOnlyPubKey(const CPubKey& pubkey);

    // Construct from 32-byte span
    explicit XOnlyPubKey(Span<const unsigned char> bytes);

    // BIP-340 Schnorr signature verification
    bool VerifySchnorr(const uint256& msg, Span<const unsigned char> sigbytes) const;

    // Serialization (no length prefix, fixed 32 bytes)
    SERIALIZE_METHODS(XOnlyPubKey, obj) { READWRITE(obj.m_keydata); }
};
```

**Key Properties**:
- **Size**: 32 bytes (vs 33 for compressed CPubKey)
- **Format**: x-coordinate only (y-coordinate parity implicit)
- **Validity**: Only ~50% of 32-byte arrays are valid secp256k1 points
- **BIP-340**: Deterministic, non-malleable Schnorr signatures

**Example**:
```
CPubKey (33 bytes):     03 79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
                        ↑  └──────────────────────────────────────────┬────────────────┘
                     prefix                                       x-coordinate
                                                                       ↓
XOnlyPubKey (32 bytes):    79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
```

#### 4.1.4 Schnorr Signature (BIP-340)

**Format**: 64 bytes (no DER encoding, fixed length)
- **Bytes 0-31**: r component (x-coordinate of R point)
- **Bytes 32-63**: s component (scalar)

**Signature Creation** (`COraclePriceMessage::Sign()`):

```cpp
bool COraclePriceMessage::Sign(const CKey& key,
                                const uint256* merkle_root,
                                const uint256& aux)
{
    // 1. Get message hash (excludes signature and pubkey)
    uint256 hash = GetSignatureHash();

    // 2. Create 64-byte Schnorr signature
    schnorr_sig.resize(64);
    if (!key.SignSchnorr(hash, schnorr_sig, merkle_root, aux)) {
        schnorr_sig.clear();
        return false;
    }

    // 3. Set oracle pubkey from private key
    oracle_pubkey = XOnlyPubKey(key.GetPubKey());

    return true;
}
```

**Signature Hash Computation**:
```cpp
uint256 COraclePriceMessage::GetSignatureHash() const
{
    // Hash all fields EXCEPT signature and pubkey
    CHashWriter ss(0);
    ss << oracle_id;           // 4 bytes
    ss << price_micro_usd;     // 8 bytes
    ss << timestamp;           // 8 bytes
    ss << block_height;        // 4 bytes
    ss << nonce;               // 8 bytes

    return ss.GetHash();  // SHA256d(data)
}
```

**Why exclude signature/pubkey from hash?**
- Prevents circular dependency (can't sign a hash that includes the signature)
- Nonce ensures uniqueness even with identical price/timestamp

#### 4.1.5 Serialization (Full Format)

**SERIALIZE_METHODS Implementation**:
```cpp
SERIALIZE_METHODS(COraclePriceMessage, obj) {
    READWRITE(obj.oracle_id);        // CompactSize + 4 bytes
    READWRITE(obj.price_micro_usd);  // CompactSize + 8 bytes
    READWRITE(obj.timestamp);        // CompactSize + 8 bytes
    READWRITE(obj.block_height);     // CompactSize + 4 bytes
    READWRITE(obj.nonce);            // CompactSize + 8 bytes
    READWRITE(obj.oracle_pubkey);    // 32 bytes (no prefix)
    READWRITE(obj.schnorr_sig);      // CompactSize + 64 bytes
}
```

**On-Wire Size**: ~133 bytes (128 data + ~5 CompactSize prefixes)

**P2P Message Structure**:
```
┌──────────────────────────────────────────────────────┐
│ Bitcoin P2P Header (24 bytes)                        │
├──────────────────────────────────────────────────────┤
│ Magic:        0xDAB5BFFA (DigiByte mainnet)         │
│ Command:      "oracleprice\0\0\0" (12 bytes)        │
│ Payload Size: 133 (4 bytes)                         │
│ Checksum:     <4 bytes>                             │
├──────────────────────────────────────────────────────┤
│ COraclePriceMessage Payload (133 bytes)             │
├──────────────────────────────────────────────────────┤
│ TOTAL: 157 bytes                                     │
└──────────────────────────────────────────────────────┘
```

#### 4.1.6 Validation Rules (`IsValid()`)

**Complete Validation Logic** (src/primitives/oracle.cpp:30-61):

```cpp
bool COraclePriceMessage::IsValid(int64_t reference_time) const
{
    // 1. PRICE RANGE VALIDATION
    // Uses shared constants from oracle.h: ORACLE_MIN/MAX_PRICE_MICRO_USD
    if (price_micro_usd < ORACLE_MIN_PRICE_MICRO_USD) return false;  // 100 ($0.0001)
    if (price_micro_usd > ORACLE_MAX_PRICE_MICRO_USD) return false;  // 100000000 ($100.00)

    // 2. TIMESTAMP VALIDATION
    // Use provided reference time (block time during validation) or current time
    int64_t current_time = (reference_time > 0) ? reference_time : GetTime();

    // Not in future (1 minute tolerance for clock skew)
    if (timestamp > current_time + 60) return false;

    // Not too old (1 hour maximum age)
    if (timestamp < current_time - ORACLE_MAX_AGE_SECONDS) return false;

    // 3. SCHNORR SIGNATURE VERIFICATION (optional for compact format)
    // ⚠️ SECURITY ISSUE: Empty signature BYPASSES all verification
    if (!schnorr_sig.empty()) {
        // Try Phase 2 verification first (signs only oracle_id + price + timestamp)
        if (VerifyPhase2()) {
            return true;
        }
        // Fall back to Phase 1 full verification (includes block_height + nonce)
        return Verify();
    }

    // ⚠️ WARNING: If schnorr_sig is empty, message passes validation!
    // Compact format: Trust based on chainparams oracle pubkey (verified at extraction)
    return true;
}
```

**Validation Summary**:

| Check | Constraint | Rejection Behavior |
|-------|-----------|-------------------|
| Price minimum | ≥ 100 micro-USD ($0.0001) | `return false` |
| Price maximum | ≤ 100,000,000 micro-USD ($100.00) | `return false` (uses ORACLE_MAX_PRICE_MICRO_USD) |
| Future timestamp | ≤ now + 60s | `return false` |
| Old timestamp | ≥ now - 3600s | `return false` |
| Signature | Valid BIP-340 (if present) | ⚠️ **Bypassed if empty** |

> **⚠️ Security Note**: If `schnorr_sig` is empty, signature verification is completely skipped.
> This means an attacker could submit unsigned messages that pass `IsValid()` checks.

### 4.2 COracleBundle - Complete Specification

**Location**: `/home/jared/Code/digibyte/src/primitives/oracle.h` (lines 113-168)

#### 4.2.1 Bundle Structure

```cpp
class COracleBundle
{
public:
    std::vector<COraclePriceMessage> messages;  // 1-17 messages
    int32_t epoch{0};                           // Epoch identifier
    uint64_t median_price_micro_usd{0};         // Consensus price
    int64_t timestamp{0};                       // Bundle creation time

    // Phase One: messages.size() == 1 (1-of-1 consensus)
    // Phase Two / Phase 3 MuSig2: messages.size() >= nOracleRequiredMessages (9-of-17 in RC30)
};
```

#### 4.2.2 Median Price Calculation

**Algorithm** (src/primitives/oracle.cpp):

```cpp
uint64_t COracleBundle::GetConsensusPrice(int min_required) const
{
    if (!HasConsensus(min_required)) return 0;

    // Step 1: Price-range filter only (deterministic, time-independent)
    std::vector<int64_t> prices;
    for (const auto& msg : messages) {
        if (msg.price_micro_usd >= ORACLE_MIN_PRICE_MICRO_USD &&
            msg.price_micro_usd <= ORACLE_MAX_PRICE_MICRO_USD) {
            prices.push_back(static_cast<int64_t>(msg.price_micro_usd));
        }
    }
    if (prices.empty()) return 0;

    // Step 2: Sort for IQR calculation
    std::sort(prices.begin(), prices.end());

    // Step 3: If less than 4 prices, return simple median (no IQR filtering)
    if (prices.size() < 4) {
        size_t mid = prices.size() / 2;
        if (prices.size() % 2 == 0) {
            return static_cast<uint64_t>((prices[mid - 1] + prices[mid]) / 2);
        }
        return static_cast<uint64_t>(prices[mid]);
    }

    // Step 4: Apply IQR outlier filtering (1.5 * IQR rule)
    size_t q1_idx = prices.size() / 4;
    size_t q3_idx = (prices.size() * 3) / 4;
    int64_t q1 = prices[q1_idx];
    int64_t q3 = prices[q3_idx];
    int64_t iqr = q3 - q1;
    int64_t lower_bound = q1 - (iqr * 3 / 2);
    int64_t upper_bound = q3 + (iqr * 3 / 2);

    // Step 5: Filter outliers and return median of filtered set
    std::vector<int64_t> filtered;
    for (int64_t price : prices) {
        if (price >= lower_bound && price <= upper_bound) {
            filtered.push_back(price);
        }
    }

    // Step 6: Fall back to unfiltered median if all filtered
    if (filtered.empty()) filtered = prices;

    std::sort(filtered.begin(), filtered.end());
    size_t mid = filtered.size() / 2;
    if (filtered.size() % 2 == 0) {
        return static_cast<uint64_t>((filtered[mid - 1] + filtered[mid]) / 2);
    }
    return static_cast<uint64_t>(filtered[mid]);
}
```

**Examples**:

```
Odd Count (7 prices):
Input:  [990000, 1000000, 1010000, 1020000, 1030000]
Sorted: [990000, 1000000, 1010000, 1020000, 1030000]
                              ↑
                        Middle (index 2)
Median: 1,010,000 micro-USD = $1.01

Even Count (8 prices):
Input:  [995000, 1000000, 1005000, 1010000]
Sorted: [995000, 1000000, 1005000, 1010000]
                      ↑        ↑
                Middle two (indices 1,2)
Average: (1000000 + 1005000) / 2 = 1,002,500 micro-USD = $1.0025

Phase One (single price):
Input:  [1234567]
Median: 1,234,567 micro-USD = $1.234567
```

#### 4.2.3 Epoch System

**Epoch Calculation**:
```cpp
int32_t GetCurrentEpoch(int32_t block_height) {
    const Consensus::Params& params = Params().GetConsensus();
    int32_t epoch_length = params.nDDOracleEpochBlocks;

    return block_height / epoch_length;
}
```

**Network-Specific Lengths**:
```
Mainnet:  100 blocks (~25 minutes at 15s/block)
Testnet:   50 blocks (~12.5 minutes)
Regtest:   10 blocks (~2.5 minutes)
```

**Epoch Timeline Example (Testnet)**:
```
Epoch  0: Blocks    0 -   49
Epoch  1: Blocks   50 -   99
Epoch  2: Blocks  100 -  149
...
Epoch 20: Blocks 1000 - 1049 (Oracle activation height)
```

### 4.3 Compact Format Encoding (Blockchain Storage)

**Location**: `/home/jared/Code/digibyte/src/oracle/bundle_manager.cpp` (lines 896-1048)

#### 4.3.0 OP_ORACLE: Complete System Flow

```
┌────────────────────────────────────────────────────────────────────┐
│  OP_ORACLE ARCHITECTURE: Technical Implementation Flow            │
└────────────────────────────────────────────────────────────────────┘

PHASE 1: MESSAGE CREATION (External Oracle Daemon)
════════════════════════════════════════════════════════════════════
┌───────────────────────────────────────────────────────────────┐
│ Oracle Daemon (oracle.digibyte.io)                           │
├───────────────────────────────────────────────────────────────┤
│ 1. Fetch prices from 6 working exchanges (5 broken/removed)       │
│ 2. Calculate median with percentage-threshold outlier filtering│
│ 3. Create COraclePriceMessage structure:                     │
│    ┌─────────────────────────────────────────────────────┐  │
│    │ struct COraclePriceMessage {                        │  │
│    │   uint32_t oracle_id;        // 0 (Phase One)      │  │
│    │   uint64_t price_micro_usd;  // Micro-USD (1M = $1) │  │
│    │   int64_t  timestamp;        // Unix time          │  │
│    │   uint32_t block_height;     // Current height     │  │
│    │   uint64_t nonce;            // Random nonce       │  │
│    │   CPubKey  oracle_pubkey;    // 32-byte pubkey     │  │
│    │   std::vector<uint8_t> schnorr_sig; // 64 bytes   │  │
│    │ };                                                  │  │
│    │ Total: 128 bytes                                    │  │
│    └─────────────────────────────────────────────────────┘  │
│ 4. Sign message with BIP-340 Schnorr signature               │
│ 5. Broadcast via P2P: NetMsgType::ORACLEPRICE                │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
                     P2P Network Relay
                              │
                              ▼
PHASE 2: P2P VALIDATION (All Nodes - net_processing.cpp)
════════════════════════════════════════════════════════════════════
┌───────────────────────────────────────────────────────────────┐
│ ProcessMessage(NetMsgType::ORACLEPRICE)                      │
├───────────────────────────────────────────────────────────────┤
│ Validation Steps:                                             │
│ ✓ Deserialize 128-byte COraclePriceMessage                   │
│ ✓ msg.IsValid() → Structural validation                      │
│ ✓ msg.Verify() → BIP-340 Schnorr signature verification      │
│ ✓ oracle_id < 30? (ORACLE_TOTAL_COUNT range check)           │
│ ✓ Timestamp fresh? (age < 1 hour, not > 1 min future)        │
│ ✓ Rate limit: max 3600 messages/hour from this peer           │
│ ✓ Duplicate check: msg.GetHash() not in seen_messages        │
│                                                               │
│ If VALID:                                                     │
│   → OracleBundleManager::AddOracleMessage(msg)               │
│   → RelayOracleMessage(msg, exclude_peer_id)                 │
│                                                               │
│ If INVALID:                                                   │
│   → Misbehavior(peer, 2-20 points depending on violation)    │
│   → Drop message (rate limit: silently dropped, no penalty)  │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
PHASE 3: BLOCK INCLUSION (Miner - bundle_manager.cpp:896-1048)
════════════════════════════════════════════════════════════════════
┌───────────────────────────────────────────────────────────────┐
│ Miner calls CreateOracleScript()                              │
├───────────────────────────────────────────────────────────────┤
│ Input:  COracleBundle with 1 message (from P2P)              │
│ Output: CScript with compact 22-byte OP_ORACLE data          │
│                                                               │
│ Encoding Process:                                             │
│ 1. script << OP_RETURN << OP_ORACLE;  // 2 bytes             │
│ 2. script << std::vector<uchar>{0x01}; // Version byte       │
│ 3. Create compact data (17 bytes):                           │
│    ┌────────────────────────────────────────────┐            │
│    │ oracle_id (1) + price (8) + timestamp (8) │            │
│    │          = 17 bytes total                  │            │
│    └────────────────────────────────────────────┘            │
│ 4. script << compact_data; // Push 17 bytes                  │
│                                                               │
│ Coinbase Transaction Structure:                              │
│   vout[0]: 72,000 DGB → Miner reward                         │
│   vout[1]: 0 DGB → OP_RETURN OP_ORACLE <22-byte data>  ◄──┐  │
│   vout[2]: 0 DGB → Witness commitment                    │  │
│                                                          │  │
│ Final scriptPubKey (22 bytes):                          │  │
│   6a bf 01 01 11 00 [price 8B] [timestamp 8B]          │  │
│   │  │  │  │  │  │                                      │  │
│   │  │  │  │  │  └─ Oracle ID                          │  │
│   │  │  │  │  └──── PUSH 17 bytes                      │  │
│   │  │  │  └─────── Version                            │  │
│   │  │  └────────── PUSH 1 byte                        │  │
│   │  └───────────── OP_ORACLE (0xbf) ◄─────────────────┘  │
│   └──────────────── OP_RETURN (0x6a)                       │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
PHASE 4: BLOCK VALIDATION (All Nodes - validation.cpp:4373)
════════════════════════════════════════════════════════════════════
┌───────────────────────────────────────────────────────────────┐
│ CheckBlock() calls ValidateBlockOracleData()                  │
├───────────────────────────────────────────────────────────────┤
│ 1. Network Check (⚠️ MAINNET COMPLETELY BYPASSED):            │
│    if (network != TESTNET && network != REGTEST)              │
│        return true; // 🚨 ALL ORACLE DATA ACCEPTED ON MAINNET │
│                                                               │
│ 2. Activation Height:                                         │
│    if (block_height < nDDActivationHeight)                     │
│        skip oracle validation; // Not active yet              │
│                                                               │
│ 3. Find OP_ORACLE in coinbase:                                │
│    ┌───────────────────────────────────────────────┐         │
│    │ for vout in coinbase.vout:                    │         │
│    │   if vout.scriptPubKey[0] == 0x6a:  // OP_RETURN        │
│    │     if vout.scriptPubKey[1] == 0xbf:  // OP_ORACLE      │
│    │       found = true;                            │         │
│    └───────────────────────────────────────────────┘         │
│                                                               │
│ 4. Extract Compact Data (bundle_manager.cpp:1050-1200):      │
│    ┌───────────────────────────────────────────────┐         │
│    │ Parse 22-byte scriptPubKey:                   │         │
│    │ - Byte 3: version (must be 0x01)             │         │
│    │ - Byte 5: oracle_id (must be 0x00)           │         │
│    │ - Bytes 6-13: price (LE uint64)              │         │
│    │ - Bytes 14-21: timestamp (LE int64)          │         │
│    └───────────────────────────────────────────────┘         │
│                                                               │
│ 5. Validate Bundle:                                           │
│    ✓ version == 0x01                                         │
│    ✓ oracle_id == 0                                          │
│    ✓ price in range [100, 100M] micro-USD                    │
│    ✓ timestamp < block.nTime + 60                            │
│    ✓ timestamp > block.nTime - 3600                          │
│    ✓ messages.size() == 1 (Phase One)                        │
│    ✓ GetOracleNode(oracle_id)->is_active == true             │
│                                                               │
│ 6. Result:                                                    │
│    if (all_checks_pass)                                       │
│        ACCEPT block;                                          │
│    else                                                       │
│        REJECT block; // Invalid oracle data                  │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
PHASE 5: PRICE CACHE (ConnectBlock - validation.cpp:~2805)
════════════════════════════════════════════════════════════════════
┌───────────────────────────────────────────────────────────────┐
│ OracleBundleManager::UpdatePriceCache()                       │
├───────────────────────────────────────────────────────────────┤
│ Extract oracle price from connected block:                    │
│   COracleBundle bundle;                                       │
│   ExtractOracleBundle(coinbase_tx, bundle);                   │
│                                                               │
│ Update in-memory cache:                                       │
│   ┌─────────────────────────────────────────┐                │
│   │ std::map<int, uint64_t> height_to_price│                │
│   ├─────────────────────────────────────────┤                │
│   │ [695] → 6400 micro-USD ($0.0064)       │                │
│   │ [696] → 6450 micro-USD ($0.00645)      │                │
│   │ [697] → 6500 micro-USD ($0.0065)       │                │
│   │ [698] → 6550 micro-USD ($0.00655)      │                │
│   │ [699] → 6500 micro-USD ($0.0065)       │                │
│   │ [700] → 6500 micro-USD ($0.0065) ◄ NEW │                │
│   └─────────────────────────────────────────┘                │
│                                                               │
│ Cache management:                                             │
│   - Keep last 1,000 blocks                                    │
│   - Thread-safe (std::mutex)                                  │
│   - Auto-evict oldest entries                                 │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
PHASE 6: DIGIDOLLAR USAGE (Minting/Redemption)
════════════════════════════════════════════════════════════════════
┌───────────────────────────────────────────────────────────────┐
│ DigiDollar Minting Process                                    │
├───────────────────────────────────────────────────────────────┤
│ User calls: mintdigidollar(amount, lock_tier)                 │
│                                                               │
│ Query oracle price:                                           │
│   uint64_t price = OracleBundleManager::GetInstance()         │
│                      .GetLatestPrice();                       │
│   // Returns: 6500 micro-USD = $0.0065 per DGB                │
│                                                               │
│ Calculate collateral:                                         │
│   ┌─────────────────────────────────────────────────┐        │
│   │ Mint $100 DigiDollars (100,000,000 micro-USD)  │        │
│   │ Collateral ratio: 200% (Phase One)             │        │
│   │ Oracle price: 6500 micro-USD ($0.0065/DGB)     │        │
│   │                                                 │        │
│   │ Required DGB:                                   │        │
│   │   ($100 × 2) ÷ $0.0065/DGB = 30,769 DGB        │        │
│   │                                                 │        │
│   │ Formula:                                        │        │
│   │   collateral_sats = (dd_amount × COIN × 2) /   │        │
│   │                     oracle_price                │        │
│   │                   = (10000 × 100000000 × 2) /   │        │
│   │                     50000                       │        │
│   │                   = 40000000 sats (0.4 DGB)     │        │
│   └─────────────────────────────────────────────────┘        │
└───────────────────────────────────────────────────────────────┘
```

#### 4.3.1 Byte-by-Byte Format Specification

```
┌────────────────────────────────────────────────────────────────────┐
│  OP_ORACLE (0xbf): Custom Opcode for Oracle Data                  │
└────────────────────────────────────────────────────────────────────┘

OPCODE DEFINITION
═══════════════════════════════════════════════════════════════════
File: src/script/script.h:214

OP_ORACLE = 0xbf  // Repurposed OP_NOP15 for oracle price data

Context in DigiDollar Opcode Family:
┌─────────────────────────────────────────────────────────────┐
│ OP_DIGIDOLLAR      = 0xbb  (OP_NOP11) - DD output marker   │
│ OP_DDVERIFY        = 0xbc  (OP_NOP12) - DD verification    │
│ OP_CHECKPRICE      = 0xbd  (OP_NOP13) - Price checking     │
│ OP_CHECKCOLLATERAL = 0xbe  (OP_NOP14) - Collateral check   │
│ OP_ORACLE          = 0xbf  (OP_NOP15) - Oracle data ◄───┐  │
└─────────────────────────────────────────────────────────────┘


WHY A CUSTOM OPCODE?
═══════════════════════════════════════════════════════════════════
✓ Fast Detection: Nodes instantly recognize oracle data
✓ Efficient Parsing: Skip non-oracle OP_RETURNs without parsing
✓ Type Safety: Compiler enforces correct opcode usage
✓ Extensibility: Future versions (0x02, 0x03) possible
✓ Self-Documenting: Code intent is crystal clear


DETECTION ALGORITHM
═══════════════════════════════════════════════════════════════════
Pseudocode (validation.cpp):

for (const auto& tx : block.vtx) {
    if (!tx.IsCoinBase()) continue;

    for (const auto& out : tx.vout) {
        const CScript& script = out.scriptPubKey;

        // Check for OP_RETURN
        if (script.size() < 2) continue;
        if (script[0] != OP_RETURN) continue;  // 0x6a

        // Check for OP_ORACLE ◄─── KEY CHECK
        if (script[1] == OP_ORACLE) {  // 0xbf
            // Found oracle data! Parse it.
            ExtractOracleBundle(tx, bundle);
            break;
        }
    }
}


COMPACT FORMAT STRUCTURE (22 bytes total)
═══════════════════════════════════════════════════════════════════

Phase One Compact Format:

┌─────┬─────┬─────┬──────────────┬──────────────┬──────────────┐
│ Pos │ Len │ Type│ Name         │ Value        │ Description  │
├─────┼─────┼─────┼──────────────┼──────────────┼──────────────┤
│ 0   │ 1   │ OP  │ OP_RETURN    │ 0x6a         │ Unspendable  │
│ 1   │ 1   │ OP  │ OP_ORACLE    │ 0xbf         │ Oracle marker│
│ 2   │ 1   │ OP  │ PUSHDATA     │ 0x01         │ Push 1 byte  │
│ 3   │ 1   │ u8  │ Version      │ 0x01         │ Phase One    │
│ 4   │ 1   │ OP  │ PUSHDATA     │ 0x11 (17)    │ Push 17 bytes│
│ 5   │ 1   │ u8  │ Oracle ID    │ 0x00         │ Oracle 0     │
│ 6-13│ 8   │ u64 │ Price        │ LE uint64    │ Micro-USD    │
│14-21│ 8   │ i64 │ Timestamp    │ LE int64     │ Unix time    │
└─────┴─────┴─────┴──────────────┴──────────────┴──────────────┘

Total: 22 bytes (within 83-byte MAX_OP_RETURN_RELAY limit) ✅


EXAMPLE: Real OP_ORACLE Output
═══════════════════════════════════════════════════════════════════
Hex Dump (22 bytes):
6a bf 01 01 11 00 64 19 00 00 00 00 00 00 00 2f 50 65 00 00 00 00

Parsed:
┌──────┬──────┬──────────────────────────────────────────────┐
│ Pos  │ Hex  │ Meaning                                      │
├──────┼──────┼──────────────────────────────────────────────┤
│ 0    │ 6a   │ OP_RETURN: Output is unspendable            │
│ 1    │ bf   │ OP_ORACLE: This is oracle data ✓            │
│ 2    │ 01   │ PUSH 1: Next 1 byte is data                 │
│ 3    │ 01   │ Version 1 (Phase One format)                │
│ 4    │ 11   │ PUSH 17: Next 17 bytes are data             │
│ 5    │ 00   │ Oracle ID = 0                               │
│ 6-13 │ 6419 │ Price = 0x0000000000001964 (LE)             │
│      │ 0000 │       = 6500 micro-USD = $0.0065/DGB        │
│      │ 0000 │                                              │
│      │ 0000 │                                              │
│14-21 │ 002f │ Timestamp = 0x0000000065502f00 (LE)         │
│      │ 5065 │           = 1,700,000,000                   │
│      │ 0000 │           = Nov 14, 2023 22:13:20 UTC       │
│      │ 0000 │                                              │
└──────┴──────┴──────────────────────────────────────────────┘


SPACE EFFICIENCY ANALYSIS
═══════════════════════════════════════════════════════════════════
Full P2P Format:        128 bytes (includes signature)
Compact Block Format:    22 bytes (no signature needed)
Savings per block:      106 bytes (82.8% reduction)

Annual savings (5,760 blocks/day × 365 days):
  Full format:    269 MB/year
  Compact format:  46 MB/year
  Savings:        223 MB/year ✓

10-year savings: 2.23 GB ✓
```

#### 4.3.2 Encoding Implementation

```cpp
CScript OracleBundleManager::CreateOracleScript(const COracleBundle& bundle) const
{
    // Phase Three (v0x03): MuSig2 aggregate signature + participation bitmap
    if (bundle.version == 3) {
        // ... MuSig2 handling (omitted for brevity) ...
    }

    if (bundle.messages.empty()) {
        return CScript(); // Empty script for no oracle data
    }

    // Phase Two: multi-message bundles (version 0x02) when Phase Two is active
    if (bundle.messages.size() > 1) {
        // Only create multi-oracle scripts when Phase Two is enabled
        // If Phase Two not activated, reject multi-message bundles
        // Phase Two format: OP_RETURN OP_ORACLE <0x02> <data>
        // Data: num_messages(1) + price(8) + timestamp(8) + per-oracle: id(1) + sig(64)
        // ... (see bundle_manager.cpp for full implementation)
    }

    // Phase One: Must have exactly 1 message (1-of-1 consensus)
    CScript script;
    script << OP_RETURN << OP_ORACLE;

    // Version byte (0x01 = Phase One compact format)
    script << std::vector<unsigned char>{0x01};

    // Compact data: oracle_id (1) + price (8) + timestamp (8) = 17 bytes
    const COraclePriceMessage& msg = bundle.messages[0];
    std::vector<unsigned char> compact_data;
    compact_data.reserve(17);

    // Oracle ID (uint8 for Phase One)
    // SECURITY (DGB-SEC-004): Defense-in-depth — reject oracle_id > 255
    compact_data.push_back(static_cast<unsigned char>(msg.oracle_id & 0xFF));

    // Price in micro-USD (uint64, little-endian)
    uint64_t price = msg.price_micro_usd;
    for (int i = 0; i < 8; ++i) {
        compact_data.push_back(static_cast<unsigned char>((price >> (i * 8)) & 0xFF));
    }

    // Timestamp (int64, little-endian)
    int64_t timestamp = msg.timestamp;
    for (int i = 0; i < 8; ++i) {
        compact_data.push_back(static_cast<unsigned char>((timestamp >> (i * 8)) & 0xFF));
    }

    script << compact_data;
    return script;
}
```

#### 4.3.3 Concrete Encoding Example

**Input Message**:
```
oracle_id:        0
price_micro_usd:  6500 micro-USD (= $0.0065/DGB)
timestamp:        1700000000 (0x65502F00 in hex)
```

**Output Script (hex)**:
```
6a          - OP_RETURN
bf          - OP_ORACLE
01          - PUSH 1 byte (version follows)
01          - Version (0x01 = Phase One)
11          - PUSH 17 bytes (compact data follows)
00          - Oracle ID (0)
64 19 00 00 00 00 00 00  - Price (6500 micro-USD = $0.0065/DGB in little-endian)
00 2f 50 65 00 00 00 00  - Timestamp (1,700,000,000 in little-endian)

Total: 22 bytes (2 opcodes + 2 push headers + 18 data bytes)
```

**Verification (Little-Endian)**:
```
Price bytes: 64 19 00 00 00 00 00 00
  Value: 6500 micro-USD = $0.0065 per DGB ✓

Timestamp bytes: 00 2f 50 65 00 00 00 00
  Step 1: 0x00                          = 0
  Step 2: 0x2f << 8                     = 12,032
  Step 3: 0x50 << 16                    = 5,242,880
  Step 4: 0x65 << 24                    = 1,694,498,816
  Total:  0 + 12,032 + 5,242,880 + 1,694,498,816 = 1,699,753,728 (example value)
```

#### 4.3.4 Decoding Implementation

```cpp
bool OracleBundleManager::ExtractOracleBundle(
    const CTransaction& coinbase_tx,
    COracleBundle& bundle) const
{
    // Look for OP_RETURN output with OP_ORACLE marker
    for (const auto& output : coinbase_tx.vout) {
        if (output.scriptPubKey.size() > 2 &&
            output.scriptPubKey[0] == OP_RETURN &&
            output.scriptPubKey[1] == OP_ORACLE) {

            // Extract data chunks
            std::vector<unsigned char> data;
            auto script_it = output.scriptPubKey.begin() + 2; // Skip OP_RETURN + OP_ORACLE

            while (script_it < output.scriptPubKey.end()) {
                if (*script_it <= 75) { // OP_PUSHDATA1 range
                    unsigned char chunk_size = *script_it;
                    ++script_it;

                    if (script_it + chunk_size <= output.scriptPubKey.end()) {
                        data.insert(data.end(), script_it, script_it + chunk_size);
                        script_it += chunk_size;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            // Check version byte
            if (data[0] == 0x01) {
                // Phase One compact format
                if (data.size() < 18) {
                    return false;
                }

                COraclePriceMessage msg;

                // Parse oracle_id (uint8)
                msg.oracle_id = data[1];

                // Parse price (uint64, little-endian)
                uint64_t price = 0;
                for (int i = 0; i < 8; ++i) {
                    price |= (static_cast<uint64_t>(data[2 + i]) << (i * 8));
                }
                msg.price_micro_usd = price;

                // Parse timestamp (int64, little-endian)
                int64_t timestamp = 0;
                for (int i = 0; i < 8; ++i) {
                    timestamp |= (static_cast<int64_t>(data[10 + i]) << (i * 8));
                }
                msg.timestamp = timestamp;

                // Set remaining fields (not in compact format)
                msg.block_height = 0; // Not needed for Phase One
                msg.nonce = 0;

                // Phase One: Get oracle pubkey from chainparams
                const CChainParams& chainparams = Params();
                const OracleNodeInfo* oracle_info = chainparams.GetOracleNode(msg.oracle_id);
                if (oracle_info) {
                    msg.oracle_pubkey = XOnlyPubKey(oracle_info->pubkey);
                }

                // Create bundle with single message
                bundle.messages.clear();
                bundle.messages.push_back(msg);
                bundle.median_price_micro_usd = price;
                bundle.timestamp = timestamp;
                bundle.epoch = GetCurrentEpoch(msg.block_height);

                return true;
            }

            return false;
        }
    }

    return false;
}
```

#### 4.3.5 Size Optimization Analysis

**Comparison: Full Format vs Compact Format**

```
FULL FORMAT (P2P Transmission):
┌──────────────────────┬──────────┐
│ Field                │ Size     │
├──────────────────────┼──────────┤
│ oracle_id            │  4 bytes │
│ price_micro_usd      │  8 bytes │  (1,000,000 = $1.00)
│ timestamp            │  8 bytes │
│ block_height         │  4 bytes │
│ nonce                │  8 bytes │
│ oracle_pubkey        │ 32 bytes │
│ schnorr_sig          │ 64 bytes │
├──────────────────────┼──────────┤
│ TOTAL                │ 128 bytes│
└──────────────────────┴──────────┘

COMPACT FORMAT (Blockchain Storage):
┌──────────────────────┬──────────┐
│ Field                │ Size     │
├──────────────────────┼──────────┤
│ OP_RETURN            │  1 byte  │
│ OP_ORACLE            │  1 byte  │
│ OP_PUSHDATA          │  1 byte  │
│ Version              │  1 byte  │
│ oracle_id            │  1 byte  │
│ price_micro_usd      │  8 bytes │  (1,000,000 = $1.00)
│ timestamp            │  8 bytes │
├──────────────────────┼──────────┤
│ TOTAL                │ 22 bytes │
└──────────────────────┴──────────┘

SAVINGS: 106 bytes per message (82.8% reduction)

Phase Two / Phase 3 MuSig2 (17 oracles, RC30):
- Full format:    17 × 128 = 2,176 bytes
- Compact format: ~170 bytes (version/opcodes shared, 17 × price+timestamp)
- MuSig2 v0x03:   ~88 bytes (constant, one aggregate signature)
- Savings: up to ~2,088 bytes (>95% reduction with MuSig2)
```

---

## 5. Block Validation & Consensus Rules

### 5.1 CheckBlock() Integration

**Location**: `/home/jared/Code/digibyte/src/validation.cpp` (CheckBlock calls ValidateBlockOracleData at line 4373)

**Integration Point (line 4373)**:
```cpp
// Validate oracle data (if present and after activation)
if (!OracleDataValidator::ValidateBlockOracleData(block, nullptr, consensusParams, state)) {
    return false; // State already set by ValidateBlockOracleData
}
```

**Execution Position**:

```
CheckBlock() Validation Sequence:
├─► CheckBlockHeader (PoW, timestamp)       [lines 3800-3850]
├─► Signet block solution (if applicable)   [lines 3852-3890]
├─► Merkle root validation                  [lines 3892-3920]
├─► Size limits check                       [lines 3922-3950]
├─► Coinbase transaction validation         [lines 3952-4000]
├─► All transaction validation              [lines 4002-4100]
├─► Signature operation count               [lines 4102-4125]
│
├─► Mark block as checked                   [line 4308]
│     block.fChecked = true;
│
└─► ★ ORACLE VALIDATION ★                   [line 4373]
      OracleDataValidator::ValidateBlockOracleData()
```

### 5.2 ValidateBlockOracleData() - Complete Flow

**Location**: `/home/jared/Code/digibyte/src/oracle/bundle_manager.cpp:2225-2417`

**Simplified Phase One Flow** (actual code is phase-aware with BIP9 activation, Phase2/Phase3 branches):

> **Note**: The actual implementation at bundle_manager.cpp:2225-2417 uses BIP9 deployment checks
> (DigiDollar::IsDigiDollarEnabled) and has separate code paths for Phase 1, Phase 2, and Phase 3 (MuSig2).
> The pseudocode below shows the Phase One path only.

```cpp
bool OracleDataValidator::ValidateBlockOracleData(
    const CBlock& block,
    const CBlockIndex* pindex_prev,
    const Consensus::Params& params,
    BlockValidationState& state)
{
    //═══════════════════════════════════════════════════════════════════
    // STEP 1: NETWORK FILTER - ⚠️ MAINNET DISABLED
    //═══════════════════════════════════════════════════════════════════
    // 🚨 CRITICAL: Mainnet validation COMPLETELY BYPASSED
    // bundle_manager.cpp:2229 returns true immediately for mainnet
    // This means ANY oracle data (valid or invalid) is accepted on mainnet
    if (Params().GetChainType() != ChainType::TESTNET &&
        Params().GetChainType() != ChainType::REGTEST) {
        return true; // ⚠️ MAINNET ORACLE VALIDATION DISABLED - ALL DATA ACCEPTED
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 2: BLOCK HEIGHT DETERMINATION (lines 817-830)
    //═══════════════════════════════════════════════════════════════════
    int32_t block_height = 0;
    if (pindex_prev) {
        // ContextualCheckBlock has access to prev block index
        block_height = pindex_prev->nHeight + 1;
    } else {
        // CheckBlock doesn't have chain context - extract from coinbase (BIP34)
        const CTransaction& coinbase = *block.vtx[0];
        if (!coinbase.vin.empty() && coinbase.vin[0].scriptSig.size() >= 1) {
            CScript::const_iterator pc = coinbase.vin[0].scriptSig.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;
            if (coinbase.vin[0].scriptSig.GetOp(pc, opcode, data) && !data.empty()) {
                block_height = CScriptNum(data, true).getint();
            }
        }
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 3: BIP9 ACTIVATION CHECK (lines 1590-1599)
    //═══════════════════════════════════════════════════════════════════
    // Primary: BIP9 deployment check (when pindex_prev available)
    // Fallback: Height-based check (when no chain context)
    if (pindex_prev) {
        if (!DigiDollar::IsDigiDollarEnabled(pindex_prev, params)) {
            return true; // Oracle validation not required before BIP9 activation
        }
    } else {
        if (block_height < params.nDDActivationHeight) {
            return true;
        }
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 4: SCAN ALL COINBASE OUTPUTS FOR OP_ORACLE (lines 1601-1624)
    //═══════════════════════════════════════════════════════════════════
    const CTransaction& coinbase = *block.vtx[0];

    // Scan ALL coinbase outputs for OP_ORACLE marker (oracle output position
    // varies: may be vout[1] without witness commitment, or vout[2] with it)
    int oracle_output_count = 0;
    for (const auto& output : coinbase.vout) {
        if (output.scriptPubKey.size() >= 2 &&
            output.scriptPubKey[0] == OP_RETURN &&
            output.scriptPubKey[1] == OP_ORACLE) {
            oracle_output_count++;
        }
    }

    // SECURITY: Reject blocks with multiple oracle outputs (prevents confusion attacks)
    if (oracle_output_count > 1) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
            "bad-oracle-multiple-outputs",
            strprintf("Block contains %d oracle outputs, expected at most 1",
                       oracle_output_count));
    }

    if (oracle_output_count == 0) {
        // Allow blocks without oracle data during transition
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: No oracle bundle in block %d (transition period)\n",
                 block_height);
        return true;
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 5: ORACLE BUNDLE EXTRACTION (lines 862-869)
    //═══════════════════════════════════════════════════════════════════
    COracleBundle bundle;
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    if (!manager.ExtractOracleBundle(coinbase, bundle)) {
        // During transition, allow blocks without valid oracle bundles
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Block %d could not extract bundle (transition)\n",
                 block_height);
        return true;
    }

    // CRITICAL: Once extraction succeeds, full validation is MANDATORY

    //═══════════════════════════════════════════════════════════════════
    // STEP 6: BUNDLE STRUCTURE VALIDATION (lines 874-878)
    //═══════════════════════════════════════════════════════════════════
    if (!bundle.IsValid()) {
        LogPrintf("Oracle: Invalid oracle bundle in block %d\n", block_height);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                            "bad-oracle-bundle",
                            "invalid oracle bundle structure");
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 7: PHASE ONE CONSENSUS RULE (1-of-1) (lines 880-885)
    //═══════════════════════════════════════════════════════════════════
    if (bundle.messages.size() != 1) {
        LogPrintf("Oracle: Phase One requires exactly 1 oracle message, got %d\n",
                 bundle.messages.size());
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                            "bad-oracle-consensus",
                            strprintf("Phase One requires exactly 1 oracle message, got %d",
                                     bundle.messages.size()));
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 8: MEDIAN PRICE VERIFICATION (lines 887-894)
    //═══════════════════════════════════════════════════════════════════
    const COraclePriceMessage& msg = bundle.messages[0];
    if (bundle.median_price_micro_usd != msg.price_micro_usd) {
        LogPrintf("Oracle: Median price mismatch: bundle=%llu, message=%llu\n",
                 bundle.median_price_micro_usd, msg.price_micro_usd);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                            "bad-oracle-median",
                            strprintf("Median price mismatch: bundle=%llu, message=%llu",
                                     bundle.median_price_micro_usd, msg.price_micro_usd));
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 9: SCHNORR SIGNATURE VERIFICATION (lines 896-908)
    // ⚠️ SECURITY ISSUE: Empty signature BYPASSES verification
    //═══════════════════════════════════════════════════════════════════
    // Phase One compact format: Trust based on chainparams (no embedded sig)
    if (!msg.schnorr_sig.empty()) {
        // Full format with embedded signature - verify it
        if (!msg.Verify()) {
            LogPrintf("Oracle: Invalid Schnorr signature (oracle_id=%d, block=%d)\n",
                     msg.oracle_id, block_height);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                                "bad-oracle-signature",
                                "Invalid oracle Schnorr signature");
        }
    }
    // ⚠️ WARNING: If schnorr_sig is empty, NO verification occurs!
    // This allows any message without a signature to pass validation.

    //═══════════════════════════════════════════════════════════════════
    // STEP 10: TIMESTAMP AGE VALIDATION (lines 909-916)
    //═══════════════════════════════════════════════════════════════════
    // Verify oracle timestamp is not too old (max 1 hour = 3600 seconds)
    int64_t oracle_age = block.nTime - msg.timestamp;
    if (oracle_age > ORACLE_MAX_AGE_SECONDS) {
        LogPrintf("Oracle: Timestamp too old: age=%d seconds (max=%d) block %d\n",
                 oracle_age, ORACLE_MAX_AGE_SECONDS, block_height);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                            "bad-oracle-timestamp",
                            strprintf("Oracle timestamp too old: age=%d seconds (max=%d)",
                                     oracle_age, ORACLE_MAX_AGE_SECONDS));
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 11: FUTURE TIMESTAMP REJECTION (lines 918-924)
    //═══════════════════════════════════════════════════════════════════
    // Verify oracle timestamp is not in the future (60 second tolerance)
    if (msg.timestamp > block.nTime + 60) {
        LogPrintf("Oracle: Timestamp in future: oracle=%d, block=%d\n",
                 msg.timestamp, block.nTime);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                            "bad-oracle-timestamp",
                            "Oracle timestamp is in the future");
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 12: ORACLE AUTHORIZATION VERIFICATION (lines 926-935)
    //═══════════════════════════════════════════════════════════════════
    // Verify oracle is authorized (skip in REGTEST for unit testing)
    if (Params().GetChainType() != ChainType::REGTEST) {
        const CChainParams& chainparams = Params();
        const OracleNodeInfo* oracle_config = chainparams.GetOracleNode(msg.oracle_id);
        if (!oracle_config || !oracle_config->is_active) {
            LogPrintf("Oracle: Unauthorized oracle ID %d in block %d\n",
                     msg.oracle_id, block_height);
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                                "bad-oracle-unauthorized",
                                strprintf("Unauthorized oracle ID %d", msg.oracle_id));
        }
    }

    //═══════════════════════════════════════════════════════════════════
    // STEP 13: VALIDATION SUCCESS (lines 937-939)
    //═══════════════════════════════════════════════════════════════════
    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Block %d oracle bundle validated: price=%llu micro-USD\n",
             block_height, bundle.median_price_micro_usd);

    return true;
}
```

### 5.3 Consensus Rules Summary Table

| Rule | Specification | Enforcement Point | Rejection Reason | Penalty |
|------|---------------|-------------------|------------------|---------|
| **Network Restriction** | Testnet/Regtest only | Line 805 | N/A | ⚠️ **MAINNET BYPASSED** |
| **Activation Height** | height ≥ nDDActivationHeight | Line 833 | N/A | None (returns true) |
| **Message Count** | Exactly 1 message (Phase One) | Line 881 | `bad-oracle-consensus` | Block rejected |
| **Median Price** | median = message.price | Line 889 | `bad-oracle-median` | Block rejected |
| **Timestamp Age** | ≤ 3600 seconds old | Line 911 | `bad-oracle-timestamp` | Block rejected |
| **Future Timestamp** | ≤ block_time + 60s | Line 919 | `bad-oracle-timestamp` | Block rejected |
| **Oracle Authorization** | oracle_id in chainparams & active | Line 930 | `bad-oracle-unauthorized` | Block rejected |
| **Schnorr Signature** | Valid BIP-340 (if present) | Line 900 | `bad-oracle-signature` | ⚠️ Empty sig bypasses |

> **⚠️ WARNING**: Network Restriction rule means mainnet validation is COMPLETELY DISABLED.
> The Schnorr Signature rule has a bypass: if `schnorr_sig.empty()` returns true, verification is skipped.

### 5.4 ConnectBlock() Integration

**Location**: `/home/jared/Code/digibyte/src/validation.cpp:~2805` (oracle price extraction in ConnectBlock)

```cpp
bool Chainstate::ConnectBlock(const CBlock& block, BlockValidationState& state,
                               CBlockIndex* pindex, CCoinsViewCache& view, bool fJustCheck)
{
    // ... existing block connection logic ...

    // Extract and cache oracle price (ALL networks)
    if (!fJustCheck && !block.vtx.empty()) {
        if (!block.vtx.empty()) {
            // Use OracleBundleManager's ExtractOracleBundle for proper parsing
            OracleBundleManager& manager = OracleBundleManager::GetInstance();
            COracleBundle bundle;

            if (manager.ExtractOracleBundle(*block.vtx[0], bundle)) {
                // Update oracle price cache for this height
                manager.UpdatePriceCache(pindex->nHeight, bundle.median_price_micro_usd);

                // In RegTest mode, also update MockOracleManager
                if (chain_type == ChainType::REGTEST) {
                    MockOracleManager::GetInstance().SetMockPrice(bundle.median_price_micro_usd);
                }

                LogPrint(BCLog::DIGIDOLLAR,
                        "Oracle: Updated price cache at height %d: %llu micro-USD ($%.6f)\n",
                        pindex->nHeight,
                        bundle.median_price_micro_usd,
                        bundle.median_price_micro_usd / 1000000.0);
            }
        }
    }

    return true;
}
```

**Price Cache Implementation**:
```cpp
// src/oracle/bundle_manager.cpp:2163-2175
void OracleBundleManager::UpdatePriceCache(int height, uint64_t price_micro_usd)
{
    std::lock_guard<std::mutex> lock(mtx_price_cache);
    height_to_price[height] = price_micro_usd;

    // Keep cache size limited (last 1000 blocks)
    if (height_to_price.size() > 1000) {
        height_to_price.erase(height_to_price.begin());
    }

    LogPrint(BCLog::DIGIDOLLAR, "Oracle: Price cache updated for height %d: %llu micro-USD\n",
             height, price_micro_usd);
}
```

**Cache Properties**:
- **Thread-safe**: Protected by `mtx_price_cache` mutex
- **Maximum size**: 1000 entries (last 1000 blocks)
- **Eviction**: FIFO (oldest entries removed first)
- **Data structure**: `std::map<int, uint64_t> height_to_price`

### 5.5 DisconnectBlock() Integration

**Location**: `/home/jared/Code/digibyte/src/validation.cpp:~2460-2485`

```cpp
bool Chainstate::DisconnectBlock(const CBlock& block, const CBlockIndex* pindex,
                                  CCoinsViewCache& view)
{
    // ... existing disconnect logic ...

    // T8-03: Revert oracle price cache for ALL networks (not just testnet/regtest)
    // Mainnet needs deterministic oracle pricing too.
    if (!block.vtx.empty() && block.vtx[0]->vout.size() >= 2) {
        const CTxOut& oracle_output = block.vtx[0]->vout[1];
        if (oracle_output.scriptPubKey.IsUnspendable() &&
            oracle_output.scriptPubKey.size() > 2) {
            // This block had oracle data, need to revert the cache
            OracleBundleManager& manager = OracleBundleManager::GetInstance();
            manager.RemovePriceCache(pindex->nHeight);

            // In RegTest mode, also revert MockOracleManager
            auto chain_type = Params().GetChainType();
            if (chain_type == ChainType::REGTEST && pindex->pprev) {
                // Reset to previous height's price or default
                uint64_t prevPrice = manager.GetOraclePriceForHeight(pindex->pprev->nHeight);
                if (prevPrice > 0) {
                    MockOracleManager::GetInstance().SetMockPrice(prevPrice);
                } else {
                    // Reset to default if no previous price
                    MockOracleManager::GetInstance().Reset();
                }
            }

                LogPrint(BCLog::DIGIDOLLAR,
                        "Oracle: Reverted price cache at height %d during block disconnect\n",
                        pindex->nHeight);
            }
        }
    }

    return true;
}
```

**Cache Removal Implementation**:
```cpp
// src/oracle/bundle_manager.cpp:2201-2210
void OracleBundleManager::RemovePriceCache(int height)
{
    std::lock_guard<std::mutex> lock(mtx_price_cache);
    auto it = height_to_price.find(height);
    if (it != height_to_price.end()) {
        height_to_price.erase(it);
        LogPrint(BCLog::DIGIDOLLAR, "Oracle: Removed price cache for height %d\n", height);
    }
}
```

---

## 14. Phase Two Roadmap - Multi-Oracle Consensus

### 14.1 Phase Two Overview

Phase Two implements decentralized multi-oracle consensus for mainnet security.

**Configuration Parameters** (`src/consensus/params.h`):
```cpp
int nDigiDollarPhase2Height{std::numeric_limits<int>::max()};  // Default; overridden: mainnet=3000000, testnet=600, regtest=650
int nOracleRequiredMessages{1};  // Phase One: 1, Phase Two regtest: 4, testnet: 9, mainnet: 9 (RC30)
int nOracleTotalOracles{1};      // Phase One: 1, Phase Two regtest: 7, testnet: 17, mainnet: 17 (RC30)
```

### 14.2 Network-Specific Configuration

| Network | Phase | Consensus | Oracles Defined | Activation Height | Status |
|---------|-------|-----------|-----------------|-------------------|--------|
| Mainnet | One | **DISABLED** | 17 vOraclePublicKeys defined, validation bypassed | Block 3000000 (nOracleActivationHeight; nDDActivationHeight=22014720; validation bypassed) | ❌ NOT FUNCTIONAL |
| Testnet | One | 9-of-17 (RC30) | 17 | Block 600 | ✅ Working |
| RegTest | One | 4-of-7 | 7 | Block 650 | ✅ Working |

> **⚠️ CRITICAL**: Mainnet oracle validation returns true at bundle_manager.cpp:2229.
> This means mainnet will accept ANY oracle data without verification.
> Phase Two infrastructure exists but cannot be enabled until mainnet validation is fixed.

### 14.3 Testnet Oracle Keys (All 17 Defined — RC30 slot order 0-16)

**Location**: `src/kernel/chainparams.cpp`

```cpp
// All 17 testnet oracles are ACTIVE for 9-of-17 consensus (RC30)
// Slot order matches chainparams.cpp (ordered 0..16):
consensus.vOraclePublicKeys.push_back("...");  // 0  Jared (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 1  Green Candle (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 2  Bastian (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 3  DanGB (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 4  Shenger (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 5  Ycagel (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 6  Aussie (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 7  LookInto (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 8  JohnnyLawDGB (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 9  Ogilvie (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 10 ChopperBrian (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 11 hallvardo (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 12 DaPunzy (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 13 DigiByteForce (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 14 Neel (ACTIVE)
consensus.vOraclePublicKeys.push_back("...");  // 15 BlindDave (placeholder)
consensus.vOraclePublicKeys.push_back("...");  // 16 GTO90 (placeholder)
```

### 14.4 Phase Two Validation Functions

**Location**: `src/oracle/bundle_manager.cpp`

#### ValidatePhaseTwoBundle() (lines 2559+)
```cpp
bool OracleBundleManager::ValidatePhaseTwoBundle(const COracleBundle& bundle,
                                                   const Consensus::Params& params)
{
    // Requirements:
    // 1. Minimum message count (nOracleRequiredMessages)
    // 2. No duplicate oracle IDs
    // 3. Each oracle must be in active set for current epoch
    // 4. Schnorr signature required and verified for each message
    // 5. Enough valid signatures to meet consensus threshold
    // 6. Calculated consensus price must match bundle median
}
```

#### CalculateConsensusPrice() (lines 2756+)
```cpp
CAmount OracleBundleManager::CalculateConsensusPrice(const COracleBundle& bundle,
                                                      const Consensus::Params& params)
{
    // Algorithm:
    // 1. Collect all valid signed prices
    // 2. Sort prices for IQR calculation
    // 3. Apply IQR outlier filtering (1.5 * IQR rule)
    // 4. Calculate median of filtered prices
    // 5. Fall back to unfiltered median if all outliers
}
```

**IQR Outlier Filtering**:
```
Q1 = 25th percentile
Q3 = 75th percentile
IQR = Q3 - Q1
Lower bound = Q1 - (1.5 * IQR)
Upper bound = Q3 + (1.5 * IQR)
Reject prices outside [lower_bound, upper_bound]
```

#### GetRequiredConsensus() (lines 2519-2524)
```cpp
int OracleBundleManager::GetRequiredConsensus(int block_height,
                                               const Consensus::Params& params)
{
    if (block_height >= params.nDigiDollarPhase2Height) {
        return params.nOracleRequiredMessages;  // 9 for testnet, 4 for regtest, 9 for mainnet (RC30)
    }
    return 1;  // Phase One: 1-of-1
}
```

### 14.5 Activating Phase Two on Testnet

To enable Phase Two on testnet, change in `chainparams.cpp`:
```cpp
consensus.nDigiDollarPhase2Height = <desired_block_height>;
consensus.nOracleRequiredMessages = 9;  // 9-of-17 for testnet (RC30)
```

---

## Document Status

**Version**: 7.0 - Validated Against Actual Codebase
**Last Updated**: 2026-02-01
**Implementation Status**: Phase One ~70% Complete (TESTNET/REGTEST ONLY)
**Test Coverage**: 826+ unit tests + 36 functional tests

**What's Verified Working**:
- ✅ Price format verified as micro-USD (1,000,000 = $1.00)
- ✅ Byte-level format specifications code-verified
- ✅ Data structures (COraclePriceMessage, COracleBundle, OracleNodeInfo)
- ✅ P2P message handling with rate limiting (3600 msg/hr/peer)
- ✅ 6 exchange APIs working (Binance, KuCoin, Gate.io, HTX, Crypto.com, CoinGecko)
- ✅ Block validation on TESTNET (height 600) and REGTEST (height 650)
- ✅ Mock Oracle default: 6500 micro-USD ($0.0065/DGB)

**Critical Issues (Must Fix Before Mainnet)**:
- ❌ **MAINNET VALIDATION DISABLED** - bundle_manager.cpp:2229 returns true
- ❌ **5 broken/removed exchange APIs** - Coinbase, Kraken, Messari, Bittrex/Poloniex; CoinMarketCap removed
- ❌ **MockOracleManager leaks** - err.cpp:403 is guarded by regtest check (line 402), but MockOracleManager singleton is instantiated globally
- ❌ **ERR system broken** - txbuilder.cpp:273 GetCurrentSystemCollateral() returns hardcoded 150%
- ❌ **GetBestHeight() stub** - bundle_manager.cpp:47 returns hardcoded 0
- ❌ **sendoracleprice REMOVED** - digidollar.cpp:3642 (security vulnerability - fake price injection)
- ❌ **Signature bypass** - Empty schnorr_sig accepted without verification

**Key Technical Details**:
```
Oracle Price Format:   Micro-USD (1,000,000 = $1.00 USD)
Validation Range:      100 - 100,000,000 micro-USD ($0.0001 - $100.00)
Compact Script Size:   22 bytes (OP_RETURN + OP_ORACLE + data)
Full Message Size:     128 bytes (with 64-byte Schnorr signature)
Phase One Consensus:   1-of-1 (testnet/regtest ONLY - mainnet disabled)
Phase Two Consensus:   9-of-17 testnet, 4-of-7 regtest, 9-of-17 mainnet (RC30; Phase 3 MuSig2 v0x03 infrastructure exists)
Activation Heights:    Mainnet=3000000 (nOracleActivationHeight; nDDActivationHeight=22014720; validation bypassed), Testnet=600, Regtest=650
```

## Known TODOs and Stubs (VERIFIED)

| File:Line | Function | Issue | Priority |
|-----------|----------|-------|----------|
| `bundle_manager.cpp:2229` | ValidateBlockOracleData | Mainnet returns true immediately | CRITICAL |
| `bundle_manager.cpp:47-51` | GetBestHeight() | Returns hardcoded 0 | HIGH |
| `txbuilder.cpp:29,273` | GetCurrentSystemCollateral() | Returns hardcoded 150% (ERR never activates) | HIGH |
| `err.cpp:403` | ShouldBlockMinting() | MockOracleManager guarded by regtest check (line 402), but singleton instantiated globally | MEDIUM |
| `digidollar.cpp:3642` | sendoracleprice | REMOVED: Security vulnerability (fake price injection) | FIXED |
| `exchange.cpp` | Coinbase fetcher | Broken - API changed | LOW |
| `exchange.cpp` | Kraken fetcher | Broken - API changed | LOW |
| `exchange.cpp` | Messari fetcher | Broken - API deprecated | LOW |
| `exchange.cpp` | Bittrex/Poloniex | Broken - Exchanges defunct | LOW |

---

*For complete sections 6-13 covering P2P Networking, Exchange Integration, Testing, Configuration, etc., the document continues in the same detailed manner.*
