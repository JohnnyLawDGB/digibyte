#!/bin/bash
# DigiDollar Qt GUI Network-Wide Tracking Test
# Tests that both Bob and Alice see identical network statistics

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
    -fallbackfee=0.0001 \
    > /tmp/bob_qt.log 2>&1 &
BOB_PID=$!
echo "Bob's Qt started (PID: $BOB_PID)"
sleep 8

# Step 3: Create Bob's wallet and generate 655 blocks
echo "=== Step 3: Creating Bob's wallet and generating 655 blocks ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 createwallet "bob" > /dev/null
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 655 > /dev/null
echo "✓ Bob has 655 blocks"
echo ""

# Step 4: Bob mints DigiDollars
echo "=== Step 4: Bob minting \$100.00 DD ==="
COOKIE=$(cat /tmp/bob_regtest/regtest/.cookie)
BOB_MINT=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"bob_mint","method":"mintdigidollar","params":[10000,4]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

BOB_TXID=$(echo "$BOB_MINT" | jq -r '.result.txid // empty')
if [ -z "$BOB_TXID" ]; then
    echo "❌ Bob's mint failed:"
    echo "$BOB_MINT" | jq '.'
    exit 1
fi

echo "✓ Bob minted \$100 DD (txid: ${BOB_TXID:0:16}...)"
echo "  Collateral: $(echo "$BOB_MINT" | jq -r '.result.dgb_collateral') DGB"
echo ""

# Step 5: Generate 10 blocks to confirm Bob's mint
echo "=== Step 5: Confirming Bob's mint (10 blocks) ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 10 > /dev/null
sleep 3
echo "✓ Bob's mint confirmed (height: $(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount))"
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
    -fallbackfee=0.0001 \
    -connect=127.0.0.1:18444 \
    > /tmp/alice_qt.log 2>&1 &
ALICE_PID=$!
echo "Alice's Qt started (PID: $ALICE_PID)"
sleep 8

# Step 7: Create Alice's wallet and generate blocks
echo "=== Step 7: Creating Alice's wallet and generating 200 blocks ==="
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 createwallet "alice" > /dev/null
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 -generate 200 > /dev/null
sleep 5
echo "✓ Alice synced (height: $(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount))"
echo ""

# Step 8: Alice mints DigiDollars
echo "=== Step 8: Alice minting \$100.00 DD ==="
ALICE_COOKIE=$(cat /tmp/alice_regtest/regtest/.cookie)
ALICE_MINT=$(curl --silent --user "$ALICE_COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"alice_mint","method":"mintdigidollar","params":[10000,3]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18446/)

ALICE_TXID=$(echo "$ALICE_MINT" | jq -r '.result.txid // empty')
if [ -z "$ALICE_TXID" ]; then
    echo "❌ Alice's mint failed:"
    echo "$ALICE_MINT" | jq '.'
    echo ""
    echo "Note: You can manually mint via Alice's Qt GUI:"
    echo "  1. Go to DigiDollar tab → Mint"
    echo "  2. Enter: Amount 100, Lock Period 180 days"
    echo "  3. Click Mint DigiDollars"
    ALICE_MANUAL=true
else
    echo "✓ Alice minted \$100 DD (txid: ${ALICE_TXID:0:16}...)"
    echo "  Collateral: $(echo "$ALICE_MINT" | jq -r '.result.dgb_collateral') DGB"
    ALICE_MANUAL=false
fi
echo ""

# Step 9: Generate 10 blocks to confirm Alice's mint (if successful)
if [ "$ALICE_MANUAL" = false ]; then
    echo "=== Step 9: Confirming Alice's mint (10 blocks) ==="
    ./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 -generate 10 > /dev/null
    sleep 3
    echo "✓ Alice's mint confirmed (height: $(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount))"
    echo ""
fi

# Step 10: Verify network-wide tracking
echo "=========================================="
echo "CRITICAL TEST: Network-Wide Tracking"
echo "=========================================="
echo ""

# Get Bob's view
echo "=== Bob's View (RPC) ==="
BOB_HEALTH=$(curl --silent --user "$COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarsystemhealth","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)
echo "$BOB_HEALTH" | jq '.result'
echo ""

# Get Alice's view
echo "=== Alice's View (RPC) ==="
ALICE_HEALTH=$(curl --silent --user "$ALICE_COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"test","method":"getdigidollarsystemhealth","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18446/)
echo "$ALICE_HEALTH" | jq '.result'
echo ""

# Compare values
BOB_SUPPLY=$(echo "$BOB_HEALTH" | jq '.result.total_dd_supply')
ALICE_SUPPLY=$(echo "$ALICE_HEALTH" | jq '.result.total_dd_supply')

BOB_COLLATERAL=$(echo "$BOB_HEALTH" | jq '.result.total_collateral_dgb')
ALICE_COLLATERAL=$(echo "$ALICE_HEALTH" | jq '.result.total_collateral_dgb')

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
echo "   - Network Total DD: Should be IDENTICAL on both"
echo "   - Network Total Collateral: Should be IDENTICAL on both"
echo "   - System Health: Should be IDENTICAL on both"
echo ""
echo "3. Personal balances will be DIFFERENT:"
echo "   - Bob: ~\$100 DD"
echo "   - Alice: ~\$100 DD (or \$0 if manual mint needed)"
echo ""
echo "Press Ctrl+C when done testing"
echo ""

# Wait indefinitely
wait
