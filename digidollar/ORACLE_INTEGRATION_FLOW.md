# Oracle System Integration Flow

## Overview

This document describes the complete integration flow of the DigiDollar Oracle System (Phase One) across all components of the DigiByte blockchain.

**Phase One Status**: 1-of-1 Consensus (Single Oracle)
**Network**: Testnet Only
**Price Source**: 8 Exchange APIs

---

## Component Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 1: Exchange API Layer (8 Exchanges)                               │
│                                                                          │
│ ┌────────────┐ ┌─────────────┐ ┌────────────┐ ┌──────────┐             │
│ │  Binance   │ │ CoinMarketCap│ │ CoinGecko  │ │ Coinbase │             │
│ └────────────┘ └─────────────┘ └────────────┘ └──────────┘             │
│ ┌────────────┐ ┌─────────────┐ ┌────────────┐ ┌──────────┐             │
│ │   Kraken   │ │   Messari   │ │   KuCoin   │ │Crypto.com│             │
│ └────────────┘ └─────────────┘ └────────────┘ └──────────┘             │
│                                                                          │
│ Files: src/oracle/exchange.{h,cpp}                                      │
│ Classes: ExchangeAPI::MultiExchangeAggregator                           │
│          ExchangeAPI::BinanceFetcher, CoinGeckoFetcher, etc.            │
└──────────────────────────┬───────────────────────────────────────────────┘
                           │ FetchAggregatePrice()
                           ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 2: Oracle Node Layer (Price Fetching & Signing)                   │
│                                                                          │
│ ┌───────────────────────────────────────────────────────────────────┐   │
│ │ Oracle Node (ID 0 - Phase One)                                    │   │
│ │                                                                   │   │
│ │ • Fetches prices every 15 seconds (1 DGB block)                  │   │
│ │ • Calculates median from 8 exchanges                             │   │
│ │ • Creates COraclePriceMessage                                    │   │
│ │ • Signs with BIP-340 Schnorr signature                           │   │
│ │ • Broadcasts to P2P network                                      │   │
│ └───────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│ Files: src/oracle/node.{h,cpp}                                          │
│ Classes: OracleNode, ExchangePriceFetcher                               │
│ Data: COraclePriceMessage (Schnorr signed)                              │
└──────────────────────────┬───────────────────────────────────────────────┘
                           │ BroadcastPriceMessage()
                           ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 3: P2P Network Layer (Message Broadcasting)                       │
│                                                                          │
│ ┌───────────────────────────────────────────────────────────────────┐   │
│ │ P2P Message Types:                                                │   │
│ │ • ORACLEPRICE  - Individual oracle price messages                │   │
│ │ • ORACLEBUNDLE - Oracle bundle (collection of messages)          │   │
│ │ • GETORACLES   - Request oracle data from peers                  │   │
│ │                                                                   │   │
│ │ Features:                                                         │   │
│ │ • Signature verification before relay                            │   │
│ │ • DOS protection (rate limiting)                                 │   │
│ │ • Duplicate detection                                            │   │
│ └───────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│ Files: src/net_processing.cpp, src/protocol.{h,cpp}                     │
│ Namespace: OracleP2P::ValidateIncomingMessage()                         │
└──────────────────────────┬───────────────────────────────────────────────┘
                           │ ProcessMessage(ORACLEPRICE)
                           ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 4: Bundle Manager Layer (Consensus Collection)                    │
│                                                                          │
│ ┌───────────────────────────────────────────────────────────────────┐   │
│ │ OracleBundleManager (Singleton)                                   │   │
│ │                                                                   │   │
│ │ Phase One Consensus: 1-of-1                                       │   │
│ │ • Collects oracle messages from P2P network                      │   │
│ │ • Creates bundles (1 message required in Phase One)              │   │
│ │ • Validates signatures using Schnorr verification                │   │
│ │ • Manages price cache (block height → price)                     │   │
│ │ • Provides oracle data to miner and validation                   │   │
│ └───────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│ Files: src/oracle/bundle_manager.{h,cpp}                                │
│ Classes: OracleBundleManager, OracleDataValidator                       │
│ Data: COracleBundle (contains 1 message in Phase One)                   │
└──────────────────────────┬───────────────────────────────────────────────┘
                           │ AddOracleBundleToBlock()
                           ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 5: Miner Integration Layer (Block Creation)                       │
│                                                                          │
│ ┌───────────────────────────────────────────────────────────────────┐   │
│ │ BlockAssembler::CreateNewBlock()                                  │   │
│ │                                                                   │   │
│ │ Coinbase Transaction Structure:                                  │   │
│ │ • vout[0]: Miner payout (72,000 DGB subsidy + fees)              │   │
│ │ • vout[1]: Oracle bundle OP_RETURN (unspendable, value = 0)      │   │
│ │                                                                   │   │
│ │ OP_RETURN Format:                                                 │   │
│ │ OP_RETURN <serialized_COracleBundle>                             │   │
│ │                                                                   │   │
│ │ Size Limit: 83 bytes (MAX_OP_RETURN_RELAY)                       │   │
│ └───────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│ Files: src/node/miner.cpp                                               │
│ Function: OracleBundleManager::AddOracleBundleToBlock()                 │
│ Integration: Called after DigiDollar activation check                   │
└──────────────────────────┬───────────────────────────────────────────────┘
                           │ Block submitted for validation
                           ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 6: Block Validation Layer (Consensus Rules)                       │
│                                                                          │
│ ┌───────────────────────────────────────────────────────────────────┐   │
│ │ CheckBlock() - Basic Validation                                   │   │
│ │ • Validates bundle structure                                      │   │
│ │ • Verifies Schnorr signatures                                     │   │
│ │ • Checks message count (1 in Phase One)                          │   │
│ │ • Validates OP_RETURN size limits                                │   │
│ │                                                                   │   │
│ │ ContextualCheckBlock() - Contextual Validation                   │   │
│ │ • Validates timestamps (not too old)                             │   │
│ │ • Checks oracle epoch alignment                                  │   │
│ │                                                                   │   │
│ │ ConnectBlock() - State Updates                                   │   │
│ │ • Extracts oracle bundle from coinbase                           │   │
│ │ • Updates price cache for block height                           │   │
│ │ • Makes price available to DigiDollar                            │   │
│ └───────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│ Files: src/validation.cpp, src/primitives/oracle.{h,cpp}                │
│ Functions: OracleDataValidator::ValidateBlockOracleData()               │
│           OracleBundleManager::UpdatePriceCache()                       │
└──────────────────────────┬───────────────────────────────────────────────┘
                           │ Price cache updated
                           ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ LAYER 7: DigiDollar Integration Layer (Price Access)                    │
│                                                                          │
│ ┌───────────────────────────────────────────────────────────────────┐   │
│ │ OracleIntegration Namespace                                       │   │
│ │                                                                   │   │
│ │ GetOraclePriceForHeight(int nHeight)                             │   │
│ │ • Retrieves cached price for specific block height              │   │
│ │ • Used by DigiDollar mint/redeem validation                      │   │
│ │ • Returns price in micro-USD (1,000,000 = $1.00)                │   │
│ │                                                                   │   │
│ │ GetCurrentOraclePrice()                                          │   │
│ │ • Returns most recent oracle price                               │   │
│ │ • Used for mempool transaction validation                        │   │
│ │ • Falls back to safe default if unavailable                      │   │
│ └───────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│ Files: src/oracle/bundle_manager.{h,cpp}                                │
│        src/consensus/digidollar_transaction_validation.cpp              │
│ Namespace: OracleIntegration::                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Integration Points Detail

### Integration Point 1: Exchange API → Oracle Node

**Location**: `src/oracle/node.cpp:FetchMedianPrice()`

**Code**:
```cpp
CAmount OracleNode::FetchMedianPrice()
{
    ExchangeAPI::MultiExchangeAggregator aggregator;
    std::vector<ExchangeAPI::MultiExchangeAggregator::ExchangePrice> prices = aggregator.FetchAllPrices();

    if (prices.empty()) {
        LogPrintf("Oracle: Failed to fetch prices from exchanges\n");
        return 0;
    }

    return aggregator.CalculateMedianPrice(prices);
}
```

**Data Flow**:
- Oracle node queries 8 exchanges
- Each exchange returns DGB/USD price
- Median price calculated from valid responses
- Minimum 3 exchanges required for valid price

**Error Handling**:
- Outlier filtering (10% threshold)
- Fallback to subset if some exchanges fail
- Returns 0 if insufficient data

---

### Integration Point 2: Oracle Node → Bundle Manager

**Location**: `src/oracle/node.cpp:BroadcastCurrentPrice()`

**Code**:
```cpp
void OracleNode::BroadcastCurrentPrice()
{
    CAmount price = FetchMedianPrice();
    if (price <= 0) return;

    COraclePriceMessage msg = CreatePriceMessage(price, GetTime());
    if (!msg.Sign(private_key)) {
        LogPrintf("Oracle: Failed to sign message\n");
        return;
    }

    BroadcastPriceMessage(msg);
}

bool OracleNode::BroadcastPriceMessage(const COraclePriceMessage& message)
{
    return OracleBundleManager::GetInstance().BroadcastMessage(message);
}
```

**Data Flow**:
- Oracle node creates `COraclePriceMessage`
- Signs with BIP-340 Schnorr signature
- Broadcasts to P2P network via Bundle Manager
- Bundle Manager relays to connected peers

---

### Integration Point 3: P2P Network → Bundle Manager

**Location**: `src/net_processing.cpp:ProcessMessage(ORACLEPRICE)`

**Code**:
```cpp
// In ProcessMessage() handler
if (msg_type == NetMsgType::ORACLEPRICE) {
    COraclePriceMessage oracle_msg;
    vRecv >> oracle_msg;

    // Validate message before accepting
    if (!OracleP2P::ValidateIncomingMessage(oracle_msg)) {
        Misbehaving(pfrom, 10, "invalid oracle message");
        return;
    }

    // Add to bundle manager
    OracleBundleManager::GetInstance().AddOracleMessage(oracle_msg);

    // Relay to other peers
    OracleBundleManager::GetInstance().BroadcastMessage(oracle_msg);
}
```

**Data Flow**:
- Peer receives `ORACLEPRICE` P2P message
- Validates signature and structure
- Adds to local bundle manager
- Relays to other peers (flood-fill)

**DOS Protection**:
- Rate limiting per oracle ID
- Message size limits
- Duplicate detection
- Signature verification before relay

---

### Integration Point 4: Bundle Manager → Miner

**Location**: `src/node/miner.cpp:CreateNewBlock()`

**Code**:
```cpp
// In CreateNewBlock() after coinbase creation
if (DigiDollar::IsDigiDollarEnabled(pindexPrev, m_chainstate.m_chainman)) {
    OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
    if (!oracle_manager.AddOracleBundleToBlock(*pblock, nHeight)) {
        LogPrintf("CreateNewBlock(): Warning - Failed to add oracle bundle to block %d\n", nHeight);
        // Continue with block creation (graceful degradation)
    }
}
```

**Location**: `src/oracle/bundle_manager.cpp:AddOracleBundleToBlock()`

**Code**:
```cpp
bool OracleBundleManager::AddOracleBundleToBlock(CBlock& block, int32_t block_height) const
{
    int32_t epoch = GetCurrentEpoch(block_height);
    COracleBundle bundle = GetCurrentBundle(epoch);

    if (!bundle.IsValid() || !bundle.HasConsensus()) {
        return false;  // No valid bundle available
    }

    // Create OP_RETURN script with serialized bundle
    CScript oracle_script = CreateOracleScript(bundle);

    // Add as second output in coinbase
    CMutableTransaction mtx(*block.vtx[0]);
    mtx.vout.push_back(CTxOut(0, oracle_script));  // Unspendable output
    block.vtx[0] = MakeTransactionRef(mtx);

    return true;
}
```

**Data Flow**:
- Miner requests current oracle bundle
- Bundle Manager returns bundle for current epoch
- Bundle serialized to OP_RETURN format
- Added as second output in coinbase transaction

---

### Integration Point 5: Miner → Block Validation

**Location**: `src/validation.cpp:CheckBlock()`

**Code**:
```cpp
// In CheckBlock() after merkle root validation
if (!OracleDataValidator::ValidateBlockOracleData(block, nullptr, consensusParams)) {
    return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS,
                        "bad-oracle-data", "invalid oracle data in block");
}
```

**Location**: `src/oracle/bundle_manager.cpp:ValidateBlockOracleData()`

**Code**:
```cpp
bool OracleDataValidator::ValidateBlockOracleData(const CBlock& block,
                                                  const CBlockIndex* pindex_prev,
                                                  const Consensus::Params& params)
{
    // Extract oracle bundle from coinbase
    if (block.vtx.empty()) return true;

    COracleBundle bundle;
    if (!OracleBundleManager::GetInstance().ExtractOracleBundle(*block.vtx[0], bundle)) {
        return true;  // No oracle data present (allowed)
    }

    // Validate bundle structure
    if (!bundle.IsValid()) {
        return false;
    }

    // Verify all signatures
    for (const auto& msg : bundle.messages) {
        if (!msg.Verify()) {
            return false;
        }
    }

    // Phase One: Require exactly 1 message
    if (bundle.messages.size() != 1) {
        return false;
    }

    return true;
}
```

**Data Flow**:
- Block validation extracts oracle bundle from coinbase
- Validates bundle structure
- Verifies all Schnorr signatures
- Checks Phase One consensus (1-of-1)
- Rejects block if validation fails

---

### Integration Point 6: Block Validation → Price Cache

**Location**: `src/validation.cpp:ConnectBlock()`

**Code**:
```cpp
// In ConnectBlock() after transaction processing
// Update oracle price cache (Phase One: testnet only)
if (m_chainman.GetParams().GetChainType() == ChainType::TESTNET && !fJustCheck) {
    if (!block.vtx.empty() && block.vtx[0]->vout.size() >= 2) {
        const CTxOut& oracle_output = block.vtx[0]->vout[1];

        if (oracle_output.scriptPubKey.IsUnspendable() && oracle_output.scriptPubKey.size() > 2) {
            std::vector<unsigned char> data(oracle_output.scriptPubKey.begin() + 2,
                                           oracle_output.scriptPubKey.end());

            try {
                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                COracleBundle bundle;
                ss >> bundle;

                // Update oracle price cache for this height
                OracleBundleManager& manager = OracleBundleManager::GetInstance();
                manager.UpdatePriceCache(pindex->nHeight, bundle.median_price_micro_usd);

                LogPrint(BCLog::DIGIDOLLAR, "Oracle: Updated price cache at height %d: %llu micro-USD\n",
                         pindex->nHeight, bundle.median_price_micro_usd);

            } catch (const std::exception& e) {
                LogPrint(BCLog::DIGIDOLLAR, "Oracle: Failed to update price cache: %s\n", e.what());
            }
        }
    }
}
```

**Data Flow**:
- ConnectBlock extracts oracle bundle from coinbase
- Deserializes bundle data
- Updates price cache with median price
- Cache indexed by block height

**Price Cache**:
- Thread-safe map: `height → price_micro_usd`
- LRU cache (keeps last 1000 blocks)
- Used for historical price lookups

---

### Integration Point 7: Price Cache → DigiDollar

**Location**: `src/oracle/bundle_manager.cpp:OracleIntegration`

**Code**:
```cpp
namespace OracleIntegration {

CAmount GetOraclePriceForHeight(int nHeight)
{
    // In RegTest mode, use MockOracleManager for testing
    if (Params().GetChainType() == ChainType::REGTEST) {
        return MockOracleManager::GetOraclePriceForHeight(nHeight);
    }

    // Production: Use real oracle bundle manager
    OracleBundleManager& manager = OracleBundleManager::GetInstance();
    uint64_t price = manager.GetOraclePriceForHeight(nHeight);

    // Fallback to default price if no oracle data available
    if (price <= 0) {
        price = 50000;  // $0.05 per DGB default (micro-USD)
        LogPrintf("Oracle: Using fallback price: %llu micro-USD\n", price);
    }

    return price;
}

CAmount GetCurrentOraclePrice()
{
    // Get price from most recent block
    int current_height = GetBestHeight();
    return GetOraclePriceForHeight(current_height);
}

} // namespace OracleIntegration
```

**Location**: `src/consensus/digidollar_transaction_validation.cpp`

**Code**:
```cpp
// DigiDollar mint/redeem validation uses oracle price
bool ValidateDigiDollarMint(const CTransaction& tx, int block_height)
{
    // Get oracle price for this block
    CAmount dgb_price_micro_usd = OracleIntegration::GetOraclePriceForHeight(block_height);

    // Calculate expected DigiDollar output
    CAmount dgb_input = GetDGBInputAmount(tx);
    CAmount expected_dd_output = (dgb_input * dgb_price_micro_usd) / 1000000;

    // Validate mint amount
    CAmount actual_dd_output = GetDDOutputAmount(tx);
    return (actual_dd_output == expected_dd_output);
}
```

**Data Flow**:
- DigiDollar transaction validation requests price
- `OracleIntegration::GetOraclePriceForHeight()` called
- Price retrieved from cache
- Price used for mint/redeem calculation
- Falls back to safe default if unavailable

---

## Phase One Specifications

### Consensus Model: 1-of-1

Phase One uses simplified consensus:
- **Single oracle** (Oracle ID 0)
- **No multi-oracle aggregation**
- **No median calculation** (single price is the price)
- **Testnet only**

This allows for:
- Rapid deployment and testing
- Simple validation logic
- Easy debugging
- Smooth transition to Phase Two

### Phase Two Upgrade Path

Phase Two will implement:
- **15 active oracles** (rotated from pool of 30)
- **8-of-15 consensus** (majority required)
- **Median price calculation** from multiple oracles
- **Outlier filtering** (MAD-based)
- **Mainnet deployment**

The integration points remain the same - only the bundle creation and validation logic changes.

---

## Error Handling & Graceful Degradation

### No Oracle Data Available

**Scenario**: Oracle system disabled or no messages received

**Behavior**:
- Miner creates block without oracle data
- Block validation passes (oracle data optional in Phase One)
- DigiDollar uses fallback price ($0.05 default)
- System logs warning

**Code**:
```cpp
// In CreateNewBlock()
if (!oracle_manager.AddOracleBundleToBlock(*pblock, nHeight)) {
    LogPrintf("CreateNewBlock(): Warning - Failed to add oracle bundle\n");
    // Continue with block creation - graceful degradation
}
```

### Invalid Oracle Signatures

**Scenario**: Oracle message has invalid Schnorr signature

**Behavior**:
- P2P layer rejects message
- Peer misbehavior score increased
- Message not relayed
- Bundle Manager never sees invalid message

**Code**:
```cpp
// In ProcessMessage(ORACLEPRICE)
if (!OracleP2P::ValidateIncomingMessage(oracle_msg)) {
    Misbehaving(pfrom, 10, "invalid oracle message");
    return;  // Don't relay
}
```

### Oracle Message Too Old

**Scenario**: Oracle message timestamp > 1 hour old

**Behavior**:
- Bundle Manager rejects message
- Message not included in bundle
- Peer not penalized (may be network delay)

**Code**:
```cpp
// In OracleBundleManager::AddOracleMessage()
if (GetTime() - message.timestamp > ORACLE_MAX_AGE_SECONDS) {
    LogPrintf("Oracle: Message too old from oracle %d\n", message.oracle_id);
    return false;
}
```

### Price Outliers

**Scenario**: Exchange returns price 50% different from median

**Behavior**:
- Exchange API filters outlier
- Median calculated from remaining valid prices
- Requires minimum 3 valid prices
- Returns 0 if insufficient data

**Code**:
```cpp
// In MultiExchangeAggregator::FilterOutliers()
std::vector<ExchangePrice> MultiExchangeAggregator::FilterOutliers(
    const std::vector<ExchangePrice>& prices)
{
    CAmount median = CalculateMedianPrice(prices);
    std::vector<ExchangePrice> filtered;

    for (const auto& price : prices) {
        double deviation = std::abs(price.price_cents - median) / static_cast<double>(median);
        if (deviation <= outlier_threshold) {
            filtered.push_back(price);
        }
    }

    return filtered;
}
```

---

## Testing Integration

### Unit Tests

**File**: `src/test/oracle_integration_tests.cpp`

Tests:
1. **end_to_end_oracle_flow** - Complete flow from Exchange API to DigiDollar
2. **oracle_graceful_degradation** - System works without oracle data
3. **verify_integration_points** - All 7 integration points connected

### Functional Tests

**Planned**: `test/functional/feature_oracle_integration.py`

Tests:
- Multi-node oracle message propagation
- Block creation with oracle bundles
- Chain reorg handling
- Oracle price cache consistency

---

## Performance Characteristics

### Oracle Message Size

**COraclePriceMessage**:
- oracle_id: 4 bytes
- price_micro_usd: 8 bytes
- timestamp: 8 bytes
- block_height: 4 bytes
- nonce: 8 bytes
- oracle_pubkey: 32 bytes (x-only)
- schnorr_sig: 64 bytes
- **Total**: ~128 bytes

**COracleBundle** (Phase One):
- messages: 1 × 128 bytes
- epoch: 4 bytes
- median_price_micro_usd: 8 bytes
- timestamp: 8 bytes
- **Total**: ~148 bytes

**OP_RETURN overhead**: 2 bytes (OP_RETURN + push)
**Coinbase size increase**: ~150 bytes

### Network Bandwidth

**Per Block** (15 seconds):
- 1 oracle message broadcast: ~128 bytes
- 1 block with oracle bundle: ~150 bytes overhead
- **Total**: ~278 bytes per 15 seconds
- **Rate**: ~18.5 bytes/second

**Negligible impact** on network bandwidth.

### CPU Usage

**Schnorr Signature Verification**:
- ~0.5ms per signature on modern CPU
- 1 signature per block in Phase One
- **Negligible impact** on block validation time

### Memory Usage

**Price Cache**:
- 1000 blocks × 16 bytes = ~16 KB
- **Negligible impact** on memory usage

---

## Security Considerations

### Schnorr Signature Security

- **BIP-340 compliant** signatures
- **32-byte x-only public keys** (compact)
- **64-byte signatures** (no malleability)
- **Deterministic signing** (RFC 6979)

### DOS Protection

- **Rate limiting**: Max 1 message per oracle per 15 seconds
- **Message size limits**: Max 200 bytes per message
- **Duplicate detection**: Hash-based seen filter
- **Signature verification**: Before relay, not after

### Price Manipulation Resistance

**Phase One** (1-of-1):
- Single oracle = **trusted setup**
- Suitable for testnet only
- Median of 8 exchanges provides some protection

**Phase Two** (8-of-15):
- Requires majority oracle compromise
- Multiple independent operators
- Outlier filtering (MAD-based)
- Significantly more secure

---

## Monitoring & Observability

### Logging

All oracle activity logged with `BCLog::DIGIDOLLAR` category:

```bash
# Enable oracle logging
digibyted -debug=digidollar

# Example logs
Oracle: Initializing Oracle Bundle Manager
Oracle: Added new message from oracle 0: price=50000 micro-USD, timestamp=1234567890
Oracle: Created oracle bundle (1-of-1 consensus)
Oracle: Updated price cache at height 12345: 50000 micro-USD ($0.05)
DigiDollar: Using oracle price: 50000 micro-USD ($0.05)
```

### RPC Commands

```bash
# Get oracle stats
digibyte-cli getoracle stats

# Get oracle price for height
digibyte-cli getoracle price 12345

# Get oracle bundle for epoch
digibyte-cli getoracle bundle 82

# Check oracle system status
digibyte-cli getoracle status
```

### Metrics

Track:
- Oracle message count
- Bundle creation rate
- Price cache hit rate
- Signature verification failures
- Exchange API failures

---

## Deployment Checklist

See `ORACLE_INTEGRATION_CHECKLIST.md` for complete deployment checklist.

---

## Future Enhancements

### Phase Two Upgrades
- [ ] Multi-oracle consensus (8-of-15)
- [ ] Dynamic oracle selection (rotation)
- [ ] Advanced outlier filtering (MAD-based)
- [ ] Mainnet deployment

### Additional Features
- [ ] Oracle node discovery (P2P)
- [ ] Oracle reputation system
- [ ] Price volatility detection
- [ ] Circuit breaker for extreme price changes
- [ ] Historical price API

---

## References

- **Oracle System Specification**: `DIGIDOLLAR_ORACLE_ORCHESTRATOR_PROMPT.md`
- **BIP-340 Schnorr Signatures**: https://github.com/bitcoin/bips/blob/master/bip-0340.mediawiki
- **DigiDollar Documentation**: `doc/DIGIDOLLAR.md`
- **Integration Tests**: `src/test/oracle_integration_tests.cpp`

---

**Document Version**: 1.0
**Last Updated**: 2025-11-18
**Status**: Phase One Implementation Complete
