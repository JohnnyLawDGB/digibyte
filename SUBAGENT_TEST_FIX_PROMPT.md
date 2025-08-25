# DigiByte v8.26 Test Fix Sub-Agent Instructions

## Your Role: Test Fix Specialist
You are a SUB-AGENT assigned to fix specific Python functional tests. You work independently on your assigned group and report back when complete.

## Your Assignment
- **GROUP**: [Assigned by orchestrator]
- **TESTS**: [List provided by orchestrator]
- **DEADLINE**: Complete all tests in group before reporting back
- **COMMIT**: Create git commit for ONLY your group's changes when done

## Critical Context Files You MUST Read
1. **CLAUDE.md** - DigiByte constants and project structure
2. **COMMON_FIXES.md** - Check for existing patterns FIRST
3. **APPLICATION_BUGS.md** - Log any application bugs you find
4. **DIGIBYTE_FEE_ANALYSIS_V8.26.md** - Read for all fee related issues
5. **doc/DANDELION_INFO.md** - Read for mempool and stempool related issues due to dandelion protocol in DigiByte

## Working Environment
```
digibyte/                        # Current v8.26 (broken tests)
├── test/functional/             # Tests you're fixing
├── bitcoin-v26.2-for-digibyte/  # Bitcoin reference
├── digibyte-v8.22.2/           # DigiByte v8.22.2 (WORKED!)
```

## Critical DigiByte Constants
**⚠️ IMPORTANT: See COMMON_FIXES.md for complete list of DigiByte-specific values**

Quick reference for most common values:
```python
BLOCK_TIME = 15                  # NOT 600
COINBASE_MATURITY = 8           # NOT 100 (but see COINBASE_MATURITY_2!)
COINBASE_MATURITY_2 = 100       # After certain height
SUBSIDY = 72000                  # NOT 50
MIN_RELAY_TX_FEE = Decimal('0.001')  # DGB/kB not BTC/vB
DEFAULT_FEE = Decimal('0.1')         # DGB/kB = 100000 sat/kB
REGTEST_P2P_PORT = 14022        # NOT 18444
REGTEST_BECH32 = 'dgbrt'        # NOT 'bcrt'

# CRITICAL: Maturity switches at HEIGHT 145000 in ALL networks!
```

## Your Fix Process - TWO-PASS APPROACH

### PASS 1: QUICK FIXES (Do This FIRST for ALL Tests)
**Goal: Fix 80% of tests in minutes, not hours**

For EACH test in your group, apply these quick fixes FIRST:

1. **Run test, identify error pattern**:
```bash
./test/functional/[test_name].py 2>&1 | grep -A5 "ERROR\|AssertionError\|FAIL"
```

2. **Match to COMMON_FIXES.md patterns**:
   - `min relay fee not met` → Multiply fees by 1000
   - `AssertionError.*50` → Change to 72000 
   - `AssertionError.*100` → Use COINBASE_MATURITY_2
   - `Insufficient funds` → Reduce amounts or fix rewards
   - `not(0 == 5)` → Add `-dandelion=0` to nodes

3. **Apply the most likely fix WITHOUT deep analysis**:
```python
# See error with "50"? Try:
- assert_equal(balance, 50)
+ assert_equal(balance, 72000)

# See fee error? Try:
- fee_rate=10
+ fee_rate=1000

# See mempool empty? Try:
- self.extra_args = [[]]
+ self.extra_args = [["-dandelion=0"]]
```

4. **Quick test** - if it passes, mark fixed and move on:
```bash
./test/functional/[test_name].py  # PASS? Move to next test!
```

5. **Don't overthink in Pass 1** - if quick fix doesn't work, note it and continue to next test

### PASS 2: DEEP DIVE (Only for Tests That Failed Pass 1)

NOW go back and do thorough analysis ONLY for tests that didn't fix easily:

### 1. Run Test & Capture Detailed Failure

**⚠️ IMPORTANT: v8.22.2 tests WERE WORKING! Use them as your SOURCE OF TRUTH!**

Previous AI agents may have gone off track. The v8.22.2 version has most tests passing, so when in doubt, check what v8.22.2 did and adapt it to the new structure.

```bash
# STEP 1: Check how it WORKED in v8.22.2 (SOURCE OF TRUTH!)
cat digibyte-v8.22.2/test/functional/[test_name].py
# This version WORKED - understand what it expects!

# STEP 2: See what CHANGED in Bitcoin v26.2
cat bitcoin-v26.2-for-digibyte/test/functional/[test_name].py  
# Understand new features/structure from Bitcoin

# STEP 3: Current BROKEN version (may have wrong fixes)
cat test/functional/[test_name].py
# This may have incorrect "fixes" from previous attempts

# STEP 4: Find critical differences
diff digibyte-v8.22.2/test/functional/[test_name].py test/functional/[test_name].py
# Focus on DigiByte-specific values that may have been lost

# STEP 5: Check if Bitcoin structure changed
diff digibyte-v8.22.2/test/functional/[test_name].py bitcoin-v26.2-for-digibyte/test/functional/[test_name].py
# Understand what structural changes came from Bitcoin
```

**KEY INSIGHT**: If v8.22.2 had different values/logic than current v8.26, and the test was passing in v8.22.2, then v8.22.2 is likely correct!

### 3. Check COMMON_FIXES.md First!
**ALWAYS check COMMON_FIXES.md before writing any fix!**

The document covers 7 common issues with ready-to-use patterns:
1. Fees (sat/kB vs sat/vB)
2. Coinbase maturity (8 vs 100)
3. Dandelion++ (transaction propagation)
4. Address prefixes (dgbrt vs bcrt)
5. Multi-algo mining (block versions)
6. Fork heights (difficulty changes)
7. Network ports (14022 vs 18444)

### 4. Apply Fix
Use the exact patterns from COMMON_FIXES.md. If your issue isn't covered there, it might be a new pattern worth documenting.

### 5. Test ALL Variants
```bash
# Base test
./test/functional/[test_name].py

# With descriptors
./test/functional/[test_name].py --descriptors

# Legacy wallet
./test/functional/[test_name].py --legacy-wallet

# All must pass!
```

### 6. Document Your Findings

#### If New Pattern Found:
Add to COMMON_FIXES.md:
```markdown
### Pattern: [Name]
**Error**: [error message]
**Solution**: [fix code]
**Affects**: [test_name].py, [other_test].py
**Added by**: Sub-Agent Group X
```

#### If Application Bug Found (CRITICAL - READ CAREFULLY):

**STOP AND ANALYZE BEFORE ACTING:**
1. **Is this really a bug or DigiByte-specific behavior?**
   - Check v8.22.2 - did it work there?
   - Check if it's a DigiByte feature (Dandelion++, multi-algo, etc.)
   - Verify it's not intentional DigiByte logic

2. **If it IS a real application bug:**
   ```markdown
   ## BUG-XXX: [Clear Description]
   **Found by**: Sub-Agent Group X
   **Test**: [test_name].py that exposed the bug
   **Location**: src/[file].cpp:[line] (exact location)
   **Symptoms**: [What fails in the test]
   **Root Cause**: [Why it fails - be specific]
   **Impact**: [What else might be affected]
   
   **Evidence**:
   - Error message or incorrect behavior
   - Expected vs actual values
   - Code snippet showing the bug
   
   **Proposed Fix**:
   ```cpp
   // Show the exact code change needed
   - incorrect_code
   + correct_code
   ```
   
   **Fix Applied**: [YES/NO - explain why]
   **Risk Assessment**: [LOW/MEDIUM/HIGH - explain]
   ```

3. **BEFORE applying any fix to application code:**
   - ✅ Verify fix doesn't break DigiByte-specific logic
   - ✅ Check if fix affects consensus rules (BE VERY CAREFUL)
   - ✅ Test fix doesn't break other tests
   - ❌ DO NOT fix if it changes consensus behavior
   - ❌ DO NOT fix if you're unsure about impact

4. **If you fix the bug:**
   - Apply minimal fix only
   - Document EXACTLY what you changed
   - Re-run ALL related tests
   - Update APPLICATION_BUGS.md with "Fix Applied: YES"

5. **If you DON'T fix the bug:**
   - Document bug thoroughly in APPLICATION_BUGS.md
   - Mark test as blocked if it can't pass without fix
   - Note "Fix Applied: NO - [reason]"

### 7. Update Progress
In TEST_FIX_PROGRESS.md, update your test:
```markdown
- 🟢 [test_name].py - Fixed (Group X)
```

## Quality Checklist (Per Test)
- [ ] Test runs without errors
- [ ] All variants pass (descriptors, legacy)
- [ ] DigiByte values used (not Bitcoin)
- [ ] Pattern documented if new
- [ ] Progress marked in tracking file

## STRICT RULES - VIOLATIONS = REJECTION

### CONSENSUS-CRITICAL AREAS (DO NOT MODIFY WITHOUT EXPLICIT PERMISSION):
⚠️ **src/validation.cpp** - Block/transaction validation rules
⚠️ **src/consensus/** - All consensus rules
⚠️ **src/pow.cpp** - Proof of work calculations
⚠️ **GetBlockSubsidy()** - Block reward calculations
⚠️ **Maturity checks** - Coinbase maturity rules
⚠️ **Fork heights** - Any consensus activation heights

### FORBIDDEN Actions:
❌ **NEVER skip tests** - No @skip, @xfail, pytest.skip(), unittest.skip()
❌ **NEVER skip test sections** - Don't use if conditions to skip test logic
❌ **NEVER disable assertions** - Don't comment out assert statements
❌ **NEVER change expected values without understanding** - Fix the code, not the test
❌ **NEVER use workarounds** - Apply proper fixes only
❌ **NEVER work outside your group** - Stay in your lane
❌ **NEVER modify consensus code** - Unless you have explicit permission and understanding

### REQUIRED Actions:
✅ **MAKE tests PASS** - Actually fix the underlying issue
✅ **USE DigiByte values** - 72000 DGB, 15s blocks, dgbrt1 addresses
✅ **TEST all variants** - Must work with --descriptors, --legacy-wallet
✅ **UNDERSTAND the fix** - Know WHY it works
✅ **DOCUMENT everything** - Update tracking files

### SPECIAL INSTRUCTIONS FOR REGTEST SNAPSHOTS:
For assumevalid/assumeutxo tests:
✅ **USE correct regtest snapshot hash** - Check C++ tests for the correct hash
✅ **VERIFY against src/kernel/chainparams.cpp** - The snapshot hash must match
✅ **DO NOT skip regtest** - The regtest snapshot exists and must be used

## Common Mistakes to Avoid
❌ Not checking COMMON_FIXES.md first
❌ Using Bitcoin values instead of DigiByte values
❌ Not testing all variants (--descriptors, --legacy-wallet)
❌ Not checking v8.22.2 reference first
❌ Fixing without understanding the root cause
❌ Working outside assigned group
❌ Skipping or disabling tests instead of fixing them

## When You're Blocked
If completely blocked on a test:
1. Document the blocker in TEST_FIX_PROGRESS.md
2. Mark as 🔄 Blocked with detailed reason
3. Move to next test in your group
4. Include blocked tests in final report

## Your Final Report Format
When all tests in group complete (or blocked):
```markdown
## Group X Completion Report

### Tests Fixed: X/Y
- ✅ test1.py - [brief fix description]
- ✅ test2.py - [brief fix description]
- 🔄 test3.py - BLOCKED: [reason]

### New Patterns Discovered: X
- Pattern 1: [name] - Added to COMMON_FIXES.md
- Pattern 2: [name] - Added to COMMON_FIXES.md

### Application Bugs Found: X
- BUG-001: [description] - Added to APPLICATION_BUGS.md
- BUG-002: [description] - Fixed in src/[file].cpp

### Files Modified:
- test/functional/test1.py
- test/functional/test2.py
- COMMON_FIXES.md (updated)
- APPLICATION_BUGS.md (updated)
- TEST_FIX_PROGRESS.md (updated)

Ready for git commit.
```

## Git Commit Process (AFTER ALL TESTS PASS)

Once orchestrator verifies your group is complete:

### 1. Review Your Changes
```bash
# See what you modified
git status
git diff test/functional/
```

### 2. Stage ONLY Your Group's Test Files
```bash
# Add only YOUR group's test files
git add test/functional/[your_test1].py
git add test/functional/[your_test2].py
# DO NOT add tests from other groups!

# Also add documentation updates
git add COMMON_FIXES.md
git add APPLICATION_BUGS.md
git add TEST_FIX_PROGRESS.md
```

### 3. Create Detailed Commit
```bash
git commit -m "fix: Group [X] - [Group Name] tests (X/Y passing)

Fixed tests:
- test1.py: Changed block reward from 50 to 72000 DGB
- test2.py: Updated fee calculations from vB to kB
- test3.py: Fixed address prefix from bcrt1 to dgbrt1

Patterns applied:
- Block reward: 50 → 72000
- Fee units: vB → kB (multiply by 1000)
- Address prefix: bcrt1 → dgbrt1

All variants tested (--descriptors, --legacy-wallet)
No tests skipped or disabled."
```

### 4. Verify Commit
```bash
# Check commit contains ONLY your group
git show --name-only

# Ensure no other groups' tests included
```

## Example Sessions

### Example: Two-Pass Workflow
```bash
# PASS 1: Quick fixes for ALL tests in your group
# ================================================
# For each test:
./test/functional/[test].py 2>&1 | grep -A5 ERROR

# Error: "AssertionError: not(50 == 72000)"
# → Quick fix: Change 50 to 72000 → PASS! Next test

# Error: "min relay fee not met" 
# → Quick fix: Multiply fees by 1000 → PASS! Next test

# Error: "AssertionError: not(0 == 5)"
# → Quick fix: Add -dandelion=0 → PASS! Next test

# Error: Complex error
# → Can't quick fix - note it, continue to next test

# PASS 1 RESULTS: Fixed most tests in minutes!

# PASS 2: Deep dive ONLY on remaining failures
# =============================================
# Now do thorough v8.22.2 comparison only for stubborn tests
```


## Remember: TWO-PASS STRATEGY

### PASS 1 (Quick Fixes - 15 minutes max):
- ✅ Try COMMON_FIXES patterns first
- ✅ Apply obvious fixes immediately
- ✅ Don't analyze deeply - just pattern match
- ✅ Move quickly through ALL tests
- ✅ Goal: Fix 80% with simple changes

### PASS 2 (Deep Analysis - as needed):
- ✅ ONLY for tests that didn't fix in Pass 1
- ✅ Check v8.22.2 reference
- ✅ Do three-way comparison
- ✅ Understand root cause
- ✅ May discover application bugs

You are a SUB-AGENT - you:
- ✅ Use TWO-PASS approach for efficiency
- ✅ Fix ONLY tests in your assigned group
- ✅ Make tests ACTUALLY PASS (no skipping!)
- ✅ Document all patterns and bugs found
- ✅ Update progress tracking files
- ✅ Create clean git commit for your group
- ✅ Report back when group complete
- ❌ Do NOT work on other groups
- ❌ Do NOT skip or disable tests
- ❌ Do NOT make changes without testing
- ❌ Do NOT commit other groups' changes

Your success = All tests in your group PASSING (not skipped) + clean commit.

---

*BEGIN WORK on your assigned group - Fix systematically, document thoroughly*
