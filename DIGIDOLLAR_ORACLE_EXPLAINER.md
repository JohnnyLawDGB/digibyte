# DigiDollar Oracle System - Simple Explainer

## Overview

The Oracle System is the "price engine" that powers DigiDollar. It tells the DigiByte blockchain the current DGB/USD exchange rate, enabling DigiDollar to know how much your DGB collateral is worth in real-time.

**Think of it like this**: When you lock DGB to mint DigiDollars, the blockchain needs to know: "Is $200 of DGB actually $200?" The oracle system provides this answer by fetching real prices from major cryptocurrency exchanges.

---

## Why Do We Need Oracles?

### The Blockchain Blind Spot

Blockchains are **isolated by design** - they can't access the internet, fetch data, or "see" outside their own network. This creates a fundamental problem:

**Without oracles**: The blockchain has no idea what DGB is worth in USD. It only knows you locked 1,000 DGB - but is that $50 or $5,000? It can't tell.

**With oracles**: The blockchain receives verified price feeds from the real world. Now it knows: "1,000 DGB = $50 USD at block height 1,234,567"

### Why Trust Matters

The oracle is a **critical trust point** - if it reports a fake price, the entire DigiDollar system breaks down. That's why Phase Two will implement **multi-oracle consensus** (8-of-15 agreement required). But for Phase One testnet, we start simple with a single trusted oracle to prove the concept works.

---

## How It Works: Phase One Implementation

### Current Status: Single Oracle (1-of-1 Consensus)

Phase One implements a **streamlined testnet-ready system**:

- **One Oracle Node**: Operated by DigiByte Devs For Testnet Only
- **Testnet Only**: Not active on mainnet (safety first!)
- **Simple Trust Model**: The single oracle's price is the consensus price
- **Compact Storage**: Only 21 bytes per block (minimal blockchain overhead)

**This is like a prototype** - we're testing the concept before deploying the full multi-oracle system in Phase Two.

### **IMPORTANT: Current Implementation Status**

**What This Document Describes**: The **intended Phase One architecture** - the complete system as designed.

**What's Currently Implemented in DigiByte Core**:
- ✅ **P2P oracle message validation and relay** - Nodes can receive, validate, and propagate oracle messages
- ✅ **Block validation and compact storage** - Blocks can include oracle data; nodes validate it
- ✅ **Price cache system** - Stores oracle prices by block height
- ✅ **Mock oracle for RegTest** - Testing infrastructure using `setmockoracleprice` RPC

**What's NOT Yet Integrated into DigiByte Core**:
- ❌ **Direct exchange fetching** - DigiByte Core doesn't fetch prices from exchanges
- ❌ **Oracle daemon** - The price-fetching daemon is separate software (not part of Core)

**How It Works Today**:
- **RegTest**: Uses `setmockoracleprice` RPC command to manually set prices for testing
- **Testnet**: Would receive oracle messages from an **external oracle daemon** via P2P network
- **Mainnet**: Completely disabled until Phase Two (safety guard)

**The External Oracle Daemon** (separate from DigiByte Core):
- Fetches prices from 12 exchanges every 15 seconds
- Calculates median with MAD outlier filtering
- Creates and signs 128-byte oracle messages
- Broadcasts to P2P network (which DigiByte Core nodes receive)

**Think of it like email**: DigiByte Core is like your email client (receives, validates, stores messages). The oracle daemon is like the email server (creates and sends messages). They're separate but work together.

### The Complete Flow (Every 15 Seconds)

Here's exactly what happens when the oracle updates the price:

---

### **Visual Flow Diagrams**

#### **Overall Oracle System Architecture**

```
┌──────────────────────────────────────────────────────────────┐
│  PHASE ONE ORACLE ARCHITECTURE (Current Implementation)      │
└──────────────────────────────────────────────────────────────┘

                    EXTERNAL ORACLE DAEMON
                    (Not part of DigiByte Core)
                            │
                            ▼
            ┌───────────────────────────────┐
            │  1. Fetch Prices from         │
            │     10 Exchanges              │
            │     (Binance, Coinbase, etc.) │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  2. Calculate Median Price    │
            │     with MAD Outlier Filter   │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  3. Create & Sign Message     │
            │     (BIP-340 Schnorr)         │
            │     128-byte full message     │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  4. Broadcast to P2P Network  │
            │     (ORACLEPRICE message)     │
            └───────────────┬───────────────┘
                            │
        ┌───────────────────┴───────────────────┐
        │                                       │
        ▼                                       ▼
┌─────────────────┐                   ┌─────────────────┐
│  DigiByte Node  │                   │  DigiByte Node  │
│  (Validator)    │◄──P2P Relay──────►│  (Validator)    │
└────────┬────────┘                   └────────┬────────┘
         │                                     │
         └──────────────┬──────────────────────┘
                        │
                        ▼
            ┌───────────────────────────────┐
            │  5. Validate Schnorr Signature│
            │     Check price range         │
            │     Check timestamp           │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  6. Relay to Peers            │
            │     (if valid)                │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  7. Miner Includes in Block   │
            │     (Compact 21-byte format)  │
            │     in Coinbase OP_RETURN     │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  8. Block Validation          │
            │     CheckBlock() verifies     │
            │     oracle data               │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  9. Price Cache Update        │
            │     Store: height → price     │
            │     (Last 1,000 blocks)       │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │  10. DigiDollar Uses Price    │
            │      For collateral calcs     │
            └───────────────────────────────┘
```

#### **P2P Message Flow (Network Propagation)**

```
┌─────────────────────────────────────────────────────────────┐
│  HOW ORACLE MESSAGES SPREAD ACROSS THE NETWORK (2-5 seconds)│
└─────────────────────────────────────────────────────────────┘

Oracle Node
     │
     │ Sends 128-byte ORACLEPRICE message
     │
     ├──────┬──────┬──────┬──────┬──────┬──────┬──────┐
     ▼      ▼      ▼      ▼      ▼      ▼      ▼      ▼
   Peer1  Peer2  Peer3  Peer4  Peer5  Peer6  Peer7  Peer8
     │      │      │      │      │      │      │      │
     │  Each peer validates:                          │
     │  ✓ Schnorr signature correct?                  │
     │  ✓ Price in range (100-100M micro-USD)?         │
     │  ✓ Timestamp fresh (<5 min old)?               │
     │  ✓ Not duplicate?                              │
     │                                                 │
     ├──────┴──────┴──────┼──────┴──────┴──────┴──────┤
     │                    │                            │
     │  If valid: RELAY   │  If invalid: REJECT        │
     │                    │                            │
     ▼                    ▼                            ▼
┌─────────┐          ┌─────────┐                  ┌─────────┐
│ 64 more │          │ 64 more │                  │ Misbehave│
│  peers  │          │  peers  │                  │ +penalty │
└────┬────┘          └────┬────┘                  └─────────┘
     │                    │
     ├────────────────────┤
     │                    │
     ▼                    ▼
  512 more            4,096 more
   peers                peers
     │                    │
     └────────┬───────────┘
              │
              ▼
    95% of network reached
       within 2-5 seconds
```

#### **Block Validation Flow (CheckBlock)**

```
┌──────────────────────────────────────────────────────────────┐
│  BLOCK VALIDATION: How Nodes Verify Oracle Data              │
└──────────────────────────────────────────────────────────────┘

New Block Received
     │
     ▼
┌────────────────────────────────┐
│ Basic Block Validation         │
│ (PoW, Merkle Root, etc.)       │
└────────┬───────────────────────┘
         │
         ▼
    ╔═══════════════════════╗
    ║ Oracle Validation     ║  ← validation.cpp:4130
    ║ (CheckBlock)          ║
    ╚═══════════════════════╝
         │
         ▼
┌────────────────────────────────┐
│ 1. Network Filter              │
│    Only testnet/regtest?       │
│    (Skip on mainnet)           │───NO──→ Skip oracle check
└────────┬───────────────────────┘
         │ YES
         ▼
┌────────────────────────────────┐
│ 2. Activation Height           │
│    Block ≥ 1,000,000?          │
│    (Testnet activation)        │───NO──→ Skip oracle check
└────────┬───────────────────────┘
         │ YES
         ▼
┌────────────────────────────────┐
│ 3. Find Oracle Data            │
│    Look for OP_RETURN OP_ORACLE│
│    in coinbase vout[1]         │
└────────┬───────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│ 4. Parse Compact Format        │
│    Extract: oracle_id, price,  │
│    timestamp (21 bytes)        │
└────────┬───────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│ 5. Validate Structure          │
│    ✓ Version = 0x01?           │
│    ✓ Oracle ID = 0?            │
└────────┬───────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│ 6. Validate Price Range        │
│    ✓ Price: 100-100M micro-USD?│
│    ($0.0001 - $100.00 per DGB) │
└────────┬───────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│ 7. Validate Timestamp          │
│    ✓ Not > block time + 60s?  │
│    ✓ Not < block time - 3600s?│
└────────┬───────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│ 8. Phase One Consensus         │
│    ✓ Exactly 1 oracle message?│
│    (8-of-15 in Phase Two)      │
└────────┬───────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│ 9. Oracle Authorization        │
│    ✓ Oracle ID 0 authorized in│
│      chainparams?              │
│    ✓ Oracle is active?         │
└────────┬───────────────────────┘
         │
    ┌────┴────┐
    │         │
   PASS      FAIL
    │         │
    ▼         ▼
┌─────────┐ ┌───────────────┐
│ Accept  │ │ REJECT Block  │
│ Block   │ │ (Invalid)     │
└────┬────┘ └───────────────┘
     │
     ▼
┌─────────────────────────────────┐
│ ConnectBlock()                  │
│ Update price cache:             │
│ height_to_price[700] = 5 cents  │
└─────────────────────────────────┘
```

#### **Price Format Explanation**

```
┌──────────────────────────────────────────────────────────────┐
│  MICRO-USD FORMAT: Why 1,000,000 micro-USD = $1.00?          │
└──────────────────────────────────────────────────────────────┘

PROBLEM: Floating Point Errors
────────────────────────────────
double price = 0.0065;
double collateral = price * 2.0;
Result: 0.012999999999 ❌  (WRONG!)


SOLUTION: Integer Arithmetic (Micro-USD)
────────────────────────────────────────────────
uint64_t price_micro_usd = 6500;     // 6500 micro-USD = $0.0065
uint64_t collateral_micro_usd = price_micro_usd * 2;
Result: 13000 ✓  (EXACT!)


CONVERSION TABLE:
─────────────────
USD Price    Micro-USD           Storage (uint64_t)
─────────    ─────────           ──────────────────
$0.0001      100                 100 (MIN)
$0.001       1,000               1,000
$0.0065      6,500               6,500 (realistic DGB price)
$0.01        10,000              10,000
$0.10        100,000             100,000
$1.00        1,000,000           1,000,000
$10.00       10,000,000          10,000,000
$100.00      100,000,000         100,000,000 (MAX)


VALIDATION LIMITS:
──────────────────
MIN_PRICE_MICRO_USD = 100         ($0.0001 per DGB)
MAX_PRICE_MICRO_USD = 100,000,000 ($100.00 per DGB)


WHY THIS FORMAT?
────────────────
✓ Exact arithmetic (no rounding errors)
✓ High precision (6 decimal places)
✓ Compact (fits in 8 bytes)
✓ Fast (integer operations)
✓ Safe (no float overflow issues)
```

---

┌─────────────────────────────────────────────────────────────┐
│ ⓘ NOTE: Steps 1-4 describe the EXTERNAL ORACLE DAEMON      │
│   (not part of DigiByte Core). DigiByte nodes receive the   │
│   final signed message via P2P and validate it (Steps 5-10).│
└─────────────────────────────────────────────────────────────┘

#### Step 1: Fetch Prices from 12 Exchanges (Every 15 seconds)

The oracle connects to 12 major cryptocurrency exchanges simultaneously:

- Binance
- Coinbase
- Kraken
- KuCoin
- Crypto.com
- Bittrex
- Poloniex
- Messari
- CoinMarketCap
- CoinGecko
- Gate.io
- HTX (Huobi)

**Example responses**:
```
Binance:    $0.05023
Coinbase:   $0.05018
Kraken:     $0.05021
KuCoin:     $0.05017
Crypto.com: $0.05024
Bittrex:    $0.05020
Poloniex:   $0.05019
Messari:    $0.05022
CMC:        $0.05020
CoinGecko:  $0.05021
```

#### Step 2: Calculate Median Price

The oracle uses **median calculation** (not average) to filter out outliers:

**Why median?** If one exchange has bad data ($0.10) while others say $0.05, the median stays at $0.05. An average would be skewed to $0.055.

**Our example**:
- Sorted prices: [$0.05017, $0.05018, $0.05019, $0.05020, $0.05020, $0.05021, $0.05021, $0.05022, $0.05023, $0.05024]
- Middle value (median): **$0.05020**

**Advanced outlier filtering** (Median Absolute Deviation):
- Removes prices that are statistically too far from the median
- Prevents a single exchange from manipulating the price
- Keeps the system resilient even if 2-3 exchanges have bad data

#### Step 3: Convert to Micro-USD

The oracle converts the price to **micro-USD** format:

**Micro-USD Format**: `1,000,000 micro-USD = $1.00 USD`

Why this format?
- ✅ **High Precision**: 6 decimal places (perfect for crypto prices at any level)
- ✅ **No Floating Point**: Integer arithmetic prevents rounding errors
- ✅ **Wide Range**: Supports prices from $0.0001 to $100.00 per DGB
- ✅ **Compact**: Fits in 8 bytes (uint64_t)

**Conversion**:
```
$0.0065 USD = 6,500 micro-USD (realistic DGB price)
$0.05020 USD = 50,200 micro-USD
```

#### Step 4: Sign the Message with Schnorr Signature

The oracle creates a price message containing:

```
Oracle ID:     0 (always 0 in Phase One)
Price:         6500 (micro-USD, = $0.0065 per DGB)
Timestamp:     1732204800 (Unix timestamp)
Block Height:  700 (current blockchain height)
Nonce:         0x123456789ABCDEF0 (random number for uniqueness)
Oracle Pubkey: <32-byte BIP-340 Schnorr public key>
```

Then it **signs** this message using **BIP-340 Schnorr signatures**:

**What's a Schnorr signature?** A cryptographic proof that this message came from the authorized oracle and wasn't tampered with. Think of it like a tamper-proof wax seal on a letter.

**Key properties**:
- ✅ **Compact**: Only 64 bytes (smaller than traditional Bitcoin signatures)
- ✅ **Secure**: Mathematically impossible to forge without the private key
- ✅ **Deterministic**: Same message + same key = same signature (reproducible)
- ✅ **Non-malleable**: Can't be modified after creation

**Full message size**: 128 bytes (used for P2P transmission)

#### Step 5: Broadcast to P2P Network

The oracle broadcasts the **full 128-byte message** to all connected peers via the P2P network:

```
Message Type: "oracleprice"
Payload: <128-byte signed message>
```

**Network propagation**:
- Oracle sends to 8-10 connected peers
- Each peer validates the signature
- Valid messages are relayed to their peers
- Within 2-5 seconds, 95% of the network has the message

**Validation at each node**:
1. ✅ Check Schnorr signature (authentic oracle?)
2. ✅ Check oracle ID is 0 (Phase One requirement)
3. ✅ Check timestamp (not too old, not in future)
4. ✅ Check price range ($0.0001 - $100.00 per DGB)
5. ✅ Check for duplicates (already seen this message?)

**Rate limiting**: Nodes reject more than 3 oracle messages per minute (180 per hour) from any single peer (prevents spam attacks).

#### Step 6: Compact Storage in Blocks

When a miner creates a new block, they include the oracle data in the **coinbase transaction** (the first transaction in every block).

**Coinbase structure**:
```
vout[0]: 72,000 DGB → Miner reward
vout[1]: 0 DGB → OP_RETURN OP_ORACLE <compact oracle data>
vout[2]: 0 DGB → Witness commitment (SegWit)
```

**The compact 21-byte format** (blockchain storage):

The full 128-byte message is too large to store in every block. Instead, the miner creates a **compact 21-byte version**:

```
Byte 0:      OP_RETURN (0x6a) - "This output is unspendable"
Byte 1:      OP_ORACLE (0xbf) - "This is oracle data"
Byte 2:      0x01 - "Push 1 byte" (for version)
Byte 3:      0x01 - Version (Phase One format)
Byte 4:      0x11 - "Push 17 bytes" (for data)
Byte 5:      0x00 - Oracle ID (always 0)
Bytes 6-13:  <Price in micro-USD, 8 bytes>
Bytes 14-21: <Timestamp, 8 bytes>
```

**Example (hex format)**:
```
6a bf 01 01 11 00 05 00 00 00 00 00 00 00 00 2f 50 65 00 00 00 00 00
│  │  │  │  │  └─────────────┘ └─────────────────┘
│  │  │  │  │   Price: 6500 micro-USD  Timestamp: 1,700,000,000
│  │  │  │  Oracle ID: 0
│  │  │  Version: 1
│  │  Push 18 bytes
│  OP_ORACLE
OP_RETURN
```

**Total: 21 bytes = 25.3% of the 83-byte OP_RETURN limit** ✅

**Why compact format?**
- ✅ **Saves space**: 128 bytes → 21 bytes (83.6% reduction!)
- ✅ **Fits easily**: Only 26% of OP_RETURN size limit
- ✅ **Fast validation**: Minimal overhead during block verification
- ✅ **Scalable**: Leaves room for future enhancements

**What's excluded?**
- ❌ No Schnorr signature (saves 64 bytes)
- ❌ No public key (saves 32 bytes)
- ❌ No block height (saves 4 bytes)
- ❌ No nonce (saves 8 bytes)

**How can we trust it without a signature?**

The compact format **relies on chainparams trust**: The oracle's public key is hardcoded in the DigiByte Core source code (`chainparams.cpp`). Nodes know which oracle is authorized, so they trust that if a block has oracle data from oracle_id=0, it came from the legitimate oracle.

**This works for Phase One because**:
- ✅ Single oracle (no consensus needed)
- ✅ Signature verified during P2P broadcast (before block inclusion)
- ✅ Testnet only (lower security requirements than mainnet)

**Phase Two will add signatures back** for multi-oracle consensus with on-chain verification.

#### Step 7: Block Validation

Every node validates incoming blocks. When a block contains oracle data, the validation process checks:

**CheckBlock() Validation** (consensus rules):

1. ✅ **Network check**: Only validate on testnet/regtest (skip on mainnet)
2. ✅ **Activation height**: Block height ≥ 1,000,000 (testnet activation)
3. ✅ **Find OP_ORACLE output**: Look for `OP_RETURN OP_ORACLE` in coinbase vout[1]
4. ✅ **Extract compact data**: Parse the 21-byte format
5. ✅ **Validate structure**: Version byte = 0x01, Oracle ID = 0
6. ✅ **Validate price range**: 100-100,000,000 micro-USD ($0.0001-$100.00 per DGB)
7. ✅ **Validate timestamp**: Not more than 1 hour old, not in future
8. ✅ **Phase One consensus**: Exactly 1 message (reject if multiple oracles)
9. ✅ **Oracle authorization**: Oracle ID 0 is in chainparams and active

**If validation fails**: The entire block is **rejected**. The miner loses their block reward.

**If validation succeeds**: Block is accepted and the price is cached.

#### Step 8: Price Cache Update

After a valid block is connected to the blockchain, the oracle price is stored in the **price cache**:

**ConnectBlock() Updates**:
```cpp
height_to_price[700] = 6500 micro-USD  // $0.0065 per DGB
```

**Cache properties**:
- ✅ **Thread-safe**: Protected by mutex (multiple threads can access safely)
- ✅ **Limited size**: Keeps last 1,000 blocks (auto-evicts old prices)
- ✅ **Fast lookup**: O(1) access time (instant retrieval)

**Example cache**:
```
Block 695: 6400 micro-USD ($0.0064 per DGB)
Block 696: 6450 micro-USD ($0.00645 per DGB)
Block 697: 6500 micro-USD ($0.0065 per DGB)
Block 698: 6500 micro-USD ($0.0065 per DGB)
Block 699: 6500 micro-USD ($0.0065 per DGB)
Block 700: 6500 micro-USD ($0.0065 per DGB) ← Current price
```

#### Step 9: DigiDollar Uses the Price

When you mint DigiDollars, the system queries the price cache:

```cpp
uint64_t current_price = OracleBundleManager::GetInstance().GetLatestPrice();
// Returns: 6500 micro-USD ($0.0065 per DGB)
```

**Collateral calculation example**:

You want to mint $100 DigiDollars:

```
Required collateral: 200% (Phase One testnet setting)
DGB price: $0.05 per DGB (from oracle)

Calculation:
$100 DigiDollars × 200% collateral = $200 worth of DGB needed
$200 ÷ $0.05 per DGB = 4,000 DGB required

You must lock: 4,000 DGB to mint 100 DigiDollars
```

**The oracle price is critical** - without it, the system has no idea how much DGB equals $200 USD.

---

## Network Differences: Regtest vs Testnet vs Mainnet

The oracle system behaves differently on each network:

### RegTest (Development/Testing)

**Purpose**: Local testing for developers

**Oracle Mode**: **Mock Oracle (Fake Prices)**

**How it works**:
- No real oracle daemon running
- No exchange API calls
- Prices are **manually set** or use default values
- Useful for unit tests and functional tests

**Example**:
```cpp
MockOracleManager::GetInstance().SetMockPrice(6500); // Set to 6500 micro-USD ($0.0065)
```

**Why mock?**
- ✅ **Fast**: No network calls, instant price updates
- ✅ **Deterministic**: Tests always get same results
- ✅ **Isolated**: Doesn't depend on external exchanges
- ✅ **Controllable**: Can simulate any price scenario

**Activation height**: Block 1 (immediate)

**Use case**: Running automated tests, developing features locally

### Testnet (Public Testing Network)

**Purpose**: Real-world testing before mainnet

**Oracle Mode**: **Real Oracle with Real Prices**

**How it works**:
- Actual oracle daemon running at `oracle.digibyte.io:9001`
- Fetches **real prices** from 12 exchanges
- Broadcasts every 15 seconds via P2P
- Miners include real oracle data in blocks

**Key differences from mainnet**:
- ⚠️ **Single oracle** (1-of-1 consensus, not 8-of-15)
- ⚠️ **Testnet DGB** (free, no value)
- ⚠️ **Lower security** (acceptable for testing)

**Activation height**: Block 1,000,000

**Use case**:
- Testing DigiDollar minting with real prices
- Testing oracle daemon upgrades
- Validating P2P propagation
- Preparing for mainnet deployment

### Mainnet (Production Network)

**Purpose**: Real DigiByte blockchain with actual value

**Oracle Mode**: **DISABLED (Phase One)**

**How it works**:
- Oracle validation is **completely disabled**
- Activation height set to `INT_MAX` (never activates)
- Blocks are **not required** to have oracle data
- Code has **safety guards** preventing accidental activation

**Why disabled?**

Phase One's single oracle (1-of-1 consensus) is **not secure enough for mainnet**. If the single oracle:
- Gets hacked → Attacker controls all DigiDollar prices
- Goes offline → DigiDollar system stops working
- Has a bug → Could report wrong prices

**Phase Two requirements for mainnet**:
- ✅ **15 independent oracles** (geographic + legal diversity)
- ✅ **8-of-15 consensus** (majority agreement required)
- ✅ **Economic incentives** (staking, slashing for bad behavior)
- ✅ **Reputation system** (track oracle accuracy over time)
- ✅ **On-chain signatures** (all 8 signatures verified in blocks)

**Activation height**: `2,147,483,647` (INT_MAX - never)

**Use case**: N/A (not active yet)

---

## The Price Feed Mechanism

### 12 Exchange Data Sources

The oracle aggregates prices from **12 major exchanges** to ensure reliability:

**High-Volume Exchanges** (Primary Sources):
1. **Binance** - Largest crypto exchange globally
2. **Coinbase** - Major US exchange (regulated)
3. **Kraken** - Large US exchange (regulated)
4. **KuCoin** - Popular international exchange
5. **Crypto.com** - Growing exchange with good liquidity

**Additional Data Sources** (Backup/Verification):
6. **Bittrex** - Established US exchange
7. **Poloniex** - Long-running exchange
8. **Messari** - Professional crypto data aggregator
9. **CoinMarketCap** - Most-visited crypto data site
10. **CoinGecko** - Popular crypto data aggregator
11. **Gate.io** - Large international exchange
12. **HTX (Huobi)** - Major global exchange

**Why 12 exchanges?**
- ✅ **Redundancy**: If 2-3 exchanges are down, oracle still works
- ✅ **Outlier detection**: Can identify and filter bad data
- ✅ **Market representation**: Captures global DGB price across multiple markets
- ✅ **Manipulation resistance**: Hard to manipulate 12 independent data sources

**Minimum requirement**: At least **3 exchanges** must return valid prices (30% threshold). If fewer than 3 respond, the oracle doesn't update the price.

### Median Calculation

**Why median instead of average?**

**Average problem**:
```
9 exchanges: $0.05
1 exchange:  $0.50 (bad data)

Average: ($0.05 × 9 + $0.50) ÷ 10 = $0.095 (WRONG!)
Median:  $0.05 (CORRECT!)
```

The average is **skewed by outliers**, while the median is **resistant to outliers**.

**How median works**:
1. Sort all prices from lowest to highest
2. If odd count: Take the middle value
3. If even count: Average the two middle values

**Example with 12 exchanges**:
```
Raw data: [$0.05017, $0.05021, $0.05018, $0.05020, $0.05024,
           $0.05020, $0.05019, $0.05022, $0.05023, $0.05021]

Sorted:   [$0.05017, $0.05018, $0.05019, $0.05020, $0.05020,
           $0.05021, $0.05021, $0.05022, $0.05023, $0.05024]
                                         ↑         ↑
                                    Middle two (positions 5 & 6)

Median: ($0.05020 + $0.05021) ÷ 2 = $0.050205 ≈ $0.05020
```

### Advanced Outlier Filtering (MAD Algorithm)

**MAD = Median Absolute Deviation**

This statistical method identifies prices that are **too far** from the median:

**Algorithm**:
1. Calculate median price (M)
2. Calculate absolute deviations: `|price - M|` for each price
3. Calculate MAD: median of all absolute deviations
4. Reject prices where: `|price - M| > 3 × MAD`

**Example**:
```
Prices: [$0.05, $0.05, $0.05, $0.05, $0.05, $0.05, $0.05, $0.05, $0.05, $0.20]

Step 1: Median (M) = $0.05
Step 2: Deviations = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0.15]
Step 3: MAD = median([0, 0, 0, 0, 0, 0, 0, 0, 0, 0.15]) = 0
Step 4: Check $0.20: |0.20 - 0.05| = 0.15 > 3 × 0 → REJECT

Final median: $0.05 (outlier removed)
```

**Why MAD?**
- ✅ **Statistically robust**: Works even with multiple outliers
- ✅ **Self-adjusting**: Threshold adapts to market volatility
- ✅ **Industry standard**: Used in professional trading systems

### Micro-USD Format (On-Chain Storage)

**Format**: `1,000,000 micro-USD = $1.00 USD`

**Why this format?**

**Problem with floating point**:
```cpp
double price = 0.00650;
double collateral = price * 2.0;  // Should be 0.01300
// Actual result: 0.01299999999999 (rounding error!)
```

**Solution with micro-USD**:
```cpp
uint64_t price_micro_usd = 6500;  // 6500 micro-USD = $0.0065
uint64_t collateral_micro_usd = price_micro_usd * 2;  // 13000 micro-USD (exact!)
```

**Conversion examples**:

```
USD Price    →  Micro-USD          →  Storage (uint64_t)
$0.0001      →  100                →  100 (minimum)
$0.0065      →  6,500              →  6,500 (realistic DGB price)
$0.01        →  10,000             →  10,000
$0.10        →  100,000            →  100,000
$1.00        →  1,000,000          →  1,000,000
$10.00       →  10,000,000         →  10,000,000
$100.00      →  100,000,000        →  100,000,000 (maximum)
```

**Validation constraints**:
- ✅ **Minimum**: 100 micro-USD ($0.0001 per DGB) - prevents near-zero spam
- ✅ **Maximum**: 100,000,000 micro-USD ($100.00 per DGB) - reasonable upper bound

**Mathematical properties**:
- ✅ **Exact arithmetic**: No rounding errors
- ✅ **Compact**: Fits in 8 bytes (uint64_t)
- ✅ **Precision**: 6 decimal places for USD values
- ✅ **Max value**: 18,446,744,073,709,551,615 micro-USD = $18 trillion per DGB (way beyond realistic range!)

---

## The Compact 21-Byte On-Chain Storage Format

### Why Compact Format?

**The challenge**: Oracle price data must be stored in **every block** (every 15 seconds). This adds up fast:

```
Full format: 128 bytes per block
Blocks per day: 5,760 (86,400 seconds ÷ 15)
Data per day: 737,280 bytes = 720 KB/day
Data per year: 262 MB/year

Compact format: 21 bytes per block
Blocks per day: 5,760
Data per day: 120,960 bytes = 118 KB/day
Data per year: 43 MB/year

Savings: 219 MB/year (83.6% reduction!)
```

Over 10 years, this saves **2.19 GB** of blockchain space - significant for a small 21-byte optimization!

### OP_ORACLE: Complete Lifecycle Flowchart

```
┌────────────────────────────────────────────────────────────────────┐
│  OP_ORACLE COMPLETE LIFECYCLE: From Creation to DigiDollar Usage  │
└────────────────────────────────────────────────────────────────────┘

STEP 1: P2P BROADCAST (Full 128-byte Format)
════════════════════════════════════════════════════════════════════
External Oracle Daemon creates signed message:
┌─────────────────────────────────────────┐
│ ORACLEPRICE P2P Message (128 bytes)    │
├─────────────────────────────────────────┤
│ oracle_id:        0         [4 bytes]  │
│ price:            6500      [8 bytes]  │  ← Micro-USD ($0.0065/DGB)
│ timestamp:        1732204800 [8 bytes] │
│ block_height:     700       [4 bytes]  │
│ nonce:            0x123...  [8 bytes]  │
│ oracle_pubkey:    <32 bytes>           │
│ schnorr_sig:      <64 bytes>           │  ← BIP-340 signature
└─────────────────────────────────────────┘
                    │
                    ▼
         ┌──────────────────────┐
         │ Broadcast to Network │
         └──────────────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
        ▼                       ▼
┌──────────────┐        ┌──────────────┐
│  Node A      │        │  Node B      │
│ Validates:   │        │ Validates:   │
│ ✓ Signature  │        │ ✓ Signature  │
│ ✓ Price range│        │ ✓ Price range│
│ ✓ Timestamp  │        │ ✓ Timestamp  │
└──────┬───────┘        └──────┬───────┘
       │                       │
       └───────────┬───────────┘
                   ▼
         Network-wide propagation
         (95% coverage in 2-5s)


STEP 2: BLOCK INCLUSION (Compact 22-byte Format with OP_ORACLE)
════════════════════════════════════════════════════════════════════
Miner creates coinbase transaction:
┌───────────────────────────────────────────────────────────────┐
│ Coinbase Transaction (First tx in block)                     │
├───────────────────────────────────────────────────────────────┤
│ vout[0]: 72,000 DGB  → Miner reward                          │
│ vout[1]: 0 DGB       → OP_RETURN OP_ORACLE <compact data>    │ ◄── HERE!
│ vout[2]: 0 DGB       → Witness commitment (SegWit)           │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
        ┌─────────────────────────────────────────┐
        │ OP_ORACLE OUTPUT STRUCTURE (22 bytes)  │
        ├─────────────────────────────────────────┤
        │ Byte  0: 0x6a  OP_RETURN               │ ◄── Makes output unspendable
        │ Byte  1: 0xbf  OP_ORACLE               │ ◄── Oracle data marker
        │ Byte  2: 0x01  PUSH 1 byte             │
        │ Byte  3: 0x01  Version (Phase One)     │
        │ Byte  4: 0x11  PUSH 17 bytes           │
        │ Byte  5: 0x00  Oracle ID = 0           │
        │ Bytes 6-13:    Price (6500 micro-USD)  │ ◄── Little-endian uint64
        │ Bytes 14-21:   Timestamp (Unix time)   │ ◄── Little-endian int64
        └─────────────────────────────────────────┘
                              │
                              ▼
                     Block is broadcast


STEP 3: BLOCK VALIDATION (CheckBlock)
════════════════════════════════════════════════════════════════════
Every node validates the block:
┌────────────────────────────────────────┐
│ Node receives new block                │
└────────────┬───────────────────────────┘
             ▼
    ┌────────────────────┐
    │ Basic validation   │
    │ (PoW, merkle, etc.)│
    └────────┬───────────┘
             ▼
    ┌────────────────────────────────────────────┐
    │ Oracle Validation (validation.cpp:4130)    │
    ├────────────────────────────────────────────┤
    │ 1. Find OP_ORACLE in coinbase vout[1]     │ ◄── Looks for 0x6a 0xbf
    │ 2. Extract 22-byte compact data            │
    │ 3. Parse: version, oracle_id, price, time  │
    │ 4. Validate:                               │
    │    ✓ Version == 0x01?                      │
    │    ✓ Oracle ID == 0?                       │
    │    ✓ Price: 100-100M micro-USD?            │
    │    ✓ Timestamp not too old/future?         │
    │    ✓ Exactly 1 oracle (Phase One)?         │
    │    ✓ Oracle authorized in chainparams?     │
    └────────┬───────────────────────────────────┘
             │
        ┌────┴────┐
        │         │
       PASS      FAIL
        │         │
        ▼         ▼
    Accept    Reject Block
    Block     (Invalid)


STEP 4: PRICE CACHE UPDATE (ConnectBlock)
════════════════════════════════════════════════════════════════════
After block is accepted:
┌─────────────────────────────────────────┐
│ ConnectBlock() extracts oracle price   │
└─────────────┬───────────────────────────┘
              ▼
┌─────────────────────────────────────────┐
│ OracleBundleManager::UpdatePriceCache() │
├─────────────────────────────────────────┤
│ height_to_price[700] = 6500 micro-USD   │ ◄── Cached in memory
└─────────────┬───────────────────────────┘
              │
              ▼
      ┌───────────────────┐
      │ Price Cache (RAM) │
      ├───────────────────┤
      │ [695] = 49500     │
      │ [696] = 49800     │
      │ [697] = 50000     │
      │ [698] = 50200     │
      │ [699] = 50100     │
      │ [700] = 50000  ◄─ Current price
      └───────────────────┘


STEP 5: DIGIDOLLAR USAGE
════════════════════════════════════════════════════════════════════
When user mints DigiDollars:
┌──────────────────────────────────────────┐
│ User: "I want to mint $100 DigiDollars" │
└──────────────┬───────────────────────────┘
               ▼
┌──────────────────────────────────────────────────┐
│ DigiDollar queries OracleBundleManager          │
│ price = GetLatestPrice() → 6500 micro-USD       │
└──────────────┬───────────────────────────────────┘
               ▼
┌──────────────────────────────────────────────────┐
│ Collateral Calculation:                         │
│                                                  │
│ Mint amount:    $100.00 = 100,000,000 micro-USD │
│ Collateral:     200% (Phase One)                │
│ Oracle price:   6500 micro-USD = $0.0065/DGB    │
│                                                  │
│ Required DGB:                                    │
│   ($100 × 2) ÷ $0.0065/DGB = 30,769 DGB         │
│                                                  │
│ User must lock: 30,769 DGB to mint $100 DD      │
└──────────────────────────────────────────────────┘
```

### OP_ORACLE Opcode: What Is It?

```
┌────────────────────────────────────────────────────────────────────┐
│  OP_ORACLE (0xbf): The Oracle Data Marker                         │
└────────────────────────────────────────────────────────────────────┘

OPCODE DEFINITION (src/script/script.h:214)
═══════════════════════════════════════════════════════════════════
OP_ORACLE = 0xbf  // Repurposed OP_NOP15 for oracle price data

CONTEXT: DigiDollar Custom Opcodes
─────────────────────────────────────────────────────────────────
OP_DIGIDOLLAR      = 0xbb  (OP_NOP11) - Marks DD outputs
OP_DDVERIFY        = 0xbc  (OP_NOP12) - Verify DD conditions
OP_CHECKPRICE      = 0xbd  (OP_NOP13) - Check oracle price
OP_CHECKCOLLATERAL = 0xbe  (OP_NOP14) - Verify collateral ratio
OP_ORACLE          = 0xbf  (OP_NOP15) - Oracle price data marker ◄── THIS ONE


WHAT DOES OP_ORACLE DO?
═══════════════════════════════════════════════════════════════════
Purpose: Marks an OP_RETURN output as containing oracle price data

Usage Pattern:
┌──────────────────────────────────────────────────────────────┐
│ OP_RETURN OP_ORACLE <version> <oracle_id> <price> <timestamp>│
│    0x6a      0xbf      0x01       0x00      ...      ...     │
│     ▲         ▲                                               │
│     │         │                                               │
│     │         └─ Oracle marker (distinguishes from other data)│
│     └─────────── Makes output unspendable                     │
└──────────────────────────────────────────────────────────────┘

WHY USE A CUSTOM OPCODE?
═══════════════════════════════════════════════════════════════════
✓ Instant recognition: "This is oracle data, not arbitrary data"
✓ Efficient parsing: No need to parse entire OP_RETURN
✓ Validation optimization: Nodes can skip non-oracle OP_RETURNs
✓ Future extensibility: Can add OP_ORACLE2, OP_ORACLE3 for formats
✓ Clear intent: Self-documenting code


HOW NODES DETECT OP_ORACLE
═══════════════════════════════════════════════════════════════════
Step 1: Scan coinbase outputs for OP_RETURN
┌────────────────────────────────────┐
│ for (const auto& out : coinbase)  │
│     if (out.scriptPubKey[0] == OP_RETURN)  // Found 0x6a
│         check_for_oracle();        │
└────────────────────────────────────┘

Step 2: Check if next byte is OP_ORACLE
┌────────────────────────────────────┐
│ if (scriptPubKey[1] == OP_ORACLE)  │  // Found 0xbf
│     parse_oracle_data();           │
│ else                               │
│     ignore; // Other OP_RETURN data│
└────────────────────────────────────┘

Step 3: Extract oracle price
┌────────────────────────────────────┐
│ ExtractOracleBundle(tx, bundle);  │
│ // Parses version, oracle_id,     │
│ // price, timestamp                │
└────────────────────────────────────┘


EXAMPLE: Finding OP_ORACLE in a Block
═══════════════════════════════════════════════════════════════════
Raw scriptPubKey hex:
6a bf 01 01 11 00 50 c3 00 00 00 00 00 00 00 2f 50 65 00 00 00 00

Byte-by-byte parsing:
[0]  0x6a = OP_RETURN    ← "This output is unspendable"
[1]  0xbf = OP_ORACLE    ← "This is oracle data!" ✓
[2]  0x01 = PUSH 1       ← "Next 1 byte is data"
[3]  0x01 = Version 1    ← "Phase One format"
[4]  0x11 = PUSH 17      ← "Next 17 bytes are data"
[5]  0x00 = Oracle ID 0  ← "Oracle #0"
[6-13]    = Price        ← "50000 cents = $500.00/DGB"
[14-21]   = Timestamp    ← "Unix time when price was set"
```

### Byte-by-Byte Structure Breakdown

```
┌────────────────────────────────────────────────────────────────────┐
│  OP_ORACLE OUTPUT: Complete 22-Byte Structure                     │
└────────────────────────────────────────────────────────────────────┘

VISUAL BREAKDOWN
═══════════════════════════════════════════════════════════════════

Byte Position:  0    1    2    3    4    5    6-13        14-21
               ┌────┬────┬────┬────┬────┬────┬──────────┬──────────┐
Hex Value:     │ 6a │ bf │ 01 │ 01 │ 11 │ 00 │ 50c30... │ 002f50...│
               └────┴────┴────┴────┴────┴────┴──────────┴──────────┘
                 │    │    │    │    │    │       │          │
                 │    │    │    │    │    │       │          │
Names:     OP_RETURN │    │    │    │    │    Price    Timestamp
              OP_ORACLE   │    │    │    │   (8 bytes)  (8 bytes)
                     PUSH 1    │    │ Oracle
                          Version  PUSH 17  ID


DETAILED FIELD DESCRIPTIONS
═══════════════════════════════════════════════════════════════════

Position  Length  Type    Name         Value       Description
─────────────────────────────────────────────────────────────────
0         1       opcode  OP_RETURN    0x6a        Output is unspendable
1         1       opcode  OP_ORACLE    0xbf        Oracle data marker
2         1       opcode  PUSHDATA     0x01        Push 1 byte (version)
3         1       uint8   Version      0x01        Phase One format
4         1       opcode  PUSHDATA     0x11 (17)   Push 17 bytes (data)
5         1       uint8   Oracle ID    0x00        Oracle 0 (Phase One)
6-13      8       uint64  Price        <LE bytes>  DigiDollar cents
14-21     8       int64   Timestamp    <LE bytes>  Unix timestamp


FIELD-BY-FIELD EXPLANATION
═══════════════════════════════════════════════════════════════════

┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ BYTE 0: OP_RETURN (0x6a)                                    ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Purpose: Marks output as "provably unspendable"            ┃
┃ Effect:  This output cannot be used as an input (no UTXO)  ┃
┃ Why:     Data-only output, not meant to hold value         ┃
┃ Size:    Does NOT count toward UTXO set (prunable)         ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ BYTE 1: OP_ORACLE (0xbf)                                    ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Purpose: Identifies this as oracle price data              ┃
┃ Why:     Distinguishes from other OP_RETURN data           ┃
┃ Usage:   Nodes check: if (script[1] == 0xbf) parse_oracle()┃
┃ Benefit: Fast detection without parsing entire OP_RETURN   ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ BYTES 2-3: Version Header (0x01 0x01)                       ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Byte 2:  PUSH 1 byte (0x01)                                ┃
┃ Byte 3:  Version number (0x01 = Phase One)                 ┃
┃ Purpose: Forward compatibility for format changes          ┃
┃ Future:  Phase Two might use 0x02 for multi-oracle format  ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ BYTES 4-5: Oracle ID (0x11 0x00)                            ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Byte 4:  PUSH 17 bytes (0x11)                              ┃
┃ Byte 5:  Oracle ID = 0 (always 0 in Phase One)             ┃
┃ Purpose: Identifies which oracle provided this price       ┃
┃ Phase 2: Will support IDs 0-14 (15 oracles total)          ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ BYTES 6-13: Price (8 bytes, little-endian uint64)           ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Format:  Micro-USD (1,000,000 = $1.00)                     ┃
┃ Example: 62 19 00 00 00 00 00 00 (LE) = 6,500 = $0.0065    ┃
┃ Range:   100 to 100,000,000 ($0.0001 to $100.00 per DGB)   ┃
┃ Endian:  Little-endian (LSB first, Intel/AMD byte order)   ┃
┃ Type:    uint64_t (unsigned 64-bit integer)                ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ BYTES 14-21: Timestamp (8 bytes, little-endian int64)       ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Format:  Unix timestamp (seconds since Jan 1, 1970 UTC)    ┃
┃ Example: 00 2f 50 65 00 00 00 00 (LE) = 1,700,000,000      ┃
┃          = Nov 14, 2023 22:13:20 UTC                        ┃
┃ Range:   Must be within 1 hour of block time               ┃
┃ Type:    int64_t (signed 64-bit integer)                   ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛


TOTAL SIZE CALCULATION
═══════════════════════════════════════════════════════════════════
OP_RETURN:       1 byte
OP_ORACLE:       1 byte
PUSH (version):  1 byte
Version:         1 byte
PUSH (data):     1 byte
Oracle ID:       1 byte
Price:           8 bytes
Timestamp:       8 bytes
─────────────────────────
TOTAL:          22 bytes ✓

Percentage of MAX_OP_RETURN_RELAY (83 bytes): 26.5% ✓
```

**Total: 22 bytes (2 marker opcodes + 2 push opcodes + 1 version byte + 17 data bytes)**

### What's Excluded (and Why It's Safe)

**Excluded fields**:
- ❌ **Schnorr signature** (64 bytes saved)
- ❌ **Oracle public key** (32 bytes saved)
- ❌ **Block height** (4 bytes saved)
- ❌ **Nonce** (8 bytes saved)

**Total savings: 107 bytes (83.6%)**

**"Wait - no signature? How do we trust it?"**

**The trust model**:

1. **P2P validation**: When the oracle broadcasts the **full 128-byte message** over P2P, every node verifies the Schnorr signature before relaying it. Invalid signatures are rejected and the peer gets penalized.

2. **Chainparams authorization**: The oracle's public key is **hardcoded** in `chainparams.cpp`:
   ```cpp
   consensus.oracle_nodes.push_back({
       0,  // oracle_id
       CPubKey(...),  // Authorized public key
       true  // is_active
   });
   ```

3. **Miner inclusion**: Miners only include oracle messages they've already validated via P2P. They won't include fake data because it would cause their block to be rejected.

4. **Block validation**: Nodes check that oracle_id=0 matches the authorized oracle in chainparams. Data from unauthorized oracles is rejected.

**This works for Phase One because**:
- ✅ Single oracle (no consensus needed)
- ✅ Testnet only (lower security requirements)
- ✅ Full signature verified before block inclusion

**Phase Two will include signatures** for multi-oracle consensus with on-chain verification.

### Example: Encoding and Decoding

**Input data**:
```
Oracle ID:  0
Price:      6500 micro-USD ($0.0065/DGB)
Timestamp:  1,700,000,000 (Unix timestamp)
```

**Encoding process**:

```cpp
CScript script;
script << OP_RETURN;           // Byte 0: 0x6a
script << OP_ORACLE;           // Byte 1: 0xbf
script << std::vector<uchar>{  // Bytes 2+ (17 bytes of data):
    0x01,                      // Version
    0x00,                      // Oracle ID
    0x62, 0x19, 0x00, 0x00,   // Price (6500 micro-USD, little-endian)
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x2f, 0x50, 0x65,   // Timestamp (little-endian)
    0x00, 0x00, 0x00, 0x00
};
```

**Resulting hex bytes**:
```
6a bf 11 01 00 05 00 00 00 00 00 00 00 00 2f 50 65 00 00 00 00 00
```

**Decoding process**:

```cpp
// Check bytes 0-1
if (scriptPubKey[0] == OP_RETURN && scriptPubKey[1] == OP_ORACLE) {

    // Extract data (starting at byte 3, after PUSHDATA)
    uint8_t version = scriptPubKey[3];      // 0x01
    uint8_t oracle_id = scriptPubKey[4];    // 0x00

    // Price (bytes 5-12, little-endian)
    uint64_t price = 0;
    for (int i = 0; i < 8; ++i) {
        price |= (uint64_t(scriptPubKey[5 + i]) << (i * 8));
    }
    // Result: 6500 micro-USD

    // Timestamp (bytes 13-20, little-endian)
    int64_t timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        timestamp |= (int64_t(scriptPubKey[13 + i]) << (i * 8));
    }
    // Result: 1,700,000,000
}
```

**Verification**:
```
Price bytes (little-endian): 62 19 00 00 00 00 00 00
  = 0x62 + (0x19 << 8) = 98 + 6400 = 6500 micro-USD ✓

Timestamp bytes (little-endian): 00 2f 50 65 00 00 00 00
  = 0x00 + (0x2f << 8) + (0x50 << 16) + (0x65 << 24)
  = 0 + 12,032 + 5,242,880 + 1,694,498,816
  = 1,700,000,000 ✓
```

---

## Block Validation Integration

### Where Validation Happens

Oracle validation is integrated into the **block validation pipeline**:

```
New Block Received
    ↓
CheckBlock()  ← Basic structure validation
    ├─ PoW valid?
    ├─ Merkle root valid?
    ├─ Transactions valid?
    └─ [ORACLE VALIDATION] ← Happens here
         ↓
ContextualCheckBlock()  ← Context-dependent validation
    ├─ Difficulty target correct?
    ├─ Timestamp acceptable?
    └─ Oracle timestamp fresh?
         ↓
ConnectBlock()  ← Add to active chain
    ├─ Execute transactions
    ├─ Update UTXO set
    └─ [CACHE ORACLE PRICE] ← Update price cache
         ↓
Block Accepted
```

### CheckBlock() Oracle Validation

**Location**: `validation.cpp` line 4127

**Validation steps** (in order):

#### 1. Network Filter
```cpp
if (network != TESTNET && network != REGTEST) {
    return true;  // Skip validation on mainnet
}
```

**Purpose**: Ensure oracle is only active on testnet/regtest (safety guard).

#### 2. Activation Height Check
```cpp
if (block_height < 1,000,000) {
    return true;  // Not active yet
}
```

**Purpose**: Oracle doesn't activate until block 1,000,000 on testnet.

#### 3. Extract Oracle Bundle
```cpp
// Look for OP_RETURN OP_ORACLE in coinbase vout[1]
if (coinbase.vout[1].scriptPubKey[0] == OP_RETURN &&
    coinbase.vout[1].scriptPubKey[1] == OP_ORACLE) {

    // Extract compact data
    ExtractOracleBundle(coinbase, bundle);
}
```

**Purpose**: Find and parse the 22-byte compact oracle data.

#### 4. Validate Bundle Structure
```cpp
if (!bundle.IsValid()) {
    return state.Invalid("bad-oracle-bundle");
}
```

**IsValid() checks**:
- ✅ Price in valid range (100-100M micro-USD)
- ✅ Timestamp not in future (+60 second tolerance)
- ✅ Timestamp not too old (≤1 hour)

#### 5. Phase One Consensus (Exactly 1 Message)
```cpp
if (bundle.messages.size() != 1) {
    return state.Invalid("bad-oracle-consensus");
}
```

**Purpose**: Phase One requires exactly 1 oracle. Reject if multiple.

#### 6. Verify Median Price
```cpp
if (bundle.median_price != bundle.messages[0].price) {
    return state.Invalid("bad-oracle-median");
}
```

**Purpose**: For 1 message, median must equal the message price (trivial check in Phase One).

#### 7. Timestamp Age Check
```cpp
int64_t age = block.nTime - message.timestamp;
if (age > 3600) {  // 1 hour max
    return state.Invalid("bad-oracle-timestamp");
}
```

**Purpose**: Reject stale oracle data (older than 1 hour).

#### 8. Future Timestamp Rejection
```cpp
if (message.timestamp > block.nTime + 60) {
    return state.Invalid("bad-oracle-timestamp");
}
```

**Purpose**: Prevent timestamp manipulation (60 second tolerance for clock skew).

#### 9. Oracle Authorization Check
```cpp
const OracleNodeInfo* info = chainparams.GetOracleNode(oracle_id);
if (!info || !info->is_active) {
    return state.Invalid("bad-oracle-unauthorized");
}
```

**Purpose**: Verify oracle_id=0 is in the authorized oracle list and active.

**If any check fails**: Block is **rejected entirely** (miner loses block reward).

**If all checks pass**: Block is accepted, oracle price is cached.

### ConnectBlock() Price Cache Update

**Location**: `validation.cpp` line 2826

**What happens**:

```cpp
// Extract oracle bundle from coinbase
COracleBundle bundle;
if (ExtractOracleBundle(coinbase_tx, bundle)) {

    // Update price cache
    OracleBundleManager::GetInstance()
        .UpdatePriceCache(block_height, bundle.median_price);

    LogPrint("Oracle: Updated price cache at height %d: %llu cents\n",
             block_height, bundle.median_price);
}
```

**Price cache structure**:
```cpp
std::map<int, uint64_t> height_to_price;

// Example:
height_to_price[697] = 5;  // Block 697: 5 cents
height_to_price[698] = 5;  // Block 698: 5 cents
height_to_price[699] = 5;  // Block 699: 5 cents
height_to_price[700] = 5;  // Block 700: 5 cents (current)
```

**Cache management**:
- ✅ **Thread-safe**: Mutex-protected for multi-threaded access
- ✅ **Limited size**: Keeps last 1,000 blocks (auto-evicts oldest)
- ✅ **Fast lookup**: O(1) average case (hash map)

**Why cache?**

DigiDollar needs historical prices for:
- ✅ **Current minting**: Get latest price for new DigiDollars
- ✅ **Historical validation**: Verify old transactions used correct price
- ✅ **Reorganization handling**: Restore correct price during chain reorgs

### DisconnectBlock() (Reorganizations)

**What is a reorganization?**

Sometimes two miners find blocks at the same height. The blockchain temporarily "forks" until one chain becomes longer. Nodes then **reorganize** to the longer chain, disconnecting the shorter chain's blocks.

**Oracle cache handling**:

```cpp
// When disconnecting block 700
DisconnectBlock(block_700) {
    // Remove oracle price from cache
    OracleBundleManager::GetInstance()
        .RemovePriceCache(700);

    // RegTest: Reset mock oracle to previous price
    if (is_regtest) {
        uint64_t prev_price = GetOraclePriceForHeight(699);
        MockOracleManager::GetInstance().SetMockPrice(prev_price);
    }
}
```

**Why this matters**:

Without cache cleanup during reorganizations, you could have:
- ❌ Orphaned prices (from abandoned chain)
- ❌ Incorrect DigiDollar valuations
- ❌ Cache bloat (duplicate heights)

**With proper cleanup**:
- ✅ Cache always matches active chain
- ✅ DigiDollar uses correct prices
- ✅ Reorganizations are transparent to users

---

## Network Propagation via P2P

### P2P Message Flow

**The journey of an oracle price message**:

```
Oracle Node (oracle.digibyte.io)
    │
    │ 1. Fetch prices from 12 exchanges
    │ 2. Calculate median: 6500 micro-USD
    │ 3. Create COraclePriceMessage (128 bytes)
    │ 4. Sign with Schnorr signature
    │
    ├──► P2P: ORACLEPRICE message (128 bytes)
    │
    ▼
Connected Peers (8-10 nodes)
    │
    │ 1. Receive message
    │ 2. Verify Schnorr signature ✓
    │ 3. Validate structure ✓
    │ 4. Check timestamp ✓
    │ 5. Store in OracleBundleManager
    │
    ├──► Relay to their peers (except sender)
    │
    ▼
Network-Wide Propagation (1,000+ nodes)
    │
    │ Same validation at each hop
    │ Invalid messages rejected
    │ Duplicate messages ignored
    │
    ▼
95% of network has message within 2-5 seconds
```

### Message Format (P2P Layer)

**Full format** (used for P2P transmission):

```
Bitcoin P2P Header (24 bytes):
  Magic:         0xDAB5BFFA (DigiByte mainnet magic)
  Command:       "oracleprice\0\0\0" (12 bytes)
  Payload Size:  128 (4 bytes)
  Checksum:      <4 bytes>

COraclePriceMessage Payload (128 bytes):
  oracle_id:        0 (4 bytes)
  price:            5 cents (8 bytes)
  timestamp:        1732204800 (8 bytes)
  block_height:     700 (4 bytes)
  nonce:            0x123... (8 bytes)
  oracle_pubkey:    <32 bytes>
  schnorr_sig:      <64 bytes>

Total: 152 bytes (24 header + 128 payload)
```

### Validation at Each Node

**When a node receives ORACLEPRICE message**:

```cpp
// net_processing.cpp, ProcessMessage()

if (msg_type == NetMsgType::ORACLEPRICE) {

    // Step 1: Deserialize
    COraclePriceMessage msg;
    vRecv >> msg;

    // Step 2: Validate structure
    if (!msg.IsValid()) {
        Misbehavior(peer, 10, "invalid oracle message");
        return;
    }

    // Step 3: Verify Schnorr signature
    if (!msg.Verify()) {
        Misbehavior(peer, 100, "invalid oracle signature");
        return;
    }

    // Step 4: Check oracle_id (Phase One: must be 0)
    if (msg.oracle_id != 0) {
        Misbehavior(peer, 10, "invalid oracle_id");
        return;
    }

    // Step 5: Check timestamp freshness
    int64_t age = GetTime() - msg.timestamp;
    if (age > 300 || age < -60) {  // 5 min old, or 1 min future
        return;  // Silently ignore stale/future messages
    }

    // Step 6: Rate limiting
    if (peer.oracle_message_count > 180) {  // per hour (3/min)
        Misbehavior(peer, 1, "oracle message spam");
        return;
    }
    ++peer.oracle_message_count;

    // Step 7: Duplicate detection
    uint256 msg_hash = msg.GetHash();
    if (seen_oracle_messages.count(msg_hash)) {
        return;  // Already have this message
    }
    seen_oracle_messages.insert(msg_hash);

    // Step 8: Store locally
    OracleBundleManager::GetInstance().AddOracleMessage(msg);

    // Step 9: Relay to other peers (except sender)
    RelayOracleMessage(msg, peer_id);
}
```

### Rate Limiting & Anti-Spam

**Per-peer rate limits**:
- ✅ **180 messages per hour** (max 3 msg/minute)
- ✅ **Duplicate detection** (hash-based)
- ✅ **Timestamp validation** (reject old/future messages)

**Misbehavior penalties**:
```
Invalid structure:     +10 points
Invalid signature:    +100 points (serious offense)
Invalid oracle_id:     +10 points
Message spam:          +1 point per excess message

At 100 points: Peer is disconnected and banned
```

**Why strict rate limiting?**

Without it, an attacker could:
- ❌ **Flood the network** with fake oracle messages
- ❌ **DoS attack** nodes by forcing expensive signature verification
- ❌ **Waste bandwidth** with junk data

**With rate limiting**:
- ✅ Legitimate oracle sends ~180 messages/hour (at the limit with 3/min)
- ✅ Attackers get banned quickly
- ✅ Network stays healthy

### Propagation Performance

**Typical timeline**:

```
T+0s:     Oracle broadcasts to 8 connected peers
T+0.5s:   8 peers validate and relay to ~64 peers (8×8)
T+1s:     64 peers relay to ~512 peers
T+2s:     512 peers relay to ~4,096 peers
T+5s:     95% of network has the message

Network size: ~1,000-5,000 nodes (testnet estimate)
```

**Factors affecting speed**:
- ✅ **Network topology**: Well-connected nodes propagate faster
- ⚠️ **Signature verification**: CPU-intensive (adds ~10ms per node)
- ⚠️ **Network latency**: Geographic distance between nodes
- ⚠️ **Bandwidth**: Slow connections delay propagation

**Why fast propagation matters**:

If miners don't receive oracle messages quickly:
- ❌ Blocks might be mined without oracle data (during transition)
- ❌ Network could have inconsistent price views (briefly)
- ❌ DigiDollar minting could use stale prices

**With 2-5 second propagation**:
- ✅ Miners get fresh prices before next block (15 second target)
- ✅ Network consensus on current price
- ✅ DigiDollar always uses recent data

---

## Current Limitations & Phase Two Plans

### Phase One Limitations (Testnet Only)

#### 1. Single Point of Failure
**Problem**: If the one oracle goes offline or gets hacked, the entire system breaks.

**Impact**:
- ❌ No price updates → DigiDollar can't be minted
- ❌ Hacked oracle → Could report false prices
- ❌ Network partition → Some nodes might not receive updates

**Acceptable for Phase One** because:
- ✅ Testnet only (no real money at risk)
- ✅ Testing the concept, not the production deployment
- ✅ Can be restarted/reset if issues occur

#### 2. No On-Chain Signature Verification
**Problem**: Compact format doesn't include the Schnorr signature, relying on chainparams trust.

**Impact**:
- ❌ Can't cryptographically prove data came from authorized oracle
- ❌ If chainparams is compromised (unlikely), could authorize fake oracle
- ❌ Less transparent (can't verify signature in block explorers)

**Mitigated by**:
- ✅ P2P layer verifies full signature before relay
- ✅ Chainparams is source code (requires recompile to change)
- ✅ Testnet only (lower security bar)

#### 3. No Economic Incentives
**Problem**: Oracle operator isn't rewarded or punished for accuracy.

**Impact**:
- ❌ No incentive to stay online 24/7
- ❌ No penalty for reporting wrong prices (accidentally)
- ❌ Relies on trust, not game theory

**Acceptable for Phase One** because:
- ✅ Operated by DigiByte Foundation (mission-aligned)
- ✅ Reputation matters (poor performance damages credibility)
- ✅ Phase Two will add staking/slashing

#### 4. Centralized Control
**Problem**: DigiByte Foundation controls the single oracle.

**Impact**:
- ❌ Could theoretically manipulate prices (though highly unlikely)
- ❌ Regulatory risk (single entity could be targeted)
- ❌ Not truly decentralized

**Acceptable for Phase One** because:
- ✅ Testnet environment (proof-of-concept)
- ✅ Transition plan to Phase Two multi-oracle system
- ✅ Community can monitor oracle behavior

### Phase Two Future Plans

Phase Two will implement a **fully decentralized multi-oracle system** for mainnet:

#### 1. 15 Independent Oracles (Geographic Diversity)
**Design**:
- 15 oracle operators around the world
- Different legal jurisdictions
- Different infrastructure providers
- 8-of-15 consensus (majority agreement required)

**Benefits**:
- ✅ **No single point of failure**: Up to 7 oracles can be offline
- ✅ **Byzantine fault tolerance**: Can tolerate 7 malicious/faulty oracles
- ✅ **Geographic resilience**: Natural disasters can't take down system
- ✅ **Censorship resistance**: Hard to target all 15 jurisdictions

#### 2. On-Chain Signature Verification
**Design**:
- All 8 consensus signatures stored in blocks
- BIP-340 Schnorr signature aggregation
- Merkleized format for space efficiency

**Benefits**:
- ✅ **Cryptographic proof**: Can verify every price came from authorized oracles
- ✅ **Transparent**: Block explorers show which oracles agreed
- ✅ **Auditable**: Historical prices can be independently verified

**Storage impact**:
```
Phase One: 21 bytes per block
Phase Two: ~150 bytes per block (8 signatures + metadata)

Note: Phase Two will exceed 83-byte OP_RETURN limit - will require
protocol upgrade or alternative storage (e.g., witness data).
```

#### 3. Economic Incentives (Staking & Slashing)
**Design**:
- Oracle operators stake 1,000,000 DGB (collateral)
- Earn fees for accurate price reporting
- Lose stake (slashing) for bad behavior

**Slashing conditions**:
- ❌ Reporting price >5% from consensus median
- ❌ Downtime >10% in 30-day window
- ❌ Invalid signatures

**Benefits**:
- ✅ **Skin in the game**: Operators lose money for bad performance
- ✅ **Self-enforcing**: No human judgment needed (automated slashing)
- ✅ **Attracts quality operators**: High-reputation oracles can earn fees

#### 4. Reputation System
**Design**:
- Track accuracy over time (consensus agreement %)
- Public scoreboard (on-chain or off-chain)
- Weight oracle votes by reputation (optional enhancement)

**Metrics tracked**:
- ✅ Uptime percentage (last 90 days)
- ✅ Consensus agreement rate (% of times in 8-of-15)
- ✅ Median deviation (how close to final price)

**Benefits**:
- ✅ **Transparency**: Users can see which oracles are reliable
- ✅ **Competition**: Oracles compete for reputation
- ✅ **Continuous improvement**: Poor performers naturally get replaced

#### 5. Larger On-Chain Format
**Design**:
- Include all 8 consensus signatures
- Merkleized format: Store root hash + signatures
- Estimated: ~150 bytes per block (up from 22)

**Example structure**:
```
OP_RETURN OP_ORACLE <version> <merkle_root> <signatures>

merkle_root: 32 bytes (covers all 15 oracle messages)
signatures: 8 × 64 bytes = 512 bytes (Schnorr signatures)

Optimization: Use Schnorr multi-signature aggregation
Final size: ~150 bytes (merkle_root + aggregated_sig + metadata)
```

**Still fits comfortably in 83-byte OP_RETURN**? No - will require protocol upgrade to allow larger OP_RETURN for oracle data, or use alternative storage (e.g., witness data).

### Phase Two Timeline

**Current Status**: Phase One testnet deployment (2025 Q1 target)

**Phase Two Milestones**:
1. **Research & Design** (Q2 2025) - Finalize multi-oracle architecture
2. **Implementation** (Q3 2025) - Code 15-oracle consensus
3. **Testnet Testing** (Q4 2025) - Deploy to testnet, identify issues
4. **Mainnet Deployment** (2026 Q1) - Launch on mainnet if successful

**Dependencies**:
- ✅ Phase One success on testnet
- ✅ Community approval via governance
- ✅ Security audits (external review)
- ✅ Oracle operator recruitment (15 diverse operators)

---

## Summary: Oracle System at a Glance

### What It Does
- ✅ **Fetches** real DGB/USD prices from 12 exchanges every 15 seconds (external daemon)
- ✅ **Calculates** median price with outlier filtering (MAD algorithm)
- ✅ **Broadcasts** signed messages via P2P network (2-5 second propagation)
- ✅ **Stores** compact 21-byte format in every block (25.3% of OP_RETURN limit)
- ✅ **Validates** all oracle data during block verification (CheckBlock)
- ✅ **Caches** last 1,000 prices for DigiDollar collateral calculations

### How It Works
1. **External oracle daemon** fetches prices → median → sign → broadcast
2. **P2P network** (DigiByte Core nodes) validates signature → relays to all nodes
3. **Miners** include compact 21-byte data in coinbase transaction
4. **Validators** verify oracle data → reject invalid blocks
5. **Price cache** stores height → price mapping
6. **DigiDollar** queries cache for current DGB/USD rate

### Network Modes
- **RegTest**: Mock oracle (fake prices for testing)
- **Testnet**: Real oracle, real prices, 1-of-1 consensus
- **Mainnet**: Disabled (Phase Two required)

### Current Limitations
- ⚠️ Single oracle (1-of-1 consensus)
- ⚠️ No on-chain signatures (compact format)
- ⚠️ No economic incentives (trust-based)
- ⚠️ Testnet only (not production-ready)

### Phase Two Improvements
- ✅ 15 independent oracles
- ✅ 8-of-15 consensus (majority agreement)
- ✅ On-chain signature verification
- ✅ Economic incentives (staking, slashing, fees)
- ✅ Reputation tracking
- ✅ Mainnet deployment

---

## Why This Matters for Testnet Rollout

### Planning Testnet Deployment

**Key considerations**:

1. **Oracle Availability**
   - Need reliable oracle.digibyte.io server (99.9% uptime target)
   - Monitoring/alerting for oracle downtime
   - Backup plans if oracle fails

2. **Price Data Quality**
   - Verify 5+ exchanges working consistently
   - Monitor for price outliers
   - Test MAD filtering with real market data

3. **Network Stability**
   - Ensure P2P propagation works across testnet
   - Monitor rate limiting effectiveness
   - Track message relay performance

4. **Block Validation**
   - Verify activation height enforcement (block 1,000,000)
   - Test transition period (blocks without oracle data)
   - Confirm miners include oracle data correctly

5. **DigiDollar Integration**
   - Test minting with real oracle prices
   - Verify collateral calculations (200% requirement)
   - Ensure price cache stays synchronized

### Testing Checklist

Before testnet launch:

- [ ] Oracle daemon runs continuously (24+ hours)
- [ ] All 12 exchanges return valid prices
- [ ] P2P messages propagate to 95% of network
- [ ] Miners include oracle data in coinbase
- [ ] Block validation accepts valid data, rejects invalid
- [ ] Price cache updates correctly on ConnectBlock
- [ ] Price cache reverts correctly on DisconnectBlock (reorgs)
- [ ] DigiDollar minting uses real prices (not mock)
- [ ] All critical unit tests passing (100+ tests)
- [ ] Functional tests passing (digidollar_oracle.py)

### Rollout Strategy

**Recommended phased approach**:

**Phase 1A: Internal Testing** (1-2 weeks)
- Deploy oracle on private testnet
- Test with 3-5 nodes
- Verify basic functionality

**Phase 1B: Public Testnet Soft Launch** (2-4 weeks)
- Activate oracle at block 1,000,000
- Monitor for issues
- Gather community feedback

**Phase 1C: Full Testnet Deployment** (ongoing)
- Oracle running 24/7
- DigiDollar minting enabled
- Community stress testing

**Phase 2: Mainnet Preparation** (6-12 months)
- Design multi-oracle system
- Recruit oracle operators
- Security audits
- Community governance approval

---

## Quick Reference: Current vs Planned Implementation

### **Implementation Status Matrix**

| **Component** | **RegTest (Now)** | **Testnet (Phase One)** | **Mainnet (Phase Two)** |
|--------------|-------------------|-------------------------|-------------------------|
| **Oracle Daemon** | Mock (built-in) | External daemon | 15 external daemons |
| **Price Source** | Manual (`setmockoracleprice`) | 10 real exchanges | 10 real exchanges |
| **Consensus Model** | N/A (mock) | 1-of-1 (single oracle) | 8-of-15 (majority) |
| **P2P Validation** | ✅ Implemented | ✅ Implemented | ✅ Implemented |
| **Block Validation** | ✅ Implemented | ✅ Implemented | ✅ Implemented |
| **Schnorr Signatures** | ❌ Not used (mock) | ✅ P2P only (not in blocks) | ✅ On-chain (8 signatures) |
| **Compact Format** | ✅ 21 bytes | ✅ 21 bytes | ~150 bytes (with sigs) |
| **Price Cache** | ✅ Implemented | ✅ Implemented | ✅ Implemented |
| **Activation Height** | Block 1 | Block 1,000,000 | TBD (governance) |
| **Economic Incentives** | ❌ None | ❌ None (trust-based) | ✅ Staking/slashing |
| **Reputation System** | ❌ None | ❌ None | ✅ On-chain metrics |
| **Status** | ✅ **Working now** | 🚧 **Ready (needs daemon)** | 📋 **Planned (2026)** |

### **What Works Today (as of November 2025)**

#### ✅ **DigiByte Core (v8.26) Includes:**
1. **P2P Message Handling**
   - Receive `ORACLEPRICE` messages via P2P
   - Validate BIP-340 Schnorr signatures
   - Relay valid messages to peers
   - Rate limiting (180 msg/hour per peer)
   - Anti-spam misbehavior penalties

2. **Block Validation**
   - Parse compact 21-byte oracle format
   - Validate price range (100-100M micro-USD)
   - Validate timestamp freshness
   - Check oracle authorization (chainparams)
   - Phase One consensus (exactly 1 oracle)
   - Reject invalid blocks

3. **Price Cache System**
   - Store height → price mapping
   - Keep last 1,000 blocks
   - Thread-safe access
   - Reorganization handling

4. **RegTest Testing**
   - Mock oracle manager
   - `setmockoracleprice` RPC command
   - Functional test: `test/functional/digidollar_oracle.py`
   - Unit tests: `src/test/oracle_tests.cpp`

#### ❌ **What's NOT in DigiByte Core:**
1. **Oracle Daemon** (separate software)
   - Exchange API integration
   - Median calculation with MAD filtering
   - Message creation and signing
   - P2P broadcasting logic

2. **Direct Exchange Fetching**
   - No Binance/Coinbase/etc. API calls in Core
   - No price aggregation in Core
   - No median calculation in Core (only validation)

### **Deployment Checklist**

#### **RegTest (Development) - ✅ READY**
- [x] Mock oracle implemented
- [x] RPC commands working
- [x] Functional tests passing
- [x] Unit tests passing
- [x] Can test DigiDollar minting locally

#### **Testnet (Phase One) - 🚧 NEEDS ORACLE DAEMON**
- [x] DigiByte Core code complete
- [x] P2P validation working
- [x] Block validation working
- [ ] **External oracle daemon developed** ← MISSING
- [ ] Oracle daemon deployed to `oracle.digibyte.io`
- [ ] 10 exchange APIs configured
- [ ] Continuous operation (24/7)
- [ ] Monitoring/alerting setup

#### **Mainnet (Phase Two) - 📋 PLANNED (2026)**
- [ ] Multi-oracle architecture designed
- [ ] 15 oracle operators recruited
- [ ] Economic incentive system (staking/slashing)
- [ ] Reputation tracking system
- [ ] On-chain signature aggregation
- [ ] Security audits completed
- [ ] Community governance approval
- [ ] Protocol upgrade for larger oracle data

### **Key Takeaway**

**DigiByte Core is ready** to validate and use oracle data. The **missing piece** is the **external oracle daemon** that fetches prices and broadcasts them to the network.

**For RegTest**: Use the built-in mock oracle (`setmockoracleprice`).

**For Testnet**: Requires developing and deploying the external oracle daemon.

**For Mainnet**: Requires full multi-oracle system (Phase Two).

---

**The oracle system is the foundation that makes DigiDollar possible - providing the critical link between blockchain consensus and real-world price data.**
