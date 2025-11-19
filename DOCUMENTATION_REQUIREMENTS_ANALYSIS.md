# DigiDollar Oracle Phase One - Documentation Requirements Analysis

**Document Version**: 1.0
**Date**: 2025-11-18
**Author**: Documentation & Integration Reviewer
**Status**: Planning Phase - DO NOT IMPLEMENT YET
**Deadline**: End of Day 3, Week 1

---

## Executive Summary

This document provides a comprehensive analysis of ALL documentation requirements for DigiDollar Oracle Phase One. This is a PLANNING document - actual documentation writing will occur in Week 5-6 of the implementation timeline.

**Total Documentation Required**: 6 major documents + configuration examples + FAQs
**Estimated Effort**: 20-24 hours of technical writing
**Target Audience**: Oracle operators, developers, testers, and DigiDollar integrators

---

## Table of Contents

1. [Complete Documentation Inventory](#1-complete-documentation-inventory)
2. [Testnet Reset Procedures Analysis](#2-testnet-reset-procedures-analysis)
3. [Oracle Operator Setup Guide Outline](#3-oracle-operator-setup-guide-outline)
4. [Integration Validation Checklist](#4-integration-validation-checklist)
5. [Documentation Standards](#5-documentation-standards)
6. [FAQ Section Requirements](#6-faq-section-requirements)
7. [Implementation Timeline](#7-implementation-timeline)

---

## 1. Complete Documentation Inventory

### 1.1 Operator Documentation (CRITICAL PATH)

These documents are REQUIRED for testnet operations:

#### 1.1.1 Oracle Operator Setup Guide
**File**: `/doc/ORACLE_OPERATOR_GUIDE.md`
**Priority**: CRITICAL
**Audience**: Testnet oracle operators
**Estimated Length**: 2,500-3,000 words
**Completion Deadline**: End of Week 5

**Purpose**:
- Complete step-by-step setup instructions for running a testnet oracle
- Exchange API key configuration
- Security best practices
- Operational procedures

**Dependencies**:
- Phase One implementation complete
- All 8 exchange API clients working
- P2P broadcasting operational
- Oracle daemon tested

#### 1.1.2 Testnet Reset Procedures
**File**: `/doc/TESTNET_RESET_PROCEDURES.md`
**Priority**: CRITICAL
**Audience**: DigiDollar developers, testnet operators
**Estimated Length**: 1,500-2,000 words
**Completion Deadline**: End of Week 5

**Purpose**:
- Complete 12-step testnet reset procedure
- Pre-reset checklist
- Post-reset verification
- Troubleshooting common issues

**Dependencies**:
- Testnet configuration in chainparams.cpp complete
- Oracle integration tested on testnet
- Reset procedure tested successfully

#### 1.1.3 Oracle Troubleshooting Guide
**File**: `/doc/ORACLE_TROUBLESHOOTING.md`
**Priority**: HIGH
**Audience**: Oracle operators, developers
**Estimated Length**: 2,000-2,500 words
**Completion Deadline**: End of Week 6

**Purpose**:
- Common oracle failures and solutions
- Debug log analysis
- Configuration validation
- Exchange API debugging
- P2P network issues

**Dependencies**:
- Testing phase complete (identify common errors)
- Debug logging implemented
- Error messages standardized

#### 1.1.4 Oracle Configuration Reference
**File**: `/doc/ORACLE_CONFIGURATION.md`
**Priority**: HIGH
**Audience**: Oracle operators
**Estimated Length**: 1,000-1,500 words
**Completion Deadline**: End of Week 6

**Purpose**:
- Complete digibyte.conf reference for oracle mode
- All configuration parameters documented
- Default values and valid ranges
- Exchange-specific configuration

**Dependencies**:
- All configuration parameters finalized
- Default values tested

---

### 1.2 Developer Documentation (INTEGRATION)

These documents are REQUIRED for DigiDollar integration:

#### 1.2.1 Oracle Integration Guide for DigiDollar
**File**: `/doc/ORACLE_INTEGRATION_GUIDE.md`
**Priority**: HIGH
**Audience**: DigiDollar developers, future integrators
**Estimated Length**: 2,000-2,500 words
**Completion Deadline**: End of Week 6

**Purpose**:
- How DigiDollar uses oracle prices
- API reference for oracle functions
- Integration examples
- DCA/ERR/Volatility integration

**Dependencies**:
- DigiDollar integration complete
- All integration points tested

#### 1.2.2 P2P Protocol Documentation
**File**: `/doc/ORACLE_P2P_PROTOCOL.md`
**Priority**: MEDIUM
**Audience**: Core developers, protocol analysts
**Estimated Length**: 1,500-2,000 words
**Completion Deadline**: End of Week 6

**Purpose**:
- P2P message format specification
- Message validation rules
- DoS protection mechanisms
- Message flow diagrams

**Dependencies**:
- P2P implementation complete
- Message formats finalized

#### 1.2.3 Oracle Architecture Overview
**File**: `/doc/ORACLE_ARCHITECTURE.md`
**Priority**: MEDIUM
**Audience**: Developers, code reviewers
**Estimated Length**: 2,500-3,000 words
**Completion Deadline**: End of Week 6

**Purpose**:
- System architecture diagrams
- Component interaction flows
- Data structure documentation
- Phase One vs Phase Two differences

**Dependencies**:
- Phase One implementation complete
- Architecture stable and tested

---

### 1.3 Testing Documentation (QUALITY ASSURANCE)

These documents track testing and quality:

#### 1.3.1 Test Coverage Report
**File**: `/doc/ORACLE_TEST_COVERAGE.md`
**Priority**: HIGH
**Audience**: QA, code reviewers
**Estimated Length**: 1,000-1,500 words
**Completion Deadline**: End of Week 6

**Purpose**:
- Unit test coverage (target: 90%+)
- Functional test scenarios
- Integration test results
- Performance benchmarks

**Dependencies**:
- All tests written and passing
- Coverage analysis complete

#### 1.3.2 Functional Test Scenarios
**File**: `/test/functional/ORACLE_TEST_SCENARIOS.md`
**Priority**: MEDIUM
**Audience**: Test engineers, QA
**Estimated Length**: 1,500-2,000 words
**Completion Deadline**: End of Week 6

**Purpose**:
- Complete functional test scenario descriptions
- Expected results for each test
- Test data and setup requirements

**Dependencies**:
- Functional tests complete

#### 1.3.3 Performance Benchmarks
**File**: `/doc/ORACLE_PERFORMANCE.md`
**Priority**: MEDIUM
**Audience**: Performance engineers, operations
**Estimated Length**: 1,000 words
**Completion Deadline**: End of Week 6

**Purpose**:
- Exchange API fetch latency measurements
- P2P message propagation times
- Bundle validation performance
- Memory usage analysis

**Dependencies**:
- Performance testing complete

---

### 1.4 Configuration Examples

These examples provide ready-to-use configurations:

#### 1.4.1 Complete digibyte.conf for Oracle Node
**File**: `/doc/examples/oracle-node-testnet.conf`
**Priority**: CRITICAL
**Completion Deadline**: End of Week 5

**Purpose**: Copy-paste testnet oracle configuration

#### 1.4.2 Complete digibyte.conf for Regular Testnet Node
**File**: `/doc/examples/regular-node-testnet.conf`
**Priority**: HIGH
**Completion Deadline**: End of Week 5

**Purpose**: Configuration for non-oracle testnet participants

#### 1.4.3 Exchange API Key Setup Examples
**File**: `/doc/examples/exchange-api-setup.md`
**Priority**: HIGH
**Completion Deadline**: End of Week 5

**Purpose**: How to obtain API keys from each exchange

---

## 2. Testnet Reset Procedures Analysis

### 2.1 Complete 12-Step Procedure Outline

Based on Section 7 of Phase One Spec, the testnet reset guide MUST include:

#### Pre-Reset Phase

**Pre-Reset Checklist**:
```markdown
## Pre-Reset Checklist

Before resetting testnet, ensure:

- [ ] All testnet participants notified (24-hour notice)
- [ ] Testnet oracle operator identified and confirmed
- [ ] Exchange API keys obtained (Binance, CoinMarketCap, etc.)
- [ ] DigiByte Core v8.26 built with oracle support
- [ ] Oracle private key generated and securely stored
- [ ] Backup wallet.dat if needed (testnet coins usually worthless)
- [ ] All testnet nodes accessible via SSH/terminal
- [ ] Estimated downtime: 30-60 minutes
```

**What Needs Backup**:
```markdown
## Data to Backup (Optional)

### Critical (Always Backup):
- `/home/user/.digibyte/digibyte.conf` - Configuration file
- Oracle private key (if previously generated)

### Optional (Usually Not Needed for Testnet):
- `/home/user/.digibyte/testnet4/wallet.dat` - Testnet wallet
- `/home/user/.digibyte/testnet4/wallets/` - Named wallets
- DigiDollar testing transaction history (export if needed)

### Do NOT Backup (Will Be Deleted):
- `/home/user/.digibyte/testnet4/blocks/` - Old blockchain
- `/home/user/.digibyte/testnet4/chainstate/` - Old UTXO set
- `/home/user/.digibyte/testnet4/indexes/` - Old indexes
```

**Nodes to Stop**:
```markdown
## Nodes to Stop

### Oracle Node (Priority 1):
```bash
# Stop oracle node FIRST
ssh oracle-node.testnet
digibyte-cli -testnet stop
# Wait 10 seconds for clean shutdown
```

### Regular Testnet Nodes (Priority 2):
```bash
# Stop all other testnet nodes
for node in node1 node2 node3; do
  ssh $node "digibyte-cli -testnet stop"
done

# Verify all stopped
for node in oracle-node node1 node2 node3; do
  ssh $node "ps aux | grep digibyted | grep testnet"
done
# Should return no results
```
```

---

#### Reset Phase (12 Steps from Spec)

**STEP 1: Stop All Testnet Nodes**
```markdown
## Step 1: Stop All Testnet Nodes

### Commands:
```bash
# On each testnet node:
digibyte-cli -testnet stop

# Wait for clean shutdown
sleep 10

# Verify process stopped
ps aux | grep digibyted | grep testnet
# Should return nothing
```

### Verification:
- [ ] No digibyted processes running with -testnet flag
- [ ] All RPC connections closed
- [ ] Clean shutdown logged in debug.log
```

**STEP 2: Backup Wallet Data**
```markdown
## Step 2: Backup Wallet Data (Optional)

### Why:
Testnet coins are usually valueless, but backup if:
- Testing specific wallet features
- Have important testnet transaction history
- Want to preserve DigiDollar test positions

### Commands:
```bash
# Backup wallet
cp ~/.digibyte/testnet4/wallet.dat \
   ~/testnet_wallet_backup_$(date +%Y%m%d_%H%M%S).dat

# Backup configuration
cp ~/.digibyte/digibyte.conf \
   ~/digibyte_conf_backup_$(date +%Y%m%d_%H%M%S).conf

# Verify backups exist
ls -lh ~/*backup*.{dat,conf}
```

### Verification:
- [ ] Wallet backup created with correct timestamp
- [ ] Configuration backup created
- [ ] Backup files are non-zero size
```

**STEP 3: Delete Blockchain Data**
```markdown
## Step 3: Delete Testnet Blockchain Data

### CRITICAL WARNING:
This will DELETE ALL testnet blockchain data. Make sure you are in testnet4 directory, NOT mainnet!

### Commands:
```bash
# Verify you're in testnet4 directory (CRITICAL!)
pwd
# Should show: /home/user/.digibyte/testnet4

# Delete blockchain data
cd ~/.digibyte/testnet4
rm -rf blocks/
rm -rf chainstate/
rm -rf indexes/

# Delete mempool and peer data
rm -f mempool.dat
rm -f peers.dat
rm -f banlist.dat

# Delete logs
rm -f debug.log

# Verify deletion
ls -la
# Should NOT see: blocks/, chainstate/, indexes/
```

### What Gets Deleted:
- **blocks/** - All testnet blocks (blockchain history)
- **chainstate/** - UTXO set (all unspent outputs)
- **indexes/** - Transaction indexes
- **mempool.dat** - Pending transactions
- **peers.dat** - Known peer addresses
- **debug.log** - Old debug logs

### What Gets KEPT:
- **wallet.dat** - Testnet wallet (will have zero balance on new chain)
- **wallets/** - Named wallets
- **digibyte.conf** - Configuration (in parent directory)
- **settings.json** - GUI settings

### Verification:
- [ ] blocks/ directory deleted
- [ ] chainstate/ directory deleted
- [ ] wallet.dat still exists (if you want to keep it)
```

**STEP 4: Update to DigiByte v8.26**
```markdown
## Step 4: Update DigiByte Core to v8.26

### Prerequisites:
- Git access to digibyte repository
- Build dependencies installed
- libcurl development library (for exchange APIs)

### Commands:
```bash
# Navigate to source directory
cd ~/code/digibyte

# Ensure on correct branch
git checkout feature/digidollar-v1
git pull origin feature/digidollar-v1

# Verify oracle code present
ls src/oracle/
# Should see: bundle_manager.cpp, exchange.cpp, node.cpp, etc.

# Clean previous build
make clean

# Configure with oracle support
./autogen.sh
./configure --with-curl --enable-tests --enable-debug

# Build (use all CPU cores)
make -j$(nproc)

# Run tests to verify build
make check

# Install (optional, or run from build directory)
sudo make install
```

### Verification:
- [ ] Build completes without errors
- [ ] Tests pass (make check)
- [ ] Oracle code present in src/oracle/
- [ ] libcurl linked (ldd src/digibyted | grep curl)
- [ ] digibyted --version shows v8.26
```

**STEP 5: Configure Oracle Node**
```markdown
## Step 5: Configure Oracle Node (Testnet Oracle Only)

### Who Should Do This:
ONLY the designated testnet oracle operator. Regular testnet nodes skip this step.

### Edit Configuration:
```bash
nano ~/.digibyte/digibyte.conf
```

### Add Oracle Configuration:
```ini
# Testnet configuration
testnet=1
server=1

# Oracle configuration (TESTNET ORACLE ONLY!)
oracle=1
oracleprivkey=<TESTNET_ORACLE_PRIVATE_KEY>

# Exchange API configuration
oracleexchanges=binance,coinmarketcap,coingecko,coinbase,kraken,messari,kucoin,cryptocom

# Exchange API keys
cmcapikey=<COINMARKETCAP_API_KEY>
# Add other API keys as needed (see ORACLE_OPERATOR_GUIDE.md)

# Oracle behavior
oraclefetchinterval=60        # Fetch prices every 60 seconds
oraclebroadcastinterval=60    # Broadcast every 60 seconds
oracleminexchanges=5          # Require 5 of 8 exchanges minimum

# Logging
debug=oracle
logips=1
```

### Generate Oracle Keys (One-Time Setup):
```bash
# Generate new testnet address
digibyte-cli -testnet getnewaddress "testnet-oracle" "bech32"
# Returns: dgbt1q...

# Dump private key (WIF format)
digibyte-cli -testnet dumpprivkey dgbt1q...
# Returns: cT... (private key in WIF format)

# Record this private key securely!
# Add to oracleprivkey in digibyte.conf
```

### Verification:
- [ ] oracle=1 in digibyte.conf
- [ ] oracleprivkey set to valid WIF private key
- [ ] At least one exchange API key configured
- [ ] Configuration file saved
```

**STEP 6: Generate Oracle Keys**
```markdown
## Step 6: Generate Oracle Keys (Developer Task)

### Extract Public Key for Chainparams:
```bash
# Get public key from private key
digibyte-cli -testnet getaddressinfo dgbt1q...

# Output includes:
# "pubkey": "02abcdef..." <- Use this in chainparams.cpp
```

### Update chainparams.cpp:
```cpp
// File: src/chainparams.cpp

class CTestNetParams : public CChainParams {
    consensus.vOracleNodes = {
        // Testnet oracle (Phase One: single oracle)
        {"testnet-oracle.digibyte.org", "02abcdef..."} // <-- Update pubkey here
    };

    // Phase One: Single oracle consensus
    consensus.nOracleThreshold = 1;  // 1-of-1 for testnet
    consensus.nOracleEpochBlocks = 1440;  // ~6 hours (not used in Phase One)
    consensus.nOracleUpdateInterval = 4;   // Update every 4 blocks (~1 min)
};
```

### Rebuild After Chainparams Change:
```bash
# Chainparams changes require rebuild
make clean
make -j$(nproc)

# Reinstall
sudo make install
```

### Verification:
- [ ] Oracle public key added to chainparams.cpp
- [ ] Rebuild successful
- [ ] Public key matches private key in oracle config
```

**STEP 7: Update Genesis Block (If Needed)**
```markdown
## Step 7: Update Genesis Block (Usually Not Needed)

### When to Update Genesis:
- Changing testnet genesis timestamp
- Resetting testnet block height to 0
- Changing testnet magic bytes (rare)

### Most Resets Do NOT Need This
For Phase One, you likely do NOT need to update genesis. Skip unless:
- You want a specific genesis timestamp
- You're creating a completely new testnet

### If Genesis Update Needed:
```cpp
// File: src/chainparams.cpp

genesis = CreateGenesisBlock(
    1735689600,  // nTime (2025-01-01 00:00:00 UTC)
    2083236893,  // nNonce
    0x1e0ffff0,  // nBits
    1,           // nVersion
    72000 * COIN // genesisReward
);

consensus.hashGenesisBlock = genesis.GetHash();
```

### Verification:
- [ ] Genesis timestamp updated (if needed)
- [ ] Rebuild successful
- [ ] Hash genesis block matches
```

**STEP 8: Start Oracle Node**
```markdown
## Step 8: Start Oracle Node

### Start Oracle Node First:
```bash
# Start in daemon mode
digibyted -testnet -daemon

# Wait 5 seconds for startup
sleep 5

# Check status
digibyte-cli -testnet getblockcount
# Should return 0 (fresh chain)

# Check oracle daemon started
tail -f ~/.digibyte/testnet4/debug.log | grep -i oracle
# Should see: "Oracle daemon started"
```

### Verify Oracle is Broadcasting:
```bash
# Check oracle status after 60 seconds
tail -100 ~/.digibyte/testnet4/debug.log | grep -E "Oracle|ORACLE"

# Should see logs like:
# "Fetching prices from exchanges..."
# "Binance price: $0.01234"
# "Median price: $0.01234"
# "Oracle price broadcast: 1234 cents"
```

### Verification:
- [ ] digibyted running (-testnet flag)
- [ ] Block count is 0 (fresh chain)
- [ ] Oracle daemon started (log message)
- [ ] Oracle fetching prices from exchanges (log messages)
- [ ] Oracle broadcasting price messages (log messages)
```

**STEP 9: Connect Peer Nodes**
```markdown
## Step 9: Connect Peer Nodes

### Start Other Testnet Nodes:
```bash
# On each peer node:
digibyted -testnet -daemon -addnode=<ORACLE_NODE_IP>:12025

# Example:
digibyted -testnet -daemon -addnode=192.168.1.100:12025
```

### Verify Connections:
```bash
# Check peer connections
digibyte-cli -testnet getpeerinfo

# Should see oracle node in peer list
# Look for: "addr": "<ORACLE_NODE_IP>:12025"

# Check peer count
digibyte-cli -testnet getconnectioncount
# Should be > 0
```

### Verification:
- [ ] All peer nodes started
- [ ] Peers connected to oracle node
- [ ] Oracle node sees peer connections (getconnectioncount > 0)
```

**STEP 10: Mine Initial Blocks**
```markdown
## Step 10: Mine Initial Blocks

### Generate Blocks (Oracle Node):
```bash
# Generate 200 blocks to oracle node
digibyte-cli -testnet generatetoaddress 200 <TESTNET_ADDRESS>

# Wait for generation (takes ~10 seconds)

# Check block count
digibyte-cli -testnet getblockcount
# Should return 200
```

### Verify Oracle Bundles in Blocks:
```bash
# Check block 10 for oracle bundle
digibyte-cli -testnet getblock \
  $(digibyte-cli -testnet getblockhash 10) 2 | jq '.tx[0].vout'

# Should see OP_RETURN output with oracle data
# vout[1] should contain oracle bundle

# Extract oracle price from block 10
digibyte-cli -testnet getblock \
  $(digibyte-cli -testnet getblockhash 10) 2 | \
  jq '.tx[0].vout[1].scriptPubKey.hex' | \
  xxd -r -p | strings | grep -i price
```

### Verification:
- [ ] 200 blocks mined
- [ ] Coinbase maturity reached (8 blocks)
- [ ] Oracle bundles present in blocks (vout[1] OP_RETURN)
- [ ] Oracle price in bundles is reasonable ($0.01-$1.00 for DGB)
```

**STEP 11: Verify Oracle Integration**
```markdown
## Step 11: Verify Oracle Integration

### Check Oracle Price Cache:
```bash
# Get current oracle price
digibyte-cli -testnet getoracleprice

# Should return something like:
# {
#   "price_cents": 1234,
#   "price_usd": 0.01234,
#   "timestamp": 1735690000,
#   "block_height": 200,
#   "source": "oracle_bundle"
# }
```

### Test DigiDollar Mint with Oracle Price:
```bash
# Create test DigiDollar mint (1000 DD, 1-year lock)
digibyte-cli -testnet createdigidollar 1000 <RECIPIENT_ADDRESS> 365

# This should use the oracle price (NOT mock price)

# Get transaction details
digibyte-cli -testnet getrawtransaction <TXID> 1

# Verify transaction has oracle price in metadata
digibyte-cli -testnet decoderawtransaction <RAW_TX> | \
  jq '.vout[] | select(.scriptPubKey.type == "nulldata")'
```

### Verify DCA System Uses Oracle Price:
```bash
# Check system health calculation
digibyte-cli -testnet getdigidollarsysteminfo

# Should show:
# "oracle_price_used": true  (NOT false!)
# "system_health": <percentage based on real price>
```

### Verification:
- [ ] getoracleprice returns valid price
- [ ] DigiDollar mint uses oracle price
- [ ] DCA system uses oracle price (not mock)
- [ ] Oracle price updates every ~1 minute (4 blocks)
```

**STEP 12: Distribute Testnet Coins**
```markdown
## Step 12: Distribute Testnet Coins

### Send Testnet DGB to Participants:
```bash
# Oracle node has 200 blocks of rewards = 200 * 72000 = 14,400,000 DGB

# Send to each testnet participant
digibyte-cli -testnet sendtoaddress <USER1_ADDRESS> 10000
digibyte-cli -testnet sendtoaddress <USER2_ADDRESS> 10000
digibyte-cli -testnet sendtoaddress <USER3_ADDRESS> 10000

# Verify sends
digibyte-cli -testnet listtransactions
```

### Create Testnet Faucet (Optional):
```markdown
Set up automated faucet for testnet coins:
1. Create faucet web interface (simple HTML form)
2. Endpoint: POST /faucet with address
3. Sends 1,000 testnet DGB per request
4. Rate limit: 1 request per address per day
```

### Verification:
- [ ] Testnet participants receive DGB
- [ ] Participants can mint DigiDollars
- [ ] Testnet is operational and accessible
```

---

### 2.2 Post-Reset Verification

```markdown
## Post-Reset Verification Checklist

### Blockchain Health:
- [ ] Block count increasing (new blocks being mined)
- [ ] All peer nodes synchronized
- [ ] No orphan blocks or reorganizations

### Oracle System:
- [ ] Oracle broadcasting prices every ~60 seconds
- [ ] Oracle bundles in every block (vout[1] of coinbase)
- [ ] Oracle price reasonable ($0.01-$1.00 for DGB)
- [ ] Exchange APIs responding (check logs)
- [ ] Median calculation working (5+ exchanges)

### DigiDollar Integration:
- [ ] getoracleprice returns valid price
- [ ] DigiDollar mint transactions validate
- [ ] DigiDollar redeem transactions validate
- [ ] DCA system uses oracle price (not mock)
- [ ] ERR system uses oracle price
- [ ] Volatility monitoring active

### Network Health:
- [ ] Peer connections stable (getconnectioncount > 0)
- [ ] Oracle messages propagating to peers
- [ ] Block propagation normal (< 5 seconds)
- [ ] No excessive memory usage
- [ ] No error messages in debug.log

### Testing Readiness:
- [ ] Testnet participants have DGB for testing
- [ ] DigiDollar test transactions possible
- [ ] All integration points accessible
- [ ] Documentation available for testers
```

---

### 2.3 Troubleshooting Section (Preview)

This section previews common issues that will be detailed in ORACLE_TROUBLESHOOTING.md:

```markdown
## Common Testnet Reset Issues

### Issue 1: Oracle Not Broadcasting Prices

**Symptoms**:
- No "Oracle price broadcast" messages in debug.log
- getoracleprice returns error
- Blocks have no oracle bundle

**Diagnosis**:
```bash
# Check oracle configuration
grep -i oracle ~/.digibyte/digibyte.conf

# Should see:
# oracle=1
# oracleprivkey=cT...

# Check oracle daemon started
tail -100 ~/.digibyte/testnet4/debug.log | grep -i "oracle daemon"

# Check exchange API responses
tail -200 ~/.digibyte/testnet4/debug.log | grep -i -E "binance|coinbase|kraken"
```

**Solutions**:
1. Verify oracle=1 in digibyte.conf
2. Verify oracleprivkey is valid WIF private key
3. Verify at least one exchange API key configured
4. Restart oracle node: `digibyte-cli -testnet stop && digibyted -testnet -daemon`
5. Check exchange API keys are valid (test manually with curl)

---

### Issue 2: Blocks Missing Oracle Bundles

**Symptoms**:
- Blocks mine successfully
- But vout[1] has no OP_RETURN oracle data
- getoracleprice returns "no oracle data"

**Diagnosis**:
```bash
# Check if oracle is enabled in chainparams
grep -n "fOracleEnabled" src/chainparams.cpp

# Should return: fOracleEnabled = true; (for testnet)

# Check block structure
digibyte-cli -testnet getblock $(digibyte-cli -testnet getblockhash 10) 2 | \
  jq '.tx[0].vout | length'

# Should be at least 2 (vout[0] = miner reward, vout[1] = oracle bundle)
```

**Solutions**:
1. Verify chainparams.cpp has oracle enabled for testnet
2. Rebuild if chainparams changed: `make clean && make -j$(nproc)`
3. Verify oracle daemon is running (Step 8)
4. Check miner is including oracle bundle (miner.cpp integration)

---

### Issue 3: DigiDollar Transactions Fail "No Oracle Price"

**Symptoms**:
- DigiDollar mint returns error: "No oracle price available"
- getoracleprice returns null or error
- Oracle bundles present in blocks

**Diagnosis**:
```bash
# Check oracle price cache
digibyte-cli -testnet getoracleprice

# Check recent blocks have oracle data
for i in {1..10}; do
  echo "Block $i:"
  digibyte-cli -testnet getblock $(digibyte-cli -testnet getblockhash $i) 2 | \
    jq '.tx[0].vout[1].scriptPubKey.type'
done

# Should all show: "nulldata" (OP_RETURN)
```

**Solutions**:
1. Wait for at least 1 block with oracle bundle (Step 10)
2. Verify oracle bundle extraction working (validation.cpp)
3. Verify OracleBundleManager caching price
4. Check debug logs for oracle price cache updates
5. Restart node to force bundle re-extraction

---

### Issue 4: Exchange API Timeouts

**Symptoms**:
- Log shows: "Binance fetch error: timeout"
- Only 1-2 exchanges responding
- Oracle price fetches fail with "insufficient exchanges"

**Diagnosis**:
```bash
# Test exchange APIs manually
curl "https://api.binance.com/api/v3/ticker/price?symbol=DGBUSDT"
curl "https://api.coinbase.com/v2/prices/DGB-USD/spot"

# Check if network allows outbound HTTPS
ping api.binance.com
```

**Solutions**:
1. Verify internet connectivity
2. Verify firewall allows outbound HTTPS (port 443)
3. Increase timeout in exchange.cpp (default: 5 seconds)
4. Use different exchanges if some are down
5. Reduce oracleminexchanges temporarily (minimum: 3)

---

### Issue 5: Peer Nodes Not Connecting

**Symptoms**:
- getconnectioncount returns 0
- Peers can't connect to oracle node
- Oracle messages not propagating

**Diagnosis**:
```bash
# Check peer connections
digibyte-cli -testnet getpeerinfo

# Check if testnet port open
netstat -an | grep 12025

# Check firewall rules
sudo ufw status | grep 12025
```

**Solutions**:
1. Open testnet port: `sudo ufw allow 12025/tcp`
2. Verify -addnode=<IP>:12025 in peer configuration
3. Check oracle node IP address is correct
4. Verify oracle node is running and synced
5. Check network connectivity between nodes

---

### Issue 6: High Memory Usage

**Symptoms**:
- digibyted using > 500MB RAM
- System swap active
- Node slow to respond

**Diagnosis**:
```bash
# Check memory usage
top -p $(pidof digibyted)

# Check database cache size
digibyte-cli -testnet getmemoryinfo
```

**Solutions**:
1. Reduce dbcache size: `dbcache=100` in digibyte.conf
2. Reduce maxconnections: `maxconnections=8`
3. Clear transaction index if not needed: `txindex=0`
4. Restart node to clear memory leaks
```

---

## 3. Oracle Operator Setup Guide Outline

### 3.1 Complete Document Structure

```markdown
# DigiDollar Oracle Operator Setup Guide

**Version**: 1.0
**Target**: DigiByte Core v8.26
**Network**: Testnet Only (Phase One)
**Estimated Setup Time**: 2-3 hours

---

## Table of Contents

1. Introduction
2. Prerequisites
3. Installation
4. Configuration
5. Exchange API Setup
6. Starting the Oracle
7. Monitoring and Maintenance
8. Security Best Practices
9. Troubleshooting
10. FAQ

---

## 1. Introduction

### What is an Oracle Operator?

An oracle operator runs a DigiByte node that fetches real-time DGB/USD prices from multiple exchanges and broadcasts these prices to the network. DigiDollar uses these oracle prices for minting and redemption calculations.

### Phase One: Testnet Only

**IMPORTANT**: Phase One oracle is for **testnet only**. Do not attempt to run oracle mode on mainnet.

### Responsibilities

As an oracle operator, you are responsible for:
- Maintaining 99%+ uptime
- Keeping exchange API keys secure
- Monitoring oracle health
- Responding to price feed issues
- Updating software as needed

### Rewards

**Phase One**: No rewards (testnet volunteer)
**Phase Two**: Economic rewards from DigiDollar fees

---

## 2. Prerequisites

### 2.1 Hardware Requirements

**Minimum**:
- CPU: 2 cores
- RAM: 4 GB
- Disk: 50 GB SSD
- Network: 10 Mbps upload/download

**Recommended**:
- CPU: 4 cores
- RAM: 8 GB
- Disk: 100 GB SSD
- Network: 100 Mbps upload/download

### 2.2 Software Dependencies

**Operating System**:
- Ubuntu 22.04 LTS (recommended)
- Debian 11+
- macOS 12+
- Windows 10+ (WSL2)

**Required Packages**:
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
  build-essential \
  libtool \
  autotools-dev \
  automake \
  pkg-config \
  bsdmainutils \
  python3 \
  libssl-dev \
  libevent-dev \
  libboost-all-dev \
  libminiupnpc-dev \
  libzmq3-dev \
  libcurl4-openssl-dev \
  git \
  jq \
  curl

# CRITICAL: libcurl4-openssl-dev required for exchange APIs
```

### 2.3 Network Requirements

**Firewall Rules**:
```bash
# Open testnet P2P port
sudo ufw allow 12025/tcp

# Open RPC port (local only)
sudo ufw allow from 127.0.0.1 to any port 18332
```

**Outbound HTTPS**:
- Oracle must connect to exchange APIs (port 443)
- Verify outbound HTTPS allowed:
```bash
curl -I https://api.binance.com
# Should return: HTTP/2 200
```

### 2.4 Exchange API Keys

You will need API keys from at least 5 of these exchanges:

**Required** (must have at least 3):
1. **Binance** - https://www.binance.com/en/my/settings/api-management
2. **CoinMarketCap** - https://pro.coinmarketcap.com/account
3. **CoinGecko** - https://www.coingecko.com/en/api

**Recommended** (have at least 2):
4. **Coinbase** - https://www.coinbase.com/settings/api
5. **Kraken** - https://www.kraken.com/u/security/api
6. **Messari** - https://messari.io/api

**Optional** (nice to have):
7. **KuCoin** - https://www.kucoin.com/account/api
8. **Crypto.com** - https://crypto.com/exchange/document/api

**API Key Permissions Needed**:
- **READ ONLY** - No trading permissions!
- Spot market data access
- Public price ticker access

**Security Note**: NEVER give trading permissions to oracle API keys. Read-only access only.

---

## 3. Installation

### 3.1 Clone DigiByte Repository

```bash
# Create code directory
mkdir -p ~/code
cd ~/code

# Clone repository
git clone https://github.com/digibyte/digibyte.git
cd digibyte

# Checkout oracle branch
git checkout feature/digidollar-v1

# Verify oracle code exists
ls src/oracle/
# Should see: bundle_manager.cpp, exchange.cpp, node.cpp, etc.
```

### 3.2 Build DigiByte Core

```bash
# Generate build scripts
./autogen.sh

# Configure with oracle support
./configure \
  --with-curl \
  --enable-tests \
  --enable-debug \
  --with-incompatible-bdb

# Build (use all CPU cores)
make -j$(nproc)

# Run tests to verify build
make check

# Test should pass 100%
```

### 3.3 Install Binaries

```bash
# Install globally
sudo make install

# OR run from build directory
# ./src/digibyted -testnet -daemon
# ./src/digibyte-cli -testnet <command>

# Verify installation
digibyted --version
# Should show: DigiByte Core version v8.26.0-oracle
```

---

## 4. Configuration

### 4.1 Create Configuration File

```bash
# Create .digibyte directory if not exists
mkdir -p ~/.digibyte

# Create configuration file
nano ~/.digibyte/digibyte.conf
```

### 4.2 Complete Oracle Configuration

Paste this configuration and customize:

```ini
#################
# Testnet Mode
#################
testnet=1
server=1

#################
# Oracle Configuration
#################
oracle=1
oracleprivkey=<YOUR_ORACLE_PRIVATE_KEY>

#################
# Exchange API Configuration
#################
# Specify which exchanges to use (comma-separated)
oracleexchanges=binance,coinmarketcap,coingecko,coinbase,kraken,messari,kucoin,cryptocom

# Exchange API Keys
# Binance
oracleapikey_binance=<YOUR_BINANCE_API_KEY>
oracleapisecret_binance=<YOUR_BINANCE_API_SECRET>

# CoinMarketCap (required for aggregated data)
cmcapikey=<YOUR_COINMARKETCAP_API_KEY>

# CoinGecko (optional, has free tier)
#oracleapikey_coingecko=<YOUR_COINGECKO_API_KEY>

# Coinbase (optional, public API available)
#oracleapikey_coinbase=<YOUR_COINBASE_API_KEY>

# Kraken (optional, public API available)
#oracleapikey_kraken=<YOUR_KRAKEN_API_KEY>

# Messari (optional)
#oracleapikey_messari=<YOUR_MESSARI_API_KEY>

# KuCoin
#oracleapikey_kucoin=<YOUR_KUCOIN_API_KEY>
#oracleapisecret_kucoin=<YOUR_KUCOIN_API_SECRET>

# Crypto.com
#oracleapikey_cryptocom=<YOUR_CRYPTOCOM_API_KEY>

#################
# Oracle Behavior
#################
# How often to fetch prices from exchanges (seconds)
oraclefetchinterval=60

# How often to broadcast prices to network (seconds)
oraclebroadcastinterval=60

# Minimum exchanges required for median calculation
oracleminexchanges=5

# Maximum price deviation allowed (0.05 = 5%)
oraclemaxdeviation=0.05

#################
# Performance Tuning
#################
# Database cache (MB)
dbcache=300

# Maximum connections
maxconnections=125

#################
# Logging
#################
# Enable oracle debug logging
debug=oracle
debug=net

# Log IP addresses (helps diagnose P2P issues)
logips=1

# Log timestamps
logtimestamps=1
```

**CRITICAL CUSTOMIZATION REQUIRED**:
1. Replace `<YOUR_ORACLE_PRIVATE_KEY>` with your testnet oracle WIF private key
2. Replace all `<YOUR_*_API_KEY>` with your actual API keys
3. Remove comment `#` from exchanges you configured

### 4.3 Generate Oracle Keys (One-Time)

If you don't have an oracle private key yet:

```bash
# Start digibyte in testnet mode
digibyted -testnet -daemon

# Generate new address
digibyte-cli -testnet getnewaddress "testnet-oracle" "bech32"
# Returns: dgbt1q...

# Dump private key
digibyte-cli -testnet dumpprivkey dgbt1q...
# Returns: cT... (WIF format)

# Copy this private key to oracleprivkey in digibyte.conf

# Get public key (for chainparams.cpp)
digibyte-cli -testnet getaddressinfo dgbt1q... | jq -r '.pubkey'
# Returns: 02abcdef...

# Send this public key to DigiDollar developers for chainparams.cpp
```

---

## 5. Exchange API Setup

### 5.1 Binance API Key Creation

1. Go to https://www.binance.com/en/my/settings/api-management
2. Click "Create API"
3. Name: "DigiByte Oracle - Read Only"
4. **Enable Reading** (checkbox)
5. **Disable** all trading permissions
6. **Disable** withdrawal permissions
7. **IP Restriction**: Add your oracle node IP (recommended)
8. Copy API Key and Secret
9. Add to digibyte.conf:
```ini
oracleapikey_binance=YOUR_API_KEY_HERE
oracleapisecret_binance=YOUR_SECRET_HERE
```

### 5.2 CoinMarketCap API Key Creation

1. Go to https://pro.coinmarketcap.com/signup
2. Sign up for free account (basic plan sufficient)
3. Verify email
4. Go to API Keys section
5. Copy API key (no secret needed)
6. Add to digibyte.conf:
```ini
cmcapikey=YOUR_CMC_API_KEY_HERE
```

**Free Tier Limits**: 333 calls/day, 10,000 calls/month (sufficient for testnet)

### 5.3 CoinGecko API Key Creation

1. Go to https://www.coingecko.com/en/api
2. Sign up for free account
3. Go to Developer Dashboard
4. Copy API key
5. Add to digibyte.conf:
```ini
oracleapikey_coingecko=YOUR_COINGECKO_API_KEY_HERE
```

**Free Tier Limits**: 30 calls/minute (sufficient for testnet)

### 5.4 Test API Keys

Before starting oracle, test your API keys:

```bash
# Test Binance API
curl -H "X-MBX-APIKEY: YOUR_BINANCE_API_KEY" \
  "https://api.binance.com/api/v3/ticker/price?symbol=DGBUSDT"

# Should return: {"symbol":"DGBUSDT","price":"0.01234"}

# Test CoinMarketCap API
curl -H "X-CMC_PRO_API_KEY: YOUR_CMC_API_KEY" \
  "https://pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest?symbol=DGB"

# Should return JSON with DGB price

# Test CoinGecko API
curl -H "x-cg-pro-api-key: YOUR_COINGECKO_API_KEY" \
  "https://api.coingecko.com/api/v3/simple/price?ids=digibyte&vs_currencies=usd"

# Should return: {"digibyte":{"usd":0.01234}}
```

If any API test fails, verify:
- API key is correct (no extra spaces)
- API key has read permissions
- Your IP is whitelisted (if IP restriction enabled)

---

## 6. Starting the Oracle

### 6.1 Start Oracle Node

```bash
# Start in daemon mode
digibyted -testnet -daemon

# Wait 10 seconds for startup
sleep 10

# Verify running
digibyte-cli -testnet getblockcount
```

### 6.2 Verify Oracle Started

```bash
# Check oracle status in logs
tail -f ~/.digibyte/testnet4/debug.log | grep -i oracle

# You should see:
# "Oracle daemon started (ID: 1)"
# "Fetching prices from 8 exchanges..."
# "Binance price: $0.01234"
# "Median price: $0.01234"
# "Oracle price broadcast: 1234 cents"
```

### 6.3 Verify Price Fetching

After 60 seconds, check if oracle is fetching prices:

```bash
# Check last 200 log lines for exchange fetches
tail -200 ~/.digibyte/testnet4/debug.log | grep -E "Binance|CoinMarketCap|CoinGecko"

# Should see:
# "Binance price: $0.01234"
# "CoinMarketCap price: $0.01235"
# "CoinGecko price: $0.01233"
# etc.

# Check median calculation
tail -200 ~/.digibyte/testnet4/debug.log | grep "Median price"

# Should see:
# "Median price: $0.01234 (from 7 exchanges)"
```

### 6.4 Verify P2P Broadcasting

```bash
# Check if oracle is broadcasting to peers
tail -200 ~/.digibyte/testnet4/debug.log | grep "Broadcast oracle price"

# Should see:
# "Broadcast oracle price to 5 peers"

# Check peer connections
digibyte-cli -testnet getconnectioncount

# Should be > 0 (connected to other testnet nodes)
```

---

## 7. Monitoring and Maintenance

### 7.1 Monitor Oracle Health

**Create monitoring script** (`~/monitor-oracle.sh`):

```bash
#!/bin/bash

echo "=== Oracle Health Check ==="
echo ""

# Check if node running
if ! pgrep -x "digibyted" > /dev/null; then
    echo "❌ CRITICAL: Oracle node not running!"
    exit 1
fi
echo "✅ Oracle node running"

# Check block height
BLOCKS=$(digibyte-cli -testnet getblockcount 2>/dev/null)
if [ -z "$BLOCKS" ]; then
    echo "❌ CRITICAL: Cannot query node!"
    exit 1
fi
echo "✅ Block height: $BLOCKS"

# Check peer connections
PEERS=$(digibyte-cli -testnet getconnectioncount 2>/dev/null)
if [ "$PEERS" -eq 0 ]; then
    echo "⚠️  WARNING: No peer connections!"
else
    echo "✅ Peer connections: $PEERS"
fi

# Check oracle price
ORACLE_PRICE=$(digibyte-cli -testnet getoracleprice 2>/dev/null | jq -r '.price_usd')
if [ -z "$ORACLE_PRICE" ] || [ "$ORACLE_PRICE" = "null" ]; then
    echo "❌ CRITICAL: No oracle price available!"
    exit 1
fi
echo "✅ Oracle price: \$$ORACLE_PRICE"

# Check last price broadcast (in last 5 minutes)
LAST_BROADCAST=$(tail -500 ~/.digibyte/testnet4/debug.log | \
  grep "Oracle price broadcast" | tail -1)
if [ -z "$LAST_BROADCAST" ]; then
    echo "⚠️  WARNING: No recent price broadcast (last 5 min)"
else
    echo "✅ Recent broadcast: $LAST_BROADCAST"
fi

# Check exchange API responses
EXCHANGE_COUNT=$(tail -500 ~/.digibyte/testnet4/debug.log | \
  grep -E "Binance|CoinMarketCap|CoinGecko|Coinbase|Kraken|Messari|KuCoin|Crypto.com" | \
  grep -c "price:")
if [ "$EXCHANGE_COUNT" -lt 5 ]; then
    echo "⚠️  WARNING: Only $EXCHANGE_COUNT exchanges responding (need 5+)"
else
    echo "✅ Exchange responses: $EXCHANGE_COUNT"
fi

echo ""
echo "=== Health Check Complete ==="
```

**Run monitor**:
```bash
chmod +x ~/monitor-oracle.sh
~/monitor-oracle.sh
```

**Add to crontab** (run every 5 minutes):
```bash
crontab -e

# Add:
*/5 * * * * ~/monitor-oracle.sh >> ~/oracle-health.log 2>&1
```

### 7.2 Monitor Exchange API Usage

**Check API rate limits**:

```bash
# Check Binance rate limit headers
curl -v -H "X-MBX-APIKEY: YOUR_KEY" \
  "https://api.binance.com/api/v3/ticker/price?symbol=DGBUSDT" 2>&1 | \
  grep -i "x-mbx"

# CoinMarketCap credits remaining
curl -v -H "X-CMC_PRO_API_KEY: YOUR_KEY" \
  "https://pro-api.coinmarketcap.com/v1/key/info" 2>&1 | \
  jq '.data.usage'
```

### 7.3 Monitor System Resources

```bash
# CPU and memory usage
top -p $(pidof digibyted)

# Disk usage
df -h ~/.digibyte/testnet4

# Network usage
nethogs -p digibyted
```

**Expected Resource Usage**:
- CPU: 5-10% average
- RAM: 200-300 MB
- Disk: 5-10 GB (testnet blockchain)
- Network: 1-5 Mbps (minimal)

### 7.4 Backup Oracle Configuration

```bash
# Backup configuration weekly
cp ~/.digibyte/digibyte.conf ~/oracle-backup-$(date +%Y%m%d).conf

# Backup oracle private key (encrypted)
echo "YOUR_ORACLE_PRIVKEY" | gpg -c > ~/oracle-privkey.gpg

# Store encrypted backup securely (offsite)
```

---

## 8. Security Best Practices

### 8.1 Private Key Security

**CRITICAL**: Your oracle private key must be kept secure.

```bash
# Set correct file permissions
chmod 600 ~/.digibyte/digibyte.conf

# Verify
ls -l ~/.digibyte/digibyte.conf
# Should show: -rw------- (only owner can read/write)

# Never commit to git
echo "digibyte.conf" >> ~/.gitignore
```

**Offline Backup**:
1. Write oracle private key on paper
2. Store in secure location (safe, bank deposit box)
3. Never store unencrypted privkey in cloud storage

### 8.2 API Key Security

**Read-Only Keys Only**:
- NEVER give trading permissions
- NEVER give withdrawal permissions
- Enable IP whitelisting if possible

**Rotate Keys Regularly**:
```bash
# Every 90 days:
1. Generate new API keys
2. Update digibyte.conf
3. Restart oracle node
4. Revoke old API keys
```

### 8.3 Firewall Configuration

```bash
# Allow only necessary ports
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow 22/tcp      # SSH
sudo ufw allow 12025/tcp   # DigiByte testnet P2P
sudo ufw enable

# RPC should only be accessible locally (NOT externally)
# Do NOT open port 18332 to the internet
```

### 8.4 SSH Hardening

```bash
# Disable root login
sudo sed -i 's/PermitRootLogin yes/PermitRootLogin no/' /etc/ssh/sshd_config

# Use SSH keys (not passwords)
ssh-keygen -t ed25519

# Disable password authentication
sudo sed -i 's/#PasswordAuthentication yes/PasswordAuthentication no/' /etc/ssh/sshd_config

# Restart SSH
sudo systemctl restart sshd
```

### 8.5 System Updates

```bash
# Enable automatic security updates
sudo apt install unattended-upgrades
sudo dpkg-reconfigure -plow unattended-upgrades

# Manual updates weekly
sudo apt update && sudo apt upgrade -y
```

---

## 9. Troubleshooting

**See**: `/doc/ORACLE_TROUBLESHOOTING.md` for complete troubleshooting guide.

### Quick Checks:

**Oracle not broadcasting?**
```bash
# Check configuration
grep oracle=1 ~/.digibyte/digibyte.conf

# Check private key set
grep oracleprivkey ~/.digibyte/digibyte.conf

# Restart oracle
digibyte-cli -testnet stop && digibyted -testnet -daemon
```

**Exchange APIs failing?**
```bash
# Check internet connection
ping api.binance.com

# Test API keys manually (see section 5.4)

# Check logs for specific errors
tail -500 ~/.digibyte/testnet4/debug.log | grep -i error
```

**No peer connections?**
```bash
# Check port open
sudo ufw status | grep 12025

# Add peers manually
digibyte-cli -testnet addnode "testnet-peer-ip:12025" "add"
```

---

## 10. FAQ

### Q: How much does it cost to run an oracle?

**A**: Phase One (testnet) has no rewards. Costs:
- Server: $5-20/month (VPS)
- Bandwidth: Minimal (< 100 GB/month)
- Exchange API keys: Free (basic tiers)
- Total: $5-20/month

Phase Two (mainnet) will have economic rewards from DigiDollar fees.

### Q: Can I run oracle on same server as regular node?

**A**: Yes, but:
- Use separate configuration files
- Oracle node should be dedicated (not mining)
- Ensure sufficient resources (8GB RAM recommended)

### Q: What happens if my oracle goes offline?

**A**: Phase One (testnet):
- Single oracle = DigiDollar stops working on testnet
- Bring oracle back online ASAP
- No penalties (testnet)

Phase Two (mainnet with 15 oracles):
- Other oracles continue (8-of-15 consensus)
- Minimal impact if 1 oracle offline
- Extended downtime may affect rewards

### Q: How do I know if my oracle is working?

**A**: Check:
1. `getoracleprice` returns valid price
2. Logs show "Oracle price broadcast" every ~60 seconds
3. Blocks contain oracle bundles (vout[1] OP_RETURN)
4. DigiDollar mints succeed
5. Run monitoring script (section 7.1)

### Q: Can I test oracle on regtest?

**A**: No, oracle is disabled on regtest. Use testnet only.

### Q: What if an exchange API key stops working?

**A**: Oracle will continue with remaining exchanges (need 5 of 8 minimum).

1. Check logs to identify failing exchange
2. Generate new API key for that exchange
3. Update digibyte.conf
4. Restart oracle: `digibyte-cli -testnet stop && digibyted -testnet -daemon`

### Q: How often should I update DigiByte Core?

**A**: Check for updates weekly:
```bash
cd ~/code/digibyte
git fetch origin
git log HEAD..origin/feature/digidollar-v1 --oneline

# If updates available:
git pull origin feature/digidollar-v1
make clean && make -j$(nproc)
sudo make install
digibyte-cli -testnet stop
digibyted -testnet -daemon
```

### Q: What are the system requirements?

**A**: See section 2.1. Minimum: 2 CPU, 4GB RAM, 50GB SSD.

### Q: Can I run multiple oracles?

**A**: Phase One allows only ONE testnet oracle. Phase Two (mainnet) will support 15 oracles.

### Q: Where can I get help?

**A**:
- DigiByte Discord: #digidollar channel
- GitHub Issues: https://github.com/digibyte/digibyte/issues
- Telegram: @DigiByteCoin
- Email: digidollar@digibyte.org (for oracle operators)
```

---

## 4. Integration Validation Checklist

This checklist tracks Phase One completion. ALL items must be checked before declaring production-ready.

### 4.1 Complete Integration Checklist (from Orchestrator)

```markdown
# Phase One Oracle System - Integration Validation Checklist

**Last Updated**: [DATE]
**Status**: [X]/[TOTAL] Complete ([PERCENTAGE]%)

---

## Exchange API Integration (8/8 Required)

### Primary Exchanges:
- [ ] **Binance** (DGB/USDT)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] API key authentication
  - [ ] Rate limiting (10 req/min)
  - [ ] Unit tests passing (10+ tests)
  - [ ] Functional test passing
  - [ ] Error handling complete
  - [ ] Timeout handling (5 seconds)

- [ ] **CoinMarketCap** (Aggregated)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] API key authentication
  - [ ] Rate limiting
  - [ ] Unit tests passing
  - [ ] Functional test passing
  - [ ] Error handling complete

- [ ] **CoinGecko** (Aggregated)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] API key handling (optional)
  - [ ] Rate limiting
  - [ ] Unit tests passing
  - [ ] Functional test passing

- [ ] **Coinbase** (DGB/USD)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] Unit tests passing
  - [ ] Functional test passing

- [ ] **Kraken** (DGB/USD)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] Unit tests passing
  - [ ] Functional test passing

- [ ] **Messari** (Aggregated)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] API key authentication
  - [ ] Unit tests passing
  - [ ] Functional test passing

- [ ] **KuCoin** (DGB/USDT)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] API key authentication
  - [ ] Unit tests passing
  - [ ] Functional test passing

- [ ] **Crypto.com** (DGB/USD)
  - [ ] HTTP client implemented
  - [ ] JSON parsing working
  - [ ] Unit tests passing
  - [ ] Functional test passing

### Exchange Core Functionality:
- [ ] HTTP client (libcurl) with SSL/TLS
- [ ] JSON parsing (UniValue)
- [ ] Median calculation
- [ ] Outlier filtering (MAD algorithm)
- [ ] Rate limiting per exchange
- [ ] Exponential backoff on failures
- [ ] Minimum 5 of 8 exchanges required
- [ ] Error handling for all scenarios
- [ ] Unit tests: 30-40 tests passing
- [ ] Functional test: digidollar_oracle_exchange_api.py
- [ ] Code coverage: ≥90%

**Exchange API Status**: 0/8 Complete (0%)

---

## Oracle Message System (5/5 Required)

### Message Creation:
- [ ] COraclePriceMessage structure implemented
- [ ] Message hash calculation (SHA256)
- [ ] Price format: cents (100 = $1.00)
- [ ] Timestamp inclusion (Unix timestamp)
- [ ] Block height inclusion
- [ ] Nonce generation (anti-replay)
- [ ] Unit tests: 5+ tests passing

### Schnorr Signatures:
- [ ] Signature creation (64-byte Schnorr)
- [ ] Signature verification
- [ ] Public key extraction
- [ ] Invalid signature detection
- [ ] Unit tests: 5+ tests passing

### P2P Broadcasting:
- [ ] ORACLEPRICE message type (protocol.h)
- [ ] Message serialization
- [ ] Broadcast to all peers
- [ ] Message validation on receipt
- [ ] Message relay logic
- [ ] Rate limiting (max 3 msg/min per oracle)
- [ ] DoS protection (reject messages > 5 min old)
- [ ] Unit tests: 10+ tests passing
- [ ] Functional test: digidollar_oracle_p2p_broadcast.py

### Message Handler (net_processing.cpp):
- [ ] ORACLEPRICE handler (line ~5315)
- [ ] ORACLEBUNDLE handler (line ~5397)
- [ ] GETORACLES handler (line ~5463)
- [ ] Signature verification in handler
- [ ] Timestamp validation
- [ ] Relay to peers
- [ ] Code coverage: ≥95%

**Message System Status**: 0/5 Complete (0%)

---

## Oracle Bundle & Consensus (5/5 Required)

### Bundle Creation:
- [ ] COracleBundle structure implemented
- [ ] OracleBundleManager singleton
- [ ] AddOracleMessage() - add incoming messages
- [ ] TryCreateBundle() - 1-of-1 for testnet
- [ ] Median price calculation (for future 8-of-15)
- [ ] Merkle root calculation
- [ ] Bundle caching for blocks
- [ ] Unit tests: 10+ tests passing
- [ ] Code coverage: 100%

### Bundle Validation:
- [ ] Signature verification (1 signature for testnet)
- [ ] Timestamp validation (< 5 minutes old)
- [ ] Price deviation check (< 10% from previous)
- [ ] Oracle active in epoch check
- [ ] Bundle structure validation
- [ ] Unit tests: 10+ tests passing

### Consensus Logic:
- [ ] GetLatestBundle() - return cached bundle
- [ ] GetConsensusPrice() - return median price
- [ ] Phase One: 1-of-1 consensus
- [ ] Architecture expandable to 8-of-15
- [ ] Historical price tracking
- [ ] Unit tests: 5+ tests passing

**Bundle System Status**: 0/5 Complete (0%)

---

## Block Integration (6/6 Required)

### Miner Integration (miner.cpp):
- [ ] AddOracleBundleToBlock() implemented (lines 170-177)
- [ ] Get latest bundle from OracleBundleManager
- [ ] Embed bundle in coinbase OP_RETURN (vout[1])
- [ ] Serialize bundle (price, timestamp, signatures, merkle root)
- [ ] Unit tests: 5+ tests passing
- [ ] Functional test: block creation with oracle bundle

### Block Validation (validation.cpp):
- [ ] CheckBlock() - extract oracle bundle
- [ ] Validate bundle structure
- [ ] Verify signatures (1 for testnet)
- [ ] Verify price deviation (< 10%)
- [ ] Verify timestamp reasonable (< 5 min)
- [ ] Unit tests: 10+ tests passing
- [ ] Functional test: block validation with oracle bundle

### Block Acceptance:
- [ ] Accept block if bundle valid
- [ ] Reject block if bundle invalid (testnet only)
- [ ] Cache oracle price from bundle
- [ ] Update price history
- [ ] Unit tests: 5+ tests passing
- [ ] Code coverage: ≥95%

**Block Integration Status**: 0/6 Complete (0%)

---

## DigiDollar Integration (6/6 Required)

### Oracle Price Integration:
- [ ] GetCurrentOraclePrice() implemented (/src/oracle/integration.cpp)
- [ ] GetOraclePriceForTransaction() integration (validation.cpp)
- [ ] Price caching from oracle bundles
- [ ] Fallback to mock price if no oracle data (with warning)
- [ ] Unit tests: 5+ tests passing

### DigiDollar Transaction Validation:
- [ ] Mint transaction uses oracle price
- [ ] Redeem transaction uses oracle price
- [ ] Partial redeem uses oracle price
- [ ] ERR redemption uses oracle price
- [ ] Collateral calculation correct with oracle price
- [ ] Unit tests: 10+ tests passing
- [ ] Functional test: digidollar_oracle_integration.py

### DCA/ERR/Volatility Systems:
- [ ] DCA system health uses oracle price
- [ ] ERR adjustment uses oracle price
- [ ] Volatility monitoring uses oracle price
- [ ] Price history tracking for volatility
- [ ] TWAP calculation (if implemented)
- [ ] Unit tests: 10+ tests passing
- [ ] Code coverage: ≥95%

**DigiDollar Integration Status**: 0/6 Complete (0%)

---

## Testnet Configuration (5/5 Required)

### Chainparams Configuration:
- [ ] Testnet oracle node configured (chainparams.cpp)
- [ ] Oracle public key added
- [ ] Consensus parameters configured:
  - [ ] nOracleEpochBlocks = 1440
  - [ ] nOracleUpdateInterval = 4
  - [ ] nOracleThreshold = 1 (1-of-1)
  - [ ] nMaxPriceDeviation = 10 (10%)
- [ ] Network type checking (testnet only)
- [ ] Oracle disabled on mainnet/regtest

### Configuration File:
- [ ] Complete digibyte.conf example for oracle
- [ ] Complete digibyte.conf example for regular node
- [ ] Exchange API key configuration documented
- [ ] All parameters documented
- [ ] Default values specified

### Testnet Reset:
- [ ] Complete testnet reset procedure documented
- [ ] Pre-reset checklist complete
- [ ] 12-step procedure complete
- [ ] Post-reset verification documented
- [ ] Troubleshooting section complete
- [ ] Functional test: digidollar_oracle_testnet_reset.py

**Testnet Configuration Status**: 0/5 Complete (0%)

---

## Documentation (6/6 Required)

### Operator Documentation:
- [ ] Oracle operator setup guide complete (`/doc/ORACLE_OPERATOR_GUIDE.md`)
  - [ ] Prerequisites section
  - [ ] Installation section
  - [ ] Configuration section (with examples)
  - [ ] Exchange API setup
  - [ ] Monitoring section
  - [ ] Security best practices
  - [ ] Troubleshooting section
  - [ ] FAQ section (20+ questions)

- [ ] Testnet reset procedures complete (`/doc/TESTNET_RESET_PROCEDURES.md`)
  - [ ] Pre-reset checklist
  - [ ] 12-step procedure
  - [ ] Post-reset verification
  - [ ] Troubleshooting section
  - [ ] Tested successfully

- [ ] Oracle troubleshooting guide (`/doc/ORACLE_TROUBLESHOOTING.md`)
  - [ ] Common issues and solutions
  - [ ] Debug log analysis
  - [ ] Configuration validation
  - [ ] Exchange API debugging

- [ ] Configuration reference (`/doc/ORACLE_CONFIGURATION.md`)
  - [ ] All parameters documented
  - [ ] Default values specified
  - [ ] Valid ranges specified
  - [ ] Examples provided

### Developer Documentation:
- [ ] Oracle integration guide (`/doc/ORACLE_INTEGRATION_GUIDE.md`)
  - [ ] API reference
  - [ ] Integration examples
  - [ ] DCA/ERR/Volatility integration

- [ ] Architecture overview (`/doc/ORACLE_ARCHITECTURE.md`)
  - [ ] System diagrams
  - [ ] Component interactions
  - [ ] Data structures
  - [ ] Phase One vs Phase Two

**Documentation Status**: 0/6 Complete (0%)

---

## Test Suite (4/4 Required)

### Unit Tests:
- [ ] 50-100 unit tests implemented
- [ ] All unit tests passing (make check)
- [ ] Test coverage ≥90%
- [ ] Exchange API tests (30-40 tests)
- [ ] Oracle message tests (15-20 tests)
- [ ] Bundle consensus tests (10-15 tests)
- [ ] Block integration tests (5-10 tests)
- [ ] DigiDollar integration tests (5-10 tests)

### Functional Tests:
- [ ] 10-15 functional tests implemented
- [ ] All functional tests passing
- [ ] digidollar_oracle_exchange_api.py
- [ ] digidollar_oracle_p2p_broadcast.py
- [ ] digidollar_oracle_block_integration.py
- [ ] digidollar_oracle_integration.py
- [ ] digidollar_oracle_testnet_reset.py

### Quality Checks:
- [ ] No memory leaks (valgrind clean)
- [ ] No compiler warnings
- [ ] Follows DigiByte coding standards
- [ ] All error scenarios handled

### Performance:
- [ ] Exchange API fetch: < 5 seconds median
- [ ] P2P message propagation: < 2 seconds
- [ ] Oracle bundle validation: < 10ms
- [ ] Memory usage: < 50MB for oracle system

**Test Suite Status**: 0/4 Complete (0%)

---

## Production Readiness (10/10 Required)

### Functional Validation:
- [ ] Oracle fetches real prices from 5+ exchanges
- [ ] Prices update every 4 blocks (~1 minute)
- [ ] P2P messages broadcast to all peers
- [ ] Oracle bundles in coinbase transactions
- [ ] DigiDollar mint validates with oracle price
- [ ] DigiDollar redeem validates with oracle price
- [ ] DCA system uses oracle price
- [ ] Testnet can be reset and re-initialized
- [ ] All integration points verified
- [ ] End-to-end testing complete

### Quality Validation:
- [ ] All unit tests passing (50-100 tests)
- [ ] All functional tests passing (10-15 tests)
- [ ] Code coverage ≥90%
- [ ] No memory leaks
- [ ] No compiler warnings
- [ ] Performance benchmarks met
- [ ] Code review complete
- [ ] Security review complete

### Documentation Validation:
- [ ] All operator documentation complete
- [ ] All developer documentation complete
- [ ] All configuration examples provided
- [ ] All troubleshooting guides complete
- [ ] FAQ sections complete (50+ total questions)

### Deployment Validation:
- [ ] Testnet oracle operational
- [ ] Regular testnet nodes connected
- [ ] DigiDollar operations verified
- [ ] Reset procedures tested
- [ ] Monitoring tools operational

**Production Readiness**: 0/10 Complete (0%)

---

## OVERALL STATUS

**Total Components**: 40
**Components Complete**: 0
**Completion**: 0%

**READY FOR TESTNET DEPLOYMENT**: ❌ NO

**Blocking Issues**:
1. Exchange API integration not started
2. Oracle message system not implemented
3. Bundle consensus not implemented
4. Block integration not complete
5. DigiDollar integration not verified
6. Tests not written
7. Documentation not written

**Next Steps**:
1. Deploy Week 1 sub-agents (Foundation analysis)
2. Deploy Week 2 Exchange Integration Engineers
3. Begin TDD red-green-refactor cycle
4. Track progress in this checklist daily
```

---

## 5. Documentation Standards

### 5.1 Markdown Format Standards

All documentation must follow these standards:

```markdown
# Document Format Requirements

## File Structure:
1. Title (H1)
2. Metadata (version, date, status, etc.)
3. Table of contents (for documents > 1000 words)
4. Sections (H2)
5. Subsections (H3, H4 as needed)

## Code Examples:
- Always use syntax highlighting (```cpp, ```bash, ```ini)
- Include comments explaining key lines
- Show complete, runnable examples
- Include expected output

## Commands:
- Prefix with $ for regular user commands
- Prefix with # for root commands
- Show complete command with all flags
- Include example output in code block

## File Paths:
- Always use absolute paths: `/home/user/.digibyte/digibyte.conf`
- NOT relative paths: `~/.digibyte/digibyte.conf` (except in examples)
- Specify for Linux/macOS vs Windows where different

## Links:
- Use absolute GitHub links (with line numbers for code)
- External links should be complete URLs
- Internal links use relative paths

## Formatting:
- **Bold** for emphasis and warnings
- `code` for commands, file names, parameters
- > blockquotes for important notes
- Lists for steps and checklists
- Tables for comparisons

## Warnings:
Use consistent warning format:

**CRITICAL**: For data loss or security issues
**WARNING**: For important but non-critical issues
**NOTE**: For helpful information
**TIP**: For best practices
```

### 5.2 Documentation Review Checklist

Before finalizing any documentation:

```markdown
# Documentation Quality Checklist

## Technical Accuracy:
- [ ] All commands tested and verified
- [ ] All file paths correct
- [ ] All configuration examples valid
- [ ] All code examples compile/run
- [ ] All links working (no 404s)

## Completeness:
- [ ] All sections outlined are present
- [ ] All prerequisites listed
- [ ] All steps numbered sequentially
- [ ] All verification steps included
- [ ] All troubleshooting scenarios covered

## Clarity:
- [ ] Written for target audience level
- [ ] Technical jargon explained
- [ ] Acronyms defined on first use
- [ ] Steps are clear and unambiguous
- [ ] Examples illustrate concepts

## Consistency:
- [ ] Terminology consistent throughout
- [ ] Code style consistent
- [ ] Formatting consistent
- [ ] Voice and tone consistent

## Usability:
- [ ] Table of contents for long documents
- [ ] Clear section headings
- [ ] Adequate white space
- [ ] Code blocks with syntax highlighting
- [ ] Visual aids where helpful (diagrams, screenshots)

## SEO & Findability:
- [ ] Descriptive title
- [ ] Clear section headings
- [ ] Searchable keywords included
- [ ] Cross-references to related docs
```

---

## 6. FAQ Section Requirements

### 6.1 FAQ Categories and Questions

Based on Phase One Spec and implementation, FAQs must cover:

#### Category 1: Setup and Installation (15 questions)

```markdown
## Setup and Installation

### Q1: What are the minimum system requirements to run an oracle?
**A**: Minimum: 2 CPU cores, 4GB RAM, 50GB SSD, 10 Mbps network.
Recommended: 4 CPU cores, 8GB RAM, 100GB SSD, 100 Mbps network.

### Q2: Can I run oracle on Windows?
**A**: Use WSL2 (Windows Subsystem for Linux). Native Windows builds not officially supported for oracle mode.

### Q3: Do I need to build from source or can I use binaries?
**A**: Phase One requires building from source (feature/digidollar-v1 branch). No official binaries yet.

### Q4: What is libcurl and why do I need it?
**A**: libcurl is an HTTP client library required for connecting to exchange APIs. Install: `sudo apt install libcurl4-openssl-dev`

### Q5: How do I verify my build includes oracle support?
**A**: Check for oracle files:
```bash
ls src/oracle/
# Should see: bundle_manager.cpp, exchange.cpp, node.cpp, etc.
```

### Q6: Can I run oracle on a VPS or does it need to be dedicated hardware?
**A**: VPS is fine. Most cloud providers (AWS, DigitalOcean, Linode) work well.

### Q7: How much bandwidth does oracle use?
**A**: Minimal. ~100MB/day for P2P + ~50MB/day for exchange APIs = ~150MB/day total.

### Q8: Do I need a static IP address?
**A**: Not required, but recommended for stable peer connections.

### Q9: Can I run oracle and miner on same machine?
**A**: Not recommended. Oracle should be dedicated (not mining) for best performance.

### Q10: What version of DigiByte Core do I need?
**A**: v8.26 or later with oracle support (feature/digidollar-v1 branch).

### Q11: How long does it take to sync testnet blockchain?
**A**: Fresh testnet sync: 10-30 minutes (testnet is small).

### Q12: Can I use existing testnet node or do I need fresh install?
**A**: Fresh install recommended. Testnet reset deletes existing blockchain.

### Q13: Do I need to be a developer to run an oracle?
**A**: Basic command-line knowledge required. Not a developer? Follow ORACLE_OPERATOR_GUIDE.md exactly.

### Q14: Can I run oracle in Docker container?
**A**: Yes, but not officially supported in Phase One. Manual Docker setup required.

### Q15: What operating systems are supported?
**A**: Ubuntu 22.04 LTS (primary), Debian 11+, macOS 12+, Windows 10+ (WSL2).
```

#### Category 2: Configuration (15 questions)

```markdown
## Configuration

### Q16: Where is the configuration file located?
**A**: `~/.digibyte/digibyte.conf` (Linux/macOS) or `%APPDATA%\DigiByte\digibyte.conf` (Windows)

### Q17: What is the difference between oracle=1 and oracle=0?
**A**: `oracle=1` enables oracle mode (fetch and broadcast prices). `oracle=0` or omitted = regular node.

### Q18: Can I run oracle on mainnet?
**A**: NO. Phase One is testnet only. Oracle disabled on mainnet.

### Q19: How do I generate an oracle private key?
**A**: Use digibyte-cli:
```bash
digibyte-cli -testnet getnewaddress "oracle" "bech32"
digibyte-cli -testnet dumpprivkey <ADDRESS>
```

### Q20: What format should the oracle private key be in?
**A**: WIF (Wallet Import Format). Starts with 'c' for testnet (e.g., cT...).

### Q21: Can I use the same private key as my wallet?
**A**: Yes, but not recommended. Use dedicated oracle key for security.

### Q22: How many exchange API keys do I need?
**A**: Minimum 5 exchanges required. Recommended: all 8.

### Q23: Are exchange API keys free?
**A**: Most have free tiers sufficient for testnet:
- Binance: Free
- CoinMarketCap: Free (333 calls/day)
- CoinGecko: Free (30 calls/min)
- Coinbase: Free
- Kraken: Free

### Q24: Do I need trading permissions for API keys?
**A**: NO. **Read-only permissions only**. Never enable trading or withdrawals.

### Q25: How often should I rotate API keys?
**A**: Every 90 days recommended for security.

### Q26: What is oraclefetchinterval?
**A**: How often (in seconds) to fetch prices from exchanges. Default: 60 seconds.

### Q27: What is oracleminexchanges?
**A**: Minimum number of exchanges that must respond to create valid median. Default: 5.

### Q28: Can I customize which exchanges to use?
**A**: Yes. Set `oracleexchanges=binance,coinmarketcap,coingecko,...` (comma-separated).

### Q29: What is the recommended dbcache size for oracle?
**A**: 300-500 MB. Set `dbcache=300` in digibyte.conf.

### Q30: Should I enable txindex for oracle?
**A**: Not required for oracle operation. Disable to save disk space: `txindex=0`.
```

#### Category 3: Exchange APIs (10 questions)

```markdown
## Exchange APIs

### Q31: Which exchanges are supported?
**A**: Binance, CoinMarketCap, CoinGecko, Coinbase, Kraken, Messari, KuCoin, Crypto.com (8 total).

### Q32: Why does oracle need multiple exchanges?
**A**: Redundancy and manipulation resistance. Median of 5-8 exchanges prevents single-source manipulation.

### Q33: What if one exchange API goes down?
**A**: Oracle continues with remaining exchanges (need 5 minimum).

### Q34: How do I test if my API keys work?
**A**: Use curl to test manually:
```bash
curl -H "X-MBX-APIKEY: YOUR_KEY" \
  "https://api.binance.com/api/v3/ticker/price?symbol=DGBUSDT"
```

### Q35: What are rate limits and why do they matter?
**A**: Rate limits prevent excessive API calls. Oracle respects limits automatically (10 calls/min per exchange).

### Q36: What happens if I exceed rate limits?
**A**: Oracle waits before next call. No permanent issues, but temporary delays possible.

### Q37: Do I need paid API plans for testnet?
**A**: No. Free tiers sufficient for testnet (60-second update interval).

### Q38: Can I use a VPN with exchange APIs?
**A**: Yes, but some exchanges block VPNs. Test API connectivity first.

### Q39: What if DGB is delisted from an exchange?
**A**: Oracle automatically skips non-responsive exchanges. Update configuration to remove delisted exchange.

### Q40: How does oracle handle different price pairs (DGB/USD vs DGB/USDT)?
**A**: Treats USDT as equivalent to USD. Median calculation normalizes all prices.
```

#### Category 4: Operation (10 questions)

```markdown
## Operation

### Q41: How do I start the oracle?
**A**: `digibyted -testnet -daemon` (ensure oracle=1 in config)

### Q42: How do I stop the oracle?
**A**: `digibyte-cli -testnet stop`

### Q43: How can I tell if oracle is broadcasting?
**A**: Check logs:
```bash
tail -f ~/.digibyte/testnet4/debug.log | grep "Oracle price broadcast"
```

### Q44: How often should oracle broadcast prices?
**A**: Every ~60 seconds (configurable with oraclebroadcastinterval).

### Q45: What is a reasonable oracle price for DGB?
**A**: $0.005 - $0.10 historically. Anything outside this range warrants investigation.

### Q46: How do I check the current oracle price?
**A**: `digibyte-cli -testnet getoracleprice`

### Q47: Can oracle run 24/7 unattended?
**A**: Yes. Set up monitoring (section 7.1 in ORACLE_OPERATOR_GUIDE.md) and automated restarts.

### Q48: What happens if oracle crashes?
**A**: DigiDollar stops working on testnet (single oracle). Restart ASAP. Use systemd to auto-restart.

### Q49: How much CPU does oracle use?
**A**: 5-10% average. Spikes to 20-30% during price fetches.

### Q50: How much disk space does testnet use?
**A**: 5-10 GB for blockchain, ~100 MB for oracle logs.
```

#### Category 5: Troubleshooting (5 questions)

```markdown
## Troubleshooting

### Q51: Oracle not broadcasting - where do I start?
**A**: See ORACLE_TROUBLESHOOTING.md. Quick check:
1. `grep oracle=1 ~/.digibyte/digibyte.conf`
2. `grep oracleprivkey ~/.digibyte/digibyte.conf`
3. Restart: `digibyte-cli -testnet stop && digibyted -testnet -daemon`

### Q52: How do I read debug logs?
**A**: `tail -f ~/.digibyte/testnet4/debug.log` (real-time) or `less ~/.digibyte/testnet4/debug.log` (historical)

### Q53: What do I do if all exchange APIs fail?
**A**:
1. Check internet connection: `ping api.binance.com`
2. Verify firewall allows outbound HTTPS (port 443)
3. Test API keys manually
4. Check exchange API status pages

### Q54: Oracle price is stuck/not updating - why?
**A**:
1. Check if oracle daemon is running
2. Verify peers connected: `digibyte-cli -testnet getconnectioncount`
3. Check last broadcast time in logs
4. Verify blocks being mined

### Q55: Where can I get help?
**A**:
- DigiByte Discord: #digidollar channel
- GitHub Issues: https://github.com/digibyte/digibyte/issues
- Telegram: @DigiByteCoin
- Email: digidollar@digibyte.org
```

---

## 7. Implementation Timeline

### 7.1 Documentation Writing Schedule

```markdown
# Documentation Implementation Timeline

## Week 1 (Days 1-5): Planning Phase
**Status**: Current task
**Deliverables**:
- [x] Complete documentation inventory (this document)
- [x] Testnet reset procedures outline
- [x] Oracle operator setup guide outline
- [x] Integration validation checklist
- [x] Documentation standards defined
- [x] FAQ requirements specified

**Hours**: 8-10 hours

---

## Week 5 (Days 22-26): Critical Documentation

### Day 22-23: Testnet Reset Procedures
**File**: `/doc/TESTNET_RESET_PROCEDURES.md`
**Status**: Not started
**Estimated Hours**: 6-8 hours
**Dependencies**:
- Testnet configuration complete (chainparams.cpp)
- Oracle daemon tested on testnet
**Tasks**:
- [ ] Write complete 12-step procedure
- [ ] Document pre-reset checklist
- [ ] Document post-reset verification
- [ ] Write troubleshooting section
- [ ] Test procedure on clean testnet
- [ ] Update based on test results

### Day 24-25: Oracle Operator Setup Guide
**File**: `/doc/ORACLE_OPERATOR_GUIDE.md`
**Status**: Not started
**Estimated Hours**: 8-10 hours
**Dependencies**:
- All 8 exchange API clients working
- Oracle daemon operational
- Configuration finalized
**Tasks**:
- [ ] Write prerequisites section
- [ ] Write installation section
- [ ] Write configuration section with examples
- [ ] Write exchange API setup for all 8 exchanges
- [ ] Write monitoring section
- [ ] Write security best practices
- [ ] Write troubleshooting quick reference
- [ ] Write FAQ (20+ questions)

### Day 26: Configuration Examples
**Files**:
- `/doc/examples/oracle-node-testnet.conf`
- `/doc/examples/regular-node-testnet.conf`
**Status**: Not started
**Estimated Hours**: 2-3 hours
**Tasks**:
- [ ] Create complete oracle node config
- [ ] Create complete regular node config
- [ ] Add comments explaining each parameter
- [ ] Test configurations on testnet
- [ ] Validate all parameters work

**Week 5 Total Hours**: 16-21 hours

---

## Week 6 (Days 27-30): Comprehensive Documentation

### Day 27: Oracle Troubleshooting Guide
**File**: `/doc/ORACLE_TROUBLESHOOTING.md`
**Status**: Not started
**Estimated Hours**: 6-8 hours
**Dependencies**:
- Testing phase complete (common errors identified)
- Debug logging implemented
**Tasks**:
- [ ] Document all common failure scenarios
- [ ] Provide diagnosis steps for each issue
- [ ] Provide solutions for each issue
- [ ] Include log analysis examples
- [ ] Add configuration validation section
- [ ] Test troubleshooting steps

### Day 27: Oracle Configuration Reference
**File**: `/doc/ORACLE_CONFIGURATION.md`
**Status**: Not started
**Estimated Hours**: 3-4 hours
**Tasks**:
- [ ] Document all configuration parameters
- [ ] Specify default values
- [ ] Specify valid ranges
- [ ] Provide examples for each parameter
- [ ] Cross-reference with operator guide

### Day 28: Integration Documentation
**File**: `/doc/ORACLE_INTEGRATION_GUIDE.md`
**Status**: Not started
**Estimated Hours**: 5-6 hours
**Dependencies**:
- DigiDollar integration complete
- All integration points tested
**Tasks**:
- [ ] Document GetCurrentOraclePrice() API
- [ ] Document GetOraclePriceForTransaction() API
- [ ] Provide integration examples
- [ ] Document DCA/ERR/Volatility integration
- [ ] Provide code snippets
- [ ] Reference actual implementation files with line numbers

### Day 29: Architecture Documentation
**File**: `/doc/ORACLE_ARCHITECTURE.md`
**Status**: Not started
**Estimated Hours**: 6-8 hours
**Tasks**:
- [ ] Create system architecture diagrams
- [ ] Document component interactions
- [ ] Document data structures
- [ ] Explain Phase One vs Phase Two differences
- [ ] Document P2P protocol
- [ ] Provide data flow diagrams

### Day 30: Final Documentation & Review
**Files**: All documentation
**Status**: Not started
**Estimated Hours**: 4-6 hours
**Tasks**:
- [ ] Complete test coverage report
- [ ] Complete performance benchmarks document
- [ ] Final review of all documentation
- [ ] Verify all links work
- [ ] Verify all code examples tested
- [ ] Integration validation checklist 100% complete
- [ ] Spell check and grammar check
- [ ] Peer review

**Week 6 Total Hours**: 24-32 hours

---

## Total Documentation Effort

**Planning (Week 1)**: 8-10 hours
**Writing (Week 5-6)**: 40-53 hours
**Total**: 48-63 hours

**Completion Deadline**: End of Week 6 (Day 30)
```

---

## Acceptance Criteria

This documentation plan is complete when:

- [x] Complete documentation inventory created (6 major docs + examples)
- [x] Testnet reset procedure outlined (complete 12-step procedure)
- [x] Oracle operator setup guide outlined (10 sections)
- [x] Integration validation checklist complete (40 components, 150+ items)
- [x] Documentation standards defined
- [x] FAQ section requirements specified (55+ questions across 5 categories)
- [x] Implementation timeline established (Week 5-6)

**Status**: ✅ COMPLETE - READY FOR WEEK 5-6 IMPLEMENTATION

---

## Appendix: Documentation File Tree

```
/doc/
├── ORACLE_OPERATOR_GUIDE.md           (2,500-3,000 words, Week 5)
├── TESTNET_RESET_PROCEDURES.md        (1,500-2,000 words, Week 5)
├── ORACLE_TROUBLESHOOTING.md          (2,000-2,500 words, Week 6)
├── ORACLE_CONFIGURATION.md            (1,000-1,500 words, Week 6)
├── ORACLE_INTEGRATION_GUIDE.md        (2,000-2,500 words, Week 6)
├── ORACLE_ARCHITECTURE.md             (2,500-3,000 words, Week 6)
├── ORACLE_TEST_COVERAGE.md            (1,000-1,500 words, Week 6)
├── ORACLE_PERFORMANCE.md              (1,000 words, Week 6)
└── examples/
    ├── oracle-node-testnet.conf        (Complete config with comments, Week 5)
    ├── regular-node-testnet.conf       (Complete config with comments, Week 5)
    └── exchange-api-setup.md           (API key setup guide, Week 5)

Total: 8 major documents + 3 configuration examples
Total Word Count: 15,000-19,500 words
Total Estimated Effort: 48-63 hours
```

---

**END OF DOCUMENTATION REQUIREMENTS ANALYSIS**

*This is a PLANNING document. Actual documentation writing will occur in Week 5-6 after implementation is complete.*
