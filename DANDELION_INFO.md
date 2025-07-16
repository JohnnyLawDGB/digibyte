# DigiByte v8.22.2 Dandelion++ Implementation Analysis

## Overview

DigiByte v8.22.2 implements Dandelion++, an enhanced privacy protocol for transaction propagation. This document provides a comprehensive analysis of how Dandelion++ is implemented in DigiByte's codebase, including all file locations, key functions, and a detailed explanation of how the protocol works.

## What is Dandelion++?

Dandelion++ is a privacy-enhancing transaction routing mechanism that protects users from network-level deanonymization attacks. Based on BIP-156, it works by:

1. **Stem Phase**: Transactions travel through a randomly selected path before broad network diffusion
2. **Fluff Phase**: Eventually transitions to standard Bitcoin-style flooding propagation
3. **Privacy Guarantee**: Breaks the symmetry of transaction propagation to prevent adversaries from linking transactions to their origin

## File Locations and Structure

### Core Implementation Files

#### 1. `/src/dandelion.cpp` (327 lines)
**Purpose**: Main Dandelion++ routing implementation
**Key Components**:
- Connection management for Dandelion peers
- Route selection and shuffling algorithms
- Embargo management for transactions
- Thread management for periodic route reshuffling

#### 2. `/src/protocol.h` & `/src/protocol.cpp`
**Purpose**: Network protocol definitions
**Key Additions**:
- `NetMsgType::DANDELIONTX` - Message type for stem phase transactions
- `MSG_DANDELION_TX = 5` - Inventory type for Dandelion transactions
- `MSG_DANDELION_WITNESS_TX` - Witness version of Dandelion transactions

#### 3. `/src/net.h` & `/src/net.cpp`
**Purpose**: Network layer integration
**Key Components**:
- Dandelion peer vectors: `vDandelionInbound`, `vDandelionOutbound`, `vDandelionDestination`
- Route mapping: `mDandelionRoutes`
- Embargo tracking: `mDandelionEmbargo`
- Configuration constants

#### 4. `/src/net_processing.h` & `/src/net_processing.cpp`
**Purpose**: Message processing and transaction relay
**Key Functions**:
- `ProcessMessage()` - Handles DANDELIONTX messages
- `RelayDandelionTransaction()` - Routes transactions in stem phase
- `CheckDandelionEmbargoes()` - Manages transaction embargoes

#### 5. `/src/node/transaction.cpp`
**Purpose**: Transaction broadcasting interface
**Key Features**:
- Wallet integration for Dandelion transactions
- Embargo creation for locally generated transactions
- Stempool submission logic

#### 6. `/src/txmempool.h` & `/src/txmempool.cpp`
**Purpose**: Memory pool with stempool support
**Key Features**:
- `m_is_stempool` flag to differentiate stem transactions
- Separate memory pool instance for stem phase

#### 7. `/src/validation.h` & `/src/validation.cpp`
**Purpose**: Transaction validation with stempool integration
**Key Features**:
- Stempool validation logic
- AcceptToMemoryPool overloads for stempool

### Supporting Files

#### Wallet Integration
- `/src/wallet/wallet.h` - Wallet Dandelion support
- `/src/wallet/init.cpp` - Command-line options
- `/src/dummywallet.cpp` - Stub implementation

#### Initialization & Logging
- `/src/init.cpp` - Dandelion initialization and thread startup
- `/src/logging.h` & `/src/logging.cpp` - BCLog::DANDELION category

#### Build System
- `/src/Makefile.am` - Includes dandelion.cpp in build

## Key Constants and Configuration

```cpp
// Network Constants (from net.h)
static const bool DEFAULT_DANDELION = true;                    // Enabled by default
static const int DANDELION_MAX_DESTINATIONS = 2;               // Max outbound Dandelion peers
static constexpr auto DANDELION_SHUFFLE_INTERVAL = 10min;      // Route reshuffling interval
static constexpr auto DANDELION_EMBARGO_MINIMUM = 10s;         // Minimum embargo time
static constexpr auto DANDELION_EMBARGO_AVG_ADD = 20s;         // Average additional embargo

// Fluff Probability (from net_processing.h)
static const unsigned int DANDELION_FLUFF = 10;                // 10% chance to fluff

// Discovery Hash (from net.h)
static const uint256 DANDELION_DISCOVERYHASH = uint256S("0xfff...fff");
```

## Core Components and Data Structures

### 1. Peer Management (CConnman class)

```cpp
// Dandelion peer vectors
std::vector<CNode*> vDandelionInbound;      // Peers that can send us stem transactions
std::vector<CNode*> vDandelionOutbound;     // Peers we can send stem transactions to
std::vector<CNode*> vDandelionDestination;  // Selected subset for routing (max 2)

// Routing table
std::map<CNode*, CNode*> mDandelionRoutes;  // Maps inbound peer to outbound destination

// Local routing
CNode* localDandelionDestination;            // Where we send our own transactions

// Transaction embargoes
std::map<uint256, std::chrono::microseconds> mDandelionEmbargo;
```

### 2. Stempool

The stempool is a separate transaction memory pool for stem phase transactions:
- Implemented as a second CTxMemPool instance
- Marked with `m_is_stempool = true`
- Transactions move from stempool to mempool during fluff

## How Dandelion++ Works in DigiByte

### 1. Initialization Phase

When DigiByte starts:
1. `-dandelion` command line option is checked (default: true)
2. A stempool instance is created alongside the regular mempool
3. `ThreadDandelionShuffle` thread is started after IBD completes

### 2. Connection Establishment

**Inbound Connections**:
```cpp
// When accepting a new inbound connection (net.cpp)
vDandelionInbound.push_back(pnode);
CNode* pto = SelectFromDandelionDestinations();
if (pto) {
    mDandelionRoutes.insert(std::make_pair(pnode, pto));
}
```

**Outbound Connections**:
```cpp
// When making an outbound connection (net.cpp)
vDandelionOutbound.push_back(pnode);
if (vDandelionDestination.size() < DANDELION_MAX_DESTINATIONS) {
    vDandelionDestination.push_back(pnode);
}
// Send discovery message
CInv dummyInv(MSG_DANDELION_TX, DANDELION_DISCOVERYHASH);
pnode->PushInventory(dummyInv);
```

### 3. Transaction Flow

#### A. Local Transaction Broadcast

When a wallet creates a transaction:

1. **Stempool Submission** (node/transaction.cpp):
```cpp
if (gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
    // Submit to stempool instead of mempool
    AcceptToMemoryPool(chainstate, *node.stempool, tx, false, false);

    // Create embargo
    auto nEmbargo = DANDELION_EMBARGO_MINIMUM + PoissonNextSend(current_time, DANDELION_EMBARGO_AVG_ADD);
    node.connman->insertDandelionEmbargo(txid, nEmbargo);

    // Send via Dandelion
    CInv embargoTx(MSG_DANDELION_TX, txid);
    node.connman->localDandelionDestinationPushInventory(embargoTx);
}
```

2. **Route Selection**:
- Uses `localDandelionDestination` (selected from `vDandelionDestination`)
- Falls back to regular broadcast if no Dandelion peers available

#### B. Receiving Stem Transactions

When receiving a DANDELIONTX message (net_processing.cpp):

1. **Validation**:
```cpp
if (m_connman.isDandelionInbound(&pfrom)) {
    if (!m_stempool.exists(inv.hash)) {
        MempoolAcceptResult result = AcceptToMemoryPool(chainstate, m_stempool, ptx, false);
        if (result.m_result_type == MempoolAcceptResult::ResultType::VALID) {
            // Create embargo to prevent premature requests
            auto nEmbargo = DANDELION_EMBARGO_MINIMUM + PoissonNextSend(...);
            m_connman.insertDandelionEmbargo(tx.GetHash(), nEmbargo);
        }
    }
}
```

2. **Relay Decision**:
```cpp
void RelayDandelionTransaction(const CTransaction& tx, CNode* pfrom) {
    // 10% chance to fluff
    bool willFluff = rng.randrange(100) < DANDELION_FLUFF;

    if (willFluff) {
        // Move to mempool and broadcast normally
        CTransactionRef ptx = m_stempool.get(tx.GetHash());
        AcceptToMemoryPool(chainstate, m_mempool, ptx, false);
        RelayTransaction(tx.GetHash(), tx.GetWitnessHash());
    } else {
        // Continue stem phase
        CNode* destination = m_connman.getDandelionDestination(pfrom);
        if (destination) {
            CInv inv(MSG_DANDELION_TX, tx.GetHash());
            destination->PushOtherInventory(inv);
        }
    }
}
```

### 4. Embargo System

Embargoes prevent information leakage through timing:

1. **Purpose**: Hide whether we have a transaction by delaying responses
2. **Duration**: 10-30 seconds (randomized)
3. **Behavior**: GETDATA requests receive NOTFOUND during embargo

```cpp
// In ProcessGetData (net_processing.cpp)
if (m_connman.isTxDandelionEmbargoed(inv.hash)) {
    vNotFound.push_back(inv);
    continue;
}
```

### 5. Route Shuffling

Every ~10 minutes (randomized), routes are reshuffled:

```cpp
void DandelionShuffle() {
    // Clear all routes
    mDandelionRoutes.clear();
    localDandelionDestination = nullptr;
    vDandelionDestination.clear();

    // Select new destinations (max 2)
    while (vDandelionDestination.size() < DANDELION_MAX_DESTINATIONS) {
        // Random selection from vDandelionOutbound
    }

    // Regenerate routes for all inbound peers
    for (auto pnode : vDandelionInbound) {
        CNode* pto = SelectFromDandelionDestinations();
        if (pto) {
            mDandelionRoutes.insert(std::make_pair(pnode, pto));
        }
    }
}
```

### 6. Peer Disconnection Handling

When a Dandelion peer disconnects:

1. Remove from all Dandelion vectors
2. If it was a destination, select replacement
3. Update all routes using this peer
4. Replace localDandelionDestination if needed

## Security Properties

### 1. Anonymity Set
- Each node appears equally likely to be the source
- Even partial deployment provides population-level benefits

### 2. Black Hole Resistance
- 10% fluff probability ensures eventual propagation
- Embargo timeouts prevent indefinite delays
- Missing routes trigger immediate fluff

### 3. Routing Attacks
- Per-inbound-edge routing prevents fingerprinting
- Periodic shuffling limits route learning
- Limited destinations (2) reduces graph analysis

## Testing

### Functional Tests
- `/test/functional/p2p_dandelion.py` - Main Dandelion test suite
- `/test/functional/wallet_basic.py` - Wallet integration tests
- `/test/functional/feature_block.py` - Block handling with stempool

### Unit Tests
- `/src/test/denialofservice_tests.cpp` - DoS protection
- `/src/test/validation_*_tests.cpp` - Validation with stempool
- `/src/wallet/test/wallet_tests.cpp` - Wallet unit tests

## Command Line Options

```bash
-dandelion=<0|1>    # Enable/disable Dandelion++ (default: 1)
-debug=dandelion    # Enable Dandelion debug logging
```

## Differences from BIP-156

1. **Dandelion++ vs Dandelion**: DigiByte implements the enhanced ++ version
2. **Two destinations**: Instead of one, for improved reliability
3. **Embargo system**: Additional privacy through timing obfuscation
4. **Discovery mechanism**: Uses special inventory message for capability detection

## Summary

DigiByte's Dandelion++ implementation provides strong transaction privacy by:
- Routing transactions through random paths before broadcasting
- Using separate stem and fluff phases with probabilistic transition
- Implementing embargoes to prevent timing analysis
- Periodically reshuffling routes to prevent learning attacks
- Maintaining compatibility with non-Dandelion nodes

The implementation is well-integrated throughout the codebase, with clear separation between stem phase (stempool) and fluff phase (mempool) transactions, making it a robust privacy enhancement for DigiByte users.
