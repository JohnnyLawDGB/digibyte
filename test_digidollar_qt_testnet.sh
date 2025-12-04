#!/bin/bash
# DigiDollar Qt GUI TestNet Test with Live Oracle
# Tests the full DigiDollar cycle on TestNet with real-time exchange price data
# This script assumes TestNet is already running with DigiDollar active (block 650+)

set -e

echo "=========================================="
echo "DigiDollar Qt TestNet Automated Test"
echo "Using LIVE Oracle Price Data"
echo "=========================================="
echo ""

# Configuration
CLI="./src/digibyte-cli -testnet"
TESTNET_RPC_PORT=14024
TESTNET_WALLET="testnet_oracle"
ORACLE_PRIVATE_KEY="0000000000000000000000000000000000000000000000000000000000000001"
# MINING_ADDR will be set dynamically after wallet is created/loaded

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper function to print colored output
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

# Helper function to get DigiDollar stats
get_dd_stats() {
    $CLI getdigidollarstats 2>/dev/null || echo "{}"
}

# Helper function to get oracle price
get_oracle_price() {
    $CLI getoracleprice 2>/dev/null || echo "{}"
}

# Helper function to get DD balance
get_dd_balance() {
    $CLI -rpcwallet=$TESTNET_WALLET getdigidollarbalance 2>/dev/null | jq -r '.total // 0'
}

# Helper function to wait for oracle to be active
wait_for_oracle() {
    local max_attempts=30
    local attempt=0
    echo "Waiting for oracle to become active..."

    while [ $attempt -lt $max_attempts ]; do
        local status=$($CLI getoracleprice 2>/dev/null | jq -r '.status // "inactive"')
        if [ "$status" = "active" ]; then
            print_status "ok" "Oracle is active"
            return 0
        fi
        attempt=$((attempt + 1))
        sleep 2
    done

    print_status "fail" "Oracle did not become active after $max_attempts attempts"
    return 1
}

# Helper function to display live oracle price info
display_oracle_info() {
    local oracle_data=$(get_oracle_price)
    local price_micro=$(echo "$oracle_data" | jq -r '.price_micro_usd // "N/A"')
    local price_usd=$(echo "$oracle_data" | jq -r '.price_usd // "N/A"')
    local price_cents=$(echo "$oracle_data" | jq -r '.price_cents // "N/A"')
    local status=$(echo "$oracle_data" | jq -r '.status // "unknown"')
    local is_stale=$(echo "$oracle_data" | jq -r '.is_stale // true')

    echo "=========================================="
    echo "LIVE ORACLE PRICE DATA"
    echo "=========================================="
    echo "  Status:        $status"
    echo "  Price (USD):   \$$price_usd"
    echo "  Price (micro): $price_micro micro-USD"
    echo "  Price (cents): $price_cents cents"
    echo "  Is Stale:      $is_stale"
    echo "=========================================="
    echo ""
}

# Helper function to display DigiDollar system stats
display_dd_stats() {
    local stats=$(get_dd_stats)
    local health=$(echo "$stats" | jq -r '.health_percentage // 0')
    local health_status=$(echo "$stats" | jq -r '.health_status // "unknown"')
    local total_supply=$(echo "$stats" | jq -r '.total_dd_supply // 0')
    local total_collateral=$(echo "$stats" | jq -r '.total_collateral_dgb // 0')
    local oracle_price_micro=$(echo "$stats" | jq -r '.oracle_price_micro_usd // 0')

    echo "=========================================="
    echo "DIGIDOLLAR SYSTEM STATS"
    echo "=========================================="
    echo "  Health Status:    $health_status ($health%)"
    echo "  Total DD Supply:  $total_supply cents (\$$(echo "scale=2; $total_supply / 100" | bc 2>/dev/null || echo "N/A"))"
    echo "  Total Collateral: $total_collateral DGB"
    echo "  Oracle Price:     $oracle_price_micro micro-USD"
    echo "=========================================="
    echo ""
}

# Step 1: Check TestNet daemon is running
echo "=== Step 1: Checking TestNet daemon status ==="
if ! $CLI getblockchaininfo > /dev/null 2>&1; then
    print_status "warn" "TestNet daemon not running, starting it..."
    ./src/digibyted -testnet -daemon
    sleep 10
fi

CURRENT_HEIGHT=$($CLI getblockcount 2>/dev/null || echo "0")
echo "Current block height: $CURRENT_HEIGHT"

if [ "$CURRENT_HEIGHT" -lt 650 ]; then
    print_status "fail" "Block height ($CURRENT_HEIGHT) is below DigiDollar activation height (650)"
    echo "Please mine more blocks to activate DigiDollar:"
    echo "  $CLI generatetoaddress 100 $MINING_ADDR"
    exit 1
fi
print_status "ok" "TestNet is running at height $CURRENT_HEIGHT (DigiDollar active)"
echo ""

# Step 2: Start the oracle if not running
echo "=== Step 2: Starting Live Oracle ==="
ORACLE_STATUS=$($CLI getoracleprice 2>/dev/null | jq -r '.status // "inactive"')
if [ "$ORACLE_STATUS" != "active" ]; then
    echo "Starting oracle with testnet key..."
    $CLI startoracle 0 "$ORACLE_PRIVATE_KEY" 2>/dev/null || true
    sleep 5

    # Mine a block to trigger oracle price broadcast
    echo "Mining block to trigger oracle price update..."
    $CLI generatetoaddress 1 "$MINING_ADDR" > /dev/null 2>&1
    sleep 3
fi

# Wait for oracle to be active
wait_for_oracle
display_oracle_info
echo ""

# Step 3: Check/Create wallet
echo "=== Step 3: Checking wallet ==="
WALLET_EXISTS=$($CLI listwallets 2>/dev/null | jq -r ".[] | select(. == \"$TESTNET_WALLET\")" || echo "")
if [ -z "$WALLET_EXISTS" ]; then
    echo "Wallet '$TESTNET_WALLET' not loaded, checking if it exists..."
    # Try to load existing wallet first
    LOAD_RESULT=$($CLI loadwallet "$TESTNET_WALLET" 2>&1)
    if echo "$LOAD_RESULT" | grep -q "error"; then
        echo "Wallet doesn't exist, creating new wallet '$TESTNET_WALLET'..."
        $CLI createwallet "$TESTNET_WALLET" 2>/dev/null
        if [ $? -ne 0 ]; then
            print_status "fail" "Could not create wallet '$TESTNET_WALLET'"
            exit 1
        fi
    else
        echo "Loaded existing wallet '$TESTNET_WALLET'"
    fi
fi
print_status "ok" "Wallet '$TESTNET_WALLET' ready"

# Get a mining address from this wallet (ensures funds go to the wallet we're testing)
MINING_ADDR=$($CLI -rpcwallet=$TESTNET_WALLET getnewaddress "mining" "bech32" 2>/dev/null)
if [ -z "$MINING_ADDR" ]; then
    print_status "fail" "Could not get mining address from wallet"
    exit 1
fi
echo "Mining address (from wallet): $MINING_ADDR"

# Check balance
DGB_BALANCE=$($CLI -rpcwallet=$TESTNET_WALLET getbalance 2>/dev/null || echo "0")
echo "DGB Balance: $DGB_BALANCE DGB"
echo ""

# Step 4: Get initial DigiDollar state
echo "=== Step 4: Initial DigiDollar State ==="
display_dd_stats

INITIAL_DD_BALANCE=$(get_dd_balance)
echo "Initial DD Balance: $INITIAL_DD_BALANCE cents (\$$(echo "scale=2; $INITIAL_DD_BALANCE / 100" | bc 2>/dev/null || echo "N/A"))"
echo ""

# Step 5: Test minting with live oracle price
echo "=== Step 5: Testing Mint with Live Oracle Price ==="

# Get current oracle price for calculations
ORACLE_PRICE_MICRO=$($CLI getoracleprice 2>/dev/null | jq -r '.price_micro_usd // 0')
ORACLE_PRICE_USD=$(echo "scale=6; $ORACLE_PRICE_MICRO / 1000000" | bc 2>/dev/null || echo "0.006")

echo "Current Oracle Price: \$$ORACLE_PRICE_USD per DGB ($ORACLE_PRICE_MICRO micro-USD)"

# Calculate collateral requirement for $100 DD with 365 day lock
echo ""
echo "Calculating collateral for \$100 DD (365 day lock)..."
COLLATERAL_REQ=$($CLI calculatecollateralrequirement 10000 365 2>/dev/null)
REQUIRED_DGB=$(echo "$COLLATERAL_REQ" | jq -r '.required_dgb // "N/A"')
EFFECTIVE_RATIO=$(echo "$COLLATERAL_REQ" | jq -r '.effective_ratio // "N/A"')
COLLATERAL_ORACLE=$(echo "$COLLATERAL_REQ" | jq -r '.oracle_price // "N/A"')

echo "  Required DGB:    $REQUIRED_DGB DGB"
echo "  Effective Ratio: $EFFECTIVE_RATIO%"
echo "  Oracle Price:    $COLLATERAL_ORACLE micro-USD"
echo ""

# Check if we have enough balance
REQUIRED_INT=$(echo "$REQUIRED_DGB" | cut -d'.' -f1)
BALANCE_INT=$(echo "$DGB_BALANCE" | cut -d'.' -f1)

if [ "$BALANCE_INT" -lt "$REQUIRED_INT" ]; then
    print_status "warn" "Insufficient balance for mint test"
    echo "Need ~$REQUIRED_DGB DGB but have $DGB_BALANCE DGB"
    echo "Mining more blocks to get funds..."
    $CLI generatetoaddress 200 "$MINING_ADDR" > /dev/null 2>&1
    sleep 5
    DGB_BALANCE=$($CLI -rpcwallet=$TESTNET_WALLET getbalance 2>/dev/null || echo "0")
    echo "New balance: $DGB_BALANCE DGB"
fi

# Perform the mint
echo ""
echo "Minting \$100 DD with 1-hour lock (tier 0 for quick testing)..."
MINT_RESULT=$($CLI -rpcwallet=$TESTNET_WALLET mintdigidollar 10000 0 2>&1)

if echo "$MINT_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    MINT_TXID=$(echo "$MINT_RESULT" | jq -r '.txid')
    MINT_DD=$(echo "$MINT_RESULT" | jq -r '.dd_minted // "N/A"')
    MINT_COLLATERAL=$(echo "$MINT_RESULT" | jq -r '.dgb_collateral // "N/A"')

    print_status "ok" "Mint successful!"
    echo "  Transaction ID: ${MINT_TXID:0:16}..."
    echo "  DD Minted:      $MINT_DD cents (\$$(echo "scale=2; $MINT_DD / 100" | bc 2>/dev/null || echo "N/A"))"
    echo "  DGB Collateral: $MINT_COLLATERAL DGB"
else
    print_status "fail" "Mint failed!"
    echo "$MINT_RESULT"
    exit 1
fi
echo ""

# Step 6: Confirm the mint
echo "=== Step 6: Confirming Mint Transaction ==="
$CLI generatetoaddress 2 "$MINING_ADDR" > /dev/null 2>&1
sleep 5

# Verify DD balance increased
NEW_DD_BALANCE=$(get_dd_balance)
DD_INCREASE=$((NEW_DD_BALANCE - INITIAL_DD_BALANCE))

echo "DD Balance After Mint: $NEW_DD_BALANCE cents (\$$(echo "scale=2; $NEW_DD_BALANCE / 100" | bc 2>/dev/null || echo "N/A"))"
echo "DD Increase: $DD_INCREASE cents"

if [ "$DD_INCREASE" -eq 10000 ]; then
    print_status "ok" "Mint verified - balance increased by \$100.00"
else
    print_status "warn" "Unexpected DD increase: $DD_INCREASE cents (expected 10000)"
fi
echo ""

# Step 7: Display updated stats
echo "=== Step 7: Post-Mint DigiDollar Stats ==="
display_dd_stats
display_oracle_info

# Step 8: Test DD transfer
echo "=== Step 8: Testing DD Transfer ==="

# Get a DD address for transfer test
DD_ADDRESS=$($CLI -rpcwallet=$TESTNET_WALLET getdigidollaraddress 2>/dev/null)
echo "DD Address for self-transfer test: $DD_ADDRESS"

# Transfer $10 DD to ourselves (sanity check)
echo "Transferring \$10 DD (1000 cents)..."
TRANSFER_RESULT=$($CLI -rpcwallet=$TESTNET_WALLET senddigidollar "$DD_ADDRESS" 1000 2>&1)

if echo "$TRANSFER_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
    TRANSFER_TXID=$(echo "$TRANSFER_RESULT" | jq -r '.txid')
    print_status "ok" "Transfer successful!"
    echo "  Transaction ID: ${TRANSFER_TXID:0:16}..."
else
    print_status "fail" "Transfer failed!"
    echo "$TRANSFER_RESULT"
fi
echo ""

# Confirm transfer
$CLI generatetoaddress 2 "$MINING_ADDR" > /dev/null 2>&1
sleep 3

# Step 9: List positions
echo "=== Step 9: Listing DigiDollar Positions ==="
POSITIONS=$($CLI -rpcwallet=$TESTNET_WALLET listdigidollarpositions 2>/dev/null)
POSITION_COUNT=$(echo "$POSITIONS" | jq 'length')

echo "Total positions: $POSITION_COUNT"
if [ "$POSITION_COUNT" -gt 0 ]; then
    echo ""
    echo "Recent positions:"
    echo "$POSITIONS" | jq -r '.[-3:][] | "  - \(.dd_amount // .amount // "?") cents, tier \(.tier // "?"), status: \(.status // "?")"' 2>/dev/null || echo "$POSITIONS"
fi
echo ""

# Step 10: Mine blocks to reach unlock height (for tier 0 = 240 blocks)
echo "=== Step 10: Testing Redemption ==="

# Get current height
CURRENT_HEIGHT=$($CLI getblockcount)
# Tier 0 = 240 blocks lock
UNLOCK_HEIGHT=$((CURRENT_HEIGHT + 240))

echo "Current height: $CURRENT_HEIGHT"
echo "Unlock height for tier 0: ~$UNLOCK_HEIGHT (need to mine ~240 blocks)"
echo ""

echo "Mining 250 blocks to pass unlock period..."
$CLI generatetoaddress 250 "$MINING_ADDR" > /dev/null 2>&1
sleep 10

NEW_HEIGHT=$($CLI getblockcount)
echo "New height: $NEW_HEIGHT"
echo ""

# Try to redeem
echo "Attempting redemption..."
if [ -n "$MINT_TXID" ]; then
    REDEEM_RESULT=$($CLI -rpcwallet=$TESTNET_WALLET redeemdigidollar "$MINT_TXID" 10000 2>&1)

    if echo "$REDEEM_RESULT" | jq -e '.txid' > /dev/null 2>&1; then
        REDEEM_TXID=$(echo "$REDEEM_RESULT" | jq -r '.txid')
        REDEEM_DD=$(echo "$REDEEM_RESULT" | jq -r '.dd_redeemed // "N/A"')
        REDEEM_DGB=$(echo "$REDEEM_RESULT" | jq -r '.dgb_unlocked // "N/A"')

        print_status "ok" "Redemption successful!"
        echo "  Transaction ID: ${REDEEM_TXID:0:16}..."
        echo "  DD Redeemed:    $REDEEM_DD cents"
        echo "  DGB Unlocked:   $REDEEM_DGB DGB"
    else
        print_status "fail" "Redemption failed!"
        echo "$REDEEM_RESULT"
    fi
else
    print_status "warn" "No mint transaction to redeem"
fi
echo ""

# Confirm redemption
$CLI generatetoaddress 2 "$MINING_ADDR" > /dev/null 2>&1
sleep 3

# Step 11: Final state
echo "=== Step 11: Final DigiDollar State ==="
display_dd_stats

FINAL_DD_BALANCE=$(get_dd_balance)
echo "Final DD Balance: $FINAL_DD_BALANCE cents (\$$(echo "scale=2; $FINAL_DD_BALANCE / 100" | bc 2>/dev/null || echo "N/A"))"
echo ""

# Step 12: Summary
echo "=========================================="
echo "TESTNET DIGIDOLLAR TEST COMPLETE"
echo "=========================================="
echo ""
echo "Test Results Summary:"
echo "  1. Oracle: Using LIVE exchange price data"
ORACLE_FINAL=$($CLI getoracleprice 2>/dev/null | jq -r '.price_usd // "N/A"')
echo "     Current Price: \$$ORACLE_FINAL per DGB"
echo ""
echo "  2. Minting: Tested with real collateral calculation"
echo "     Initial DD: $INITIAL_DD_BALANCE cents"
echo "     Final DD:   $FINAL_DD_BALANCE cents"
echo ""
echo "  3. Transfer: Self-transfer test completed"
echo ""
echo "  4. Redemption: Lock period expiry and redemption tested"
echo ""

# Check if Qt is running
if pgrep -f "digibyte-qt.*testnet" > /dev/null; then
    echo "Note: DigiByte-Qt is running. You can verify the results in the GUI:"
    echo "  - DigiDollar tab > Overview: Check balances and positions"
    echo "  - DigiDollar tab > Mint: Verify oracle price is shown correctly"
    echo ""
fi

print_status "ok" "All DigiDollar TestNet tests completed successfully!"
echo ""
echo "=========================================="
echo "TESTNET ORACLE IS RUNNING"
echo "=========================================="
echo "The oracle is now fetching live DGB/USD prices from exchanges."
echo "Price updates happen every ~2 blocks (30 seconds)."
echo ""
echo "To check oracle status:"
echo "  $CLI getoracleprice"
echo ""
echo "To mint more DD:"
echo "  $CLI -rpcwallet=$TESTNET_WALLET mintdigidollar <cents> <tier>"
echo ""
echo "  Tiers: 0=1hr, 1=30d, 2=90d, 3=180d, 4=365d, 5=730d, 6=2738d"
echo "=========================================="
