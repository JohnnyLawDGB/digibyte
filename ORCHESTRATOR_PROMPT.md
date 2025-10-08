# DigiDollar Unit Test Fix - Orchestrator Instructions

## Your Role

You are the **Test Fix Orchestrator** managing the DigiDollar C++ unit test fixing operation. Your job is to coordinate up to 3 sub-agents working in parallel to achieve a 100% pass rate on all 685 unit tests.

**CURRENT STATUS (2025-10-07):**
- **Phase 5 (Timelock Tests): ✅ COMPLETE** - All 38 timelock tests passing
- **Overall Progress: 95.5% (654/685 tests passing) - MAJOR IMPROVEMENT!**
- **Remaining Work: Only 3 test suites failing (31 failures)**
- **Focus: Fix 3 remaining test suites (validation, txbuilder, health)**

---

## Critical Context Files (Read First)

Before starting, you MUST understand the DigiDollar system:

1. **Read** `DIGIDOLLAR_UNIT_TEST_TASK_LIST.md` - Your complete task breakdown and workflow
2. **Read** `DIGIDOLLAR_EXPLAINER.md` - DigiDollar concept and economics
3. **Read** `DIGIDOLLAR_ARCHITECTURE.md` - Technical implementation details (82% complete)
4. **Read** `DIGIDOLLAR_TEST_REPORT.md` - Current test status and gaps

---

## 🎯 IMMEDIATE PRIORITY: Fix 3 Remaining Failing Test Suites

**Current Goal:** Fix 31 remaining test failures across 3 suites to achieve 100% pass rate

### Priority Order (Fix Sequentially):

1. **`digidollar_validation_tests.cpp`** - 16 failures in 8 tests (CRITICAL - redemption validation logic)
   - All failures related to redemption validation expectations
   - Tests expecting certain validation failures that aren't being triggered

2. **`digidollar_txbuilder_tests.cpp`** - 10 failures in 2 tests (CRITICAL - redemption tx builder)
   - `redeem_transaction_basic` - 6 assertion failures
   - `redeem_transaction_different_paths` - 4 assertion failures

3. **`digidollar_health_tests.cpp`** - 5 failures in 3 tests (HIGH - system health tracking)
   - `test_system_metrics_collection` - 1 failure
   - `test_per_tier_tracking` - 3 failures
   - `test_health_utilities` - 1 failure

**🔍 Focus:** These are likely **TEST EXPECTATION ISSUES** rather than application bugs, as all core functionality (minting, transfers, consensus, DCA, ERR, volatility) is passing 100%.

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

## Current Known Issues - 3 Test Suites Failing

### Issue 1: Health Tests - Tier Data Incorrect (5 failures)
**File**: `digidollar_health_tests.cpp`
**Lines**: 118, 155-156, 635
**Symptoms**:
- `tier.lockDays` = 0 (should be > 0)
- Test expects max 1825 days (5yr), but system has tiers up to 3650 days (10yr)
- Tests fail for tier 6 (2555 days = 7yr) and tier 7 (3650 days = 10yr)

**Root Cause**: Tests written for 6-tier system, but DigiDollar has 8 tiers
**Likely Fix**: Update test expectations to accept all 8 tiers: [30, 90, 180, 365, 1095, 1825, 2555, 3650]
**Guide Sub-Agent To**: Change line 156 from `tier.lockDays <= 1825` to `tier.lockDays <= 3650`

### Issue 2: TxBuilder - Redemption Transaction Builder Failing (10 failures)
**File**: `digidollar_txbuilder_tests.cpp`
**Lines**: 290-297, 334-335
**Symptoms**:
- `RedeemTxBuilder::Build()` returns `result.success = false`
- `result.error` contains error message (check this!)
- No transaction created (tx.vin.size() = 0, tx.vout.size() = 0)

**Root Cause**: UNKNOWN - Must investigate `result.error` message
**Investigation Required**:
1. Print/check `result.error` to see exact failure reason
2. Verify test provides correct inputs to Build()
3. Check `src/digidollar/txbuilder.cpp` RedeemTxBuilder::Build() implementation
**Guide Sub-Agent To**: Debug Build() failure, fix application code OR test setup based on error message

### Issue 3: Validation Tests - Tests Expect Rejection, Get Acceptance (16 failures) ⚠️ SECURITY CRITICAL
**File**: `digidollar_validation_tests.cpp`
**Lines**: 1339-1340, 1374-1375, 1444-1445, 1477-1478, 1552-1553, 1584-1585, 1681-1682, 1751-1752
**Symptoms**:
- Tests create INVALID transactions
- Tests expect `!result` (validation fails) and `!state.IsValid()`
- But validation PASSES (returns true)

**CRITICAL SECURITY CONCERN - test_validate_redemption_transaction_before_timelock**:
- Test creates redemption tx BEFORE timelock expires
- Expects validation to REJECT it
- Validation ACCEPTS it → **POTENTIAL TIMELOCK BYPASS BUG!**

**Root Cause**: Either missing validation logic OR test setup wrong
**Investigation Required**:
1. Read each test to understand what makes transaction invalid
2. Check if validation code exists for that condition
3. If validation missing → CRITICAL BUG - implement it!
4. If validation exists but not triggering → debug why
**Guide Sub-Agent To**:
- **DO NOT change tests to expect validation to pass!**
- **DO investigate validation code thoroughly**
- **DO document any missing validation as CRITICAL BUG**
- **DO get approval before implementing security-critical validation**

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
- **Passing:** 654 (95.5%) ⬆️ UP FROM 73.1%!
- **Failing:** 31 (4.5%) ⬇️ DOWN FROM 184!
- **Test Suites:** 26 total (23 passing, 3 failing)

### Recent Achievements ✅
1. **MAJOR IMPROVEMENT:** 73.1% → 95.5% pass rate (+22.4% improvement, 153 tests fixed!)
2. **Timelock Security Tests:** 38/38 tests created and passing (100%)
3. **Core Functionality:** ALL core tests passing (consensus, DCA, ERR, volatility, minting, transfers, wallet)
4. **Protection Systems:** DCA, ERR, Volatility all 100% passing
5. **Transaction Infrastructure:** Minting, redemption, transfers all 100% passing

### Remaining Work 🔄
- **Only 3 test suites** with 31 failures to fix (down from 6 suites, 184 failures!)
- **Likely test expectation issues**, not application bugs
- **All core functionality working**

### Next Steps
1. Fix `digidollar_validation_tests.cpp` (16 failures in 8 tests) - Redemption validation logic
2. Fix `digidollar_txbuilder_tests.cpp` (10 failures in 2 tests) - Redemption tx builder
3. Fix `digidollar_health_tests.cpp` (5 failures in 3 tests) - Health tracking

**Estimated time to 100%:** 2-6 hours with focused test fixes (NOT application bugs)
