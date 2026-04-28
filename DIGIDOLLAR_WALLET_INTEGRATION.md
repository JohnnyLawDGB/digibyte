# DigiDollar Wallet Integration Guide

*For wallet providers who already support DigiByte and want to add DigiDollar (DD) support.*

---

## What Is DigiDollar?

DigiDollar is a decentralized USD stablecoin built natively into DigiByte Core. Each DigiDollar = $1 USD, backed by DGB collateral locked in time-locked vaults on-chain. No company controls it — everything runs inside the DigiByte protocol.

There are only 4 operations: **Mint**, **Transfer**, **Redeem**, and regular DGB transactions.

---

## Quick Overview

| Feature | Detail |
|---------|--------|
| Token type | Native UTXO (not a layer-2 token) |
| Address format | `DD...` (mainnet), `TD...` (testnet), `RD...` (regtest) |
| Amount unit | **USD cents** (10000 = $100.00) |
| Fees | Always paid in **DGB** (not DD) |
| Minimum fee | 0.1 DGB per transaction |
| Signing | Schnorr (P2TR) for DD, ECDSA for DGB fee inputs |
| Confirmations | Same as DGB — 15-second blocks |
| Backend | Requires DigiByte Core v9.26.0+ with `digidollar=1` |

---

## 1. Prerequisites

Your wallet must:
- Run DigiByte Core v9.26.0 or later (with DigiDollar consensus rules)
- Set `digidollar=1` in `digibyte.conf`
- Wait for BIP9 activation (DigiDollar features are disabled until activation)

```ini
# digibyte.conf
server=1
digidollar=1
txindex=1  # recommended for full transaction lookups
```

---

## 2. Address Generation

DigiDollar uses its **own address format** — standard DGB addresses won't work for DD operations.

| Network | Prefix | Example |
|---------|--------|---------|
| Mainnet | `DD` | `DD1qw508d6qejxtdg4y5r3z...` |
| Testnet | `TD` | `TD1q7s7uus7u5eteet7qgh6...` |
| Regtest | `RD` | `RD1qhsfjkdl38fjsl29dkf...` |

**Generate a DD address:**
```bash
digibyte-cli getdigidollaraddress
# Returns: "DDxxxxxxxxxxxxxxxxxxxxxxxx"
```

**List all DD addresses in wallet:**
```bash
digibyte-cli listdigidollaraddresses
```

DD addresses are P2TR (Taproot) under the hood, with a 2-byte Base58Check version prefix. Your wallet should validate addresses using the `DD/TD/RD` prefix before sending.

---

## 3. Checking Balances

DigiDollar balances are tracked **separately** from DGB balances. The wallet maintains its own DD UTXO set.

**Get total DD balance:**
```bash
digibyte-cli getdigidollarbalance
# Returns: { "confirmed": 50000, "pending": 10000, "total": 60000 }
# (amounts in cents — 50000 = $500.00)
```

**Get balance for a specific address:**
```bash
digibyte-cli getdigidollarbalance "DDaddress..."
```

**Key points:**
- `confirmed` — DD in confirmed transactions
- `pending` — DD in unconfirmed but trusted transactions
- Users need BOTH a DD balance (to send DD) AND a DGB balance (to pay fees)

---

## 4. Minting DigiDollars

Minting locks DGB as collateral and creates new DD tokens.

### Lock Tiers

| Tier | Lock Period | Collateral Ratio |
|------|-----------|-----------------|
| 0 | 1 hour | 1000% (testing only) |
| 1 | 30 days | 500% |
| 2 | 90 days | 400% |
| 3 | 180 days | 350% |
| 4 | 1 year | 300% |
| 5 | 2 years | 275% |
| 6 | 3 years | 250% |
| 7 | 5 years | 225% |
| 8 | 7 years | 212% |
| 9 | 10 years | 200% |

Longer lock = lower collateral requirement. The collateral stays in YOUR wallet — you never give up your keys.

### Mint Limits

- **Minimum:** $100 (10,000 cents)
- **Maximum:** $100,000 per transaction (10,000,000 cents)

### How to Mint

**Step 1: Check the oracle price**
```bash
digibyte-cli getoracleprice
# Returns current DGB/USD price in micro-USD
```

**Step 2: Estimate collateral needed**
```bash
digibyte-cli calculatecollateralrequirement 10000 180
# 10000 cents ($100), 180 days lock (350% ratio)
# Returns: required DGB amount

# Or use estimatecollateral with tier number:
digibyte-cli estimatecollateral 10000 3
# 10000 cents ($100), tier 3 (180 days)
```

**Step 3: Mint**
```bash
digibyte-cli mintdigidollar 10000 3
# Locks DGB collateral, creates $100 DD
```

**Response:**
```json
{
  "txid": "abc123...",
  "dd_minted": "10000",
  "dgb_collateral": "55468.12345678",
  "lock_tier": 3,
  "unlock_height": 1234567,
  "collateral_ratio": 350,
  "fee_paid": "0.10000000",
  "position_id": "abc123..."
}
```

### What Happens Under the Hood

The mint transaction creates:
1. **Collateral output** (P2TR) — Your DGB locked with a CLTV timelock. Uses a NUMS internal key (mathematically unspendable via key-path), ensuring it can only be unlocked via the script-path after the timelock expires.
2. **DD token output** (P2TR) — A 0-satoshi Taproot output representing your DigiDollars. Freely transferable.
3. **OP_RETURN metadata** — Records the DD amount, lock height, and tier for network-wide tracking.
4. **DGB change** — Any leftover DGB returned to your wallet.

---

## 5. Sending DigiDollars

Sending DD is straightforward — it works like sending any UTXO.

```bash
digibyte-cli senddigidollar "DDrecipientAddress..." 5000
# Sends $50.00 worth of DD
```

**With optional comment:**
```bash
digibyte-cli senddigidollar "DDrecipientAddress..." 5000 "Payment for services"
```

**Response:**
```json
{
  "txid": "def456...",
  "to_address": "DDrecipientAddress...",
  "amount": 5000,
  "status": "sent",
  "fee_paid": "0.10000000",
  "change_amount": 5000
}
```

### Important Notes

- Transfers require **DD UTXOs** (for the value) AND **DGB UTXOs** (for the miner fee)
- Minimum fee: **0.1 DGB** per transaction
- Maximum single transfer: **$100,000**
- DD change is automatically returned to your wallet
- Transfers are **confirmed-only** as of RC32: a DD UTXO must have at least one confirmation before it can be spent in a subsequent transfer or redeem. Consensus refuses to resolve DD amounts from `MEMPOOL_HEIGHT` inputs for transfer/redeem, and the wallet no longer chains unconfirmed DigiDollar outputs. Plan throughput around the 15-second block time.

### Sending to many recipients in one transaction

Use `sendmanydigidollar` to fan out DD to many addresses with a single fee:

```bash
digibyte-cli -rpcwallet=hot sendmanydigidollar '{"DDaddr1...":1500,"DDaddr2...":2500}'
# amounts in cents
```

This is the DigiDollar analogue of `sendmany`. Like `senddigidollar`, it requires confirmed DD inputs and pays the fee in DGB.

---

## 6. Receiving DigiDollars

Receiving DD works like receiving DGB — share your DD address and wait for the transaction.

**Watch for incoming DD:**
```bash
digibyte-cli listdigidollartxs 10 0 "" "receive"
# Lists last 10 received DD transactions
```

**Response includes:**
```json
{
  "txid": "ghi789...",
  "category": "receive",
  "amount": 5000,
  "address": "DDyourAddress...",
  "confirmations": 6,
  "blockheight": 12345,
  "time": 1770934000
}
```

---

## 7. Viewing Transaction History

```bash
# All DD transactions (last 20)
digibyte-cli listdigidollartxs 20

# Filter by category
digibyte-cli listdigidollartxs 10 0 "" "mint"
digibyte-cli listdigidollartxs 10 0 "" "send"
digibyte-cli listdigidollartxs 10 0 "" "receive"
digibyte-cli listdigidollartxs 10 0 "" "redeem"

# Filter by address
digibyte-cli listdigidollartxs 10 0 "DDspecificAddress..."
```

**Transaction categories:**
- `mint` — You minted new DD (locked DGB collateral)
- `send` — You sent DD to someone
- `receive` — You received DD from someone
- `redeem` — You redeemed DD back to DGB

---

## 8. Managing Collateral Positions

When you mint DD, you create a collateral position. You can view and manage these:

```bash
# List all your positions (filterable by tier / amount / active)
digibyte-cli listdigidollarpositions

# Check if a position can be redeemed
digibyte-cli getredemptioninfo "position_id"
```

`listdigidollarpositions` reports `unlock_height` and `is_redeemable` for each position; clients should filter on those fields rather than calling a separate "redeemable only" RPC. (The legacy `listredeemablepositions` symbol exists in `src/rpc/digidollar_transactions.cpp` but is not registered.)

Each position tracks:
- DD amount minted
- DGB collateral locked
- Lock tier and unlock height
- Whether it's active or redeemed

---

## 9. Redeeming DigiDollars

Redeeming burns DD tokens and unlocks your DGB collateral. The timelock must have expired.

```bash
# Redeem a position (must redeem full vault amount)
digibyte-cli redeemdigidollar "position_id" 10000
# position_id = the mint transaction hash
# amount = DD cents to redeem (must match full vault amount)
```

### Two Redemption Paths

- **Normal** (system health ≥ 100%): Burn your original DD amount → get 100% of your collateral back
- **ERR** (system health < 100%): You may need to burn extra DD (up to 125%) to get your full collateral back. This creates buying pressure on DD during crises, helping stabilize the peg.

**Critical rule:** Collateral can NEVER be unlocked before the timelock expires. No exceptions. No forced liquidation. No margin calls. Your DGB is safe.

---

## 10. Network Health & Oracle Data

```bash
# System-wide DD stats
digibyte-cli getdigidollarstats
# Returns: total DD supply, total collateral, system health ratio

# Current oracle price
digibyte-cli getoracleprice
# Returns: DGB/USD price, staleness info

# Check activation status
digibyte-cli getdigidollardeploymentinfo
# Returns: BIP9 status, signaling progress
```

---

## 11. Identifying DD Transactions (Raw Parsing)

If your wallet parses raw transactions, here's how to identify DD transactions:

**Check the transaction version:**
```
(tx.nVersion & 0x0000FFFF) == 0x0770  → It's a DD transaction
```

**Extract the type:**
```
(tx.nVersion & 0xFF000000) >> 24
  1 = MINT
  2 = TRANSFER
  3 = REDEEM
```

**Find the DD amount in the OP_RETURN output:**
- Format: `OP_RETURN "DD" <txType> <ddAmount> <lockHeight> [<lockTier>]`
- DD amount is in cents (int64)

**DD token outputs** have 0-satoshi value with P2TR scripts. The actual DD value is in the OP_RETURN.

**Custom opcodes (soft-fork via OP_NOP slots):**
| Opcode | Hex | Purpose |
|--------|-----|---------|
| `OP_DIGIDOLLAR` | `0xbb` | Marks DD outputs |
| `OP_DDVERIFY` | `0xbc` | Verify DD conditions |
| `OP_CHECKPRICE` | `0xbd` | Oracle price verification |
| `OP_CHECKCOLLATERAL` | `0xbe` | Collateral ratio check |

Non-DD-aware wallets can safely ignore these — they fall through as NOPs.

---

## 12. RPC Quick Reference

### Wallet RPCs (require loaded wallet)

| Command | Description |
|---------|-------------|
| `getdigidollaraddress` | Generate new DD deposit address |
| `listdigidollaraddresses` | List all DD addresses in wallet |
| `getdigidollarbalance [addr] [minconf]` | Get DD balance (confirmed + pending) |
| `mintdigidollar <cents> <tier>` | Mint DD by locking DGB collateral |
| `senddigidollar <addr> <cents>` | Send DD to a DD address |
| `sendmanydigidollar <amounts_obj>` | Send DD to multiple DD addresses in one tx |
| `listdigidollartxs [count] [skip] [addr] [category]` | List DD transaction history |
| `listdigidollarpositions` | List all collateral positions |
| `getredemptioninfo <position_id>` | Check redemption status of a position |
| `redeemdigidollar <position_id> <cents>` | Redeem DD → unlock DGB collateral |

### Information RPCs (no wallet needed)

| Command | Description |
|---------|-------------|
| `getdigidollardeploymentinfo` | BIP9 activation status |
| `getdigidollarstats` | Network-wide DD supply and health |
| `getoracleprice` | Current DGB/USD oracle price |
| `calculatecollateralrequirement <cents> <lock_days>` | Calculate needed collateral |
| `estimatecollateral <cents> <tier>` | Estimate collateral at current price |
| `getdigidollardeploymentinfo` | BIP9 activation status, signaling progress |

---

## 13. Test on Testnet Now!

DigiDollar is **live and activated on testnet23**. You can start integrating today.

### Quick Setup

1. Download the latest DigiByte Core v9.26.0 RC build (RC30 or later)
2. Configure for testnet:
   ```ini
   testnet=1
   [test]
   digidollar=1
   addnode=oracle1.digibyte.io
   server=1
   rpcuser=yourusername
   rpcpassword=yourpassword
   ```
3. Launch: `digibyted -testnet -daemon`
4. Get testnet DGB from the dev chat: https://app.gitter.im/#/room/#digidollar:gitter.im
5. Start minting, sending, and receiving DD!

### Testnet Details

| Parameter | Value |
|-----------|-------|
| Testnet name | testnet23 |
| P2P Port | 12030 |
| DD Address Prefix | `TD` |
| Oracle Consensus | 9-of-17 MuSig2 Schnorr threshold (RC30+) |
| Exchange Sources | Binance, CoinGecko, KuCoin, Gate.io, HTX, Crypto.com |
| Activation | Active (BIP9 ACTIVE at block 600) |

### Mainnet Timeline

- **BIP9 signaling window:** May 1, 2026 → May 1, 2028
- **Target release date:** May 1, 2026
- Miners will vote to activate by signaling bit 23

---

## Questions?

Join the developer chat: https://app.gitter.im/#/room/#digidollar:gitter.im

Track testnet activation: https://digibyte.io/testnet/activation

💎 DigiDollar — the first truly decentralized stablecoin on a UTXO blockchain.
