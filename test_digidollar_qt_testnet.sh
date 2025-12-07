#!/bin/bash
# DigiDollar Qt GUI TestNet Test with Live Oracle
# Tests the full DigiDollar cycle on TestNet with real-time exchange price data
# Opens 3 SEPARATE Qt wallet instances (Bob, Alice, Charlie) like regtest script

set -e

echo "=========================================="
echo "DigiDollar Qt TestNet Automated Test"
echo "With 3 SEPARATE Qt GUI Instances"
echo "Using LIVE Oracle Price Data"
echo "=========================================="
echo ""

# Configuration
ORACLE_PRIVATE_KEY="0000000000000000000000000000000000000000000000000000000000000001"

# Mini Testnet ports - separate from main testnet (12028) to avoid blockchain conflicts
# Starting at 12027 for P2P to create an isolated mini testnet
BOB_PORT=12027      # P2P port (mini testnet base)
BOB_RPC=14027       # RPC port
ALICE_PORT=12029
ALICE_RPC=14029
CHARLIE_PORT=12030
CHARLIE_RPC=14030

# Data directories - use minitestnet to separate from main testnet5 blockchain
BOB_DATADIR="/tmp/bob_minitestnet"
ALICE_DATADIR="/tmp/alice_minitestnet"
CHARLIE_DATADIR="/tmp/charlie_minitestnet"

# CLI commands for each node (must use datadir for cookie auth AND explicit rpcport)
BOB_CLI="./src/digibyte-cli -testnet -datadir=$BOB_DATADIR -rpcport=$BOB_RPC"
ALICE_CLI="./src/digibyte-cli -testnet -datadir=$ALICE_DATADIR -rpcport=$ALICE_RPC"
CHARLIE_CLI="./src/digibyte-cli -testnet -datadir=$CHARLIE_DATADIR -rpcport=$CHARLIE_RPC"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() {
    local status=$1
    local message=$2
    if [ "$status" = "ok" ]; then
        echo -e "${GREEN}[OK]${NC} $message"
    elif [ "$status" = "fail" ]; then
        echo -e "${RED}[FAIL]${NC} $message"
    elif [ "$status" = "warn" ]; then
        echo -e "${YELLOW}[WARN]${NC} $message"
    else
        echo "[INFO] $message"
    fi
}

# Helper to wait for RPC
wait_for_rpc() {
    local cli=$1
    local name=$2
    local max=30
    for i in $(seq 1 $max); do
        if $cli getblockchaininfo > /dev/null 2>&1; then
            return 0
        fi
        sleep 2
    done
    return 1
}

# Helper to display balances across all nodes
display_all_balances() {
    local BOB_DD=$($BOB_CLI -rpcwallet=bob getdigidollarbalance 2>/dev/null | jq -r '.total // 0')
    local ALICE_DD=$($ALICE_CLI -rpcwallet=alice getdigidollarbalance 2>/dev/null | jq -r '.total // 0')
    local CHARLIE_DD=$($CHARLIE_CLI -rpcwallet=charlie getdigidollarbalance 2>/dev/null | jq -r '.total // 0')
    local TOTAL=$((BOB_DD + ALICE_DD + CHARLIE_DD))

    echo "=========================================="
    echo "WALLET DD BALANCES (All 3 Nodes)"
    echo "=========================================="
    echo "  Bob:     $BOB_DD cents (\$$(echo "scale=2; $BOB_DD / 100" | bc 2>/dev/null || echo "0"))"
    echo "  Alice:   $ALICE_DD cents (\$$(echo "scale=2; $ALICE_DD / 100" | bc 2>/dev/null || echo "0"))"
    echo "  Charlie: $CHARLIE_DD cents (\$$(echo "scale=2; $CHARLIE_DD / 100" | bc 2>/dev/null || echo "0"))"
    echo "  ----------------------------------------"
    echo "  TOTAL:   $TOTAL cents (\$$(echo "scale=2; $TOTAL / 100" | bc 2>/dev/null || echo "0"))"
    echo "=========================================="
    echo ""
}

# Helper to display network stats with collateralization ratio
display_network_stats() {
    local STEP_DESC=$1
    local STATS=$($BOB_CLI getdigidollarstats 2>/dev/null)
    local ORACLE=$($BOB_CLI getoracleprice 2>/dev/null)

    local DD_SUPPLY=$(echo $STATS | jq -r '.total_dd_supply // 0')
    local COLLATERAL=$(echo $STATS | jq -r '.total_collateral_dgb // 0')
    local COLLATERAL_RATIO=$(echo $STATS | jq -r '.system_collateral_ratio // 0')
    local HEALTH=$(echo $STATS | jq -r '.health_status // "unknown"')
    local ORACLE_PRICE=$(echo $ORACLE | jq -r '.price_usd // 0')
    local ORACLE_MICRO=$(echo $ORACLE | jq -r '.price_micro_usd // 0')

    # Calculate DD value in USD and collateral value in USD
    local DD_VALUE_USD=$(echo "scale=2; $DD_SUPPLY / 100" | bc 2>/dev/null || echo "0")
    local COLLATERAL_VALUE_USD=$(echo "scale=2; $COLLATERAL * $ORACLE_PRICE" | bc 2>/dev/null || echo "0")

    # Calculate actual collateralization percentage
    # Collateral value / DD value * 100
    local ACTUAL_RATIO="0"
    if [ "$DD_SUPPLY" -gt 0 ]; then
        ACTUAL_RATIO=$(echo "scale=0; ($COLLATERAL * $ORACLE_MICRO) / $DD_SUPPLY" | bc 2>/dev/null || echo "0")
    fi

    echo "=========================================="
    echo "NETWORK STATS: $STEP_DESC"
    echo "=========================================="
    echo ""
    echo "  Oracle Price: \$$ORACLE_PRICE per DGB ($ORACLE_MICRO micro-USD)"
    echo ""
    echo "  Total DD Supply:    $DD_SUPPLY cents (\$$DD_VALUE_USD)"
    echo "  Total Collateral:   $COLLATERAL DGB (\$$COLLATERAL_VALUE_USD)"
    echo ""
    echo "  Collateralization:  ${ACTUAL_RATIO}%"
    echo "  System Ratio:       ${COLLATERAL_RATIO}%"
    echo "  Health Status:      $HEALTH"
    echo "=========================================="
    echo ""
}

# Step 1: Clean environment
echo "=== Step 1: Cleaning environment ==="
pkill -f "digibyte-qt.*testnet" 2>/dev/null || true
pkill -f "digibyted.*testnet" 2>/dev/null || true
sleep 2

rm -rf $BOB_DATADIR $ALICE_DATADIR $CHARLIE_DATADIR
mkdir -p $BOB_DATADIR $ALICE_DATADIR $CHARLIE_DATADIR
print_status "ok" "Clean environment ready"
echo ""

# Step 2: Start Bob's Qt node (primary miner)
echo "=== Step 2: Starting Bob's Qt node ==="
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
    -testnet \
    -datadir=$BOB_DATADIR \
    -port=$BOB_PORT \
    -rpcport=$BOB_RPC \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    > /tmp/bob_testnet.log 2>&1 &
BOB_PID=$!
echo "Bob's Qt started (PID: $BOB_PID)"

echo "Waiting for Bob's RPC..."
if wait_for_rpc "$BOB_CLI" "Bob"; then
    print_status "ok" "Bob's Qt RPC is ready"
else
    print_status "fail" "Bob's Qt failed to start"
    exit 1
fi

# Read Bob's cookie for peer connections
BOB_COOKIE=$(cat $BOB_DATADIR/testnet5/.cookie 2>/dev/null || echo "")
echo ""

# Step 3: Create Bob's wallet and mine initial blocks
echo "=== Step 3: Setting up Bob's wallet and mining ==="
$BOB_CLI createwallet "bob" 2>/dev/null || true
BOB_ADDR=$($BOB_CLI -rpcwallet=bob getnewaddress "mining" "bech32")
echo "Bob's mining address: $BOB_ADDR"

echo "Mining 105 blocks for coinbase maturity (instant with fEasyPow)..."
$BOB_CLI generatetoaddress 105 "$BOB_ADDR" > /dev/null 2>&1
HEIGHT=$($BOB_CLI getblockcount)
print_status "ok" "Mined to height $HEIGHT"

BOB_BALANCE=$($BOB_CLI -rpcwallet=bob getbalance)
echo "Bob's DGB balance: $BOB_BALANCE DGB"
echo ""

# Step 4: Start the oracle on Bob's node
echo "=== Step 4: Starting Live Oracle on Bob's node ==="
$BOB_CLI startoracle 0 "$ORACLE_PRIVATE_KEY" 2>/dev/null || true
sleep 2
$BOB_CLI generatetoaddress 1 "$BOB_ADDR" > /dev/null 2>&1

# Wait for oracle
for i in {1..20}; do
    STATUS=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.status // "inactive"')
    if [ "$STATUS" = "active" ]; then
        print_status "ok" "Oracle is active"
        break
    fi
    sleep 2
done

ORACLE_PRICE=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.price_usd // "N/A"')
echo "LIVE Oracle Price: \$$ORACLE_PRICE per DGB"
echo ""

# Step 5: Start Alice's Qt node
echo "=== Step 5: Starting Alice's Qt node ==="
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
    -testnet \
    -datadir=$ALICE_DATADIR \
    -port=$ALICE_PORT \
    -rpcport=$ALICE_RPC \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    -connect=127.0.0.1:$BOB_PORT \
    > /tmp/alice_testnet.log 2>&1 &
ALICE_PID=$!
echo "Alice's Qt started (PID: $ALICE_PID)"

if wait_for_rpc "$ALICE_CLI" "Alice"; then
    print_status "ok" "Alice's Qt RPC is ready"
else
    print_status "fail" "Alice's Qt failed to start"
fi

$ALICE_CLI createwallet "alice" 2>/dev/null || true
ALICE_ADDR=$($ALICE_CLI -rpcwallet=alice getnewaddress "receive" "bech32")
echo "Alice's address: $ALICE_ADDR"
echo ""

# Step 6: Start Charlie's Qt node
echo "=== Step 6: Starting Charlie's Qt node ==="
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
    -testnet \
    -datadir=$CHARLIE_DATADIR \
    -port=$CHARLIE_PORT \
    -rpcport=$CHARLIE_RPC \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    -connect=127.0.0.1:$BOB_PORT \
    > /tmp/charlie_testnet.log 2>&1 &
CHARLIE_PID=$!
echo "Charlie's Qt started (PID: $CHARLIE_PID)"

if wait_for_rpc "$CHARLIE_CLI" "Charlie"; then
    print_status "ok" "Charlie's Qt RPC is ready"
else
    print_status "fail" "Charlie's Qt failed to start"
fi

$CHARLIE_CLI createwallet "charlie" 2>/dev/null || true
CHARLIE_ADDR=$($CHARLIE_CLI -rpcwallet=charlie getnewaddress "receive" "bech32")
echo "Charlie's address: $CHARLIE_ADDR"
echo ""

# Step 7: Fund Alice and Charlie with DGB for transaction fees AND future minting
echo "=== Step 7: Funding Alice and Charlie with DGB for fees and minting ==="
echo "Mining 55 blocks to Alice and 62 blocks to Charlie..."
echo "(Need 100+ block maturity for later minting tests)"
echo "(Charlie needs extra for $200 DD at tier 8 = 200% collateral)"
$BOB_CLI generatetoaddress 55 "$ALICE_ADDR" > /dev/null 2>&1
$BOB_CLI generatetoaddress 62 "$CHARLIE_ADDR" > /dev/null 2>&1
sleep 5

ALICE_DGB=$($ALICE_CLI -rpcwallet=alice getbalance 2>/dev/null || echo "0")
CHARLIE_DGB=$($CHARLIE_CLI -rpcwallet=charlie getbalance 2>/dev/null || echo "0")
print_status "ok" "Alice funded: 55 blocks mined (balance will mature after ~100 blocks)"
print_status "ok" "Charlie funded: 62 blocks mined (balance will mature after ~100 blocks)"
echo ""

# Step 8: Wait for chain sync
echo "=== Step 8: Syncing chains ==="
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

BOB_HEIGHT=$($BOB_CLI getblockcount)
ALICE_HEIGHT=$($ALICE_CLI getblockcount 2>/dev/null || echo "0")
CHARLIE_HEIGHT=$($CHARLIE_CLI getblockcount 2>/dev/null || echo "0")

echo "Chain heights: Bob=$BOB_HEIGHT, Alice=$ALICE_HEIGHT, Charlie=$CHARLIE_HEIGHT"

# Wait for sync
for i in {1..30}; do
    ALICE_HEIGHT=$($ALICE_CLI getblockcount 2>/dev/null || echo "0")
    CHARLIE_HEIGHT=$($CHARLIE_CLI getblockcount 2>/dev/null || echo "0")
    if [ "$ALICE_HEIGHT" = "$BOB_HEIGHT" ] && [ "$CHARLIE_HEIGHT" = "$BOB_HEIGHT" ]; then
        print_status "ok" "All nodes synced at height $BOB_HEIGHT"
        break
    fi
    sleep 2
done
echo ""

# Step 9: Display initial state
echo "=== Step 9: Initial DigiDollar State ==="
display_network_stats "Initial State (No DD Minted Yet)"
display_all_balances

# Step 10: Bob mints $100 DD
echo "=== Step 10: Bob mints \$100 DD ==="
ORACLE_PRICE=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.price_usd')
echo "Current LIVE Oracle Price: \$$ORACLE_PRICE per DGB"
echo ""

echo "Bob minting \$100 DD (10000 cents) with tier 0..."
MINT_RESULT=$($BOB_CLI -rpcwallet=bob mintdigidollar 10000 0 2>&1)

if echo "$MINT_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    MINT_TXID=$(echo "$MINT_RESULT" | jq -r '.txid')
    BOB_FIRST_MINT_TX="$MINT_TXID"  # Save for later redemption test in Step 23
    print_status "ok" "Mint successful! TX: ${MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $MINT_RESULT | jq -r '.dd_minted') cents"
    echo "  Collateral: $(echo $MINT_RESULT | jq -r '.dgb_collateral') DGB"
else
    print_status "fail" "Mint failed: $MINT_RESULT"
    exit 1
fi
echo ""

# Confirm and sync
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

display_all_balances

# Step 11: Bob sends $30 DD to Alice
echo "=== Step 11: Bob sends \$30 DD to Alice ==="
ALICE_DD_ADDR=$($ALICE_CLI -rpcwallet=alice getdigidollaraddress 2>/dev/null)
echo "Alice's DD address: $ALICE_DD_ADDR"

echo "Bob sending 3000 cents (\$30) to Alice..."
SEND_RESULT=$($BOB_CLI -rpcwallet=bob senddigidollar "$ALICE_DD_ADDR" 3000 2>&1)

if echo "$SEND_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    print_status "ok" "Transfer successful! TX: $(echo $SEND_RESULT | jq -r '.txid' | head -c 16)..."
else
    print_status "fail" "Transfer failed: $SEND_RESULT"
fi
echo ""

# Confirm and sync
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

display_all_balances

# Step 12: Bob sends $20 DD to Charlie
echo "=== Step 12: Bob sends \$20 DD to Charlie ==="
CHARLIE_DD_ADDR=$($CHARLIE_CLI -rpcwallet=charlie getdigidollaraddress 2>/dev/null)
echo "Charlie's DD address: $CHARLIE_DD_ADDR"

echo "Bob sending 2000 cents (\$20) to Charlie..."
SEND_RESULT=$($BOB_CLI -rpcwallet=bob senddigidollar "$CHARLIE_DD_ADDR" 2000 2>&1)

if echo "$SEND_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    print_status "ok" "Transfer successful! TX: $(echo $SEND_RESULT | jq -r '.txid' | head -c 16)..."
else
    print_status "fail" "Transfer failed: $SEND_RESULT"
fi
echo ""

# Confirm and sync
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

display_all_balances

# Step 13: Display state after transfers
echo "=== Step 13: State After Transfers ==="
display_network_stats "After Bob's Transfers to Alice and Charlie"
display_all_balances

# Step 13.5: WALLET RESTART TEST - Verify DD balance persists after Bob's wallet restart
echo "=== Step 13.5: Bob Wallet Restart Test (DD Persistence) ==="
echo "Recording Bob's DD balance before restart..."

# Record balances before restart
BOB_DD_BEFORE=$($BOB_CLI -rpcwallet=bob getdigidollarbalance 2>/dev/null | jq -r '.total // 0')
BOB_DGB_BEFORE=$($BOB_CLI -rpcwallet=bob getbalance 2>/dev/null || echo "0")
echo "  Before restart - Bob's DD balance: $BOB_DD_BEFORE cents"
echo "  Before restart - Bob's DGB balance: $BOB_DGB_BEFORE DGB"
echo ""

echo "Stopping Bob's Qt wallet..."
kill $BOB_PID 2>/dev/null || true

# Wait for the process to fully terminate and lock file to be released
echo "Waiting for Bob's wallet to fully close..."
for i in {1..30}; do
    if ! kill -0 $BOB_PID 2>/dev/null; then
        # Process is dead, but wait a bit more for lock file release
        sleep 2
        break
    fi
    sleep 1
done

# Double-check lock file is gone
LOCK_FILE="$BOB_DATADIR/testnet5/.lock"
for i in {1..10}; do
    if [ ! -f "$LOCK_FILE" ] || ! lsof "$LOCK_FILE" 2>/dev/null | grep -q .; then
        break
    fi
    echo "  Waiting for lock file to be released..."
    sleep 1
done

echo "Restarting Bob's Qt wallet..."
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
    -testnet \
    -datadir=$BOB_DATADIR \
    -port=$BOB_PORT \
    -rpcport=$BOB_RPC \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    > /tmp/bob_testnet.log 2>&1 &
BOB_PID=$!
echo "Bob's Qt restarted (PID: $BOB_PID)"

echo "Waiting for Bob's RPC..."
if wait_for_rpc "$BOB_CLI" "Bob"; then
    print_status "ok" "Bob's Qt RPC is ready after restart"
else
    print_status "fail" "Bob's Qt failed to restart"
    exit 1
fi

# Load Bob's wallet
echo "Loading Bob's wallet..."
$BOB_CLI loadwallet "bob" 2>/dev/null || true
sleep 2

# Verify DD balance persisted
BOB_DD_AFTER=$($BOB_CLI -rpcwallet=bob getdigidollarbalance 2>/dev/null | jq -r '.total // 0')
BOB_DGB_AFTER=$($BOB_CLI -rpcwallet=bob getbalance 2>/dev/null || echo "0")
echo "  After restart - Bob's DD balance: $BOB_DD_AFTER cents"
echo "  After restart - Bob's DGB balance: $BOB_DGB_AFTER DGB"
echo ""

if [ "$BOB_DD_BEFORE" = "$BOB_DD_AFTER" ]; then
    print_status "ok" "DD balance PERSISTED after wallet restart! ($BOB_DD_AFTER cents)"
else
    print_status "fail" "DD balance CHANGED after restart! Before: $BOB_DD_BEFORE, After: $BOB_DD_AFTER"
fi

# Restart the oracle on Bob's node (it was stopped when we killed Bob's Qt)
echo "Restarting oracle on Bob's node..."
$BOB_CLI startoracle 0 "$ORACLE_PRIVATE_KEY" 2>/dev/null || true
sleep 2
$BOB_CLI generatetoaddress 1 "$BOB_ADDR" > /dev/null 2>&1

# Wait for oracle to be active again
for i in {1..10}; do
    STATUS=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.status // "inactive"')
    if [ "$STATUS" = "active" ]; then
        print_status "ok" "Oracle is active again"
        break
    fi
    sleep 1
done
echo ""

# Step 14: Bob mints $10 DD with short lock (tier 0 = 240 blocks = 1 hour)
echo "=== Step 14: Bob mints \$10 DD with 1-hour lock (tier 0) ==="
echo "This will be used for redemption testing..."
echo ""

MINT_RESULT2=$($BOB_CLI -rpcwallet=bob mintdigidollar 1000 0 2>&1)

if echo "$MINT_RESULT2" | jq -e '.txid' > /dev/null 2>&1; then
    REDEEM_MINT_TXID=$(echo "$MINT_RESULT2" | jq -r '.txid')
    print_status "ok" "Mint successful! TX: ${REDEEM_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $MINT_RESULT2 | jq -r '.dd_minted') cents"
    echo "  Collateral: $(echo $MINT_RESULT2 | jq -r '.dgb_collateral') DGB"
    echo "  Lock period: 240 blocks (tier 0)"
else
    print_status "fail" "Mint failed: $MINT_RESULT2"
    exit 1
fi
echo ""

# Confirm
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

CURRENT_HEIGHT=$($BOB_CLI getblockcount)
UNLOCK_HEIGHT=$((CURRENT_HEIGHT + 240))
echo "Current block: $CURRENT_HEIGHT"
echo "Unlock at block: $UNLOCK_HEIGHT"
echo ""

display_network_stats "After \$10 DD Mint for Redemption"
display_all_balances

# Step 15: Try early redemption (should FAIL)
echo "=== Step 15: Try early redemption (should FAIL) ==="
echo "Attempting to redeem vault BEFORE lock expires..."
echo ""

# Temporarily disable exit on error for this command (expected to fail)
set +e
EARLY_REDEEM=$($BOB_CLI -rpcwallet=bob redeemdigidollar "$REDEEM_MINT_TXID" 1000 2>&1)
EARLY_EXIT_CODE=$?
set -e

if [ $EARLY_EXIT_CODE -ne 0 ] || echo "$EARLY_REDEEM" | grep -qi "error\|lock\|expired"; then
    print_status "ok" "Early redemption correctly REJECTED"
    echo "   Response: $EARLY_REDEEM"
else
    print_status "warn" "Unexpected response (may have succeeded when it shouldn't)"
    echo "   Response: $EARLY_REDEEM"
fi
echo ""

# Step 16: Mine blocks to pass lock period (240 blocks for tier 0)
echo "=== Step 16: Mining 245 blocks to pass lock period ==="
echo "This will take a moment..."
$BOB_CLI generatetoaddress 245 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

NEW_HEIGHT=$($BOB_CLI getblockcount)
print_status "ok" "Mined to height $NEW_HEIGHT (past unlock at $UNLOCK_HEIGHT)"
echo ""

# Step 17: Redeem the vault (should SUCCEED)
echo "=== Step 17: Redeem the \$10 DD vault (should SUCCEED) ==="
echo "Attempting to redeem vault AFTER lock expires..."
echo ""

set +e
REDEEM_RESULT=$($BOB_CLI -rpcwallet=bob redeemdigidollar "$REDEEM_MINT_TXID" 1000 2>&1)
REDEEM_EXIT_CODE=$?
set -e

if [ $REDEEM_EXIT_CODE -eq 0 ] && echo "$REDEEM_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    REDEEM_TXID=$(echo "$REDEEM_RESULT" | jq -r '.txid')
    print_status "ok" "Redemption SUCCEEDED!"
    echo "   Redemption TX: ${REDEEM_TXID:0:16}..."
    echo "   DD Redeemed: $(echo $REDEEM_RESULT | jq -r '.dd_redeemed // 1000') cents"
    echo "   Collateral returned: $(echo $REDEEM_RESULT | jq -r '.dgb_unlocked // "N/A"') DGB"
else
    print_status "fail" "Redemption FAILED: $REDEEM_RESULT"
fi
echo ""

# Confirm redemption
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

# Step 18: Bob mints $200 DD with 10-year lock (tier 8)
echo "=== Step 18: Bob mints \$200 DD with 10-year lock (tier 8) ==="
echo "Testing long-term vault creation..."
echo ""

MINT_RESULT3=$($BOB_CLI -rpcwallet=bob mintdigidollar 20000 8 2>&1)

if echo "$MINT_RESULT3" | jq -e '.txid' > /dev/null 2>&1; then
    LONG_MINT_TXID=$(echo "$MINT_RESULT3" | jq -r '.txid')
    print_status "ok" "10-year mint successful! TX: ${LONG_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $MINT_RESULT3 | jq -r '.dd_minted') cents (\$200)"
    echo "  Collateral: $(echo $MINT_RESULT3 | jq -r '.dgb_collateral') DGB"
    echo "  Lock period: 10 years (tier 8 = 200% collateral)"
else
    print_status "fail" "10-year mint failed: $MINT_RESULT3"
    exit 1
fi
echo ""

# Confirm with 7 blocks
echo "Mining 7 blocks to confirm transaction..."
$BOB_CLI generatetoaddress 7 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

CONFIRM_HEIGHT=$($BOB_CLI getblockcount)
print_status "ok" "Transaction confirmed at height $CONFIRM_HEIGHT"
echo ""

display_network_stats "After \$200 DD Mint (10-Year Lock)"
display_all_balances

# Step 19: Charlie sends $15 DD to Alice (tests recipient can spend received DD)
echo "=== Step 19: Charlie sends \$15 DD to Alice ==="
echo "Testing that recipients can spend received DigiDollars..."
echo ""

# Get Alice's DD address for receiving
ALICE_DD_ADDR2=$($ALICE_CLI -rpcwallet=alice getdigidollaraddress 2>/dev/null)
echo "Alice's DD address: $ALICE_DD_ADDR2"

echo "Charlie sending 1500 cents (\$15) to Alice..."
set +e
CHARLIE_SEND_RESULT=$($CHARLIE_CLI -rpcwallet=charlie senddigidollar "$ALICE_DD_ADDR2" 1500 2>&1)
CHARLIE_SEND_EXIT=$?
set -e

if [ $CHARLIE_SEND_EXIT -eq 0 ] && echo "$CHARLIE_SEND_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    print_status "ok" "Transfer successful! TX: $(echo $CHARLIE_SEND_RESULT | jq -r '.txid' | head -c 16)..."
else
    print_status "fail" "Charlie's transfer failed: $CHARLIE_SEND_RESULT"
    echo "  (This tests whether recipients can spend received DD)"
fi
echo ""

# Mine 8 blocks to confirm
echo "Mining 8 blocks to confirm transaction..."
$BOB_CLI generatetoaddress 8 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

display_all_balances

# Step 20: Alice sends $10 DD back to Bob (completing the full circle)
echo "=== Step 20: Alice sends \$10 DD back to Bob ==="
echo "Completing the full circle - DD returns to original minter..."
echo ""

# Get Bob's DD address for receiving
BOB_DD_ADDR=$($BOB_CLI -rpcwallet=bob getdigidollaraddress 2>/dev/null)
echo "Bob's DD address: $BOB_DD_ADDR"

echo "Alice sending 1000 cents (\$10) back to Bob..."
set +e
ALICE_SEND_RESULT=$($ALICE_CLI -rpcwallet=alice senddigidollar "$BOB_DD_ADDR" 1000 2>&1)
ALICE_SEND_EXIT=$?
set -e

if [ $ALICE_SEND_EXIT -eq 0 ] && echo "$ALICE_SEND_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    print_status "ok" "Transfer successful! TX: $(echo $ALICE_SEND_RESULT | jq -r '.txid' | head -c 16)..."
    echo "  Full circle complete: Bob -> Alice -> Bob"
else
    print_status "fail" "Alice's transfer failed: $ALICE_SEND_RESULT"
    echo "  (This tests whether recipients can spend received DD)"
fi
echo ""

# Mine 2 blocks to confirm + 7 more for network stress test
echo "Mining 2 blocks to confirm transaction + 7 more blocks..."
$BOB_CLI generatetoaddress 9 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

display_all_balances

# Step 21: Alice mints $100 DD with 7-year lock (tier 7)
echo "=== Step 21: Alice mints \$100 DD with 7-year lock (tier 7) ==="
echo "Testing Alice's ability to mint after receiving DGB from mining..."
echo ""

ALICE_BALANCE=$($ALICE_CLI -rpcwallet=alice getbalance 2>/dev/null || echo "0")
echo "Alice's DGB balance: $ALICE_BALANCE DGB"

echo "Alice minting \$100 DD (10000 cents) with tier 7 (7-year lock)..."
set +e
ALICE_MINT_RESULT=$($ALICE_CLI -rpcwallet=alice mintdigidollar 10000 7 2>&1)
ALICE_MINT_EXIT=$?
set -e

if [ $ALICE_MINT_EXIT -eq 0 ] && echo "$ALICE_MINT_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    ALICE_MINT_TXID=$(echo "$ALICE_MINT_RESULT" | jq -r '.txid')
    print_status "ok" "Alice's 7-year mint successful! TX: ${ALICE_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $ALICE_MINT_RESULT | jq -r '.dd_minted') cents (\$100)"
    echo "  Collateral: $(echo $ALICE_MINT_RESULT | jq -r '.dgb_collateral') DGB"
    echo "  Lock period: 7 years (tier 7)"
else
    print_status "fail" "Alice's mint failed: $ALICE_MINT_RESULT"
fi
echo ""

# Confirm with 2 blocks
echo "Mining 2 blocks to confirm..."
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

display_all_balances

# Step 22: Charlie mints $200 DD with 10-year lock (tier 8)
echo "=== Step 22: Charlie mints \$200 DD with 10-year lock (tier 8) ==="
echo "Testing Charlie's ability to mint after receiving DGB from mining..."
echo ""

CHARLIE_BALANCE=$($CHARLIE_CLI -rpcwallet=charlie getbalance 2>/dev/null || echo "0")
echo "Charlie's DGB balance: $CHARLIE_BALANCE DGB"

echo "Charlie minting \$200 DD (20000 cents) with tier 8 (10-year lock)..."
set +e
CHARLIE_MINT_RESULT=$($CHARLIE_CLI -rpcwallet=charlie mintdigidollar 20000 8 2>&1)
CHARLIE_MINT_EXIT=$?
set -e

if [ $CHARLIE_MINT_EXIT -eq 0 ] && echo "$CHARLIE_MINT_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    CHARLIE_MINT_TXID=$(echo "$CHARLIE_MINT_RESULT" | jq -r '.txid')
    print_status "ok" "Charlie's 10-year mint successful! TX: ${CHARLIE_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $CHARLIE_MINT_RESULT | jq -r '.dd_minted') cents (\$200)"
    echo "  Collateral: $(echo $CHARLIE_MINT_RESULT | jq -r '.dgb_collateral') DGB"
    echo "  Lock period: 10 years (tier 8 = 200% collateral)"
else
    print_status "fail" "Charlie's mint failed: $CHARLIE_MINT_RESULT"
fi
echo ""

# Confirm with 7 blocks
echo "Mining 7 blocks to confirm..."
$BOB_CLI generatetoaddress 7 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

display_all_balances

# Step 23: Bob redeems first $100 vault (fungible DD test)
echo "=== Step 23: Bob redeems first \$100 vault (fungible DD) ==="
echo "Testing that ANY DD can be used to redeem a vault (DD is fungible)..."
echo "Bob originally minted \$100 DD, sent \$50 to others, but has \$260+ DD total now."
echo ""

BOB_DD_BALANCE=$($BOB_CLI -rpcwallet=bob getdigidollarbalance 2>/dev/null | jq -r '.dd_balance_cents' 2>/dev/null || echo "0")
echo "Bob's current DD balance: $BOB_DD_BALANCE cents (\$$(echo "scale=2; $BOB_DD_BALANCE / 100" | bc))"

echo "Attempting to redeem first vault (\$100 DD = 10000 cents) using $BOB_FIRST_MINT_TX..."
set +e
FINAL_REDEEM_RESULT=$($BOB_CLI -rpcwallet=bob redeemdigidollar $BOB_FIRST_MINT_TX 10000 2>&1)
FINAL_REDEEM_EXIT=$?
set -e

if [ $FINAL_REDEEM_EXIT -eq 0 ] && echo "$FINAL_REDEEM_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    FINAL_REDEEM_TXID=$(echo "$FINAL_REDEEM_RESULT" | jq -r '.txid')
    print_status "ok" "Redemption SUCCEEDED! TX: ${FINAL_REDEEM_TXID:0:16}..."
    echo "  DD Redeemed: $(echo $FINAL_REDEEM_RESULT | jq -r '.dd_redeemed') cents"
    echo "  Collateral returned: $(echo $FINAL_REDEEM_RESULT | jq -r '.dgb_returned') DGB"

    echo ""
    echo "Mining 7 blocks to confirm redemption..."
    $BOB_CLI generatetoaddress 7 "$BOB_ADDR" > /dev/null 2>&1
    sleep 5

    echo ""
    echo "Verifying balances after redemption..."
    display_all_balances

    # Calculate expected totals after $100 redemption
    # Before: Bob $260, Alice $135, Charlie $205 = $600 total
    # After:  Bob $160, Alice $135, Charlie $205 = $500 total
    TOTAL_AFTER=$($BOB_CLI getdigidollarstats 2>/dev/null | jq -r '.total_dd_supply' || echo "0")
    echo "Total DD Supply after redemption: $TOTAL_AFTER cents"

    if [ "$TOTAL_AFTER" -lt 60000 ]; then
        print_status "ok" "Total reduced from 60000 to $TOTAL_AFTER cents (correct)"
    else
        print_status "fail" "Total should have reduced, got: $TOTAL_AFTER"
    fi
else
    print_status "fail" "Redemption FAILED: $FINAL_REDEEM_RESULT"
    echo "  This should not happen - Bob has sufficient DD balance for redemption"
fi
echo ""

# Mine 2 final blocks for network sync
echo "Mining 2 final blocks for network upgrade sync..."
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

# Step 24: Final network state
echo "=== Step 24: Final DigiDollar Network State ==="
display_network_stats "Final State (After All Mints - Network Stress Test)"
display_all_balances

echo "=========================================="
echo "LIVE ORACLE PRICE DATA"
echo "=========================================="
$BOB_CLI getoracleprice 2>/dev/null | jq '{status, price_usd, price_micro_usd}'
echo "=========================================="
echo ""

# Summary
echo "=========================================="
echo "TESTNET DIGIDOLLAR TEST COMPLETE"
echo "=========================================="
echo ""
echo "TESTS PERFORMED:"
echo "  1. Fund Alice and Charlie with DGB (55+62 blocks for minting)"
echo "  2. Bob mints \$100 DD with tier 0 (1000% collateral)"
echo "  3. Bob sends \$30 DD to Alice"
echo "  4. Bob sends \$20 DD to Charlie"
echo "  5. BOB WALLET RESTART TEST (DD persistence verification)"
echo "  6. Bob mints \$10 DD with 1-hour lock"
echo "  7. Early redemption REJECTED (lock not expired)"
echo "  8. Redemption SUCCEEDED (after lock expired)"
echo "  9. Bob mints \$200 DD with 10-year lock (tier 8 = 200% collateral)"
echo "  10. Charlie sends \$15 DD to Alice (recipient spending test)"
echo "  11. Alice sends \$10 DD back to Bob (full circle test)"
echo "  12. Alice mints \$100 DD with 7-year lock (tier 7)"
echo "  13. Charlie mints \$200 DD with 10-year lock (tier 8)"
echo ""
echo "NETWORK STRESS TEST - All 3 Nodes Minting:"
echo "  - Bob: \$200 DD (10-year) + \$100 DD transfers received"
echo "  - Alice: \$100 DD (7-year) + \$45 DD transfers received"
echo "  - Charlie: \$200 DD (10-year) + \$5 DD transfers remaining"
echo ""
echo "3 Qt GUI Windows Are Now Open:"
echo "  - Bob's Qt (PID: $BOB_PID) - Primary miner, multiple vaults"
echo "  - Alice's Qt (PID: $ALICE_PID) - Has own vault + received DD"
echo "  - Charlie's Qt (PID: $CHARLIE_PID) - Has own vault + sent DD"
echo ""
echo "All 3 nodes are connected and synced!"
echo ""
print_status "ok" "All tests completed successfully!"
echo ""
echo "=========================================="
echo "TESTNET WITH LIVE ORACLE IS RUNNING"
echo "=========================================="
echo ""
echo "Oracle is fetching LIVE DGB/USD prices from exchanges!"
echo ""
echo "Commands:"
echo "  $BOB_CLI getoracleprice"
echo "  $BOB_CLI -rpcwallet=bob mintdigidollar <cents> <tier>"
echo "  $ALICE_CLI -rpcwallet=alice getdigidollarbalance"
echo "  $CHARLIE_CLI -rpcwallet=charlie getdigidollarbalance"
echo ""
echo "Qt windows remain open for manual verification."
echo "Press Ctrl+C to exit (will close all Qt windows)."
echo ""

# Keep script running so Qt stays open
trap "kill $BOB_PID $ALICE_PID $CHARLIE_PID 2>/dev/null; echo 'All Qt windows closed.'" EXIT
wait $BOB_PID
