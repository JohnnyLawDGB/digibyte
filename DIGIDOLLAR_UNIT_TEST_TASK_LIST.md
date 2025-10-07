# DigiDollar Unit Test Fix - Task List
**Generated**: 2025-10-06
**Objective**: Achieve 100% pass rate for all 685 C++ unit tests
**Strategy**: Orchestrator manages up to 3 parallel sub-agents, each fixing 1 test file at a time

---

## Current Status

**Total Unit Tests**: 685 test cases across 25 files
**Current Pass Rate**: 73.1% (501 passing, 184 test failures)
**Target**: 100% pass rate (0 failures)

---

## Test Files Priority Matrix

### 🔴 CRITICAL - Core Functionality (Fix First - Sequential)

| File | Tests | Priority | Reason | Status |
|------|-------|----------|--------|--------|
| `digidollar_consensus_tests.cpp` | 11 | P0 | Mint amount validation failing, chainparams integration broken | ❌ FAILING (10 failures) |
| `digidollar_transfer_tests.cpp` | 43 | P0 | Multiple DD output extraction failures, transaction version issues | ✅ PASSING |
| `digidollar_change_tests.cpp` | 4 | P0 | Memory access violation, DD amount extraction broken | ✅ PASSING |
| `digidollar_validation_tests.cpp` | 72 | P0 | Validation pipeline critical for all operations | ❌ FAILING (51 failures) |
| `digidollar_wallet_tests.cpp` | 129 | P0 | Largest test suite, wallet integration critical | ❌ FAILING (12 failures) |

### 🟡 HIGH - Protection Systems (Fix Second - Parallel OK)

| File | Tests | Priority | Reason | Status |
|------|-------|----------|--------|--------|
| `digidollar_dca_tests.cpp` | 22 | P1 | DCA system critical for collateral safety | ✅ PASSING |
| `digidollar_err_tests.cpp` | 37 | P1 | ERR system critical for under-collateralization | ❌ FAILING (81 failures) |
| `digidollar_volatility_tests.cpp` | 24 | P1 | Volatility protection prevents market manipulation | ✅ PASSING |
| `digidollar_health_tests.cpp` | 24 | P1 | System health monitoring foundational | ✅ PASSING |

### 🟢 MEDIUM - Transaction Infrastructure (Fix Third - Parallel OK)

| File | Tests | Priority | Reason | Status |
|------|-------|----------|--------|--------|
| `digidollar_mint_tests.cpp` | 29 | P2 | Minting process tests | ❌ FAILING (22 failures) |
| `digidollar_redeem_tests.cpp` | 24 | P2 | Redemption path tests | ✅ PASSING |
| `digidollar_transaction_tests.cpp` | 35 | P2 | Transaction type encoding | ✅ PASSING |
| `digidollar_txbuilder_tests.cpp` | 13 | P2 | Transaction builder logic | ❌ FAILING (8 failures) |
| `digidollar_scripts_tests.cpp` | 13 | P2 | P2TR script creation | ✅ PASSING |

### 🔵 LOW - Supporting Infrastructure (Fix Fourth - Parallel OK)

| File | Tests | Priority | Reason | Status |
|------|-------|----------|--------|--------|
| `digidollar_activation_tests.cpp` | 5 | P3 | BIP9 activation | ✅ PASSING |
| `digidollar_address_tests.cpp` | 11 | P3 | Address encoding/decoding | ✅ PASSING |
| `digidollar_opcodes_tests.cpp` | 21 | P3 | Script opcodes | ✅ PASSING |
| `digidollar_oracle_tests.cpp` | 35 | P3 | Oracle consensus (mock prices) | ✅ PASSING |
| `digidollar_p2p_tests.cpp` | 12 | P3 | P2P relay and Dandelion++ | ✅ PASSING |
| `digidollar_rpc_tests.cpp` | 30 | P3 | RPC commands | ✅ PASSING |
| `digidollar_structures_tests.cpp` | 18 | P3 | Data structures | ✅ PASSING |
| `digidollar_gui_tests.cpp` | 11 | P3 | GUI widgets | ⚠️ NOT TESTED |
| `digidollar_persistence_keys_tests.cpp` | 3 | P3 | Key persistence | ✅ PASSING |
| `digidollar_persistence_serialization_tests.cpp` | 3 | P3 | Serialization | ✅ PASSING |
| `digidollar_persistence_walletbatch_tests.cpp` | 18 | P3 | Wallet batch operations | ✅ PASSING |

---

## ✅ COMPLETE: Cryptographic Timelock Security Tests

### Test File Created: `digidollar_timelock_tests.cpp`

**Priority**: 🔴 **CRITICAL - P0**
**Total Tests**: 38 test cases
**Status**: ✅ PASSING (All 38 tests passing)

#### Test Coverage Completed:

**1. OP_CHECKLOCKTIMEVERIFY (CLTV) Tests** (8 tests) ✅
- [x] Test CLTV with block height-based timelocks (30 days → blocks)
- [x] Test CLTV with timestamp-based timelocks (1 year → Unix timestamp)
- [x] Test CLTV enforcement (transaction rejected before timelock)
- [x] Test CLTV acceptance (transaction accepted after timelock)
- [x] Test CLTV with nLockTime interaction
- [x] Test CLTV with negative timelock (should fail)
- [x] Test CLTV with max timelock (10 years)
- [x] Test CLTV script validation

**2. OP_CHECKSEQUENCEVERIFY (CSV) Tests** (6 tests) ✅
- [x] Test CSV relative timelock (blocks since UTXO creation)
- [x] Test CSV with sequence number encoding
- [x] Test CSV with BIP68 compliance
- [x] Test CSV enforcement before relative time expires
- [x] Test CSV acceptance after relative time expires
- [x] Test CSV with different sequence types (blocks vs time)

**3. nLockTime Transaction Tests** (5 tests) ✅
- [x] Test nLockTime prevents early mining (block height)
- [x] Test nLockTime prevents early mining (timestamp)
- [x] Test nLockTime with mempool acceptance rules
- [x] Test nLockTime with block validation
- [x] Test nLockTime edge cases (height vs timestamp boundary)

**4. Timelock Cryptographic Security** (8 tests) ✅
- [x] Test timelock cannot be bypassed with signature manipulation
- [x] Test timelock cannot be bypassed with script modification
- [x] Test timelock with Schnorr signature validation
- [x] Test timelock with ECDSA signature validation
- [x] Test timelock with P2TR witness validation
- [x] Test timelock with MAST tree path selection
- [x] Test timelock replay protection (same tx different times)
- [x] Test timelock with chain reorganization scenarios

**5. DigiDollar-Specific Timelock Integration** (6 tests) ✅
- [x] Test collateral vault timelock (all 8 tiers: 30d→10y)
- [x] Test normal redemption path requires timelock expiry
- [x] Test emergency redemption bypasses timelock (8-of-15 oracles)
- [x] Test partial redemption with active timelock
- [x] Test ERR redemption with active timelock
- [x] Test timelock metadata in OP_RETURN encoding

**6. Timelock Attack Vectors** (5 tests) ✅
- [x] Test timelock DOS attack prevention (cannot spam timelocked txs)
- [x] Test timelock grief attack prevention (cannot lock others' funds)
- [x] Test timelock front-running prevention
- [x] Test timelock with RBF (Replace-By-Fee) attacks
- [x] Test timelock with transaction malleability

**Total Completed**: **38 timelock security tests - ✅ ALL PASSING**

---

## Orchestration Workflow

### Phase 1: Critical Core (Sequential - 1 agent at a time)
**Objective**: Fix foundation tests that everything depends on
**Parallelization**: ❌ NO - Fix sequentially to avoid conflicts

1. **Agent 1**: `digidollar_consensus_tests.cpp` (11 tests)
2. **Agent 1**: `digidollar_transfer_tests.cpp` (43 tests)
3. **Agent 1**: `digidollar_change_tests.cpp` (4 tests)
4. **Agent 1**: `digidollar_validation_tests.cpp` (72 tests)
5. **Agent 1**: `digidollar_wallet_tests.cpp` (129 tests)

**Estimated Time**: 5 cycles × 2-4 hours = **10-20 hours**

---

### Phase 2: Protection Systems (Parallel - 3 agents)
**Objective**: Fix DCA, ERR, volatility, health systems
**Parallelization**: ✅ YES - Independent systems

**Batch 2A** (parallel):
- **Agent A**: `digidollar_dca_tests.cpp` (22 tests)
- **Agent B**: `digidollar_err_tests.cpp` (37 tests)
- **Agent C**: `digidollar_volatility_tests.cpp` (24 tests)

**Batch 2B** (parallel):
- **Agent A**: `digidollar_health_tests.cpp` (24 tests)

**Estimated Time**: 2 batches × 2-3 hours = **4-6 hours**

---

### Phase 3: Transaction Infrastructure (Parallel - 3 agents)
**Objective**: Fix minting, redemption, transaction building
**Parallelization**: ✅ YES - Can work in parallel

**Batch 3A** (parallel):
- **Agent A**: `digidollar_mint_tests.cpp` (29 tests)
- **Agent B**: `digidollar_redeem_tests.cpp` (24 tests)
- **Agent C**: `digidollar_transaction_tests.cpp` (35 tests)

**Batch 3B** (parallel):
- **Agent A**: `digidollar_txbuilder_tests.cpp` (13 tests)
- **Agent B**: `digidollar_scripts_tests.cpp` (13 tests)

**Estimated Time**: 2 batches × 2-3 hours = **4-6 hours**

---

### Phase 4: Supporting Infrastructure (Parallel - 3 agents)
**Objective**: Fix remaining tests
**Parallelization**: ✅ YES - All independent

**Batch 4A** (parallel):
- **Agent A**: `digidollar_opcodes_tests.cpp` (21 tests)
- **Agent B**: `digidollar_oracle_tests.cpp` (35 tests)
- **Agent C**: `digidollar_p2p_tests.cpp` (12 tests)

**Batch 4B** (parallel):
- **Agent A**: `digidollar_rpc_tests.cpp` (30 tests)
- **Agent B**: `digidollar_structures_tests.cpp` (18 tests)
- **Agent C**: `digidollar_gui_tests.cpp` (11 tests)

**Batch 4C** (parallel):
- **Agent A**: `digidollar_persistence_keys_tests.cpp` (3 tests)
- **Agent B**: `digidollar_persistence_serialization_tests.cpp` (3 tests)
- **Agent C**: `digidollar_persistence_walletbatch_tests.cpp` (18 tests)

**Estimated Time**: 3 batches × 1-2 hours = **3-6 hours**

---

### Phase 5: NEW - Cryptographic Timelock Security (Create & Validate)
**Objective**: Create comprehensive timelock security test suite
**Parallelization**: ⚠️ PARTIAL - Create first, then parallelize test categories

**Step 5A**: Create test file structure (1 agent)
- **Agent A**: Create `digidollar_timelock_tests.cpp` with all test scaffolding

**Step 5B**: Implement test categories (parallel - 3 agents)
- **Agent A**: CLTV tests (8) + CSV tests (6) = 14 tests
- **Agent B**: nLockTime tests (5) + Crypto security (8) = 13 tests
- **Agent C**: DigiDollar integration (6) + Attack vectors (5) = 11 tests

**Estimated Time**: 1 + 1 = **2-4 hours**

---

## Total Estimated Timeline

| Phase | Work Type | Agent Mode | Estimated Time |
|-------|-----------|------------|----------------|
| Phase 1 | Critical Core | Sequential (1 agent) | 10-20 hours |
| Phase 2 | Protection Systems | Parallel (3 agents) | 4-6 hours |
| Phase 3 | Transaction Infra | Parallel (3 agents) | 4-6 hours |
| Phase 4 | Supporting Infra | Parallel (3 agents) | 3-6 hours |
| Phase 5 | Timelock Security | Mixed (1→3 agents) | 2-4 hours |
| **TOTAL** | **All Phases** | **Mixed** | **23-42 hours** |

**With 3 parallel agents**: Estimated **1-2 weeks wall-clock time**

---

## Success Criteria

### ✅ Definition of Done

1. **All 647 existing unit tests passing** (100% pass rate)
2. **38 new timelock security tests created and passing**
3. **Total: 685 unit tests at 100% pass rate**
4. **Zero application bugs remaining** (all found bugs documented and fixed)
5. **Cryptographic timelock security validated** (CLTV, CSV, nLockTime)
6. **All tests run successfully in CI**: `./src/test/test_digibyte --run_test=digidollar_*`

### 📊 Progress Tracking

**Current**: 19/25 files passing, 501/685 tests passing (73.1%)
**Target**: 25/25 files fixed, 685/685 tests passing (100%)
**Remaining**: 6 files failing, 184 test failures to fix (some tests have multiple assertions)

---

## Known Issues to Fix

### Critical Bugs Found in Test Runs

**FIXED** ✅ (Previously Failing, Now Passing):
1. **`digidollar_transfer_tests.cpp`**: All 43 tests now passing
   - Fixed DD output extraction
   - Fixed transaction version issues
   - Fixed DD amount balance verification

2. **`digidollar_change_tests.cpp`**: All 4 tests now passing
   - Fixed memory access violation
   - Fixed DD amount extraction

**STILL FAILING** ❌ (6 test suites, 184 failures):

1. **`digidollar_consensus_tests.cpp`** - 10 failures (out of 11 tests):
   - ❌ `IsValidMintAmount()` failing for valid amounts (100, 1000, 100000 cents)
   - ❌ ChainParams integration broken: `minMintAmount` mismatch
     - Mainnet: expects 10000 cents, getting 100000000
     - Testnet: expects 100 cents, getting 1000000
     - Regtest: expects 1 cent, getting 10000

2. **`digidollar_err_tests.cpp`** - 81 failures (out of 37 tests):
   - ❌ ERR system calculations failing (multiple assertions per test)
   - ❌ Emergency redemption validation broken

3. **`digidollar_mint_tests.cpp`** - 22 failures (out of 29 tests):
   - ❌ Minting transaction creation failing
   - ❌ Assertion error: CCheckQueue worker threads not empty

4. **`digidollar_txbuilder_tests.cpp`** - 8 failures (out of 13 tests):
   - ❌ Transaction builder logic broken

5. **`digidollar_validation_tests.cpp`** - 51 failures (out of 72 tests):
   - ❌ Validation pipeline critical failures

6. **`digidollar_wallet_tests.cpp`** - 12 failures (out of 129 tests):
   - ❌ Wallet integration issues

### Bug Documentation Format

Each sub-agent must create bug reports in `DIGIDOLLAR_BUGS_FOUND.md`:

```markdown
## Bug #X: [Short Description]
**File**: [test file that exposed bug]
**Severity**: Critical/High/Medium/Low
**Component**: [application file with bug]
**Symptom**: [what's failing]
**Root Cause**: [why it's failing]
**Fix Applied**: [code change made]
**Tests Affected**: [which tests now pass]
```

---

## Agent Assignment Log

| Phase | Batch | Agent | File | Tests | Status | Start Time | End Time | Bugs Found |
|-------|-------|-------|------|-------|--------|------------|----------|------------|
| 1 | - | A | digidollar_consensus_tests.cpp | 11 | 🔄 | - | - | - |
| 1 | - | A | digidollar_transfer_tests.cpp | 43 | ⏳ | - | - | - |
| 1 | - | A | digidollar_change_tests.cpp | 4 | ⏳ | - | - | - |
| 1 | - | A | digidollar_validation_tests.cpp | 72 | ⏳ | - | - | - |
| 1 | - | A | digidollar_wallet_tests.cpp | 129 | ⏳ | - | - | - |
| 2 | A | A | digidollar_dca_tests.cpp | 22 | ⏳ | - | - | - |
| 2 | A | B | digidollar_err_tests.cpp | 37 | ⏳ | - | - | - |
| 2 | A | C | digidollar_volatility_tests.cpp | 24 | ⏳ | - | - | - |
| 2 | B | A | digidollar_health_tests.cpp | 24 | ⏳ | - | - | - |
| 3 | A | A | digidollar_mint_tests.cpp | 29 | ⏳ | - | - | - |
| 3 | A | B | digidollar_redeem_tests.cpp | 24 | ⏳ | - | - | - |
| 3 | A | C | digidollar_transaction_tests.cpp | 35 | ⏳ | - | - | - |
| 3 | B | A | digidollar_txbuilder_tests.cpp | 13 | ⏳ | - | - | - |
| 3 | B | B | digidollar_scripts_tests.cpp | 13 | ⏳ | - | - | - |
| 4 | A | A | digidollar_opcodes_tests.cpp | 21 | ⏳ | - | - | - |
| 4 | A | B | digidollar_oracle_tests.cpp | 35 | ⏳ | - | - | - |
| 4 | A | C | digidollar_p2p_tests.cpp | 12 | ⏳ | - | - | - |
| 4 | B | A | digidollar_rpc_tests.cpp | 30 | ⏳ | - | - | - |
| 4 | B | B | digidollar_structures_tests.cpp | 18 | ⏳ | - | - | - |
| 4 | B | C | digidollar_gui_tests.cpp | 11 | ⏳ | - | - | - |
| 4 | C | A | digidollar_persistence_keys_tests.cpp | 3 | ⏳ | - | - | - |
| 4 | C | B | digidollar_persistence_serialization_tests.cpp | 3 | ⏳ | - | - | - |
| 4 | C | C | digidollar_persistence_walletbatch_tests.cpp | 18 | ⏳ | - | - | - |
| 5 | A | A | digidollar_timelock_tests.cpp | 38 (NEW) | ⏳ | - | - | - |

**Legend**: 🔄 In Progress | ⏳ Pending | ✅ Complete | ❌ Failed

---

## Final Validation Checklist

After all phases complete, orchestrator must verify:

- [ ] Run full test suite: `./src/test/test_digibyte --run_test=digidollar_*`
- [ ] Confirm 685/685 tests passing (100% pass rate)
- [ ] Review `DIGIDOLLAR_BUGS_FOUND.md` for all documented bugs
- [ ] Verify all bug fixes applied correctly
- [ ] Run tests 3 times to ensure stability (no flaky tests)
- [ ] Verify timelock security tests cover all attack vectors
- [ ] Confirm cryptographic soundness validated
- [ ] Update DIGIDOLLAR_TEST_REPORT.md with final results
- [ ] Git commit all test fixes with detailed commit message
- [ ] Create summary report of all work completed

---

**Generated**: 2025-10-06
**Status**: Ready for Orchestrator Execution
**Total Tests to Fix**: 647 existing + 38 new = 685 total
**Estimated Completion**: 1-2 weeks with 3 parallel agents
