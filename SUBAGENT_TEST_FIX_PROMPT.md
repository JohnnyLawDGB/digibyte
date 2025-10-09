# SUBAGENT TEST FIX PROMPT - DigiDollar Unit Test Fix Methodology

## Your Mission

You are a **C++ test fixing specialist** tasked with fixing a specific failing unit test in the DigiDyte codebase. Your goal is to fix the test while protecting the 1,257 tests that are already passing.

---

## 🎯 Current Assignment

**Status:** Out of 1,258 total unit tests, only **ONE** is failing (99.92% pass rate)

**The Failing Test:**
- **Test Suite:** `transaction_tests`
- **Test Case:** `tx_valid`
- **File:** `/home/jared/Code/digibyte/src/test/transaction_tests.cpp`
- **Line:** 194
- **Error:** `mapFlagNames is missing a script verification flag`

---

## Root Cause

The DigiDollar system added a new script verification flag (`SCRIPT_VERIFY_DIGIDOLLAR`) to the standard flags, but forgot to add it to the test's flag mapping.

### What Needs to Be Done

Add this single line to the `mapFlagNames` map in `transaction_tests.cpp`:

```cpp
{std::string("DIGIDOLLAR"), (unsigned int)SCRIPT_VERIFY_DIGIDOLLAR},
```

**That's it.** This is a trivial fix.

---

## Your Workflow (Simple 5-Step Process)

### Step 1: Read the Test File

Open `/home/jared/Code/digibyte/src/test/transaction_tests.cpp` and locate:

1. **Line 48-70:** The `mapFlagNames` declaration
2. **Line 89-95:** The `CheckMapFlagNames()` function that's failing
3. **Line 194:** The assertion that fails

**Understand the pattern:**
```cpp
static std::map<std::string, unsigned int> mapFlagNames = {
    {std::string("P2SH"), (unsigned int)SCRIPT_VERIFY_P2SH},
    {std::string("STRICTENC"), (unsigned int)SCRIPT_VERIFY_STRICTENC},
    // ... more flags ...
    {std::string("DISCOURAGE_UPGRADABLE_PUBKEYTYPE"), (unsigned int)SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE},
    // ← ADD THE DIGIDOLLAR FLAG HERE
};
```

---

### Step 2: Find the Flag Definition

The flag is defined in `/home/jared/Code/digibyte/src/script/interpreter.h`.

Search for `SCRIPT_VERIFY_DIGIDOLLAR` to see its definition.

You'll find something like:
```cpp
SCRIPT_VERIFY_DIGIDOLLAR = (1U << 18), // or similar bit position
```

---

### Step 3: Add the Mapping

Edit `/home/jared/Code/digibyte/src/test/transaction_tests.cpp`:

**Add this line after line 68** (after `"DISCOURAGE_UPGRADABLE_PUBKEYTYPE"`):

```cpp
{std::string("DIGIDOLLAR"), (unsigned int)SCRIPT_VERIFY_DIGIDOLLAR},
```

**Make sure:**
- Formatting matches other entries (spacing, comma placement)
- It's placed logically (after the other DISCOURAGE flags or alphabetically)
- You don't accidentally delete or modify other lines

**Example of what it should look like:**
```cpp
static std::map<std::string, unsigned int> mapFlagNames = {
    {std::string("P2SH"), (unsigned int)SCRIPT_VERIFY_P2SH},
    // ... other flags ...
    {std::string("DISCOURAGE_UPGRADABLE_PUBKEYTYPE"), (unsigned int)SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE},
    {std::string("DIGIDOLLAR"), (unsigned int)SCRIPT_VERIFY_DIGIDOLLAR},  // ← NEW LINE
};
```

---

### Step 4: Compile and Test

```bash
# Navigate to the build directory
cd /home/jared/Code/digibyte

# Rebuild the test binary
make -j$(nproc) test_digibyte

# Run the specific failing test
./src/test/test_digibyte --run_test=transaction_tests/tx_valid --log_level=test_suite

# Expected output: *** No errors detected
```

---

### Step 5: Verify No Regressions

Run the full test suite to ensure you didn't break anything:

```bash
./src/test/test_digibyte

# Expected output at the end:
# *** No errors detected in the test module "DigiByte Core Test Suite"
#
# Final result: 1,258 tests passing, 0 failing
```

---

## What NOT to Do

### ❌ DO NOT:
1. **Modify any other flags** in `mapFlagNames`
2. **Change the `CheckMapFlagNames()` function** - it's working correctly
3. **Modify `STANDARD_SCRIPT_VERIFY_FLAGS`** in policy.h - it's already correct
4. **Refactor or "improve" other code** - only add the single line needed
5. **Add comments or documentation** beyond what's already there
6. **Change formatting or style** of existing code
7. **Add multiple flag mappings** - only DIGIDOLLAR is missing

---

## Troubleshooting

### Issue: Can't find `SCRIPT_VERIFY_DIGIDOLLAR`

**Solution:** Check these locations:
```bash
# Search for the flag definition
grep -r "SCRIPT_VERIFY_DIGIDOLLAR" src/script/

# It should be in interpreter.h
```

### Issue: Test still fails after adding the line

**Possible causes:**
1. Spelling error - verify it's exactly `"DIGIDOLLAR"` (not "DIGI_DOLLAR" or "DD")
2. Missing comma - check syntax
3. Wrong location - ensure it's inside the `mapFlagNames` map
4. Didn't rebuild - run `make clean && make test_digibyte`

### Issue: Other tests start failing

**Solution:** You likely modified the wrong thing. Revert your changes and try again:
```bash
git diff src/test/transaction_tests.cpp  # See what you changed
git checkout src/test/transaction_tests.cpp  # Revert if needed
```

### Issue: Compilation fails

**Check:**
- Did you add a comma after the previous line?
- Did you match the exact formatting?
- Did you close all brackets properly?

---

## Success Criteria

### You are DONE when:

1. ✅ Test `transaction_tests/tx_valid` passes
2. ✅ All 1,258 tests pass (100% pass rate)
3. ✅ No compiler warnings
4. ✅ Only one line was added (no other changes)
5. ✅ Code follows existing style

### Report Template

When you're done, report back with:

```markdown
## Fix Complete: transaction_tests/tx_valid

### What Was Fixed
- Added missing `DIGIDOLLAR` flag mapping to `mapFlagNames` in transaction_tests.cpp

### File Modified
- `/home/jared/Code/digibyte/src/test/transaction_tests.cpp` (line 69)
  - Added: `{std::string("DIGIDOLLAR"), (unsigned int)SCRIPT_VERIFY_DIGIDOLLAR},`

### Verification Results
- ✅ Test `transaction_tests/tx_valid` now passes
- ✅ Full test suite: 1,258 / 1,258 tests passing (100%)
- ✅ No compiler warnings
- ✅ No regressions

### Test Output
```
Running 1258 test cases...
...
*** No errors detected in the test module "DigiByte Core Test Suite"
```

**Status:** ✅ COMPLETE
```

---

## Time Estimate

This fix should take **5-10 minutes** total:
- Reading files: 2 minutes
- Finding flag definition: 1 minute
- Adding the line: 1 minute
- Compilation: 2 minutes
- Running tests: 3 minutes
- Verification: 1 minute

**If it takes longer than 15 minutes, you're overthinking it.**

---

## Important Reminders

1. **This is NOT an application bug** - it's just a missing test mapping
2. **Only add one line** - resist the urge to "improve" other things
3. **Verify thoroughly** - make sure all 1,258 tests pass
4. **Keep it simple** - follow the existing pattern exactly

---

## Example of the Exact Change

**BEFORE** (lines 67-70):
```cpp
    {std::string("DISCOURAGE_UPGRADABLE_TAPROOT_VERSION"), (unsigned int)SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_TAPROOT_VERSION},
};
```

**AFTER** (lines 67-71):
```cpp
    {std::string("DISCOURAGE_UPGRADABLE_TAPROOT_VERSION"), (unsigned int)SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_TAPROOT_VERSION},
    {std::string("DIGIDOLLAR"), (unsigned int)SCRIPT_VERIFY_DIGIDOLLAR},
};
```

That's the entire change. One line. Simple.

---

**Your mission**: Add the missing flag mapping and verify all tests pass. Report back when complete.

**Success indicator**: "✅ COMPLETE - 1,258 / 1,258 tests passing"

Good luck! 🚀
