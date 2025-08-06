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
   - Line 874-876: Checks against `m_pool.m_min_relay_feerate` (0.001 DGB/kvB)
   - Line 674-681: `CheckFeeRate()` validates against dynamic mempool minimum
   - No absolute minimum enforced

3. **Network Relay** (`net_processing.cpp`)
   - Uses `DEFAULT_MIN_RELAY_TX_FEE` for relay decisions
   - Fee filter uses same per-kilobyte rates

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
   - Update `CheckFeeRate()` to enforce absolute minimum
   - Modify mempool acceptance logic (lines 874-876)

5. **src/wallet/spend.cpp**
   - Update fee calculation in `CreateTransactionInternal()`
   - Ensure coin selection respects new minimums

6. **src/txmempool.cpp**
   - Update `GetMinFee()` to respect absolute minimum

### Configuration and RPC Files

7. **src/init.cpp**
   - Update help text for `-mintxfee` and `-minrelaytxfee`
   - Add new `-absolutemintxfee` option if desired

8. **src/rpc/mempool.cpp**
   - Update fee-related RPC responses

9. **src/wallet/rpc/spend.cpp**
   - Update `sendtoaddress`, `sendmany`, etc. to respect new minimums

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

## Implementation Recommendations

### 1. Immediate Actions

1. **Implement Option 3 (Hybrid Approach)**
   - Provides both per-kilobyte and absolute minimum protection
   - Most flexible for different transaction sizes
   - Easiest to roll back if issues arise

2. **Add Configuration Option**
   ```cpp
   // Add to init.cpp
   "-absolutemintxfee=<amt>  Absolute minimum fee per transaction (default: 0.1 DGB)"
   ```

3. **Gradual Rollout**
   - Start with wallet enforcement only
   - Monitor network acceptance
   - Then enforce in mempool/relay

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

## Conclusion

The current DigiByte v8.26 fee structure allows transactions to pay less than the required 0.1 DGB minimum, creating spam vulnerability. The recommended solution is a hybrid approach that enforces both:

1. **Absolute minimum**: 0.1 DGB per transaction
2. **Rate-based fees**: For larger transactions

This approach provides spam protection while maintaining fairness for legitimate large transactions. Implementation should be gradual with extensive testing and clear communication to the ecosystem.

## Appendix: Quick Reference

### Current State
- Small TX (250 bytes) pays: **0.025 DGB** ❌
- Medium TX (1000 bytes) pays: **0.1 DGB** ✓
- Large TX (10000 bytes) pays: **1.0 DGB** ✓

### After Implementation
- Small TX (250 bytes) pays: **0.1 DGB** ✓
- Medium TX (1000 bytes) pays: **0.1 DGB** ✓
- Large TX (10000 bytes) pays: **1.0 DGB** ✓

### Key Files to Modify
1. `src/policy/policy.h` - Add `ABSOLUTE_MIN_TX_FEE`
2. `src/wallet/fees.cpp` - Enforce in `GetMinimumFee()`
3. `src/validation.cpp` - Enforce in `CheckFeeRate()`
4. 15+ test files - Update expectations

### Testing Command
```bash
# Run all fee-related tests after changes
./test/functional/test_runner.py --extended feature_fee wallet_fee mempool_fee
```