# ORCHESTRATOR PROMPT - DigiDollar Unit Test Fix Management

## Your Role

You are the **Orchestrator Agent** responsible for managing sub-agents to fix failing DigiDollar unit tests in the DigiByte codebase. Your job is to coordinate specialized sub-agents, ensure they don't conflict, track progress, and deliver 100% passing unit tests.

---

## 🎉 EXCELLENT NEWS - Current Status

**Test Results:** ✅ **1,257 PASSING** | ❌ **1 FAILING**
**Success Rate:** 99.92% (Almost Perfect!)
**Total Tests:** 1,258

Out of 1,258 total unit tests, only **ONE** test is failing. This demonstrates exceptional code quality.

---

## The Single Failing Test

**Test Suite:** `transaction_tests`
**Test Case:** `tx_valid`
**File:** `/home/jared/Code/digibyte/src/test/transaction_tests.cpp`
**Line:** 194
**Error:** `mapFlagNames is missing a script verification flag`

### Root Cause Analysis

The test is failing because:

1. **What Happened:** DigiDollar added a new script verification flag: `SCRIPT_VERIFY_DIGIDOLLAR`
2. **Where It Was Added:** This flag was added to `STANDARD_SCRIPT_VERIFY_FLAGS` in `/home/jared/Code/digibyte/src/policy/policy.h:118`
3. **What's Missing:** The flag was NOT added to the test's `mapFlagNames` mapping in `transaction_tests.cpp` (lines 48-70)
4. **Why It Fails:** The test function `CheckMapFlagNames()` verifies that ALL flags in `STANDARD_SCRIPT_VERIFY_FLAGS` have corresponding entries in `mapFlagNames` for test purposes

### The Fix Required

Add this single line to `mapFlagNames` in `transaction_tests.cpp`:

```cpp
{std::string("DIGIDOLLAR"), (unsigned int)SCRIPT_VERIFY_DIGIDOLLAR},
```

**Location:** Between lines 68-69 (after "DISCOURAGE_UPGRADABLE_PUBKEYTYPE" and before the closing brace)

---

## Your Mission

Deploy **ONE** sub-agent to add the missing flag mapping. This is a simple, low-risk fix.

---

## Critical Rules

### 1. **PROTECT THE 1,257 PASSING TESTS**
- **DO NOT** modify any other flags or test logic
- **DO NOT** refactor unrelated code
- **DO NOT** change test behavior
- **ONLY** add the single missing flag mapping

### 2. **Sub-Agent Deployment Strategy**
You need exactly **ONE** sub-agent for this fix:

- **Agent Type:** Code Fix Specialist
- **Task:** Add SCRIPT_VERIFY_DIGIDOLLAR to mapFlagNames
- **Scope:** Single line addition to one file
- **Risk Level:** LOW (simple addition following existing pattern)
- **Expected Time:** 5-10 minutes

### 3. **Verification Protocol**

**Before Fix:**
```bash
./src/test/test_digibyte 2>&1 | tail -5
# Shows: *** 1 failure is detected
```

**After Fix:**
```bash
./src/test/test_digibyte 2>&1 | tail -5
# Should show: *** No errors detected
```

**Full Verification:**
```bash
./src/test/test_digibyte --run_test=transaction_tests/tx_valid
# Should show: *** No errors detected in the test module "DigiByte Core Test Suite"
```

### 4. **Quality Checks**
- [ ] Code compiles without warnings
- [ ] All 1,258 tests pass (0 failures)
- [ ] No new warnings introduced
- [ ] Code follows existing style/format
- [ ] Added flag appears in alphabetical/logical order

---

## Sub-Agent Instructions

When deploying your sub-agent, provide these exact instructions:

```markdown
## TASK: Fix mapFlagNames Missing DIGIDOLLAR Flag

### Context

You are fixing the ONLY failing unit test out of 1,258 total tests. This is a simple flag mapping addition.

**File to Modify:** `/home/jared/Code/digibyte/src/test/transaction_tests.cpp`
**Lines to Examine:** 48-70 (mapFlagNames definition)

### Problem

DigiDollar added `SCRIPT_VERIFY_DIGIDOLLAR` to the standard verification flags, but forgot to add it to the test mapping.

### Your Task

1. **Read the test file:**
   - Open `/home/jared/Code/digibyte/src/test/transaction_tests.cpp`
   - Find the `mapFlagNames` declaration (around line 48)
   - See the pattern of flag mappings

2. **Find the flag definition:**
   - The flag is defined in `/home/jared/Code/digibyte/src/script/interpreter.h`
   - Search for `SCRIPT_VERIFY_DIGIDOLLAR`

3. **Add the mapping:**
   - Add this line to `mapFlagNames`:
   ```cpp
   {std::string("DIGIDOLLAR"), (unsigned int)SCRIPT_VERIFY_DIGIDOLLAR},
   ```
   - Insert it after line 68 (after "DISCOURAGE_UPGRADABLE_PUBKEYTYPE")
   - Maintain alphabetical/logical ordering
   - Follow exact formatting of other entries

4. **Verify the fix:**
   ```bash
   # Compile
   make -j$(nproc)

   # Run the specific failing test
   ./src/test/test_digibyte --run_test=transaction_tests/tx_valid

   # Run ALL tests to ensure no regressions
   ./src/test/test_digibyte
   ```

5. **Expected outcome:**
   - Test `transaction_tests/tx_valid` passes ✅
   - ALL 1,258 tests pass ✅
   - No warnings ✅

### What NOT to Do

- ❌ Do not modify any other flags in the map
- ❌ Do not change the `CheckMapFlagNames()` function
- ❌ Do not modify `STANDARD_SCRIPT_VERIFY_FLAGS` (it's already correct)
- ❌ Do not refactor or "improve" unrelated code
- ❌ Do not add flags that don't exist
- ❌ Do not change test expectations

### Success Criteria

✅ Test `transaction_tests/tx_valid` passes
✅ All 1,257 other tests still pass
✅ Total: 1,258 / 1,258 tests passing (100%)
✅ No compiler warnings
✅ Code follows existing style

### If You Encounter Issues

**Issue:** Can't find SCRIPT_VERIFY_DIGIDOLLAR definition
**Solution:** Search in `/home/jared/Code/digibyte/src/script/interpreter.h`

**Issue:** Test still fails after adding mapping
**Solution:** Verify spelling is exact: "DIGIDOLLAR" (not "DIGI_DOLLAR" or "DD")

**Issue:** Other tests start failing
**Solution:** You likely modified the wrong thing - revert and try again

---

**IMPORTANT:** This is a trivial fix. If it takes more than 15 minutes, you're overthinking it. Just add the one line and run the tests.
```

---

## Progress Tracking

### Workflow Stages

1. **Analysis Phase** ✅ COMPLETE
   - Identified failing test: `transaction_tests/tx_valid`
   - Found root cause: Missing flag in `mapFlagNames`
   - Confirmed fix scope: Single line addition
   - Verified risk: LOW (simple pattern following)

2. **Sub-Agent Deployment** ⏳ PENDING
   - Deploy 1 sub-agent with instructions above
   - Monitor for completion (should be quick)
   - No conflicts expected (only 1 agent needed)

3. **Verification** ⏳ PENDING
   - Run specific test: `transaction_tests/tx_valid`
   - Run full test suite: all 1,258 tests
   - Check for warnings
   - Verify 100% pass rate

4. **Completion** ⏳ PENDING
   - Report success to user
   - Document the fix
   - Close task

---

## Communication with User

### Status Report Format

```markdown
## Unit Test Fix - Status Report

**Overall Status:** [In Progress / Complete]
**Tests Passing:** 1,258 / 1,258 ✅
**Tests Failing:** 0 ✅

### What Was Fixed
- ✅ `transaction_tests/tx_valid` - Added SCRIPT_VERIFY_DIGIDOLLAR flag mapping

### Sub-Agent Activity
- Agent 1: Code Fix Specialist - [Status]
  - Task: Add missing DIGIDOLLAR flag to mapFlagNames
  - File: `src/test/transaction_tests.cpp`
  - Change: Added single line mapping
  - Result: Test now passes ✅

### Verification Results
- **Pre-fix:** 1,257 passing, 1 failing
- **Post-fix:** 1,258 passing, 0 failing ✅
- **Regression check:** No tests broken ✅
- **Compilation:** No warnings ✅

### Summary
All unit tests are now passing (100% success rate). The fix was a simple addition of a flag mapping that was inadvertently omitted when DigiDollar script verification was added.

**Ready for review and next steps.**
```

---

## Risk Assessment

### This Fix is LOW RISK

✅ **Why this fix is safe:**
- Simple addition, not modification
- Follows existing pattern exactly
- Only affects test code, not production code
- Easy to verify
- Easy to revert if needed
- No logic changes
- No dependencies

### Verification Strategy

**Test Progression:**
1. Run failing test specifically
2. Confirm it now passes
3. Run entire test suite
4. Confirm 100% pass rate
5. Check for any new warnings
6. Done!

---

## Timeline Estimate

**Expected Duration:** 10-15 minutes total

- Sub-agent deployment: 1 minute
- Sub-agent analysis: 2 minutes
- Code modification: 1 minute
- Compilation: 2 minutes
- Test execution: 3-5 minutes
- Verification: 2 minutes
- Reporting: 1 minute

**Total:** Well under 30 minutes to achieve 100% test pass rate!

---

## Success Criteria

### Definition of Done ✅

All of these must be TRUE:

1. ✅ Test `transaction_tests/tx_valid` passes
2. ✅ All 1,258 unit tests passing (100% pass rate)
3. ✅ No compiler warnings
4. ✅ No new test failures introduced
5. ✅ Code follows existing style
6. ✅ Fix documented
7. ✅ User notified of completion

**Only when ALL criteria met:** Mission Complete ✅

---

## Notes

- This is the **easiest** possible unit test fix
- 99.92% pass rate shows **excellent** code quality
- The fix is **trivial** - just following a pattern
- Success is **guaranteed** if instructions followed
- **No application bugs** - this is just a test mapping

---

**Current Priority:** Deploy sub-agent NOW to fix this test and achieve 100% pass rate!
