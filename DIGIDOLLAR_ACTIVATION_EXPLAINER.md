# DigiDollar BIP9 Activation — Complete Explainer

## Overview

DigiDollar activates on the DigiByte blockchain through **BIP9 version bit signaling** — the same proven mechanism used by Bitcoin for SegWit and other soft forks. This ensures DigiDollar only activates when a supermajority of miners explicitly signal support, preventing chain splits and ensuring network consensus.

**Key principle:** Nothing DigiDollar-related works until activation. No RPCs, no P2P oracle messages, no DD transactions, no DD opcodes. The entire feature is dormant until BIP9 reaches `ACTIVE` state.

---

## BIP9 Deployment Parameters

### Mainnet
| Parameter | Value |
|-----------|-------|
| Bit | 23 |
| Start Time | May 1, 2026 (epoch 1777593600) |
| Timeout | May 1, 2028 (epoch 1840752000) |
| Min Activation Height | 22,014,720 |
| Confirmation Window | 40,320 blocks (~1 week) |
| Threshold | 70% (28,224 of 40,320) |

### Testnet (testnet23)
| Parameter | Value |
|-----------|-------|
| Bit | 23 |
| Start Time | Genesis timestamp (already past) |
| Timeout | Jan 1, 2028 (epoch 1830297600) |
| Min Activation Height | 600 |
| Confirmation Window | 200 blocks |
| Threshold | 70% (140 of 200) |

### Regtest
| Parameter | Value |
|-----------|-------|
| Status | ALWAYS_ACTIVE |
| Min Activation Height | 0 |

---

## BIP9 State Machine

DigiDollar follows the standard BIP9 state transitions:

```
DEFINED ──→ STARTED ──→ LOCKED_IN ──→ ACTIVE
   │            │
   │            └──→ FAILED (if timeout reached)
   └──→ FAILED (if timeout reached before start)
```

### Phase 1: DEFINED (blocks 0–199 on testnet)
- **What happens:** Nothing. DigiDollar deployment exists in the code but signaling hasn't begun.
- **Miner behavior:** Miners don't need to do anything. Block versions don't include bit 23.
- **User experience:** DigiDollar tab visible in Qt but shows "DigiDollar is not yet active on this blockchain" with current BIP9 status.
- **RPC behavior:** All 31 DD/Oracle RPCs return error: "DigiDollar is not yet active on this blockchain"
- **P2P behavior:** Oracle price/bundle/discovery messages are silently dropped.
- **Consensus:** DD transactions rejected with "digidollar-not-active". DD opcodes treated as NOPs.

### Phase 2: STARTED (blocks 200–399 on testnet)
- **What happens:** Miners can now signal support by setting bit 23 in their block version.
- **Miner behavior:** `getblocktemplate` automatically includes bit 23 in the version field because `gbt_force=true`. Any miner using GBT (including cpuminer) signals automatically — no configuration needed.
- **Signaling check:** `getdeploymentinfo` RPC shows signal count and progress toward threshold.
- **User experience:** Same as DEFINED — everything still blocked. Qt overlay shows "STARTED" status.
- **Threshold:** 140 of 200 blocks in the window must signal bit 23 (70%).

### Phase 3: LOCKED_IN (blocks 400–599 on testnet)
- **What happens:** Threshold reached! Activation is guaranteed but delayed until `min_activation_height`.
- **Miner behavior:** Bit 23 is forced into block versions (`nVersion |= Mask`). All blocks signal.
- **User experience:** Still blocked. Qt overlay shows "LOCKED_IN" status. Users know activation is imminent.
- **Why the delay:** `min_activation_height` ensures all nodes have time to upgrade before DD transactions become valid.

### Phase 4: ACTIVE (block 600+ on testnet)
- **What happens:** DigiDollar is fully operational.
- **RPC behavior:** All 31 DD/Oracle RPCs become functional.
- **P2P behavior:** Oracle messages are processed, relayed, and validated.
- **Consensus:** DD transactions are validated. DD opcodes are enforced. `SCRIPT_VERIFY_DIGIDOLLAR` flag is set.
- **Qt behavior:** Activation overlay disappears. Full DD tab (overview, send, receive, mint, redeem, vault, transactions) becomes accessible.
- **Oracle behavior:** Oracle operators can start oracles, submit prices, and participate in consensus.

---

## What Gets Gated (Complete List)

### RPC Commands (31 total — all gated)

**Core DD Operations:**
- `mintdigidollar` — Mint new DigiDollar
- `redeemdigidollar` — Redeem DD back to DGB
- `senddigidollar` — Send DD to another address
- `sendmanydigidollar` — Send DD to multiple addresses in one transaction

**DD Wallet/Balance:**
- `getdigidollaraddress` — Generate DD address
- `getdigidollarbalance` — Get DD balance
- `listdigidollarpositions` — List minted positions
- `listdigidollartxs` — List DD transactions
- `listdigidollaraddresses` — List DD addresses
- `importdigidollaraddress` — Import watch-only DD address

**DD Info/Stats:**
- `getdigidollarstats` — System health stats
- `getdigidollardeploymentinfo` — BIP9 deployment info
- `calculatecollateralrequirement` — Calculate collateral needed
- `estimatecollateral` — Estimate collateral for mint
- `getredemptioninfo` — Get redemption details
- `getprotectionstatus` — DCA protection status
- `getdcamultiplier` — DCA multiplier value
- `validateddaddress` — Validate DD address format

**Oracle Operations:**
- `getoracleprice` — Get current oracle price
- `getalloracleprices` — Get all oracle prices
- `getoracles` — List oracle configuration
- `listoracle` — Show running oracle status
- `getoraclepubkey` — Get oracle public key
- `createoraclekey` — Generate oracle key in wallet
- `startoracle` — Start oracle service
- `stoporacle` — Stop oracle service
- `submitoracleprice` — Submit price via P2P (regtest only)
- `simulatepricevolatility` — Simulate price changes (regtest only)
- `setmockoracleprice` — Set mock oracle price (regtest only)
- `getmockoracleprice` — Get mock oracle price (regtest only)
- `enablemockoracle` — Enable/disable mock oracle (regtest only)

**Note:** `sendoracleprice` was REMOVED as a security vulnerability (fake price injection). Oracle prices come exclusively from live exchange aggregation.

**Gate pattern:** Each RPC checks `DigiDollar::IsDigiDollarEnabled(tip, chainman)` which calls `DeploymentActiveAfter()` — the BIP9 status check.

### P2P Message Handlers (7 total — all gated)

| Message | Handler | Gate |
|---------|---------|------|
| `ORACLEPRICE` | Line ~5440 | `IsOracleActive()` — height-based (nOracleActivationHeight=600) |
| `ORACLEBUNDLE` | Line ~5607 | `IsOracleActive()` — height-based |
| `ORACLECONSENSUS` | Line ~5762 | `IsOracleActive()` — height-based |
| `ORACLEATTESTATION` | Line ~5892 | `IsOracleActive()` — height-based |
| `ORACLEMUSIGNONCE` | Line ~5991 | `IsOracleActive()` — height-based |
| `ORACLEMUSIGPARTIALSIG` | Line ~6103 | `IsOracleActive()` — height-based |
| `GETORACLES` | Line ~6206 | `IsOracleActive()` — height-based |

**Note:** P2P handlers use `Consensus::IsOracleActive()` which is height-based (`nHeight >= nOracleActivationHeight`), not BIP9. On testnet, `nOracleActivationHeight=600` matches `min_activation_height=600`, so they align in practice. On mainnet, `nOracleActivationHeight` is set to `3000000`. A malicious node sending oracle messages before activation gets silently ignored (no ban, no penalty — just dropped).

### Consensus Validation (all BIP9-gated)

1. **Mempool acceptance** (`validation.cpp:~765`): `DigiDollar::HasDigiDollarMarker(tx)` + `IsDigiDollarEnabled()` → rejects DD TXs with `TX_CONSENSUS "digidollar-not-active"`
2. **Block validation** (`validation.cpp:~2874`): Same check during `ConnectBlock()` → rejects blocks containing DD TXs before activation
3. **Script verification** (`validation.cpp:~2566`): `SCRIPT_VERIFY_DIGIDOLLAR` flag only set when `DeploymentActiveAt()` returns true → DD opcodes are NOPs before activation

### Qt GUI

- **DigiDollar tab:** Always visible, but shows activation status overlay (QStackedWidget) when DD inactive
- **Sub-widget polling:** All DD widgets check `isVisible()` before making RPC calls — prevents RPC queue flooding when DD tab is hidden behind overlay
- **Activation check timer:** Runs every 5 seconds, calls `DigiDollar::IsDigiDollarEnabled()`. Stops and reveals DD functionality once active.

---

## Miner Signaling — How It Works

### Why miners signal automatically

The `VBDeploymentInfo` for DigiDollar has `gbt_force = true` (in `src/deploymentinfo.cpp`). This means:

1. During `STARTED` state, `getblocktemplate` includes bit 23 in `vbavailable`
2. Because `gbt_force=true`, the bit is NOT cleared even if the miner doesn't explicitly support "digidollar" in its GBT rules
3. The version field returned by `getblocktemplate` already has bit 23 set
4. cpuminer (and any GBT-based miner) uses this version directly → automatic signaling

### Block version format

```
Base version:  0x20000000 (BIP9 base)
+ Taproot bit: 0x00000004 (bit 2)
+ DD bit:      0x00800000 (bit 23)
= Combined:    0x20800004
```

During STARTED/LOCKED_IN, blocks should have version `0x20800004` or similar (with bit 23 set). Note: SegWit is a buried deployment in DigiByte (activated at a fixed height), not a version bits deployment, so it does not set any bit.

---

## Testing BIP9 Activation

### Functional Tests

1. **`digidollar_activation.py`** — Tests the full activation lifecycle:
   - Mines through DEFINED → STARTED → LOCKED_IN → ACTIVE
   - Verifies DD RPCs fail before activation, work after
   - Tests DD minting, sending, redeeming after activation
   - Checks version bits in block headers

2. **`digidollar_activation_boundary.py`** — Tests edge cases:
   - Exact block boundaries between phases
   - Threshold calculation (exactly 140/200)
   - Below-threshold signaling (remains STARTED)
   - min_activation_height enforcement

### Manual Testing Checklist

Before activation (any block < 600):
- [ ] All 31 DD RPCs return "DigiDollar is not yet active on this blockchain"
- [ ] `getdeploymentinfo` shows correct BIP9 state
- [ ] Qt DD tab shows activation overlay
- [ ] No oracle messages processed (check debug.log)
- [ ] DD transactions rejected from mempool
- [ ] Block version includes bit 23 after STARTED (block 200+)

After activation (block 600+):
- [ ] All DD RPCs functional
- [ ] Can mint DigiDollar
- [ ] Can send DigiDollar
- [ ] Can redeem DigiDollar
- [ ] Oracle can start and submit prices
- [ ] Oracle prices propagate via P2P
- [ ] Qt DD tab shows full functionality
- [ ] `SCRIPT_VERIFY_DIGIDOLLAR` enabled in block script flags

---

## Mainnet Activation Timeline

On mainnet, the process is:

1. **Release:** Publish binaries with DigiDollar code and BIP9 deployment
2. **Upgrade period:** Miners and nodes upgrade (BIP9 start time: May 1, 2026)
3. **Signaling begins:** After start time (May 1, 2026), miners signal bit 23 in blocks
4. **Threshold reached:** 70% of blocks in a 40,320-block window (~1 week) signal support
5. **Lock-in period:** One more 40,320-block window for remaining nodes to upgrade
6. **Activation:** Block height reaches `min_activation_height` (22,014,720) and BIP9 is ACTIVE
7. **DigiDollar live:** All DD functionality enabled across the network

**Timeout:** If 70% signaling is not reached by May 1, 2028, the deployment transitions to FAILED. A new deployment with different parameters would be needed.

---

## Security Considerations

1. **Pre-activation protection:** All DD code paths are gated. A malicious node cannot trick other nodes into processing DD transactions or oracle messages before activation.

2. **No premature mining:** DD opcodes are NOPs before activation. Even if someone crafts a transaction with DD opcodes, they have no effect until `SCRIPT_VERIFY_DIGIDOLLAR` is set.

3. **Oracle P2P safety:** Oracle messages received before activation are silently dropped (not banned). This prevents an attacker from getting peers banned by sending premature oracle messages.

4. **Consensus safety:** Block validation explicitly rejects blocks containing DD transactions before activation. A miner who includes DD TXs in a pre-activation block will have that block rejected by the network.

5. **BIP9 guarantees:** The activation mechanism is the same one Bitcoin used for SegWit. It's battle-tested across multiple blockchains and provides clear upgrade coordination.

---

## File Reference

| Component | File | Function |
|-----------|------|----------|
| BIP9 deployment params | `src/kernel/chainparams.cpp` | `vDeployments[DEPLOYMENT_DIGIDOLLAR]` |
| BIP9 state machine | `src/versionbits.cpp` | `ThresholdConditionChecker` |
| Deployment info | `src/deploymentinfo.cpp` | `VersionBitsDeploymentInfo[]` |
| RPC activation gate | `src/rpc/digidollar.cpp` | `IsDigiDollarEnabled()` check in each RPC |
| P2P activation gate | `src/net_processing.cpp` | `IsOracleActive()` in ORACLEPRICE/BUNDLE/CONSENSUS/ATTESTATION/MUSIGNONCE/MUSIGPARTIALSIG/GETORACLES |
| Mempool gate | `src/validation.cpp:~765` | `IsDigiDollarEnabled()` in `AcceptToMemoryPool` |
| Block validation gate | `src/validation.cpp:~2874` | `IsDigiDollarEnabled()` in `ConnectBlock` |
| Script flags | `src/validation.cpp:~2566` | `SCRIPT_VERIFY_DIGIDOLLAR` flag |
| Qt activation overlay | `src/qt/digidollartab.cpp` | `checkActivationStatus()` timer |
| Qt widget polling guard | `src/qt/digidollar*widget.cpp` | `if (!isVisible()) return;` |
| Oracle height gate | `src/consensus/params.h:244` | `IsOracleActive()` |
| DD enabled check | `src/digidollar/digidollar.cpp` | `IsDigiDollarEnabled()` |
