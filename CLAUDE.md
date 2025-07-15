# CLAUDE.md - AI Assistant Guide for DigiByte Development

## Overview
This file provides context and guidance for AI assistants working on the DigiByte codebase, particularly for the Bitcoin Core v26.2 merge creating DigiByte v8.26.

# DigiByte v8.26 C++ Unit Test Fix Guide

You are a DigiByte engineer fixing C++ unit tests. **CURRENT FOCUS: Category 1 - Address & Key Format Tests**

**TOP PRIORITY: Fix all failing tests in `src/test/key_io_tests.cpp` to properly validate DigiByte addresses and keys.**

## IMMEDIATE TASK: Category 1 - Address & Key Format Tests

### Current Test Failures
The `key_io_tests` are failing because they expect Bitcoin address formats. You need to update them for DigiByte.

### Key DigiByte Address Information
```cpp
// Mainnet
base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,30);  // 'D' addresses
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,63);  // 'S' addresses
bech32_hrp = "dgb";

// Testnet
base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,126);
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,140);
bech32_hrp = "dgbt";

// Regtest
bech32_hrp = "dgbrt";
```

### Required Changes

1. **Update test data files**:
   - `src/test/data/key_io_valid.json` - Replace Bitcoin addresses with DigiByte
   - `src/test/data/key_io_invalid.json` - Update for DigiByte validation rules
   - Change all "bc1" → "dgb1", "tb1" → "dgbt1", "bcrt1" → "dgbrt1"

2. **Fix test expectations**:
   - Bitcoin P2PKH addresses (starting with '1') → DigiByte 'D' addresses
   - Bitcoin P2SH addresses (starting with '3') → DigiByte 'S' addresses
   - Update WIF private key prefixes if needed

3. **Verify chainparams.cpp** has correct DigiByte values for all networks

### Example Valid DigiByte Addresses
```
// From v8.22.2
P2PKH: "DGSbdXzKqPNLBpPDWK7MXgXN45LxYvPqFD"
P2SH: "SfBbrV3yCGjKW52dac8Tgkvqd6zMgvPZFG"
Bech32 mainnet: "dgb1..." (not "bc1...")
Bech32 testnet: "dgbt1..." (not "tb1...")
```

### Test Command
```bash
# After making changes
make -j6
./src/test/test_digibyte --log_level=message --run_test=key_io_tests
```

### Success Criteria
- All 3 test cases in key_io_tests pass (key_io_valid_parse, key_io_valid_gen, key_io_invalid)
- No Bitcoin addresses remain in test data
- DigiByte address validation works correctly

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

## C++ Unit Test Fix Plan - Systematic Approach

### Test Status Overview
After running `make check`, we have identified the following failing C++ unit tests:

**Total Failing Test Files**: 6 confirmed
**Total Test Failures**: ~2,227 individual test assertions

**Breakdown by Test File**:
1. `coins_tests.cpp` - 1,651 failures (coins_cache_simulation_test)
2. `bip324_tests.cpp` - 280 failures (packet_test_vectors)
3. `key_io_tests.cpp` - 228 failures (address/key validation)
4. `transaction_tests.cpp` - 46 failures (test_IsStandard)
5. `compress_tests.cpp` - 1 failure (21 billion supply compression)
6. `miner_tests.cpp` - 1 failure (multi-algo initialization)
7. `blockfilter_index_tests.cpp` - 1 failure (genesis block sync)

These must be fixed systematically without commenting out tests or changing test logic without understanding.

### Failing Test Categories (Priority Order)

Based on comprehensive testing with `make check`, here are ALL failing C++ unit test files:

#### Category 1: Address & Key Format Tests (HIGH PRIORITY)
**Files**: 
- `src/test/key_io_tests.cpp` (228 failures)

**Issues**:
- Bitcoin address prefixes vs DigiByte prefixes (D=30, S=63)
- Bech32 prefix differences (dgb1/dgbt1/dgbrt1 vs bc1/tb1/bcrt1)
- Private key WIF format differences
- Test/Regtest/Signet address formats need DigiByte equivalents

**Fix Approach**:
1. Update all address prefixes in chainparams.cpp for all networks
2. Update bech32 HRP (Human Readable Part) strings
3. Verify against v8.22.2 test vectors
4. Update test data files (key_io_valid.json, key_io_invalid.json)

#### Category 2: Supply & Amount Tests (HIGH PRIORITY)
**Files**: 
- `src/test/compress_tests.cpp` (1 failure - TestPair(21000000000*COIN))

**Issues**:
- MAX_MONEY compression (21 billion DGB vs 21 million BTC)
- Amount compression algorithm assumptions

**Fix Approach**:
1. Update compression algorithm to handle larger MAX_MONEY
2. Verify all amount-related constants match DigiByte
3. Test edge cases around 21 billion supply limit

#### Category 3: Transaction Validation Tests (HIGH PRIORITY)
**Files**: 
- `src/test/transaction_tests.cpp` (46 failures in test_IsStandard)

**Issues**:
- Dust limit differences (need DigiByte's dust threshold)
- Standard transaction policy differences
- OP_RETURN size limits
- Witness transaction differences

**Fix Approach**:
1. Update dust threshold to DigiByte values
2. Verify transaction standardness rules
3. Check OP_RETURN relay limits
4. Update test vectors with DigiByte transactions

#### Category 4: Mining & Algorithm Tests (HIGH PRIORITY)
**Files**: 
- `src/test/miner_tests.cpp` (1 failure - "Algorithm 'sha256d' is not currently active")

**Issues**:
- Multi-algorithm mining not properly initialized
- Algorithm activation heights
- Block reward calculations
- Algorithm-specific difficulty adjustments

**Fix Approach**:
1. Initialize all 5 mining algorithms in test setup
2. Set correct algorithm activation heights
3. Implement proper block reward schedule
4. Test each algorithm's difficulty adjustment

#### Category 5: UTXO & Coin Cache Tests (MEDIUM PRIORITY)
**Files**: 
- `src/test/coins_tests.cpp` (1651 failures in coins_cache_simulation_test)

**Issues**:
- Coin serialization format differences
- Cache simulation with DigiByte parameters
- UTXO database assumptions

**Fix Approach**:
1. Update coin serialization to match DigiByte format
2. Adjust cache parameters for DigiByte's UTXO set
3. Verify database format compatibility

#### Category 6: Network Protocol Tests (MEDIUM PRIORITY)
**Files**: 
- `src/test/bip324_tests.cpp` (280 failures in packet_test_vectors)

**Issues**:
- Network magic bytes incorrect
- Protocol version differences
- Packet encryption test vectors

**Fix Approach**:
1. Update network magic bytes for all networks
2. Set correct protocol versions
3. Generate new test vectors with DigiByte parameters

#### Category 7: Block Index Tests (MEDIUM PRIORITY)
**Files**: 
- `src/test/blockfilter_index_tests.cpp` (1 failure in blockfilter_index_initial_sync)

**Issues**:
- Genesis block hash differences
- Checkpoint validation
- Block height assumptions

**Fix Approach**:
1. Update genesis block parameters
2. Verify checkpoint hashes
3. Adjust height-based test assumptions

#### Category 8: Other Failing Tests (LOW PRIORITY)
**Files**: 
- Additional test files may fail as we discover them during the fix process

**Common Issues Across Tests**:
- Block time (15s vs 600s)
- Difficulty adjustment algorithms
- Fork activation heights
- Coinbase maturity differences

**Fix Approach**:
1. Systematically update all consensus parameters
2. Verify against mainnet behavior
3. Test edge cases around forks

### Application Bug Discovery Protocol

**IMPORTANT**: When fixing tests, we may discover bugs in the main application code. Follow this protocol:

1. **Document the Bug**: Create a detailed report including:
   - File and line number
   - Expected behavior (from v8.22.2)
   - Actual behavior in v8.26
   - Impact assessment

2. **Ask Permission**: Before fixing any application code:
   ```
   DISCOVERED APPLICATION BUG:
   File: src/[filename].cpp:XXX
   Issue: [description]
   v8.22.2 behavior: [expected]
   v8.26 behavior: [actual]
   Proposed fix: [solution]
   
   May I proceed with fixing this application bug?
   ```

3. **Only Fix Tests**: Unless explicitly approved, only modify test files

### Missing v8.22.2 Features Checklist

While fixing tests, actively look for missing DigiByte features:
- [ ] Dandelion++ implementation
- [ ] Multi-algorithm mining setup
- [ ] Custom RPC commands (getblockreward, etc.)
- [ ] DigiSpeed difficulty adjustment
- [ ] Odocrypt algorithm activation
- [ ] Custom checkpoint logic
- [ ] DigiByte-specific wallet features

## Important Reminders
- Both Bitcoin and DigiByte copyrights must be preserved
- NEVER comment out failing tests
- ALWAYS verify fixes against v8.22.2 behavior
- Document every change thoroughly


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
