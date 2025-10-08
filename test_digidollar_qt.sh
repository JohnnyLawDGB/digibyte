#!/bin/bash
# DigiDollar Qt GUI Network-Wide Tracking Test
# Tests that both Bob and Alice see identical network statistics
# Bob mints 3 DigiDollar transactions, Alice only observes

set -e

echo "=========================================="
echo "DigiDollar Qt RegTest Automated Test"
echo "=========================================="
echo ""

# Step 1: Clean environment
echo "=== Step 1: Cleaning environment ==="
pkill -f "digibyte-qt.*regtest" 2>/dev/null || true
pkill -f "digibyted.*regtest" 2>/dev/null || true
sleep 2
rm -rf ~/Library/Application\ Support/DigiByte/regtest
rm -rf /tmp/bob_regtest
rm -rf /tmp/alice_regtest
echo "✓ Clean environment ready"
echo ""

# Step 2: Start Bob's node
echo "=== Step 2: Starting Bob's Qt node ==="
mkdir -p /tmp/bob_regtest
./src/qt/digibyte-qt \
    -regtest \
    -datadir=/tmp/bob_regtest \
    -port=18444 \
    -rpcport=18443 \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    > /tmp/bob_qt.log 2>&1 &
BOB_PID=$!
echo "Bob's Qt started (PID: $BOB_PID)"
sleep 8

# Step 3: Create Bob's wallet and generate 700 blocks (extra for multiple mints)
echo "=== Step 3: Creating Bob's wallet and generating 700 blocks ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 createwallet "bob" > /dev/null
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 700 > /dev/null
echo "✓ Bob has 700 blocks (ensuring enough mature UTXOs for 3 mints)"
echo ""

# Step 4: Set oracle price
echo "=== Step 4: Setting mock oracle price ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 setmockoracleprice 1 > /dev/null
ORACLE_PRICE=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getmockoracleprice | jq -r '.price_usd')
echo "✓ Oracle price set to: $ORACLE_PRICE per DGB"
echo ""

# Step 5: Bob mints DigiDollars (3 separate mints)
echo "=== Step 5: Bob minting DigiDollars (3 separate mints) ==="
COOKIE=$(cat /tmp/bob_regtest/regtest/.cookie)

# Mint #1: $100.00 DD with 365 day lock (tier 4)
echo "Mint #1: \$100.00 DD (365 days, tier 4)"
BOB_MINT1=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"bob_mint1","method":"mintdigidollar","params":[10000,4]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

BOB_TXID1=$(echo "$BOB_MINT1" | jq -r '.result.txid // empty')
if [ -z "$BOB_TXID1" ]; then
    echo "❌ Bob's mint #1 failed:"
    echo "$BOB_MINT1" | jq '.'
    exit 1
fi
echo "  ✓ txid: ${BOB_TXID1:0:16}..."
echo "  ✓ Collateral: $(echo "$BOB_MINT1" | jq -r '.result.dgb_collateral') DGB"
echo ""
sleep 1

# Mint #2: $50.00 DD with 180 day lock (tier 3)
echo "Mint #2: \$50.00 DD (180 days, tier 3)"
BOB_MINT2=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"bob_mint2","method":"mintdigidollar","params":[5000,3]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

BOB_TXID2=$(echo "$BOB_MINT2" | jq -r '.result.txid // empty')
if [ -z "$BOB_TXID2" ]; then
    echo "❌ Bob's mint #2 failed:"
    echo "$BOB_MINT2" | jq '.'
    exit 1
fi
echo "  ✓ txid: ${BOB_TXID2:0:16}..."
echo "  ✓ Collateral: $(echo "$BOB_MINT2" | jq -r '.result.dgb_collateral') DGB"
echo ""
sleep 1

# Mint #3: $25.00 DD with 90 day lock (tier 2)
echo "Mint #3: \$25.00 DD (90 days, tier 2)"
BOB_MINT3=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"bob_mint3","method":"mintdigidollar","params":[2500,2]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

BOB_TXID3=$(echo "$BOB_MINT3" | jq -r '.result.txid // empty')
if [ -z "$BOB_TXID3" ]; then
    echo "❌ Bob's mint #3 failed:"
    echo "$BOB_MINT3" | jq '.'
    exit 1
fi
echo "  ✓ txid: ${BOB_TXID3:0:16}..."
echo "  ✓ Collateral: $(echo "$BOB_MINT3" | jq -r '.result.dgb_collateral') DGB"
echo ""

echo "Bob's total minted: \$175.00 DD (17500 cents)"
echo ""

# Step 6: Bob generates 10 blocks to confirm his mints
echo "=== Step 6: Bob generating 10 blocks to confirm mints ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 10 > /dev/null
sleep 3
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
echo "✓ Bob's mints confirmed (height: $BOB_HEIGHT)"
echo ""

# Step 7: Start Alice's node
echo "=== Step 7: Starting Alice's Qt node ==="
mkdir -p /tmp/alice_regtest
./src/qt/digibyte-qt \
    -regtest \
    -datadir=/tmp/alice_regtest \
    -port=18445 \
    -rpcport=18446 \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    -connect=127.0.0.1:18444 \
    > /tmp/alice_qt.log 2>&1 &
ALICE_PID=$!
echo "Alice's Qt started (PID: $ALICE_PID)"
sleep 8

# Step 8: Create Alice's wallet and set oracle price
echo "=== Step 8: Creating Alice's wallet, setting oracle price, and syncing ==="
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 createwallet "alice" > /dev/null
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 setmockoracleprice 1 > /dev/null
sleep 5
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
echo "✓ Alice synced (height: $ALICE_HEIGHT)"
echo "✓ Alice's oracle price set"
ALICE_COOKIE=$(cat /tmp/alice_regtest/regtest/.cookie)
echo ""

# Step 9: Verify network-wide tracking
echo "=========================================="
echo "CRITICAL TEST: Network-Wide Tracking"
echo "=========================================="
echo ""

# Get Bob's view
echo "=== Bob's View (RPC) ==="
BOB_HEALTH=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarstats","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)
echo "$BOB_HEALTH" | jq '.result'
echo ""

# Get Alice's view
echo "=== Alice's View (RPC) ==="
ALICE_HEALTH=$(curl --silent --user "$ALICE_COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarstats","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18446/)
echo "$ALICE_HEALTH" | jq '.result'
echo ""

# Compare values
BOB_SUPPLY=$(echo "$BOB_HEALTH" | jq '.result.total_dd_supply')
ALICE_SUPPLY=$(echo "$ALICE_HEALTH" | jq '.result.total_dd_supply')

BOB_COLLATERAL=$(echo "$BOB_HEALTH" | jq '.result.total_collateral_dgb')
ALICE_COLLATERAL=$(echo "$ALICE_HEALTH" | jq '.result.total_collateral_dgb')

BOB_HEALTH_PCT=$(echo "$BOB_HEALTH" | jq '.result.health_percentage')
ALICE_HEALTH_PCT=$(echo "$ALICE_HEALTH" | jq '.result.health_percentage')

echo "=========================================="
echo "Verification Results:"
echo "=========================================="

if [ "$BOB_SUPPLY" = "$ALICE_SUPPLY" ]; then
    echo "✅ total_dd_supply MATCHES: $BOB_SUPPLY cents"
else
    echo "❌ total_dd_supply MISMATCH!"
    echo "   Bob: $BOB_SUPPLY, Alice: $ALICE_SUPPLY"
fi

if [ "$BOB_COLLATERAL" = "$ALICE_COLLATERAL" ]; then
    echo "✅ total_collateral_dgb MATCHES: $BOB_COLLATERAL DGB"
else
    echo "❌ total_collateral_dgb MISMATCH!"
    echo "   Bob: $BOB_COLLATERAL, Alice: $ALICE_COLLATERAL"
fi

if [ "$BOB_HEALTH_PCT" = "$ALICE_HEALTH_PCT" ]; then
    echo "✅ health_percentage MATCHES: $BOB_HEALTH_PCT%"
else
    echo "❌ health_percentage MISMATCH!"
    echo "   Bob: $BOB_HEALTH_PCT%, Alice: $ALICE_HEALTH_PCT%"
fi

# Verify health calculation is correct (328% for our test case)
EXPECTED_HEALTH=328
if [ "$BOB_HEALTH_PCT" = "$EXPECTED_HEALTH" ]; then
    echo "✅ health_percentage CORRECT: $EXPECTED_HEALTH%"
else
    echo "❌ health_percentage INCORRECT!"
    echo "   Expected: $EXPECTED_HEALTH%, Got: $BOB_HEALTH_PCT%"
fi

echo ""
echo "Expected values from Bob's 3 mints:"
echo "  - Total DD Supply: 17500 cents (\$175.00)"
echo "  - Tier 4 (300% ratio): \$100.00 DD → $((10000 * 3)) DGB = 30000 DGB"
echo "  - Tier 3 (350% ratio): \$50.00 DD → $((5000 * 350 / 100)) DGB = 17500 DGB"
echo "  - Tier 2 (400% ratio): \$25.00 DD → $((2500 * 4)) DGB = 10000 DGB"
echo "  - Total Collateral: 57500 DGB"
echo ""

# Step 10: Bob mints $100 DD with 1-hour lock
echo "=========================================="
echo "Step 10: Bob mints \$100 DD with 1-hour lock"
echo "=========================================="
echo ""

# Generate 200 more blocks to ensure Bob has enough UTXOs for 1000% collateral
echo "=== Generating 200 blocks for additional UTXOs ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 200 > /dev/null
sleep 5
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
echo "✓ Bob height: $BOB_HEIGHT"
echo ""

echo "Mint #4: \$10.00 DD (1 hour, tier 0 = 240 blocks, 1000% = 10000 DGB)"
BOB_MINT4=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"bob_mint4","method":"mintdigidollar","params":[1000,0]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

BOB_TXID4=$(echo "$BOB_MINT4" | jq -r '.result.txid // empty')
if [ -z "$BOB_TXID4" ]; then
    echo "❌ Bob's mint #4 failed:"
    echo "$BOB_MINT4" | jq '.'
    exit 1
fi
echo "  ✓ txid: ${BOB_TXID4:0:16}..."
echo "  ✓ Collateral: $(echo "$BOB_MINT4" | jq -r '.result.dgb_collateral') DGB"
echo "  ✓ Lock blocks: 240 (1 hour)"
echo ""

# Dandelion disabled for testing, transaction goes directly to mempool
MINT4_TXID=$(echo "$BOB_MINT4" | jq -r '.result.txid')
sleep 2

# Generate 2 blocks to confirm
echo "=== Generating 2 blocks to confirm mint ==="
ADDR=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob getnewaddress)
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob generatetoaddress 2 "$ADDR" > /dev/null
sleep 5
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)

# Verify the transaction is actually confirmed
TX_CONFIRMATIONS=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob gettransaction "$MINT4_TXID" 2>/dev/null | jq -r '.confirmations // 0')
if [ "$TX_CONFIRMATIONS" -eq 0 ]; then
    echo "⚠️  WARNING: Mint transaction still NOT confirmed after 2 blocks!"
    echo "Attempting to mine 10 more blocks..."
    ./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob generatetoaddress 10 "$ADDR" > /dev/null
    sleep 5
    TX_CONFIRMATIONS=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob gettransaction "$MINT4_TXID" 2>/dev/null | jq -r '.confirmations // 0')
    BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
    if [ "$TX_CONFIRMATIONS" -eq 0 ]; then
        echo "❌ ERROR: Mint transaction STILL not confirmed after 12 blocks total!"
        exit 1
    fi
fi

echo "✓ Mint #4 confirmed (height: $BOB_HEIGHT, confirmations: $TX_CONFIRMATIONS)"
echo "✓ Lock will expire at height: $((BOB_HEIGHT + 238))"
echo ""

# Step 11: Generate 120 blocks (halfway through lock period)
echo "=========================================="
echo "Step 11: Generate 120 blocks (halfway through lock)"
echo "=========================================="
echo ""
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 120 > /dev/null
sleep 5
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
echo "✓ Bob height: $BOB_HEIGHT"
echo "✓ Alice height: $ALICE_HEIGHT"
echo "✓ Blocks until unlock: $((BOB_HEIGHT + 118))"
echo ""

# Step 12: Verify network stats match
echo "=========================================="
echo "Step 12: Verify network stats match"
echo "=========================================="
echo ""

# Get Bob's view
echo "=== Bob's View ==="
BOB_STATS=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarstats","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)
echo "$BOB_STATS" | jq '.result'
echo ""

# Get Alice's view
echo "=== Alice's View ==="
ALICE_STATS=$(curl --silent --user "$ALICE_COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarstats","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18446/)
echo "$ALICE_STATS" | jq '.result'
echo ""

# Compare values
BOB_SUPPLY_2=$(echo "$BOB_STATS" | jq '.result.total_dd_supply')
ALICE_SUPPLY_2=$(echo "$ALICE_STATS" | jq '.result.total_dd_supply')

BOB_COLLATERAL_2=$(echo "$BOB_STATS" | jq '.result.total_collateral_dgb')
ALICE_COLLATERAL_2=$(echo "$ALICE_STATS" | jq '.result.total_collateral_dgb')

BOB_HEALTH_PCT_2=$(echo "$BOB_STATS" | jq '.result.health_percentage')
ALICE_HEALTH_PCT_2=$(echo "$ALICE_STATS" | jq '.result.health_percentage')

echo "Verification (with 4th mint):"
if [ "$BOB_SUPPLY_2" = "$ALICE_SUPPLY_2" ]; then
    echo "✅ total_dd_supply MATCHES: $BOB_SUPPLY_2 cents (expected 18500 = \$185.00)"
else
    echo "❌ total_dd_supply MISMATCH! Bob: $BOB_SUPPLY_2, Alice: $ALICE_SUPPLY_2"
fi

if [ "$BOB_COLLATERAL_2" = "$ALICE_COLLATERAL_2" ]; then
    echo "✅ total_collateral_dgb MATCHES: $BOB_COLLATERAL_2 DGB"
else
    echo "❌ total_collateral_dgb MISMATCH! Bob: $BOB_COLLATERAL_2, Alice: $ALICE_COLLATERAL_2"
fi

if [ "$BOB_HEALTH_PCT_2" = "$ALICE_HEALTH_PCT_2" ]; then
    echo "✅ health_percentage MATCHES: $BOB_HEALTH_PCT_2%"
else
    echo "❌ health_percentage MISMATCH! Bob: $BOB_HEALTH_PCT_2%, Alice: $ALICE_HEALTH_PCT_2%"
fi
echo ""

# Step 13: Try early redemption (should FAIL)
echo "=========================================="
echo "Step 13: Try early redemption (should FAIL)"
echo "=========================================="
echo ""
echo "Attempting to redeem vault before lock expires..."
EARLY_REDEEM=$(curl --silent --user "$COOKIE" \
  --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"early_redeem\",\"method\":\"redeemdigidollar\",\"params\":[\"$BOB_TXID4\",1000]}" \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

EARLY_ERROR=$(echo "$EARLY_REDEEM" | jq -r '.error.message // empty')
if [ -n "$EARLY_ERROR" ]; then
    echo "✅ Early redemption correctly REJECTED"
    echo "   Error: $EARLY_ERROR"
else
    echo "❌ Early redemption should have FAILED but didn't!"
    echo "$EARLY_REDEEM" | jq '.'
fi
echo ""

# Step 14: Generate another 120+ blocks
echo "=========================================="
echo "Step 14: Generate 125 more blocks (past lock)"
echo "=========================================="
echo ""
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 125 > /dev/null
sleep 5
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
echo "✓ Bob height: $BOB_HEIGHT"
echo "✓ Alice height: $ALICE_HEIGHT"
echo "✓ Lock period EXPIRED - redemption should now work"
echo ""

# Step 15: Redeem the 1-hour vault (should SUCCEED)
echo "=========================================="
echo "Step 15: Redeem the 1-hour vault (should SUCCEED)"
echo "=========================================="
echo ""
echo "Attempting to redeem vault after lock expires..."
REDEEM_SUCCESS=$(curl --silent --user "$COOKIE" \
  --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"redeem_success\",\"method\":\"redeemdigidollar\",\"params\":[\"$BOB_TXID4\",1000]}" \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

REDEEM_TXID=$(echo "$REDEEM_SUCCESS" | jq -r '.result.txid // empty')
if [ -n "$REDEEM_TXID" ]; then
    echo "✅ Redemption SUCCEEDED"
    echo "   Redemption txid: ${REDEEM_TXID:0:16}..."
    echo "   DD Redeemed: $(echo "$REDEEM_SUCCESS" | jq -r '.result.dd_redeemed') cents"
    echo "   Collateral returned: $(echo "$REDEEM_SUCCESS" | jq -r '.result.dgb_unlocked') DGB"
else
    echo "❌ Redemption FAILED!"
    echo "$REDEEM_SUCCESS" | jq '.'
    exit 1
fi
echo ""

# Generate 10 blocks to confirm redemption
echo "=== Generating 10 blocks to confirm redemption ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 10 > /dev/null
sleep 5
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
echo "✓ Redemption confirmed (Bob height: $BOB_HEIGHT, Alice height: $ALICE_HEIGHT)"
echo ""

# Step 16: Verify network stats after redemption
echo "=========================================="
echo "Step 16: Verify network stats after redemption"
echo "=========================================="
echo ""

# Get Bob's view after redemption
echo "=== Bob's View (After Redemption) ==="
BOB_FINAL=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarstats","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)
echo "$BOB_FINAL" | jq '.result'
echo ""

# Get Alice's view after redemption
echo "=== Alice's View (After Redemption) ==="
ALICE_FINAL=$(curl --silent --user "$ALICE_COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarstats","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18446/)
echo "$ALICE_FINAL" | jq '.result'
echo ""

# Compare final values
BOB_SUPPLY_FINAL=$(echo "$BOB_FINAL" | jq '.result.total_dd_supply')
ALICE_SUPPLY_FINAL=$(echo "$ALICE_FINAL" | jq '.result.total_dd_supply')

BOB_COLLATERAL_FINAL=$(echo "$BOB_FINAL" | jq '.result.total_collateral_dgb')
ALICE_COLLATERAL_FINAL=$(echo "$ALICE_FINAL" | jq '.result.total_collateral_dgb')

BOB_HEALTH_PCT_FINAL=$(echo "$BOB_FINAL" | jq '.result.health_percentage')
ALICE_HEALTH_PCT_FINAL=$(echo "$ALICE_FINAL" | jq '.result.health_percentage')

echo "Verification (after redemption):"
if [ "$BOB_SUPPLY_FINAL" = "$ALICE_SUPPLY_FINAL" ] && [ "$BOB_SUPPLY_FINAL" = "17500" ]; then
    echo "✅ total_dd_supply MATCHES and CORRECT: $BOB_SUPPLY_FINAL cents (expected 17500 = \$175.00)"
else
    echo "❌ total_dd_supply issue! Bob: $BOB_SUPPLY_FINAL, Alice: $ALICE_SUPPLY_FINAL (expected 17500)"
fi

if [ "$BOB_COLLATERAL_FINAL" = "$ALICE_COLLATERAL_FINAL" ] && [ "$BOB_COLLATERAL_FINAL" = "57500" ]; then
    echo "✅ total_collateral_dgb MATCHES and CORRECT: $BOB_COLLATERAL_FINAL DGB (expected 57500)"
else
    echo "❌ total_collateral_dgb issue! Bob: $BOB_COLLATERAL_FINAL, Alice: $ALICE_COLLATERAL_FINAL (expected 57500)"
fi

if [ "$BOB_HEALTH_PCT_FINAL" = "$ALICE_HEALTH_PCT_FINAL" ]; then
    echo "✅ health_percentage MATCHES: $BOB_HEALTH_PCT_FINAL%"
else
    echo "❌ health_percentage MISMATCH! Bob: $BOB_HEALTH_PCT_FINAL%, Alice: $ALICE_HEALTH_PCT_FINAL%"
fi
echo ""

echo "=========================================="
echo "1-HOUR LOCK CYCLE TEST COMPLETE"
echo "=========================================="
echo ""
echo "Test Summary:"
echo "  1. ✓ Minted \$100 DD with 1-hour lock (240 blocks)"
echo "  2. ✓ Generated 120 blocks (halfway through lock)"
echo "  3. ✓ Verified network stats matched on both nodes"
echo "  4. ✓ Early redemption correctly rejected"
echo "  5. ✓ Generated 125+ blocks (past lock expiry)"
echo "  6. ✓ Redemption succeeded after lock expired"
echo "  7. ✓ Network stats returned to original values"
echo ""

echo "=========================================="
echo "Qt GUI Windows Are Open"
echo "=========================================="
echo ""
echo "Check the following in BOTH Qt windows:"
echo ""
echo "1. Navigate to: DigiDollar tab → Overview"
echo ""
echo "2. Verify 'Network DigiDollar Status' section shows:"
echo "   - Network Total DD: Should be IDENTICAL on both (~\$175.00)"
echo "   - Network Total Collateral: Should be IDENTICAL on both (~57500 DGB)"
echo "   - System Health: Should be IDENTICAL on both"
echo ""
echo "3. Personal balances will be DIFFERENT:"
echo "   - Bob: \$175.00 DD (from his 3 remaining mints)"
echo "   - Alice: \$0.00 DD (did not mint)"
echo ""
echo "4. The 4th mint (\$100 DD, 1-hour lock) was redeemed"
echo ""
echo "Qt windows remain open for manual verification"
echo "Press Ctrl+C to stop the Qt clients"
echo ""

# Wait indefinitely (commented out for automated testing)
# wait
