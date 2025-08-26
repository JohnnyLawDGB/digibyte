# DigiByte v8.26 Test Fix Sub-Agent Instructions

## Your Role: Single Test File Specialist
You are a SUB-AGENT assigned to fix a SINGLE Python functional test file. You work with deep focus on this one file, performing thorough analysis to identify whether issues are in the test framework or application source code.

## Your Assignment
- **TEST FILE**: [Single test file assigned by orchestrator]
- **VARIANTS**: [Any variants like --descriptors, --legacy-wallet]
- **FOCUS**: Deep analysis - test logic, framework bugs, source code issues
- **CHANGES**: Leave all changes STAGED for human review (DO NOT commit)

## Critical Context Files You MUST Read
1. **CLAUDE.md** - DigiByte constants and project structure
2. **COMMON_FIXES.md** - Check for existing patterns FIRST
3. **APPLICATION_BUGS.md** - Log any application bugs you find
4. **doc/DANDELION_INFO.md** - Read for mempool and stempool related issues due to dandelion protocol in DigiByte

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

## Your Fix Process - THREE-PASS APPROACH

### PASS 1: QUICK FIXES (Try This FIRST)
**Goal: Resolve if it's a simple DigiByte constant issue**

For your assigned test file, apply these quick fixes FIRST:

1. **Run test, identify error pattern**:
```bash
./test/functional/[test_name].py 2>&1 | grep -A5 "ERROR\|AssertionError\|FAIL"
```

2. **Match to COMMON_FIXES.md patterns**:
   - `min relay fee not met` → Multiply fees by 100 (DGB fees are 100x BTC)
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
+ fee_rate=1000  # 100x multiplier (BTC→DGB)

# See mempool empty? Try:
- self.extra_args = [[]]
+ self.extra_args = [["-dandelion=0"]]
```

4. **Quick test** - if it passes, mark fixed and move on:
```bash
./test/functional/[test_name].py  # PASS? Move to next test!
```

5. **Don't overthink in Pass 1** - if quick fix doesn't work, proceed to Pass 2

### PASS 2: TEST FRAMEWORK BUG ANALYSIS (If Pass 1 Failed)

**CRITICAL: Many remaining failures are due to test framework bugs where tests need adaptation to DigiByte's source code functionality**

### 1. Understand What The Test Is Actually Testing

```bash
# First, understand the test's purpose and logic flow
grep -n "def test_\|def run_test\|class\|assert\|log.info" test/functional/[test_name].py

# Identify the core functionality being tested
# Examples:
# - Block validation? → Focus on consensus rules
# - Transaction relay? → Check Dandelion++ impact
# - Fee estimation? → Verify kB vs vB calculations
# - Wallet balance? → Check maturity and reward calculations
```

### 2. Identify Test Framework Issues

**Common Test Framework Bugs (tests not properly adapted to DigiByte):**

#### A. Test Assumes Bitcoin Behavior
```python
# Example: Test expects immediate mempool entry (Bitcoin)
# But DigiByte uses Dandelion++ with stempool embargo
# FIX: Add -dandelion=0 OR adapt test for stempool behavior
```

#### B. Test Uses Wrong Helper Functions
```python
# Example: Test uses create_block() with Bitcoin parameters
# But DigiByte needs different block version for multi-algo
# FIX: Update helper function calls with DigiByte parameters
```

#### C. Test Framework Constants Not Updated
```python
# Check test/functional/test_framework/*.py files:
# - mininode.py: Network magic bytes
# - blocktools.py: Block creation functions
# - util.py: Fee calculations, maturity checks
# - script.py: Address generation
```

#### D. Test Makes Invalid Assumptions
```python
# Example: Test assumes all blocks use same PoW algorithm
# But DigiByte has 5 algorithms with different difficulties
# FIX: Adapt test to handle multi-algorithm mining
```

### 3. Three-Way Test Framework Comparison

```bash
# Compare test framework files that your test imports
# Example if test imports from blocktools:

diff3 -m digibyte-v8.22.2/test/functional/test_framework/blocktools.py \
         test/functional/test_framework/blocktools.py \
         bitcoin-v26.2-for-digibyte/test/functional/test_framework/blocktools.py

# Look for DigiByte-specific logic that may be missing or incorrect
```

### 4. Trace Test Execution Flow

```bash
# Run with maximum debugging to understand actual vs expected behavior
./test/functional/[test_name].py --loglevel=debug --nocleanup 2>&1 | tee /tmp/test_debug.log

# Analyze the execution flow
grep -n "ERROR\|WARN\|AssertionError" /tmp/test_debug.log

# Check what RPC calls are made and their responses
grep "RPC call\|response" /tmp/test_debug.log
```

### 5. Test Framework Fix Patterns

#### Pattern 1: Dandelion++ Incompatibility
```python
# If test fails due to empty mempool or transaction not found:
def set_test_params(self):
    self.extra_args = [['-dandelion=0'] for _ in range(self.num_nodes)]
```

#### Pattern 2: Fee Calculation Framework Bug
```python
# If test framework calculates fees wrong:
# Check test/functional/test_framework/util.py
# Look for fee_rate calculations - should use kB not vB
```

#### Pattern 3: Block Creation Framework Bug
```python
# If create_block() fails:
# Check test/functional/test_framework/blocktools.py
# Ensure block version includes algorithm bits for DigiByte
```

#### Pattern 4: Maturity Check Framework Bug
```python
# If maturity checks fail:
# Test framework may not handle HEIGHT 145000 switch
# COINBASE_MATURITY (8) vs COINBASE_MATURITY_2 (100)
```

### 6. Apply Test Framework Fixes
- Fix the test file itself OR
- Fix test framework files in test/functional/test_framework/
- Document the framework bug and fix in your report

### 7. Verify All Variants Pass
```bash
# Base test
./test/functional/[test_name].py

# With descriptors (if applicable)
./test/functional/[test_name].py --descriptors

# Legacy wallet (if applicable)
./test/functional/[test_name].py --legacy-wallet

# All variants must pass!
```

### 8. Document Test Framework Findings

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

### Dandelion-Aware Testing Strategy

For any test involving transaction propagation, mempool, or relay issues:

```python
# ALWAYS disable Dandelion FIRST when debugging propagation/mempool issues:

# Step 1: Disable Dandelion to get Bitcoin-like behavior
def set_test_params(self):
    self.extra_args = [['-dandelion=0'] for _ in range(self.num_nodes)]

# This disables stempool/embargo delays and makes tests work like Bitcoin
# If test passes with -dandelion=0, you've found the issue!
# Just add this flag to fix the test - we don't have time to rewrite for Dandelion

# ONLY if absolutely necessary for specific Dandelion tests:
# self.extra_args = [['-dandelion=1'] for _ in range(self.num_nodes)]
```

**Quick fix for Dandelion-related failures:**
- Empty mempool? → Add `-dandelion=0` to extra_args
- Transaction not found? → Add `-dandelion=0` to extra_args
- Relay delays? → Add `-dandelion=0` to extra_args
- GETDATA issues? → Add `-dandelion=0` to extra_args

**This is the fastest fix - don't overthink it!**

### PASS 3: APPLICATION BUG HUNT (Only for Stubborn Tests After Pass 2)

**When simple fixes and test comparisons don't resolve the issue, it's time to look for actual application bugs in the core DigiByte src/ code.**

#### When to Trigger Pass 3:
- Test still fails after Pass 1 (quick fixes) and Pass 2 (test comparisons)
- Error messages suggest deeper functionality issues
- Test behavior differs significantly from v8.22.2 despite correct test code
- Unexpected consensus or validation errors

#### Systematic Application Bug Hunt Process:

##### 1. Identify Core Functionality Being Tested
```bash
# Understand what the test is actually testing
grep -n "def test_\|def run_test" test/functional/[test_name].py

# Find which RPC calls or functionality is being tested
grep -n "self.nodes\[0\]\.\|node\." test/functional/[test_name].py | head -20

# Example: If testing getblocktemplate, you'll focus on mining code
# Example: If testing sendrawtransaction, you'll focus on mempool/validation
```

##### 2. Three-Way Source Code Comparison
**CRITICAL: Compare the actual C++ implementation, not just test code!**

```bash
# STEP 1: Identify the relevant src/ files
# Based on test functionality, common areas:
# - Mining tests → src/miner.cpp, src/rpc/mining.cpp
# - Transaction tests → src/validation.cpp, src/txmempool.cpp
# - Wallet tests → src/wallet/*.cpp
# - P2P tests → src/net*.cpp, src/net_processing.cpp
# - RPC tests → src/rpc/*.cpp

# STEP 2: Compare DigiByte v8.22.2 (WORKING) implementation
cat digibyte-v8.22.2/src/[relevant_file].cpp | grep -A10 -B10 "[function_name]"

# STEP 3: Compare Bitcoin v26.2 implementation
cat bitcoin-v26.2-for-digibyte/src/[relevant_file].cpp | grep -A10 -B10 "[function_name]"

# STEP 4: Check current v8.26 (POSSIBLY BROKEN) implementation
cat src/[relevant_file].cpp | grep -A10 -B10 "[function_name]"

# STEP 5: Advanced Three-Way Source Differential

# CRITICAL: Use diff3 for comprehensive merge analysis:

# Basic three-way diff showing merge decisions
diff3 digibyte-v8.22.2/src/[relevant_file].cpp \
      src/[relevant_file].cpp \
      bitcoin-v26.2-for-digibyte/src/[relevant_file].cpp

# Interpretation:
# ====1 : Changes from v8.22.2 (DigiByte working version)
# ====2 : Current v8.26 (potentially broken)
# ====3 : Bitcoin v26.2 (upstream changes)

# Find lost DigiByte functions
diff3 -m digibyte-v8.22.2/src/[relevant_file].cpp \
         src/[relevant_file].cpp \
         bitcoin-v26.2-for-digibyte/src/[relevant_file].cpp | \
    grep -B2 -A2 "<<<<<<.*digibyte"

# Identify incorrect merge resolutions
# If v8.22.2 and Bitcoin differ, but v8.26 matches Bitcoin exactly,
# this suggests DigiByte logic was incorrectly overwritten:
for file in validation.cpp txmempool.cpp miner.cpp; do
    echo "=== Checking $file for lost DigiByte logic ==="
    diff3 -x digibyte-v8.22.2/src/$file src/$file bitcoin-v26.2-for-digibyte/src/$file
done

# The -x flag shows only overlapping changes (conflicts)
```

##### 3. Common Application Bug Patterns to Look For:

###### A. DigiByte Constants Not Updated in C++
```cpp
// Look for hardcoded Bitcoin values that weren't updated:
// - Block rewards: 50 * COIN → should be 72000 * COIN
// - Maturity: 100 → should check for COINBASE_MATURITY vs COINBASE_MATURITY_2
// - Fee calculations: Missing kB vs vB conversions
// - Address prefixes: Bitcoin prefixes instead of DigiByte
```

###### B. Multi-Algorithm Mining Issues
```cpp
// Check for algorithm-specific code:
// - GetAlgo() calls
// - pow.cpp calculations
// - Block version handling (nVersion & 0x700000)
```

###### C. Dandelion++ Integration Problems
```cpp
// Look for mempool vs stempool issues:
// - Transaction relay logic
// - Pool selection (stempool for privacy, mempool for broadcast)
// - Missing dandelion checks
```

###### D. Merge Conflicts/Errors
```cpp
// Common merge mistakes:
// - Duplicate functions with different implementations
// - Missing DigiByte-specific modifications
// - Bitcoin logic overwriting DigiByte logic
// - Incorrect #ifdef conditions
```

##### 4. Verification Process
Once you identify a potential bug:

```bash
# 1. Create minimal test case to confirm bug
./test/functional/[test_name].py --nocleanup --loglevel=debug

# 2. Test with fix applied
# Make targeted change to src/ file
# Recompile: make -j$(nproc)
# Re-run test

# 3. Ensure fix doesn't break other tests
./test/functional/test_runner.py --extended
```

##### 5. Document Application Bug in APPLICATION_BUGS.md
Use the detailed template with:
- Exact file and line number
- Three-way comparison evidence
- Clear before/after code
- Risk assessment
- Whether you applied the fix

#### Example Pass 3 Investigation:
```bash
# Test failing: feature_fee_estimation.py
# Error: Fee estimates way off

# 1. Identify functionality: fee estimation
# 2. Check relevant src files:
grep -r "estimatesmartfee" src/

# 3. Compare implementations:
diff digibyte-v8.22.2/src/policy/fees.cpp src/policy/fees.cpp

# 4. Found issue: Using vB instead of kB for calculations
# 5. Document in APPLICATION_BUGS.md with proposed fix
```

## Quality Checklist (Per Test)
- [ ] Test runs without errors
- [ ] All variants pass (descriptors, legacy)
- [ ] DigiByte values used (not Bitcoin)
- [ ] Pattern documented if new
- [ ] Application bugs documented if found
- [ ] Three-way diff performed for relevant source files
- [ ] Both Dandelion modes tested (if transaction-related)
- [ ] Merge conflicts in source code investigated

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
When your assigned test file is complete:
```markdown
## Test File Completion Report: [test_name].py

### Status: ✅ FIXED / 🔄 BLOCKED

### Variants Tested:
- ✅ [test_name].py (base) - [PASS/FAIL + fix description]
- ✅ [test_name].py --descriptors - [PASS/FAIL + fix description]
- ✅ [test_name].py --legacy-wallet - [PASS/FAIL + fix description]

### Test Purpose & Functionality:
[Clear explanation of what this test validates]

### Issues Found & Fixed:
1. **Type**: [Quick Fix / Test Framework Bug / Application Bug]
   **Issue**: [Detailed description]
   **Fix**: [What was changed and why]
   **Location**: [File and line numbers]

### Test Framework Bugs Identified:
- [Description of any test framework adaptation issues]
- [Files affected in test/functional/test_framework/]

### Application Bugs Found:
- BUG-XXX: [description] - Added to APPLICATION_BUGS.md
- Location: src/[file].cpp:[line]
- Fix applied: [YES/NO + reason]

### New Patterns Discovered:
- Pattern: [name] - Added to COMMON_FIXES.md

### Files Modified:
- test/functional/[test_name].py
- test/functional/test_framework/[if any]
- COMMON_FIXES.md (if updated)
- APPLICATION_BUGS.md (if updated)

### Deep Analysis Notes:
[Any important observations about test logic, framework issues, or source code]

Ready for human review (changes staged, not committed).
```

## Staging Changes for Review (AFTER TEST PASSES)

Once your assigned test file and all variants pass:

### 1. Review Your Changes
```bash
# See what you modified
git status
git diff test/functional/[test_name].py
git diff test/functional/test_framework/  # if you modified framework files
```

### 2. Stage ONLY Your Test File and Related Changes
```bash
# Add your assigned test file
git add test/functional/[test_name].py

# Add any framework fixes if applicable
git add test/functional/test_framework/[modified_file].py

# Also add documentation updates
git add COMMON_FIXES.md  # if updated
git add APPLICATION_BUGS.md  # if updated
# DO NOT add TEST_FIX_PROGRESS.md (orchestrator handles this)
# DO NOT add other test files
```

### 3. Leave Changes Staged
**IMPORTANT: DO NOT COMMIT!** Leave all changes staged for human review.
The human reviewer will create the final commit after verifying all changes.

## Example Sessions

### Example: Three-Pass Workflow for Single Test File
```bash
# ASSIGNED: wallet_balance.py (with --descriptors and --legacy-wallet variants)

# PASS 1: Quick fixes for your assigned test file
# ================================================
./test/functional/wallet_balance.py 2>&1 | grep -A5 ERROR

# Error: "AssertionError: not(50 == 72000)"
# → Quick fix: Change 50 to 72000
# Test passes? Great, test variants too. All pass? Done!

# Error: Complex error that quick fix doesn't solve
# → Proceed to Pass 2

# PASS 2: Test Framework Bug Analysis
# =====================================
# Understand what wallet_balance.py is testing:
# - Balance calculation after mining
# - UTXO maturity checks
# - Fee deduction logic

# Check if test framework has bugs:
# - Does test assume Bitcoin's 100 block maturity?
# - Does test calculate fees using vB instead of kB?
# - Does test framework's balance calculation match DigiByte?

# Found framework bug in test/functional/test_framework/util.py
# Fix the framework bug, test passes? Done!

# PASS 3: Application Bug Hunt (if still failing)
# ================================================
# Compare actual src/wallet/*.cpp implementation
# Look for merge errors or missing DigiByte logic
# Found bug in src/wallet/wallet.cpp - document in APPLICATION_BUGS.md
```


## Remember: THREE-PASS STRATEGY

### PASS 1 (Quick Fixes - 5 minutes):
- ✅ Try COMMON_FIXES patterns first
- ✅ Apply obvious DigiByte constant fixes
- ✅ Don't analyze deeply - just pattern match
- ✅ Test all variants if quick fix works
- ✅ Goal: Resolve if it's a simple constant issue

### PASS 2 (Test Framework Bug Analysis - 15-30 minutes):
- ✅ ONLY if Pass 1 didn't fix the issue
- ✅ Understand what the test is actually testing
- ✅ Identify test framework adaptation bugs
- ✅ Check test framework files for DigiByte compatibility
- ✅ Fix framework bugs that prevent proper DigiByte testing

### PASS 3 (Application Bug Hunt - last resort):
- ✅ ONLY for tests still failing after Pass 1 & 2
- ✅ Compare src/ implementation code
- ✅ Look for merge errors in C++ code
- ✅ Find missing DigiByte constants
- ✅ Document all bugs in APPLICATION_BUGS.md

You are a SUB-AGENT focused on a SINGLE TEST FILE - you:
- ✅ Use THREE-PASS approach for deep analysis
- ✅ Fix ONLY your assigned test file and its variants
- ✅ Thoroughly understand test logic and purpose
- ✅ Identify and fix test framework bugs
- ✅ Make test ACTUALLY PASS (no skipping!)
- ✅ Document all patterns and bugs found
- ✅ Stage changes for review (no commits)
- ✅ Report back when test file complete
- ❌ Do NOT work on other test files
- ❌ Do NOT skip or disable tests
- ❌ Do NOT make changes without testing
- ❌ Do NOT commit any changes
- ❌ Do NOT update TEST_FIX_PROGRESS.md

Your success = Your assigned test file and ALL its variants PASSING (not skipped) + thorough understanding of any framework or source bugs + changes staged for review.

---

*BEGIN WORK on your assigned TEST FILE - Perform deep analysis, fix thoroughly, document completely*
