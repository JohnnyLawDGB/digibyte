# DigiByte v8.26 Transaction Fee Analysis and Implementation Specification

## Executive Summary

DigiByte v8.26 currently implements a **fee-per-kilobyte** model inherited from Bitcoin, which does not enforce a minimum absolute fee of 0.1 DGB per transaction as required for spam protection. This document provides a comprehensive analysis of the current fee implementation and specifications for necessary changes.

**Critical Finding**: The current implementation allows transactions as small as 250 bytes to pay only 0.025 DGB in fees (at the default 0.1 DGB/kvB rate), which is insufficient for spam protection given DigiByte's 1000x larger supply compared to Bitcoin.

## Current Fee Implementation in DigiByte v8.26

### Fee Constants and Their Values

| Constant | Value (satoshis) | Value (DGB) | Location | Purpose |
|----------|------------------|-------------|----------|---------|
| `DEFAULT_TRANSACTION_MINFEE` | 10,000,000 | 0.1 DGB/kvB | `wallet/wallet.h:117` | Wallet minimum fee rate |
| `DEFAULT_MIN_RELAY_TX_FEE` | 100,000 | 0.001 DGB/kvB | `policy/policy.h:59` | Network relay minimum |
| `DUST_RELAY_TX_FEE` | 30,000 | 0.0003 DGB/kvB | `policy/policy.h:57` | Dust threshold calculation |
| `DEFAULT_FALLBACK_FEE` | 1,000,000 | 0.01 DGB/kvB | `wallet/wallet.h:113` | Fallback when fee estimation fails |
| `WALLET_INCREMENTAL_RELAY_FEE` | 1,000,000 | 0.01 DGB/kvB | `wallet/wallet.h:131` | RBF fee increment |
| `DEFAULT_DISCARD_FEE` | 10,000 | 0.0001 DGB/kvB | `wallet/wallet.h:115` | Small change discard threshold |

### Fee Calculation Model

The current implementation uses a **fee-per-kilobyte** model:

```cpp
// From policy/feerate.cpp:22-35
CAmount CFeeRate::GetFee(uint32_t num_bytes) const
{
    // Calculate fee as: (rate_per_kvB * size_in_bytes) / 1000
    CAmount nFee = std::ceil(nSatoshisPerK * num_bytes / 1000.0);
    return nFee;
}
```

**Problem**: This means a 250-byte transaction pays:
- Fee = 10,000,000 * 250 / 1000 = 2,500,000 satoshis = **0.025 DGB**
- This is 4x less than the required 0.1 DGB minimum

### Fee Enforcement Points

1. **Wallet Fee Calculation** (`wallet/fees.cpp`)
   - `GetMinimumFeeRate()`: Returns max of wallet minimum and relay minimum
   - `GetRequiredFeeRate()`: Enforces wallet.m_min_fee (0.1 DGB/kvB)
   - No absolute minimum enforced

2. **Mempool Acceptance** (`validation.cpp`)
   - Line 670-684: `CheckFeeRate()` validates against dynamic mempool minimum
   - Line 874-876: Checks against `m_pool.m_min_relay_feerate` (0.001 DGB/kvB)
   - No absolute minimum enforced

3. **Network Relay** (`net_processing.cpp`)
   - Uses `DEFAULT_MIN_RELAY_TX_FEE` for relay decisions
   - Fee filter messages (`feefilter`) use same per-kilobyte rates
   - `PeerManager` enforces fee expectations

4. **RPC Raw Transactions** (bypasses wallet logic)
   - `sendrawtransaction`: Direct mempool submission
   - `testmempoolaccept`: Fee validation testing
   - `submitpackage`: Package fee validation
   - These endpoints need explicit fee validation

5. **Dynamic Mempool Fee** (`txmempool.cpp`)
   - Line 1116: `CTxMemPool::GetMinFee()` implements rolling fee adjustments
   - Adjusts based on mempool size and eviction
   - No absolute minimum enforced

6. **Dandelion++ Stempool** (`dandelion.cpp`)
   - Transactions in stem phase before broadcast
   - Currently no fee validation in stempool
   - Potential spam vector during privacy phase

## Bitcoin vs DigiByte Fee Scaling Analysis

### Supply and Economic Differences

| Metric | Bitcoin | DigiByte | Ratio |
|--------|---------|----------|-------|
| Max Supply | 21 million | 21 billion | 1000x |
| Block Time | 600 seconds | 15 seconds | 40x faster |
| Typical TX Size | 250 bytes | 250 bytes | Same |
| Min Relay Fee | 0.00001 BTC/kvB | 0.001 DGB/kvB | 100x |
| Wallet Min Fee | 0.00001 BTC/kvB | 0.1 DGB/kvB | 10,000x |

### Current Fee Comparison for 250-byte Transaction

| Network | Fee Rate | Actual Fee | USD Value* |
|---------|----------|------------|------------|
| Bitcoin | 0.00001 BTC/kvB | 0.0000025 BTC | ~$0.25 |
| DigiByte (current) | 0.1 DGB/kvB | 0.025 DGB | ~$0.0003 |
| DigiByte (required) | 0.4 DGB/kvB OR absolute 0.1 DGB | 0.1 DGB | ~$0.001 |

*USD values are illustrative based on approximate market prices

## Required Changes for 0.1 DGB Minimum Fee

### Option 1: Absolute Minimum Fee (Recommended)

Implement an absolute minimum fee check that overrides the per-kilobyte calculation:

```cpp
// New constant in policy/policy.h
static constexpr CAmount ABSOLUTE_MIN_TX_FEE{10000000}; // 0.1 DGB absolute minimum

// Modified fee calculation in wallet/fees.cpp
CAmount GetMinimumFee(const CWallet& wallet, unsigned int nTxBytes, 
                      const CCoinControl& coin_control, FeeCalculation* feeCalc)
{
    CFeeRate feerate_needed = GetMinimumFeeRate(wallet, coin_control, feeCalc);
    CAmount calculated_fee = feerate_needed.GetFee(nTxBytes);
    
    // Enforce absolute minimum
    return std::max(calculated_fee, ABSOLUTE_MIN_TX_FEE);
}
```

### Option 2: Increased Fee Rate

Increase the minimum fee rate to ensure even small transactions meet the 0.1 DGB minimum:

```cpp
// For a 250-byte transaction to pay 0.1 DGB:
// Required rate = 0.1 DGB * 1000 / 250 = 0.4 DGB/kvB = 40,000,000 satoshis/kvB
static const CAmount DEFAULT_TRANSACTION_MINFEE = 40000000; // 0.4 DGB/kvB
```

### Option 3: Hybrid Approach (Most Flexible)

Combine both approaches for maximum flexibility:

```cpp
// Use higher of: calculated fee or absolute minimum
CAmount final_fee = std::max(
    feerate.GetFee(tx_size),     // Per-kilobyte calculation
    ABSOLUTE_MIN_TX_FEE           // Absolute minimum of 0.1 DGB
);
```

## Files Requiring Modification

### Core Implementation Files

1. **src/policy/policy.h**
   - Add `ABSOLUTE_MIN_TX_FEE` constant
   - Update `DEFAULT_MIN_RELAY_TX_FEE` if using Option 2

2. **src/wallet/wallet.h**
   - Update `DEFAULT_TRANSACTION_MINFEE` if using Option 2
   - Add absolute minimum fee member variable

3. **src/wallet/fees.cpp**
   - Modify `GetMinimumFee()` to enforce absolute minimum
   - Update `GetRequiredFee()` similarly

4. **src/validation.cpp**
   - Update `CheckFeeRate()` (line 670) to enforce absolute minimum
   - Modify mempool acceptance logic (lines 874-876)
   - Add absolute minimum check in `PreChecks()`

5. **src/wallet/spend.cpp**
   - Update fee calculation in `CreateTransactionInternal()` (line 967)
   - Add absolute minimum check after line 1096
   - Update coin selection parameters around line 1050
   - Ensure coin selection respects new minimums

6. **src/txmempool.cpp**
   - Update `GetMinFee()` (line 1116) to respect absolute minimum
   - Modify rolling fee calculation to include absolute floor
   - ```cpp
     return std::max({CFeeRate(llround(rollingMinimumFeeRate)), 
                      m_incremental_relay_feerate,
                      CFeeRate(ABSOLUTE_MIN_TX_FEE/1000)});
     ```

7. **src/dandelion.cpp**
   - Add fee validation for stempool transactions
   - Update embargo logic to check minimum fees
   - Prevent low-fee transactions in stem phase

8. **src/node/transaction.cpp**
   - Update `BroadcastTransaction()` fee validation
   - Add absolute minimum check before broadcast

### Configuration and RPC Files

9. **src/init.cpp**
   - Update help text for `-mintxfee` and `-minrelaytxfee`
   - Add new `-absolutemintxfee` option
   - Parse command-line arguments for new fee options

10. **src/kernel/mempool_options.h**
    - Line 46: Update default mempool fee initialization
    - Add absolute minimum fee member

11. **src/rpc/mempool.cpp**
    - Update fee-related RPC responses
    - Add absolute minimum to `getmempoolinfo`

12. **src/rpc/rawtransaction.cpp**
    - Add fee validation to `sendrawtransaction`
    - Update `testmempoolaccept` with absolute minimum check
    - Validate fees in `submitpackage`

13. **src/wallet/rpc/spend.cpp**
    - Update `sendtoaddress`, `sendmany`, etc. to respect new minimums
    - Add clear error messages for insufficient fees

14. **src/net_processing.cpp**
    - Update fee filter message handling
    - Modify `PeerManager` fee expectations
    - Add version-based fee enforcement logic

## Test Files Requiring Updates

### Primary Fee-Related Tests

1. **test/functional/feature_fee_estimation.py**
   - Update expected fee calculations
   - Add tests for absolute minimum

2. **test/functional/wallet_fallbackfee.py**
   - Verify fallback respects absolute minimum

3. **test/functional/wallet_bumpfee.py**
   - Ensure RBF respects new minimums

4. **test/functional/mempool_accept.py**
   - Test rejection of low-fee transactions

5. **test/functional/mempool_dust.py**
   - Update dust threshold calculations

6. **test/functional/p2p_feefilter.py**
   - Update fee filter expectations

7. **test/functional/rpc_estimatefee.py**
   - Verify estimation respects minimums

### Secondary Tests Affected by Fee Changes

8. **test/functional/wallet_basic.py**
   - Transaction creation tests

9. **test/functional/wallet_sendmany_chain.py**
   - Multi-output transaction fees

10. **test/functional/wallet_fundrawtransaction.py**
    - Raw transaction funding

11. **test/functional/mempool_limit.py**
    - Mempool eviction by fee

12. **test/functional/mempool_packages.py**
    - Package fee validation

13. **test/functional/feature_rbf.py**
    - Replace-by-fee increments

14. **test/functional/wallet_create_tx.py**
    - Transaction creation edge cases

15. **test/functional/wallet_groups.py**
    - Coin selection with fees

### Additional Tests Needed

16. **test/functional/feature_absolute_min_fee.py** (NEW)
    - Test absolute minimum fee enforcement
    - Verify rejection of low-fee transactions
    - Test hybrid approach behavior

17. **test/functional/dandelion_fee_validation.py** (NEW)
    - Test stempool fee validation
    - Verify Dandelion++ with minimum fees
    - Test embargo with fee checks

18. **test/functional/rpc_raw_fee_validation.py** (NEW)
    - Test sendrawtransaction fee validation
    - Test testmempoolaccept with low fees
    - Test submitpackage fee requirements

## Implementation Recommendations

### 1. Immediate Actions

1. **Implement Option 3 (Hybrid Approach)**
   - Provides both per-kilobyte and absolute minimum protection
   - Most flexible for different transaction sizes
   - Protects against spam while being fair to larger transactions
   - Easiest to roll back if issues arise

2. **Add Configuration Options**
   ```cpp
   // Add to init.cpp
   "-absolutemintxfee=<amt>  Absolute minimum fee per transaction (default: 0.1 DGB)"
   "-enforceabsolutefee=<bool>  Enforce absolute minimum fee (default: true)"
   ```

3. **Phased Rollout Strategy**
   - **Phase 1**: Wallet enforcement only (immediate)
     - Update wallet fee calculation
     - Add user warnings for low fees
   - **Phase 2**: Mempool enforcement (after 2 weeks testing)
     - Enforce in validation.cpp
     - Update mempool acceptance rules
   - **Phase 3**: Network relay enforcement (after network readiness)
     - Update peer communication
     - Coordinate with major nodes/pools
     - Consider protocol version bump

### 2. Testing Strategy

1. **Unit Tests**
   - Test fee calculation with various transaction sizes
   - Verify absolute minimum enforcement
   - Check edge cases (very large transactions)

2. **Functional Tests**
   - Update all fee-related tests listed above
   - Add new test: `test/functional/feature_absolute_min_fee.py`
   - Verify network propagation

3. **Testnet Deployment**
   - Deploy to testnet first
   - Monitor for transaction rejection issues
   - Gather fee metrics

### 3. Monitoring and Metrics

Add logging to track:
- Transactions paying exactly minimum fee
- Transactions rejected for low fees
- Average fee per transaction
- Fee distribution histogram

## Potential Issues and Mitigations

### Issue 1: Large Transaction Overhead
**Problem**: A 10KB transaction would pay 1.0 DGB at 0.1 DGB/kvB rate
**Mitigation**: Use hybrid approach - larger transactions use per-kilobyte rate

### Issue 2: Wallet Compatibility
**Problem**: Older wallets may create invalid transactions
**Mitigation**: 
- Implement grace period with warnings
- Provide clear error messages
- Update wallet software first

### Issue 3: Exchange Integration
**Problem**: Exchanges may have hardcoded fee logic
**Mitigation**:
- Provide advance notice
- Offer integration support
- Consider phased rollout

### Issue 4: Smart Contract/Script Transactions
**Problem**: Complex transactions may be disproportionately affected
**Mitigation**: Monitor and adjust rates based on actual usage

### Issue 5: Dandelion++ Compatibility
**Problem**: Stempool transactions may be rejected after broadcast
**Mitigation**:
- Validate fees before entering stempool
- Update embargo logic to include fee checks
- Ensure stem and fluff phases have consistent validation

### Issue 6: Raw Transaction APIs
**Problem**: Direct RPC submission bypasses wallet fee logic
**Mitigation**:
- Add explicit fee validation in RPC handlers
- Provide clear error messages for API users
- Document minimum fee requirements in RPC help

## Conclusion

The current DigiByte v8.26 fee structure allows transactions to pay less than the required 0.1 DGB minimum, creating spam vulnerability. The recommended solution is a hybrid approach that enforces both:

1. **Absolute minimum**: 0.1 DGB per transaction
2. **Rate-based fees**: For larger transactions

This approach provides spam protection while maintaining fairness for legitimate large transactions. Implementation should be gradual with extensive testing and clear communication to the ecosystem.

## Additional Implementation Details

### Detailed Code Changes

#### 1. Wallet Fee Enforcement (`wallet/spend.cpp`)
```cpp
// After line 1096 in CreateTransactionInternal()
CAmount absolute_min = ABSOLUTE_MIN_TX_FEE;
if (not_input_fees < absolute_min && !coin_control.fOverrideFeeRate) {
    return util::Error{strprintf(_("Transaction fee %s is below minimum required %s"), 
                                FormatMoney(not_input_fees), FormatMoney(absolute_min))};
}
```

#### 2. Mempool Dynamic Fee (`txmempool.cpp`)
```cpp
// Update GetMinFee() at line 1137
CFeeRate CTxMemPool::GetMinFee(size_t sizelimit) const {
    // ... existing rolling fee calculation ...
    CFeeRate min_fee = std::max(CFeeRate(llround(rollingMinimumFeeRate)), 
                                m_incremental_relay_feerate);
    // Enforce absolute minimum
    CFeeRate absolute_min_rate(ABSOLUTE_MIN_TX_FEE / 1000);
    return std::max(min_fee, absolute_min_rate);
}
```

#### 3. Validation Check (`validation.cpp`)
```cpp
// Update CheckFeeRate() at line 670
bool CheckFeeRate(size_t package_size, CAmount package_fee, TxValidationState& state) {
    // Check absolute minimum first
    if (package_fee < ABSOLUTE_MIN_TX_FEE) {
        return state.Invalid(TxValidationResult::TX_MEMPOOL_POLICY, 
                            "absolute min fee not met", 
                            strprintf("%d < %d", package_fee, ABSOLUTE_MIN_TX_FEE));
    }
    // ... existing per-kb checks ...
}
```

### Stempool Integration (Dandelion++)

**Critical**: Transactions in the stempool must respect minimum fees to prevent spam during the privacy phase.

#### Files to Update:
- `src/dandelion.cpp`: Add fee validation before embargo
- `src/node/transaction.cpp`: Check fees in `BroadcastTransaction()`
- `src/txmempool.cpp`: Validate stempool transaction fees

#### Example Implementation:
```cpp
// In dandelion.cpp
bool ValidateStemPoolFee(const CTransaction& tx, CAmount& fee_out) {
    // Calculate transaction fee
    CAmount tx_fee = /* calculate from inputs - outputs */;
    
    // Check absolute minimum
    if (tx_fee < ABSOLUTE_MIN_TX_FEE) {
        LogPrint(BCLog::DANDELION, "Rejecting stempool tx %s: fee %s below minimum %s\n",
                tx.GetHash().ToString(), FormatMoney(tx_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE));
        return false;
    }
    
    fee_out = tx_fee;
    return true;
}
```

### RPC Endpoint Updates

These RPC methods bypass wallet logic and need explicit fee validation:

1. **sendrawtransaction**: Direct mempool submission
2. **testmempoolaccept**: Fee validation testing  
3. **submitpackage**: Package fee validation

#### Implementation:
```cpp
// In rpc/rawtransaction.cpp
static RPCHelpMan sendrawtransaction() {
    // ... existing code ...
    
    // Before accepting to mempool
    CAmount tx_fee = /* calculate fee */;
    if (tx_fee < ABSOLUTE_MIN_TX_FEE && !bypass_limits) {
        throw JSONRPCError(RPC_TRANSACTION_REJECTED,
            strprintf("Transaction fee %s is below minimum required %s",
                     FormatMoney(tx_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE)));
    }
}
```

### Network Protocol Updates

When enforcing new minimums network-wide:

1. **Update PROTOCOL_VERSION** to signal support
2. **Add version-based enforcement** in validation
3. **Update feefilter messages** to respect absolute minimum

```cpp
// In net_processing.cpp
void PeerManagerImpl::ProcessFeeFilter(CNode& peer, CAmount newFeeFilter) {
    // Ensure fee filter respects absolute minimum
    CAmount min_filter = std::max(newFeeFilter, ABSOLUTE_MIN_TX_FEE / 1000);
    peer.m_fee_filter_sent = min_filter;
}
```

## Appendix: Quick Reference

### Current State
- Small TX (250 bytes) pays: **0.025 DGB** ❌
- Medium TX (1000 bytes) pays: **0.1 DGB** ✓
- Large TX (10000 bytes) pays: **1.0 DGB** ✓

### After Implementation
- Small TX (250 bytes) pays: **0.1 DGB** ✓
- Medium TX (1000 bytes) pays: **0.1 DGB** ✓
- Large TX (10000 bytes) pays: **1.0 DGB** ✓

### Key Files to Modify (Priority Order)
1. `src/policy/policy.h` - Add `ABSOLUTE_MIN_TX_FEE`
2. `src/wallet/fees.cpp` - Enforce in `GetMinimumFee()`
3. `src/wallet/spend.cpp` - Check in `CreateTransactionInternal()`
4. `src/validation.cpp` - Enforce in `CheckFeeRate()`
5. `src/txmempool.cpp` - Update `GetMinFee()`
6. `src/dandelion.cpp` - Add stempool validation
7. `src/rpc/rawtransaction.cpp` - Validate in RPC methods
8. 18+ test files - Update expectations

### Testing Commands
```bash
# Run all fee-related tests after changes
./test/functional/test_runner.py --extended feature_fee wallet_fee mempool_fee

# Test Dandelion++ integration
./test/functional/test_runner.py dandelion_fee_validation

# Test RPC endpoints
./test/functional/test_runner.py rpc_raw_fee_validation

# Full test suite
./test/functional/test_runner.py --extended
```

### Monitoring Metrics

Add these log statements for monitoring:
```cpp
LogPrint(BCLog::MEMPOOL, "Fee validation: tx=%s size=%d fee=%s min=%s\n",
         hash.ToString(), tx_size, FormatMoney(tx_fee), FormatMoney(ABSOLUTE_MIN_TX_FEE));
```