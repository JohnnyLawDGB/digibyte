# CLAUDE.md - AI Assistant Guide for DigiByte Development

## Overview
This file provides context and guidance for AI assistants working on the DigiByte codebase, particularly for the Bitcoin Core v26.2 merge creating DigiByte v8.26.

# DigiByte v8.26 C++ Unit Test Fix Guide

You are a DigiByte engineer fixing C++ unit tests. **TOP PRIORITY: Get `make check` passing with REAL fixes - NO commenting out code.**

## Setup
**Required repositories:**
- `digibyte-v8.26` (merged code - BUILD/TEST ONLY HERE)
- `bitcoin-v26.2-for-digibyte` (Bitcoin v26.2 reference)
- `digibyte-v8.22.2` (DigiByte v8.22.2 - SOURCE OF TRUTH for hashes, addresses, prefixes)

## Test Fix Process

```bash
# Build with tests
./autogen.sh
./configure --enable-tests --enable-bench --enable-debug CXXFLAGS="-O0 -g"
make -j6

# Run tests and capture failures
make check 2>&1 | tee test_results.log
```

## Fixing Test Failures - Step by Step

### 1. Identify Failing Test
```bash
# Find first failure
grep -A10 "FAILED" test_results.log

# Run single test with debug output
./src/test/test_digibyte --log_level=all --run_test=TESTNAME
```

### 2. Compare with v8.22.2 Tests (SOURCE OF TRUTH)
```bash
# ALWAYS check v8.22.2 for correct DigiByte values
export TEST_FILE="src/test/failing_test.cpp"
vimdiff digibyte-v8.26/$TEST_FILE digibyte-v8.22.2/$TEST_FILE
```

**Common values to copy from v8.22.2:**
- Genesis block hashes
- Address prefixes (D=30, S=63)
- Example addresses
- Block hashes
- Transaction IDs
- Merkle roots
- Network magic bytes

### 3. Fix Categories

#### Supply & Constants
```cpp
// WRONG (Bitcoin value)
BOOST_CHECK_EQUAL(MAX_MONEY, 21000000 * COIN);

// CORRECT (from v8.22.2)
BOOST_CHECK_EQUAL(MAX_MONEY, 21000000000 * COIN);
```

#### Address Tests
```cpp
// WRONG (Bitcoin addresses)
BOOST_CHECK(IsValidDestinationString("1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62i"));

// CORRECT (DigiByte addresses from v8.22.2)
BOOST_CHECK(IsValidDestinationString("DGSbdXzKqPNLBpPDWK7MXgXN45LxYvPqFD"));
```

#### Block/Transaction Hashes
```cpp
// ALWAYS use hashes from v8.22.2 tests
// Genesis hash: 7497ea1b465eb39f1c8f507bc877078fe016d6fcb6dfad3a64c98dcc6e1e8680
// Example: Check v8.22.2's test/data/tx_valid.json for valid transactions
```

#### Mining Tests
```cpp
// Must test all algorithms
for (int algo = ALGO_SHA256D; algo <= ALGO_QUBIT; algo++) {
    // Test each algorithm
}
// Don't forget ALGO_ODO = 7 for Odocrypt
```

#### Timing Tests
```cpp
// Bitcoin: 600 seconds (10 min)
// DigiByte: 15 seconds
consensus.nPowTargetSpacing = 15;
```

#### Difficulty Tests
```cpp
// DigiByte has 4 versions - test each at correct heights
if (height < 145000) GetNextWorkRequiredV1(...);
else if (height < 400000) GetNextWorkRequiredV2(...);
// etc.
```

### 4. NEVER DO THIS
```cpp
// NEVER comment out failing tests
// BOOST_AUTO_TEST_CASE(some_test) {  // <-- DON'T DO THIS

// NEVER skip tests
return; // <-- DON'T ADD THIS

// NEVER change test logic to make it pass without understanding why
```

### 5. Document Every Fix
```markdown
## Test: [test_name]
**File**: src/test/[filename].cpp
**Failure**: [exact error message]
**Root cause**: [Bitcoin assumption vs DigiByte reality]
**Fix**: [what was changed]
**v8.22.2 reference**: [file:line where correct value found]
```

## Quick Reference from v8.22.2

```cpp
// Critical DigiByte values (from v8.22.2)
MAX_MONEY = 21000000000 * COIN
P2PKH_PREFIX = 30  // 'D' addresses
P2SH_PREFIX = 63   // 'S' addresses
POW_TARGET_SPACING = 15
COINBASE_MATURITY = 100
COINBASE_MATURITY_2 = 8640
NUM_ALGOS = 5
ALGO_ODO = 7

// Example valid DigiByte addresses (from v8.22.2 tests)
"DGSbdXzKqPNLBpPDWK7MXgXN45LxYvPqFD"  // P2PKH
"SfBbrV3yCGjKW52dac8Tgkvqd6zMgvPZFG"  // P2SH
```

## Verification

```bash
# After each fix
make -j6 && ./src/test/test_digibyte --run_test=FIXED_TEST_NAME

# Final check - must be 100%
make check

# No disabled tests allowed
grep -r "DISABLED\|SKIP\|commented.*TEST" src/test/
```

**Remember: v8.22.2 tests are your source of truth for all DigiByte-specific values!**

## Important Reminders
- Both Bitcoin and DigiByte copyrights must be preserved


## DigiByte Unique Features

### Multi-Algorithm Mining
DigiByte uses 5 mining algorithms that must be preserved:
- SHA256D (ALGO_SHA256D = 0)
- Scrypt (ALGO_SCRYPT = 1)
- Groestl (ALGO_GROESTL = 2)
- Skein (ALGO_SKEIN = 3)
- Qubit (ALGO_QUBIT = 4)
- Odocrypt (ALGO_ODO = 7) - Activates at height 9,112,320

Each algorithm targets a 75-second block time (15 seconds × 5 algorithms = 75 seconds per algo).

### Critical Constants
```cpp
// Never change these values
static const int POW_TARGET_SPACING = 15; // 15 seconds
static const CAmount MAX_MONEY = 21000000000 * COIN; // 21 billion DGB
static const int MAINNET_DEFAULT_PORT = 12024;
static const int TESTNET_DEFAULT_PORT = 12025;
static const unsigned char MAINNET_MESSAGE_START[4] = {0xfa, 0xc3, 0xb6, 0xda};
```

### Custom RPC Commands
When working with RPC commands, preserve these DigiByte-specific commands:
- `getblockreward` - Returns current block reward
- Enhanced `getmininginfo` - Shows per-algorithm statistics
- Enhanced `getdifficulty` - Returns object with all algorithm difficulties
- Mining commands with `algo` parameter support

### Dandelion++ Privacy
DigiByte implements Dandelion++ for transaction privacy. Key files:
- src/dandelion.cpp
- src/dandelion.h
- src/stempool.h

Preserve all Dandelion-related code, especially:
- `NetMsgType::DANDELIONTX` message handling
- Stem pool management
- Transaction routing logic

### Difficulty Adjustment
DigiByte uses custom difficulty algorithms:
- DigiShield V1 (block 67,200)
- MultiAlgo V2 (block 145,000)
- MultiShield V3 (block 400,000)
- DigiSpeed V4 (block 1,430,000)

## Development Commands

### Testing
```bash
# Run all tests
./test/functional/test_runner.py

# Run DigiByte-specific tests
./test/functional/digibyte_*.py

# Run unit tests
./src/test/test_digibyte

# Check for linting issues
./test/lint/all-lint.sh
```
