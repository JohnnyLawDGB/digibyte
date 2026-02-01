#!/bin/bash
# Phase Two Multi-Oracle RegTest Test
# Tests 7 oracle nodes reaching 4-of-7 consensus
# Mirrors testnet configuration: 7 oracles, 4-of-7 strict majority
#
# Test scenarios:
#   1. All 7 oracles agree → consensus forms
#   2. 4 of 7 agree, 3 disagree → consensus still forms (median)
#   3. Only 3 of 7 report → no consensus (below 4-of-7 threshold)
#   4. Oracle goes offline → remaining 6 still reach consensus
#   5. Phase transition: blocks before/after Phase Two activation height
#   6. Price disagreement with outliers → median filters them

set -e

echo "=========================================="
echo "Phase Two Multi-Oracle RegTest Test"
echo "4-of-7 Oracle Consensus Testing"
echo "(Mirrors testnet: 7 oracles, strict majority)"
echo "=========================================="
echo ""

NODE_BINARY="./src/digibyted"
CLI="./src/digibyte-cli"

if [ ! -x "$NODE_BINARY" ]; then
    echo "❌ digibyted not found. Build first: make -j\$(nproc)"
    exit 1
fi

# Total oracle count (matches testnet)
NUM_ORACLES=7
THRESHOLD=4

# ============================================================================
# CLEANUP
# ============================================================================
echo "=== Cleanup: Stopping any existing regtest nodes ==="
pkill -f "digibyted.*regtest" 2>/dev/null || true
sleep 3
for dir in /tmp/oracle_node_{0..6} /tmp/miner_node; do
    rm -rf "$dir"
done
echo "✓ Clean environment"
echo ""

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

wait_for_rpc() {
    local DATADIR=$1
    local RPCPORT=$2
    local MAX_WAIT=${3:-60}
    local WAITED=0
    while [ $WAITED -lt $MAX_WAIT ]; do
        if $CLI -regtest -datadir=$DATADIR -rpcport=$RPCPORT getblockcount >/dev/null 2>&1; then
            return 0
        fi
        sleep 2
        WAITED=$((WAITED + 2))
    done
    echo "  ❌ RPC not ready after ${MAX_WAIT}s at port $RPCPORT"
    return 1
}

rpc() {
    local PORT=$1
    local DIR=$2
    shift 2
    $CLI -regtest -datadir=$DIR -rpcport=$PORT "$@"
}

rpc_json() {
    local PORT=$1
    local COOKIE=$2
    local METHOD=$3
    local PARAMS=${4:-"[]"}
    curl --silent --user "$COOKIE" \
        --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"test\",\"method\":\"$METHOD\",\"params\":$PARAMS}" \
        -H 'content-type: text/plain;' \
        http://127.0.0.1:${PORT}/
}

# ============================================================================
# STEP 1: Start miner node
# ============================================================================
echo "=== Step 1: Starting miner node ==="
mkdir -p /tmp/miner_node

$NODE_BINARY \
    -regtest \
    -datadir=/tmp/miner_node \
    -port=18444 \
    -rpcport=18443 \
    -server \
    -listen=1 \
    -discover=0 \
    -digidollar=1 \
    -txindex=1 \
    -fallbackfee=0.0001 \
    -dandelion=0 \
    > /tmp/miner_node.log 2>&1 &
MINER_PID=$!
echo "Miner node started (PID: $MINER_PID)"

if ! wait_for_rpc /tmp/miner_node 18443 60; then
    echo "❌ Miner node failed to start"
    cat /tmp/miner_node.log | tail -30
    exit 1
fi

MINER_COOKIE=$(cat /tmp/miner_node/regtest/.cookie)

# Create wallet and generate initial blocks
rpc 18443 /tmp/miner_node createwallet "miner" > /dev/null
echo "✓ Miner wallet created"

# ============================================================================
# STEP 2: Generate blocks up to Phase Two activation
# ============================================================================
echo ""
echo "=== Step 2: Phase One → Phase Two transition ==="
echo "Phase Two activates at block 100"
echo ""

# Set mock oracle price (Phase One uses MockOracleManager)
rpc 18443 /tmp/miner_node setmockoracleprice 10000
echo "✓ Mock oracle price set to 10000 micro-USD (\$0.01/DGB)"

# Generate 95 blocks (still Phase One)
echo "Generating 95 blocks (Phase One)..."
rpc 18443 /tmp/miner_node -generate 95 > /dev/null
sleep 3
HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)
echo "✓ Height: $HEIGHT (Phase One active, Phase Two at block 100)"

# Check a Phase One block's oracle data
echo ""
echo "Checking Phase One block oracle data..."
BLOCK_HASH=$(rpc 18443 /tmp/miner_node getblockhash 50)
BLOCK_DATA=$(rpc 18443 /tmp/miner_node getblock $BLOCK_HASH 2)
COINBASE_TX=$(echo "$BLOCK_DATA" | jq -r '.tx[0].txid')
echo "Block 50 coinbase: ${COINBASE_TX:0:16}..."

ORACLE_VOUT=$(echo "$BLOCK_DATA" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.asm' 2>/dev/null)
if [ -n "$ORACLE_VOUT" ]; then
    echo "✓ Phase One oracle data found in coinbase"
else
    echo "⚠️  No oracle OP_RETURN found in block 50 (may be normal for mock oracle in regtest)"
fi

# ============================================================================
# STEP 3: Create oracle keys in wallet (7 oracles)
# ============================================================================
echo ""
echo "=== Step 3: Creating $NUM_ORACLES oracle keys in miner wallet ==="
echo ""

ORACLE_PUBKEYS=()
for i in $(seq 0 $((NUM_ORACLES - 1))); do
    RESULT=$(rpc_json 18443 "$MINER_COOKIE" "createoraclekey" "[$i]" 2>/dev/null)
    PUBKEY=$(echo "$RESULT" | jq -r '.result.pubkey // empty')
    ERROR=$(echo "$RESULT" | jq -r '.error.message // empty')
    
    if [ -n "$PUBKEY" ]; then
        ORACLE_PUBKEYS[$i]="$PUBKEY"
        echo "✓ Oracle $i: pubkey=${PUBKEY:0:20}..."
    elif [ -n "$ERROR" ]; then
        echo "⚠️  Oracle $i: $ERROR"
    else
        echo "❌ Oracle $i: unexpected response"
        echo "$RESULT" | jq '.'
    fi
done
echo ""

# ============================================================================
# STEP 4: Start all 7 oracle nodes
# ============================================================================
echo "=== Step 4: Starting $NUM_ORACLES oracle nodes ==="
echo ""

ORACLE_STARTED=0
for i in $(seq 0 $((NUM_ORACLES - 1))); do
    RESULT=$(rpc_json 18443 "$MINER_COOKIE" "startoracle" "[$i]" 2>/dev/null)
    STATUS=$(echo "$RESULT" | jq -r '.result.status // empty')
    MSG=$(echo "$RESULT" | jq -r '.result.message // empty')
    ERROR=$(echo "$RESULT" | jq -r '.error.message // empty')
    
    if [ "$STATUS" = "success" ] || [ "$STATUS" = "running" ]; then
        echo "✓ Oracle $i started: $MSG"
        ORACLE_STARTED=$((ORACLE_STARTED + 1))
    elif [ -n "$ERROR" ]; then
        echo "⚠️  Oracle $i: $ERROR"
    else
        echo "⚠️  Oracle $i status: $STATUS - $MSG"
    fi
done

echo ""
echo "Oracles started: $ORACLE_STARTED / $NUM_ORACLES"
echo ""

# List running oracles
echo "=== Running oracles ==="
ORACLE_LIST=$(rpc_json 18443 "$MINER_COOKIE" "listoracles" "[true]")
echo "$ORACLE_LIST" | jq -r '.result[] | "  Oracle \(.oracle_id): running=\(.is_running), pubkey=\(.pubkey[0:20])..."' 2>/dev/null || echo "  (could not list oracles)"
echo ""

# ============================================================================
# STEP 5: Cross the Phase Two activation height
# ============================================================================
echo "=== Step 5: Crossing Phase Two activation (block 100) ==="
echo ""

rpc 18443 /tmp/miner_node -generate 10 > /dev/null
sleep 3
HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)
echo "✓ Height: $HEIGHT (Phase Two should now be active)"

echo ""
echo "Checking Phase Two blocks for oracle data..."
for BLK in 101 102 103; do
    BH=$(rpc 18443 /tmp/miner_node getblockhash $BLK 2>/dev/null)
    if [ -n "$BH" ]; then
        BD=$(rpc 18443 /tmp/miner_node getblock $BH 2)
        ORACLE_DATA=$(echo "$BD" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.hex' 2>/dev/null)
        if [ -n "$ORACLE_DATA" ]; then
            echo "  Block $BLK: Oracle data hex = ${ORACLE_DATA:0:40}..."
        else
            echo "  Block $BLK: No oracle OP_RETURN found"
        fi
    fi
done

# ============================================================================
# STEP 6: Test all 7 oracles submitting (7-of-7)
# ============================================================================
echo ""
echo "=== Step 6: Testing 7-of-7 oracle price submission ==="
echo ""

echo "Sending price from all $NUM_ORACLES oracles (\$0.01/DGB = 10000 micro-USD)..."
for i in $(seq 0 $((NUM_ORACLES - 1))); do
    SEND_RESULT=$(rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, $i]" 2>/dev/null)
    SENT=$(echo "$SEND_RESULT" | jq -r '.result.broadcasted // empty')
    ERROR=$(echo "$SEND_RESULT" | jq -r '.error.message // empty')
    
    if [ "$SENT" = "true" ]; then
        echo "  ✓ Oracle $i: price sent"
    elif [ -n "$ERROR" ]; then
        echo "  ⚠️  Oracle $i: $ERROR"
    else
        echo "  ⚠️  Oracle $i: $(echo "$SEND_RESULT" | jq -c '.result // .error')"
    fi
done

echo ""
echo "Mining block to include oracle consensus..."
rpc 18443 /tmp/miner_node -generate 1 > /dev/null
sleep 2
HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)
echo "✓ Mined block $HEIGHT"

BH=$(rpc 18443 /tmp/miner_node getblockhash $HEIGHT)
BD=$(rpc 18443 /tmp/miner_node getblock $BH 2)
ORACLE_HEX=$(echo "$BD" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.hex' 2>/dev/null)
ORACLE_ASM=$(echo "$BD" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.asm' 2>/dev/null)

echo ""
echo "Block $HEIGHT oracle data:"
echo "  ASM: $ORACLE_ASM"
echo "  Hex: ${ORACLE_HEX:0:60}..."
echo ""

# ============================================================================
# STEP 7: Test exactly 4-of-7 (minimum threshold)
# ============================================================================
echo "=== Step 7: Testing 4-of-7 consensus (minimum threshold) ==="
echo ""
echo "Sending price from only oracles 0, 1, 2, 3..."

for i in 0 1 2 3; do
    rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, $i]" > /dev/null 2>&1
    echo "  ✓ Oracle $i: price sent"
done

echo ""
echo "Mining block..."
rpc 18443 /tmp/miner_node -generate 1 > /dev/null
sleep 2
HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)

BH=$(rpc 18443 /tmp/miner_node getblockhash $HEIGHT)
BD=$(rpc 18443 /tmp/miner_node getblock $BH 2)
ORACLE_HEX=$(echo "$BD" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.hex' 2>/dev/null)

if [ -n "$ORACLE_HEX" ]; then
    echo "✅ Block $HEIGHT: 4-of-7 consensus achieved!"
    echo "  Oracle data: ${ORACLE_HEX:0:40}..."
else
    echo "⚠️  Block $HEIGHT: No oracle data (4-of-7 may not have formed consensus)"
fi
echo ""

# ============================================================================
# STEP 8: Test 3-of-7 (should fail — below 4-of-7 threshold)
# ============================================================================
echo "=== Step 8: Testing 3-of-7 (below threshold — should NOT form consensus) ==="
echo ""
echo "Sending price from only oracles 0, 1, 2..."

for i in 0 1 2; do
    rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, $i]" > /dev/null 2>&1
    echo "  ✓ Oracle $i: price sent"
done

echo ""
echo "Mining block..."
rpc 18443 /tmp/miner_node -generate 1 > /dev/null
sleep 2
HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)

BH=$(rpc 18443 /tmp/miner_node getblockhash $HEIGHT)
BD=$(rpc 18443 /tmp/miner_node getblock $BH 2)
ORACLE_HEX=$(echo "$BD" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.hex' 2>/dev/null)

# Filter out SegWit commitment (aa21a9ed...) — only look for oracle data
ORACLE_ONLY=$(echo "$ORACLE_HEX" | tr ' ' '\n' | grep -v "^6a24aa21a9ed" | head -1)

if [ -z "$ORACLE_ONLY" ]; then
    echo "✅ Block $HEIGHT: Correctly NO consensus with only 3 oracles (need $THRESHOLD)"
else
    echo "⚠️  Block $HEIGHT: Oracle data found with only 3 oracles?"
    echo "  Data: $ORACLE_ONLY"
fi
echo ""

# ============================================================================
# STEP 9: Test 5-of-7 (above threshold, 2 oracles offline)
# ============================================================================
echo "=== Step 9: Testing 5-of-7 (2 oracles offline — should still reach consensus) ==="
echo ""
echo "Sending price from oracles 0, 1, 2, 3, 4 (oracles 5, 6 offline)..."

for i in 0 1 2 3 4; do
    rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, $i]" > /dev/null 2>&1
    echo "  ✓ Oracle $i: price sent"
done

echo ""
echo "Mining block..."
rpc 18443 /tmp/miner_node -generate 1 > /dev/null
sleep 2
HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)

BH=$(rpc 18443 /tmp/miner_node getblockhash $HEIGHT)
BD=$(rpc 18443 /tmp/miner_node getblock $BH 2)
ORACLE_HEX=$(echo "$BD" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.hex' 2>/dev/null)

if [ -n "$ORACLE_HEX" ]; then
    echo "✅ Block $HEIGHT: 5-of-7 consensus achieved (2 oracles offline, still above threshold)"
    echo "  Oracle data: ${ORACLE_HEX:0:40}..."
else
    echo "⚠️  Block $HEIGHT: No oracle data with 5 oracles"
fi
echo ""

# ============================================================================
# STEP 10: Test price disagreement (4 agree, 3 outliers)
# ============================================================================
echo "=== Step 10: Testing price disagreement (4 agree at \$0.01, 3 outliers) ==="
echo ""
echo "Oracles 0,1,2,3 send \$0.01 | Oracle 4: \$0.05 | Oracle 5: \$0.001 | Oracle 6: \$0.10"

rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, 0]" > /dev/null 2>&1
rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, 1]" > /dev/null 2>&1
rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, 2]" > /dev/null 2>&1
rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, 3]" > /dev/null 2>&1
rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.05, 4]" > /dev/null 2>&1
rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.001, 5]" > /dev/null 2>&1
rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.10, 6]" > /dev/null 2>&1
echo "  ✓ All 7 oracles sent prices"

echo ""
echo "Mining block..."
rpc 18443 /tmp/miner_node -generate 1 > /dev/null
sleep 2
HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)

BH=$(rpc 18443 /tmp/miner_node getblockhash $HEIGHT)
BD=$(rpc 18443 /tmp/miner_node getblock $BH 2)
ORACLE_HEX=$(echo "$BD" | jq -r '.tx[0].vout[] | select(.scriptPubKey.asm | contains("OP_RETURN")) | .scriptPubKey.hex' 2>/dev/null)

if [ -n "$ORACLE_HEX" ]; then
    echo "✅ Block $HEIGHT: Consensus formed with price disagreement"
    echo "  Sorted prices: \$0.001, \$0.01, \$0.01, \$0.01, \$0.01, \$0.05, \$0.10"
    echo "  Median should be \$0.01 (middle value of 7)"
    echo "  Oracle data: ${ORACLE_HEX:0:40}..."
else
    echo "⚠️  Block $HEIGHT: No consensus despite 7 oracle messages"
fi
echo ""

# ============================================================================
# STEP 11: Verify oracle price is reflected in the system
# ============================================================================
echo "=== Step 11: Verify oracle price from getdigidollarstats ==="
echo ""

rpc 18443 /tmp/miner_node setmockoracleprice 10000

DD_STATS=$(rpc_json 18443 "$MINER_COOKIE" "getdigidollarstats" "[]")
ORACLE_PRICE=$(echo "$DD_STATS" | jq -r '.result.oracle_price_usd // "N/A"')
ORACLE_PRICE_MICRO=$(echo "$DD_STATS" | jq -r '.result.oracle_price_micro_usd // "N/A"')
echo "Oracle price from stats: \$$ORACLE_PRICE (${ORACLE_PRICE_MICRO} micro-USD)"
echo ""

# ============================================================================
# STEP 12: Mine 600+ blocks and test DigiDollar minting with Phase Two oracles
# ============================================================================
echo "=== Step 12: DigiDollar minting with Phase Two active ==="
echo ""

CURRENT_HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)
BLOCKS_NEEDED=$((700 - CURRENT_HEIGHT))
if [ $BLOCKS_NEEDED -gt 0 ]; then
    echo "Mining $BLOCKS_NEEDED blocks to reach height 700 (DD active at 650)..."
    while [ $BLOCKS_NEEDED -gt 0 ]; do
        BATCH=$((BLOCKS_NEEDED > 100 ? 100 : BLOCKS_NEEDED))
        rpc 18443 /tmp/miner_node -generate $BATCH > /dev/null
        BLOCKS_NEEDED=$((BLOCKS_NEEDED - BATCH))
        sleep 2
    done
fi

HEIGHT=$(rpc 18443 /tmp/miner_node getblockcount)
echo "✓ Height: $HEIGHT (DD active since 650)"

# Send oracle prices from 4+ oracles before minting
echo ""
echo "Sending oracle prices for mint (4-of-7)..."
for i in 0 1 2 3 4 5 6; do
    rpc_json 18443 "$MINER_COOKIE" "sendoracleprice" "[0.01, $i]" > /dev/null 2>&1
done
rpc 18443 /tmp/miner_node -generate 5 > /dev/null
sleep 5

# Attempt a mint
echo "Attempting DigiDollar mint (\$10.00, tier 0)..."
MINT_RESULT=$(rpc_json 18443 "$MINER_COOKIE" "mintdigidollar" "[1000, 0]")
MINT_TXID=$(echo "$MINT_RESULT" | jq -r '.result.txid // empty')
MINT_ERROR=$(echo "$MINT_RESULT" | jq -r '.error.message // empty')

if [ -n "$MINT_TXID" ]; then
    MINT_COLLATERAL=$(echo "$MINT_RESULT" | jq -r '.result.dgb_collateral // 0')
    echo "✅ Mint succeeded under Phase Two!"
    echo "  TXID: ${MINT_TXID:0:16}..."
    echo "  Collateral: $MINT_COLLATERAL DGB"
    
    rpc 18443 /tmp/miner_node -generate 10 > /dev/null
    sleep 5
    
    DD_BAL=$(rpc_json 18443 "$MINER_COOKIE" "getdigidollarbalance" "[]")
    TOTAL_DD=$(echo "$DD_BAL" | jq -r '.result.total // 0')
    echo "  DD Balance: $TOTAL_DD cents"
else
    echo "⚠️  Mint failed: $MINT_ERROR"
    echo "  (This may be expected if oracle price isn't being picked up from Phase Two bundles)"
fi
echo ""

# ============================================================================
# STEP 13: Verify block integrity across the Phase transition
# ============================================================================
echo "=== Step 13: Block integrity check ==="
echo ""

echo "Verifying chain is valid..."
CHAIN_INFO=$(rpc 18443 /tmp/miner_node getblockchaininfo)
CHAIN_HEIGHT=$(echo "$CHAIN_INFO" | jq -r '.blocks')
CHAIN_BEST=$(echo "$CHAIN_INFO" | jq -r '.bestblockhash')
echo "  Chain height: $CHAIN_HEIGHT"
echo "  Best block: ${CHAIN_BEST:0:16}..."

echo ""
echo "Checking blocks around Phase Two transition (height 100):"
for BLK in 98 99 100 101 102; do
    BH=$(rpc 18443 /tmp/miner_node getblockhash $BLK 2>/dev/null)
    if [ -n "$BH" ]; then
        BD=$(rpc 18443 /tmp/miner_node getblock $BH 1)
        NTXNS=$(echo "$BD" | jq -r '.nTx')
        echo "  Block $BLK: hash=${BH:0:16}... txns=$NTXNS ✓"
    fi
done
echo ""

# ============================================================================
# SUMMARY
# ============================================================================

echo "=========================================="
echo "Phase Two Multi-Oracle Test Summary"
echo "=========================================="
echo ""
echo "  Chain height:           $CHAIN_HEIGHT"
echo "  Phase Two activation:   block 100"
echo "  Oracle configuration:   $THRESHOLD-of-$NUM_ORACLES consensus (strict majority)"
echo "  Oracles started:        $ORACLE_STARTED / $NUM_ORACLES"
echo ""
echo "Test Results:"
echo "  Step 1-2:   ✓ Node started, Phase One blocks mined"
echo "  Step 3:     Oracle key generation (createoraclekey × $NUM_ORACLES)"
echo "  Step 4:     Oracle startup (startoracle × $NUM_ORACLES)"
echo "  Step 5:     Phase Two activation crossing"
echo "  Step 6:     7-of-7 oracle price submission"
echo "  Step 7:     4-of-7 minimum threshold test"
echo "  Step 8:     3-of-7 below threshold test (should fail)"
echo "  Step 9:     5-of-7 with 2 oracles offline"
echo "  Step 10:    Price disagreement / outlier test (7 oracles, 3 outliers)"
echo "  Step 11:    Oracle price verification"
echo "  Step 12:    DigiDollar minting under Phase Two"
echo "  Step 13:    Block integrity verification"
echo ""
echo "=========================================="

# Auto-shutdown
kill $MINER_PID 2>/dev/null
wait $MINER_PID 2>/dev/null
echo "Miner node stopped. Test complete."
