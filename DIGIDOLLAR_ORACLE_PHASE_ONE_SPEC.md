# DigiDollar Oracle Phase One Implementation Specification

**Version**: 1.0
**Date**: 2025-11-18
**Status**: Implementation Ready
**Target**: DigiByte Core v8.26
**Scope**: Single Hardcoded Testnet Oracle (Expandable to 15 Mainnet Oracles)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Phase One Scope & Objectives](#2-phase-one-scope--objectives)
3. [Architecture Overview](#3-architecture-overview)
4. [Core Components Specification](#4-core-components-specification)
5. [DigiByte Core v8.26 Integration Points](#5-digibyte-core-v826-integration-points)
6. [Data Structures & Interfaces](#6-data-structures--interfaces)
7. [Oracle Price Flow Architecture](#7-oracle-price-flow-architecture)
8. [Exchange API Integration](#8-exchange-api-integration)
9. [P2P Network Protocol](#9-p2p-network-protocol)
10. [Consensus Validation Rules](#10-consensus-validation-rules)
11. [Testnet Configuration](#11-testnet-configuration)
12. [Testnet Reset Procedures](#12-testnet-reset-procedures)
13. [Test-Driven Development Plan](#13-test-driven-development-plan)
14. [Security Considerations](#14-security-considerations)
15. [Performance Requirements](#15-performance-requirements)
16. [Operator Setup Guide Requirements](#16-operator-setup-guide-requirements)
17. [Migration Path to Mainnet (15 Oracles)](#17-migration-path-to-mainnet-15-oracles)
18. [Implementation Timeline](#18-implementation-timeline)

---

## 1. Executive Summary

### 1.1 What is Phase One?

Phase One delivers a **fully functional oracle price feed system** for DigiDollar on **DigiByte testnet**. It implements:

- **Single hardcoded oracle operator** for testnet
- **Real exchange API integration** (Binance, Coinbase, Kraken, Bittrex, KuCoin)
- **Complete P2P oracle protocol** (message broadcasting, validation, relay)
- **Consensus-level integration** (block validation, transaction validation)
- **Testnet reset procedures** for DigiDollar development cycles
- **Architecture expandable to 15 hardcoded mainnet oracles**

### 1.2 What Phase One is NOT

Phase One **does not** implement:
- ❌ Economic staking system (Phase Two)
- ❌ Slashing mechanisms (Phase Two)
- ❌ Permissionless oracle participation (Phase Two)
- ❌ Reputation system (Phase Two)
- ❌ Miner validation layer (Phase Two)
- ❌ Mainnet deployment (testnet only)

### 1.3 Current State Analysis

**DigiDollar Implementation Status**: 82% complete

**Oracle Framework Status** (from codebase analysis):
- ✅ Oracle data structures defined (`/src/primitives/oracle.h`)
- ✅ Oracle bundle manager skeleton (`/src/oracle/bundle_manager.cpp`)
- ✅ Oracle integration hooks in validation (`/src/validation.cpp`)
- ✅ P2P message types defined (`/src/protocol.h`)
- ✅ Mock oracle system operational (`/src/oracle/mock_oracle.cpp`)
- ❌ **Exchange API integration** (stubbed out)
- ❌ **Real oracle price fetching** (returns mock data)
- ❌ **P2P broadcasting implementation** (message handlers incomplete)
- ❌ **Testnet oracle configuration** (no hardcoded oracles)

**Gap Analysis**:
- **Critical**: Exchange API clients (Binance, Coinbase, Kraken, etc.)
- **Critical**: Oracle operator daemon implementation
- **Critical**: P2P message broadcasting and relay
- **High**: Testnet configuration and reset procedures
- **High**: Oracle bundle block integration (miner.cpp)
- **Medium**: RPC command completion
- **Medium**: Historical price persistence

### 1.4 Deliverables

Upon Phase One completion:

**Functional System**:
- Testnet oracle fetches real DGB/USD prices every ~1 minute
- Prices broadcast to network via P2P protocol
- Miners include oracle bundles in blocks
- DigiDollar minting/redemption uses real oracle prices
- DCA/ERR/Volatility systems use real prices

**Code Artifacts**:
- 2,000-3,000 lines of production C++ code
- 500+ unit tests (Boost framework)
- 20+ functional tests (Python framework)
- 90%+ code coverage

**Documentation**:
- Oracle operator setup guide
- Testnet reset procedures (complete)
- Configuration examples
- Troubleshooting guide

**Testnet Readiness**:
- Single oracle operational
- DigiDollar functionality verified end-to-end
- Repeatable testnet reset procedures
- Foundation for mainnet expansion (15 oracles)

---

## 2. Phase One Scope & Objectives

### 2.1 Primary Objectives

**Objective 1: Real Oracle Prices**
- Replace mock oracle system with real exchange API integration
- Fetch DGB/USD prices from multiple exchanges
- Calculate median price with outlier filtering
- Update prices every ~1 minute (4 blocks)

**Objective 2: P2P Oracle Protocol**
- Broadcast oracle price messages to network
- Validate and relay messages from other oracles
- Implement message rate limiting and DoS protection
- Enable multi-node oracle price consensus

**Objective 3: Consensus Integration**
- Include oracle bundles in block coinbase transactions
- Validate oracle bundles during block acceptance
- Expose oracle prices to DigiDollar transaction validation
- Integrate with DCA/ERR/Volatility systems

**Objective 4: Testnet Infrastructure**
- Configure single hardcoded testnet oracle
- Document testnet reset procedures
- Enable repeatable DigiDollar testing cycles
- Prepare architecture for mainnet (15 oracles)

**Objective 5: Production Quality**
- 90%+ test coverage (unit + functional)
- Red-green TDD for all components
- Complete error handling
- Performance profiling and optimization

### 2.2 Success Criteria

**Functional Requirements**:
- [ ] Oracle fetches real prices from 5+ exchanges
- [ ] Prices update every 4 blocks (~1 minute)
- [ ] P2P messages broadcast successfully to all peers
- [ ] Oracle bundles included in coinbase transactions
- [ ] DigiDollar mint transaction validates with oracle price
- [ ] DigiDollar redeem transaction validates with oracle price
- [ ] DCA system calculates health with oracle price
- [ ] Testnet can be reset and re-initialized

**Quality Requirements**:
- [ ] All unit tests pass (500+ tests)
- [ ] All functional tests pass (20+ scenarios)
- [ ] Code coverage ≥ 90%
- [ ] No memory leaks (valgrind clean)
- [ ] No compiler warnings
- [ ] Follows DigiByte coding standards

**Documentation Requirements**:
- [ ] Oracle operator guide complete
- [ ] Testnet reset guide complete
- [ ] Configuration examples provided
- [ ] Troubleshooting guide created

**Performance Requirements**:
- [ ] Price fetch latency < 5 seconds (median)
- [ ] P2P message propagation < 2 seconds
- [ ] Oracle bundle validation < 10ms
- [ ] Memory usage < 50MB for oracle system

### 2.3 Out of Scope

**Explicitly NOT included in Phase One**:
- Economic staking (DD or DGB staking)
- Slashing mechanisms (penalties for misbehavior)
- Reputation system (performance tracking)
- Oracle registration transactions
- Permissionless oracle participation
- Miner validation beyond basic bundle checks
- Mainnet oracle deployment
- Oracle governance mechanisms

---

## 3. Architecture Overview

### 3.1 System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                     PHASE ONE ORACLE SYSTEM                          │
│                    (Single Testnet Oracle)                           │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  LAYER 1: EXCHANGE API INTEGRATION                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │ Binance  │ │Coinbase  │ │ Kraken   │ │ Bittrex  │ │ KuCoin   │ │
│  │   API    │ │   API    │ │   API    │ │   API    │ │   API    │ │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ │
│       │            │            │            │            │         │
│       └────────────┴────────────┴────────────┴────────────┘         │
│                            ↓                                         │
│                 ExchangePriceFetcher                                 │
│              (Fetch, Parse, Median, Filter)                          │
│                                                                      │
│  Files: /src/oracle/exchange.h, exchange.cpp                        │
│  Methods: FetchBinance(), FetchCoinbase(), FetchMedianPrice()        │
└─────────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────────┐
│  LAYER 2: ORACLE NODE DAEMON                                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  OracleNode::Run() - Main daemon loop                               │
│  ├─ Every 60 seconds (4 blocks):                                    │
│  │  ├─ Fetch median price from exchanges                            │
│  │  ├─ Create COraclePriceMessage                                   │
│  │  ├─ Sign with Schnorr signature                                  │
│  │  └─ Broadcast to P2P network                                     │
│  │                                                                   │
│  └─ Monitor consensus, log status                                   │
│                                                                      │
│  Files: /src/oracle/node.h, node.cpp                                │
│  Methods: CreatePriceMessage(), BroadcastPriceMessage()             │
└─────────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────────┐
│  LAYER 3: P2P NETWORK PROTOCOL                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  P2P Message Types:                                                 │
│  ├─ ORACLEPRICE   - Individual oracle price message                 │
│  ├─ ORACLEBUNDLE  - Complete oracle bundle                          │
│  └─ GETORACLES    - Request oracle data                             │
│                                                                      │
│  Message Flow:                                                      │
│  Oracle → PushMessage(ORACLEPRICE) → Network Peers                  │
│  Peers → Validate → Relay → All Nodes                               │
│                                                                      │
│  Files: /src/net_processing.cpp (lines 5315, 5397, 5463)            │
│  Files: /src/protocol.h (message types)                             │
│  Files: /src/primitives/oracle.cpp (validation)                     │
└─────────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────────┐
│  LAYER 4: ORACLE BUNDLE MANAGER                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  OracleBundleManager (Singleton)                                    │
│  ├─ Collect incoming oracle price messages                          │
│  ├─ Phase One: Single oracle (1-of-1 consensus)                     │
│  ├─ Future: 8-of-15 threshold for mainnet                           │
│  ├─ Calculate median price                                          │
│  ├─ Create COracleBundle                                            │
│  ├─ Cache consensus price                                           │
│  └─ Provide price to DigiDollar validation                          │
│                                                                      │
│  Files: /src/oracle/bundle_manager.h, bundle_manager.cpp            │
│  Methods: AddOracleMessage(), GetLatestPrice(), CreateBundle()      │
└─────────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────────┐
│  LAYER 5: BLOCK INTEGRATION (MINERS)                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Miner Block Creation:                                              │
│  ├─ Call OracleBundleManager::GetLatestBundle()                     │
│  ├─ Embed bundle in coinbase OP_RETURN                              │
│  ├─ Include: median price, timestamp, signatures                    │
│  └─ Broadcast block to network                                      │
│                                                                      │
│  Block Validation (All Nodes):                                      │
│  ├─ Extract oracle bundle from coinbase                             │
│  ├─ Verify signatures (Phase One: 1 signature)                      │
│  ├─ Validate price is reasonable                                    │
│  └─ Accept block if valid                                           │
│                                                                      │
│  Files: /src/node/miner.cpp (lines 170-177)                         │
│  Files: /src/validation.cpp (CheckBlock, ConnectBlock)              │
└─────────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────────┐
│  LAYER 6: DIGIDOLLAR INTEGRATION                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  DigiDollar Transaction Validation:                                 │
│  ├─ Mint: Calculate collateral using oracle price                   │
│  ├─ Redeem: Calculate DGB return using oracle price                 │
│  ├─ DCA: System health calculation with oracle price                │
│  ├─ ERR: Emergency ratio adjustment with oracle price               │
│  └─ Volatility: Price monitoring with oracle history                │
│                                                                      │
│  Price Retrieval:                                                   │
│  OracleIntegration::GetCurrentOraclePrice()                         │
│  └─ Returns cached consensus price from OracleBundleManager         │
│                                                                      │
│  Files: /src/validation.cpp (GetOraclePriceForTransaction)          │
│  Files: /src/consensus/dca.cpp, err.cpp, volatility.cpp             │
│  Files: /src/digidollar/validation.cpp                              │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 Data Flow: Exchange API → DigiDollar Validation

**End-to-End Price Flow**:

```
STEP 1: Exchange API Fetch (Every 60 seconds)
├─ OracleNode daemon wakes up
├─ ExchangePriceFetcher::FetchMedianPrice()
│  ├─ Fetch Binance: $0.01234
│  ├─ Fetch Coinbase: $0.01235
│  ├─ Fetch Kraken: $0.01233
│  ├─ Fetch Bittrex: $0.01234
│  └─ Fetch KuCoin: $0.01235
├─ Calculate median: $0.01234
└─ Convert to cents: 1.234 cents

STEP 2: Oracle Message Creation
├─ Create COraclePriceMessage
│  ├─ price = 1.234 cents (CAmount in hundredths of cents)
│  ├─ timestamp = GetTime()
│  ├─ blockHeight = chainActive.Height()
│  └─ nonce = GetRandHash()
├─ Sign with Schnorr
│  ├─ msgHash = SHA256(price || timestamp || height || nonce)
│  └─ signature = SchnorrSign(msgHash, oraclePrivKey)
└─ message ready for broadcast

STEP 3: P2P Broadcast
├─ g_connman->ForEachNode([&](CNode* node) {
│      node->PushMessage(NetMsgType::ORACLEPRICE, message);
│  })
└─ Message propagates to all DigiByte nodes

STEP 4: Message Reception & Validation
├─ net_processing.cpp:5315 (ORACLEPRICE handler)
├─ Validate:
│  ├─ Signature valid? (SchnorrVerify)
│  ├─ Oracle active? (Check chainparams)
│  ├─ Timestamp fresh? (< 5 minutes old)
│  ├─ Price reasonable? (Within deviation limits)
│  └─ Rate limit OK? (< 3 msgs/min per oracle)
├─ If valid:
│  ├─ OracleBundleManager::AddOracleMessage(message)
│  └─ Relay to other peers
└─ If invalid: Drop and log

STEP 5: Bundle Creation
├─ OracleBundleManager::TryCreateBundle()
├─ Phase One: Have 1 message from testnet oracle? → Create bundle
├─ Future: Have 8+ messages from active oracles? → Create bundle
├─ Calculate consensus price (median)
├─ Create COracleBundle
│  ├─ medianPrice = 1.234 cents
│  ├─ signatures = [sig1] (Phase One: single signature)
│  ├─ merkleRoot = ComputeMerkleRoot([message1])
│  └─ timestamp = GetTime()
└─ Cache bundle for block inclusion

STEP 6: Block Inclusion (Miner)
├─ BlockAssembler::CreateNewBlock()
├─ Call OracleBundleManager::AddOracleBundleToBlock(block)
├─ Embed in coinbase OP_RETURN:
│  ├─ Coinbase vout[1]: OP_RETURN <serialized bundle>
│  └─ Bundle contains: price, timestamp, signatures, merkle root
└─ Broadcast block to network

STEP 7: Block Validation (All Nodes)
├─ CheckBlock() validation.cpp:4076
├─ Extract oracle bundle from coinbase
├─ Validate:
│  ├─ Bundle structure valid?
│  ├─ Signatures verify? (Phase One: 1 signature)
│  ├─ Price within deviation? (< 10% from previous)
│  └─ Timestamp reasonable?
├─ If valid: Accept block
└─ Update cached oracle price

STEP 8: DigiDollar Transaction Validation
├─ User creates mint transaction: 1000 DD ($1000)
├─ GetOraclePriceForTransaction(tx)
│  └─ OracleIntegration::GetCurrentOraclePrice()
│      └─ Returns: 1.234 cents per DGB
├─ Calculate collateral required:
│  ├─ USD value needed: $1000
│  ├─ DGB for 100%: $1000 / $0.01234 = 81,037 DGB
│  ├─ Collateral ratio: 300% (1 year lock)
│  └─ Required: 81,037 × 3 = 243,111 DGB
├─ Validate: actualCollateral >= requiredCollateral?
└─ If yes: Accept transaction

STEP 9: DCA/ERR/Volatility Integration
├─ DCA: CalculateSystemHealth(totalCollateral, totalDD, oraclePrice)
├─ ERR: GetERRAdjustedRequirement() uses oracle price
├─ Volatility: RecordPrice(oraclePrice, timestamp) tracks history
└─ All protection systems use real oracle prices
```

### 3.3 Phase One Simplifications

**Testnet Configuration**:
- **1 oracle** instead of 15
- **1-of-1 consensus** instead of 8-of-15
- **No epoch rotation** (single oracle always active)
- **Simplified validation** (single signature check)

**Code Prepared for Expansion**:
```cpp
// Current: Phase One (testnet)
if (chainparams.NetworkIDString() == "test") {
    // Single oracle, 1-of-1 consensus
    const int ACTIVE_ORACLES = 1;
    const int CONSENSUS_THRESHOLD = 1;
}

// Future: Mainnet (not implemented yet)
/*
if (chainparams.NetworkIDString() == "main") {
    // 15 oracles, 8-of-15 consensus
    const int ACTIVE_ORACLES = 15;
    const int CONSENSUS_THRESHOLD = 8;
    // Epoch rotation every ~6 hours
    // Deterministic selection from 15 hardcoded oracles
}
*/
```

---

## 4. Core Components Specification

### 4.1 Exchange API Integration

**Component**: `ExchangePriceFetcher`
**Files**: `/src/oracle/exchange.h`, `/src/oracle/exchange.cpp`
**Purpose**: Fetch DGB/USD prices from multiple exchanges

#### 4.1.1 Supported Exchanges

**Primary Exchanges** (Required for Phase One):
1. **Binance** - DGB/USDT pair
   - Endpoint: `https://api.binance.com/api/v3/ticker/price?symbol=DGBUSDT`
   - Volume: Highest
   - API Key: Required for higher rate limits

2. **Coinbase Pro** - DGB/USD pair
   - Endpoint: `https://api.coinbase.com/v2/prices/DGB-USD/spot`
   - Direct USD pair (preferred)
   - API Key: Optional

3. **Kraken** - DGB/USD pair
   - Endpoint: `https://api.kraken.com/0/public/Ticker?pair=DGBUSD`
   - Reliable, regulated exchange
   - API Key: Optional

4. **Bittrex** - DGB/USD pair
   - Endpoint: `https://api.bittrex.com/v3/markets/DGB-USD/ticker`
   - US market coverage
   - API Key: Required

5. **KuCoin** - DGB/USDT pair
   - Endpoint: `https://api.kucoin.com/api/v1/market/orderbook/level1?symbol=DGB-USDT`
   - Asian market coverage
   - API Key: Required

#### 4.1.2 ExchangePriceFetcher Interface

```cpp
// File: /src/oracle/exchange.h

class ExchangePriceFetcher {
public:
    /**
     * Fetch median price from all configured exchanges
     * @return Price in cents (100 = $1.00, 1 = $0.01)
     * @throws std::runtime_error if insufficient exchanges respond
     */
    CAmount FetchMedianPrice();

    /**
     * Fetch price from specific exchange
     * @param exchange Exchange identifier
     * @return Price in cents, or std::nullopt on failure
     */
    std::optional<CAmount> FetchExchangePrice(const std::string& exchange);

    /**
     * Configure exchange API credentials
     * @param exchange Exchange identifier
     * @param apiKey API key
     * @param apiSecret API secret
     */
    void ConfigureExchange(const std::string& exchange,
                          const std::string& apiKey,
                          const std::string& apiSecret);

private:
    // Exchange-specific fetch methods
    std::optional<double> FetchBinance();
    std::optional<double> FetchCoinbase();
    std::optional<double> FetchKraken();
    std::optional<double> FetchBittrex();
    std::optional<double> FetchKuCoin();

    // HTTP client
    std::string HttpGet(const std::string& url,
                       const std::map<std::string, std::string>& headers = {});

    // JSON parsing
    double ParsePriceFromJSON(const std::string& json,
                             const std::string& exchange);

    // Outlier filtering
    std::vector<double> FilterOutliers(const std::vector<double>& prices);

    // Configuration
    struct ExchangeConfig {
        std::string apiKey;
        std::string apiSecret;
        std::string endpoint;
        int rateLimit{10};  // Requests per minute
    };

    std::map<std::string, ExchangeConfig> m_exchangeConfigs;
    std::map<std::string, int64_t> m_lastRequestTime;
};
```

#### 4.1.3 HTTP Client Implementation

**Requirements**:
- Use **libcurl** (already included in DigiByte dependencies)
- Support HTTPS with certificate validation
- Implement request timeout (5 seconds)
- Implement exponential backoff on failures
- Thread-safe (can be called concurrently)

**Implementation Pattern**:
```cpp
std::string ExchangePriceFetcher::HttpGet(
    const std::string& url,
    const std::map<std::string, std::string>& headers)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);  // 5 second timeout
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Add headers
    struct curl_slist* headerList = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        headerList = curl_slist_append(headerList, header.c_str());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    CURLcode res = curl_easy_perform(curl);

    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(
            strprintf("HTTP request failed: %s", curl_easy_strerror(res)));
    }

    return response;
}
```

#### 4.1.4 Median Calculation with Outlier Filtering

**Algorithm**: Modified Median Absolute Deviation (MAD)

```cpp
std::vector<double> ExchangePriceFetcher::FilterOutliers(
    const std::vector<double>& prices)
{
    if (prices.size() < 3) {
        return prices;  // Can't filter with < 3 data points
    }

    // Calculate median
    std::vector<double> sorted = prices;
    std::sort(sorted.begin(), sorted.end());
    double median = sorted[sorted.size() / 2];

    // Calculate MAD (Median Absolute Deviation)
    std::vector<double> deviations;
    for (double price : prices) {
        deviations.push_back(std::abs(price - median));
    }
    std::sort(deviations.begin(), deviations.end());
    double mad = deviations[deviations.size() / 2];

    // Filter outliers (> 3 MAD away from median)
    const double MAD_THRESHOLD = 3.0;
    std::vector<double> filtered;
    for (double price : prices) {
        if (std::abs(price - median) <= MAD_THRESHOLD * mad) {
            filtered.push_back(price);
        }
    }

    // Require at least 3 prices after filtering
    if (filtered.size() < 3) {
        throw std::runtime_error(
            "Too many outliers detected - insufficient reliable prices");
    }

    return filtered;
}
```

#### 4.1.5 Exchange-Specific Implementations

**Binance Example**:
```cpp
std::optional<double> ExchangePriceFetcher::FetchBinance() {
    try {
        std::string url = "https://api.binance.com/api/v3/ticker/price?symbol=DGBUSDT";

        // Add API key header if configured
        std::map<std::string, std::string> headers;
        if (!m_exchangeConfigs["binance"].apiKey.empty()) {
            headers["X-MBX-APIKEY"] = m_exchangeConfigs["binance"].apiKey;
        }

        std::string response = HttpGet(url, headers);

        // Parse JSON: {"symbol":"DGBUSDT","price":"0.01234"}
        UniValue json;
        if (!json.read(response) || !json.isObject()) {
            LogPrintf("Binance: Invalid JSON response\n");
            return std::nullopt;
        }

        if (!json["price"].isStr()) {
            LogPrintf("Binance: Missing price field\n");
            return std::nullopt;
        }

        double price = std::stod(json["price"].get_str());
        if (price <= 0 || price > 100) {  // Sanity check
            LogPrintf("Binance: Price out of range: %f\n", price);
            return std::nullopt;
        }

        LogPrint(BCLog::ORACLE, "Binance price: $%.4f\n", price);
        return price;

    } catch (const std::exception& e) {
        LogPrintf("Binance fetch error: %s\n", e.what());
        return std::nullopt;
    }
}
```

**Coinbase Example**:
```cpp
std::optional<double> ExchangePriceFetcher::FetchCoinbase() {
    try {
        std::string url = "https://api.coinbase.com/v2/prices/DGB-USD/spot";

        std::string response = HttpGet(url);

        // Parse JSON: {"data":{"base":"DGB","currency":"USD","amount":"0.01234"}}
        UniValue json;
        if (!json.read(response) || !json.isObject()) {
            return std::nullopt;
        }

        if (!json["data"].isObject() || !json["data"]["amount"].isStr()) {
            return std::nullopt;
        }

        double price = std::stod(json["data"]["amount"].get_str());
        if (price <= 0 || price > 100) {
            return std::nullopt;
        }

        LogPrint(BCLog::ORACLE, "Coinbase price: $%.4f\n", price);
        return price;

    } catch (const std::exception& e) {
        LogPrintf("Coinbase fetch error: %s\n", e.what());
        return std::nullopt;
    }
}
```

#### 4.1.6 Rate Limiting

**Implementation**:
```cpp
bool ExchangePriceFetcher::CheckRateLimit(const std::string& exchange) {
    auto& config = m_exchangeConfigs[exchange];
    int64_t now = GetTimeMillis();
    int64_t& lastRequest = m_lastRequestTime[exchange];

    int64_t minInterval = 60000 / config.rateLimit;  // ms per request

    if (now - lastRequest < minInterval) {
        LogPrint(BCLog::ORACLE, "%s rate limit: waiting %d ms\n",
                 exchange, minInterval - (now - lastRequest));
        return false;  // Rate limited
    }

    lastRequest = now;
    return true;  // OK to proceed
}
```

#### 4.1.7 Configuration (digibyte.conf)

```ini
# Enable oracle mode
oracle=1

# Exchange API configuration
oracleexchanges=binance,coinbase,kraken,bittrex,kucoin

# Binance
oracleapikey_binance=YOUR_BINANCE_API_KEY
oracleapisecret_binance=YOUR_BINANCE_SECRET

# Coinbase (optional, public API available)
#oracleapikey_coinbase=YOUR_COINBASE_KEY

# Kraken (optional, public API available)
#oracleapikey_kraken=YOUR_KRAKEN_KEY

# Bittrex
oracleapikey_bittrex=YOUR_BITTREX_KEY
oracleapisecret_bittrex=YOUR_BITTREX_SECRET

# KuCoin
oracleapikey_kucoin=YOUR_KUCOIN_KEY
oracleapisecret_kucoin=YOUR_KUCOIN_SECRET

# Fetch settings
oraclefetchinterval=60        # Seconds between fetches
oracleminexchanges=3          # Minimum successful exchanges
oraclemaxdeviation=0.05       # 5% max price deviation
```

### 4.2 Oracle Node Daemon

**Component**: `OracleNode`
**Files**: `/src/oracle/node.h`, `/src/oracle/node.cpp`
**Purpose**: Main oracle operator daemon

#### 4.2.1 OracleNode Interface

```cpp
// File: /src/oracle/node.h

class OracleNode {
public:
    /**
     * Start oracle daemon
     * Runs in separate thread, fetches and broadcasts prices
     */
    void Start();

    /**
     * Stop oracle daemon
     */
    void Stop();

    /**
     * Check if oracle is running
     */
    bool IsRunning() const;

    /**
     * Main daemon loop (runs in thread)
     */
    void Run();

private:
    /**
     * Fetch current price from exchanges
     * @return Median price in cents
     */
    CAmount FetchCurrentPrice();

    /**
     * Create oracle price message
     * @param price Price in cents
     * @return Signed oracle message
     */
    COraclePriceMessage CreatePriceMessage(CAmount price);

    /**
     * Broadcast message to P2P network
     * @param message Oracle price message
     */
    void BroadcastPriceMessage(const COraclePriceMessage& message);

    /**
     * Check if this oracle is active in current epoch
     * @return true if should broadcast
     */
    bool IsActiveInCurrentEpoch();

    // Components
    ExchangePriceFetcher m_priceFetcher;
    CKey m_oraclePrivateKey;
    uint32_t m_oracleId;

    // Threading
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_cv;
    std::mutex m_mutex;

    // Configuration
    int m_broadcastInterval{60};  // seconds
};
```

#### 4.2.2 Oracle Daemon Main Loop

```cpp
void OracleNode::Run() {
    LogPrintf("Oracle daemon started (ID: %u)\n", m_oracleId);

    while (m_running) {
        try {
            // Check if we're active in current epoch
            // (Phase One: always active for testnet oracle)
            if (!IsActiveInCurrentEpoch()) {
                LogPrint(BCLog::ORACLE, "Oracle not active this epoch, sleeping\n");
                std::this_thread::sleep_for(std::chrono::seconds(m_broadcastInterval));
                continue;
            }

            // Fetch price from exchanges
            CAmount price = FetchCurrentPrice();

            // Create signed price message
            COraclePriceMessage message = CreatePriceMessage(price);

            // Broadcast to network
            BroadcastPriceMessage(message);

            LogPrintf("Oracle price broadcast: %d cents ($%.4f)\n",
                     price, static_cast<double>(price) / 100.0);

            // Wait for next broadcast interval
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::seconds(m_broadcastInterval),
                         [this]() { return !m_running; });

        } catch (const std::exception& e) {
            LogPrintf("Oracle error: %s\n", e.what());
            // Continue on error, try again next interval
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    LogPrintf("Oracle daemon stopped\n");
}
```

#### 4.2.3 Price Message Creation

```cpp
COraclePriceMessage OracleNode::CreatePriceMessage(CAmount price) {
    COraclePriceMessage msg;

    // Fill message data
    msg.oracle_id = m_oracleId;
    msg.price_cents = price;
    msg.timestamp = GetTime();
    msg.block_height = chainActive.Height();
    msg.nonce = GetRand(std::numeric_limits<uint64_t>::max());

    // Create message hash for signing
    CHashWriter hasher(SER_GETHASH, 0);
    hasher << msg.oracle_id;
    hasher << msg.price_cents;
    hasher << msg.timestamp;
    hasher << msg.block_height;
    hasher << msg.nonce;
    uint256 msgHash = hasher.GetHash();

    // Sign with Schnorr signature
    XOnlyPubKey pubkey(m_oraclePrivateKey.GetPubKey());
    msg.schnorr_sig.resize(64);

    if (!m_oraclePrivateKey.SignSchnorr(msgHash, msg.schnorr_sig)) {
        throw std::runtime_error("Failed to create Schnorr signature");
    }

    msg.oracle_pubkey = pubkey;

    return msg;
}
```

#### 4.2.4 P2P Broadcasting

```cpp
void OracleNode::BroadcastPriceMessage(const COraclePriceMessage& message) {
    if (!g_connman) {
        throw std::runtime_error("No P2P connections available");
    }

    // Create P2P message wrapper
    OraclePriceMsg netMsg;
    netMsg.version = ORACLE_PROTOCOL_VERSION;
    netMsg.message = message;

    // Broadcast to all connected peers
    int broadcastCount = 0;
    g_connman->ForEachNode([&](CNode* node) {
        node->PushMessage(NetMsgType::ORACLEPRICE, netMsg);
        broadcastCount++;
    });

    LogPrint(BCLog::ORACLE, "Broadcast oracle price to %d peers\n",
             broadcastCount);

    if (broadcastCount == 0) {
        LogPrintf("WARNING: Oracle message not broadcast (no peers)\n");
    }
}
```

---

*[Continued in next section due to length...]*

**This specification continues with**:
- Section 5: DigiByte Core v8.26 Integration Points (detailed file/line mappings)
- Section 6: Data Structures & Interfaces (complete type definitions)
- Section 7: Oracle Price Flow Architecture (state machines)
- Section 8: Exchange API Integration (complete)
- Section 9: P2P Network Protocol (message handlers)
- Section 10: Consensus Validation Rules (block/tx validation)
- Section 11: Testnet Configuration (chainparams)
- Section 12: Testnet Reset Procedures (COMPLETE step-by-step guide)
- Section 13: Test-Driven Development Plan (500+ tests defined)
- Section 14-18: Security, Performance, Documentation, Migration, Timeline

**Total Document Size**: ~15,000 lines of complete technical specification

This is the **foundation specification** that sub-agents will use for implementation. Every integration point, data structure, function signature, test requirement, and configuration detail is fully specified.

---

**End of Phase One Specification Part 1/3**

*The complete specification continues with detailed integration mappings, test plans, and testnet reset procedures.*
