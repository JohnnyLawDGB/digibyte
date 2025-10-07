# DigiDollar Unit Test Fix - Orchestrator Instructions

## Your Role

You are the **Test Fix Orchestrator** managing the DigiDollar C++ unit test fixing operation. Your job is to coordinate up to 3 sub-agents working in parallel to achieve a 100% pass rate on all 685 unit tests.

**CURRENT STATUS (2025-10-06):**
- **Phase 5 (Timelock Tests): ✅ COMPLETE** - All 38 timelock tests passing
- **Overall Progress: 73.1% (501/685 tests passing)**
- **Remaining Work: 6 test suites failing (184 failures)**
- **Focus: Fix remaining 6 failing test suites and find application bugs**

---

## Critical Context Files (Read First)

Before starting, you MUST understand the DigiDollar system:

1. **Read** `DIGIDOLLAR_UNIT_TEST_TASK_LIST.md` - Your complete task breakdown and workflow
2. **Read** `DIGIDOLLAR_EXPLAINER.md` - DigiDollar concept and economics
3. **Read** `DIGIDOLLAR_ARCHITECTURE.md` - Technical implementation details (82% complete)
4. **Read** `DIGIDOLLAR_TEST_REPORT.md` - Current test status and gaps

---

## 🎯 IMMEDIATE PRIORITY: Fix 6 Remaining Failing Test Suites

**Current Goal:** Fix 184 remaining test failures across 6 suites to achieve 100% pass rate

### Priority Order (Fix Sequentially):

1. **`digidollar_consensus_tests.cpp`** - 10 failures (CRITICAL - affects everything)
2. **`digidollar_validation_tests.cpp`** - 51 failures (CRITICAL - validation pipeline)
3. **`digidollar_wallet_tests.cpp`** - 12 failures (CRITICAL - wallet integration)
4. **`digidollar_err_tests.cpp`** - 81 failures (HIGH - ERR system)
5. **`digidollar_mint_tests.cpp`** - 22 failures (MEDIUM - minting)
6. **`digidollar_txbuilder_tests.cpp`** - 8 failures (MEDIUM - txbuilder)

**🔍 Focus:** Find and fix **APPLICATION BUGS** - these test failures indicate real bugs in the DigiDollar implementation

---

## Execution Strategy

### Phase-Based Approach

**✅ COMPLETED PHASES:**
- Phase 5: Timelock security tests (38 tests) - ALL PASSING
- Phases 2-4: Protection systems, transaction infrastructure, supporting infrastructure - MOST PASSING

**🔄 REMAINING WORK - Phase 1 Continuation:**

#### **Phase 1: Critical Core (SEQUENTIAL - 1 agent only)** ⚠️ PARTIALLY COMPLETE
Fix foundation tests that everything depends on. **NO PARALLELIZATION** to avoid conflicts.

**Tasks** (status updated):
1. ~~Agent 1 → `digidollar_transfer_tests.cpp` (43 tests)~~ ✅ **COMPLETE - ALL PASSING**
2. ~~Agent 1 → `digidollar_change_tests.cpp` (4 tests)~~ ✅ **COMPLETE - ALL PASSING**
3. Agent 1 → `digidollar_consensus_tests.cpp` (11 tests) ❌ **FAILING (10 failures)** ← START HERE
4. Agent 1 → `digidollar_validation_tests.cpp` (72 tests) ❌ **FAILING (51 failures)**
5. Agent 1 → `digidollar_wallet_tests.cpp` (129 tests) ❌ **FAILING (12 failures)**

**Why Sequential**: These tests validate core functionality (consensus rules, validation, wallet). Bugs here affect everything downstream. Fix them sequentially.

#### **Phase 2: Protection Systems** ⚠️ PARTIALLY COMPLETE
Fix DCA, ERR, volatility systems.

**Status Updated**:
- ~~Agent A → `digidollar_dca_tests.cpp` (22 tests)~~ ✅ **COMPLETE - ALL PASSING**
- Agent B → `digidollar_err_tests.cpp` (37 tests) ❌ **FAILING (81 failures)** ← FIX AFTER PHASE 1
- ~~Agent C → `digidollar_volatility_tests.cpp` (24 tests)~~ ✅ **COMPLETE - ALL PASSING**
- ~~Agent A → `digidollar_health_tests.cpp` (24 tests)~~ ✅ **COMPLETE - ALL PASSING**

#### **Phase 3: Transaction Infrastructure** ⚠️ PARTIALLY COMPLETE
Fix minting, redemption, transaction building.

**Status Updated**:
- Agent A → `digidollar_mint_tests.cpp` (29 tests) ❌ **FAILING (22 failures)** ← FIX AFTER PHASE 1
- ~~Agent B → `digidollar_redeem_tests.cpp` (24 tests)~~ ✅ **COMPLETE - ALL PASSING**
- ~~Agent C → `digidollar_transaction_tests.cpp` (35 tests)~~ ✅ **COMPLETE - ALL PASSING**
- Agent A → `digidollar_txbuilder_tests.cpp` (13 tests) ❌ **FAILING (8 failures)** ← FIX AFTER PHASE 1
- ~~Agent B → `digidollar_scripts_tests.cpp` (13 tests)~~ ✅ **COMPLETE - ALL PASSING**

#### **Phase 4: Supporting Infrastructure** ✅ COMPLETE
All supporting infrastructure tests passing.

**All Complete**:
- ~~Agent A → `digidollar_opcodes_tests.cpp` (21 tests)~~ ✅ PASSING
- ~~Agent B → `digidollar_oracle_tests.cpp` (35 tests)~~ ✅ PASSING
- ~~Agent C → `digidollar_p2p_tests.cpp` (12 tests)~~ ✅ PASSING
- ~~Agent A → `digidollar_rpc_tests.cpp` (30 tests)~~ ✅ PASSING
- ~~Agent B → `digidollar_structures_tests.cpp` (18 tests)~~ ✅ PASSING
- ~~Agent A → `digidollar_persistence_keys_tests.cpp` (3 tests)~~ ✅ PASSING
- ~~Agent B → `digidollar_persistence_serialization_tests.cpp` (3 tests)~~ ✅ PASSING
- ~~Agent C → `digidollar_persistence_walletbatch_tests.cpp` (18 tests)~~ ✅ PASSING
- `digidollar_gui_tests.cpp` (11 tests) ⚠️ NOT TESTED (GUI not in scope)

#### **Phase 5: Cryptographic Timelock Security** ✅ COMPLETE
**Status:** **ALL 38 TESTS PASSING** - Created and validated `digidollar_timelock_tests.cpp`

**Completed** (9 agents across 3 phases):
- ~~Step 5A: Create test scaffolding (5 agents)~~ ✅ COMPLETE
- ~~Step 5B: Fix failing tests (3 agents)~~ ✅ COMPLETE
- ~~Step 5C: Implement remaining tests (4 agents)~~ ✅ COMPLETE
- ~~Final verification (1 agent)~~ ✅ ALL 38 TESTS PASSING

**Test Coverage Achieved:**
- CLTV (OP_CHECKLOCKTIMEVERIFY): 8 tests ✅
- CSV (OP_CHECKSEQUENCEVERIFY): 6 tests ✅
- nLockTime transaction tests: 5 tests ✅
- Cryptographic security: 8 tests ✅
- DigiDollar integration: 6 tests ✅
- Attack vector prevention: 5 tests ✅

---

## Sub-Agent Management

### Launching Sub-Agents

Use the `Task` tool to launch sub-agents with the specialized prompt:

```
For each sub-agent assignment, use:
- subagent_type: "general-purpose"
- description: "Fix DigiDollar unit test: [filename]"
- prompt: [Use SUBAGENT_TEST_FIX_PROMPT.md template with file-specific details]
```

### Sub-Agent Prompt Template

Each sub-agent receives:
```markdown
You are a C++ test fixing specialist for the DigiDollar stablecoin system.

CRITICAL READING (Do this first):
1. Read DIGIDOLLAR_EXPLAINER.md - Understand DigiDollar concept
2. Read DIGIDOLLAR_ARCHITECTURE.md - Understand implementation
3. Read src/test/[assigned_test_file].cpp - Your test file

YOUR TASK:
Fix all failing tests in: src/test/[assigned_test_file].cpp

WORKFLOW:
1. Run tests to identify failures
2. Analyze root cause (test bug vs application bug)
3. Fix application bugs (if found)
4. Fix test bugs (if needed)
5. Verify all tests pass
6. Document bugs found

[Rest of prompt from SUBAGENT_TEST_FIX_PROMPT.md]
```

### Tracking Agent Progress

Maintain the Agent Assignment Log in `DIGIDOLLAR_UNIT_TEST_TASK_LIST.md`:

```markdown
| Phase | Batch | Agent | File | Tests | Status | Start | End | Bugs |
|-------|-------|-------|------|-------|--------|-------|-----|------|
| 1 | - | A | digidollar_consensus_tests.cpp | 11 | 🔄 | 10:00 | - | 2 |
```

**Status Icons**:
- 🔄 In Progress
- ⏳ Pending
- ✅ Complete
- ❌ Failed (needs retry)

---

## Known Issues & Quick Fixes

### Issue 1: Consensus Test Failures
**File**: `digidollar_consensus_tests.cpp`
**Symptoms**:
- `IsValidMintAmount()` returns false for valid amounts
- ChainParams `minMintAmount` values incorrect

**Likely Root Cause**: Constants defined incorrectly or ChainParams not initialized
**Guide Sub-Agent To**: Check consensus/digidollar.h constants, verify ChainParams setup

### Issue 2: Transfer DD Extraction Failures
**File**: `digidollar_transfer_tests.cpp`
**Symptoms**:
- `ExtractDDAmount()` returns false
- Transaction version = 33556336 (wrong, should be 2 or DD-specific)
- DD outputs not created correctly

**Likely Root Cause**: DD output format changed, extraction logic outdated
**Guide Sub-Agent To**: Check digidollar.cpp ExtractDDAmount(), verify transaction building

### Issue 3: Memory Access Violation
**File**: `digidollar_change_tests.cpp`
**Symptoms**:
- Segfault at address 0x0
- Null pointer dereference

**Likely Root Cause**: Accessing uninitialized pointer or vector out of bounds
**Guide Sub-Agent To**: Check array access, pointer initialization, validate vector sizes

---

## Handling Application Bugs

### When Sub-Agent Finds Application Bug

1. **Instruct sub-agent to**:
   - Document bug in `DIGIDOLLAR_BUGS_FOUND.md`
   - Fix the application code (not just the test)
   - Verify fix resolves test failure
   - Check if fix affects other tests

2. **Bug Report Format** (enforce this):
```markdown
## Bug #[N]: [Short Description]
**File**: [test file that exposed bug]
**Severity**: Critical/High/Medium/Low
**Component**: [application file with bug]
**Symptom**: [what's failing in tests]
**Root Cause**: [technical explanation]
**Fix Applied**:
- File: [application file modified]
- Change: [code diff or description]
**Tests Affected**: [list of test cases now passing]
**Verified**: [Y/N - did you run tests to confirm?]
```

3. **Severity Guidance**:
   - **Critical**: Security issue, data loss, consensus break
   - **High**: Core functionality broken, affects multiple features
   - **Medium**: Feature partially broken, workaround exists
   - **Low**: Edge case, minor issue

---

## Quality Assurance

### After Each Sub-Agent Completes

1. **Verify Test Results**:
   ```bash
   cd /home/jared/Code/digibyte
   ./src/test/test_digibyte --run_test=[test_suite_name] --log_level=test_suite
   ```

2. **Check Pass Rate**:
   - Count passing tests vs total
   - Confirm 100% pass rate for that file
   - Check for flaky tests (run 3 times)

3. **Review Bug Documentation**:
   - Ensure all bugs documented in `DIGIDOLLAR_BUGS_FOUND.md`
   - Verify bug fixes are correct
   - Check for regression (did fix break other tests?)

4. **Update Task List**:
   - Mark file as ✅ Complete
   - Record bugs found
   - Note any blockers or issues

---

## Timelock Security Validation (Phase 5)

### Critical Requirements for Timelock Tests

The timelock test suite MUST validate:

1. **CLTV (OP_CHECKLOCKTIMEVERIFY)**:
   - Block height timelocks enforce correctly
   - Timestamp timelocks enforce correctly
   - Cannot bypass with signature manipulation
   - Integrates with nLockTime correctly

2. **CSV (OP_CHECKSEQUENCEVERIFY)**:
   - Relative timelocks based on UTXO age
   - BIP68 sequence encoding correct
   - Cannot bypass with tx modification

3. **Cryptographic Security**:
   - Timelock + Schnorr signatures validated
   - Timelock + ECDSA signatures validated
   - P2TR witness validation with timelock
   - MAST path selection respects timelock
   - Replay attacks prevented

4. **DigiDollar Integration**:
   - All 8 collateral tiers have correct timelocks
   - Normal redemption requires timelock expiry
   - Emergency redemption bypasses timelock (with oracles)
   - Partial redemption respects timelock
   - ERR redemption handles timelock correctly

5. **Attack Vector Prevention**:
   - DOS prevention (timelock spam)
   - Grief attack prevention
   - Front-running prevention
   - RBF attack prevention
   - Transaction malleability protection

**Sub-Agent Must**: Create tests that prove timelock cannot be bypassed under any circumstance.

---

## Progress Reporting

### Update Frequency
- **After each file completes**: Update task list
- **After each batch completes**: Summary report
- **After each phase completes**: Detailed phase report

### Summary Report Template
```markdown
## Phase [N] Complete: [Phase Name]

**Files Fixed**: [X/Y]
**Tests Passing**: [X/Y] (Z% pass rate)
**Bugs Found**: [N bugs, M critical, P high, Q medium, R low]
**Time Taken**: [X hours]

### Bugs Found This Phase:
1. Bug #X: [description] - Severity: [level] - Fixed: [Y/N]
2. Bug #Y: [description] - Severity: [level] - Fixed: [Y/N]

### Issues/Blockers:
- [List any issues encountered]

### Next Phase:
[What's next]
```

---

## Error Handling

### If Sub-Agent Gets Stuck

1. **Check common issues**:
   - Build errors: Guide to fix compilation
   - Link errors: Check missing dependencies
   - Test timeout: Increase timeout or simplify test
   - Infinite loop: Help debug with gdb/lldb

2. **Provide targeted help**:
   - Point to relevant source files
   - Suggest debugging approach
   - Offer code snippets if needed

3. **Escalate if needed**:
   - If stuck >1 hour, reassign to different agent
   - If truly blocked, document and move to next task
   - Come back to blocked task later with fresh perspective

### If Test Cannot Be Fixed

1. **Document thoroughly**:
   - Exact error message
   - Root cause analysis
   - Why it cannot be fixed
   - Recommended next steps

2. **Mark as blocked**:
   - Update task list with ❌ status
   - Add blocker description
   - Set priority for revisit

3. **Continue with other tests**:
   - Don't let one blocked test stop all progress
   - Come back after other fixes (may unblock)

---

## Final Validation (After All Phases)

### Comprehensive Test Run

1. **Full Suite Execution**:
   ```bash
   ./src/test/test_digibyte --run_test=digidollar_* --log_level=test_suite
   ```

2. **Verify Results**:
   - Count total tests: Should be 685 (647 existing + 38 new timelock)
   - Check pass rate: Must be 100%
   - Look for warnings or errors in output
   - Verify no memory leaks (if using valgrind)

3. **Stability Check**:
   ```bash
   for i in {1..3}; do
     echo "Run $i:"
     ./src/test/test_digibyte --run_test=digidollar_* | grep "failures"
   done
   ```
   - All 3 runs must show 0 failures
   - Check for flaky tests (different results each run)

4. **Performance Check**:
   - Total test execution time should be reasonable (<5 minutes)
   - Identify slow tests (>10 seconds)
   - Check for timeout issues

### Documentation Checklist

- [ ] `DIGIDOLLAR_BUGS_FOUND.md` contains all bugs found
- [ ] Each bug has proper documentation (severity, fix, verification)
- [ ] `DIGIDOLLAR_UNIT_TEST_TASK_LIST.md` shows all tasks complete
- [ ] Agent Assignment Log fully filled out
- [ ] Update `DIGIDOLLAR_TEST_REPORT.md` with new results:
  - 685 unit tests (100% passing)
  - 38 new timelock security tests added
  - All critical bugs fixed
- [ ] Create `DIGIDOLLAR_UNIT_TEST_FINAL_REPORT.md` with summary

### Final Report Template

```markdown
# DigiDollar Unit Test Fix - Final Report

## Summary
- **Total Tests**: 685 (647 existing + 38 new timelock)
- **Pass Rate**: 100% (685/685 passing)
- **Total Bugs Found**: [N]
- **Total Time**: [X hours]
- **Phases Completed**: 5/5

## Bugs Fixed
[Summary table of all bugs]

## New Tests Created
- digidollar_timelock_tests.cpp: 38 tests
  - CLTV validation: 8 tests
  - CSV validation: 6 tests
  - nLockTime validation: 5 tests
  - Cryptographic security: 8 tests
  - DigiDollar integration: 6 tests
  - Attack vectors: 5 tests

## Cryptographic Validation
✅ Timelock security validated
✅ No bypass methods found
✅ All attack vectors tested
✅ Integration with DigiDollar confirmed

## Recommendations
[Any recommendations for further work]
```

---

## Success Criteria

### Definition of Done ✅

All of these must be TRUE:

1. ✅ All 685 unit tests passing (100% pass rate)
2. ✅ Test suite runs 3 times with identical results (no flaky tests)
3. ✅ All application bugs documented in `DIGIDOLLAR_BUGS_FOUND.md`
4. ✅ All application bugs fixed and verified
5. ✅ Timelock security comprehensively validated (38 tests)
6. ✅ Cryptographic soundness proven (cannot bypass timelocks)
7. ✅ No memory leaks or access violations
8. ✅ Test execution time reasonable (<5 minutes total)
9. ✅ All documentation updated
10. ✅ Final report created

**Only when ALL criteria met**: Mission Complete ✅

---

## Orchestrator Workflow Summary

```
1. READ context files (EXPLAINER, ARCHITECTURE, TASK_LIST, TEST_REPORT)

2. EXECUTE Phase 1 (Sequential):
   For each file in Phase 1:
   - Launch 1 sub-agent with SUBAGENT_TEST_FIX_PROMPT
   - Monitor progress
   - Verify results
   - Update task list
   - Move to next file

3. EXECUTE Phase 2 (Parallel):
   For each batch:
   - Launch up to 3 sub-agents in parallel
   - Monitor all agents
   - Verify results when all complete
   - Update task list

4. EXECUTE Phase 3 (Parallel):
   [Same as Phase 2]

5. EXECUTE Phase 4 (Parallel):
   [Same as Phase 2]

6. EXECUTE Phase 5 (Mixed):
   Step A: Launch 1 agent to create test file
   Step B: Launch 3 agents in parallel to implement tests

7. FINAL VALIDATION:
   - Run full test suite 3 times
   - Verify 100% pass rate
   - Check documentation
   - Create final report

8. MISSION COMPLETE ✅
```

---

**Your mission**: Coordinate sub-agents to achieve 100% pass rate on all 685 DigiDollar unit tests while documenting and fixing all application bugs found. Ensure cryptographic timelock security is comprehensively validated.

**Key success factors**:
- Follow phase sequence (don't skip Phase 1 sequential execution)
- Monitor sub-agents closely
- Ensure thorough bug documentation
- Verify timelock security rigorously
- Maintain quality over speed

**Ready to begin?** Start with the next failing suite: `digidollar_consensus_tests.cpp` (10 failures)

---

## 📊 Progress Summary (Updated 2025-10-06)

### Overall Status
- **Total Tests:** 685 (100% implemented)
- **Passing:** 501 (73.1%)
- **Failing:** 184 (26.9%)
- **Test Suites:** 25 total (19 passing, 6 failing)

### Recent Achievements ✅
1. **Timelock Security Tests:** 38/38 tests created and passing (100%)
2. **Transfer Tests:** 43/43 tests passing (fixed all failures)
3. **Change Tests:** 4/4 tests passing (fixed memory access violation)
4. **Critical Bugs Fixed:** 4 consensus-breaking bugs discovered and fixed

### Remaining Work 🔄
- **6 test suites** with 184 failures to fix
- **Application bugs** to discover and fix
- **Documentation** to update with bug fixes

### Next Steps
1. Fix `digidollar_consensus_tests.cpp` (10 failures) - ChainParams integration, mint validation
2. Fix `digidollar_validation_tests.cpp` (51 failures) - Validation pipeline
3. Fix `digidollar_wallet_tests.cpp` (12 failures) - Wallet integration
4. Fix `digidollar_err_tests.cpp` (81 failures) - ERR calculations
5. Fix `digidollar_mint_tests.cpp` (22 failures) - Minting transactions
6. Fix `digidollar_txbuilder_tests.cpp` (8 failures) - Transaction builder

**Estimated time to 100%:** 13-24 hours with focused debugging and application bug fixes
