# DigiByte v8.26 Application Bugs Found During Test Fixes

This document tracks actual bugs in the DigiByte application code (not test code) discovered while fixing Python functional tests. These are bugs that would affect production if not fixed.

## BUG-006: RBF Transaction Replacement Not Working Without Explicit Signaling
**File**: src/validation.cpp:771-772
**Test**: wallet_resendwallettransactions.py (--descriptors variant)
**Severity**: Medium
**Status**: 🔴 Open (Test Fixed with Workaround)

### Issue
Transaction replacement fails with "txn-mempool-conflict" error when the original transaction doesn't signal RBF, even if the replacement has higher fees. This prevents fee bumping of transactions that don't explicitly signal RBF.

### Root Cause
The validation logic at line 771-772 immediately rejects replacement attempts if the conflicting transaction doesn't signal RBF (`!SignalsOptInRBF(*ptxConflicting)`), returning "txn-mempool-conflict" without ever reaching the fee comparison checks in `ReplacementChecks()`.

### Evidence
```cpp
// src/validation.cpp:771-772
if (!m_pool.m_full_rbf && !SignalsOptInRBF(*ptxConflicting)) {
    return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY, "txn-mempool-conflict");
}
```
This prevents `ws.m_conflicts.insert()` from being called, so `m_rbf` remains false and `ReplacementChecks()` is never invoked.

### Fix Applied
**Test Workaround**: Modified test to ensure all transactions signal RBF (sequence=0xfffffffd)
**Application Fix**: NOT FIXED - Requires decision on whether to enable full RBF by default or modify RBF policy

### Impact
Users cannot bump fees on transactions that don't signal RBF, even if wallet supports it. This affects fee bumping functionality in descriptors wallets particularly.

## Bug Tracking Format

Each bug should be documented using this template:
```markdown
## BUG-[NUMBER]: [Short Description]
**File**: src/[filename].cpp:[line]
**Test**: [test_name.py] that exposed this bug
**Severity**: Critical | High | Medium | Low
**Status**: 🔴 Open | 🟡 In Progress | 🟢 Fixed

### Issue
[Clear description of what's broken]

### Root Cause  
[Why the Bitcoin v26.2 merge broke this]

### Symptoms
- [User-visible symptom 1]
- [User-visible symptom 2]

### Fix Applied
\```cpp
// OLD (broken):
[code snippet]

// NEW (fixed):
[code snippet]
\```

### Impact If Unfixed
[What happens in production without this fix]

### Verification
[How to test this fix works]

### Related Tests
- [test1.py]
- [test2.py]

### PR/Commit
[Link to PR or commit hash]
```

---

*This document is critical for production stability. Every bug here represents a real issue that affects users. Update immediately when bugs are found or fixed.*