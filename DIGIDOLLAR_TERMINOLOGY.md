# DigiDollar Terminology Guide

## ✅ CORRECT TERMINOLOGY

### What They Actually Are
These are **Time-Locked DGB backing DigiDollars** (DDTimeLocks)

### Structure
```
DigiDollar Mint Transaction:
├─ vout[0]: Time-Locked DGB (collateral/backing)
│   ├─ Amount: Based on DD amount × collateral ratio × lock tier
│   ├─ Script: P2TR with timelock
│   └─ Purpose: Backs the DigiDollar value
│
└─ vout[1]: DigiDollar (spendable token)
    ├─ Amount: DD cents (e.g., 50000 = $500.00)
    ├─ Script: P2TR with DD marker
    └─ Purpose: Transferable DigiDollar token
```

### Proper Names
✅ **YES - Use These**:
- "DD Time-Lock"
- "Time-Locked DGB backing DD"
- "DDTimeLock"
- "Time-locked collateral"
- "DD backing structure"

❌ **NO - Don't Use These**:
- "position" (too generic, not descriptive)
- "collateral position" (better but still not clear)
- Just "collateral" (ambiguous)

### In Code Comments
```cpp
// ✅ CORRECT
// Get all DD time-locks (Time-Locked DGB backing DigiDollars)
std::vector<WalletCollateralPosition> timelocks = GetPositions(true);

// Iterate through DD time-locks
for (const auto& timelock : timelocks) {
    // vout[0] = Time-Locked DGB (backing)
    // vout[1] = DigiDollar (spendable)
    COutPoint dd_utxo(timelock.position_id, 1);
}
```

```cpp
// ❌ WRONG
// Get positions
auto positions = GetPositions(true);

// Check position
for (const auto& pos : positions) {
    // ...
}
```

### In Documentation
✅ **CORRECT**:
> "Each DigiDollar is backed by Time-Locked DGB held in a DD time-lock structure. When you mint DD, you create a transaction with vout[0] containing the Time-Locked DGB backing, and vout[1] containing the spendable DigiDollar."

❌ **WRONG**:
> "Each DigiDollar is backed by a position. When you mint DD, you create a position."

### Function Naming

#### Phase 0: Rename Existing Functions (REQUIRED)
```cpp
// OLD NAMES (Phase 0 will rename these):
GetPositions()           // → RENAME to GetDDTimeLocks()
WritePosition()          // → RENAME to WriteDDTimeLock()
ReadPosition()           // → RENAME to ReadDDTimeLock()
position_id              // → RENAME to dd_timelock_id

// NEW NAMES (After Phase 0 refactoring):
GetDDTimeLocks()         // Returns DD time-locks
WriteDDTimeLock()        // Saves DD time-lock
ReadDDTimeLock()         // Reads DD time-lock
dd_timelock_id           // Time-lock transaction ID

// Properly documented:
/**
 * Get active DD time-locks (Time-Locked DGB backing DigiDollars)
 * @param active_only If true, only return active time-locks
 * @return Vector of DD time-lock structures
 */
std::vector<WalletCollateralPosition> GetDDTimeLocks(bool active_only = false);
```

#### Variable Naming (After Phase 0)
```cpp
// Use correct naming in all new code:
timelock.dd_timelock_id  // Time-lock transaction ID
timelock.dd_amount       // DD amount in cents
timelock.dgb_locked      // Time-Locked DGB amount
```

### In User-Facing Text
When explaining to users:

✅ **CORRECT**:
- "Your DigiDollars are backed by Time-Locked DGB"
- "Each DD requires locked DGB collateral"
- "The DGB remains locked for the time period you selected"
- "When the lock expires, you can redeem to unlock your DGB"

❌ **WRONG**:
- "Your DigiDollars are backed by positions"
- "Create a position to mint DD"

### In Error Messages
✅ **CORRECT**:
```cpp
error = "Insufficient Time-Locked DGB to back this DD amount";
error = "Cannot spend DD: time-lock not yet mature";
error = "No active DD time-locks found";
```

❌ **WRONG**:
```cpp
error = "No positions available";
error = "Position not found";
```

### In Test Names
✅ **CORRECT**:
```cpp
BOOST_AUTO_TEST_CASE(test_get_dd_timelocks)
BOOST_AUTO_TEST_CASE(test_timelock_utxo_selection)
BOOST_AUTO_TEST_CASE(test_dd_backed_by_timelocked_dgb)
```

❌ **WRONG**:
```cpp
BOOST_AUTO_TEST_CASE(test_get_positions)
BOOST_AUTO_TEST_CASE(test_position_selection)
```

### In Logs
✅ **CORRECT**:
```cpp
LogPrintf("DigiDollar: Found %d active DD time-locks\n", count);
LogPrintf("DigiDollar: Time-Locked DGB: %d, DD amount: %d\n", dgb, dd);
LogPrintf("DigiDollar: Selecting DD from time-lock %s\n", id.ToString());
```

❌ **WRONG**:
```cpp
LogPrintf("DigiDollar: Found %d positions\n", count);
LogPrintf("DigiDollar: Position amount: %d\n", amount);
```

## Understanding the Concept

### What Happens When You Mint DD?
1. You lock up DGB for a time period (30 days to 10 years)
2. This creates a **Time-Locked DGB backing** (the collateral)
3. In return, you receive **DigiDollar tokens** (spendable)
4. The Time-Locked DGB **backs the value** of the DigiDollars
5. You can **transfer the DD** to others (vout[1])
6. The **Time-Locked DGB stays locked** until maturity (vout[0])
7. At maturity, you can **redeem to unlock your DGB**

### The Two Outputs
```
Mint TX: abc123...
│
├─ Output 0: Time-Locked DGB (BACKING)
│  ├─ Amount: 1000 DGB
│  ├─ Locked until: Block 850,000
│  ├─ Purpose: Backs the DD value
│  └─ Redeemable: After timelock expires
│
└─ Output 1: DigiDollar (SPENDABLE)
   ├─ Amount: 500 DD ($500.00)
   ├─ Backed by: The 1000 DGB in output 0
   ├─ Purpose: Transferable stablecoin
   └─ Can be spent: Immediately
```

### Why "Time-Lock" is the Correct Term
- **Time**: The DGB is locked for a specific time period
- **Lock**: Cannot be spent until maturity
- **DGB**: It's DigiByte being locked
- **Backing**: It backs the DigiDollar value
- **DD**: It enables minting DigiDollars

"Position" is too generic and doesn't convey:
- That it's time-based
- That it's locked
- That it backs DD value
- What asset is locked

## Summary

**Always use**: "Time-Locked DGB backing DD" or "DD Time-Lock"
**Never use**: "position" (unless referring to existing variable names)
**In new code**: Use `timelock`, `dd_timelock`, `DDTimeLock` naming
**In comments**: Always explain: "Time-Locked DGB backing DigiDollars"

---

**This terminology makes the codebase self-documenting and clear to all developers.**
