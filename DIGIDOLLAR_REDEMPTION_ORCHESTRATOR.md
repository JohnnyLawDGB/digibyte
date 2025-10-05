# DigiDollar Redemption - Orchestrator Guide

## Mission

Implement **complete DigiDollar redemption functionality**: RPC commands, GUI, and all tests passing.

## Critical Facts

1. **Timelock is absolute** - OP_CHECKLOCKTIMEVERIFY cannot be bypassed
2. **ERR not implemented yet** - Add placeholder code only
3. **Need full GUI** - Vault tab + redeem dialog working
4. **Need full RPC** - `redeemdigidollar` command functional
5. **All tests must pass** - Unit tests + functional tests

## What We're Building

### RPC Interface
```bash
# Check if position can be redeemed
digibyte-cli getredemptioninfo <collateral_txid:vout>

# Execute redemption
digibyte-cli redeemdigidollar <collateral_txid:vout> [partial_amount]

# List redeemable positions
digibyte-cli listredeemablepositions
```

### GUI Interface
- **Vault Tab**: Shows all collateral positions with status
- **Redeem Button**: Appears when timelock expired
- **Redeem Dialog**: Confirms redemption, shows amounts
- **Status Updates**: Real-time position status

## Task Breakdown (10 Tasks, 3 Phases)

### Phase 1: Core Logic (Sequential - 1 agent)
**Agent 1: Foundation Builder**

**Task 1**: Redemption Eligibility Checker
- Files: `src/digidollar/redemption.h/cpp` (create)
- Check timelock expiration
- Placeholder ERR check (always returns 100%)
- Test: `test/functional/test_digidollar_redeem.py::test_eligibility`

**Task 2**: Redemption Transaction Builder
- Files: `src/digidollar/txbuilder.cpp/h` (extend)
- Build redemption TX structure
- Handle partial redemptions
- Set tx.nLockTime correctly
- Test: `src/test/digidollar_redeem_tests.cpp::test_tx_building`

**Task 3**: P2TR Signing for Redemption
- Files: `src/wallet/digidollarwallet.cpp`
- Sign collateral input after timelock
- Witness stack construction
- Test: `src/test/digidollar_redeem_tests.cpp::test_signing`

---

### Phase 2: RPC + State (Parallel - 3 agents)

**Agent 1: RPC Commands**

**Task 4**: RPC - `redeemdigidollar`
- Files: `src/rpc/digidollar.cpp`
- Implement redemption RPC
- Parameter validation
- Error handling
- Test: `test/functional/test_digidollar_redeem.py::test_rpc_redeem`

**Task 5**: RPC - Helper Commands
- Files: `src/rpc/digidollar.cpp`
- `getredemptioninfo` - check eligibility
- `listredeemablepositions` - list ready positions
- Test: `test/functional/test_digidollar_redeem.py::test_rpc_info`

**Agent 2: State Management**

**Task 6**: DD Burning & Position Closure
- Files: `src/wallet/digidollarwallet.cpp`, `src/wallet/walletdb.cpp`
- Remove burned DD from UTXOs
- Close/update collateral positions
- Database persistence
- Test: `src/test/digidollar_wallet_tests.cpp::test_burning`

**Agent 3: Testing Foundation**

**Task 7**: Add 1-Hour Test Lock + Fix Existing Tests
- Files: `src/consensus/digidollar.cpp`, `test/functional/*.py`
- Add `{"1hour", 240}` lock period
- Fix any broken digidollar tests
- Ensure all existing tests pass
- Test: Run entire `test/functional/test_digidollar_*.py` suite

---

### Phase 3: GUI (Parallel - 2 agents)

**Agent 1: Vault Tab**

**Task 8**: Vault Tab Position Display
- Files: `src/qt/digidollarvaultwidget.cpp/h`
- Display all collateral positions
- Show status (locked/redeemable/redeemed)
- Show countdown until redeemable
- Redeem button (enabled when redeemable)
- Test: Manual Qt testing

**Agent 2: Redeem Dialog**

**Task 9**: Redemption Dialog
- Files: `src/qt/digidollarredeemdialog.cpp/h` (create)
- Show position details
- Amount inputs (full/partial)
- Confirmation flow
- Error display
- Test: Manual Qt testing

**Final Agent: Integration**

**Task 10**: End-to-End Integration & Testing
- Wire up GUI signals/slots
- Connect RPC to wallet functions
- Run complete test suite
- Fix any integration issues
- Test: `./test-DigiDollar-QT-E2E.sh` (if exists)

## Execution Strategy

### Phase 1 (1 Agent, Sequential)
```
Deploy Agent 1 → Tasks 1, 2, 3
Duration: ~3-4 hours
Wait for completion before Phase 2
```

### Phase 2 (3 Agents, Parallel)
```
Deploy Agent 1 → Tasks 4, 5 (RPC)
Deploy Agent 2 → Task 6 (State)
Deploy Agent 3 → Task 7 (Tests)

Duration: ~2-3 hours
Wait for all 3 to complete
```

### Phase 3 (2 Agents, Parallel)
```
Deploy Agent 1 → Task 8 (Vault)
Deploy Agent 2 → Task 9 (Dialog)

Duration: ~2-3 hours
Then deploy final agent for Task 10
```

## Testing Requirements

### 1-Hour Timelock Setup
```cpp
// src/consensus/digidollar.cpp
namespace DigiDollar {
    static const std::map<std::string, int64_t> LOCK_PERIODS = {
        {"1hour", 240},        // Testing only
        {"30days", 172800},
        // ... existing periods
    };
}
```

### Test Flow
```python
# test/functional/test_digidollar_redeem.py
def test_redemption_flow(self):
    # Mint with 1 hour lock
    result = self.nodes[0].mintdigidollar(1000, "1hour")
    collateral = result['collateral_outpoint']

    # Cannot redeem before timelock
    assert_raises_rpc_error(-8, "timelock",
        self.nodes[0].redeemdigidollar, collateral)

    # Wait for timelock
    self.nodes[0].generate(240)

    # Can redeem now
    redeem = self.nodes[0].redeemdigidollar(collateral)
    assert redeem['success']
    assert redeem['dgb_unlocked'] > 0
```

### ERR Placeholder
```cpp
// src/consensus/err.cpp (placeholder only)
namespace ERR {
    bool IsActive() {
        // TODO: Implement system health monitoring
        return false;  // Always disabled for now
    }

    CAmount CalculateRequirement(CAmount original) {
        // TODO: Implement ERR calculation
        return original;  // Always 1:1 for now
    }
}
```

## Success Criteria

### RPC Must Work
```bash
$ digibyte-cli mintdigidollar 1000 "1hour"
$ digibyte-cli generate 240
$ digibyte-cli listredeemablepositions
[
  {
    "outpoint": "abc:0",
    "dgb_locked": 300000,
    "dd_minted": 1000,
    "status": "redeemable"
  }
]
$ digibyte-cli redeemdigidollar "abc:0"
{
  "txid": "def...",
  "dgb_unlocked": 300000,
  "dd_burned": 1000
}
```

### GUI Must Work
1. Open DigiDollar tab → Vault subtab
2. See position with "Redeemable" status
3. Click "Redeem" button
4. Dialog shows: "Burn 1000 DD to unlock 300,000 DGB"
5. Click "Confirm"
6. Success message appears
7. Position status changes to "Redeemed"

### All Tests Must Pass
```bash
# Unit tests
./src/test/test_digibyte --run_test=digidollar_redeem_*
# All pass ✅

# Functional tests
./test/functional/test_digidollar_redeem.py
# All pass ✅

# Regression tests
./test/functional/test_digidollar_mint.py
./test/functional/test_digidollar_transfer.py
# All still pass ✅
```

## Agent Communication Protocol

### After Each Task
```markdown
## Task X Complete

**Compiled**: ✅/❌
**Tests**: X/Y passed
**Files Modified**: [list]
**Blockers**: [none/describe]
**Ready for**: Next task / Next phase
```

### If Blocked
```markdown
## Task X BLOCKED

**Issue**: [specific problem]
**Error**: [exact error message]
**Need**: [what would unblock]
```

## File Organization

### New Files to Create
- `src/digidollar/redemption.h`
- `src/digidollar/redemption.cpp`
- `src/qt/digidollarredeemdialog.h`
- `src/qt/digidollarredeemdialog.cpp`
- `src/qt/digidollarvaultwidget.h` (if doesn't exist)
- `src/qt/digidollarvaultwidget.cpp` (if doesn't exist)
- `test/functional/test_digidollar_redeem.py` (if doesn't exist)
- `src/test/digidollar_redeem_tests.cpp` (if doesn't exist)

### Files to Modify
- `src/digidollar/txbuilder.cpp/h`
- `src/wallet/digidollarwallet.cpp/h`
- `src/wallet/walletdb.cpp/h`
- `src/rpc/digidollar.cpp`
- `src/qt/digidollartab.cpp` (integrate vault)
- `src/consensus/digidollar.cpp` (add 1hour lock)
- `src/consensus/err.cpp` (placeholder)

## First Actions

1. **Verify** existing code state
2. **Run** existing tests to get baseline
3. **Deploy Agent 1** for Phase 1 (Tasks 1-3)
4. **Monitor** progress and compilation
5. **Deploy Phase 2** agents when ready (3 parallel)
6. **Deploy Phase 3** agents when ready (2 parallel)
7. **Final integration** (Task 10)
8. **Celebrate** complete redemption! 🎉

---

**Version**: 3.0 - Complete Implementation Plan
**Estimated Time**: 8-10 hours total (with parallel execution)
**Agents**: Max 3 concurrent
