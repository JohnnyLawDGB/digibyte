# DigiDollar Qt GUI RegTest Testing Guide - COMPREHENSIVE

**Purpose:** This guide provides step-by-step instructions for testing DigiDollar network-wide tracking in Qt GUI using RegTest mode. Follow these instructions exactly to verify the system is working correctly.

**For AI Assistants:** This is the canonical testing procedure. Execute these steps whenever asked to test DigiDollar Qt functionality.

---

## Prerequisites

### Required Files Check
```bash
# Verify binaries exist
ls -lh ./src/qt/digibyte-qt
ls -lh ./src/digibyte-cli

# If missing, compile:
make -j8
```

### Clean Environment
**CRITICAL:** Always start with clean regtest directories to avoid state contamination.

```bash
# Stop any running nodes
pkill -f "digibyte-qt.*regtest" 2>/dev/null
pkill -f "digibyted.*regtest" 2>/dev/null
sleep 2

# Remove ALL regtest data
rm -rf ~/Library/Application\ Support/DigiByte/regtest
rm -rf /tmp/bob_regtest
rm -rf /tmp/alice_regtest

echo "✓ Clean environment ready"
```

---

## Step 1: Start Bob's Node (Primary)

**Why 644 blocks?**
- COINBASE_MATURITY = 8 blocks (early blocks)
- COINBASE_MATURITY_2 = 100 blocks (after certain height)
- 644 blocks ensures we're well past all maturity requirements and any hard fork activation points in RegTest
- Provides sufficient mature coinbase outputs for minting

```bash
# Create data directory
mkdir -p /tmp/bob_regtest

# Start Bob's Qt node in background
./src/qt/digibyte-qt \
    -regtest \
    -datadir=/tmp/bob_regtest \
    -port=18444 \
    -rpcport=18443 \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -fallbackfee=0.0001 \
    > /tmp/bob_qt.log 2>&1 &

echo "Bob's Qt node starting (PID: $!)"
sleep 8

# Verify node is running
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount
```

**Expected Output:** `0` (fresh chain)

---

## Step 2: Create Bob's Wallet & Generate Blocks

```bash
# Create wallet
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 createwallet "bob"

# Generate 644 blocks (ensures maturity and hard fork activation)
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 644

# Verify block count
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount
```

**Expected Output:** `644`

**Why this matters:**
- First 8 blocks: Initial coinbase maturity
- Blocks 9-100: COINBASE_MATURITY_2 period
- Blocks 101-644: Ensures we have hundreds of mature UTXOs for minting
- Past any RegTest hard fork activation points

---

## Step 3: Start Alice's Node (Secondary)

```bash
# Create data directory
mkdir -p /tmp/alice_regtest

# Start Alice's Qt node (connects to Bob)
./src/qt/digibyte-qt \
    -regtest \
    -datadir=/tmp/alice_regtest \
    -port=18445 \
    -rpcport=18446 \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -fallbackfee=0.0001 \
    -connect=127.0.0.1:18444 \
    > /tmp/alice_qt.log 2>&1 &

echo "Alice's Qt node starting (PID: $!)"
sleep 8

# Create wallet
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 createwallet "alice"

# Generate blocks for Alice (for her own UTXOs)
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 -generate 110

# Wait for sync
sleep 5

# Verify both nodes are at same height
echo "Bob's height: $(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)"
echo "Alice's height: $(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)"
```

**Expected Output:** Both should show `754` (644 + 110)

---

## Step 4: Set Mock Oracle Price

**IMPORTANT:** Oracle price format is in **cents per DGB**
- `1` = $0.01 per DGB
- `100` = $1.00 per DGB
- `5000` = $50.00 per DGB

For this test, we use **$0.01 per DGB** (1 cent):

```bash
# Set oracle price on Bob's node
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 setmockoracleprice 1

# Set oracle price on Alice's node
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 setmockoracleprice 1

# Verify oracle price on both
echo "=== Oracle Prices ==="
echo "Bob's oracle price:"
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getmockoracleprice

echo -e "\nAlice's oracle price:"
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getmockoracleprice
```

**Expected Output (both nodes):**
```json
{
  "price": 1,
  "price_usd": "$0.01",
  "last_update_height": ...,
  "enabled": true
}
```

---

## Step 5: Bob Mints DigiDollars

**Mint Parameters:**
- Amount: 10000 cents = $100.00
- Lock Period: Tier 4 (365 days / 1 year)
- Expected Collateral: ~300% of $100 = $300 worth of DGB
- At $0.01/DGB: 30,000 DGB required

```bash
echo "=== Bob Minting $100 DD ==="

# Mint DigiDollars
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 mintdigidollar 10000 4

# Generate blocks to confirm
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 2

# Wait for sync
sleep 3

# Verify Bob's balance
echo "Bob's DD balance:"
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob getbalance
```

---

## Step 6: Alice Mints DigiDollars

```bash
echo "=== Alice Minting $200 DD ==="

# Mint DigiDollars
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 mintdigidollar 20000 3

# Generate blocks to confirm
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 -generate 2

# Wait for sync
sleep 3
```

---

## Step 7: Verify Network-Wide Tracking (CRITICAL TEST)

**This is the key test:** Both nodes MUST see identical network-wide statistics.

```bash
echo "=========================================="
echo "CRITICAL TEST: Network-Wide Tracking"
echo "=========================================="

# Get system health from Bob
echo -e "\n=== Bob's View (RPC) ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getdigidollarsystemhealth

# Get system health from Alice
echo -e "\n=== Alice's View (RPC) ==="
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getdigidollarsystemhealth

echo -e "\n=========================================="
echo "✓ Compare the outputs above"
echo "  THEY MUST BE IDENTICAL!"
echo "=========================================="
```

**Expected Values (should match on BOTH nodes):**
```json
{
  "health_percentage": ~350,        // Will vary based on actual collateral
  "total_dd_supply": 30000,         // 100 + 200 = 300 DD = 30000 cents
  "total_collateral_dgb": ~90000,   // Combined collateral from both vaults
  "oracle_price_cents": 1,
  "system_collateral_ratio": ~350
}
```

**Calculation Verification:**
- Bob: $100 DD requires ~30,000 DGB at $0.01 (300% ratio for 1-year lock)
- Alice: $200 DD requires ~70,000 DGB at $0.01 (350% ratio for 6-month lock)
- Total: $300 DD, ~100,000 DGB
- System Health: (100,000 × $0.01) / $300 = $1,000 / $300 = 333%

---

## Step 8: Qt GUI Verification

**Both Qt windows should now be open.** Navigate to the DigiDollar tab and verify:

### Bob's Qt GUI Should Show:

**Your DigiDollar Balances:**
- Your DD Balance: ~100.00 DD (Bob's personal balance)
- Your Locked Collateral: ~30,000 DGB

**Network DigiDollar Status:**
- DGB/USD Price: $0.010000 ✅
- Network Total DD: $300.00 ✅ (Bob's + Alice's combined)
- Network Total Collateral: ~100,000 DGB ✅
- System Health: ~333.0% Healthy ✅
- DCA Level: 1.0x ✅
- ERR Level: Inactive ✅

### Alice's Qt GUI Should Show:

**Your DigiDollar Balances:**
- Your DD Balance: ~200.00 DD (Alice's personal balance)
- Your Locked Collateral: ~70,000 DGB

**Network DigiDollar Status:**
- DGB/USD Price: $0.010000 ✅
- Network Total DD: $300.00 ✅ (SAME as Bob!)
- Network Total Collateral: ~100,000 DGB ✅ (SAME as Bob!)
- System Health: ~333.0% Healthy ✅ (SAME as Bob!)
- DCA Level: 1.0x ✅
- ERR Level: Inactive ✅

### Vault Tab Verification

**Bob's Vault Tab:**
- Should show 1 vault
- DD Minted: ~100.00 DD
- DGB Collateral: ~30,000 DGB
- Lock Period: 1 year
- Health: ~300%

**Alice's Vault Tab:**
- Should show 1 vault
- DD Minted: ~200.00 DD
- DGB Collateral: ~70,000 DGB
- Lock Period: 180 days (6 months)
- Health: ~350%

---

## Step 9: Additional RPC Tests

```bash
# Get detailed stats
echo "=== Bob's Detailed Stats ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getdigidollarstats

# Get DCA multiplier
echo -e "\n=== Current DCA Multiplier ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getdcamultiplier

# Get protection status
echo -e "\n=== Protection Status ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getprotectionstatus

# Calculate collateral requirement for new mint
echo -e "\n=== Calculate Required Collateral for $500 DD (1 year) ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 calculatecollateralrequirement 500.00 4
```

---

## Step 10: Cleanup (When Done Testing)

```bash
# Stop nodes via RPC
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 stop
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 stop

# Wait for shutdown
sleep 3

# Or force kill if needed
pkill -f "digibyte-qt.*regtest"

# Optional: Remove test data
rm -rf /tmp/bob_regtest /tmp/alice_regtest

echo "✓ Cleanup complete"
```

---

## Success Criteria

✅ **All of these MUST be true:**

1. **Network-Wide Tracking:**
   - [ ] Bob and Alice see IDENTICAL `total_dd_supply`
   - [ ] Bob and Alice see IDENTICAL `total_collateral_dgb`
   - [ ] Bob and Alice see IDENTICAL `health_percentage`
   - [ ] Bob and Alice see IDENTICAL `system_collateral_ratio`

2. **Personal Balances:**
   - [ ] Bob sees his own ~100 DD personal balance
   - [ ] Alice sees her own ~200 DD personal balance
   - [ ] Personal balances are DIFFERENT (not network-wide)

3. **Qt GUI Display:**
   - [ ] System Health shows percentage > 100% (e.g., 333%)
   - [ ] NOT showing divided value (e.g., NOT 33.3%)
   - [ ] Network Total DD shows $300.00 (combined)
   - [ ] NOT showing wrong value (e.g., NOT $1.32 or $0.66)
   - [ ] Both nodes' Qt GUIs show SAME network stats

4. **Vault Health:**
   - [ ] Bob's vault shows ~300% health (300% required for 1-year)
   - [ ] Alice's vault shows ~350% health (350% required for 6-month)
   - [ ] Individual vault health percentages are correct

5. **Oracle Price:**
   - [ ] Both nodes show $0.01 per DGB
   - [ ] Price is displayed correctly in GUI
   - [ ] Calculations use correct price

---

## Troubleshooting

### Qt Windows Don't Show DigiDollar Tab
```bash
# Check that -digidollar=1 flag is set
ps aux | grep digibyte-qt | grep digidollar

# Check debug log for errors
tail -100 /tmp/bob_regtest/regtest/debug.log | grep -i "digidollar\|error"

# Restart with explicit flag
pkill -f digibyte-qt
# Then restart with -digidollar=1
```

### Nodes Don't Sync
```bash
# Check peer connection
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getpeerinfo

# Manually add peer if needed
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 addnode "127.0.0.1:18444" "add"

# Verify block heights match
echo "Bob: $(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)"
echo "Alice: $(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)"
```

### Network Stats Don't Match
**This indicates a bug in network-wide tracking!**

```bash
# Enable debug logging
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 logging '["digidollar"]'

# Regenerate blocks to trigger UTXO scan
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 1

# Check logs
grep "ScanUTXOSet\|ExtractDDAmount" /tmp/bob_regtest/regtest/debug.log
```

### Insufficient Funds Error
```bash
# Check wallet balance
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob getbalance

# If too low, generate more blocks
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 100

# Note: Coinbase outputs need 8-100 blocks to mature
```

### Wrong Oracle Price
```bash
# Check current price
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getmockoracleprice

# Reset if wrong
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 setmockoracleprice 1

# Verify
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getmockoracleprice
```

---

## For AI Assistants: Automated Test Script

If asked to run automated tests, use this script:

```bash
#!/bin/bash
# Automated DigiDollar Qt RegTest Test

set -e

# Step 1: Clean environment
echo "=== Cleaning environment ==="
pkill -f "digibyte-qt.*regtest" 2>/dev/null || true
sleep 2
rm -rf /tmp/bob_regtest /tmp/alice_regtest

# Step 2: Start Bob
echo "=== Starting Bob's node ==="
mkdir -p /tmp/bob_regtest
./src/qt/digibyte-qt -regtest -datadir=/tmp/bob_regtest -port=18444 -rpcport=18443 \
    -server -listen=1 -discover=0 -digidollar=1 -fallbackfee=0.0001 > /tmp/bob_qt.log 2>&1 &
sleep 8

# Step 3: Bob setup
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 createwallet "bob"
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 644

# Step 4: Start Alice
echo "=== Starting Alice's node ==="
mkdir -p /tmp/alice_regtest
./src/qt/digibyte-qt -regtest -datadir=/tmp/alice_regtest -port=18445 -rpcport=18446 \
    -server -listen=1 -discover=0 -digidollar=1 -fallbackfee=0.0001 \
    -connect=127.0.0.1:18444 > /tmp/alice_qt.log 2>&1 &
sleep 8

# Step 5: Alice setup
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 createwallet "alice"
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 -generate 110
sleep 5

# Step 6: Set oracle price
echo "=== Setting oracle prices ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 setmockoracleprice 1
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 setmockoracleprice 1

# Step 7: Mint DD
echo "=== Minting DigiDollars ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 mintdigidollar 10000 4
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 2
sleep 3

./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 mintdigidollar 20000 3
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 -generate 2
sleep 5

# Step 8: Verify
echo "=== Verifying Network-Wide Tracking ==="
BOB_HEALTH=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getdigidollarsystemhealth)
ALICE_HEALTH=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getdigidollarsystemhealth)

echo "Bob's system health:"
echo "$BOB_HEALTH"

echo -e "\nAlice's system health:"
echo "$ALICE_HEALTH"

# Extract and compare total_dd_supply
BOB_SUPPLY=$(echo "$BOB_HEALTH" | jq '.total_dd_supply')
ALICE_SUPPLY=$(echo "$ALICE_HEALTH" | jq '.total_dd_supply')

if [ "$BOB_SUPPLY" = "$ALICE_SUPPLY" ]; then
    echo -e "\n✅ SUCCESS: Network-wide tracking works!"
    echo "Both nodes see total_dd_supply: $BOB_SUPPLY"
else
    echo -e "\n❌ FAILED: Nodes see different DD supply!"
    echo "Bob: $BOB_SUPPLY, Alice: $ALICE_SUPPLY"
    exit 1
fi

echo -e "\n=== Qt GUIs are now running ==="
echo "Check the DigiDollar tab in both windows"
echo "Press Ctrl+C when done testing"
wait
```

---

## Technical Reference

### Block Maturity
- **COINBASE_MATURITY** = 8 blocks (early chain)
- **COINBASE_MATURITY_2** = 100 blocks (after height threshold)
- **Why 644 blocks?** Ensures hundreds of mature coinbase outputs

### Oracle Price Format
- **Input:** Cents per DGB
- **Example:** `setmockoracleprice 1` = $0.01 per DGB
- **Range:** 1 to 100000 (0.01 to $1000.00 per DGB)

### DigiDollar Amount Format
- **Storage:** Cents (int64)
- **Display:** Dollars (divided by 100)
- **Example:** 10000 cents = $100.00 DD

### Collateral Ratios by Tier
| Lock Period | Tier | Base Ratio |
|-------------|------|------------|
| 30 days     | 1    | 500%       |
| 90 days     | 2    | 400%       |
| 180 days    | 3    | 350%       |
| 365 days    | 4    | 300%       |
| 730 days    | 5    | 250%       |
| 1825 days   | 6    | 225%       |

**Note:** DCA multiplier can increase these ratios during low system health.

---

**Last Updated:** October 6, 2025
**Version:** 2.0 (Fixed for proper hard fork handling and network tracking)
**Tested:** ✅ All fixes verified with unit and functional tests
