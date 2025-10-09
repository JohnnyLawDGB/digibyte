# DigiDollar Qt Test Flow Diagram

## Test Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    ENHANCED DD QT TEST SCRIPT                    │
│                     (3-Wallet Network Test)                      │
└─────────────────────────────────────────────────────────────────┘

┌───────────────┐      ┌───────────────┐      ┌───────────────┐
│   BOB'S NODE  │      │  ALICE'S NODE │      │ CHARLIE'S NODE│
│               │      │               │      │               │
│ Port: 18444   │◄────►│ Port: 18445   │      │ Port: 18448   │
│ RPC:  18443   │      │ RPC:  18446   │◄────►│ RPC:  18447   │
│               │      │               │      │               │
│ Mints DD      │      │ Receives DD   │      │ Receives DD   │
│ Redeems DD    │      │ Observes      │      │ Observes      │
│ Sends DD      │      │               │      │               │
└───────────────┘      └───────────────┘      └───────────────┘
        │                      │                       │
        └──────────────────────┴───────────────────────┘
                               │
                        Network Consensus
                    (All see identical stats)
```

## Test Flow Timeline

### Phase 1: Setup & Initial Minting

```
Step 1-4: Environment Setup
┌──────────────────────────────────────────┐
│ 1. Clean environment                      │
│ 2. Start Bob's Qt node                   │
│ 3. Generate 700 blocks (Bob)             │
│ 4. Set oracle price = $1/DGB             │
└──────────────────────────────────────────┘

Step 5-6: Bob's Initial Mints
┌──────────────────────────────────────────┐
│ Mint #1: $100.00 (365d, tier 4, 300%)   │
│         → 30,000 DGB collateral          │
│                                          │
│ Mint #2: $50.00 (180d, tier 3, 350%)    │
│         → 17,500 DGB collateral          │
│                                          │
│ Mint #3: $25.00 (90d, tier 2, 400%)     │
│         → 10,000 DGB collateral          │
│                                          │
│ Total: $175.00 DD, 57,500 DGB locked     │
└──────────────────────────────────────────┘
                    ↓
              Generate 10 blocks
                    ↓
Step 7-10: Start Other Nodes
┌──────────────────────────────────────────┐
│ 7-8.  Start Alice's node & wallet        │
│ 9-10. Start Charlie's node & wallet      │
│       Both sync to block 710              │
└──────────────────────────────────────────┘
                    ↓
         ╔════════════════════════╗
         ║ MONITOR CHECKPOINT #1  ║
         ║ After Bob's 3 Mints    ║
         ╚════════════════════════╝
```

### Phase 2: 1-Hour Lock Cycle Test

```
Step 11: Bob's 4th Mint (1-hour lock)
┌──────────────────────────────────────────┐
│ Generate 200 more blocks                 │
│                                          │
│ Mint #4: $10.00 (1h, tier 0, 1000%)     │
│         → 10,000 DGB collateral          │
│         → Lock: 240 blocks               │
│                                          │
│ Generate 2+ blocks to confirm            │
│ Height: ~912                             │
└──────────────────────────────────────────┘
                    ↓
         ╔════════════════════════╗
         ║ MONITOR CHECKPOINT #2  ║
         ║ After 4th Mint         ║
         ║ Network: $185.00 DD    ║
         ╚════════════════════════╝
                    ↓
Step 12: Halfway Through Lock
┌──────────────────────────────────────────┐
│ Generate 120 blocks                      │
│ Height: ~1032                            │
│ Remaining lock: 118 blocks               │
└──────────────────────────────────────────┘
                    ↓
         ╔════════════════════════╗
         ║ MONITOR CHECKPOINT #3  ║
         ║ Halfway Through Lock   ║
         ╚════════════════════════╝
                    ↓
Step 13: Test Early Redemption
┌──────────────────────────────────────────┐
│ Try to redeem Mint #4                    │
│ Expected: FAIL (lock not expired)        │
│ ✓ Correctly rejected                     │
└──────────────────────────────────────────┘
                    ↓
Step 14: Past Lock Expiry
┌──────────────────────────────────────────┐
│ Generate 125 more blocks                 │
│ Height: ~1157                            │
│ Lock expired at: ~1150                   │
└──────────────────────────────────────────┘
                    ↓
Step 15: Successful Redemption
┌──────────────────────────────────────────┐
│ Redeem Mint #4                           │
│ Expected: SUCCESS                        │
│ ✓ Returns exactly 10,000 DGB             │
│ ✓ Burns $10.00 DD                        │
│                                          │
│ Generate 10 blocks to confirm            │
│ Height: ~1167                            │
└──────────────────────────────────────────┘
                    ↓
         ╔════════════════════════╗
         ║ MONITOR CHECKPOINT #4  ║
         ║ After Redemption       ║
         ║ Network: $175.00 DD    ║
         ╚════════════════════════╝
```

### Phase 3: DigiDollar Transfers (NEW)

```
Step 16: Transfer Tests

Current State:
┌─────────────────────────────────────────┐
│ Bob:     $175.00 DD  (all supply)       │
│ Alice:   $0.00 DD                       │
│ Charlie: $0.00 DD                       │
│ Network: $175.00 DD                     │
└─────────────────────────────────────────┘

Transfer #1: Bob → Alice ($34.67)
┌─────────────────────────────────────────┐
│ Get Alice's DD address                  │
│ Bob sends 3467 cents to Alice           │
│ Generate 5 blocks to confirm            │
│                                         │
│ Before:                                 │
│   Bob:   17500 cents                    │
│   Alice:     0 cents                    │
│                                         │
│ After:                                  │
│   Bob:   14033 cents (-3467) ✓         │
│   Alice:  3467 cents (+3467) ✓         │
└─────────────────────────────────────────┘
                    ↓
         ╔════════════════════════╗
         ║ MONITOR CHECKPOINT #5  ║
         ║ After Transfer #1      ║
         ║ Network: $175.00 DD    ║
         ║ (unchanged)            ║
         ╚════════════════════════╝
                    ↓
Transfer #2: Bob → Charlie ($12.53)
┌─────────────────────────────────────────┐
│ Get Charlie's DD address                │
│ Bob sends 1253 cents to Charlie         │
│ Generate 5 blocks to confirm            │
│                                         │
│ Before:                                 │
│   Bob:     14033 cents                  │
│   Charlie:     0 cents                  │
│                                         │
│ After:                                  │
│   Bob:     12780 cents (-1253) ✓       │
│   Charlie:  1253 cents (+1253) ✓       │
└─────────────────────────────────────────┘
                    ↓
         ╔════════════════════════╗
         ║ MONITOR CHECKPOINT #6  ║
         ║ After Transfer #2      ║
         ║ Network: $175.00 DD    ║
         ║ (unchanged)            ║
         ╚════════════════════════╝
```

### Phase 4: Final Verification

```
Step 17: Comprehensive Balance Check

Expected Final State:
┌─────────────────────────────────────────┐
│ Bob:     12780 cents = $127.80         │
│          (17500 - 3467 - 1253)          │
│                                         │
│ Alice:    3467 cents = $34.67          │
│          (0 + 3467)                     │
│                                         │
│ Charlie:  1253 cents = $12.53          │
│          (0 + 1253)                     │
│                                         │
│ Sum:     17500 cents = $175.00         │
│ Network: 17500 cents = $175.00         │
│                                         │
│ ✓ Conservation verified!                │
└─────────────────────────────────────────┘

Verification Matrix:
┌───────────┬──────────┬──────────┬────────┐
│  Wallet   │ Expected │  Actual  │ Status │
├───────────┼──────────┼──────────┼────────┤
│ Bob       │  12780   │  12780   │   ✓    │
│ Alice     │   3467   │   3467   │   ✓    │
│ Charlie   │   1253   │   1253   │   ✓    │
│ Network   │  17500   │  17500   │   ✓    │
│ Sum=Net   │   YES    │   YES    │   ✓    │
└───────────┴──────────┴──────────┴────────┘
```

## Network Monitoring at Each Checkpoint

Every checkpoint verifies:

```
╔══════════════════════════════════════════════════╗
║           NETWORK MONITORING CHECKS              ║
╠══════════════════════════════════════════════════╣
║                                                  ║
║ 1. Network Stats Comparison                     ║
║    ┌──────────────────────────────────┐         ║
║    │ Bob's view:   Total DD, Collat   │         ║
║    │ Alice's view: Total DD, Collat   │──→ MUST ║
║    │ Charlie's view: Total DD, Collat │   MATCH ║
║    └──────────────────────────────────┘         ║
║                                                  ║
║ 2. Individual Balances                          ║
║    ┌──────────────────────────────────┐         ║
║    │ Bob:     X cents                 │         ║
║    │ Alice:   Y cents                 │         ║
║    │ Charlie: Z cents                 │         ║
║    └──────────────────────────────────┘         ║
║                                                  ║
║ 3. DD Conservation Law                          ║
║    ┌──────────────────────────────────┐         ║
║    │ Sum(X+Y+Z) = Network Total       │──→ MUST ║
║    │                                  │   EQUAL ║
║    └──────────────────────────────────┘         ║
║                                                  ║
║ ✓ All checks pass → Continue                    ║
║ ✗ Any check fails → EXIT immediately            ║
╚══════════════════════════════════════════════════╝
```

## Key Metrics Tracked

### Network-Wide (Must be Identical on All Nodes)
| Metric | Value | Description |
|--------|-------|-------------|
| Total DD Supply | 17,500 cents | $175.00 in the system |
| Total Collateral | 57,500 DGB | Locked as collateral |
| System Health | 328% | Collateralization ratio |
| Active Vaults | 3 | Long-term vaults (4th redeemed) |

### Individual Balances (Differ Per Wallet)
| Wallet | Balance | Source |
|--------|---------|--------|
| Bob | 12,780 cents | Minted - Sent |
| Alice | 3,467 cents | Received from Bob |
| Charlie | 1,253 cents | Received from Bob |

### Conservation Equation
```
Bob + Alice + Charlie = Network Total
12780 + 3467 + 1253 = 17500 ✓

No DD created or destroyed during transfers!
```

## Test Success Criteria

### Must Pass All:
1. ✅ All 4 mints successful
2. ✅ Early redemption rejected (lock enforced)
3. ✅ Redemption successful after lock
4. ✅ Exact collateral returned
5. ✅ Network stats match on all 3 nodes (at all checkpoints)
6. ✅ Transfer #1 (Bob→Alice) correct amounts
7. ✅ Transfer #2 (Bob→Charlie) correct amounts
8. ✅ Final balances match expected
9. ✅ DD conservation verified (sum = total)
10. ✅ No errors in any operation

### Exit Immediately If:
- ❌ Network stats mismatch between nodes
- ❌ DD conservation violated (sum ≠ total)
- ❌ Any transfer fails
- ❌ Final balances don't match expected
- ❌ Any RPC call fails

## Output Example

```
==========================================
NETWORK MONITOR: After Transfer #1 (Bob → Alice $34.67)
==========================================

Network-Wide Statistics (all nodes should match):
  Bob's view:     Total DD: 17500 cents | Collateral: 57500 DGB | Health: 328%
  Alice's view:   Total DD: 17500 cents | Collateral: 57500 DGB | Health: 328%
  Charlie's view: Total DD: 17500 cents | Collateral: 57500 DGB | Health: 328%

✅ Network Total DD Supply MATCHES on all nodes: 17500 cents ($175.00)
✅ Network Total Collateral MATCHES on all nodes: 57500 DGB
✅ Network Health MATCHES on all nodes: 328%

Individual Wallet DD Balances:
  Bob:     14033 cents ($140.33)
  Alice:   3467 cents ($34.67)
  Charlie: 0 cents ($0.00)

✅ DD CONSERVATION: Sum of wallet balances (17500) = Network total (17500)
```

## Summary

This comprehensive test validates:
- **Multi-node consensus** (3 wallets see same network state)
- **DD lifecycle** (mint → transfer → redeem)
- **Lock enforcement** (time-based restrictions work)
- **Conservation law** (DD is neither created nor destroyed)
- **Balance accuracy** (all math is correct)
- **Network integrity** (all nodes agree at all times)
