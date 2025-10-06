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

# Step 4: Bob mints DigiDollars (3 separate mints)
echo "=== Step 4: Bob minting DigiDollars (3 separate mints) ==="
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

# Step 5: Bob generates 10 blocks to confirm his mints
echo "=== Step 5: Bob generating 10 blocks to confirm mints ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 10 > /dev/null
sleep 3
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
echo "✓ Bob's mints confirmed (height: $BOB_HEIGHT)"
echo ""

# Step 6: Start Alice's node
echo "=== Step 6: Starting Alice's Qt node ==="
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
    -connect=127.0.0.1:18444 \
    > /tmp/alice_qt.log 2>&1 &
ALICE_PID=$!
echo "Alice's Qt started (PID: $ALICE_PID)"
sleep 8

# Step 7: Create Alice's wallet (she only observes, doesn't mint)
echo "=== Step 7: Creating Alice's wallet and syncing ==="
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 createwallet "alice" > /dev/null
sleep 5
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
echo "✓ Alice synced (height: $ALICE_HEIGHT)"
ALICE_COOKIE=$(cat /tmp/alice_regtest/regtest/.cookie)
echo ""

# Step 8: Verify network-wide tracking
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
echo "   - Bob: \$175.00 DD (from his 3 mints)"
echo "   - Alice: \$0.00 DD (did not mint)"
echo ""
echo "Press Ctrl+C when done testing"
echo ""

# Wait indefinitely
wait
