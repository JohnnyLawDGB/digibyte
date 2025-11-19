# DigiByte Testnet Oracle Reset Procedures

## Overview

This document provides step-by-step procedures for resetting the DigiByte testnet when deploying oracle system updates. These procedures ensure a clean testnet state for testing Phase One oracle functionality.

**Phase One Configuration**:
- **Consensus**: 1-of-1 (single hardcoded oracle)
- **Network**: Testnet only
- **Oracle Count**: 1 (hardcoded)
- **Epoch Management**: No epoch rotation (single oracle always active)
- **Activation**: Controlled by chainparams configuration

**When to Reset Testnet**:
- Deploying oracle system updates
- Changing oracle consensus parameters
- Testing oracle activation from genesis
- Debugging oracle integration issues
- Existing blocks lack oracle bundles in coinbase transactions
- DigiDollar transactions on testnet used mock prices instead of oracle prices
- Validation rules changed and old blocks would fail new validation

---

## Prerequisites

Before resetting testnet, ensure you have:

1. **Backup** any important testnet data
2. **Root/sudo access** to the server (if installing system-wide)
3. **DigiByte Core v8.26** source code with oracle support
4. **Network access** to testnet peers
5. **Sufficient disk space** (~10 GB for testnet blockchain)

**Required Software**:
- DigiByte Core v8.26+ (feature/digidollar-v1 branch)
- libcurl development libraries (for exchange API integration)
- Python 3.8+ (for functional tests)
- Screen or tmux (optional, for persistent sessions)

**Required Knowledge**:
- Basic Linux/Unix command line usage
- Understanding of DigiByte testnet vs mainnet
- Familiarity with digibyte.conf configuration
- Basic git operations (for source updates)

---

## Testnet Reset Procedure

### Step 1: Stop All Testnet Nodes

Stop all running testnet nodes to ensure clean shutdown:

```bash
# Stop testnet daemon
digibyte-cli -testnet stop

# Wait for graceful shutdown
sleep 10

# Verify no processes remain
ps aux | grep digibyte | grep testnet
```

**Expected Result**: No DigiByte testnet processes running

**If processes remain**:
```bash
# Find process ID
ps aux | grep digibyted | grep testnet

# Force kill if needed (last resort)
kill -9 <PID>
```

---

### Step 2: Backup Important Data (Optional)

If you have testnet funds or custom configurations, backup before reset:

```bash
# Backup wallet (if needed)
cp ~/.digibyte/testnet4/wallet.dat ~/testnet_wallet_backup_$(date +%Y%m%d).dat

# Backup configuration
cp ~/.digibyte/digibyte.conf ~/digibyte_conf_backup_$(date +%Y%m%d).conf

# Verify backups created
ls -lh ~/*backup*.dat ~/*backup*.conf
```

**Important Notes**:
- Testnet coins have no monetary value
- Wallet backup only needed if you want to preserve testnet addresses
- Configuration backup helps restore settings if needed

---

### Step 3: Delete Testnet Blockchain Data

Remove testnet blockchain data while preserving wallet and configuration:

```bash
# Remove testnet blockchain data
rm -rf ~/.digibyte/testnet4/blocks
rm -rf ~/.digibyte/testnet4/chainstate
rm -rf ~/.digibyte/testnet4/indexes

# Remove testnet mempool and peers
rm -f ~/.digibyte/testnet4/mempool.dat
rm -f ~/.digibyte/testnet4/peers.dat
rm -f ~/.digibyte/testnet4/banlist.dat

# Remove debug logs (optional - useful for clean start)
rm -f ~/.digibyte/testnet4/debug.log

# Verify deletion
ls -la ~/.digibyte/testnet4/
```

**What gets deleted**:
- `blocks/` - All block data
- `chainstate/` - UTXO database
- `indexes/` - Transaction and block indexes
- `mempool.dat` - Unconfirmed transactions
- `peers.dat` - Known peer addresses
- `banlist.dat` - Banned peer addresses
- `debug.log` - Old log entries

**What gets preserved**:
- `wallet.dat` - Wallet keys and addresses
- `digibyte.conf` - Configuration file (in parent directory)

---

### Step 4: Update DigiByte Core to v8.26

Build the latest oracle-enabled DigiByte Core:

```bash
# Navigate to source directory
cd ~/code/digibyte

# Ensure on correct branch (feature/digidollar-v1)
git checkout feature/digidollar-v1
git pull origin feature/digidollar-v1

# Clean previous build
make clean

# Configure with libcurl support (required for exchange APIs)
./autogen.sh
./configure --with-curl --enable-tests

# Build (use all CPU cores)
make -j$(nproc)

# Optional: Install system-wide
sudo make install
```

**Verify build**:
```bash
# Check version
./src/digibyted --version

# Check for oracle symbols
nm ./src/digibyted | grep -i oracle
```

**Expected**: Version shows v8.26.x and oracle symbols are present

**If build fails**:
- Check for missing dependencies: `./configure --with-curl --enable-tests` will show errors
- Install libcurl: `sudo apt-get install libcurl4-openssl-dev` (Ubuntu/Debian)
- Review build errors in output

---

### Step 5: Configure Oracle Node (Testnet Oracle Only)

**For the designated testnet oracle operator:**

Edit digibyte.conf:

```bash
nano ~/.digibyte/digibyte.conf
```

Add oracle configuration:

```ini
# Testnet configuration
testnet=1
server=1

# Oracle configuration (testnet oracle only)
oracle=1
oracleprivkey=<TESTNET_ORACLE_PRIVATE_KEY>

# Exchange API keys (if required)
cmcapikey=<COINMARKETCAP_API_KEY>

# Logging (recommended for debugging)
debug=oracle
debug=digidollar
logips=1
```

**Generate testnet oracle keys (one-time setup)**:

```bash
# Start node temporarily to generate address
./src/digibyted -testnet -daemon
sleep 5

# Generate new address for oracle
digibyte-cli -testnet getnewaddress "testnet-oracle" "legacy"

# Dump private key (WIF format)
digibyte-cli -testnet dumpprivkey <ADDRESS_FROM_ABOVE>

# Record the private key for digibyte.conf
# Extract public key for chainparams.cpp (developer task)

# Stop temporary node
digibyte-cli -testnet stop
```

**For non-oracle testnet nodes**:

```ini
# Testnet configuration
testnet=1
server=1

# DO NOT set oracle=1 or oracleprivkey
# These nodes will receive and validate oracle messages

# Logging (optional)
debug=oracle
debug=digidollar
```

---

### Step 6: Update Testnet Genesis Block (Developer Task)

**This step is for core developers only. Skip if you are not modifying genesis parameters.**

In `/src/chainparams.cpp`, update testnet genesis if needed:

```cpp
// Update testnet genesis timestamp for reset
consensus.nTestnetGenesisTime = 1735689600;  // 2025-01-01 00:00:00 UTC

// Update oracle pubkey (if changing oracle)
consensus.vOraclePubkeys.clear();
consensus.vOraclePubkeys.push_back(ParseHex(
    "02<NEW_TESTNET_ORACLE_PUBKEY>"
));

// Ensure oracle enabled for testnet
consensus.fOracleEnabled = true;
```

**Rebuild after changing genesis**:

```bash
make clean
make -j$(nproc)
```

**Important**: If genesis parameters change, ALL testnet nodes must rebuild and reset.

---

### Step 7: Start Testnet Network

**Start oracle node first** (if you are the oracle operator):

```bash
# Oracle node starts testnet
./src/digibyted -testnet -daemon

# Wait for initialization
sleep 5

# Check oracle is running
digibyte-cli -testnet getmininginfo
digibyte-cli -testnet getblockcount  # Should be 0 (fresh chain)

# Check oracle status in logs
tail -f ~/.digibyte/testnet4/debug.log | grep -i oracle
```

**Expected logs**:
```
Oracle: Initializing oracle system
Oracle: Loading oracle private key
Oracle: Oracle broadcasting enabled (testnet)
Oracle: Starting exchange price fetcher thread
Oracle: Oracle daemon started successfully
```

**Start other testnet nodes**:

```bash
# Other nodes connect to oracle node
./src/digibyted -testnet -daemon -addnode=<ORACLE_NODE_IP>:12025

# Verify connection
digibyte-cli -testnet getpeerinfo | grep -A 5 "addr.*12025"
```

**Expected**: Peer connection established to oracle node

---

### Step 8: Mine Initial Blocks

Generate initial blocks to establish blockchain:

```bash
# Generate mining address
TESTNET_ADDRESS=$(digibyte-cli -testnet getnewaddress)

# Mine initial blocks (oracle node)
digibyte-cli -testnet generatetoaddress 200 $TESTNET_ADDRESS

# Wait for maturity (8 blocks for DigiByte)
digibyte-cli -testnet getblockcount

# Verify oracle bundles in blocks
digibyte-cli -testnet getblock $(digibyte-cli -testnet getblockhash 10) 2 | grep -i oracle
```

**Expected Results**:
- Block count: 200
- Oracle bundles present in coinbase transactions (after activation if configured)
- Blocks show oracle price data

**Verify specific block**:
```bash
# Get block 10 details
digibyte-cli -testnet getblock $(digibyte-cli -testnet getblockhash 10) 2

# Look for oracle bundle in coinbase transaction
# Should see oracle price data in scriptPubKey or witness data
```

---

### Step 9: Verify Oracle Integration

Check oracle price cache and integration:

```bash
# Check oracle price cache
digibyte-cli -testnet getoracleprice

# Expected: Should show current DGB/USD price in micro-USD
# Example: 12340 (meaning $0.012340)
```

**Create test DigiDollar transaction** (if DigiDollar is active):

```bash
# Create DigiDollar transaction
digibyte-cli -testnet createdigidollar <AMOUNT> <RECIPIENT>

# Get transaction details
digibyte-cli -testnet getrawtransaction <TXID> 1

# Verify transaction uses oracle price (not mock)
# Look for oracle price reference in transaction data
```

**Verify oracle message relay**:

```bash
# Check oracle messages received
digibyte-cli -testnet getoraclemessages

# Should show oracle price messages from network
```

---

### Step 10: Distribute Testnet Coins

Distribute testnet coins to other users for testing:

```bash
# Send test DGB to other testnet users
digibyte-cli -testnet sendtoaddress <ADDRESS> 1000

# Verify transaction
digibyte-cli -testnet gettransaction <TXID>

# Mine block to confirm
digibyte-cli -testnet generatetoaddress 1 $(digibyte-cli -testnet getnewaddress)
```

**Optional: Create faucet for testnet coins**:
- Set up web service that dispenses testnet DGB
- Users can request testnet coins for testing
- Rate limit to prevent abuse

---

### Step 11: Run Oracle Unit Tests

Verify oracle implementation with unit tests:

```bash
# Run all oracle unit tests
./src/test/test_digibyte --run_test=oracle_*

# Run DigiDollar oracle tests
./src/test/test_digibyte --run_test=digidollar_oracle_*

# Run specific test suites
./src/test/test_digibyte --run_test=oracle_exchange_tests
./src/test/test_digibyte --run_test=oracle_bundle_tests
./src/test/test_digibyte --run_test=oracle_validation_tests
```

**Expected**: All oracle tests pass

**If tests fail**:
- Review test output for specific failures
- Check compilation included all oracle code
- Verify test data matches Phase One configuration (1-of-1 consensus)

---

### Step 12: Run Oracle Functional Tests

Run comprehensive oracle functional tests:

```bash
# Run oracle P2P relay tests
./test/functional/feature_oracle_p2p.py

# Run DigiDollar oracle integration tests
./test/functional/digidollar_oracle.py

# Run with debug logging
./test/functional/feature_oracle_p2p.py --loglevel=debug

# Run all oracle-related tests
./test/functional/test_runner.py --extended oracle
```

**Expected**: All functional tests pass

**Test coverage**:
- Oracle message creation and signing
- Oracle message P2P relay
- Oracle bundle validation
- Oracle price cache updates
- DigiDollar integration with oracle prices
- Duplicate message rejection
- Invalid signature rejection

---

## Verification Checklist

After reset, verify the following:

### Oracle System
- [ ] Oracle activation configured in chainparams
- [ ] Oracle daemon enabled in digibyte.conf (oracle node only)
- [ ] Oracle logs show initialization
- [ ] No oracle errors in debug.log
- [ ] Oracle broadcasting prices (oracle node only)
- [ ] Oracle private key loaded successfully (oracle node only)

### Exchange API
- [ ] Exchange API clients compiled (check nm output)
- [ ] API keys configured in digibyte.conf (if using real APIs)
- [ ] Exchange price fetching works (check logs)
- [ ] Median calculation working (check logs)
- [ ] Multiple exchanges queried (Binance, CoinGecko, etc.)

### P2P Network
- [ ] Oracle messages relay across network
- [ ] Duplicate messages rejected
- [ ] Invalid signatures rejected
- [ ] Peer connections established
- [ ] Oracle node reachable on port 12025

### Block Validation
- [ ] Blocks with oracle bundles validate
- [ ] Blocks without oracle bundles validate (pre-activation)
- [ ] Invalid oracle bundles rejected
- [ ] Oracle bundle signatures verify
- [ ] Coinbase transactions include oracle data

### DigiDollar Integration
- [ ] Oracle prices available via RPC (getoracleprice)
- [ ] DigiDollar minting uses oracle prices
- [ ] DigiDollar redemption uses oracle prices
- [ ] Oracle price cache updates correctly

### Unit Tests
- [ ] All oracle unit tests pass
- [ ] Exchange API tests pass
- [ ] Oracle bundle tests pass
- [ ] Oracle validation tests pass

### Functional Tests
- [ ] feature_oracle_p2p.py passes
- [ ] digidollar_oracle.py passes
- [ ] All oracle test variants pass

---

## Troubleshooting

### Issue: Oracle not starting

**Symptoms**: No oracle logs in debug.log

**Solutions**:
1. Check oracle=1 in digibyte.conf (oracle node only)
2. Verify testnet=1 is set
3. Check compilation included oracle code:
   ```bash
   nm ./src/digibyted | grep -i oracle
   ```
4. Review debug.log for errors:
   ```bash
   grep -i error ~/.digibyte/testnet4/debug.log
   ```
5. Verify oracleprivkey is valid WIF format
6. Check libcurl is linked:
   ```bash
   ldd ./src/digibyted | grep curl
   ```

---

### Issue: No oracle price available

**Symptoms**: getoracleprice returns 0 or error

**Solutions**:
1. Check height >= activation height (if configured)
2. Verify exchange API connectivity:
   ```bash
   curl -s "https://api.binance.com/api/v3/ticker/price?symbol=DGBUSDT"
   ```
3. Check exchange API keys (if required for CoinMarketCap):
   ```bash
   grep cmcapikey ~/.digibyte/digibyte.conf
   ```
4. Review oracle logs for exchange errors:
   ```bash
   grep -i "exchange\|oracle\|fetch" ~/.digibyte/testnet4/debug.log
   ```
5. Verify network connectivity (firewall not blocking outbound HTTPS)
6. Check oracle daemon is running:
   ```bash
   ps aux | grep digibyted
   ```
7. Restart oracle daemon:
   ```bash
   digibyte-cli -testnet stop
   ./src/digibyted -testnet -daemon
   ```

---

### Issue: Oracle bundle validation fails

**Symptoms**: Blocks rejected with "bad-oracle-bundle" error

**Solutions**:
1. Verify oracle public key matches hardcoded key in chainparams.cpp:
   ```bash
   grep -A 5 "vOraclePubkeys" src/chainparams.cpp
   ```
2. Check Schnorr signature verification implementation
3. Verify Phase One 1-of-1 consensus configuration:
   ```bash
   grep -i "oracle.*threshold\|oracle.*required" src/chainparams.cpp
   ```
4. Check bundle timestamp within ±1 hour of block time:
   ```bash
   # Compare oracle bundle timestamp to block timestamp
   digibyte-cli -testnet getblock <BLOCKHASH> 2
   ```
5. Verify oracle message format is correct (check logs)
6. Ensure oracle daemon signed with correct private key

---

### Issue: Testnet won't sync

**Symptoms**: Blockchain sync stuck or very slow

**Solutions**:
1. Check network connectivity:
   ```bash
   ping 8.8.8.8
   ```
2. Add testnet peers manually:
   ```bash
   digibyte-cli -testnet addnode "testnet-peer.example.com" add
   digibyte-cli -testnet addnode "<ORACLE_NODE_IP>:12025" add
   ```
3. Check firewall allows port 12025 (testnet P2P):
   ```bash
   sudo ufw status | grep 12025
   # If needed:
   sudo ufw allow 12025/tcp
   ```
4. Try reindex if database corrupted:
   ```bash
   ./src/digibyted -testnet -reindex -daemon
   ```
5. Check peer connections:
   ```bash
   digibyte-cli -testnet getpeerinfo
   ```
6. Verify sufficient disk space:
   ```bash
   df -h ~/.digibyte/
   ```
7. Check debug.log for sync errors:
   ```bash
   tail -f ~/.digibyte/testnet4/debug.log | grep -i "error\|fail\|invalid"
   ```

---

### Issue: Exchange API rate limiting

**Symptoms**: "Too many requests" errors in logs

**Solutions**:
1. Reduce oracle broadcast frequency (if configurable)
2. Use API keys for higher rate limits (CoinMarketCap, CryptoCompare)
3. Implement exponential backoff in exchange client
4. Use fewer exchanges (comment out some in config)
5. Cache prices locally (already implemented)
6. Contact exchange for API key or rate limit increase

---

### Issue: Oracle messages not relaying

**Symptoms**: Oracle prices not propagating to peer nodes

**Solutions**:
1. Verify P2P message handlers registered:
   ```bash
   grep -i "oracle.*message\|MSG_ORACLE" src/net_processing.cpp
   ```
2. Check peer connections allow oracle messages:
   ```bash
   digibyte-cli -testnet getpeerinfo | grep -i relay
   ```
3. Verify message format is valid
4. Check for message size limits (ensure oracle bundle fits)
5. Review logs for relay errors:
   ```bash
   grep -i "relay.*oracle\|oracle.*relay" ~/.digibyte/testnet4/debug.log
   ```
6. Ensure peers are running compatible versions (v8.26+)

---

### Issue: DigiDollar transactions fail with "no oracle price"

**Symptoms**: createdigidollar RPC fails

**Solutions**:
1. Verify oracle price cache has data:
   ```bash
   digibyte-cli -testnet getoracleprice
   ```
2. Check oracle bundles in recent blocks:
   ```bash
   for i in {1..10}; do
     echo "Block $i:"
     digibyte-cli -testnet getblock $(digibyte-cli -testnet getblockhash $i) 2 | grep -i oracle
   done
   ```
3. Ensure height >= oracle activation height
4. Verify oracle daemon is running and broadcasting
5. Check DigiDollar validation code requires oracle price:
   ```bash
   grep -i "oracle.*required\|require.*oracle" src/digidollar.cpp
   ```

---

### Issue: Unit tests fail

**Symptoms**: test_digibyte crashes or tests fail

**Solutions**:
1. Verify test data matches Phase One configuration (1-of-1, single oracle)
2. Check test oracle keys are valid
3. Run specific failing test with verbose output:
   ```bash
   ./src/test/test_digibyte --run_test=<TEST_NAME> --log_level=all
   ```
4. Review test expectations match implementation
5. Ensure test environment has network access (for exchange API tests)
6. Check for timing issues (increase timeouts if needed)

---

### Issue: Functional tests fail

**Symptoms**: Python functional tests crash or timeout

**Solutions**:
1. Run with debug logging:
   ```bash
   ./test/functional/<TEST>.py --loglevel=debug --tracerpc
   ```
2. Check node logs in test temp directory:
   ```bash
   # Test will print temp directory path
   cat /tmp/bitcoin_func_test_<ID>/node0/debug.log
   ```
3. Verify test constants match DigiByte (not Bitcoin):
   - BLOCK_TIME = 15 seconds
   - COINBASE_MATURITY = 8 blocks
   - Fees in DGB/kB (not DGB/vB)
4. Increase test timeouts if tests timeout
5. Ensure sufficient system resources (RAM, CPU)
6. Check Python dependencies:
   ```bash
   pip3 install --user -r test/functional/requirements.txt
   ```

---

## Configuration Reference

### digibyte.conf - Testnet Oracle Configuration

**For Oracle Node**:

```ini
# Network
testnet=1
server=1

# RPC (optional, for remote access)
rpcuser=testnetrpc
rpcpassword=changethispassword
rpcallowip=127.0.0.1
rpcallowip=192.168.1.0/24

# Oracle system (Phase One - oracle node only)
oracle=1
oracleprivkey=<WIF_PRIVATE_KEY>

# Exchange API configuration
# CoinMarketCap requires API key:
cmcapikey=YOUR_KEY_HERE

# Optional: Specific exchanges to use
# (comma-separated list)
# oracleexchanges=binance,coingecko,coinbase

# Logging (recommended for debugging)
debug=oracle
debug=digidollar
debug=net
logips=1
logtimestamps=1

# P2P Network
port=12025
maxconnections=125

# Performance
dbcache=4096
maxmempool=300
```

**For Regular Testnet Node**:

```ini
# Network
testnet=1
server=1

# RPC (optional)
rpcuser=testnetrpc
rpcpassword=changethispassword
rpcallowip=127.0.0.1

# DO NOT set oracle=1 or oracleprivkey
# Regular nodes receive and validate oracle messages

# Logging (optional)
debug=oracle
debug=digidollar
logips=1

# P2P Network
port=12025
addnode=<ORACLE_NODE_IP>:12025
maxconnections=125

# Performance
dbcache=2048
maxmempool=300
```

---

### Command-Line Flags

**Start with oracle enabled**:
```bash
./src/digibyted -testnet -oracle=1 -oracleprivkey=<WIF_KEY>
```

**Start with specific oracle configuration**:
```bash
./src/digibyted -testnet \
  -oracle=1 \
  -oracleprivkey=<WIF_KEY> \
  -cmcapikey=<API_KEY> \
  -debug=oracle \
  -debug=digidollar
```

**Start regular node connecting to oracle**:
```bash
./src/digibyted -testnet -addnode=<ORACLE_IP>:12025
```

**Enable oracle debug logging**:
```bash
./src/digibyted -testnet -debug=oracle -debug=digidollar -debug=net
```

**Start with reindex (if blockchain corrupted)**:
```bash
./src/digibyted -testnet -reindex -daemon
```

---

### RPC Commands Reference

**Oracle Status Commands**:
```bash
# Get current oracle price
digibyte-cli -testnet getoracleprice

# Get oracle price at specific height
digibyte-cli -testnet getoraclepriceforheight <HEIGHT>

# Get oracle messages (P2P message pool)
digibyte-cli -testnet getoraclemessages

# Get oracle statistics (if implemented)
digibyte-cli -testnet getoraclestats
```

**Blockchain Status Commands**:
```bash
# Get blockchain info
digibyte-cli -testnet getblockchaininfo

# Get current block count
digibyte-cli -testnet getblockcount

# Get block details (verbosity 2 shows full transaction data)
digibyte-cli -testnet getblock <BLOCKHASH> 2

# Get mining info
digibyte-cli -testnet getmininginfo
```

**Network Commands**:
```bash
# Get peer information
digibyte-cli -testnet getpeerinfo

# Add peer manually
digibyte-cli -testnet addnode "<IP>:12025" add

# Remove peer
digibyte-cli -testnet addnode "<IP>:12025" remove

# Get network info
digibyte-cli -testnet getnetworkinfo
```

**Mining Commands**:
```bash
# Generate blocks to address
digibyte-cli -testnet generatetoaddress <NUMBLOCKS> <ADDRESS>

# Get new address for mining
digibyte-cli -testnet getnewaddress

# Generate 1 block
digibyte-cli -testnet generatetoaddress 1 $(digibyte-cli -testnet getnewaddress)
```

**DigiDollar Commands** (if implemented):
```bash
# Create DigiDollar transaction
digibyte-cli -testnet createdigidollar <AMOUNT> <RECIPIENT>

# Mint DigiDollar
digibyte-cli -testnet mintdigidollar <AMOUNT>

# Redeem DigiDollar
digibyte-cli -testnet redeemdigidollar <AMOUNT>
```

---

## Quick Reference

### Status Commands
```bash
# Check oracle status
digibyte-cli -testnet getoracleprice

# Get oracle messages
digibyte-cli -testnet getoraclemessages

# Check blockchain sync
digibyte-cli -testnet getblockchaininfo

# View oracle logs
tail -f ~/.digibyte/testnet4/debug.log | grep -i oracle

# Check peer connections
digibyte-cli -testnet getpeerinfo | grep -E "addr|version"

# Verify oracle in recent blocks
digibyte-cli -testnet getblock $(digibyte-cli -testnet getblockhash $(digibyte-cli -testnet getblockcount)) 2 | grep -i oracle
```

### Testing Commands
```bash
# Mine blocks
digibyte-cli -testnet generatetoaddress 1 $(digibyte-cli -testnet getnewaddress)

# Send test transaction
digibyte-cli -testnet sendtoaddress <ADDRESS> 100

# Test DigiDollar mint (if implemented)
digibyte-cli -testnet mintdigidollar 100

# Run unit tests
./src/test/test_digibyte --run_test=oracle_*

# Run functional tests
./test/functional/feature_oracle_p2p.py
./test/functional/digidollar_oracle.py
```

### Debug Commands
```bash
# Monitor oracle activity
tail -f ~/.digibyte/testnet4/debug.log | grep -E "oracle|ORACLE|Oracle"

# Monitor exchange API calls
tail -f ~/.digibyte/testnet4/debug.log | grep -E "exchange|Exchange|EXCHANGE"

# Monitor DigiDollar activity
tail -f ~/.digibyte/testnet4/debug.log | grep -E "digidollar|DigiDollar|DIGIDOLLAR"

# Check for errors
grep -i error ~/.digibyte/testnet4/debug.log | tail -20

# Check for warnings
grep -i warn ~/.digibyte/testnet4/debug.log | tail -20
```

### Maintenance Commands
```bash
# Stop daemon gracefully
digibyte-cli -testnet stop

# Start daemon
./src/digibyted -testnet -daemon

# Restart daemon
digibyte-cli -testnet stop && sleep 5 && ./src/digibyted -testnet -daemon

# Check daemon status
ps aux | grep digibyted | grep testnet

# Check disk usage
du -sh ~/.digibyte/testnet4/

# Clean old logs (keep last 1000 lines)
tail -1000 ~/.digibyte/testnet4/debug.log > /tmp/debug.log && mv /tmp/debug.log ~/.digibyte/testnet4/debug.log
```

---

## Post-Reset Testing Plan

After completing the testnet reset, execute the following testing plan:

### 1. Oracle System Tests

**Verify oracle initialization**:
```bash
grep -i "oracle.*init\|oracle.*start" ~/.digibyte/testnet4/debug.log
```

**Verify exchange API connectivity**:
```bash
grep -i "exchange.*success\|fetched.*price" ~/.digibyte/testnet4/debug.log
```

**Verify oracle broadcasting**:
```bash
grep -i "broadcasting.*oracle\|oracle.*broadcast" ~/.digibyte/testnet4/debug.log
```

### 2. P2P Network Tests

**Test oracle message relay**:
```bash
# From oracle node
digibyte-cli -testnet getoraclemessages

# From peer node (should see same messages after propagation)
digibyte-cli -testnet getoraclemessages
```

**Test peer synchronization**:
```bash
# Compare block counts across nodes
digibyte-cli -testnet getblockcount
```

### 3. Block Validation Tests

**Verify oracle bundles in blocks**:
```bash
# Check multiple recent blocks
for height in {1..20}; do
  echo "Block $height:"
  digibyte-cli -testnet getblock $(digibyte-cli -testnet getblockhash $height) 2 | grep -o "oracle" | wc -l
done
```

**Verify block acceptance**:
```bash
# Generate blocks and verify no validation errors
digibyte-cli -testnet generatetoaddress 10 $(digibyte-cli -testnet getnewaddress)
grep -i "error\|invalid" ~/.digibyte/testnet4/debug.log | tail -20
```

### 4. Oracle Price Cache Tests

**Verify price cache updates**:
```bash
# Check price multiple times (should update)
digibyte-cli -testnet getoracleprice
sleep 60
digibyte-cli -testnet getoracleprice
```

**Verify price at specific height**:
```bash
digibyte-cli -testnet getoraclepriceforheight 100
```

### 5. DigiDollar Integration Tests

**Test DigiDollar minting** (if implemented):
```bash
# Create DigiDollar transaction
digibyte-cli -testnet mintdigidollar 1000

# Verify transaction uses oracle price
digibyte-cli -testnet getrawtransaction <TXID> 1
```

**Test DigiDollar redemption** (if implemented):
```bash
digibyte-cli -testnet redeemdigidollar 500
```

### 6. Automated Test Execution

**Run all test suites**:
```bash
# Unit tests
./src/test/test_digibyte --run_test=oracle_* --log_level=message

# Functional tests
./test/functional/feature_oracle_p2p.py
./test/functional/digidollar_oracle.py

# Extended tests
./test/functional/test_runner.py --extended
```

---

## Oracle Node Operational Guidelines

### Daily Monitoring

**Check oracle health**:
```bash
# Run daily health check
cat > /usr/local/bin/oracle-health-check.sh <<'EOF'
#!/bin/bash
echo "Oracle Health Check - $(date)"
echo "================================"

# Check daemon running
if pgrep -f "digibyted.*testnet" > /dev/null; then
  echo "✓ Daemon running"
else
  echo "✗ Daemon NOT running"
  exit 1
fi

# Check oracle price
PRICE=$(digibyte-cli -testnet getoracleprice)
if [ "$PRICE" -gt 0 ]; then
  echo "✓ Oracle price available: $PRICE micro-USD"
else
  echo "✗ No oracle price"
fi

# Check peer connections
PEERS=$(digibyte-cli -testnet getconnectioncount)
echo "✓ Connected peers: $PEERS"

# Check recent errors
ERRORS=$(grep -i error ~/.digibyte/testnet4/debug.log | tail -5)
if [ -z "$ERRORS" ]; then
  echo "✓ No recent errors"
else
  echo "⚠ Recent errors found:"
  echo "$ERRORS"
fi

echo "================================"
EOF

chmod +x /usr/local/bin/oracle-health-check.sh

# Run daily via cron
# crontab -e
# 0 9 * * * /usr/local/bin/oracle-health-check.sh | mail -s "Oracle Health" admin@example.com
```

### Backup Procedures

**Backup oracle keys**:
```bash
# Backup wallet with oracle keys
cp ~/.digibyte/testnet4/wallet.dat ~/backups/oracle-wallet-$(date +%Y%m%d).dat

# Backup configuration
cp ~/.digibyte/digibyte.conf ~/backups/digibyte-conf-$(date +%Y%m%d).conf

# Encrypt backups
gpg --encrypt --recipient admin@example.com ~/backups/oracle-wallet-$(date +%Y%m%d).dat
```

### Security Best Practices

1. **Protect oracle private key**:
   - Never commit to version control
   - Encrypt wallet.dat backups
   - Use strong filesystem permissions (600)
   - Consider hardware security module (HSM) for production

2. **Firewall configuration**:
   ```bash
   # Allow testnet P2P
   sudo ufw allow 12025/tcp

   # Restrict RPC to localhost only
   # In digibyte.conf: rpcallowip=127.0.0.1
   ```

3. **Log monitoring**:
   - Monitor for unauthorized access attempts
   - Alert on oracle errors
   - Track exchange API failures

4. **Incident response**:
   - Have backup oracle operator
   - Document key rotation procedure
   - Test disaster recovery plan

---

## Appendix: Phase One vs Production Differences

### Phase One (Testnet)

- **Oracles**: 1 hardcoded testnet oracle
- **Consensus**: 1-of-1 (single signature required)
- **Epoch**: No rotation (single oracle always active)
- **Network**: Testnet only
- **Security**: Lower (testnet has no monetary value)
- **Activation**: Configurable in chainparams

### Future Production (Mainnet)

- **Oracles**: 15 authorized oracles
- **Consensus**: 8-of-15 (majority required)
- **Epoch**: Rotating active set (6 active per epoch)
- **Network**: Mainnet
- **Security**: Higher (real monetary value)
- **Activation**: Consensus-driven hard fork

### Migration Path

When transitioning from Phase One to production:

1. **Code updates**:
   - Implement multi-oracle consensus
   - Add epoch rotation logic
   - Enhance security measures
   - Add oracle key rotation

2. **Testing**:
   - Test with 15 oracles on testnet
   - Validate 8-of-15 consensus
   - Test epoch transitions
   - Stress test network load

3. **Deployment**:
   - Coordinate with 15 oracle operators
   - Set mainnet activation height
   - Deploy monitoring infrastructure
   - Establish incident response plan

---

## Support and Resources

### Documentation

- **Phase One Spec**: `/DIGIDOLLAR_ORACLE_PHASE_ONE_SPEC.md`
- **DigiByte Configuration**: `/doc/digibyte-conf.md`
- **Build Instructions**: `/doc/build-unix.md`
- **RPC Documentation**: `/doc/json-rpc-interface.md`

### Community Support

- **DigiByte Discord**: https://discord.gg/digibyte
- **DigiByte Telegram**: https://t.me/DigiByteCoin
- **GitHub Issues**: https://github.com/digibyte-core/digibyte/issues

### Developer Resources

- **DigiByte Core Repo**: https://github.com/digibyte-core/digibyte
- **Developer Docs**: https://digibyte.org/docs/developers
- **API Reference**: https://digibyte.org/docs/api

---

## Changelog

**Version 1.0 (2025-11-18)**:
- Initial testnet oracle reset procedures documentation
- Phase One configuration and setup
- Comprehensive troubleshooting guide
- Complete verification checklist
- Operational guidelines for oracle operators

---

**Document maintained by**: DigiByte Oracle Implementation Team
**Last updated**: 2025-11-18
**Status**: Active - Phase One Development
