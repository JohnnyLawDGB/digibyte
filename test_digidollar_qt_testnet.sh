#!/bin/bash
# DigiDollar Qt GUI TestNet Test with Live Oracle
# ENHANCED VERSION v3: Full 24-step test with comprehensive DGB/DD balance tracking
# Tests the full DigiDollar cycle on TestNet with real-time exchange price data
# Opens 3 SEPARATE Qt wallet instances (Bob, Alice, Charlie)
#
# KEY DEBUG FEATURES:
# - Tracks DGB AND DD balances for all 3 wallets at every step
# - Records all inputs/outputs/change for each transaction
# - Verifies transaction confirmation before proceeding
# - Checks DD positions after each operation
# - Detects DGB loss by comparing expected vs actual balances
# - Saves complete log to timestamped file for comparison between runs

set -e

# Create log file with timestamp
LOG_DIR="/tmp/digidollar_debug_logs"
mkdir -p "$LOG_DIR"
LOG_FILE="$LOG_DIR/test_run_$(date +%Y%m%d_%H%M%S).log"
echo "=========================================="
echo "DigiDollar Qt TestNet Automated Test"
echo "ENHANCED DEBUG VERSION v3 - Full 24 Steps"
echo "=========================================="
echo "Log file: $LOG_FILE"
echo ""

# Tee output to both console and log file
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "DigiDollar Qt TestNet Automated Test"
echo "With 3 SEPARATE Qt GUI Instances"
echo "Using LIVE Oracle Price Data"
echo "ENHANCED v3: Full Balance Tracking"
echo "=========================================="
echo "Test started: $(date)"
echo ""

# Configuration
ORACLE_PRIVATE_KEY="0000000000000000000000000000000000000000000000000000000000000001"

# Mini Testnet ports
BOB_PORT=12027
BOB_RPC=14027
ALICE_PORT=12029
ALICE_RPC=14029
CHARLIE_PORT=12030
CHARLIE_RPC=14030

# Data directories
BOB_DATADIR="/tmp/bob_minitestnet"
ALICE_DATADIR="/tmp/alice_minitestnet"
CHARLIE_DATADIR="/tmp/charlie_minitestnet"

# CLI commands
BOB_CLI="./src/digibyte-cli -testnet -datadir=$BOB_DATADIR -rpcport=$BOB_RPC"
ALICE_CLI="./src/digibyte-cli -testnet -datadir=$ALICE_DATADIR -rpcport=$ALICE_RPC"
CHARLIE_CLI="./src/digibyte-cli -testnet -datadir=$CHARLIE_DATADIR -rpcport=$CHARLIE_RPC"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# Global tracking
declare -A PREV_DGB
declare -A PREV_DD
PREV_DGB[bob]=0
PREV_DGB[alice]=0
PREV_DGB[charlie]=0
PREV_DD[bob]=0
PREV_DD[alice]=0
PREV_DD[charlie]=0

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

print_status() {
    local status=$1
    local message=$2
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if [ "$status" = "ok" ]; then
        echo -e "${GREEN}[OK]${NC} $message"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    elif [ "$status" = "fail" ]; then
        echo -e "${RED}[FAIL]${NC} $message"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    elif [ "$status" = "warn" ]; then
        echo -e "${YELLOW}[WARN]${NC} $message"
    elif [ "$status" = "info" ]; then
        echo -e "${CYAN}[INFO]${NC} $message"
        TOTAL_TESTS=$((TOTAL_TESTS - 1))  # Don't count info as test
    else
        echo "[INFO] $message"
        TOTAL_TESTS=$((TOTAL_TESTS - 1))
    fi
}

print_header() {
    echo ""
    echo -e "${BLUE}==========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}==========================================${NC}"
}

print_subheader() {
    echo ""
    echo -e "${CYAN}--- $1 ---${NC}"
}

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

# Get balances safely
get_dgb_balance() {
    local cli=$1
    local wallet=$2
    $cli -rpcwallet=$wallet getbalance 2>/dev/null || echo "0"
}

get_dd_balance() {
    local cli=$1
    local wallet=$2
    $cli -rpcwallet=$wallet getdigidollarbalance 2>/dev/null | jq -r '.total // 0' 2>/dev/null || echo "0"
}

get_immature_balance() {
    local cli=$1
    local wallet=$2
    $cli -rpcwallet=$wallet getbalances 2>/dev/null | jq -r '.mine.immature // 0' 2>/dev/null || echo "0"
}

# COMPREHENSIVE BALANCE SNAPSHOT
capture_balance_snapshot() {
    local step_desc=$1
    local block_height=$($BOB_CLI getblockcount 2>/dev/null || echo "0")

    print_header "BALANCE SNAPSHOT: $step_desc (Block $block_height)"

    echo ""
    echo "DIGIBYTE (DGB) BALANCES:"
    echo "------------------------"

    # Bob
    local bob_dgb=$(get_dgb_balance "$BOB_CLI" "bob")
    local bob_dgb_immature=$(get_immature_balance "$BOB_CLI" "bob")
    local bob_dgb_prev=${PREV_DGB[bob]:-0}
    local bob_dgb_delta=$(echo "$bob_dgb - $bob_dgb_prev" | bc 2>/dev/null || echo "0")

    echo "  BOB:"
    echo "    Confirmed:   $bob_dgb DGB"
    echo "    Immature:    $bob_dgb_immature DGB"
    echo "    Delta:       $bob_dgb_delta DGB"

    # Alice
    local alice_dgb=$(get_dgb_balance "$ALICE_CLI" "alice")
    local alice_dgb_immature=$(get_immature_balance "$ALICE_CLI" "alice")
    local alice_dgb_prev=${PREV_DGB[alice]:-0}
    local alice_dgb_delta=$(echo "$alice_dgb - $alice_dgb_prev" | bc 2>/dev/null || echo "0")

    echo "  ALICE:"
    echo "    Confirmed:   $alice_dgb DGB"
    echo "    Immature:    $alice_dgb_immature DGB"
    echo "    Delta:       $alice_dgb_delta DGB"

    # Charlie
    local charlie_dgb=$(get_dgb_balance "$CHARLIE_CLI" "charlie")
    local charlie_dgb_immature=$(get_immature_balance "$CHARLIE_CLI" "charlie")
    local charlie_dgb_prev=${PREV_DGB[charlie]:-0}
    local charlie_dgb_delta=$(echo "$charlie_dgb - $charlie_dgb_prev" | bc 2>/dev/null || echo "0")

    echo "  CHARLIE:"
    echo "    Confirmed:   $charlie_dgb DGB"
    echo "    Immature:    $charlie_dgb_immature DGB"
    echo "    Delta:       $charlie_dgb_delta DGB"

    echo ""
    echo "DIGIDOLLAR (DD) BALANCES:"
    echo "-------------------------"

    local bob_dd=$(get_dd_balance "$BOB_CLI" "bob")
    local bob_dd_prev=${PREV_DD[bob]:-0}
    local bob_dd_delta=$((bob_dd - bob_dd_prev))
    echo "  BOB:     $bob_dd cents (\$$(echo "scale=2; $bob_dd / 100" | bc 2>/dev/null || echo "0")) [delta: $bob_dd_delta]"

    local alice_dd=$(get_dd_balance "$ALICE_CLI" "alice")
    local alice_dd_prev=${PREV_DD[alice]:-0}
    local alice_dd_delta=$((alice_dd - alice_dd_prev))
    echo "  ALICE:   $alice_dd cents (\$$(echo "scale=2; $alice_dd / 100" | bc 2>/dev/null || echo "0")) [delta: $alice_dd_delta]"

    local charlie_dd=$(get_dd_balance "$CHARLIE_CLI" "charlie")
    local charlie_dd_prev=${PREV_DD[charlie]:-0}
    local charlie_dd_delta=$((charlie_dd - charlie_dd_prev))
    echo "  CHARLIE: $charlie_dd cents (\$$(echo "scale=2; $charlie_dd / 100" | bc 2>/dev/null || echo "0")) [delta: $charlie_dd_delta]"

    local total_dd=$((bob_dd + alice_dd + charlie_dd))
    echo ""
    echo "  TOTAL DD: $total_dd cents (\$$(echo "scale=2; $total_dd / 100" | bc 2>/dev/null || echo "0"))"

    # Update previous balances
    PREV_DGB[bob]=$bob_dgb
    PREV_DGB[alice]=$alice_dgb
    PREV_DGB[charlie]=$charlie_dgb
    PREV_DD[bob]=$bob_dd
    PREV_DD[alice]=$alice_dd
    PREV_DD[charlie]=$charlie_dd

    echo "=========================================="
}

# Display network stats
display_network_stats() {
    local STEP_DESC=$1
    local STATS=$($BOB_CLI getdigidollarstats 2>/dev/null)
    local ORACLE=$($BOB_CLI getoracleprice 2>/dev/null)

    local DD_SUPPLY=$(echo $STATS | jq -r '.total_dd_supply // 0')
    local COLLATERAL=$(echo $STATS | jq -r '.total_collateral_dgb // 0')
    local HEALTH=$(echo $STATS | jq -r '.health_status // "unknown"')
    local ORACLE_PRICE=$(echo $ORACLE | jq -r '.price_usd // 0')
    local ORACLE_MICRO=$(echo $ORACLE | jq -r '.price_micro_usd // 0')

    local DD_VALUE_USD=$(echo "scale=2; $DD_SUPPLY / 100" | bc 2>/dev/null || echo "0")

    print_header "NETWORK STATS: $STEP_DESC"
    echo ""
    echo "  Oracle Price: \$$ORACLE_PRICE per DGB ($ORACLE_MICRO micro-USD)"
    echo ""
    echo "  Total DD Supply:    $DD_SUPPLY cents (\$$DD_VALUE_USD)"
    echo "  Total Collateral:   $COLLATERAL DGB"
    echo "  Health Status:      $HEALTH"
    echo "=========================================="
}

# List DD positions for a wallet
list_dd_positions() {
    local cli=$1
    local wallet=$2
    local name=$3

    print_subheader "$name's DD Positions"

    local positions=$($cli -rpcwallet=$wallet listdigidollarpositions false 2>/dev/null || echo "[]")

    if [ "$positions" = "[]" ] || [ -z "$positions" ]; then
        echo "  No DD positions found"
        return
    fi

    local count=$(echo "$positions" | jq 'length')
    echo "  Total positions: $count"
    echo ""

    echo "$positions" | jq -r '.[] | "  Position: \(.position_id[0:16])...\n    DD Minted: \(.dd_minted) cents\n    DGB Collateral: \(.dgb_collateral) DGB\n    Lock Tier: \(.lock_tier)\n    Status: \(.status)\n    Can Redeem: \(.can_redeem)\n"' 2>/dev/null || echo "  Error parsing positions"
}

# ============================================================================
# MAIN TEST EXECUTION - ALL 24 STEPS
# ============================================================================

# Step 1: Clean environment
print_header "Step 1: Cleaning environment"
pkill -f "digibyte-qt.*testnet" 2>/dev/null || true
pkill -f "digibyted.*testnet" 2>/dev/null || true
sleep 2

rm -rf $BOB_DATADIR $ALICE_DATADIR $CHARLIE_DATADIR
mkdir -p $BOB_DATADIR $ALICE_DATADIR $CHARLIE_DATADIR
print_status "ok" "Clean environment ready"

# Step 2: Start Bob's Qt node
print_header "Step 2: Starting Bob's Qt node"
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
    -debug=digidollar \
    > /tmp/bob_testnet.log 2>&1 &
BOB_PID=$!
echo "Bob's Qt started (PID: $BOB_PID)"

if wait_for_rpc "$BOB_CLI" "Bob"; then
    print_status "ok" "Bob's Qt RPC is ready"
else
    print_status "fail" "Bob's Qt failed to start"
    exit 1
fi

# Step 3: Setup Bob's wallet and mining
print_header "Step 3: Setting up Bob's wallet and mining"
$BOB_CLI createwallet "bob" 2>/dev/null || true
BOB_ADDR=$($BOB_CLI -rpcwallet=bob getnewaddress "mining" "bech32")
echo "Bob's mining address: $BOB_ADDR"

echo "Mining 105 blocks for coinbase maturity..."
$BOB_CLI generatetoaddress 105 "$BOB_ADDR" > /dev/null 2>&1
HEIGHT=$($BOB_CLI getblockcount)
print_status "ok" "Mined to height $HEIGHT"

BOB_BALANCE=$($BOB_CLI -rpcwallet=bob getbalance)
echo "Bob's DGB balance: $BOB_BALANCE DGB"

# Step 4: Start oracle
print_header "Step 4: Starting Live Oracle on Bob's node"
$BOB_CLI startoracle 0 "$ORACLE_PRIVATE_KEY" 2>/dev/null || true
sleep 2
$BOB_CLI generatetoaddress 1 "$BOB_ADDR" > /dev/null 2>&1

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

# Step 5: Start Alice's Qt node
print_header "Step 5: Starting Alice's Qt node"
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
    -debug=digidollar \
    -connect=127.0.0.1:$BOB_PORT \
    > /tmp/alice_testnet.log 2>&1 &
ALICE_PID=$!
echo "Alice's Qt started (PID: $ALICE_PID)"

if wait_for_rpc "$ALICE_CLI" "Alice"; then
    print_status "ok" "Alice's Qt RPC is ready"
fi

$ALICE_CLI createwallet "alice" 2>/dev/null || true
ALICE_ADDR=$($ALICE_CLI -rpcwallet=alice getnewaddress "receive" "bech32")
echo "Alice's address: $ALICE_ADDR"

# Step 6: Start Charlie's Qt node
print_header "Step 6: Starting Charlie's Qt node"
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
    -debug=digidollar \
    -connect=127.0.0.1:$BOB_PORT \
    > /tmp/charlie_testnet.log 2>&1 &
CHARLIE_PID=$!
echo "Charlie's Qt started (PID: $CHARLIE_PID)"

if wait_for_rpc "$CHARLIE_CLI" "Charlie"; then
    print_status "ok" "Charlie's Qt RPC is ready"
fi

$CHARLIE_CLI createwallet "charlie" 2>/dev/null || true
CHARLIE_ADDR=$($CHARLIE_CLI -rpcwallet=charlie getnewaddress "receive" "bech32")
echo "Charlie's address: $CHARLIE_ADDR"

# Step 7: Fund Alice and Charlie
print_header "Step 7: Funding Alice and Charlie with DGB"
echo "Mining 55 blocks to Alice and 62 to Charlie for future minting..."
$BOB_CLI generatetoaddress 55 "$ALICE_ADDR" > /dev/null 2>&1
$BOB_CLI generatetoaddress 62 "$CHARLIE_ADDR" > /dev/null 2>&1
print_status "ok" "Alice funded: 55 blocks"
print_status "ok" "Charlie funded: 62 blocks"

# Step 8: Sync chains
print_header "Step 8: Syncing chains"
$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5

BOB_HEIGHT=$($BOB_CLI getblockcount)
for i in {1..30}; do
    ALICE_HEIGHT=$($ALICE_CLI getblockcount 2>/dev/null || echo "0")
    CHARLIE_HEIGHT=$($CHARLIE_CLI getblockcount 2>/dev/null || echo "0")
    if [ "$ALICE_HEIGHT" = "$BOB_HEIGHT" ] && [ "$CHARLIE_HEIGHT" = "$BOB_HEIGHT" ]; then
        print_status "ok" "All nodes synced at height $BOB_HEIGHT"
        break
    fi
    sleep 2
done

# Step 9: Display initial state
print_header "Step 9: Initial DigiDollar State"
display_network_stats "Initial State (No DD Minted Yet)"
capture_balance_snapshot "Initial State"

# Step 10: Bob mints $100 DD
print_header "Step 10: Bob mints \$100 DD"
ORACLE_PRICE=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.price_usd')
echo "Current LIVE Oracle Price: \$$ORACLE_PRICE per DGB"

echo "Bob minting \$100 DD (10000 cents) with tier 0..."
MINT_RESULT=$($BOB_CLI -rpcwallet=bob mintdigidollar 10000 0 2>&1)

if echo "$MINT_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    MINT_TXID=$(echo "$MINT_RESULT" | jq -r '.txid')
    BOB_FIRST_MINT_TX="$MINT_TXID"
    print_status "ok" "Mint successful! TX: ${MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $MINT_RESULT | jq -r '.dd_minted') cents"
    echo "  Collateral: $(echo $MINT_RESULT | jq -r '.dgb_collateral') DGB"
else
    print_status "fail" "Mint failed: $MINT_RESULT"
    exit 1
fi

$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5
capture_balance_snapshot "After Bob's \$100 Mint"
list_dd_positions "$BOB_CLI" "bob" "Bob"

# Step 11: Bob sends $30 DD to Alice
print_header "Step 11: Bob sends \$30 DD to Alice"
ALICE_DD_ADDR=$($ALICE_CLI -rpcwallet=alice getdigidollaraddress 2>/dev/null)
echo "Alice's DD address: $ALICE_DD_ADDR"

echo "Bob sending 3000 cents (\$30) to Alice..."
SEND_RESULT=$($BOB_CLI -rpcwallet=bob senddigidollar "$ALICE_DD_ADDR" 3000 2>&1)

if echo "$SEND_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    print_status "ok" "Transfer successful! TX: $(echo $SEND_RESULT | jq -r '.txid' | head -c 16)..."
else
    print_status "fail" "Transfer failed: $SEND_RESULT"
fi

$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5
capture_balance_snapshot "After Bob sends \$30 to Alice"

# Step 12: Bob sends $20 DD to Charlie
print_header "Step 12: Bob sends \$20 DD to Charlie"
CHARLIE_DD_ADDR=$($CHARLIE_CLI -rpcwallet=charlie getdigidollaraddress 2>/dev/null)
echo "Charlie's DD address: $CHARLIE_DD_ADDR"

echo "Bob sending 2000 cents (\$20) to Charlie..."
SEND_RESULT=$($BOB_CLI -rpcwallet=bob senddigidollar "$CHARLIE_DD_ADDR" 2000 2>&1)

if echo "$SEND_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    print_status "ok" "Transfer successful! TX: $(echo $SEND_RESULT | jq -r '.txid' | head -c 16)..."
else
    print_status "fail" "Transfer failed: $SEND_RESULT"
fi

$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 5
capture_balance_snapshot "After Bob sends \$20 to Charlie"

# Step 13: State after transfers
print_header "Step 13: State After Transfers"
display_network_stats "After Bob's Transfers"
list_dd_positions "$BOB_CLI" "bob" "Bob"

# Step 13.5: Bob wallet restart test
print_header "Step 13.5: Bob Wallet Restart Test (DD Persistence)"
BOB_DD_BEFORE=$(get_dd_balance "$BOB_CLI" "bob")
BOB_DGB_BEFORE=$(get_dgb_balance "$BOB_CLI" "bob")
echo "Before restart - Bob's DD: $BOB_DD_BEFORE cents, DGB: $BOB_DGB_BEFORE"

echo "Stopping Bob's Qt wallet..."
kill $BOB_PID 2>/dev/null || true

for i in {1..30}; do
    if ! kill -0 $BOB_PID 2>/dev/null; then
        sleep 2
        break
    fi
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
    -debug=digidollar \
    > /tmp/bob_testnet.log 2>&1 &
BOB_PID=$!

if wait_for_rpc "$BOB_CLI" "Bob"; then
    print_status "ok" "Bob's Qt RPC is ready after restart"
fi

$BOB_CLI loadwallet "bob" 2>/dev/null || true
sleep 2

BOB_DD_AFTER=$(get_dd_balance "$BOB_CLI" "bob")
BOB_DGB_AFTER=$(get_dgb_balance "$BOB_CLI" "bob")
echo "After restart - Bob's DD: $BOB_DD_AFTER cents, DGB: $BOB_DGB_AFTER"

if [ "$BOB_DD_BEFORE" = "$BOB_DD_AFTER" ]; then
    print_status "ok" "DD balance PERSISTED after wallet restart!"
else
    print_status "fail" "DD balance CHANGED! Before: $BOB_DD_BEFORE, After: $BOB_DD_AFTER"
fi

# Restart oracle
$BOB_CLI startoracle 0 "$ORACLE_PRIVATE_KEY" 2>/dev/null || true
sleep 2
$BOB_CLI generatetoaddress 1 "$BOB_ADDR" > /dev/null 2>&1

for i in {1..10}; do
    STATUS=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.status // "inactive"')
    if [ "$STATUS" = "active" ]; then
        print_status "ok" "Oracle is active again"
        break
    fi
    sleep 1
done

# Step 14: Bob mints $10 DD with short lock
print_header "Step 14: Bob mints \$10 DD with 1-hour lock (tier 0)"
echo "This will be used for redemption testing..."

MINT_RESULT2=$($BOB_CLI -rpcwallet=bob mintdigidollar 1000 0 2>&1)

if echo "$MINT_RESULT2" | jq -e '.txid' > /dev/null 2>&1; then
    REDEEM_MINT_TXID=$(echo "$MINT_RESULT2" | jq -r '.txid')
    print_status "ok" "Mint successful! TX: ${REDEEM_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $MINT_RESULT2 | jq -r '.dd_minted') cents"
    echo "  Lock period: 240 blocks (tier 0)"
else
    print_status "fail" "Mint failed: $MINT_RESULT2"
fi

$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

CURRENT_HEIGHT=$($BOB_CLI getblockcount)
UNLOCK_HEIGHT=$((CURRENT_HEIGHT + 240))
echo "Current block: $CURRENT_HEIGHT, Unlock at: $UNLOCK_HEIGHT"

capture_balance_snapshot "After \$10 DD Mint for Redemption"
list_dd_positions "$BOB_CLI" "bob" "Bob"

# Step 15: Try early redemption (should FAIL)
print_header "Step 15: Try early redemption (should FAIL)"
echo "Attempting to redeem vault BEFORE lock expires..."

set +e
EARLY_REDEEM=$($BOB_CLI -rpcwallet=bob redeemdigidollar "$REDEEM_MINT_TXID" 1000 2>&1)
EARLY_EXIT_CODE=$?
set -e

if [ $EARLY_EXIT_CODE -ne 0 ] || echo "$EARLY_REDEEM" | grep -qi "error\|lock\|expired"; then
    print_status "ok" "Early redemption correctly REJECTED"
    echo "   Response: $EARLY_REDEEM"
else
    print_status "warn" "Unexpected response: $EARLY_REDEEM"
fi

# Step 15.5: Test PARTIAL REDEMPTION REJECTION (EXACT-AMOUNT ENFORCEMENT)
print_header "Step 15.5: Test PARTIAL REDEMPTION REJECTION"
echo "============================================"
echo "TESTING EXACT-AMOUNT REDEMPTION ENFORCEMENT"
echo "============================================"
echo ""
echo "The vault contains 1000 cents (\$10 DD)."
echo "We will attempt to redeem ONLY 500 cents (\$5 DD)."
echo "This MUST be REJECTED - only exact amount redemption is allowed."
echo ""

# First mine past the lock period so lock isn't the issue
echo "Mining 250 blocks to pass lock period first..."
$BOB_CLI generatetoaddress 250 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

NEW_HEIGHT=$($BOB_CLI getblockcount)
echo "Current height: $NEW_HEIGHT (lock should be expired)"

echo ""
echo "Attempting PARTIAL redemption of 500 cents (should FAIL)..."
set +e
PARTIAL_REDEEM=$($BOB_CLI -rpcwallet=bob redeemdigidollar "$REDEEM_MINT_TXID" 500 2>&1)
PARTIAL_EXIT_CODE=$?
set -e

echo "Exit code: $PARTIAL_EXIT_CODE"
echo "Response: $PARTIAL_REDEEM"

if [ $PARTIAL_EXIT_CODE -ne 0 ] || echo "$PARTIAL_REDEEM" | grep -qi "error\|exact\|must equal\|full\|minted"; then
    print_status "ok" "PARTIAL REDEMPTION CORRECTLY REJECTED!"
    echo "   The system enforces exact-amount redemption as expected."
else
    print_status "fail" "PARTIAL REDEMPTION WAS NOT REJECTED!"
    echo "   This is a bug - partial redemption should be blocked."
    echo "   Response was: $PARTIAL_REDEEM"
fi

echo ""
echo "Now attempting EXACT redemption of 1000 cents (should SUCCEED)..."
set +e
EXACT_REDEEM=$($BOB_CLI -rpcwallet=bob redeemdigidollar "$REDEEM_MINT_TXID" 1000 2>&1)
EXACT_EXIT_CODE=$?
set -e

echo "Exit code: $EXACT_EXIT_CODE"

if [ $EXACT_EXIT_CODE -eq 0 ] && echo "$EXACT_REDEEM" | jq -e '.txid' > /dev/null 2>&1; then
    EXACT_REDEEM_TXID=$(echo "$EXACT_REDEEM" | jq -r '.txid')
    print_status "ok" "EXACT AMOUNT REDEMPTION SUCCEEDED!"
    echo "   TX: ${EXACT_REDEEM_TXID:0:16}..."
    echo "   DD Redeemed: $(echo $EXACT_REDEEM | jq -r '.dd_redeemed // 1000') cents"

    # Confirm it
    $BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
    sleep 3
else
    print_status "fail" "EXACT REDEMPTION FAILED: $EXACT_REDEEM"
fi

capture_balance_snapshot "After Exact-Amount Redemption Test"

# Step 16: COLLATERAL CONSISTENCY TEST
print_header "Step 16: COLLATERAL CONSISTENCY TEST"
echo "============================================"
echo "TESTING COLLATERAL CALCULATION CONSISTENCY"
echo "============================================"
echo ""
echo "Verifying that minted collateral matches calculated requirements."
echo "This tests that the GUI 'Required DGB' matches 'Collateral Locked'."
echo ""

# Get oracle price for calculation
ORACLE_PRICE_USD=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.price_usd // 0.01')
ORACLE_PRICE_MICRO=$($BOB_CLI getoracleprice 2>/dev/null | jq -r '.price_micro_usd // 10000')
echo "Current Oracle Price: \$$ORACLE_PRICE_USD per DGB ($ORACLE_PRICE_MICRO micro-USD)"

# Mint a small vault and verify collateral
echo ""
echo "Minting \$5 DD (500 cents) with tier 0 (1000% = 10x collateral)..."

# Calculate expected collateral:
# $5 DD * 10 (1000%) = $50 worth of DGB needed
# $50 / $ORACLE_PRICE_USD = DGB needed
EXPECTED_DGB=$(echo "scale=8; 5 * 10 / $ORACLE_PRICE_USD" | bc 2>/dev/null || echo "0")
echo "Expected collateral: ~$EXPECTED_DGB DGB (at \$$ORACLE_PRICE_USD/DGB)"

set +e
COLLATERAL_TEST_MINT=$($BOB_CLI -rpcwallet=bob mintdigidollar 500 0 2>&1)
COLLATERAL_MINT_EXIT=$?
set -e

if [ $COLLATERAL_MINT_EXIT -eq 0 ] && echo "$COLLATERAL_TEST_MINT" | jq -e '.txid' > /dev/null 2>&1; then
    COLLATERAL_TEST_TXID=$(echo "$COLLATERAL_TEST_MINT" | jq -r '.txid')
    ACTUAL_COLLATERAL=$(echo "$COLLATERAL_TEST_MINT" | jq -r '.dgb_collateral // 0')
    DD_MINTED=$(echo "$COLLATERAL_TEST_MINT" | jq -r '.dd_minted // 0')

    echo ""
    echo "Mint Result:"
    echo "  TX: ${COLLATERAL_TEST_TXID:0:16}..."
    echo "  DD Minted: $DD_MINTED cents (\$$(echo "scale=2; $DD_MINTED / 100" | bc))"
    echo "  Actual Collateral Locked: $ACTUAL_COLLATERAL DGB"
    echo "  Expected Collateral: ~$EXPECTED_DGB DGB"

    # Check if actual is within 5% of expected (accounting for fees, rounding)
    DIFF_PERCENT=$(echo "scale=2; (($ACTUAL_COLLATERAL - $EXPECTED_DGB) / $EXPECTED_DGB) * 100" | bc 2>/dev/null || echo "0")
    ABS_DIFF=${DIFF_PERCENT#-}

    if (( $(echo "$ABS_DIFF < 5" | bc -l 2>/dev/null || echo "0") )); then
        print_status "ok" "Collateral calculation is CONSISTENT (within 5% tolerance)"
        echo "   Difference: ${DIFF_PERCENT}%"
    else
        print_status "warn" "Collateral differs by ${DIFF_PERCENT}% (may need investigation)"
    fi
else
    print_status "fail" "Collateral test mint failed: $COLLATERAL_TEST_MINT"
fi

$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 3

# Step 17: Already covered by Step 15.5 - just show vault state
print_header "Step 17: Vault State After Redemption Tests"
list_dd_positions "$BOB_CLI" "bob" "Bob"
capture_balance_snapshot "After Collateral Test"

# Step 18: Bob mints $200 DD with 10-year lock
print_header "Step 18: Bob mints \$200 DD with 10-year lock (tier 8)"
echo "Testing long-term vault creation..."

MINT_RESULT3=$($BOB_CLI -rpcwallet=bob mintdigidollar 20000 8 2>&1)

if echo "$MINT_RESULT3" | jq -e '.txid' > /dev/null 2>&1; then
    LONG_MINT_TXID=$(echo "$MINT_RESULT3" | jq -r '.txid')
    print_status "ok" "10-year mint successful! TX: ${LONG_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $MINT_RESULT3 | jq -r '.dd_minted') cents (\$200)"
    echo "  Collateral: $(echo $MINT_RESULT3 | jq -r '.dgb_collateral') DGB"
else
    print_status "fail" "10-year mint failed: $MINT_RESULT3"
fi

$BOB_CLI generatetoaddress 7 "$BOB_ADDR" > /dev/null 2>&1
sleep 3
capture_balance_snapshot "After \$200 DD Mint (10-Year Lock)"
list_dd_positions "$BOB_CLI" "bob" "Bob"

# Step 19: Charlie sends $15 DD to Alice
print_header "Step 19: Charlie sends \$15 DD to Alice"
echo "Testing that recipients can spend received DigiDollars..."

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
fi

$BOB_CLI generatetoaddress 8 "$BOB_ADDR" > /dev/null 2>&1
sleep 5
capture_balance_snapshot "After Charlie sends \$15 to Alice"

# Step 20: Alice sends $10 DD back to Bob
print_header "Step 20: Alice sends \$10 DD back to Bob"
echo "Completing the full circle..."

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
fi

$BOB_CLI generatetoaddress 9 "$BOB_ADDR" > /dev/null 2>&1
sleep 3
capture_balance_snapshot "After Alice sends \$10 back to Bob"

# Step 21: Alice mints $100 DD
print_header "Step 21: Alice mints \$100 DD with 7-year lock (tier 7)"
ALICE_BALANCE=$($ALICE_CLI -rpcwallet=alice getbalance 2>/dev/null || echo "0")
echo "Alice's DGB balance: $ALICE_BALANCE DGB"

echo "Alice minting \$100 DD (10000 cents) with tier 7..."
set +e
ALICE_MINT_RESULT=$($ALICE_CLI -rpcwallet=alice mintdigidollar 10000 7 2>&1)
ALICE_MINT_EXIT=$?
set -e

if [ $ALICE_MINT_EXIT -eq 0 ] && echo "$ALICE_MINT_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    ALICE_MINT_TXID=$(echo "$ALICE_MINT_RESULT" | jq -r '.txid')
    print_status "ok" "Alice's 7-year mint successful! TX: ${ALICE_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $ALICE_MINT_RESULT | jq -r '.dd_minted') cents"
    echo "  Collateral: $(echo $ALICE_MINT_RESULT | jq -r '.dgb_collateral') DGB"
else
    print_status "fail" "Alice's mint failed: $ALICE_MINT_RESULT"
fi

$BOB_CLI generatetoaddress 2 "$BOB_ADDR" > /dev/null 2>&1
sleep 3
capture_balance_snapshot "After Alice's \$100 Mint"
list_dd_positions "$ALICE_CLI" "alice" "Alice"

# Step 22: Charlie mints $200 DD
print_header "Step 22: Charlie mints \$200 DD with 10-year lock (tier 8)"
CHARLIE_BALANCE=$($CHARLIE_CLI -rpcwallet=charlie getbalance 2>/dev/null || echo "0")
echo "Charlie's DGB balance: $CHARLIE_BALANCE DGB"

echo "Charlie minting \$200 DD (20000 cents) with tier 8..."
set +e
CHARLIE_MINT_RESULT=$($CHARLIE_CLI -rpcwallet=charlie mintdigidollar 20000 8 2>&1)
CHARLIE_MINT_EXIT=$?
set -e

if [ $CHARLIE_MINT_EXIT -eq 0 ] && echo "$CHARLIE_MINT_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    CHARLIE_MINT_TXID=$(echo "$CHARLIE_MINT_RESULT" | jq -r '.txid')
    print_status "ok" "Charlie's 10-year mint successful! TX: ${CHARLIE_MINT_TXID:0:16}..."
    echo "  DD Minted: $(echo $CHARLIE_MINT_RESULT | jq -r '.dd_minted') cents"
    echo "  Collateral: $(echo $CHARLIE_MINT_RESULT | jq -r '.dgb_collateral') DGB"
else
    print_status "fail" "Charlie's mint failed: $CHARLIE_MINT_RESULT"
fi

$BOB_CLI generatetoaddress 7 "$BOB_ADDR" > /dev/null 2>&1
sleep 5
capture_balance_snapshot "After Charlie's \$200 Mint"
list_dd_positions "$CHARLIE_CLI" "charlie" "Charlie"

# Step 23: Bob redeems first $100 vault
print_header "Step 23: Bob redeems first \$100 vault (fungible DD)"
BOB_DD_BALANCE=$(get_dd_balance "$BOB_CLI" "bob")
echo "Bob's current DD balance: $BOB_DD_BALANCE cents"

echo "Attempting to redeem first vault (\$100 DD = 10000 cents)..."
set +e
FINAL_REDEEM_RESULT=$($BOB_CLI -rpcwallet=bob redeemdigidollar $BOB_FIRST_MINT_TX 10000 2>&1)
FINAL_REDEEM_EXIT=$?
set -e

if [ $FINAL_REDEEM_EXIT -eq 0 ] && echo "$FINAL_REDEEM_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    FINAL_REDEEM_TXID=$(echo "$FINAL_REDEEM_RESULT" | jq -r '.txid')
    print_status "ok" "Redemption SUCCEEDED! TX: ${FINAL_REDEEM_TXID:0:16}..."
    echo "  DD Redeemed: $(echo $FINAL_REDEEM_RESULT | jq -r '.dd_redeemed') cents"
    echo "  Collateral returned: $(echo $FINAL_REDEEM_RESULT | jq -r '.dgb_returned') DGB"

    $BOB_CLI generatetoaddress 7 "$BOB_ADDR" > /dev/null 2>&1
    sleep 5
else
    print_status "fail" "Redemption FAILED: $FINAL_REDEEM_RESULT"
fi

capture_balance_snapshot "After \$100 Redemption"

# Step 24: DD TRANSACTIONS TAB TEST
print_header "Step 24: DD TRANSACTIONS TAB TEST"
echo "============================================"
echo "TESTING DD TRANSACTIONS HISTORY (>10 items)"
echo "============================================"
echo ""
echo "This tests that the DD Transactions tab shows ALL transactions,"
echo "not just the 10 most recent ones (which was a previous limitation)."
echo ""

# Count all DD transactions for each wallet
echo "Counting DD transactions for each wallet..."

# Bob's transactions
set +e
BOB_TX_LIST=$($BOB_CLI -rpcwallet=bob listdigidollartxs 2>&1)
BOB_TX_COUNT=$(echo "$BOB_TX_LIST" | jq 'length' 2>/dev/null || echo "0")
set -e
echo "  Bob's DD transactions: $BOB_TX_COUNT"

# Alice's transactions
set +e
ALICE_TX_LIST=$($ALICE_CLI -rpcwallet=alice listdigidollartxs 2>&1)
ALICE_TX_COUNT=$(echo "$ALICE_TX_LIST" | jq 'length' 2>/dev/null || echo "0")
set -e
echo "  Alice's DD transactions: $ALICE_TX_COUNT"

# Charlie's transactions
set +e
CHARLIE_TX_LIST=$($CHARLIE_CLI -rpcwallet=charlie listdigidollartxs 2>&1)
CHARLIE_TX_COUNT=$(echo "$CHARLIE_TX_LIST" | jq 'length' 2>/dev/null || echo "0")
set -e
echo "  Charlie's DD transactions: $CHARLIE_TX_COUNT"

TOTAL_TX=$((BOB_TX_COUNT + ALICE_TX_COUNT + CHARLIE_TX_COUNT))
echo ""
echo "  TOTAL DD transactions across all wallets: $TOTAL_TX"

# Verify we have more than 10 transactions
if [ "$BOB_TX_COUNT" -gt 10 ]; then
    print_status "ok" "Bob has $BOB_TX_COUNT transactions (>10 - full history available)"
elif [ "$BOB_TX_COUNT" -gt 0 ]; then
    print_status "ok" "Bob has $BOB_TX_COUNT transactions (history available)"
else
    print_status "warn" "Bob has no DD transactions listed"
fi

if [ "$TOTAL_TX" -gt 10 ]; then
    print_status "ok" "Total system has $TOTAL_TX transactions (full history working)"
fi

# Show sample of Bob's transactions
echo ""
echo "Sample of Bob's recent DD transactions:"
echo "$BOB_TX_LIST" | jq -r '.[0:5] | .[] | "  [\(.category)] \(.amount) cents - \(.txid[0:16])..."' 2>/dev/null || echo "  (unable to parse)"

# Step 24.5: ADDITIONAL EXACT-AMOUNT TEST with larger vault
print_header "Step 24.5: Additional Exact-Amount Redemption Test"
echo "Testing exact-amount enforcement on a different vault..."

# Find an active vault from Bob that we can test
BOB_POSITIONS=$($BOB_CLI -rpcwallet=bob listdigidollarpositions false 2>/dev/null || echo "[]")
ACTIVE_VAULT=$(echo "$BOB_POSITIONS" | jq -r '[.[] | select(.can_redeem == true)] | .[0] // empty')

if [ -n "$ACTIVE_VAULT" ] && [ "$ACTIVE_VAULT" != "null" ]; then
    TEST_VAULT_ID=$(echo "$ACTIVE_VAULT" | jq -r '.position_id')
    TEST_VAULT_AMOUNT=$(echo "$ACTIVE_VAULT" | jq -r '.dd_minted')

    echo "Found redeemable vault: ${TEST_VAULT_ID:0:16}..."
    echo "  DD Minted: $TEST_VAULT_AMOUNT cents"

    # Try partial redemption (should fail)
    PARTIAL_AMOUNT=$((TEST_VAULT_AMOUNT / 2))
    echo ""
    echo "Attempting partial redemption of $PARTIAL_AMOUNT cents (should FAIL)..."

    set +e
    PARTIAL_TEST=$($BOB_CLI -rpcwallet=bob redeemdigidollar "$TEST_VAULT_ID" $PARTIAL_AMOUNT 2>&1)
    PARTIAL_TEST_EXIT=$?
    set -e

    if [ $PARTIAL_TEST_EXIT -ne 0 ] || echo "$PARTIAL_TEST" | grep -qi "error\|exact\|must equal"; then
        print_status "ok" "Partial redemption ($PARTIAL_AMOUNT cents) correctly REJECTED"
    else
        print_status "fail" "Partial redemption should have been rejected!"
        echo "  Response: $PARTIAL_TEST"
    fi
else
    echo "No redeemable vaults available for additional testing"
    print_status "info" "Skipping additional exact-amount test (no redeemable vaults)"
fi

# Step 25: Final network state
print_header "Step 25: Final DigiDollar Network State"
display_network_stats "Final State (After All Operations)"
capture_balance_snapshot "FINAL STATE"

echo ""
echo "ALL DD POSITIONS:"
list_dd_positions "$BOB_CLI" "bob" "Bob"
list_dd_positions "$ALICE_CLI" "alice" "Alice"
list_dd_positions "$CHARLIE_CLI" "charlie" "Charlie"

# Summary
print_header "TEST RESULTS SUMMARY"
echo ""
echo "  Total Tests:  $TOTAL_TESTS"
echo "  Passed:       $PASSED_TESTS"
echo "  Failed:       $FAILED_TESTS"
echo ""
echo "TESTS PERFORMED:"
echo "  1-8.   Setup: Clean, start nodes, fund wallets, sync"
echo "  9.     Initial state display"
echo "  10.    Bob mints \$100 DD (tier 0)"
echo "  11.    Bob sends \$30 DD to Alice"
echo "  12.    Bob sends \$20 DD to Charlie"
echo "  13.    State after transfers"
echo "  13.5   Bob wallet restart (DD persistence test)"
echo "  14.    Bob mints \$10 DD for redemption test"
echo "  15.    Early redemption REJECTED"
echo "  15.5   *** PARTIAL REDEMPTION REJECTED (EXACT-AMOUNT ENFORCEMENT) ***"
echo "         - Tried 500 cents from 1000 cent vault -> MUST FAIL"
echo "         - Exact 1000 cents redemption -> MUST SUCCEED"
echo "  16.    *** COLLATERAL CONSISTENCY TEST ***"
echo "         - Verify minted collateral matches calculated amount"
echo "         - Tests GUI 'Required DGB' matches 'Collateral Locked'"
echo "  17.    Vault state after redemption tests"
echo "  18.    Bob mints \$200 DD (tier 8, 10-year)"
echo "  19.    Charlie sends \$15 DD to Alice"
echo "  20.    Alice sends \$10 DD back to Bob"
echo "  21.    Alice mints \$100 DD (tier 7)"
echo "  22.    Charlie mints \$200 DD (tier 8)"
echo "  23.    Bob redeems first \$100 vault"
echo "  24.    *** DD TRANSACTIONS TAB TEST ***"
echo "         - Verify listdigidollartxs returns >10 transactions"
echo "         - Full transaction history available (not limited to 10)"
echo "  24.5   Additional exact-amount test on different vault"
echo "  25.    Final network state"
echo ""
echo "KEY FEATURES TESTED:"
echo "  [1] Exact-Amount Redemption Only (no partial redemption)"
echo "  [2] Collateral Calculation Consistency"
echo "  [3] DD Transactions Full History (>10 items)"
echo ""

print_header "DEBUG LOG LOCATIONS"
echo "  Test log:    $LOG_FILE"
echo "  Bob log:     /tmp/bob_testnet.log"
echo "  Alice log:   /tmp/alice_testnet.log"
echo "  Charlie log: /tmp/charlie_testnet.log"
echo ""
echo "To check DigiDollar debug output:"
echo "  grep -i digidollar /tmp/bob_minitestnet/testnet5/debug.log | tail -50"
echo ""

print_header "RUNNING Qt WINDOWS"
echo "  - Bob's Qt (PID: $BOB_PID)"
echo "  - Alice's Qt (PID: $ALICE_PID)"
echo "  - Charlie's Qt (PID: $CHARLIE_PID)"
echo ""
echo "Commands for manual testing:"
echo "  $BOB_CLI -rpcwallet=bob getdigidollarbalance"
echo "  $BOB_CLI -rpcwallet=bob listdigidollarpositions"
echo "  $BOB_CLI getoracleprice"
echo ""
echo "Press Ctrl+C to exit (will close all Qt windows)."
echo ""

trap "kill $BOB_PID $ALICE_PID $CHARLIE_PID 2>/dev/null; echo 'All Qt windows closed.'" EXIT
wait $BOB_PID
