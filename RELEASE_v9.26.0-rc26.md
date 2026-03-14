# DigiByte v9.26.0-rc26 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## ⚠️ SAME TESTNET — NO RESET

**RC26 uses the same testnet19 chain as RC19–RC25.** No data migration needed.

- **No chain reset** — your blockchain data, wallets, and oracle keys all carry over
- **Same ports:** P2P **12033**, RPC **14025**
- **Same oracle consensus:** **5-of-9** Schnorr threshold
- **Just update the binary** and restart your node

---

## What's New in RC26

RC26 fixes **8 RPC display bugs** reported during RC25 testing. These were all data display and response format issues — no consensus or protocol changes. All RPCs now return correct, real data instead of hardcoded mocks or broken formatting.

### Bug Fixes

1. **getoracleprice: price_cents returned 0 for sub-cent prices (Bug #2)** — Integer division truncated sub-cent values to 0. Oracle price 4042 micro_usd ($0.004042/DGB) now correctly returns `price_cents: 0.4029` instead of `0`. Changed from integer CAmount to fractional double.

2. **getoracleprice: 24h_high/low and volatility hardcoded (Bug #8)** — `24h_high` and `24h_low` were computed as `priceCents ± 5%` which produced 0 when price_cents was 0. Volatility was hardcoded to 2.5. Now scans real oracle price history over up to 5760 blocks (24h). Volatility computed as coefficient of variation from recent price samples.

3. **getprotectionstatus: all values hardcoded (Bugs #7, #9)** — Returned `system_health: 150`, `current_multiplier: 1.0`, `tier: healthy`, `current_volatility: 2.5` regardless of actual system state. Now uses the same UTXO-based health calculation as `getdigidollarstats`. Health values are now consistent between RPCs.

4. **senddigidollar: amount shown in satoshi format (Bug #11/25)** — Response showed `amount: 0.00000100` instead of `100` cents because `ValueFromAmount()` treated DD cents as satoshis. Fee and inputs_used were hardcoded to 0. Now returns raw integer cents, real DGB fee from wallet data, and actual input count.

5. **senddigidollar: sub-dollar amounts crashed (Bug #18)** — Sending fractional amounts like `0.50` crashed with "JSON integer out of range" because the parser used `getInt<int64_t>()`. Now accepts both integer cents (5000 = $50) and decimal dollars (50.00 = $50). Sub-$1 sends are rejected with a clean error message.

6. **listdigidollartxs: fee always 0 on sends (Bug #13)** — The `DDTransaction::fee` field was never populated from wallet transaction data. Now computes real DGB fee using `CachedTxGetDebit - CachedTxGetCredit` for outgoing transactions.

7. **validateddaddress: ismine always false (Bug #17)** — Ownership check was hardcoded to `false`. Added `IsMyDDAddress()` which checks DD balance maps, P2TR output key lookup in `dd_address_keys`, and falls back to standard wallet `IsMine()`. Moved RPC to wallet table for proper wallet context access.

### Pre-Existing Unit Test Fixes

Three unit tests that were already failing before these changes were also fixed:
- `test_validateddaddress_basic` — updated for wallet-only RPC registration
- `test_getoracleprice_basic` — updated for double price_cents type
- `test_oracle_price_format` — updated for fractional price verification

### New Functional Tests

Five new functional tests added to the test suite:
- `digidollar_oracle_price.py` — Verifies sub-cent price_cents and 24h oracle data
- `digidollar_protection_status.py` — Verifies health consistency between RPCs
- `digidollar_send.py` — Verifies response format and sub-dollar error handling
- `digidollar_transaction_fees.py` — Verifies fee reporting on DD sends
- `digidollar_validate_address.py` — Verifies ismine ownership detection

---

## 🧪 Testing

### Test Results
- **C++ unit tests** — 2011/2011 passing (0 failures, up from 3 failures in RC25)
- **Python functional tests** — all passing
- **Live testnet verification** — all 8 bug fixes verified via RPC on testnet19 at height 138788

---

## Commits Since RC25

```
b590c49 fix: validateddaddress now correctly reports ismine for wallet DD addresses (Bug #17)
431dc89 fix: listdigidollartxs now reports actual DGB fees for send transactions (Bug #13)
ce9c955 fix: senddigidollar response amounts and sub-dollar input handling (Bugs #11/25, #18)
89e1b64 fix: getprotectionstatus returns real system health instead of hardcoded mocks (Bugs #7, #9)
b60b7d3 fix: getoracleprice sub-cent price_cents and hardcoded 24h/volatility (Bugs #2, #8)
```

---

## Downloads

| Platform | Filename |
|----------|----------|
| Linux (x86_64) | `digibyte-9.26.0-rc26-x86_64-linux-gnu.tar.gz` |
| Linux (ARM64) | `digibyte-9.26.0-rc26-aarch64-linux-gnu.tar.gz` |
| Windows (64-bit) | `digibyte-9.26.0-rc26-x86_64-w64-mingw32.zip` |
| macOS (Intel) | `digibyte-9.26.0-rc26-x86_64-apple-darwin.tar.gz` |
| macOS (Apple Silicon) | `digibyte-9.26.0-rc26-arm64-apple-darwin.tar.gz` |

---

## Upgrade Instructions

Same as RC25 — just swap the binary and restart:

```bash
# Stop your node
digibyte-cli -testnet stop

# Replace binary with RC26
# (download from releases page or build from source)

# Restart
digibyted -testnet -digidollar=1 -txindex=1 -daemon
```

Oracle operators: your oracle will auto-start (unencrypted wallets) or start after `walletpassphrase` unlock (encrypted wallets).

---

## Known Issues

- **Bug #20:** UTXO auto-consolidation in `mintdigidollar` doesn't account for the +10% collateral margin. Fix in progress (PR #391 v2).
- **Bug #16:** Block production halt from stale DD transactions — fixed in RC25.

---

## RPC Reference

See [RC25 release notes](https://github.com/DigiByte-Core/digibyte/releases/tag/v9.26.0-rc25) for complete RPC reference, oracle setup guide, and GUI instructions.
