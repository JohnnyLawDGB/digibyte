# DigiDollar Redemption Sub-Agent Guide

## Your Assignment

You'll be assigned ONE or MORE tasks from the 10 tasks in `DIGIDOLLAR_REDEMPTION_TASKS.md`. Read your assigned task(s) and implement them using TDD.

## Critical Facts

1. **Timelock = Absolute** - OP_CHECKLOCKTIMEVERIFY cannot be bypassed
2. **ERR = Placeholder** - Add stub code only (not implemented yet)
3. **Compile ALWAYS** - After every code change
4. **Test ALWAYS** - After every compilation

## TDD Workflow (MANDATORY)

### Step 1: RED - Run Failing Test
```bash
# See what's broken
./test/functional/test_digidollar_redeem.py
# OR
./src/test/test_digibyte --run_test=digidollar_redeem_*
```

### Step 2: GREEN - Minimal Implementation
```cpp
// Write just enough code to pass the test
// Follow the code examples in DIGIDOLLAR_REDEMPTION_TASKS.md
```

### Step 3: COMPILE
```bash
make -j$(nproc) src/qt/digibyte-qt
# MUST succeed before proceeding
```

### Step 4: TEST
```bash
# Run the test again
./test/functional/test_digidollar_redeem.py
# Should pass or progress further
```

### Step 5: REFACTOR
```cpp
// Improve code quality
// Add logging, comments, error handling
```

### Step 6: REPEAT
```bash
# Compile and test again
make -j$(nproc) src/qt/digibyte-qt
./test/functional/test_digidollar_redeem.py
```

## Task Quick Reference

### Tasks 1-3: Core Logic (Sequential)
**Agent 1 does all three in order**

- **Task 1**: Redemption eligibility checker
  - Create `src/digidollar/redemption.h/cpp`
  - Check if timelock expired
  - Return eligibility info

- **Task 2**: Transaction builder
  - Modify `src/digidollar/txbuilder.cpp`
  - Build redemption TX structure
  - Set `tx.nLockTime = position.unlockHeight` (CRITICAL!)

- **Task 3**: P2TR signing
  - Modify `src/wallet/digidollarwallet.cpp`
  - Sign collateral input with Schnorr
  - Witness: `[signature]` only

### Tasks 4-7: RPC + State (Parallel - 3 agents)

**Agent 1: RPC (Tasks 4-5)**
- **Task 4**: `redeemdigidollar` RPC command
- **Task 5**: `getredemptioninfo` and `listredeemablepositions` RPCs

**Agent 2: State (Task 6)**
- **Task 6**: DD burning and position closure
  - Remove DD UTXOs from map/database
  - Close or update collateral positions

**Agent 3: Testing (Task 7)**
- **Task 7**: Add 1-hour test lock
  - Modify `src/consensus/digidollar.cpp`
  - Add `{"1hour", 240}` to LOCK_PERIODS
  - Fix any broken tests

### Tasks 8-10: GUI (Parallel - 2 agents + integration)

**Agent 1: Vault (Task 8)**
- **Task 8**: Vault tab displaying positions
  - Create/modify `src/qt/digidollarvaultwidget.cpp/h`
  - Table showing positions
  - Redeem button (enabled when redeemable)

**Agent 2: Dialog (Task 9)**
- **Task 9**: Redemption confirmation dialog
  - Create `src/qt/digidollarredeemdialog.cpp/h`
  - Show amounts to burn/unlock
  - Partial redemption option

**Final Agent: Integration (Task 10)**
- **Task 10**: Wire everything together
  - Connect vault to DigiDollar tab
  - Wire redeem button to RPC
  - Run full test suite

## ERR Placeholder Code

```cpp
// In src/consensus/err.cpp (if doesn't exist, create it)
namespace ERR {
    bool IsActive() {
        // TODO: Implement system health check
        return false;  // Always disabled for now
    }

    CAmount CalculateRequirement(CAmount original) {
        // TODO: Implement ERR formula
        return original;  // Always 1:1 for now
    }
}
```

## Common Errors & Fixes

### "Undeclared identifier"
```cpp
// Add include
#include <digidollar/redemption.h>
```

### "Undefined reference"
```makefile
# Add to src/Makefile.am
digidollar/redemption.cpp \
```

### "Cannot find file"
```bash
# Check file path - must match exactly
ls -la src/digidollar/redemption.cpp
```

## Testing Commands

### Unit Tests
```bash
./src/test/test_digibyte --run_test=digidollar_redeem_*
```

### Functional Tests
```bash
./test/functional/test_digidollar_redeem.py
```

### All DigiDollar Tests
```bash
./test/functional/test_digidollar_*.py
./src/test/test_digibyte --run_test=digidollar_*
```

### Manual Qt Test
```bash
./src/qt/digibyte-qt -regtest
# 1. Mint DD with "1hour" lock
# 2. Generate 240 blocks
# 3. DigiDollar tab → Vault
# 4. Click Redeem
# 5. Verify success
```

## Reporting Format

### After Each Task
```markdown
## Task X Complete ✅

**Compiled**: SUCCESS (2m 15s)
**Tests**: PASS
**Files**:
- redemption.cpp: +95 lines
- redemption.h: +25 lines

**Works**: Timelock check correctly rejects before expiry
**Next**: Task X+1
```

### If Blocked
```markdown
## Task X BLOCKED ❌

**Issue**: [describe]
**Error**: [paste error]
**Tried**: [what you attempted]
**Need**: [help needed]
```

## Key Implementation Points

### Timelock Check (MOST IMPORTANT)
```cpp
// ALWAYS check this FIRST
if (::ChainActive().Height() < position.unlockHeight) {
    return false;  // Cannot redeem
}
```

### Set Transaction Locktime
```cpp
// CRITICAL for OP_CHECKLOCKTIMEVERIFY
tx.nLockTime = position.unlockHeight;
```

### Schnorr Signature
```cpp
// Use Schnorr, not ECDSA
SignatureHashSchnorr(sighash, tx, nIn, SIGHASH_DEFAULT,
                     SigVersion::TAPROOT, amount);
ownerKey.SignSchnorr(sighash, sig);
```

### Witness Stack
```cpp
// For redemption: just signature
tx.vin[0].scriptWitness.stack.push_back(sig);
```

## Success Criteria

Your task is complete when:
- [  ] Code compiles without errors
- [ ] Tests pass
- [ ] Timelock correctly enforced (if applicable)
- [ ] No regressions (existing tests still pass)
- [ ] Code follows existing patterns
- [ ] Proper logging added

## Final Checklist

Before reporting complete:
1. Run `make -j$(nproc) src/qt/digibyte-qt` → SUCCESS
2. Run task-specific tests → PASS
3. Run `./src/test/test_digibyte --run_test=digidollar_*` → PASS
4. Check no compilation warnings for your code
5. Verify changes match task specification

**Good luck! 🚀**

---

**Version**: 3.0 - Complete Implementation Guide
**Remember**: Compile → Test → Report
