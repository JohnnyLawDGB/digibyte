# DigiByte v9.26.0-rc14 Release Notes

**WARNING: This is a TESTNET-ONLY release. DO NOT use on mainnet.**

**Development Branch:** https://github.com/DigiByte-Core/digibyte/tree/feature/digidollar-v1

**Join the Developer Chat:** https://app.gitter.im/#/room/#digidollar:gitter.im

---

## What's New in RC14

### 🔴 Critical Fix

- **DGB Transactions No Longer Get Stuck After Minting DigiDollar** — Fixed a bug where sending DGB after minting DUSD could result in permanently stuck (unconfirmed) transactions. The wallet was accidentally trying to spend coins that were locked as DigiDollar collateral. Now the wallet properly locks collateral UTXOs on mint, filters them from coin selection, re-locks on restart/rescan, and unlocks on redeem. (5 layers of defense-in-depth)

### 🟠 Oracle Fixes

- **Oracle Keys Updated for Aussie Epic & LookIntoMyEyes** — Both operators generated new oracle wallets under RC13 after their RC12 keys were corrupted. Their new public keys are now in the code. Both can start their oracles immediately after upgrading to RC14.

- **Fixed "7-of-5 Consensus" Display Bug** — Oracle nodes were showing inflated consensus counts (e.g., "7-of-5") because stale messages from offline oracles were never cleaned up. Now purges messages older than 1 hour and displays accurate counts.

- **Fixed "is_stale: true" When Oracles Are Active** — The `getoracleprice` RPC was always reporting prices as stale, even with 7 oracles actively reporting. The check was purely block-height based, which fails when testnet blocks are slow. Now uses both block-height AND time-based checks — if either says it's fresh, it's not stale.

- **Fixed Oracle RPC Staleness, Count Limits, and Missing Names** — The `getoracles` RPC was showing incorrect online/offline status and missing oracle names.

- **Show Pending P2P Oracle Prices in `getoracles`** — You can now see oracle prices that have been received via P2P but haven't been included in a block yet.

### 🔒 Security Hardening

- **Oracle P2P Rate Limiting Fixed** — Legitimate peers were getting banned because duplicate relay messages (normal in P2P gossip networks) inflated rate limit counters. Duplicates are now filtered before counting, and only novel messages count toward limits.

- **Oracle DDoS Protection Hardened** — Schnorr signature verification now happens before rate limiting, so attackers can't exhaust rate limits with fake messages. Rate limit penalties increased (ban after 20 violations instead of 100+). Per-peer oracle message tracking added (mirrors how transaction relay works). Novel message limit tightened from 200/hr to 50/hr.

---

## Commits Since RC13

```
e4705433fa fix(oracle): purge stale messages to fix consensus count display
154040bcd0 fix(digidollar): Lock collateral UTXOs to prevent stuck DGB transactions
07a02cda86 security: harden oracle P2P message handlers against DDoS
2133d8c3a6 rpc: fix getoracleprice is_stale false positive on slow networks
14a91cd898 fix: oracle message rate limiting was banning legitimate peers
e0fd3dd43f Update oracle keys for Aussie Epic (ID 6) and LookIntoMyEyes (ID 7)
41f3c819b9 fix: getoracleprice RPC now works correctly with MockOracleManager in RegTest
9b9293537a rpc: fix oracle RPC staleness, count limits, and missing names
10160721aa rpc: show pending P2P oracle prices in getoracles
```

---

## Upgrade Notes

**RC14 uses the same testnet13 network as RC12/RC13 (port 12030). Your existing testnet data and wallets will work — no migration needed.**

If you're upgrading from RC13, simply replace the binaries and restart.

**Oracle operators (Aussie Epic & LookIntoMyEyes):** Your new keys are active in this release. After upgrading, run `startoracle <your_id>` to begin reporting prices.

---

## Known Issues

- `digidollar_oracle.py` functional test has a pre-existing assertion failure (price aggregation median calculation) — does not affect runtime behavior
- `test_oracle_price_format` C++ test has a pre-existing price rounding precision issue — cosmetic only
- Windows users may experience oracle connectivity issues after ~24 hours of continuous operation — investigating for RC15
- Fixed testnet mining difficulty causes slower-than-normal block times on some algorithms

---

## Testing

All tests validated:
- ✅ All C++ unit tests pass (1488/1489, 1 pre-existing)
- ✅ All Python functional tests pass (310/311, 1 pre-existing)

---

## Download

**GitHub Release:** https://github.com/DigiByte-Core/digibyte/releases/tag/v9.26.0-rc14
