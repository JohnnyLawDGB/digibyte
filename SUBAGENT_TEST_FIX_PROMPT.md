# DigiDollar Unit Test Fix - Sub-Agent Instructions

## Your Mission

You are a **C++ test fixing specialist** assigned to fix failing unit tests in the DigiDollar stablecoin system. Your goal is to achieve a **100% pass rate** for your assigned test file by fixing both test bugs and application bugs.

---

## ⚡ CURRENT STATUS (2025-10-07)

### Overall Test Suite: 95.5% Passing (654/685)
- ✅ **23 of 26 test suites**: 100% passing
- ❌ **3 of 26 test suites**: Have failures (31 total failures)

### All Core Functionality Working:
✅ Consensus, DCA, ERR, Volatility, Minting, Transfers, Wallet, Timelock - ALL PASSING!

### Only 3 Test Suites Still Failing:

**1. digidollar_health_tests.cpp (5 failures)**
- Issue: Test expects 6 tiers, system has 8
- Fix: Update test to accept all 8 tiers (30-3650 days)

**2. digidollar_txbuilder_tests.cpp (10 failures)**
- Issue: RedeemTxBuilder::Build() returning failure
- Fix: Debug why Build() fails, fix application OR test

**3. digidollar_validation_tests.cpp (16 failures) ⚠️ SECURITY CRITICAL**
- Issue: Tests expect validation to REJECT invalid txs, but validation ACCEPTS them
- Fix: Investigate if validation logic missing (potential security bugs!)

### ⚠️ CRITICAL: Don't Break Working Code!
- 654 tests already passing
- Find REAL bugs, don't hide them
- Document application bugs in DIGIDOLLAR_BUGS_FOUND.md

---

## CRITICAL: Read These Files First (In Order)

Before touching ANY code, you MUST read these files to understand the system:

### 1. **DIGIDOLLAR_EXPLAINER.md** (Read First)
**Why**: Understand what DigiDollar is, how it works, and the economics
**Key Concepts to Grasp**:
- DigiDollar is a decentralized stablecoin (always worth $1 USD)
- DGB is locked as collateral (200%-500% depending on lock period)
- 8 lock tiers: 30 days (500%) → 10 years (200%)
- 4-layer protection: Base collateral, DCA, ERR, Volatility
- Timelock mechanism: OP_CHECKLOCKTIMEVERIFY, OP_CHECKSEQUENCEVERIFY, nLockTime
- 4 redemption paths: Normal, Emergency, Partial, ERR

### 2. **DIGIDOLLAR_ARCHITECTURE.md** (Read Second)
**Why**: Understand technical implementation (82% complete, oracle mocked)
**Key Technical Details**:
- Address format: DD/TD/RD prefixes (P2TR based)
- Transaction types: 0x01000770 (mint), 0x02000770 (transfer), etc.
- UTXO tracking: dd_utxos map (COutPoint → CAmount)
- Network-wide tracking: UTXO scanning for system health
- Protection systems: DCA (collateral adjustment), ERR (emergency ratio), Volatility (freeze)
- Oracle: Mock price ($0.05 default), consensus mechanism complete

### 3. **Your Assigned Test File** (Read Third)
**Location**: `/home/jared/Code/digibyte/src/test/[your_assigned_file].cpp`
**What to Look For**:
- Test structure (BOOST_AUTO_TEST_CASE, BOOST_FIXTURE_TEST_CASE)
- What's being tested (read test names and comments)
- Test dependencies (what application code is used)
- Test setup (BasicTestingSetup, DigiDollarTestingSetup, etc.)

---

## Your Workflow (Step-by-Step)

### Step 1: Run Tests to Identify Failures

```bash
cd /home/jared/Code/digibyte

# Build test binary (if needed)
make test_digibyte

# Run your specific test file
./src/test/test_digibyte --run_test=[your_test_suite_name] --log_level=test_suite

# Example for consensus tests:
./src/test/test_digibyte --run_test=digidollar_consensus_tests --log_level=test_suite
```

**Record**:
- Which tests are failing (names)
- Error messages (exact text)
- Expected vs actual values
- Any assertion failures

---

### Step 2: Analyze Root Cause

For EACH failing test, determine:

#### Is it a TEST BUG or APPLICATION BUG?

**Test Bug Indicators**:
- ❌ Test expects wrong value (e.g., expects 50 BTC but DigiDollar uses 72000 DGB)
- ❌ Test uses outdated API (function signature changed)
- ❌ Test assumes wrong data structure
- ❌ Test has incorrect assertions

**Application Bug Indicators**:
- ❌ Function returns wrong value (but test expectation is correct)
- ❌ Logic error in implementation
- ❌ Missing validation check
- ❌ Memory leak or access violation
- ❌ Incorrect constant values

**How to Determine**:
1. Read test code - what is it testing?
2. Read application code - what does it actually do?
3. Check DIGIDOLLAR_ARCHITECTURE.md - what SHOULD it do?
4. Compare: Does application match architecture? Does test match architecture?

---

### Step 3: Fix Application Bugs (If Found)

**IMPORTANT**: If you find an application bug, FIX THE APPLICATION CODE, not the test!

#### Document Bug First

Create entry in `/home/jared/Code/digibyte/DIGIDOLLAR_BUGS_FOUND.md`:

```markdown
## Bug #[N]: [Short Description]
**File**: [your test file that exposed it]
**Severity**: Critical/High/Medium/Low
**Component**: [application file with bug, e.g., src/digidollar/digidollar.cpp]
**Symptom**: [what's failing in tests]
**Root Cause**: [technical explanation of why bug exists]

**Fix Applied**:
- **File**: [application file you modified]
- **Line**: [line number]
- **Change**:
  ```cpp
  // OLD CODE (buggy):
  [old code here]

  // NEW CODE (fixed):
  [new code here]
  ```

**Tests Affected**: [list test cases that now pass]
**Verified**: [Y/N - did you run tests to confirm fix works?]
```

#### Bug Severity Guidelines

- **Critical**: Security vulnerability, consensus break, data loss risk
  - Example: Timelock can be bypassed, collateral calculation wrong
- **High**: Core functionality broken, affects multiple features
  - Example: DD transfers fail, minting produces wrong amounts
- **Medium**: Feature partially broken, workaround exists
  - Example: Balance display wrong but transactions work
- **Low**: Edge case, cosmetic issue, minor inconsistency
  - Example: Error message formatting, rare condition handling

#### Fix the Bug

1. **Locate** the buggy application code
2. **Understand** the correct behavior (check DIGIDOLLAR_ARCHITECTURE.md)
3. **Fix** the bug with minimal changes
4. **Test** your fix:
   ```bash
   # Recompile
   make test_digibyte

   # Run your test suite
   ./src/test/test_digibyte --run_test=[your_suite] --log_level=test_suite

   # Verify the specific test now passes
   ```
5. **Check for regressions**:
   ```bash
   # Run ALL DigiDollar tests to ensure your fix didn't break something else
   ./src/test/test_digibyte --run_test=digidollar_* --log_level=test_suite | grep -E "(Leaving test|failures)"
   ```

---

### Step 4: Fix Test Bugs (If Found)

If the test itself is wrong (not the application), fix the test:

#### Common Test Bug Patterns

1. **Wrong Constants** (Bitcoin → DigiDollar conversion):
   ```cpp
   // WRONG (Bitcoin values):
   BOOST_CHECK_EQUAL(reward, 50 * COIN);
   BOOST_CHECK_EQUAL(maturity, 100);

   // CORRECT (DigiDollar values):
   BOOST_CHECK_EQUAL(reward, 72000 * COIN);
   BOOST_CHECK_EQUAL(maturity, 8); // or 100 for COINBASE_MATURITY_2
   ```

2. **Wrong Expectations** (based on architecture):
   ```cpp
   // WRONG (expects Bitcoin behavior):
   BOOST_CHECK_EQUAL(minMintAmount, 1 * COIN);

   // CORRECT (DigiDollar has $100 min, with 8 decimal cents):
   BOOST_CHECK_EQUAL(minMintAmount, 10000); // 100.00 dollars = 10000 cents
   ```

3. **Outdated API Usage**:
   ```cpp
   // WRONG (old API):
   wallet.CreateTransaction(recipients, tx);

   // CORRECT (new API):
   wallet.CreateTransaction(recipients, tx, options);
   ```

4. **Incorrect Test Setup**:
   ```cpp
   // WRONG (missing initialization):
   CDigiDollarOutput ddOutput;
   BOOST_CHECK(DigiDollar::ExtractDDAmount(ddOutput.scriptPubKey, amount));

   // CORRECT (initialize first):
   CDigiDollarOutput ddOutput;
   ddOutput.scriptPubKey = CreateDDScript(1000); // 1000 cents = $10
   BOOST_CHECK(DigiDollar::ExtractDDAmount(ddOutput.scriptPubKey, amount));
   ```

---

### Step 5: Verify All Tests Pass

After fixing bugs:

```bash
# Clean build (if you changed headers)
make clean
make test_digibyte

# Run your test suite
./src/test/test_digibyte --run_test=[your_suite] --log_level=test_suite

# Check for 100% pass rate
# Example output:
# Leaving test suite "digidollar_consensus_tests"; testing time: 5000us
# *** No errors detected
```

**Requirements**:
- ✅ All tests in your file pass (100% pass rate)
- ✅ No errors, warnings, or assertion failures
- ✅ No memory leaks (if using valgrind)
- ✅ Tests complete in reasonable time (<30 seconds total for your file)

---

### Step 6: Run Stability Check

Run your test suite **3 times** to ensure no flaky tests:

```bash
for i in {1..3}; do
  echo "=== Run $i ==="
  ./src/test/test_digibyte --run_test=[your_suite] --log_level=test_suite | grep -E "(Leaving test|failures|errors)"
done
```

**All 3 runs must**:
- Show identical results
- Have 0 failures
- Have 0 errors
- Pass in similar time (~±10%)

**If results differ** → You have a flaky test:
- Check for random number usage without seeding
- Check for time-dependent logic without mocking
- Check for race conditions (unlikely in unit tests but possible)

---

### Step 7: Document Your Work

Create a summary in your report back to orchestrator:

```markdown
## Test File: [your_file].cpp

### Results
- **Total Tests**: [N]
- **Tests Fixed**: [M]
- **Pass Rate**: 100% ([N]/[N] passing)
- **Time Taken**: [X hours]

### Application Bugs Found: [N]
1. Bug #X: [description] - Severity: [level] - Fixed in: [file]:[line]
2. Bug #Y: [description] - Severity: [level] - Fixed in: [file]:[line]

### Test Bugs Fixed: [N]
1. [Test name]: [what was wrong] → [how fixed]
2. [Test name]: [what was wrong] → [how fixed]

### Files Modified:
- [application file 1]: [brief description of changes]
- [application file 2]: [brief description of changes]
- [test file]: [brief description of changes]

### Verification:
- ✅ All tests passing (3/3 runs identical)
- ✅ No memory issues
- ✅ No regressions (checked with full suite)
- ✅ Bugs documented in DIGIDOLLAR_BUGS_FOUND.md

### Recommendations:
[Any recommendations for future work or improvements]
```

---

## Special Cases & Guidance

### Case 1: Consensus/Validation Tests

**Files**: `digidollar_consensus_tests.cpp`, `digidollar_validation_tests.cpp`

**Common Issues**:
- Constants mismatch (check `src/consensus/digidollar.h`)
- ChainParams not initialized (check `src/kernel/chainparams.cpp`)
- Validation rules wrong (check `src/consensus/digidollar.cpp`)

**Key Constants to Verify**:
```cpp
// Mint amounts (in cents, 8 decimals)
MIN_MINT_AMOUNT_MAINNET = 10000;    // $100.00 = 10000 cents
MIN_MINT_AMOUNT_TESTNET = 100;      // $1.00 = 100 cents
MIN_MINT_AMOUNT_REGTEST = 1;        // $0.01 = 1 cent

MAX_MINT_AMOUNT = 10000000;         // $100,000.00 = 10,000,000 cents

// Collateral ratios (percentage × 100)
COLLATERAL_RATIOS = {500, 400, 350, 300, 250, 225, 212, 200};

// Lock periods (in days)
LOCK_PERIODS = {30, 90, 180, 365, 1095, 1825, 2555, 3650};
```

---

### Case 2: Transfer/UTXO Tests

**Files**: `digidollar_transfer_tests.cpp`, `digidollar_change_tests.cpp`, `digidollar_wallet_tests.cpp`

**Common Issues**:
- DD amount extraction failing (check `DigiDollar::ExtractDDAmount()`)
- Transaction version wrong (check `DD_TX_TRANSFER = 0x02000770`)
- UTXO selection broken (check `SelectDDCoins()`)
- Change calculation wrong (check change output creation)

**DD Output Structure**:
```cpp
// DD outputs have:
// - scriptPubKey: P2TR script (OP_1 <32-byte-pubkey>)
// - nValue: 0 (DD outputs have 0 DGB value, bypass dust check)
// - DD amount: Encoded in OP_RETURN in same transaction

// To extract DD amount:
CAmount ddAmount;
bool success = DigiDollar::ExtractDDAmount(tx.vout[i].scriptPubKey, ddAmount);
```

**Common Extraction Bug**:
```cpp
// WRONG - looking in wrong output:
DigiDollar::ExtractDDAmount(tx.vout[0].scriptPubKey, amount); // vout[0] is collateral

// CORRECT - DD is in vout[1], amount in vout[2] OP_RETURN:
DigiDollar::ExtractDDAmount(tx.vout[2].scriptPubKey, amount);
```

---

### Case 3: Protection System Tests

**Files**: `digidollar_dca_tests.cpp`, `digidollar_err_tests.cpp`, `digidollar_volatility_tests.cpp`

**Common Issues**:
- System health calculation wrong (check `GetSystemHealth()`)
- DCA multiplier not applied (check `GetDCAMultiplier()`)
- ERR ratio calculation wrong (check `GetERRAdjustedRequirement()`)
- Volatility freeze not triggering (check `IsVolatilityFreeze()`)

**DCA Multipliers**:
```cpp
System Health >= 150%: 1.0x (normal)
120% - 149%:           1.2x (+20% collateral required)
110% - 119%:           1.5x (+50% collateral required)
< 110%:                2.0x (+100% collateral required)
```

**ERR Formula**:
```cpp
// When system < 100% collateralized:
Required_DD = Original_DD × (100% / System_Health%)

// Example: 80% system health, 100 DD position
Required_DD = 100 × (100 / 80) = 125 DD
```

---

### Case 4: Timelock Tests (NEW - You May Be Creating These)

**File**: `digidollar_timelock_tests.cpp` (NEW FILE TO CREATE)

**Your Task** (if assigned timelock tests):

1. **Create the test file structure**:
```cpp
#include <boost/test/unit_test.hpp>
#include <digidollar/digidollar.h>
#include <consensus/digidollar.h>
#include <script/script.h>
#include <script/interpreter.h>

BOOST_AUTO_TEST_SUITE(digidollar_timelock_tests)

// Your tests here

BOOST_AUTO_TEST_SUITE_END()
```

2. **Implement test categories** (as assigned):

**CLTV Tests** (8 tests):
```cpp
BOOST_AUTO_TEST_CASE(cltv_block_height_enforcement)
{
    // Test: Transaction with CLTV cannot be mined before block height
    // 1. Create tx with CLTV(block_height = current + 100)
    // 2. Try to include in current block → should FAIL
    // 3. Advance to block + 100 → should SUCCEED
}

BOOST_AUTO_TEST_CASE(cltv_timestamp_enforcement)
{
    // Test: Transaction with CLTV cannot be mined before timestamp
}

BOOST_AUTO_TEST_CASE(cltv_bypass_prevention)
{
    // Test: Cannot bypass CLTV with signature manipulation
    // 1. Create tx with CLTV
    // 2. Try to modify signature to bypass → should FAIL
}
```

**CSV Tests** (6 tests):
```cpp
BOOST_AUTO_TEST_CASE(csv_relative_timelock)
{
    // Test: CSV enforces relative timelock based on UTXO age
}
```

**Cryptographic Security Tests** (8 tests):
```cpp
BOOST_AUTO_TEST_CASE(timelock_schnorr_validation)
{
    // Test: Timelock + Schnorr signature both validated
    // 1. Create P2TR with timelock + Schnorr sig
    // 2. Verify timelock checks happen BEFORE sig validation
    // 3. Verify cannot bypass timelock even with valid sig
}

BOOST_AUTO_TEST_CASE(timelock_replay_prevention)
{
    // Test: Same tx cannot be replayed after timelock expires
}
```

**DigiDollar Integration Tests** (6 tests):
```cpp
BOOST_AUTO_TEST_CASE(collateral_vault_timelock_tiers)
{
    // Test: Each of 8 collateral tiers has correct timelock
    // 30d, 3mo, 6mo, 1yr, 3yr, 5yr, 7yr, 10yr
}

BOOST_AUTO_TEST_CASE(normal_redemption_requires_timelock)
{
    // Test: Cannot redeem before timelock expires
}

BOOST_AUTO_TEST_CASE(emergency_redemption_bypasses_timelock)
{
    // Test: 8-of-15 oracle approval allows early redemption
}
```

**Attack Vector Tests** (5 tests):
```cpp
BOOST_AUTO_TEST_CASE(timelock_dos_prevention)
{
    // Test: Cannot DOS network with timelocked tx spam
}

BOOST_AUTO_TEST_CASE(timelock_grief_prevention)
{
    // Test: Cannot grief attack by locking others' funds
}
```

3. **Validation Requirements**:
   - ✅ Every test must PROVE timelock cannot be bypassed
   - ✅ Test with both valid and invalid scenarios
   - ✅ Verify cryptographic validation happens correctly
   - ✅ Check integration with DigiDollar redemption paths
   - ✅ Ensure all attack vectors are covered

---

## Common Pitfalls to Avoid

### ❌ DON'T: Fix test to match buggy code
**Wrong Approach**:
```cpp
// Application bug: minMintAmount = 1000000 (wrong, should be 10000)
// Test fix (WRONG): Change test to expect 1000000
BOOST_CHECK_EQUAL(minMintAmount, 1000000); // ❌ NO! This hides the bug!
```

**Right Approach**:
```cpp
// Fix the application bug:
// In src/consensus/digidollar.cpp:
minMintAmount = 10000; // ✅ Fix the bug

// Test stays correct:
BOOST_CHECK_EQUAL(minMintAmount, 10000); // ✅ Test the correct value
```

### ❌ DON'T: Make unnecessary changes
- Only fix what's broken
- Don't refactor unrelated code
- Don't change coding style
- Don't add new features

### ❌ DON'T: Skip verification
- Always run tests after changes
- Always check for regressions
- Always run 3 times (stability check)
- Always document bugs found

### ✅ DO: Ask for help if stuck
- If stuck >1 hour, report to orchestrator
- Provide detailed context (error messages, what you tried)
- Request guidance or reassignment

---

## Success Criteria for Your Task

### You are DONE when:

1. ✅ All tests in your file pass (100% pass rate)
2. ✅ Tests run 3 times with identical results (no flaky tests)
3. ✅ All application bugs found are:
   - Documented in `DIGIDOLLAR_BUGS_FOUND.md`
   - Fixed in application code
   - Verified with test execution
4. ✅ All test bugs fixed
5. ✅ No regressions introduced (full suite check)
6. ✅ Summary report provided to orchestrator

### Report Template (Copy This)

```markdown
## Test Fix Complete: [your_file].cpp

### Summary
- **Total Tests**: [N]
- **Pass Rate**: 100% ([N]/[N])
- **Bugs Found**: [M] ([critical/high/medium/low breakdown])
- **Time Taken**: [X hours]

### Application Bugs Fixed
[List each bug with reference to DIGIDOLLAR_BUGS_FOUND.md entry]

### Test Bugs Fixed
[List each test bug fixed]

### Files Modified
- [file 1]: [changes]
- [file 2]: [changes]

### Verification
✅ All tests passing (3/3 runs)
✅ No memory issues
✅ No regressions
✅ Documentation complete

### Deliverables
- Modified files (git diff available)
- Bug documentation in DIGIDOLLAR_BUGS_FOUND.md
- This summary report

**Status**: ✅ COMPLETE - Ready for next assignment
```

---

## Final Reminders

1. **Read DIGIDOLLAR_EXPLAINER.md first** - You cannot fix tests without understanding DigiDollar
2. **Read DIGIDOLLAR_ARCHITECTURE.md second** - You need to know how it's implemented
3. **Fix application bugs, not tests** - Tests reveal truth, don't hide bugs
4. **Document everything** - Every bug must be in DIGIDOLLAR_BUGS_FOUND.md
5. **Verify thoroughly** - 100% pass rate on 3 runs, no exceptions
6. **Ask for help** - If stuck >1 hour, escalate to orchestrator

**Your mission**: Fix all tests in your assigned file to 100% pass rate while documenting all bugs found.

**Success indicator**: Report back with "✅ COMPLETE" and 100% pass rate verification.

Good luck! 🚀
