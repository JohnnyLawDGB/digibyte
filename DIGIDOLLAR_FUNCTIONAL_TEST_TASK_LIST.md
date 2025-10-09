# DigiDollar Functional Test Task List

**Generated**: 2025-10-09
**Objective**: Fix failing functional tests to achieve 100% pass rate for DigiDollar tests
**Strategy**: Deploy sub-agents to fix tests in groups based on failure type

---

## ⚠️ CRITICAL CONTEXT - READ FIRST

### STEP 0: Architecture Documents - MANDATORY READING

**BEFORE ANY AGENT STARTS WORK**, they MUST read these architecture documents:

1. **`/home/jared/Code/digibyte/DIGIDOLLAR_EXPLAINER.md`**
   - Complete overview of the DigiDollar stablecoin system
   - How minting, redemption, and collateral management work
   - System architecture and design philosophy
   - User-facing perspective of system behavior

2. **`/home/jared/Code/digibyte/DIGIDOLLAR_ARCHITECTURE.md`**
   - Technical implementation details and code structure
   - Complete RPC command reference with expected responses
   - Data structures, validation rules, and state management
   - Testing methodology and patterns

**Why This Matters**:
- Agents are fixing tests that verify DigiDollar behavior
- Must understand HOW the system is supposed to work
- Need to distinguish test bugs from application bugs
- Architecture explains WHY things work the way they do
- Without this context, agents will make incorrect fixes

**Orchestrator**: Include links to both documents in every agent's prompt. Verify agents read them before starting work.

---

### Oracle System Status

**IMPORTANT**: The DigiDollar oracle system has **NOT been implemented yet**. All tests should use a **mock hardcoded oracle price**.

**Mock Oracle Price**: `0.01 USD per DGB` (1 cent per DGB)
- This is equivalent to `1` in the cents representation
- Tests should call `setmockoracleprice(1)` to set 1 cent per DGB
- Some older tests may use `setmockoracleprice(50000)` which represents $0.50 per DGB in satoshis

**What This Means for Agents**:
- ❌ **DO NOT** implement real oracle functionality
- ❌ **DO NOT** try to fetch real prices
- ❌ **DO NOT** modify oracle-related code
- ✅ **DO** use mock prices in tests
- ✅ **DO** accept that oracle tests may test future functionality

### Working Functionality - DO NOT BREAK

**The following DigiDollar features are currently WORKING**:
1. ✅ **Minting** - Creating new DigiDollars with collateral
2. ✅ **Sending/Transferring** - Moving DD between addresses
3. ✅ **Receiving** - Accepting DD transfers
4. ✅ **Redemption** - Burning DD and unlocking collateral (basic form)

**CRITICAL RULES FOR ALL AGENTS**:
- ✅ **FIX APPLICATION BUGS** - Tests should expose real bugs! Fix them when found!
- ⚠️ **TEST EVERYTHING** - After EVERY change, run full regression tests
- 🛡️ **PROTECT WORKING FUNCTIONALITY** - Don't break minting, sending, or receiving
- 📝 **DOCUMENT CHANGES** - Explain what bug you fixed and how you tested it
- 🔄 **INCREMENTAL FIXES** - Fix one thing at a time, test after each fix

**What This Means**:
- If a test fails because the implementation has a bug → **FIX THE BUG**
- If a test fails because the test is wrong → **FIX THE TEST**
- If you're not sure which it is → **INVESTIGATE, then fix the right thing**
- If your fix might break something → **TEST THOROUGHLY before moving on**

### How to Verify You Haven't Broken Things

After EVERY fix, run these passing tests to ensure no regression:
```bash
# These must continue to pass:
./test/functional/digidollar_transfer.py        # Tests sending/receiving
./test/functional/digidollar_redeem_stats.py    # Tests redemption
./test/functional/digidollar_redemption_amounts.py  # Tests collateral return
```

If ANY of these fail after your changes, **REVERT IMMEDIATELY** and report the issue.

---

## 🎯 Current Status - GOOD PROGRESS

### Test Suite Health

**Total DigiDollar Functional Tests**: 18 test scripts
**Currently Passing**: 6 tests (33%)
**Currently Failing**: 12 tests (67%)
**Target**: 18/18 passing (100% pass rate)

### Summary

Out of 18 DigiDollar functional tests, 6 are passing and 12 are failing. The failures fall into clear categories with straightforward fixes.

---

## Test Results Summary

### ✅ PASSING TESTS (6)

| Test Name | Status | Notes |
|-----------|--------|-------|
| `digidollar_activation.py` | ✅ PASS | BIP9 activation tests working |
| `digidollar_network_tracking.py` | ⏳ LONG-RUNNING | Appears to work but waits 60s for Dandelion |
| `digidollar_redeem_stats.py` | ✅ PASS | Network stats and redemption working |
| `digidollar_redemption_amounts.py` | ✅ PASS | Collateral return verification working |
| `digidollar_transfer.py` | ✅ PASS | DD transfer operations working |
| `digidollar_tx_amounts_debug.py` | ✅ PASS | Transaction amount verification working |

### ❌ FAILING TESTS (12)

#### Group 1: Import Errors (3 tests) - EASY FIX

| Test Name | Error | Fix Required |
|-----------|-------|--------------|
| `digidollar_basic.py` | `ImportError: cannot import name 'connect_nodes'` | Remove import, use `self.connect_nodes()` instead |
| `digidollar_protection.py` | `ImportError: cannot import name 'assert_in'` | Remove import or define `assert_in` helper |
| `digidollar_wallet.py` | `ImportError: cannot import name 'assert_in'` | Remove import or define `assert_in` helper |

**Root Cause**: Tests are importing functions that don't exist as standalone imports. `connect_nodes` is a method of the test class, and `assert_in` doesn't exist in test_framework.util.

**Fix Pattern**:
- For `connect_nodes`: Remove from imports, call as `self.connect_nodes(node_a, node_b)`
- For `assert_in`: Either define locally or replace with standard Python `assert x in y`

---

#### Group 2: createwallet RPC Error (8 tests) - WALLET INITIALIZATION

| Test Name | Error | Line |
|-----------|-------|------|
| `digidollar_mint.py` | `Method not found (-32601)` on `createwallet` | test_framework.py:439 |
| `digidollar_network_relay.py` | `Method not found (-32601)` on `createwallet` | test_framework.py:439 |
| `digidollar_oracle.py` | `Method not found (-32601)` on `createwallet` | test_framework.py:439 |
| `digidollar_persistence.py` | `Method not found (-32601)` on `createwallet` | test_framework.py:439 |
| `digidollar_redeem.py` | `Method not found (-32601)` on `createwallet` | test_framework.py:439 |
| `digidollar_redemption_e2e.py` | `Method not found (-32601)` on `createwallet` | test_framework.py:439 |
| `digidollar_transactions.py` | `Method not found (-32601)` on `createwallet` | test_framework.py:439 |

**Root Cause**: The tests are trying to use wallet functionality, but wallets may not be enabled or the `createwallet` RPC is not available in the node configuration.

**Possible Fixes**:
1. Tests need to check `skip_if_no_wallet()` before running
2. Tests may need different node startup flags
3. DigiDollar node build might not include wallet support
4. Tests may need to use existing default wallet instead of creating new one

**Investigation Needed**:
- Check if digibyted was built with wallet support (`./src/digibyted --help | grep wallet`)
- Check if tests should use `self.skip_if_no_wallet()`
- Look at passing tests to see how they handle wallets

---

#### Group 3: Test Logic Errors (2 tests) - TEST CODE BUGS

| Test Name | Error | Fix Required |
|-----------|-------|--------------|
| `digidollar_rpc.py` | `AssertionError: Missing stats section: supply` | RPC response changed format, need to update test expectations |
| `digidollar_stress.py` | `AttributeError: connect_nodes_bi doesn't exist` | Use `self.connect_nodes()` instead |

**Root Cause**:
- `digidollar_rpc.py`: Test expects stats dict to have key `supply` but actual response uses `total_dd_supply`
- `digidollar_stress.py`: Trying to call `connect_nodes_bi` which doesn't exist (should be `connect_nodes`)

---

## Task Assignment Strategy

### Phase 1: Quick Wins - Import Errors (Agent 1)

| Agent ID | Tests to Fix | Estimated Time |
|----------|--------------|----------------|
| Agent-1 | digidollar_basic.py, digidollar_protection.py, digidollar_wallet.py | 15-20 minutes |

**Tasks**:
1. Fix `connect_nodes` import error in digidollar_basic.py
2. Fix `assert_in` import errors in digidollar_protection.py and digidollar_wallet.py
3. Run each test to verify fix

---

### Phase 2: Wallet Investigation & Fix (Agent 2)

| Agent ID | Investigation | Estimated Time |
|----------|---------------|----------------|
| Agent-2 | Investigate wallet support and fix createwallet errors | 30-45 minutes |

**Tasks**:
1. Determine if digibyted has wallet support
2. Check how passing tests handle wallets (look at digidollar_transfer.py)
3. Apply consistent wallet initialization pattern to all 8 failing tests
4. Verify each test runs successfully

---

### Phase 3: Test Logic Fixes (Agent 3)

| Agent ID | Tests to Fix | Estimated Time |
|----------|--------------|----------------|
| Agent-3 | digidollar_rpc.py, digidollar_stress.py | 15-20 minutes |

**Tasks**:
1. Fix digidollar_rpc.py stats key expectations
2. Fix digidollar_stress.py connect_nodes_bi call
3. Run tests to verify

---

## Detailed Fix Instructions

### Fix #1: connect_nodes Import Error

**Files**: `digidollar_basic.py`

**Current Code**:
```python
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    connect_nodes,  # ← WRONG
)
```

**Fixed Code**:
```python
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)
```

**In test_multi_node_sync() method**:
```python
# Current:
connect_nodes(self.nodes[0], self.nodes[1])  # ← WRONG

# Fixed:
self.connect_nodes(0, 1)  # ← CORRECT
```

---

### Fix #2: assert_in Import Error

**Files**: `digidollar_protection.py`, `digidollar_wallet.py`

**Option A - Define Helper**:
```python
def assert_in(item, collection, msg=""):
    assert item in collection, msg or f"{item} not in {collection}"
```

**Option B - Use Standard Python**:
```python
# Current:
assert_in('key', my_dict)

# Fixed:
assert 'key' in my_dict, "'key' should be in my_dict"
```

---

### Fix #3: createwallet RPC Error

**Investigation Steps**:
1. Check if wallet is enabled:
   ```bash
   ./src/digibyted --help | grep -i wallet
   ```

2. Look at passing test for pattern:
   ```bash
   grep -A10 "skip_if_no_wallet\|createwallet" test/functional/digidollar_transfer.py
   ```

3. Common fixes:
   - Add `self.skip_if_no_wallet()` to `skip_test_if_missing_module()`
   - Don't call `createwallet`, use default wallet
   - Add wallet-related startup args

**Pattern from Passing Tests**:
Check `digidollar_transfer.py` to see how it initializes wallets without errors.

---

### Fix #4: digidollar_rpc.py Stats Keys

**File**: `test/functional/digidollar_rpc.py`

**Current Code** (around line 151):
```python
# Test expects these keys:
expected_sections = ['supply', 'collateral', 'health', ...]
for section in expected_sections:
    assert section in stats, f"Missing stats section: {section}"
```

**Actual RPC Response**:
```python
{
    'total_dd_supply': 0,
    'total_collateral_dgb': Decimal('0E-8'),
    'health_percentage': 30000,
    ...
}
```

**Fix**:
```python
# Update expected keys to match actual response:
expected_keys = ['total_dd_supply', 'total_collateral_dgb', 'health_percentage',
                 'health_status', 'oracle_price_cents', 'is_emergency', ...]
for key in expected_keys:
    assert key in stats, f"Missing stats key: {key}"
```

---

### Fix #5: connect_nodes_bi Error

**File**: `test/functional/digidollar_stress.py`

**Current Code** (line 34):
```python
self.connect_nodes_bi(0, 1)  # ← Method doesn't exist
```

**Fixed Code**:
```python
self.connect_nodes(0, 1)  # ← Use standard method
```

---

## Verification Checklist

### After Each Fix

- [ ] Test runs without import/syntax errors
- [ ] Test initializes nodes successfully
- [ ] Test completes (pass or meaningful failure)
- [ ] No regressions in other tests

### Final Verification

- [ ] All 18 DigiDollar tests passing
- [ ] No wallet-related errors
- [ ] No import errors
- [ ] Tests complete in reasonable time

---

## Files to Modify

### Import Error Fixes
- `/home/jared/Code/digibyte/test/functional/digidollar_basic.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_protection.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_wallet.py`

### Wallet Initialization Fixes
- `/home/jared/Code/digibyte/test/functional/digidollar_mint.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_network_relay.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_oracle.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_persistence.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_redeem.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_redemption_e2e.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_transactions.py`

### Test Logic Fixes
- `/home/jared/Code/digibyte/test/functional/digidollar_rpc.py`
- `/home/jared/Code/digibyte/test/functional/digidollar_stress.py`

---

## Success Criteria

### Definition of Done ✅

1. ✅ All import errors resolved
2. ✅ All wallet initialization errors resolved
3. ✅ All test logic errors fixed
4. ✅ All 18 DigiDollar functional tests passing
5. ✅ Tests complete in reasonable time (< 5 minutes each)
6. ✅ No regressions in previously passing tests

---

## Timeline Estimate

| Phase | Agent | Tasks | Time |
|-------|-------|-------|------|
| Phase 1 | Agent-1 | Fix 3 import errors | 15-20 min |
| Phase 2 | Agent-2 | Investigate & fix 8 wallet errors | 30-45 min |
| Phase 3 | Agent-3 | Fix 2 test logic errors | 15-20 min |
| **TOTAL** | | **Fix all 12 failing tests** | **60-85 minutes** |

---

## Notes

- **Good Foundation**: 6 tests already passing shows DigiDollar core functionality works
- **Clear Patterns**: Failures fall into distinct categories with clear fixes
- **Low Risk**: Most fixes are test code only, not touching DigiDollar implementation
- **Quick Wins Available**: Import errors can be fixed in minutes
- **Main Challenge**: Understanding wallet initialization pattern

---

## Agent Progress Log

| Timestamp | Phase | Agent | Activity | Status | Tests Fixed |
|-----------|-------|-------|----------|--------|-------------|
| 2025-10-09 | Analysis | - | Analyzed 18 tests, identified failures | ✅ Complete | 0/12 |
| - | Phase 1 | Agent-1 | Fix import errors | ⏳ Pending | 0/3 |
| - | Phase 2 | Agent-2 | Fix wallet initialization | ⏳ Pending | 0/8 |
| - | Phase 3 | Agent-3 | Fix test logic | ⏳ Pending | 0/2 |

**Legend:**
- ⏳ Pending
- 🔄 In Progress
- ✅ Complete
- ❌ Failed

---

**Generated:** 2025-10-09
**Status:** Ready for agent deployment
**Objective:** Achieve 100% functional test pass rate (18/18)
**Current:** 6/18 passing (33%)
**Target:** 18/18 passing (100%)
