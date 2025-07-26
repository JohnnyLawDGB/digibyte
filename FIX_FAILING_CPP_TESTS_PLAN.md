# DigiByte v8.26 C++ Test Suite Fix Plan

## Overview
This document provides a detailed analysis and fix plan for the 6 failing C++ test suites in DigiByte v8.26 after the Bitcoin Core v26.2 merge.

## Executive Summary
- **Total Failing Test Suites:** 6
- **Categories:** Wallet (4), Validation (2)
- **Root Cause:** Bitcoin v26.2 merge conflicts with DigiByte-specific behavior
- **Estimated Effort:** 3-5 days for complete fixes

## Failing Test Suites Analysis

### 1. coinselector_tests
**File:** `wallet/test/coinselector_tests.cpp`
**Failing Tests:** `bnb_search_test`
**Error Lines:** 232, 260

#### Issue Analysis
```cpp
error: check EquivalentResult(expected_result, *result3) has failed
error: check EquivalentResult(expected_result, *result5) has failed
```

#### Root Cause
The Branch and Bound (BnB) coin selection algorithm is producing different results than expected. This is likely due to:
1. Different fee calculation in DigiByte (0.00001 DGB/kB vs Bitcoin's rates)
2. Different coin values due to DigiByte's 1000:1 ratio
3. Possible changes in the BnB algorithm from Bitcoin v26.2

#### Fix Strategy
1. **Analyze fee calculations:**
   - Check `GetDustThreshold()` implementation
   - Verify `m_effective_feerate` calculations
   - Compare with DigiByte v8.22.2 implementation

2. **Update test expectations:**
   ```cpp
   // Look for hardcoded values like:
   CAmount target = 100000; // May need adjustment for DGB
   ```

3. **Key files to check:**
   - `src/wallet/coinselection.cpp`
   - `src/policy/feerate.cpp`
   - `src/wallet/spend.cpp`

### 2. descriptor_tests
**File:** `test/descriptor_tests.cpp`
**Failing Tests:** `descriptor_test`
**Error Lines:** 181, 322, 341 (multiple)

#### Issue Analysis
```cpp
error: DescriptorID() does not match for priv wsh(...)
error: check ref[n] == HexStr(spks[n]) has failed
```

#### Root Cause
Witness Script Hash (WSH) descriptors are generating different script pubkeys than expected. This indicates:
1. Different address encoding in DigiByte
2. Possible changes in descriptor derivation
3. Hash calculation differences

#### Fix Strategy
1. **Verify address encoding:**
   - Check DigiByte's bech32 prefix (`dgb1` for mainnet, `dgbt1` for testnet)
   - Verify witness version encoding

2. **Update test vectors:**
   ```cpp
   // Current failing descriptor:
   "wsh(and_v(v:ripemd160(...),pk(...)))"
   // May need DigiByte-specific test vectors
   ```

3. **Key areas to investigate:**
   - `src/script/descriptor.cpp`
   - `src/key_io.cpp`
   - `src/script/standard.cpp`

### 3. spend_tests
**File:** `wallet/test/spend_tests.cpp`
**Failing Test:** `SubtractFee`
**Error Lines:** 39, 40 (multiple)

#### Issue Analysis
```cpp
error: check txr.tx->vout.size() == 1 has failed [2 != 1]
error: check txr.tx->vout[0].nValue == recipient.nAmount + leftover_input_amount - txr.fee has failed
```

#### Root Cause
The transaction is creating 2 outputs instead of 1, suggesting:
1. Change output is being created when it shouldn't
2. Dust threshold differences causing unexpected behavior
3. Fee subtraction logic differences

#### Fix Strategy
1. **Analyze change creation logic:**
   ```cpp
   // Check conditions for change output creation
   if (change_amount > GetDustThreshold(...)) {
       // Change output is added
   }
   ```

2. **Verify DigiByte dust threshold:**
   - Default dust: 1000 satoshis in DigiByte
   - May differ from Bitcoin's implementation

3. **Update test expectations:**
   - May need to adjust input/output amounts
   - Consider DigiByte's fee structure

### 4. validation_chainstate_tests
**File:** `test/validation_chainstate_tests.cpp`
**Failing Test:** `chainstate_update_tip`
**Error Line:** 87

#### Issue Analysis
```cpp
fatal error: critical check CreateAndActivateUTXOSnapshot(...) has failed
```

#### Root Cause
UTXO snapshot creation is failing, likely due to:
1. DigiByte-specific chainstate parameters
2. Different genesis block or chain parameters
3. Snapshot format incompatibility

#### Fix Strategy
1. **Review snapshot creation:**
   - Check `CreateAndActivateUTXOSnapshot()` implementation
   - Verify DigiByte chain parameters are used

2. **Update test setup:**
   ```cpp
   // Ensure DigiByte-specific setup
   SelectParams(ChainType::REGTEST);
   // May need DigiByte-specific genesis
   ```

3. **Key files:**
   - `src/validation.cpp`
   - `src/node/utxo_snapshot.cpp`
   - `src/chainparams.cpp`

### 5. validation_chainstatemanager_tests
**File:** `test/validation_chainstatemanager_tests.cpp`
**Failing Tests:** Multiple (chainstatemanager_activate_snapshot, chainstatemanager_loadblockindex)
**Error Lines:** 260, 529-531, 551

#### Issue Analysis
```cpp
fatal error: critical check CreateAndActivateUTXOSnapshot(this) has failed
error: check cs1.setBlockIndexCandidates.size() == 2 has failed [0 != 2]
```

#### Root Cause
Chainstate manager is not properly handling:
1. Block index candidates
2. UTXO snapshot activation
3. Multiple chainstate coordination

#### Fix Strategy
1. **Debug block index loading:**
   ```cpp
   // Add logging to understand why candidates aren't added
   LogPrintf("setBlockIndexCandidates size: %d\n", 
             cs1.setBlockIndexCandidates.size());
   ```

2. **Verify DigiByte-specific behavior:**
   - Multi-algorithm mining may affect block validation
   - Check if assumed valid blocks are set correctly

3. **Review initialization:**
   - Ensure test blocks are properly created
   - Verify chain tip handling

### 6. wallet_tests
**File:** `wallet/test/wallet_tests.cpp`
**Failing Tests:** LoadReceiveRequests, CreateWallet
**Error Lines:** 455, 458, 836, 840, 850

#### Issue Analysis
```cpp
error: check !wallet->IsAddressPreviouslySpent(ScriptHash()) has failed
error: check addtx_count == 3 has failed [1 != 3]
error: check wallet->mapWallet.count(mempool_tx.GetHash()) == 1U has failed [0 != 1]
```

#### Root Cause
Wallet is not properly:
1. Tracking spent addresses
2. Adding transactions from mempool
3. Handling wallet database operations

#### Fix Strategy
1. **Debug address tracking:**
   ```cpp
   // Check IsAddressPreviouslySpent implementation
   // May have DigiByte-specific address handling
   ```

2. **Verify transaction addition:**
   - Check wallet notification handling
   - Ensure mempool integration works

3. **Review wallet database:**
   - May need DigiByte-specific wallet format handling
   - Check for migration issues

## Implementation Plan

### Phase 1: Quick Fixes (1 day)
1. **Update test constants for DigiByte:**
   - Fee rates: 0.00001 DGB/kB
   - Dust threshold: 1000 satoshis
   - Address prefixes

2. **Fix obvious expectation mismatches:**
   - Update hardcoded values in tests
   - Adjust for 1000:1 DGB:BTC ratio

### Phase 2: Deep Analysis (2 days)
1. **Set up debugging environment:**
   ```bash
   # Run individual failing tests with debugging
   ./src/test/test_digibyte --run_test=coinselector_tests/bnb_search_test --log_level=all
   ```

2. **Add extensive logging:**
   - Transaction creation steps
   - Fee calculations
   - Address generation

3. **Compare with v8.22.2:**
   - Identify behavior changes
   - Document DigiByte-specific logic

### Phase 3: Implementation (1-2 days)
1. **Apply fixes in order of complexity:**
   - spend_tests (simplest - output count issue)
   - coinselector_tests (fee calculation)
   - descriptor_tests (address encoding)
   - wallet_tests (state tracking)
   - validation_chainstate_tests (snapshot handling)
   - validation_chainstatemanager_tests (most complex)

2. **Test each fix individually:**
   ```bash
   ./src/test/test_digibyte --run_test=<test_suite>
   ```

### Phase 4: Verification (0.5 days)
1. **Run full test suite:**
   ```bash
   ./src/test/test_digibyte --log_level=all
   ```

2. **Ensure no regressions:**
   - Verify all 100 passing tests still pass
   - Document any changes

## Testing Strategy

### Unit Test Verification
```bash
# Test individual suites after fixes
./src/test/test_digibyte --run_test=coinselector_tests
./src/test/test_digibyte --run_test=descriptor_tests
./src/test/test_digibyte --run_test=spend_tests
./src/test/test_digibyte --run_test=validation_chainstate_tests
./src/test/test_digibyte --run_test=validation_chainstatemanager_tests
./src/test/test_digibyte --run_test=wallet_tests
```

### Integration Testing
```bash
# Run full suite
make check

# Run with valgrind for memory issues
valgrind ./src/test/test_digibyte
```

## Key DigiByte Constants Reference

```cpp
// Fee structure
static constexpr CAmount DEFAULT_MIN_RELAY_TX_FEE = 1000;  // 0.00001 DGB/kB
static constexpr CAmount DUST_RELAY_TX_FEE = 1000;         // 0.00001 DGB/kB

// Network
static const int MAINNET_DEFAULT_PORT = 12024;
static const int TESTNET_DEFAULT_PORT = 12025;

// Supply
static const CAmount MAX_MONEY = 21000000000 * COIN;  // 21 billion DGB

// Block rewards
CAmount GetBlockSubsidy(int nHeight) {
    // Current: 72000 DGB
}

// Address prefixes
// Mainnet: D (30), S (63), dgb1
// Testnet: s (140), dgbt1
```

## Success Criteria
1. All 6 failing test suites pass
2. No regression in the 100 currently passing tests
3. Tests pass consistently across multiple runs
4. No memory leaks or undefined behavior
5. Code changes are minimal and well-documented

## Risk Mitigation
1. **Create fix branch:**
   ```bash
   git checkout -b fix/cpp-unit-tests
   ```

2. **Commit each fix separately:**
   - One commit per test suite
   - Clear commit messages

3. **Document all changes:**
   - Add comments explaining DigiByte-specific behavior
   - Update test documentation

4. **Peer review:**
   - Have changes reviewed before merging
   - Run tests on different platforms

## Appendix: Debugging Commands

```bash
# Run with GDB
gdb ./src/test/test_digibyte
(gdb) run --run_test=wallet_tests/CreateWallet

# Run with verbose logging
./src/test/test_digibyte --run_test=validation_chainstate_tests --log_level=all 2>&1 | tee test_output.log

# Check for memory leaks
valgrind --leak-check=full ./src/test/test_digibyte --run_test=spend_tests

# Compare test behavior
diff <(./src/test/test_digibyte --run_test=wallet_tests --log_level=all 2>&1) \
     <(cd ../digibyte-v8.22.2 && ./src/test/test_digibyte --run_test=wallet_tests --log_level=all 2>&1)
```

## References
- Bitcoin Core v26.2 release notes
- DigiByte v8.22.2 test suite
- Bitcoin Core testing documentation
- DigiByte-specific constants and parameters