# CLAUDE.md - AI Assistant Guide for DigiByte Development

## Overview
This file provides context and guidance for AI assistants working on the DigiByte codebase, particularly for the Bitcoin Core v26.2 merge creating DigiByte v8.26.

## Repository Structure
**Required repositories:**
- `/mnt/c/Users/Jared/code/digibyte` (v8.26 merged code - BUILD/TEST HERE)
- `/mnt/c/Users/Jared/code/bitcoin-v26.2-for-digibyte` (Bitcoin v26.2 reference)
- `/mnt/c/Users/Jared/code/digibyte-v8.22.2` (DigiByte v8.22.2 - SOURCE OF TRUTH)

## C++ Unit Test Fix Strategy

### Overview
We have 107 C++ unit test files that need fixing after the Bitcoin v26.2 merge. Tests are categorized into 10 groups for parallel AI assignment to avoid conflicts.

### Build Process
```bash
./autogen.sh
./configure --enable-tests --enable-bench --enable-debug CXXFLAGS="-O0 -g"
make -j6
```

### Test Execution
```bash
# Run individual test
./src/test/test_digibyte --log_level=all --run_test=TEST_NAME

# Run all tests in a file
./src/test/test_digibyte --log_level=all --run_test=TEST_FILE_WITHOUT_CPP
```

### Fix Methodology

1. **Always Compare Three Codebases:**
   - v8.26 (current - what we're fixing)
   - v8.22.2 (SOURCE OF TRUTH for DigiByte values)
   - Bitcoin v26.2 (to understand what changed)

2. **Common DigiByte Replacements:**
   - MAX_MONEY: 21000000 → 21000000000 (21 billion)
   - Block time: 600 → 15 seconds
   - Addresses: 1.../3... → D.../S...
   - Bech32: bc1/tb1/bcrt1 → dgb1/dgbt1/dgbrt1
   - Single algo → 5 algorithms + Odocrypt

3. **Application Bug Protocol:**
   - Fix bugs in application code when discovered
   - Document thoroughly
   - Report using standardized format

### Critical DigiByte Constants
```cpp
// Network
MAINNET_DEFAULT_PORT = 12024
MAINNET_MESSAGE_START = {0xfa, 0xc3, 0xb6, 0xda}

// Consensus
MAX_MONEY = 21000000000 * COIN
POW_TARGET_SPACING = 15

// Address Prefixes
PUBKEY_ADDRESS = 30  // 'D'
SCRIPT_ADDRESS = 63  // 'S'
bech32_hrp = "dgb"

// Mining Algorithms
ALGO_SHA256D = 0
ALGO_SCRYPT = 1
ALGO_GROESTL = 2
ALGO_SKEIN = 3
ALGO_QUBIT = 4
ALGO_ODO = 7  // Odocrypt
```

### Application Bug Reporting Format
```markdown
## APPLICATION BUG FIXED
**File**: src/[filename].cpp:XXX
**Test**: [test_file.cpp]::[test_name]
**Issue**: [description]
**Root Cause**: [Bitcoin v26.2 merge impact]
**Fix Applied**:
```cpp
// Code fix here
```
**Impact**: [consequence if unfixed]
**Testing**: [how verified]
```

## C++ Unit Test Categorization for Parallel AI Assignment

**Total Test Files**: 107 C++ unit test files
**Assignment Strategy**: Each AI agent gets a specific category to avoid conflicts

### Category 1: Skipped Tests
**AI Agent 1 Assignment **
- `rbf_tests.cpp` - Replace-by-fee -- SKIPPED
- `bip324_tests.cpp` - V2 transport protocol -SKIPPED

### Critical DigiByte-Specific Considerations

Each AI agent must check for:
1. **Address formats**: D/S prefixes, dgb1/dgbt1/dgbrt1 bech32
2. **Supply**: 21 billion (not 21 million)
3. **Block time**: 15 seconds (not 600)
4. **Algorithms**: 5 mining algorithms + Odocrypt
5. **Network magic**: DigiByte-specific values
6. **Genesis block**: DigiByte genesis hash
7. **Difficulty**: DigiSpeed adjustment
8. **Dandelion++**: Privacy protocol integration

### Execution Rules for Each AI Agent
1. Run only your assigned test files
2. Fix one file completely before moving to next
3. Always compare with v8.22.2 for DigiByte values
4. Document all changes in detail
5. Report any discovered application bugs (don't fix)
6. Test each file: `./src/test/test_digibyte --run_test=TEST_NAME`
7. Verify fix: File should pass 100% of its tests

### Missing v8.22.2 Features Checklist

While fixing tests, actively look for missing DigiByte features:
- [ ] Dandelion++ implementation
- [ ] Multi-algorithm mining setup
- [ ] Custom RPC commands (getblockreward, etc.)
- [ ] DigiSpeed difficulty adjustment
- [ ] Odocrypt algorithm activation
- [ ] Custom checkpoint logic
- [ ] DigiByte-specific wallet features

## Test Assignment Instructions

1. Use `TEST_FIX_PROMPT_TEMPLATE.md` to create category-specific prompts
2. Assign each AI agent ONE category from the list below
3. Agents work independently on their assigned test files
4. All fixes must preserve test logic - no disabling tests

## Important Reminders
- Both Bitcoin and DigiByte copyrights must be preserved
- NEVER comment out failing tests
- ALWAYS verify fixes against v8.22.2 behavior
- Document every change thoroughly
- Fix application bugs when discovered


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
