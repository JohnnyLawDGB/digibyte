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

## BUG-001: [EXAMPLE] Incorrect Scrypt PoW Validation
**File**: src/pow.cpp:47
**Test**: feature_block.py
**Severity**: Critical
**Status**: 🔴 Open

### Issue
PoW validation for Scrypt algorithm (algo 1) incorrectly uses SHA256 validation logic after v26.2 merge.

### Root Cause
Bitcoin v26.2 refactored PoW validation assuming single algorithm. DigiByte's multi-algo logic was not properly integrated.

### Symptoms
- Scrypt blocks rejected as invalid
- Mining on algo 1 produces orphans
- Chain splits possible between nodes

### Fix Applied
```cpp
// OLD (broken):
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params) {
    return CheckProofOfWorkSHA256(hash, nBits, params);
}

// NEW (fixed):
bool CheckProofOfWork(uint256 hash, unsigned int nBits, int nAlgo, const Consensus::Params& params) {
    switch(nAlgo) {
        case ALGO_SHA256D: return CheckProofOfWorkSHA256(hash, nBits, params);
        case ALGO_SCRYPT: return CheckProofOfWorkScrypt(hash, nBits, params);
        // ... other algos
    }
}
```

### Impact If Unfixed
- Network cannot process Scrypt blocks
- 20% of mining power excluded
- Potential chain split

### Verification
```bash
./test/functional/feature_block.py
./digibyte-cli generatetoaddress 1 [address] 1  # Force Scrypt
```

### Related Tests
- feature_block.py
- mining_basic.py
- p2p_compactblocks.py

### PR/Commit
[Pending]

---

## High Priority Bugs (Functionality)

## BUG-002: [EXAMPLE] Incorrect Fee Calculation in KvB
**File**: src/policy/fees.cpp:124  
**Test**: wallet_fundrawtransaction.py
**Severity**: High
**Status**: 🔴 Open

### Issue
Fee calculation uses vBytes (Bitcoin) instead of KiloBytes (DigiByte), causing 1000x fee miscalculation.

### Root Cause
v26.2 changed fee units to vBytes for SegWit. DigiByte should maintain KvB for compatibility.

### Symptoms
- "max-fee-exceeded" errors on normal transactions
- Wallet cannot create transactions with automatic fees
- Fee estimation off by factor of 1000

### Fix Applied
```cpp
// OLD (broken):
CAmount GetRequiredFee(unsigned int nTxBytes) {
    return nTxBytes * DEFAULT_TRANSACTION_MINFEE;  // Treats as vBytes
}

// NEW (fixed):  
CAmount GetRequiredFee(unsigned int nTxBytes) {
    return (nTxBytes * DEFAULT_TRANSACTION_MINFEE) / 1000;  // Convert to KvB
}
```

### Impact If Unfixed
- Users cannot send transactions with automatic fees
- Manual fee entry required for all transactions
- Third-party wallets break

### Verification
```bash
./test/functional/wallet_fundrawtransaction.py
./digibyte-cli fundrawtransaction [hex]
```

### Related Tests
- wallet_fundrawtransaction.py
- wallet_bumpfee.py
- feature_fee_estimation.py

### PR/Commit
[Pending]

---

## Medium Priority Bugs (Non-Critical)

[Bugs that affect functionality but don't break consensus]

---

## Low Priority Bugs (Cosmetic/Minor)

[Bugs that are minor annoyances or cosmetic issues]

---

## Fixed Bugs Archive

[Move bugs here once fixed and verified in master branch]

---

## Statistics

### Summary (as of 2025-08-24)
- 🔴 **Open**: 0 bugs (Examples provided above)
- 🟡 **In Progress**: 0 bugs  
- 🟢 **Fixed**: 0 bugs
- **Total Found**: 0 bugs (to be populated by sub-agents)

### By Severity
- **Critical**: X (consensus/security)
- **High**: Y (major functionality)
- **Medium**: Z (minor functionality)
- **Low**: W (cosmetic)

### By Component
- **Consensus**: X bugs
- **Wallet**: Y bugs
- **P2P**: Z bugs
- **RPC**: W bugs
- **Mining**: V bugs

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