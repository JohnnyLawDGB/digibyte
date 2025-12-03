# DigiDollar Oracle Testing Guide

## Overview

This guide documents how to test the DigiDollar Oracle system on DigiByte testnet. The oracle fetches **real DGB/USD prices** from multiple cryptocurrency exchanges and embeds them in the blockchain via miner oracle bundles.

### Phase One Configuration (Testnet)
- **Consensus**: 1-of-1 (single oracle for testing)
- **Activation Height**: 650
- **Oracle 0 Private Key**: `0000000000000000000000000000000000000000000000000000000000000001`
- **Oracle 0 Public Key**: `0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798`

### Price Formats
- **micro-USD**: 1,000,000 = $1.00 (full precision)
- **cents**: 100 = $1.00 (rounded)
- **USD**: Standard decimal format

---

## Quick Start (TL;DR)

```bash
# 1. Start oracle with testnet key
digibyte-cli -testnet startoracle 0 "0000000000000000000000000000000000000000000000000000000000000001"

# 2. Verify oracle is fetching real prices
digibyte-cli -testnet listoracles | grep -A5 '"oracle_id": 0'

# 3. Mine a block to embed the price
digibyte-cli -testnet generatetoaddress 1 "YOUR_ADDRESS"

# 4. Check oracle price
digibyte-cli -testnet getoracleprice
```

---

## Step-by-Step Testing

### Step 1: Verify DigiDollar is Active

DigiDollar activates at block 650 on testnet. Check current height:

```bash
digibyte-cli -testnet getblockcount
# Must be >= 650
```

### Step 2: Start the Oracle

Start oracle 0 with the testnet private key:

```bash
digibyte-cli -testnet startoracle 0 "0000000000000000000000000000000000000000000000000000000000000001"
```

**Expected Output:**
```json
{
  "success": true,
  "oracle_id": 0,
  "status": "running",
  "message": "Oracle added and started with provided private key",
  "was_already_running": false
}
```

### Step 3: Verify Oracle is Fetching Real Prices

Wait 5 seconds for the oracle to fetch prices from exchanges, then check:

```bash
digibyte-cli -testnet listoracles | head -20
```

**Expected Output (Oracle 0):**
```json
{
  "oracle_id": 0,
  "pubkey": "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798",
  "endpoint": "oracle1.digidollar.org:9001",
  "is_active": true,
  "is_running": true,
  "is_enabled": true,
  "last_price": 6422,      <-- REAL DGB price in micro-USD ($0.006422)
  "last_update": 1764798804,
  "status": "running",
  "selected_for_epoch": true
}
```

**Key Fields:**
- `is_running: true` - Oracle is actively running
- `last_price: 6422` - Real price from exchanges (6422 micro-USD = $0.006422)

### Step 4: Mine a Block

Mine a block to embed the oracle price in the blockchain:

```bash
digibyte-cli -testnet generatetoaddress 1 "dgbt1q5h54gt205546jnl4wvzn0sm9q4mqsz5kv8fulc"
```

### Step 5: Verify Oracle Price via RPC

```bash
digibyte-cli -testnet getoracleprice
```

**Expected Output:**
```json
{
  "price_micro_usd": 6422,     <-- Full precision (6422 = $0.006422)
  "price_cents": 1,            <-- Rounded to nearest cent
  "price_usd": 0.006422,       <-- Human-readable USD value
  "last_update_height": 662,
  "last_update_time": 1764798952,
  "validity_blocks": 20,
  "is_stale": false,
  "oracle_count": 1,
  "status": "active",
  "24h_high": 1,
  "24h_low": 1,
  "volatility": 2.5
}
```

---

## Testing in Qt Wallet

### Using Debug Console

1. Open DigiByte-Qt with testnet: `digibyte-qt -testnet`
2. Go to **Help > Debug Window > Console**
3. Run these commands:

```
# Start the oracle
startoracle 0 "0000000000000000000000000000000000000000000000000000000000000001"

# Wait 5 seconds, then check status
listoracles

# Mine a block
generatetoaddress 1 "dgbt1qYOUR_ADDRESS"

# Check oracle price
getoracleprice
```

### GUI Price Display

The oracle price is displayed in the Qt wallet's overview area. After mining a block with a running oracle, the DigiDollar price should update automatically.

---

## Exchange Price Sources

The oracle fetches prices from multiple exchanges and aggregates them:

| Exchange | API | Pair |
|----------|-----|------|
| Binance | Public | DGB/USDT or DGB/BTC→BTC/USDT |
| CoinGecko | Public | DGB/USD |
| Kraken | Public | DGB/USD |
| KuCoin | Public | DGB/USDT |
| Gate.io | Public | DGB/USDT |
| HTX (Huobi) | Public | DGB/USDT |
| Crypto.com | Public | DGB/USD |
| CoinMarketCap | API Key | DGB/USD |

The oracle uses a median price with outlier detection (10% threshold) to ensure accuracy.

---

## Troubleshooting

### Problem: `getoracleprice` returns all zeros

**Cause:** Oracle is not running or no price has been mined yet.

**Solution:**
```bash
# Check if oracle is running
digibyte-cli -testnet listoracles | grep -A10 '"oracle_id": 0'

# If not running, start it
digibyte-cli -testnet startoracle 0 "0000000000000000000000000000000000000000000000000000000000000001"

# Wait 5 seconds for price fetch
sleep 5

# Mine a block
digibyte-cli -testnet generatetoaddress 1 "YOUR_ADDRESS"
```

### Problem: `last_price: 0` in listoracles

**Cause:** Exchange API fetch failed or network issue.

**Solution:** Wait and retry. The oracle retries automatically. Check debug.log for errors:
```bash
tail -100 ~/.digibyte/testnet4/debug.log | grep -i oracle
```

### Problem: Oracle shows `is_stale: true`

**Cause:** Price is older than `validity_blocks` (20 blocks).

**Solution:** Mine more blocks with the oracle running:
```bash
digibyte-cli -testnet generatetoaddress 5 "YOUR_ADDRESS"
```

---

## Verified Test Results (2025-12-03)

### Test Environment
- DigiByte testnet (DigiDollar Phase One)
- Block height: 662
- Network: Local testnet node

### Test Execution

1. **Started Oracle:**
   ```bash
   $ digibyte-cli -testnet startoracle 0 "0000000000000000000000000000000000000000000000000000000000000001"
   {
     "success": true,
     "oracle_id": 0,
     "status": "running"
   }
   ```

2. **Verified Real Price Fetch:**
   ```bash
   $ digibyte-cli -testnet listoracles | head -15
   {
     "oracle_id": 0,
     "is_running": true,
     "last_price": 6422,    <-- Real DGB price: $0.006422
     "status": "running"
   }
   ```

3. **Mined Block:**
   ```bash
   $ digibyte-cli -testnet generatetoaddress 1 "dgbt1q5h54gt205546jnl4wvzn0sm9q4mqsz5kv8fulc"
   ["fdc8293501f5a76d1278ff65b9ee28c8d1bc119585b2eab040557f670a36bae9"]
   ```

4. **Verified Oracle Price:**
   ```bash
   $ digibyte-cli -testnet getoracleprice
   {
     "price_micro_usd": 6422,    <-- Correct!
     "price_cents": 1,
     "price_usd": 0.006422,      <-- Real DGB/USD price!
     "oracle_count": 1,
     "status": "active"
   }
   ```

### Test Result: **PASS**

The oracle successfully:
- Fetched real DGB/USD prices from live exchanges
- Returned accurate price of ~$0.0064 (matching current market)
- Embedded price in mined block
- Made price available via `getoracleprice` RPC
- Set status to "active" with oracle_count: 1

---

## RPC Commands Reference

| Command | Description |
|---------|-------------|
| `startoracle <id> "<privkey>"` | Start an oracle with private key |
| `stoporacle <id>` | Stop a running oracle |
| `listoracles` | List all oracles with status |
| `getoracleprice` | Get current oracle price |
| `getoracleinfo` | Get oracle system info |
| `getdigidollarinfo` | Get DigiDollar system status |

---

## Important Notes

1. **Oracle Must Be Started Each Session**: When the wallet/daemon restarts, you must run `startoracle` again to begin fetching prices.

2. **Price Persistence**: Once a price is mined into a block, it persists in the blockchain. The `getoracleprice` RPC reads from the most recent oracle bundle.

3. **Testnet Only**: The testnet private key (`0000...0001`) is **only for testnet**. Mainnet will use different, securely managed keys.

4. **Phase One Limitations**: Single oracle consensus means any oracle can set the price. This is intentional for Phase One testing. Phase Two introduces multi-oracle consensus (e.g., 5-of-8).

---

*Last Updated: 2025-12-03*
*Tested on: DigiByte v8.26 testnet, DigiDollar Phase One*
