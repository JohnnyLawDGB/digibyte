# DigiByte v8.23 Migration - Initial Work Prompt

## Overview

You are a DigiByte Core developer tasked with cherry-picking Bitcoin commits to upgrade DigiByte from v8.22 to v8.23. You must preserve ALL DigiByte-specific features while incorporating Bitcoin improvements.

## Required Reading

**BEFORE STARTING:** You must read and understand these documents:
- `DIGIBYTE_V8.23_MIGRATION_SPEC.md` - Complete migration specification
- Review the current DigiByte codebase structure, particularly the unique features

## Your Mission

Cherry-pick commits from Bitcoin v22-final to v23.2 (3,031 total commits) into DigiByte, with testing checkpoints every 250 commits.

## CRITICAL: DigiByte Features to Preserve

### 1. Multi-Algorithm Mining (5 algorithms)
- **Algorithms**: SHA256D, Scrypt, Groestl, Skein, Qubit/ODO
- **Core Files (NEVER MODIFY)**: 
  - `src/primitives/block.h` - Algorithm definitions and constants
  - `src/primitives/block.cpp` - Algorithm implementation functions
  - `src/chain.h` - CBlockIndex with `lastAlgoBlocks[NUM_ALGOS_IMPL]`
  - `src/chain.cpp` - Algorithm work factor calculations
- **Key Functions**: `GetAlgoByName()`, `GetAlgoWorkFactor()`, `CBlockHeader::GetAlgo()`
- **Constants**: `ALGO_*` enums, `BLOCK_VERSION_*` constants

### 2. Difficulty Adjustment Algorithms (V1-V4)
- **Files**: `src/pow.cpp`, `src/pow.h`
- **Functions**: `GetNextWorkRequiredV1()` through `GetNextWorkRequiredV4()`
- **Critical Heights**: 145000, 400000, 1430000 (hard fork activation points)

### 3. Dandelion++ Privacy Implementation
- **Files (PRESERVE COMPLETELY)**: `src/dandelion.cpp`, `src/dandelion.h`
- **Feature**: Transaction routing privacy protocol
- **Functions**: All Dandelion-related functions must remain intact

### 4. ODO/Odocrypt Dynamic PoW
- **Files**: `src/crypto/odocrypt.*`, `src/crypto/hashodo.h`
- **Activation**: Block 9,112,320
- **Feature**: Shape-changing PoW algorithm that changes every 10 days

### 5. Network Parameters (NEVER CHANGE)
- **Ports**: 12024 (mainnet), 14022 (RPC)
- **Block time**: 15 seconds (vs Bitcoin's 10 minutes)
- **Address prefixes**: 'D' (P2PKH), 'S' (P2SH)
- **Bech32**: "dgb"
- **Genesis block**: Unique DigiByte genesis parameters

### 6. Block Rewards (PRESERVE CUSTOM SCHEDULE)
- **Schedule**: 72,000 → 16,000 → 8,000 → 2,459 → exponential decay
- **File**: `src/validation.cpp` (`GetBlockSubsidy()` function)

## File Classification Rules for Conflict Resolution

### 🚨 MASTER RULE - HIGHEST PRIORITY 🚨
**PRESERVE ALL DIGIBYTE-UNIQUE FEATURES REGARDLESS OF FILE:**
If ANY code in ANY file contains DigiByte-specific functionality, it must be preserved:

```bash
# DigiByte-unique patterns to ALWAYS preserve (in any file):
- Multi-algorithm code: ALGO_*, GetAlgo*, *Algo*, NUM_ALGOS
- Difficulty adjustments: *V1*, *V2*, *V3*, *V4*, DigiShield, MultiShield
- Dandelion: dandelion*, Dandelion*, DANDELION_*
- ODO/Odocrypt: ODO*, odo*, Odocrypt*, ODOCRYPT_*
- Network params: 12024, 14022, 0xfa, 0xc3, 0xb6, 0xda, "dgb", "D", "S"
- Block rewards: 72000, 16000, 8000, 2459, GetBlockSubsidy custom logic
- Timing: 15 seconds, multiAlgoTargetSpacing, nAveragingInterval
- Hard fork heights: 145000, 400000, 1430000, 9112320, 9100000
- DigiByte strings: "DigiByte", "DGB", "digibyted", "digibyte-"
```

### NEVER MODIFY (Preserve DigiByte completely):
```
src/primitives/block.*
src/chain.*
src/crypto/hash*.h
src/crypto/odocrypt.*
src/crypto/scrypt.*
src/dandelion.*
src/chainparams.*
```

### CAREFUL MERGE (Preserve DigiByte logic, adapt Bitcoin improvements):
```
src/pow.cpp - Difficulty adjustments
src/validation.cpp - Block rewards, consensus
src/consensus/params.h - Consensus parameters
src/rpc/mining.cpp - Mining-related RPC
```

### PREFER BITCOIN (Take Bitcoin changes, apply DigiByte naming):
```
src/rpc/*.cpp (except mining-related)
src/wallet/*.cpp
src/qt/*.cpp
src/util/*.cpp
```

**IMPORTANT:** Even in "PREFER BITCOIN" files, if you find DigiByte-unique code patterns, preserve them!

## Setup Commands

Execute these commands to begin the migration:

```bash
# Ensure you're in the DigiByte directory
cd /Users/jt/Code/digibyte

# Add Bitcoin as upstream remote
git remote add bitcoin-upstream https://github.com/bitcoin/bitcoin.git
git fetch bitcoin-upstream --tags

# Get complete commit list from Bitcoin v22-final to v23.2
git log --oneline --reverse bitcoin-upstream/v22-final..bitcoin-upstream/v23.2 > bitcoin_commits_v22_to_v23.2.txt

# Verify commit count (should be 3,031)
wc -l bitcoin_commits_v22_to_v23.2.txt

# Create migration branch
git checkout -b digibyte-8.23-migration

# Create comprehensive migration tracking files
echo "# DigiByte v8.23 Migration Log" > migration_log.md
echo "Started: $(date)" >> migration_log.md
echo "Target: 3,031 commits from Bitcoin v22-final to v23.2" >> migration_log.md
echo "" >> migration_log.md

# Create master tracking file
echo "# DigiByte v8.23 Migration Master Tracker" > migration_tracker.md
echo "## Summary Statistics" >> migration_tracker.md
echo "- **Total Commits**: 3,031" >> migration_tracker.md
echo "- **Processed**: 0" >> migration_tracker.md
echo "- **Successful**: 0" >> migration_tracker.md
echo "- **Conflicts**: 0" >> migration_tracker.md
echo "- **Failures**: 0" >> migration_tracker.md
echo "- **Progress**: 0.0%" >> migration_tracker.md
echo "" >> migration_tracker.md
echo "## Quick Status" >> migration_tracker.md
echo "| Commit Range | Status | Conflicts | Duration | Notes |" >> migration_tracker.md
echo "|--------------|--------|-----------|----------|--------|" >> migration_tracker.md
echo "" >> migration_tracker.md

# Create conflicts-only log
echo "# DigiByte v8.23 Migration - Conflicts Log" > conflicts_log.md
echo "This file tracks all merge conflicts encountered during migration." >> conflicts_log.md
echo "Started: $(date)" >> conflicts_log.md
echo "" >> conflicts_log.md

# Create success log
echo "# DigiByte v8.23 Migration - Success Log" > success_log.md
echo "This file tracks all successfully merged commits." >> success_log.md
echo "Started: $(date)" >> success_log.md
echo "" >> success_log.md
```

## Process for Each Individual Commit

### 1. Cherry-pick the commit
```bash
COMMIT_HASH="<commit-hash>"
COMMIT_TITLE=$(git log --format=%s -n 1 bitcoin-upstream/$COMMIT_HASH)
ORIGINAL_AUTHOR=$(git log --format="%an <%ae>" -n 1 bitcoin-upstream/$COMMIT_HASH)

echo "🔄 Processing commit $COMMIT_HASH: $COMMIT_TITLE"
echo "📝 Logging attempt..." 

# Log the attempt
echo "## Commit $COMMIT_HASH - $(date) - ATTEMPTED" >> migration_log.md
echo "**Title:** $COMMIT_TITLE" >> migration_log.md
echo "**Original Author:** $ORIGINAL_AUTHOR" >> migration_log.md

# Attempt cherry-pick
if git cherry-pick -n $COMMIT_HASH; then
    echo "✅ Clean cherry-pick successful"
    CONFLICTS_RESOLVED=""
else
    echo "⚠️  Merge conflicts detected - manual resolution required"
    
    # Log conflicts
    echo "**Status:** CONFLICTS DETECTED" >> migration_log.md
    echo "**Conflicted Files:**" >> migration_log.md
    git status --porcelain | grep "^UU\|^AA\|^DD" >> migration_log.md
    
    # Set flag for later logging
    CONFLICTS_RESOLVED="Manual resolution required for: $(git status --porcelain | grep '^UU\|^AA\|^DD' | cut -c4-)"
    
    echo "🔧 Resolve conflicts manually following file classification rules"
    echo "   - NEVER MODIFY: Reject Bitcoin changes for protected files"
    echo "   - CAREFUL MERGE: Preserve DigiByte logic"
    echo "   - PREFER BITCOIN: Take Bitcoin changes with naming fixes"
fi
```

### 2. Apply conflict resolution based on MASTER RULE + file classification

**🚨 STEP 1: Check for DigiByte-unique patterns (HIGHEST PRIORITY)**
```bash
# Scan all changed files for DigiByte-unique patterns
echo "🔍 Scanning for DigiByte-unique features..."
CHANGED_FILES=$(git diff --name-only --cached)

for file in $CHANGED_FILES; do
    if grep -q "ALGO_\|GetAlgo\|DigiShield\|MultiShield\|dandelion\|Dandelion\|ODO\|odo\|Odocrypt\|12024\|14022\|0xfa.*0xc3.*0xb6.*0xda\|72000\|16000\|8000\|2459\|multiAlgoTargetSpacing\|nAveragingInterval\|145000\|400000\|1430000\|9112320\|9100000" "$file"; then
        echo "⚠️  DIGIBYTE-UNIQUE FEATURES DETECTED in $file"
        echo "🔒 This file contains DigiByte-specific code - PRESERVE ALL unique features!"
        
        # Log this detection
        echo "**CRITICAL:** DigiByte-unique features detected in $file" >> migration_log.md
    fi
done
```

**STEP 2: Apply file-specific classification**
- **NEVER MODIFY files**: If Bitcoin changes these files, reject the changes and keep DigiByte version
- **CAREFUL MERGE files**: Manually review and preserve DigiByte-specific logic  
- **PREFER BITCOIN files**: Take Bitcoin changes and apply DigiByte naming conventions

**🚨 REMEMBER**: Even in "PREFER BITCOIN" files, preserve any DigiByte-unique patterns found above!

### 3. Apply DigiByte naming conventions
```bash
# Binary names
sed -i 's/bitcoind/digibyted/g' <files>
sed -i 's/bitcoin-cli/digibyte-cli/g' <files>
sed -i 's/bitcoin-tx/digibyte-tx/g' <files>
sed -i 's/bitcoin-wallet/digibyte-wallet/g' <files>
sed -i 's/bitcoin-qt/digibyte-qt/g' <files>

# Library names
sed -i 's/libbitcoin/libdigibyte/g' <files>
sed -i 's/bitcoinconsensus/digibyteconsensus/g' <files>

# Header guards
sed -i 's/BITCOIN_/DIGIBYTE_/g' <files>

# Class names
sed -i 's/BitcoinGUI/DigiByteGUI/g' <files>
sed -i 's/BitcoinUnits/DigiByteUnits/g' <files>

# Currency codes
sed -i 's/BTC/DGB/g' <files>

# Project names
sed -i 's/Bitcoin Core/DigiByte Core/g' <files>
```

### 4. Update copyright headers
```cpp
// Add DigiByte copyright below Bitcoin copyright
// Copyright (c) 2009-2024 The Bitcoin Core developers
// Copyright (c) 2014-2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
```

### 5. MANDATORY: Test and compile BEFORE committing
```bash
# Quick compilation check (incremental build)
make -j$(nproc)

# If compilation fails, fix issues before proceeding
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed for commit $COMMIT_HASH"
    
    # Log compilation failure
    echo "**Status:** COMPILATION FAILED ❌" >> migration_log.md
    echo "**Error:** Build errors detected" >> migration_log.md
    echo "**Files Changed:**" >> migration_log.md
    git diff --cached --name-only >> migration_log.md
    echo "**Action Required:** Fix compilation errors before proceeding" >> migration_log.md
    echo "---" >> migration_log.md
    
    echo "Fix compilation errors before committing"
    exit 1
fi

# Run unit tests for affected modules
# Identify which test modules are relevant to changed files
CHANGED_FILES=$(git diff --cached --name-only)
echo "Changed files: $CHANGED_FILES"

# Run relevant unit tests based on changed files
if echo "$CHANGED_FILES" | grep -q "src/pow\|src/validation\|src/consensus"; then
    echo "Running consensus-critical tests..."
    src/test/test_digibyte --run_test=pow_tests
    src/test/test_digibyte --run_test=validation_tests
fi

if echo "$CHANGED_FILES" | grep -q "src/rpc"; then
    echo "Running RPC tests..."
    src/test/test_digibyte --run_test=rpc_tests
fi

if echo "$CHANGED_FILES" | grep -q "src/wallet"; then
    echo "Running wallet tests..."
    src/test/test_digibyte --run_test=wallet_tests
fi

# For high-risk files, run additional DigiByte-specific validation
if echo "$CHANGED_FILES" | grep -q "src/pow.cpp\|src/primitives/block\|src/chain"; then
    echo "⚠️  High-risk files changed - running DigiByte multi-algo validation"
    # Quick algorithm validation test
    src/test/test_digibyte --run_test=multialgo_tests
fi
```

### 6. Only commit if ALL tests pass
```bash
# Stage all changes
git add -A

# Commit with proper attribution only if tests passed
git commit -m "Merge bitcoin/<hash>: <original message>

Cherry-picked from Bitcoin Core commit <hash>
Original author: <original-author>

[DigiByte: Applied preservation rules and naming conventions]
Conflicts resolved in: <list of files if any>
Features preserved: Multi-algo mining, Dandelion++, DigiShield
Tests: Compilation ✓, Unit tests ✓"

echo "✅ Commit <hash> successfully applied and tested"
```

### 7. Document commit progress
```bash
# Log detailed commit information to main log
echo "## Commit $COMMIT_HASH - $(date)" >> migration_log.md
echo "**Title:** $COMMIT_TITLE" >> migration_log.md
echo "**Original Author:** $ORIGINAL_AUTHOR" >> migration_log.md
echo "**Status:** SUCCESS ✓" >> migration_log.md

# Log conflicts if any occurred
if [ -n "$CONFLICTS_RESOLVED" ]; then
    echo "**Conflicts Resolved:**" >> migration_log.md
    echo "$CONFLICTS_RESOLVED" >> migration_log.md
    
    # Also log to conflicts file
    echo "## $COMMIT_HASH - $(date)" >> conflicts_log.md
    echo "**Title:** $COMMIT_TITLE" >> conflicts_log.md
    echo "**Resolution:** $CONFLICTS_RESOLVED" >> conflicts_log.md
    echo "**Files:**" >> conflicts_log.md
    git diff --name-only HEAD~1 HEAD >> conflicts_log.md
    echo "---" >> conflicts_log.md
fi

# Log which files were changed
echo "**Files Changed:**" >> migration_log.md
git diff --name-only HEAD~1 HEAD >> migration_log.md

# Log test results
echo "**Tests Run:**" >> migration_log.md
echo "- Compilation: ✓ PASSED" >> migration_log.md
echo "- Unit Tests: ✓ PASSED" >> migration_log.md
if [ -n "$DIGIBYTE_TESTS_RUN" ]; then
    echo "- DigiByte Validation: ✓ PASSED" >> migration_log.md
fi

echo "**Features Preserved:** Multi-algo mining, Dandelion++, DigiShield" >> migration_log.md
echo "---" >> migration_log.md
echo "" >> migration_log.md

# Log to success file
echo "$COMMIT_HASH | $COMMIT_TITLE | $(date)" >> success_log.md

# Update master tracker statistics
PROCESSED_COUNT=$(grep -c "SUCCESS ✓" migration_log.md)
CONFLICTS_COUNT=$(grep -c "CONFLICTS DETECTED" migration_log.md)
PROGRESS=$(echo "scale=1; $PROCESSED_COUNT * 100 / 3031" | bc)

# Update tracker file (simplified - you may want to make this more sophisticated)
echo "Updated: $(date) | Processed: $PROCESSED_COUNT/3031 ($PROGRESS%) | Conflicts: $CONFLICTS_COUNT" >> migration_tracker.md
```

## Comprehensive Testing Protocol (Every 250 Commits)

**IN ADDITION to per-commit testing, run comprehensive tests every 250 commits:**

**MANDATORY SEQUENCE - Must be followed in order:**

### 1. C++ Unit Tests (Run First)
```bash
make check
```
**If this fails:** Stop and debug all C++ unit test failures before proceeding.

### 2. Python Functional Tests (Run Second)
```bash
test/functional/test_runner.py --extended

# DigiByte-specific tests
./test/functional/digibyte_multialgo.py
./test/functional/digibyte_dandelion.py
./test/functional/digibyte_difficulty.py
./test/functional/digibyte_rewards.py
```
**If this fails:** Stop and debug all Python test failures before proceeding.

### 3. Compilation Test (Run Last)
```bash
make clean && ./autogen.sh && ./configure && make -j$(nproc)
```
**If this fails:** Stop and debug all compilation errors before proceeding.

### 4. Checkpoint Success
Only proceed to the next 250 commits when ALL comprehensive tests pass in sequence.

## Two-Level Testing Strategy Summary

### Level 1: Per-Commit Testing (EVERY commit)
- ✅ Incremental compilation (`make`)
- ✅ Relevant unit tests for changed modules
- ✅ DigiByte-specific validation for high-risk files
- ✅ Only commit if all tests pass

### Level 2: Comprehensive Testing (Every 250 commits)
- ✅ Full C++ unit test suite (`make check`)
- ✅ Complete Python functional tests
- ✅ Clean compilation from scratch
- ✅ Full DigiByte feature validation

## Progress Tracking

### Document each checkpoint in migration_log.md:
```markdown
## Checkpoint <N> (Commits <start>-<end>)
- Started: <timestamp>
- Commits processed: 250/250
- Individual commit tests: All passed ✓
- Clean merges: <count>
- Conflicts resolved: <count>
- Comprehensive C++ tests: PASS/FAIL
- Comprehensive Python tests: PASS/FAIL
- Clean compilation: PASS/FAIL
- Status: COMPLETE/FAILED
- Duration: <time>

### Per-Commit Test Summary:
- Compilation failures: <count> (all fixed)
- Unit test failures: <count> (all fixed)
- DigiByte validation failures: <count> (all fixed)

### Major Conflicts Resolved:
- <commit-hash>: <file> - <brief description>

### Features Validated:
- ✓ Multi-algorithm mining
- ✓ Dandelion++ privacy
- ✓ Difficulty adjustments
- ✓ Block rewards
- ✓ Network parameters
```

## Conflict Resolution Examples

### Example 1: pow.cpp conflict
```cpp
// Bitcoin change: modifies GetNextWorkRequired
// DigiByte resolution: Integrate Bitcoin improvement WITHOUT breaking V1-V4 algorithms
// Action: Preserve all GetNextWorkRequiredV1-V4 functions, adapt new logic carefully
```

### Example 2: validation.cpp conflict
```cpp
// Bitcoin change: new validation rule
// DigiByte resolution: Ensure compatibility with 15-second blocks and custom rewards
// Action: Adapt validation for DigiByte's faster blocks and reward schedule
```

### Example 3: chainparams.cpp conflict
```cpp
// Bitcoin change: updates network parameters
// DigiByte resolution: REJECT Bitcoin changes completely
// Action: Keep ALL DigiByte network parameters unchanged
```

## Critical Success Factors

1. **🚨 MASTER RULE: PRESERVE ALL DIGIBYTE-UNIQUE FEATURES IN ANY FILE**
2. **NEVER compromise DigiByte's unique features**
3. **ALWAYS preserve multi-algorithm mining**
4. **PROTECT Dandelion++ privacy at all costs**
5. **MAINTAIN all difficulty adjustment algorithms**
6. **KEEP network parameters unchanged**
7. **SCAN every file for DigiByte-unique patterns before applying changes**
8. **DOCUMENT every conflict resolution**
9. **Test religiously every commit and every 250 commits**

## Emergency Procedures

If you encounter a critical issue:
1. **Stop immediately**
2. **Document the issue in migration_log.md**
3. **Create a checkpoint tag**: `git tag checkpoint-emergency-<timestamp>`
4. **Analyze the problem thoroughly**
5. **Seek guidance if needed**

## Expected Migration Statistics

- **Total commits**: 3,031
- **Expected checkpoints**: ~12 (3031 ÷ 250)
- **Critical conflicts expected**: High in consensus-related files
- **Safe merges expected**: Majority in wallet/RPC/UI files

## Files to Monitor Closely

Watch these files for conflicts that require special attention:
- `src/pow.cpp` - Difficulty adjustment logic
- `src/validation.cpp` - Block rewards and consensus
- `src/chainparams.cpp` - Network parameters
- `src/primitives/block.h` - Algorithm definitions
- `src/rpc/mining.cpp` - Mining-related RPC calls

## Ready to Begin

Once you have:
1. ✅ Read `DIGIBYTE_V8.23_MIGRATION_SPEC.md` completely
2. ✅ Executed the setup commands
3. ✅ Verified you have 3,031 commits in the list
4. ✅ Created the migration branch
5. ✅ Understand the file classification rules

**BEGIN with the first commit from bitcoin_commits_v22_to_v23.2.txt**

Process commits sequentially, document everything, and test rigorously every 250 commits. The success of DigiByte v8.23 depends on preserving every unique feature while gaining Bitcoin's improvements.

Good luck! 🚀