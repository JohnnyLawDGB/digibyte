# DigiByte v8.26 Application Bugs Found During Test Fixes

This document tracks actual bugs in the DigiByte application code (not test code) discovered while fixing Python functional tests. These are bugs that would affect production if not fixed.

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

## Critical Bugs (Consensus/Security)

## BUG-001: Missing Absolute Minimum Transaction Fee Enforcement
**File**: src/wallet/spend.cpp, src/validation.cpp, src/txmempool.cpp
**Test**: Group 3 fee tests exposed this vulnerability
**Severity**: Critical (Spam Attack Vector)
**Status**: 🔴 Open

### Issue
DigiByte v8.26 has NO absolute minimum fee enforcement. All fees are calculated as rate-based only (fee = feerate × size ÷ 1000), allowing small transactions to pay far below the intended 0.1 DGB minimum, enabling spam attacks.

### Root Cause
DigiByte intended to have a 0.1 DGB minimum fee per transaction for spam protection, but the implementation only sets fee RATES (per kvB), not absolute minimums. Small transactions exploit this to pay minimal fees.

### Symptoms
- 100-byte transaction pays only 0.01 DGB (10x less than intended)
- 250-byte transaction pays only 0.025 DGB (4x less than intended)  
- Network vulnerable to spam attacks with cheap small transactions
- Fee protection mechanism completely ineffective

### Fix Required
```cpp
// In src/policy/policy.h:59 - Add constant:
static constexpr CAmount ABSOLUTE_MIN_TX_FEE{10000000}; // 0.1 DGB in satoshis

// In src/wallet/spend.cpp:1096 - After fee calculation:
// OLD (vulnerable):
nFeeRet = GetMinimumFeeRate(*wallet, coin_control, &feeCalc).GetFee(nBytes);

// NEW (protected):
nFeeRet = GetMinimumFeeRate(*wallet, coin_control, &feeCalc).GetFee(nBytes);
nFeeRet = std::max(nFeeRet, ABSOLUTE_MIN_TX_FEE); // Enforce 0.1 DGB minimum

// Similar fixes needed in:
// - src/validation.cpp:670 (CheckFeeRate)
// - src/txmempool.cpp:1137 (GetMinFee)
// - src/wallet/fees.cpp:18 (GetMinimumFee)
```

### Impact If Unfixed
- Network can be spammed with thousands of tiny transactions
- Blockchain bloat from cheap spam transactions
- Node resource exhaustion
- Degraded network performance
- Economic attack vector remains open

### Verification
```bash
# Create small transaction and check fee
./digibyte-cli createrawtransaction '[{"txid":"...","vout":0}]' '{"address":0.5}'
# Observe fee is much less than 0.1 DGB

# After fix, same transaction should pay minimum 0.1 DGB
```

### Related Tests
- wallet_bumpfee.py
- wallet_fee_estimation_test.py
- feature_maxuploadtarget.py
- feature_fee_estimator.py

### PR/Commit
[Pending - Critical fix needed]

---

## Medium Priority Bugs (Non-Critical)

## BUG-002: Wallet Test Failures Due to Fee Validation Changes
**File**: Multiple wallet test files, underlying issue in fee validation
**Test**: wallet_fundrawtransaction.py, wallet_send.py, wallet_sendall.py (Group 6), wallet_bumpfee.py, wallet_create_tx.py, wallet_groups.py (Group 5)
**Severity**: Medium (Test Infrastructure)
**Status**: 🔴 Open

### Issue
Bitcoin v26.2 merge introduced stricter fee validation that causes multiple wallet functional tests to fail. The tests create transactions that don't meet DigiByte's fee requirements, causing mempool rejection and transaction broadcast failures.

### Root Cause  
Related to BUG-001 (fee enforcement), the stricter validation from Bitcoin v26.2 exposes the fee calculation inconsistencies in DigiByte. Tests expect transactions to be accepted but they're rejected due to:
1. Fee amounts exceeding `-maxtxfee` limits
2. Transactions not meeting minimum fee requirements
3. `testmempoolaccept` returning `allowed: false` for valid-looking transactions

### Symptoms
- `wallet_fundrawtransaction.py`: testmempoolaccept failures in external inputs test
- `wallet_send.py`: `AssertionError: not(False == True)` at mempool acceptance checks
- `wallet_sendall.py`: JSONRPC errors in transaction creation
- `wallet_fundrawtransaction.py --legacy-wallet`: "Fee exceeds maximum configured by user"
- `wallet_bumpfee.py`: "min relay fee not met" (4380 sats < 177000 sats required), transactions not mining
- `wallet_create_tx.py`: "Fee exceeds maximum configured by user" even with high maxtxfee
- `wallet_groups.py`: Balance expectations fail (4.29 DGB vs 215 DGB expected) due to fee consumption

### Fix Required
```cpp
// Two-pronged approach needed:
// 1. Fix underlying fee calculation in BUG-001
// 2. Update test expectations for DigiByte fee structure

// In tests, may need to:
// - Increase -maxtxfee from 10.0 to higher value
// - Adjust expected fee amounts in assertions
// - Account for DigiByte's 100x fee multiplier in test logic
```

### Impact If Unfixed
- Wallet functional tests remain broken
- CI/CD pipeline issues
- Reduced confidence in wallet functionality
- Difficulty detecting real wallet bugs

### Verification
```bash
# All these should pass:
./test/functional/wallet_fundrawtransaction.py --descriptors
./test/functional/wallet_fundrawtransaction.py --legacy-wallet  
./test/functional/wallet_send.py --descriptors
./test/functional/wallet_send.py --legacy-wallet
./test/functional/wallet_sendall.py --descriptors
./test/functional/wallet_sendall.py --legacy-wallet
```

### Related Tests
- All Group 6 wallet send operation tests
- Potentially other wallet tests with fee calculations

### PR/Commit
[Requires fix of BUG-001 first, then test adaptation]

---

## Low Priority Bugs (Cosmetic/Minor)

[Bugs that are minor annoyances or cosmetic issues]

---

## Fixed Bugs Archive

[Move bugs here once fixed and verified in master branch]

---

## Statistics

### Summary (as of 2025-08-26)
- 🔴 **Open**: 2 bugs (1 Critical fee vulnerability, 1 Medium test infrastructure)
- 🟡 **In Progress**: 0 bugs  
- 🟢 **Fixed**: 0 bugs
- **Total Found**: 2 bugs

### By Severity
- **Critical**: 1 (consensus/security - fee enforcement)
- **High**: 0 (major functionality)
- **Medium**: 1 (test infrastructure - wallet tests)
- **Low**: 0 (cosmetic)

### By Component
- **Consensus**: 0 bugs
- **Wallet**: 2 bugs (fee calculation + test failures)
- **P2P**: 0 bugs
- **RPC**: 0 bugs
- **Mining**: 0 bugs

---

## Quick Reference

### Most Common Bug Locations
1. `src/policy/fees.cpp` - Fee calculation issues
2. `src/validation.cpp` - Block validation logic
3. `src/wallet/wallet.cpp` - Transaction creation
4. `src/rpc/mining.cpp` - Mining RPC commands
5. `src/net_processing.cpp` - P2P message handling

### Debugging Commands
```bash
# Check for Bitcoin-specific constants that need updating:
grep -r "50.*BTC\|600.*seconds\|100.*blocks" src/

# Find where DigiByte-specific code was lost:
diff -r digibyte-v8.22.2/src/ src/ | grep "^<" | grep -i "digibyte\|dgb"

# Test specific bug fix:
./test/functional/[test_that_exposed_bug].py --loglevel=debug
```

---

*This document is critical for production stability. Every bug here represents a real issue that affects users. Update immediately when bugs are found or fixed.*