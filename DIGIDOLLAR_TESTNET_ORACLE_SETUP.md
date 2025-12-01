# DigiDollar Testnet Oracle Setup Guide

## Overview

This guide covers setting up the first DigiDollar oracle for testnet at `testnetseed.digibyte.io`. The oracle system provides DGB/USD price feeds for the DigiDollar stablecoin protocol.

### Phase One (Testnet) Configuration
- **Consensus**: 1-of-1 oracle (single oracle required)
- **Oracle Activation Height**: Block 650
- **DigiDollar Activation Height**: Block 650
- **Oracle Epoch Length**: 1440 blocks (~6 hours at 15s/block)
- **Price Update Interval**: Every 2 blocks (~30 seconds)

## Architecture

### Components

1. **DigiByte Core Node** - Validates oracle messages and DigiDollar transactions
2. **Oracle Daemon** - Built into DigiByte Core (`OracleManager` class)
3. **Exchange Aggregator** - Fetches prices from 8+ exchanges

### Oracle Key (Testnet)

For Phase One testnet, a hardcoded well-known key is used:
- **Private Key**: `0x0000000000000000000000000000000000000000000000000000000000000001`
- **Public Key (XOnlyPubKey)**: `79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798`

This is the generator point G on the secp256k1 curve - a well-known test key.

## Setup Instructions

### 1. Compile DigiByte Core

```bash
cd /home/jared/Code/digibyte

# If not already compiled:
./autogen.sh
./configure
make -j$(nproc)
```

### 2. Create Testnet Configuration

Create or edit `~/.digibyte/digibyte.conf`:

```bash
mkdir -p ~/.digibyte
cat > ~/.digibyte/digibyte.conf << 'EOF'
# Network
testnet=1
server=1
daemon=1
txindex=1

# RPC Settings (for oracle operations)
rpcuser=digibyterpc
rpcpassword=CHANGE_THIS_TO_SECURE_PASSWORD
rpcallowip=127.0.0.1

# Oracle Settings (Phase One)
debug=digidollar
debug=net

# Mining (for initial block generation)
# Set to your testnet mining address
# gen=1
# genproclimit=1

# Network connectivity
listen=1
port=12028
rpcport=14024

# DNS Seeds
addnode=testnetseed.digibyte.io

# CoinMarketCap API Key (optional - enhances price accuracy)
# coinmarketcap-api-key=YOUR_CMC_API_KEY
EOF
```

### 3. Start Testnet Node

```bash
# Start the daemon
./src/digibyted -testnet

# Check status
./src/digibyte-cli -testnet getblockchaininfo
```

### 4. Create Testnet Wallet

```bash
# Create a new wallet for mining/testing
./src/digibyte-cli -testnet createwallet "testnet_oracle_wallet"

# Generate a new address for mining rewards
./src/digibyte-cli -testnet getnewaddress "mining" "bech32"
```

### 5. Mine Initial Blocks

You need to mine at least 650 blocks to reach DigiDollar/Oracle activation height:

```bash
# Check current height
./src/digibyte-cli -testnet getblockcount

# Generate blocks (using generatetoaddress for testnet)
# Replace ADDRESS with your mining address
./src/digibyte-cli -testnet generatetoaddress 100 "YOUR_MINING_ADDRESS"

# Monitor progress
./src/digibyte-cli -testnet getblockchaininfo
```

**Note**: Since this is a fresh testnet reset for DigiDollar Phase One, you'll need to mine blocks from scratch.

### 6. Verify Oracle System Status

Once you reach height 650+:

```bash
# List configured oracles
./src/digibyte-cli -testnet listoracles

# Check oracle price
./src/digibyte-cli -testnet getoracleprice

# Get DigiDollar system stats
./src/digibyte-cli -testnet getdigidollarstats
```

### 7. Send Oracle Price Updates (Manual Testing)

For Phase One testnet, you can manually send oracle prices:

```bash
# Send a price of $0.05 per DGB using oracle ID 1
./src/digibyte-cli -testnet sendoracleprice 0.05

# With specific oracle ID
./src/digibyte-cli -testnet sendoracleprice 0.05 1
```

## Exchange API Configuration

The oracle aggregator fetches from these exchanges (no API key required for most):

| Exchange | API Key Required | Endpoint |
|----------|------------------|----------|
| Binance | No | `api.binance.com/api/v3/ticker/price?symbol=DGBUSDT` |
| CoinGecko | No | `api.coingecko.com/api/v3/simple/price?ids=digibyte&vs_currencies=usd` |
| Coinbase | No | `api.coinbase.com/v2/prices/DGB-USD/spot` |
| Kraken | No | `api.kraken.com/0/public/Ticker?pair=DGBUSD` |
| KuCoin | No | `api.kucoin.com/api/v1/market/orderbook/level1?symbol=DGB-USDT` |
| Crypto.com | No | `api.crypto.com/v2/public/get-ticker?instrument_name=DGB_USD` |
| Messari | No | `data.messari.io/api/v1/assets/dgb/metrics/market-data` |
| CoinMarketCap | **Yes** | `pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest` |

### Getting CoinMarketCap API Key (Optional)

1. Go to https://pro.coinmarketcap.com/signup
2. Sign up for a free Basic plan (10,000 calls/month)
3. Copy your API key
4. Add to `digibyte.conf`: `coinmarketcap-api-key=YOUR_KEY`

## Automated Oracle Operation

The oracle daemon starts automatically when running on testnet. The built-in `OracleManager` class:

1. Fetches prices from all 8 exchanges every 15 seconds
2. Calculates median price with outlier filtering (10% deviation threshold)
3. Requires minimum 3 valid sources
4. Signs messages with hardcoded testnet key
5. Broadcasts to P2P network

### Starting the Oracle Service

The oracle starts automatically on testnet when conditions are met:
- Network is testnet
- Current height >= activation height (650)
- Oracle key validation passes

Monitor oracle activity:
```bash
tail -f ~/.digibyte/testnet4/debug.log | grep -i oracle
```

## RPC Commands Reference

### DigiDollar Commands
| Command | Description |
|---------|-------------|
| `getdigidollarstats` | Get system health and collateralization |
| `getoracleprice` | Get current oracle price |
| `sendoracleprice <price> [oracle_id]` | Send manual price update (testnet only) |
| `listoracles [active_only]` | List all configured oracles |
| `calculatemint <dgb_amount> <tier>` | Calculate DD mint amounts |
| `mintdigidollar <dgb_amount> <tier>` | Create DigiDollar mint transaction |

### Oracle Status Fields
```json
{
  "price_usd": 0.05,
  "price_cents": 5,
  "last_update": 1704067200,
  "oracle_count": 1,
  "status": "active"
}
```

## Troubleshooting

### Oracle Not Starting
```bash
# Check if on testnet
./src/digibyte-cli -testnet getnetworkinfo

# Check height vs activation
./src/digibyte-cli -testnet getblockcount
# Must be >= 650

# Check debug log
grep -i "oracle" ~/.digibyte/testnet4/debug.log
```

### Price Fetch Failures
```bash
# Check libcurl is compiled
ldd ./src/digibyted | grep curl

# If no curl, rebuild with:
./configure --with-curl
make clean && make -j$(nproc)
```

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Oracle only runs on testnet" | Running on mainnet/regtest | Use `-testnet` flag |
| "Oracle key validation failed" | Key mismatch | Verify using hardcoded testnet key |
| "Insufficient price sources" | <3 exchanges responding | Check network connectivity |

## Security Notes

⚠️ **TESTNET ONLY**: The hardcoded private key (`0x01`) is PUBLIC and should NEVER be used on mainnet.

For mainnet (future phases):
- Oracle operators will generate unique private keys
- Keys will be registered through a governance process
- Multi-oracle consensus (7-of-15 or higher) will be required

## File Locations

| File | Location |
|------|----------|
| Config | `~/.digibyte/digibyte.conf` |
| Testnet Data | `~/.digibyte/testnet4/` |
| Debug Log | `~/.digibyte/testnet4/debug.log` |
| Wallet | `~/.digibyte/testnet4/wallets/` |
| RPC Cookie | `~/.digibyte/testnet4/.cookie` |

## Next Steps

1. **Mine to height 650+** - Required for DigiDollar activation
2. **Test oracle prices** - Use `sendoracleprice` RPC
3. **Test DD minting** - Use `mintdigidollar` after oracle active
4. **Monitor system health** - Use `getdigidollarstats`

---

*This guide is for DigiDollar Phase One testnet. Mainnet deployment will have different security requirements and oracle configurations.*
