#!/bin/bash
# DigiDollar Qt GUI Network-Wide Tracking Test
# Tests network-wide statistics and DD transfers between Bob, Alice, and Charlie
# Comprehensive balance tracking and verification throughout all operations

set -e

echo "=========================================="
echo "DigiDollar Qt RegTest Automated Test"
echo "Enhanced with 3-wallet DD transfers"
echo "=========================================="
echo ""

# Helper function to get network stats with retry logic
get_network_stats() {
    local NODE_NAME=$1
    local RPC_PORT=$2
    local COOKIE=$3
    local MAX_RETRIES=5
    local RETRY_COUNT=0

    while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
        local STATS=$(curl --silent --user "$COOKIE" \
            --data-binary '{"jsonrpc":"1.0","id":"stats","method":"getdigidollarstats","params":[]}' \
            -H 'content-type: text/plain;' \
            http://127.0.0.1:${RPC_PORT}/)

        # Check if we got valid data
        local DD_SUPPLY=$(echo "$STATS" | jq -r '.result.total_dd_supply // empty')

        if [ -n "$DD_SUPPLY" ]; then
            echo "$STATS"
            return 0
        fi

        # Retry with delay
        RETRY_COUNT=$((RETRY_COUNT + 1))
        if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
            sleep 2
        fi
    done

    # Return the last response even if invalid (for error reporting)
    echo "$STATS"
}

# Helper function to get wallet DD balance with retry logic
get_dd_balance() {
    local NODE_NAME=$1
    local RPC_PORT=$2
    local COOKIE=$3
    local WALLET_NAME=$(echo "$NODE_NAME" | tr '[:upper:]' '[:lower:]')  # bob, alice, charlie
    local MAX_RETRIES=5
    local RETRY_COUNT=0

    while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
        local BALANCE=$(curl --silent --user "$COOKIE" \
            --data-binary '{"jsonrpc":"1.0","id":"balance","method":"getdigidollarbalance","params":[]}' \
            -H 'content-type: text/plain;' \
            http://127.0.0.1:${RPC_PORT}/wallet/${WALLET_NAME})

        local TOTAL=$(echo "$BALANCE" | jq -r '.result.total // empty')

        if [ -n "$TOTAL" ]; then
            echo "$TOTAL"
            return 0
        fi

        # Retry with delay
        RETRY_COUNT=$((RETRY_COUNT + 1))
        if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
            sleep 2
        fi
    done

    # Return 0 if all retries failed
    echo "0"
}

# Helper function to display network monitoring
display_network_monitor() {
    local STEP_DESC=$1

    echo "=========================================="
    echo "NETWORK MONITOR: $STEP_DESC"
    echo "=========================================="
    echo ""

    # Get Bob's network stats
    BOB_STATS=$(get_network_stats "Bob" 18443 "$BOB_COOKIE")
    BOB_DD_SUPPLY=$(echo "$BOB_STATS" | jq -r '.result.total_dd_supply // 0')
    BOB_COLLATERAL=$(echo "$BOB_STATS" | jq -r '.result.total_collateral_dgb // 0')
    BOB_HEALTH=$(echo "$BOB_STATS" | jq -r '.result.health_percentage // 0')

    # Get Alice's network stats
    ALICE_STATS=$(get_network_stats "Alice" 18446 "$ALICE_COOKIE")
    ALICE_DD_SUPPLY=$(echo "$ALICE_STATS" | jq -r '.result.total_dd_supply // 0')
    ALICE_COLLATERAL=$(echo "$ALICE_STATS" | jq -r '.result.total_collateral_dgb // 0')
    ALICE_HEALTH=$(echo "$ALICE_STATS" | jq -r '.result.health_percentage // 0')

    # Get Charlie's network stats
    CHARLIE_STATS=$(get_network_stats "Charlie" 18447 "$CHARLIE_COOKIE")
    CHARLIE_DD_SUPPLY=$(echo "$CHARLIE_STATS" | jq -r '.result.total_dd_supply // 0')
    CHARLIE_COLLATERAL=$(echo "$CHARLIE_STATS" | jq -r '.result.total_collateral_dgb // 0')
    CHARLIE_HEALTH=$(echo "$CHARLIE_STATS" | jq -r '.result.health_percentage // 0')

    # Get individual wallet balances
    BOB_BALANCE=$(get_dd_balance "Bob" 18443 "$BOB_COOKIE")
    ALICE_BALANCE=$(get_dd_balance "Alice" 18446 "$ALICE_COOKIE")
    CHARLIE_BALANCE=$(get_dd_balance "Charlie" 18447 "$CHARLIE_COOKIE")

    echo "Network-Wide Statistics (all nodes should match):"
    echo "  Bob's view:     Total DD: $BOB_DD_SUPPLY cents | Collateral: $BOB_COLLATERAL DGB | Health: $BOB_HEALTH%"
    echo "  Alice's view:   Total DD: $ALICE_DD_SUPPLY cents | Collateral: $ALICE_COLLATERAL DGB | Health: $ALICE_HEALTH%"
    echo "  Charlie's view: Total DD: $CHARLIE_DD_SUPPLY cents | Collateral: $CHARLIE_COLLATERAL DGB | Health: $CHARLIE_HEALTH%"
    echo ""

    # Verify network stats match
    if [ "$BOB_DD_SUPPLY" = "$ALICE_DD_SUPPLY" ] && [ "$BOB_DD_SUPPLY" = "$CHARLIE_DD_SUPPLY" ]; then
        echo "✅ Network Total DD Supply MATCHES on all nodes: $BOB_DD_SUPPLY cents (\$$(echo "scale=2; $BOB_DD_SUPPLY / 100" | bc))"
    else
        echo "❌ Network Total DD Supply MISMATCH!"
        exit 1
    fi

    if [ "$BOB_COLLATERAL" = "$ALICE_COLLATERAL" ] && [ "$BOB_COLLATERAL" = "$CHARLIE_COLLATERAL" ]; then
        echo "✅ Network Total Collateral MATCHES on all nodes: $BOB_COLLATERAL DGB"
    else
        echo "❌ Network Total Collateral MISMATCH!"
        exit 1
    fi

    if [ "$BOB_HEALTH" = "$ALICE_HEALTH" ] && [ "$BOB_HEALTH" = "$CHARLIE_HEALTH" ]; then
        echo "✅ Network Health MATCHES on all nodes: $BOB_HEALTH%"
    else
        echo "❌ Network Health MISMATCH!"
        exit 1
    fi

    echo ""
    echo "Individual Wallet DD Balances:"
    echo "  Bob:     $BOB_BALANCE cents (\$$(echo "scale=2; $BOB_BALANCE / 100" | bc))"
    echo "  Alice:   $ALICE_BALANCE cents (\$$(echo "scale=2; $ALICE_BALANCE / 100" | bc))"
    echo "  Charlie: $CHARLIE_BALANCE cents (\$$(echo "scale=2; $CHARLIE_BALANCE / 100" | bc))"
    echo ""

    # Verify conservation of DD (sum of balances = network total)
    TOTAL_BALANCE=$((BOB_BALANCE + ALICE_BALANCE + CHARLIE_BALANCE))
    if [ "$TOTAL_BALANCE" = "$BOB_DD_SUPPLY" ]; then
        echo "✅ DD CONSERVATION: Sum of wallet balances ($TOTAL_BALANCE) = Network total ($BOB_DD_SUPPLY)"
    else
        echo "❌ DD CONSERVATION VIOLATION: Sum ($TOTAL_BALANCE) != Network total ($BOB_DD_SUPPLY)"
        exit 1
    fi
    echo ""
}

# Step 1: Clean environment
echo "=== Step 1: Cleaning environment ==="
pkill -f "digibyte-qt.*regtest" 2>/dev/null || true
pkill -f "digibyted.*regtest" 2>/dev/null || true
sleep 2
rm -rf ~/.digibyte/regtest 2>/dev/null || true  # Linux path
rm -rf ~/Library/Application\ Support/DigiByte/regtest 2>/dev/null || true  # macOS path
rm -rf /tmp/bob_regtest
rm -rf /tmp/alice_regtest
rm -rf /tmp/charlie_regtest
echo "✓ Clean environment ready"
echo ""

# Step 2: Start Bob's node
echo "=== Step 2: Starting Bob's Qt node ==="
mkdir -p /tmp/bob_regtest

# Use clean environment with only essential display variables (works on Linux/Mac)
env -i \
    DISPLAY="${DISPLAY}" \
    XAUTHORITY="${XAUTHORITY}" \
    WAYLAND_DISPLAY="${WAYLAND_DISPLAY}" \
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR}" \
    XDG_SESSION_TYPE="${XDG_SESSION_TYPE}" \
    HOME="${HOME}" \
    USER="${USER}" \
    PATH="${PATH}" \
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

# Step 3: Create Bob's wallet and generate 700 blocks
echo "=== Step 3: Creating Bob's wallet and generating 700 blocks ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 createwallet "bob" > /dev/null
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 700 > /dev/null
echo "✓ Bob has 700 blocks (ensuring enough mature UTXOs for mints)"
BOB_COOKIE=$(cat /tmp/bob_regtest/regtest/.cookie)
echo ""

# Step 4: Set oracle price
echo "=== Step 4: Setting mock oracle price ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 setmockoracleprice 1 > /dev/null
ORACLE_PRICE=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getmockoracleprice | jq -r '.price_usd')
echo "✓ Oracle price set to: $ORACLE_PRICE per DGB"
echo ""

# Step 5: Bob mints DigiDollars (3 separate mints)
echo "=== Step 5: Bob minting DigiDollars (3 separate mints) ==="

# Mint #1: $100.00 DD with 365 day lock (tier 4)
echo "Mint #1: \$100.00 DD (365 days, tier 4)"
BOB_MINT1=$(curl --silent --user "$BOB_COOKIE" \
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
BOB_MINT2=$(curl --silent --user "$BOB_COOKIE" \
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
BOB_MINT3=$(curl --silent --user "$BOB_COOKIE" \
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
sleep 10
echo "  Waiting for wallet to process blocks..."
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
echo "✓ Bob's mints confirmed (height: $BOB_HEIGHT)"
echo ""

# Step 7: Start Alice's node
echo "=== Step 7: Starting Alice's Qt node ==="
mkdir -p /tmp/alice_regtest

# Use clean environment with only essential display variables (works on Linux/Mac)
env -i \
    DISPLAY="${DISPLAY}" \
    XAUTHORITY="${XAUTHORITY}" \
    WAYLAND_DISPLAY="${WAYLAND_DISPLAY}" \
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR}" \
    XDG_SESSION_TYPE="${XDG_SESSION_TYPE}" \
    HOME="${HOME}" \
    USER="${USER}" \
    PATH="${PATH}" \
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
sleep 8
echo "  Waiting for sync and balance calculation..."
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
echo "✓ Alice synced (height: $ALICE_HEIGHT)"
echo "✓ Alice's oracle price set"
ALICE_COOKIE=$(cat /tmp/alice_regtest/regtest/.cookie)
echo ""

# Step 9: Start Charlie's node
echo "=== Step 9: Starting Charlie's Qt node ==="
mkdir -p /tmp/charlie_regtest

# Use clean environment with only essential display variables (works on Linux/Mac)
env -i \
    DISPLAY="${DISPLAY}" \
    XAUTHORITY="${XAUTHORITY}" \
    WAYLAND_DISPLAY="${WAYLAND_DISPLAY}" \
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR}" \
    XDG_SESSION_TYPE="${XDG_SESSION_TYPE}" \
    HOME="${HOME}" \
    USER="${USER}" \
    PATH="${PATH}" \
    ./src/qt/digibyte-qt \
    -regtest \
    -datadir=/tmp/charlie_regtest \
    -port=18448 \
    -rpcport=18447 \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    -connect=127.0.0.1:18444 \
    > /tmp/charlie_qt.log 2>&1 &
CHARLIE_PID=$!
echo "Charlie's Qt started (PID: $CHARLIE_PID)"
sleep 8

# Step 10: Create Charlie's wallet and set oracle price
echo "=== Step 10: Creating Charlie's wallet, setting oracle price, and syncing ==="
./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 createwallet "charlie" > /dev/null
./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 setmockoracleprice 1 > /dev/null
sleep 8
echo "  Waiting for sync and balance calculation..."
CHARLIE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 getblockcount)
echo "✓ Charlie synced (height: $CHARLIE_HEIGHT)"
echo "✓ Charlie's oracle price set"
CHARLIE_COOKIE=$(cat /tmp/charlie_regtest/regtest/.cookie)
echo ""

# Monitor network state after Bob's mints
echo "  Waiting for network-wide stats to propagate..."
sleep 5
display_network_monitor "After Bob's 3 Mints"

# Step 11: Bob mints $10 DD with 1-hour lock
echo "=========================================="
echo "Step 11: Bob mints \$10 DD with 1-hour lock"
echo "=========================================="
echo ""

# Generate 200 more blocks to ensure Bob has enough UTXOs for 1000% collateral
echo "=== Generating 200 blocks for additional UTXOs ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 200 > /dev/null
sleep 10
echo "  Waiting for wallet to process blocks..."
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
echo "✓ Bob height: $BOB_HEIGHT"
echo ""

echo "Mint #4: \$10.00 DD (1 hour, tier 0 = 240 blocks, 1000% = 10000 DGB)"
BOB_MINT4=$(curl --silent --user "$BOB_COOKIE" \
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

MINT4_TXID=$(echo "$BOB_MINT4" | jq -r '.result.txid')
sleep 2

# Generate 2 blocks to confirm
echo "=== Generating 2 blocks to confirm mint ==="
ADDR=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob getnewaddress)
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob generatetoaddress 2 "$ADDR" > /dev/null
sleep 10
echo "  Waiting for wallet to process blocks..."
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)

# Verify the transaction is actually confirmed
TX_CONFIRMATIONS=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob gettransaction "$MINT4_TXID" 2>/dev/null | jq -r '.confirmations // 0')
if [ "$TX_CONFIRMATIONS" -eq 0 ]; then
    echo "⚠️  WARNING: Mint transaction still NOT confirmed after 2 blocks!"
    echo "Attempting to mine 10 more blocks..."
    ./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -rpcwallet=bob generatetoaddress 10 "$ADDR" > /dev/null
    sleep 10
    echo "  Waiting for wallet to process blocks..."
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

# Monitor network state after 4th mint
echo "  Waiting for network-wide stats to propagate..."
sleep 5
display_network_monitor "After Bob's 4th Mint (\$10 DD, 1-hour lock)"

# Step 12: Generate 120 blocks (halfway through lock period)
echo "=========================================="
echo "Step 12: Generate 120 blocks (halfway through lock)"
echo "=========================================="
echo ""
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 120 > /dev/null
sleep 10
echo "  Waiting for wallet to process blocks..."
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
CHARLIE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 getblockcount)
echo "✓ Bob height: $BOB_HEIGHT"
echo "✓ Alice height: $ALICE_HEIGHT"
echo "✓ Charlie height: $CHARLIE_HEIGHT"
echo "✓ Blocks until unlock: $((BOB_HEIGHT + 118))"
echo ""

# Monitor network state halfway through lock
echo "  Waiting for network-wide stats to propagate..."
sleep 5
display_network_monitor "Halfway Through Lock Period (120 blocks)"

# Step 13: Try early redemption (should FAIL)
echo "=========================================="
echo "Step 13: Try early redemption (should FAIL)"
echo "=========================================="
echo ""
echo "Attempting to redeem vault before lock expires..."
EARLY_REDEEM=$(curl --silent --user "$BOB_COOKIE" \
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

# Step 14: Generate another 125 blocks (past lock expiry)
echo "=========================================="
echo "Step 14: Generate 125 more blocks (past lock)"
echo "=========================================="
echo ""
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 125 > /dev/null
sleep 10
echo "  Waiting for wallet to process blocks..."
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
CHARLIE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 getblockcount)
echo "✓ Bob height: $BOB_HEIGHT"
echo "✓ Alice height: $ALICE_HEIGHT"
echo "✓ Charlie height: $CHARLIE_HEIGHT"
echo "✓ Lock period EXPIRED - redemption should now work"
echo ""

# Step 15: Redeem the 1-hour vault (should SUCCEED)
echo "=========================================="
echo "Step 15: Redeem the 1-hour vault (should SUCCEED)"
echo "=========================================="
echo ""
echo "Attempting to redeem vault after lock expires..."
REDEEM_SUCCESS=$(curl --silent --user "$BOB_COOKIE" \
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
sleep 10
echo "  Waiting for wallet to process blocks..."
BOB_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getblockcount)
ALICE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getblockcount)
CHARLIE_HEIGHT=$(./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 getblockcount)
echo "✓ Redemption confirmed (Bob: $BOB_HEIGHT, Alice: $ALICE_HEIGHT, Charlie: $CHARLIE_HEIGHT)"
echo ""

# CRITICAL: Verify redemption transaction returns exact collateral amount
echo "=========================================="
echo "CRITICAL: Verify Redemption Transaction"
echo "=========================================="
echo ""

# Get the mint transaction to find collateral amount
MINT_TX=$(curl --silent --user "$BOB_COOKIE" \
  --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"test\",\"method\":\"getrawtransaction\",\"params\":[\"$BOB_TXID4\",true]}" \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

MINT_COLLATERAL=$(echo "$MINT_TX" | jq -r '.result.vout[0].value')
echo "Mint transaction locked: $MINT_COLLATERAL DGB as collateral"

# Get the redemption transaction
REDEEM_TX=$(curl --silent --user "$BOB_COOKIE" \
  --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"test\",\"method\":\"getrawtransaction\",\"params\":[\"$REDEEM_TXID\",true]}" \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/)

# Output 0 should be the returned collateral
REDEEM_COLLATERAL=$(echo "$REDEEM_TX" | jq -r '.result.vout[0].value')
echo "Redemption transaction returned: $REDEEM_COLLATERAL DGB"

# Verify they match EXACTLY
if [ "$MINT_COLLATERAL" = "$REDEEM_COLLATERAL" ]; then
    echo ""
    echo "✅✅✅ PASS: Redemption returns EXACTLY the locked collateral ✅✅✅"
    echo "   Locked:   $MINT_COLLATERAL DGB"
    echo "   Returned: $REDEEM_COLLATERAL DGB"
    echo "   Difference: 0 DGB"
else
    echo ""
    echo "❌❌❌ FAIL: Redemption amount mismatch! ❌❌❌"
    echo "   Locked:   $MINT_COLLATERAL DGB"
    echo "   Returned: $REDEEM_COLLATERAL DGB"
    echo "   Difference: $(echo "$MINT_COLLATERAL - $REDEEM_COLLATERAL" | bc) DGB"
    exit 1
fi
echo ""

# Monitor network state after redemption
echo "  Waiting for network-wide stats to propagate..."
sleep 5
display_network_monitor "After Redemption of 4th Mint"

# Step 16: NEW - Bob sends DD to Alice and Charlie
echo "=========================================="
echo "Step 16: DigiDollar Transfer Tests"
echo "=========================================="
echo ""

# Get Alice's DD receive address
echo "Getting Alice's DD receive address..."
ALICE_DD_ADDR=$(curl --silent --user "$ALICE_COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"getaddr","method":"getdigidollaraddress","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18446/wallet/alice | jq -r '.result')
echo "✓ Alice's DD address: $ALICE_DD_ADDR"

# Get Charlie's DD receive address
echo "Getting Charlie's DD receive address..."
CHARLIE_DD_ADDR=$(curl --silent --user "$CHARLIE_COOKIE" \
  --data-binary '{"jsonrpc":"1.0","id":"getaddr","method":"getdigidollaraddress","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18447/wallet/charlie | jq -r '.result')
echo "✓ Charlie's DD address: $CHARLIE_DD_ADDR"
echo ""

# Transfer 1: Bob sends $34.67 to Alice
echo "=== Transfer #1: Bob → Alice (\$34.67) ==="
echo "Before transfer:"
BOB_BAL_BEFORE=$(get_dd_balance "Bob" 18443 "$BOB_COOKIE")
ALICE_BAL_BEFORE=$(get_dd_balance "Alice" 18446 "$ALICE_COOKIE")
echo "  Bob:   $BOB_BAL_BEFORE cents (\$$(echo "scale=2; $BOB_BAL_BEFORE / 100" | bc))"
echo "  Alice: $ALICE_BAL_BEFORE cents (\$$(echo "scale=2; $ALICE_BAL_BEFORE / 100" | bc))"
echo ""

TRANSFER1=$(curl --silent --user "$BOB_COOKIE" \
  --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"transfer1\",\"method\":\"senddigidollar\",\"params\":[\"$ALICE_DD_ADDR\",3467]}" \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/wallet/bob)

TRANSFER1_TXID=$(echo "$TRANSFER1" | jq -r '.result.txid // empty')
if [ -z "$TRANSFER1_TXID" ]; then
    echo "❌ Transfer #1 failed:"
    echo "$TRANSFER1" | jq '.'
    exit 1
fi
echo "✓ Transfer sent, txid: ${TRANSFER1_TXID:0:16}..."
sleep 2

# Confirm transfer
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 5 > /dev/null
sleep 10
echo "  Waiting for wallet to process blocks..."
echo "✓ Transfer confirmed"
echo ""

echo "After transfer:"
BOB_BAL_AFTER1=$(get_dd_balance "Bob" 18443 "$BOB_COOKIE")
ALICE_BAL_AFTER1=$(get_dd_balance "Alice" 18446 "$ALICE_COOKIE")
echo "  Bob:   $BOB_BAL_AFTER1 cents (\$$(echo "scale=2; $BOB_BAL_AFTER1 / 100" | bc))"
echo "  Alice: $ALICE_BAL_AFTER1 cents (\$$(echo "scale=2; $ALICE_BAL_AFTER1 / 100" | bc))"
echo ""

# Verify transfer amounts
BOB_CHANGE=$((BOB_BAL_BEFORE - BOB_BAL_AFTER1))
ALICE_CHANGE=$((ALICE_BAL_AFTER1 - ALICE_BAL_BEFORE))
echo "Balance changes:"
echo "  Bob decreased by:   $BOB_CHANGE cents (expected: 3467)"
echo "  Alice increased by: $ALICE_CHANGE cents (expected: 3467)"

if [ "$BOB_CHANGE" -eq 3467 ] && [ "$ALICE_CHANGE" -eq 3467 ]; then
    echo "✅ Transfer #1 amounts verified correctly"
else
    echo "❌ Transfer #1 amounts incorrect!"
    exit 1
fi
echo ""

# Monitor network state after first transfer
echo "  Waiting for network-wide stats to propagate..."
sleep 5
display_network_monitor "After Transfer #1 (Bob → Alice \$34.67)"

# Transfer 2: Bob sends $12.53 to Charlie
echo "=== Transfer #2: Bob → Charlie (\$12.53) ==="
echo "Before transfer:"
BOB_BAL_BEFORE2=$(get_dd_balance "Bob" 18443 "$BOB_COOKIE")
CHARLIE_BAL_BEFORE=$(get_dd_balance "Charlie" 18447 "$CHARLIE_COOKIE")
echo "  Bob:     $BOB_BAL_BEFORE2 cents (\$$(echo "scale=2; $BOB_BAL_BEFORE2 / 100" | bc))"
echo "  Charlie: $CHARLIE_BAL_BEFORE cents (\$$(echo "scale=2; $CHARLIE_BAL_BEFORE / 100" | bc))"
echo ""

TRANSFER2=$(curl --silent --user "$BOB_COOKIE" \
  --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"transfer2\",\"method\":\"senddigidollar\",\"params\":[\"$CHARLIE_DD_ADDR\",1253]}" \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:18443/wallet/bob)

TRANSFER2_TXID=$(echo "$TRANSFER2" | jq -r '.result.txid // empty')
if [ -z "$TRANSFER2_TXID" ]; then
    echo "❌ Transfer #2 failed:"
    echo "$TRANSFER2" | jq '.'
    exit 1
fi
echo "✓ Transfer sent, txid: ${TRANSFER2_TXID:0:16}..."
sleep 2

# Confirm transfer
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 -generate 5 > /dev/null
sleep 10
echo "  Waiting for wallet to process blocks..."
echo "✓ Transfer confirmed"
echo ""

echo "After transfer:"
BOB_BAL_AFTER2=$(get_dd_balance "Bob" 18443 "$BOB_COOKIE")
CHARLIE_BAL_AFTER=$(get_dd_balance "Charlie" 18447 "$CHARLIE_COOKIE")
echo "  Bob:     $BOB_BAL_AFTER2 cents (\$$(echo "scale=2; $BOB_BAL_AFTER2 / 100" | bc))"
echo "  Charlie: $CHARLIE_BAL_AFTER cents (\$$(echo "scale=2; $CHARLIE_BAL_AFTER / 100" | bc))"
echo ""

# Verify transfer amounts
BOB_CHANGE2=$((BOB_BAL_BEFORE2 - BOB_BAL_AFTER2))
CHARLIE_CHANGE=$((CHARLIE_BAL_AFTER - CHARLIE_BAL_BEFORE))
echo "Balance changes:"
echo "  Bob decreased by:     $BOB_CHANGE2 cents (expected: 1253)"
echo "  Charlie increased by: $CHARLIE_CHANGE cents (expected: 1253)"

if [ "$BOB_CHANGE2" -eq 1253 ] && [ "$CHARLIE_CHANGE" -eq 1253 ]; then
    echo "✅ Transfer #2 amounts verified correctly"
else
    echo "❌ Transfer #2 amounts incorrect!"
    exit 1
fi
echo ""

# Monitor network state after second transfer
echo "  Waiting for network-wide stats to propagate..."
sleep 5
display_network_monitor "After Transfer #2 (Bob → Charlie \$12.53)"

# Step 17: Final comprehensive balance verification
echo "=========================================="
echo "Step 17: Final Balance Verification"
echo "=========================================="
echo ""

# Calculate expected final balances
# Bob started with $175.00 (17500 cents)
# Bob sent $34.67 (3467 cents) to Alice
# Bob sent $12.53 (1253 cents) to Charlie
# Bob expected: 17500 - 3467 - 1253 = 12780 cents ($127.80)
EXPECTED_BOB=12780
EXPECTED_ALICE=3467
EXPECTED_CHARLIE=1253
EXPECTED_NETWORK_TOTAL=17500

echo "Expected final balances:"
echo "  Bob:     $EXPECTED_BOB cents (\$$(echo "scale=2; $EXPECTED_BOB / 100" | bc))"
echo "  Alice:   $EXPECTED_ALICE cents (\$$(echo "scale=2; $EXPECTED_ALICE / 100" | bc))"
echo "  Charlie: $EXPECTED_CHARLIE cents (\$$(echo "scale=2; $EXPECTED_CHARLIE / 100" | bc))"
echo "  Network: $EXPECTED_NETWORK_TOTAL cents (\$$(echo "scale=2; $EXPECTED_NETWORK_TOTAL / 100" | bc))"
echo ""

# Get actual balances
ACTUAL_BOB=$(get_dd_balance "Bob" 18443 "$BOB_COOKIE")
ACTUAL_ALICE=$(get_dd_balance "Alice" 18446 "$ALICE_COOKIE")
ACTUAL_CHARLIE=$(get_dd_balance "Charlie" 18447 "$CHARLIE_COOKIE")

# Get network total
BOB_STATS_FINAL=$(get_network_stats "Bob" 18443 "$BOB_COOKIE")
ACTUAL_NETWORK=$(echo "$BOB_STATS_FINAL" | jq -r '.result.total_dd_supply')

echo "Actual final balances:"
echo "  Bob:     $ACTUAL_BOB cents (\$$(echo "scale=2; $ACTUAL_BOB / 100" | bc))"
echo "  Alice:   $ACTUAL_ALICE cents (\$$(echo "scale=2; $ACTUAL_ALICE / 100" | bc))"
echo "  Charlie: $ACTUAL_CHARLIE cents (\$$(echo "scale=2; $ACTUAL_CHARLIE / 100" | bc))"
echo "  Network: $ACTUAL_NETWORK cents (\$$(echo "scale=2; $ACTUAL_NETWORK / 100" | bc))"
echo ""

echo "=========================================="
echo "Final Verification Results:"
echo "=========================================="

# Verify Bob's balance
if [ "$ACTUAL_BOB" -eq "$EXPECTED_BOB" ]; then
    echo "✅ Bob's balance CORRECT: $ACTUAL_BOB cents"
else
    echo "❌ Bob's balance INCORRECT! Expected: $EXPECTED_BOB, Got: $ACTUAL_BOB"
    exit 1
fi

# Verify Alice's balance
if [ "$ACTUAL_ALICE" -eq "$EXPECTED_ALICE" ]; then
    echo "✅ Alice's balance CORRECT: $ACTUAL_ALICE cents"
else
    echo "❌ Alice's balance INCORRECT! Expected: $EXPECTED_ALICE, Got: $ACTUAL_ALICE"
    exit 1
fi

# Verify Charlie's balance
if [ "$ACTUAL_CHARLIE" -eq "$EXPECTED_CHARLIE" ]; then
    echo "✅ Charlie's balance CORRECT: $ACTUAL_CHARLIE cents"
else
    echo "❌ Charlie's balance INCORRECT! Expected: $EXPECTED_CHARLIE, Got: $ACTUAL_CHARLIE"
    exit 1
fi

# Verify network total
if [ "$ACTUAL_NETWORK" -eq "$EXPECTED_NETWORK_TOTAL" ]; then
    echo "✅ Network total CORRECT: $ACTUAL_NETWORK cents"
else
    echo "❌ Network total INCORRECT! Expected: $EXPECTED_NETWORK_TOTAL, Got: $ACTUAL_NETWORK"
    exit 1
fi

# Verify conservation
ACTUAL_SUM=$((ACTUAL_BOB + ACTUAL_ALICE + ACTUAL_CHARLIE))
if [ "$ACTUAL_SUM" -eq "$ACTUAL_NETWORK" ]; then
    echo "✅ DD CONSERVATION verified: Sum of balances ($ACTUAL_SUM) = Network total ($ACTUAL_NETWORK)"
else
    echo "❌ DD CONSERVATION VIOLATION! Sum: $ACTUAL_SUM, Network: $ACTUAL_NETWORK"
    exit 1
fi

echo ""
echo "=========================================="
echo "ALL TESTS PASSED!"
echo "=========================================="
echo ""
echo "Test Summary:"
echo "  1. ✓ Bob minted \$175.00 DD (3 long-term vaults)"
echo "  2. ✓ Bob minted \$10.00 DD with 1-hour lock"
echo "  3. ✓ Alice and Charlie synced and saw same network stats"
echo "  4. ✓ Network stats matched across all 3 nodes at every step"
echo "  5. ✓ Early redemption correctly rejected"
echo "  6. ✓ Redemption succeeded after lock expired"
echo "  7. ✓ Exact collateral amount returned on redemption"
echo "  8. ✓ Bob sent \$34.67 DD to Alice"
echo "  9. ✓ Bob sent \$12.53 DD to Charlie"
echo " 10. ✓ All final balances verified correct"
echo " 11. ✓ DD conservation verified (sum = network total)"
echo ""

echo "=========================================="
echo "Qt GUI Windows Are Open"
echo "=========================================="
echo ""
echo "Check the following in ALL THREE Qt windows:"
echo ""
echo "1. Navigate to: DigiDollar tab → Overview"
echo ""
echo "2. Verify 'Network DigiDollar Status' section shows:"
echo "   - Network Total DD: Should be IDENTICAL on all 3 (~\$175.00)"
echo "   - Network Total Collateral: Should be IDENTICAL on all 3 (~57500 DGB)"
echo "   - System Health: Should be IDENTICAL on all 3"
echo ""
echo "3. Personal balances should be DIFFERENT:"
echo "   - Bob:     \$127.80 DD (12780 cents)"
echo "   - Alice:   \$34.67 DD (3467 cents)"
echo "   - Charlie: \$12.53 DD (1253 cents)"
echo ""
echo "4. The 4th mint (\$10 DD, 1-hour lock) was redeemed"
echo ""
echo "5. Transfer history should show:"
echo "   - Bob: 2 sends to Alice and Charlie"
echo "   - Alice: 1 receive from Bob"
echo "   - Charlie: 1 receive from Bob"
echo ""
echo "Qt windows remain open for manual verification"
echo "Process IDs:"
echo "  Bob:     $BOB_PID"
echo "  Alice:   $ALICE_PID"
echo "  Charlie: $CHARLIE_PID"
echo ""
echo "Press Ctrl+C to stop the Qt clients"
echo ""

# Wait indefinitely - keeps Qt windows open for manual inspection
wait
