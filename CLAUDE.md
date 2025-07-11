# CLAUDE.md - AI Assistant Guide for DigiByte Development

## Overview
This file provides context and guidance for AI assistants working on the DigiByte codebase, particularly for the Bitcoin Core v26.2 merge creating DigiByte v8.26.

## Reference Directories
- **Current merged code**: `/Users/jt/Code/digibyte` (DigiByte v8.26 work in progress)
- **Original DigiByte**: `/Users/jt/Code/digibyte/digibyte-v8.22.2` (v8.22.2 for comparison)
- **Bitcoin v26.2**: `/Users/jt/Code/digibyte/bitcoin-v26.2-for-digibyte` (Bitcoin Core v26.2 reference)

## Build Error Resolution Process

When fixing build errors after v26.2 merge:

### Setup
**Required repositories:**
- `digibyte` (merged code, current directory)
- `bitcoin-v26.2-for-digibyte` folder in root (contains pre-converted Bitcoin v26.2 with DigiByte naming)
- `digibyte-v8.22.2` folder in root (original DigiByte v8.22.2 code for comparison)

**IMPORTANT:** The `bitcoin-v26.2-for-digibyte` folder contains Bitcoin v26.2 code that has already been converted to DigiByte naming conventions. Always reference this folder for v26.2 code patterns.

## Build Process

```bash
# Bootstrap
./autogen.sh

# START WITH MINIMAL BUILD (no GUI, no tests)
./configure --without-gui --disable-tests --disable-bench

# Capture errors
make -j6 2>&1 | tee build_errors.log

# Once minimal build works, add components:
# Step 2: ./configure --with-gui=no --enable-tests
# Step 3: ./configure --with-gui=qt5 --enable-tests --enable-bench
```

## Fix Process (ONE ERROR AT A TIME)

### 1. Identify Error
```bash
grep -A5 "error:" build_errors.log | head -20
```

### 2. Compare Three Versions
```bash
# Set error file
export ERROR_FILE="src/path/to/error.cpp"

# Visual diff (bitcoin-v26.2-for-digibyte folder contains v26.2 code)
vimdiff digibyte-v8.26/$ERROR_FILE \
        digibyte-v8.22.2/$ERROR_FILE \
        bitcoin-v26.2-for-digibyte/$ERROR_FILE
```

### 3. Fix Using v26.2 Standards
**Rule: Use Bitcoin v26.2 code as base, adapt DigiByte features to it**

```cpp
// DON'T: Copy old DigiByte code
// DO: Adapt DigiByte features to v26.2 patterns

// Example - Add algo parameter the v26.2 way:
std::unique_ptr<CBlockTemplate> CreateNewBlock(
    const CScript& scriptPubKeyIn,
    int algo = ALGO_SHA256D);  // Modern optional parameter
```

### 4. Verify & Commit
```bash
# Clean previous build artifacts
make clean

# Test fix
make -j6 2>&1 | tee build_errors.log

# If error is fixed, commit immediately
git add $ERROR_FILE
git commit -m "Fix build: $ERROR_FILE

- Error: [exact error message]
- Fix: [what changed]
- Preserves: [DigiByte feature]"

# Return to step 1 for next error
```

## Common Fixes

**Missing DigiByte function:**
- Find where it belongs in v26.2 structure
- Re-implement using v26.2 patterns (not old code)

**API mismatch:**
- Keep v26.2 base signature
- Add DigiByte params as optional/overloaded

**Build system:**
- Add DigiByte files to Makefile.am
- Use v26.2 naming conventions

**File-level fixes (saves time):**
- If a file has no DigiByte-specific features, copy entire file from `bitcoin-v26.2-for-digibyte`
- Delete duplicate/obsolete files created by merge process
- Example: Pure utility files, test helpers, or build scripts without DGB customizations
```bash
# Quick check for DigiByte-specific code
grep -i "algo\|dandelion\|digishield\|odocrypt" $ERROR_FILE
# If empty and file exists in v26.2, safe to copy:
cp bitcoin-v26.2-for-digibyte/$ERROR_FILE digibyte-v8.26/$ERROR_FILE
```

**Code removal:**
- If code was removed in Bitcoin v26.2 and is NOT DigiByte-specific, DELETE it
- Don't comment out - remove entirely to match v26.2 cleanliness
- Example: Old deprecated functions, unused utilities, legacy code
```bash
# Check if function/code exists in v26.2
grep -n "function_name" bitcoin-v26.2-for-digibyte/$ERROR_FILE
# If not found and not DGB-specific, delete it
```

## Critical Checks
Every fix must preserve:
- Multi-algo mining (5 algos + Odocrypt)
- 15-second blocks
- Dandelion++
- 21 billion supply
- Custom RPCs (getblockreward, etc.)

## Rollback Bad Fix
```bash
git reset --hard HEAD~1
# Re-analyze and try again
```

**Remember:** Always use v26.2 code style. Never copy old v8.22.2 code directly.

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

### Building
```bash
./autogen.sh
./configure --enable-debug
make -j6 2>&1 | tee build_errors.log
```

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

## Testing Checklist

After any significant changes:
- [ ] All 5 algorithms produce valid blocks
- [ ] 15-second average block time maintained
- [ ] getblockreward returns correct values
- [ ] Dandelion++ routing works
- [ ] Network sync successful
- [ ] All unit tests pass
- [ ] All functional tests pass

## Version Naming
- Current: v8.22.2
- Target: v8.26 (aligned with Bitcoin Core v26.2)
- Format: v8.XX where XX approximates Bitcoin Core version
