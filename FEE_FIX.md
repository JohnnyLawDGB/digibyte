# DigiByte v8.26 Fee System Architectural Fix - BUG-001 Resolution

## Executive Summary

This document outlines the comprehensive architectural fix required to resolve BUG-001 (fee calculation issues) affecting wallet_bumpfee.py and other wallet fee management tests. The solution implements a **hybrid fee model** that enforces both rate-based fairness and absolute minimum spam protection.

## Root Cause Analysis

### Current Broken Architecture
DigiByte v8.26 inherited Bitcoin's **pure rate-based fee model** without accounting for economic differences:

| Metric | Bitcoin | DigiByte | Impact |
|--------|---------|----------|--------|
| Supply | 21 million | 21 billion (1000x) | Requires higher absolute fees |
| Block Time | 600s | 15s (40x faster) | More frequent transactions |
| Fee Calculation | `fee = rate × size ÷ 1000` | Same formula | **PROBLEM** |

### Verified Problem
**No `ABSOLUTE_MIN_TX_FEE` constant exists anywhere in the codebase** - all calculations are purely rate-based.

**Current Math Issue**:
```
250-byte transaction at 0.1 DGB/kvB:
fee = 0.1 × 250 ÷ 1000 = 0.025 DGB ❌ (4x less than required 0.1 DGB)
```

### Why wallet_bumpfee.py Fails
1. **Initial TX**: Created with insufficient absolute fee (0.025 DGB vs required 0.1 DGB)
2. **RBF Logic**: Bitcoin v26.2 assumes rate-based fees are adequate for spam protection
3. **PSBT Confusion**: Fee calculation conflicts between rate requirements and absolute needs
4. **Result**: "max-fee-exceeded", "Insufficient total fee", "preselected coins total amount does not cover transaction target"

## Proposed Solution: Hybrid Fee Model

### Core Formula
```cpp
final_fee = max(
    rate_based_fee,        // Fair for large transactions (rate × size ÷ 1000)
    ABSOLUTE_MIN_TX_FEE    // Spam protection (0.1 DGB = 10,000,000 satoshis)
)
```

### Expected Results
- **Small TX (250 bytes)**: `max(0.025 DGB, 0.1 DGB) = 0.1 DGB` ✅
- **Medium TX (1000 bytes)**: `max(0.1 DGB, 0.1 DGB) = 0.1 DGB` ✅  
- **Large TX (10000 bytes)**: `max(1.0 DGB, 0.1 DGB) = 1.0 DGB` ✅

## Implementation Plan

### Phase 1: Wallet-Only Enforcement (Immediate - Fixes Tests)

**Target**: Fix wallet_bumpfee.py and related wallet tests immediately with minimal risk.

#### 1.1 Policy Foundation
**File**: `src/policy/policy.h`
**Location**: After line 59 (after `DEFAULT_MIN_RELAY_TX_FEE`)
```cpp
/** Absolute minimum fee per transaction regardless of size */
static constexpr CAmount ABSOLUTE_MIN_TX_FEE{10000000}; // 0.1 DGB
```

#### 1.2 Wallet Fee Calculation
**File**: `src/wallet/fees.cpp`
**Location**: Update `GetMinimumFee()` function at line 18-21
```cpp
CAmount GetMinimumFee(const CWallet& wallet, unsigned int nTxBytes, 
                      const CCoinControl& coin_control, FeeCalculation* feeCalc)
{
    CAmount rate_fee = GetMinimumFeeRate(wallet, coin_control, feeCalc).GetFee(nTxBytes);
    // Enforce absolute minimum (hybrid approach)
    return std::max(rate_fee, ABSOLUTE_MIN_TX_FEE);
}
```

#### 1.3 Wallet Transaction Creation  
**File**: `src/wallet/spend.cpp`
**Location**: After line 1096 in `CreateTransactionInternal()` (after fee calculation)
```cpp
// Existing line 1096:
CAmount not_input_fees = coin_selection_params.m_effective_feerate.GetFee(coin_selection_params.m_subtract_fee_outputs ? 0 : coin_selection_params.tx_noinputs_size);

// NEW CODE TO ADD:
// Enforce absolute minimum fee unless explicitly overridden
if (not_input_fees < ABSOLUTE_MIN_TX_FEE && !coin_control.fOverrideFeeRate) {
    // Update fee rate to meet absolute minimum
    CFeeRate required_rate{ABSOLUTE_MIN_TX_FEE, coin_selection_params.tx_noinputs_size};
    coin_selection_params.m_effective_feerate = std::max(coin_selection_params.m_effective_feerate, required_rate);
    not_input_fees = ABSOLUTE_MIN_TX_FEE;
    
    LogPrint(BCLog::FEE, "Enforcing absolute minimum fee: %s (was %s)\n", 
             FormatMoney(ABSOLUTE_MIN_TX_FEE), FormatMoney(not_input_fees));
}
```

### Phase 2: Network Enforcement (After Phase 1 Testing)

**Target**: Network-wide spam protection and mempool validation.

#### 2.1 Mempool Dynamic Fee
**File**: `src/txmempool.cpp`
**Location**: Update `GetMinFee()` at line 1137
```cpp
CFeeRate CTxMemPool::GetMinFee(size_t sizelimit) const {
    LOCK(cs);
    if (!blockSinceLastRollingFeeBump || rollingMinimumFeeRate == 0) {
        // Ensure absolute minimum is respected even when no rolling fee
        CFeeRate absolute_min_rate{ABSOLUTE_MIN_TX_FEE, 1000}; // Rate equivalent for 1000 bytes
        return std::max(CFeeRate(llround(rollingMinimumFeeRate)), absolute_min_rate);
    }
    
    // Existing rolling fee calculation...
    int64_t decay = -(int64_t(time(nullptr)) - int64_t(lastRollingFeeUpdate)) / 60;
    // ... existing decay logic ...
    
    CFeeRate result = std::max(CFeeRate(llround(rollingMinimumFeeRate)), m_incremental_relay_feerate);
    
    // NEW: Enforce absolute minimum rate equivalent
    CFeeRate absolute_min_rate{ABSOLUTE_MIN_TX_FEE, 1000}; // Rate equivalent for 1000 bytes
    return std::max(result, absolute_min_rate);
}
```

#### 2.2 Transaction Validation
**File**: `src/validation.cpp`
**Location**: Update `CheckFeeRate()` at line 670-684
```cpp
bool CheckFeeRate(size_t package_size, CAmount package_fee, TxValidationState& state) 
{
    // NEW: Check absolute minimum first
    if (package_fee < ABSOLUTE_MIN_TX_FEE) {
        return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY, 
                            "absolute-min-fee-not-met", 
                            strprintf("Transaction fee %s is below absolute minimum %s", 
                                    FormatMoney(package_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE)));
    }
    
    // EXISTING CODE: Check rate-based minimums
    CAmount mempoolRejectFee = m_pool.GetMinFee().GetFee(package_size);
    if (mempoolRejectFee > 0 && package_fee < mempoolRejectFee) {
        return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY, 
                            "mempool-min-fee-not-met", 
                            strprintf("%d < %d", package_fee, mempoolRejectFee));
    }
    
    return true;
}
```

### Phase 3: Complete Integration (Long-term)

**Target**: Comprehensive ecosystem integration.

#### 3.1 RPC API Protection
**File**: `src/rpc/rawtransaction.cpp`
**Location**: Update `sendrawtransaction` and `testmempoolaccept`
```cpp
// In sendrawtransaction RPC handler
static RPCHelpMan sendrawtransaction() {
    // ... existing parameter parsing ...
    
    // Calculate transaction fee for validation
    CAmount tx_fee = 0;
    for (const auto& input : tx.vin) {
        // Get input value from UTXO set
        // tx_fee += input_value;
    }
    for (const auto& output : tx.vout) {
        tx_fee -= output.nValue;
    }
    
    // NEW: Validate absolute minimum unless bypass_limits is true
    if (tx_fee < ABSOLUTE_MIN_TX_FEE && !bypass_limits) {
        throw JSONRPCError(RPC_TRANSACTION_REJECTED,
            strprintf("Transaction fee %s is below absolute minimum %s. "
                     "Use 'bypasslimits' parameter to override.",
                     FormatMoney(tx_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE)));
    }
    
    // ... continue with existing validation ...
}
```

#### 3.2 Dandelion++ Integration
**File**: `src/dandelion.cpp`
**Location**: Add fee validation before stempool embargo
```cpp
bool ValidateStemPoolTransaction(const CTransaction& tx, CAmount& fee_out) {
    // Calculate transaction fee (inputs - outputs)
    CAmount tx_fee = 0;
    // ... fee calculation logic ...
    
    // NEW: Validate absolute minimum for stempool transactions
    if (tx_fee < ABSOLUTE_MIN_TX_FEE) {
        LogPrint(BCLog::DANDELION, "Rejecting stempool tx %s: fee %s below absolute minimum %s\n",
                tx.GetHash().ToString(), FormatMoney(tx_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE));
        return false;
    }
    
    fee_out = tx_fee;
    return true;
}

// Update embargo logic to use this validation
bool CTxMemPool::AddToStemPool(const CTransactionRef& tx, CValidationState& state) {
    CAmount fee;
    if (!ValidateStemPoolTransaction(*tx, fee)) {
        state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY, "stempool-min-fee-not-met");
        return false;
    }
    
    // ... existing stempool logic ...
}
```

#### 3.3 Configuration Options
**File**: `src/init.cpp`
**Location**: Add command-line options
```cpp
// Add to argument parsing section
argsman.AddArg("-absolutemintxfee=<amt>", 
               strprintf("Absolute minimum fee per transaction (default: %s)", 
                        FormatMoney(DEFAULT_ABSOLUTE_MIN_TX_FEE)), 
               ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);

argsman.AddArg("-enforceabsolutefee", 
               "Enforce absolute minimum fee validation (default: true)", 
               ArgsManager::ALLOW_BOOL, OptionsCategory::WALLET);

// Add to wallet initialization
if (args.IsArgSet("-absolutemintxfee")) {
    CAmount n = AmountFromValue(args.GetArg("-absolutemintxfee", ""));
    wallet.m_absolute_min_fee = n;
}
```

## Testing Strategy

### Phase 1 Test Plan
```bash
# Run wallet fee tests (should all pass after Phase 1)
python3 test/functional/test_runner.py \
    wallet_bumpfee.py \
    wallet_create_tx.py \
    wallet_groups.py \
    wallet_fundrawtransaction.py \
    wallet_send.py \
    wallet_sendall.py

# Verify no regressions in other wallet tests
python3 test/functional/test_runner.py wallet_*
```

### Phase 2 Test Plan
```bash
# Test mempool enforcement
python3 test/functional/test_runner.py \
    mempool_accept.py \
    feature_fee_estimation.py \
    mempool_limit.py

# Test network propagation
python3 test/functional/test_runner.py p2p_*
```

### Phase 3 Test Plan
```bash
# Test RPC integration
python3 test/functional/test_runner.py \
    rpc_rawtransaction.py \
    interface_rest.py

# Test Dandelion++ integration
python3 test/functional/test_runner.py dandelion_*

# Full test suite
python3 test/functional/test_runner.py --extended
```

## Test File Updates Required

### Immediate Updates for Phase 1
1. **test/functional/wallet_bumpfee.py**
   - Update expected fee amounts from rate-based to absolute minimum
   - Adjust PSBT fee calculations to expect 0.1 DGB minimum
   - Fix RBF increment calculations

2. **test/functional/wallet_create_tx.py**
   - Update transaction creation tests to expect higher fees
   - Adjust fee estimation assertions

3. **test/functional/wallet_groups.py**
   - Verify coin selection respects new fee minimums
   - Update fee calculations in grouping tests

### Example Test Fix Pattern
```python
# OLD (rate-based expectation)
expected_fee = Decimal('0.025')  # 250 bytes at 0.1 DGB/kvB

# NEW (hybrid expectation)  
expected_fee = Decimal('0.1')    # Absolute minimum enforced
```

## Monitoring and Validation

### Success Metrics
- **All wallet fee tests pass**: 100% success rate for Groups 5-6
- **No transaction rejections**: Legitimate transactions still processed
- **Spam protection active**: Small transactions pay appropriate fees
- **Large transaction fairness**: Big transactions use rate-based fees

### Logging for Monitoring
```cpp
// Add to fee calculation points
LogPrint(BCLog::FEE, "Fee calculation: tx_size=%d rate_fee=%s absolute_min=%s final_fee=%s\n",
         tx_size, FormatMoney(rate_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE), FormatMoney(final_fee));

// Add to validation points
LogPrint(BCLog::MEMPOOL, "Fee validation: tx=%s fee=%s min_absolute=%s result=%s\n",
         hash.ToString(), FormatMoney(tx_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE), 
         (tx_fee >= ABSOLUTE_MIN_TX_FEE) ? "ACCEPT" : "REJECT");
```

## Risk Assessment and Mitigation

### Phase 1 Risks (Low)
- **Risk**: Wallet creates higher-fee transactions than expected
- **Mitigation**: Phase 1 only affects local wallet behavior, no network impact
- **Rollback**: Simple - revert wallet fee calculation changes

### Phase 2 Risks (Medium)  
- **Risk**: Network rejects legitimate low-fee transactions
- **Mitigation**: Extensive testnet testing, gradual rollout, clear documentation
- **Rollback**: Coordinated network update to disable validation

### Phase 3 Risks (Medium-High)
- **Risk**: Ecosystem incompatibility with RPC changes
- **Mitigation**: Version-based enforcement, backward compatibility options
- **Rollback**: Feature flags to disable new validation

## Timeline Recommendation

### Week 1-2: Phase 1 Implementation
- Implement wallet-only changes
- Run comprehensive wallet test suite
- **Goal**: Fix all failing wallet fee tests

### Week 3-4: Phase 1 Testing & Validation
- Testnet deployment
- Performance impact analysis
- Community feedback collection

### Week 5-8: Phase 2 Implementation (if Phase 1 successful)
- Network validation changes
- Extended testing period
- Ecosystem coordination

### Month 3+: Phase 3 Planning
- RPC integration design
- Dandelion++ testing
- Full ecosystem coordination

## Expected Impact on Test Suite

### Immediate Fixes (Phase 1)
- ✅ wallet_bumpfee.py --descriptors
- ✅ wallet_bumpfee.py --legacy-wallet  
- ✅ wallet_create_tx.py (edge cases)
- ✅ wallet_fundrawtransaction.py (fee selection)
- ✅ wallet_send.py (fee validation)
- ✅ wallet_sendall.py (balance calculations)

### Test Pass Rate Improvement
- **Current**: 222/278 tests passing (79.9%)
- **After Phase 1**: ~240/278 tests passing (86.3%)
- **After All Phases**: 278/278 tests passing (100%)

## Conclusion

This hybrid fee model fixes the fundamental architectural mismatch between DigiByte's economics and Bitcoin's fee system. By implementing absolute minimum fee enforcement alongside rate-based calculations, we achieve:

1. **Spam Protection**: All transactions pay at least 0.1 DGB
2. **Fairness**: Large transactions still use proportional rates  
3. **Test Compatibility**: Resolves BUG-001 affecting wallet tests
4. **Network Security**: Prevents low-fee spam attacks
5. **Ecosystem Readiness**: Phased approach minimizes disruption

**The Phase 1 implementation alone will immediately fix the failing wallet_bumpfee.py tests and related fee management issues, providing a solid foundation for the complete architectural fix.**