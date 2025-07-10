# DigiByte v8.23 Merge-Based Migration Specification

## Executive Summary

This specification defines the process for upgrading DigiByte from v8.22 to v8.23 by merging a pre-converted Bitcoin v23.2 codebase, eliminating thousands of naming conflicts through automated preprocessing.

## Migration Strategy Overview

1. **Clone Bitcoin v23.2** - Get clean Bitcoin v23.2 release
2. **Pre-convert Bitcoin** - Apply DigiByte naming conventions to entire Bitcoin codebase
3. **Merge into DigiByte** - Merge pre-converted Bitcoin into DigiByte v8.22
4. **Resolve conflicts** - Handle only functional conflicts (not naming)
5. **Validate & Test** - Ensure all DigiByte features remain intact

## Phase 1: Bitcoin Codebase Preparation

### 1.1 Clone Bitcoin v23.2

```bash
# Clone Bitcoin repository
git clone https://github.com/bitcoin/bitcoin.git bitcoin-v23.2-for-digibyte
cd bitcoin-v23.2-for-digibyte

# Checkout v23.2 tag
git checkout v23.2
git checkout -b digibyte-naming-conversion
```

### 1.2 Automated Naming Conversions

**CRITICAL: Run these in specific order to avoid double-replacements**

#### Pre-conversion Validation
```bash
# Create a backup branch before any modifications
git branch backup-original-bitcoin-v23.2

# Document current state
find . -type f -name "*.cpp" -o -name "*.h" | wc -l > file_count_before.txt
git ls-files | sort > files_before.txt
```

#### Step 1: File Renaming
```bash
# Rename files containing "bitcoin" (case-insensitive)
find . -name "*bitcoin*" -o -name "*Bitcoin*" | while read file; do
    newfile=$(echo "$file" | sed -e 's/bitcoin/digibyte/g' -e 's/Bitcoin/DigiByte/g')
    if [ "$file" != "$newfile" ]; then
        git mv "$file" "$newfile"
    fi
done

# Rename BTC references in filenames
find . -name "*btc*" -o -name "*BTC*" | while read file; do
    newfile=$(echo "$file" | sed -e 's/btc/dgb/g' -e 's/BTC/DGB/g')
    if [ "$file" != "$newfile" ]; then
        git mv "$file" "$newfile"
    fi
done
```

#### Step 2: Binary and Library Names
```bash
# Binary executables
find . -type f -name "*.cpp" -o -name "*.h" -o -name "*.mk" -o -name "*.am" -o -name "*.ac" -o -name "*.py" -o -name "*.sh" | xargs sed -i \
    -e 's/bitcoind/digibyted/g' \
    -e 's/bitcoin-cli/digibyte-cli/g' \
    -e 's/bitcoin-tx/digibyte-tx/g' \
    -e 's/bitcoin-wallet/digibyte-wallet/g' \
    -e 's/bitcoin-qt/digibyte-qt/g' \
    -e 's/bitcoin-util/digibyte-util/g' \
    -e 's/bitcoin-chainstate/digibyte-chainstate/g'

# Library names
find . -type f -name "*.cpp" -o -name "*.h" -o -name "*.mk" -o -name "*.am" -o -name "*.ac" | xargs sed -i \
    -e 's/libbitcoin/libdigibyte/g' \
    -e 's/LIBBITCOIN/LIBDIGIBYTE/g' \
    -e 's/bitcoinconsensus/digibyteconsensus/g' \
    -e 's/BITCOINCONSENSUS/DIGIBYTECONSENSUS/g'
```

#### Step 3: Code Element Replacements
```bash
# Header guards and macros
find . -type f -name "*.h" | xargs sed -i \
    -e 's/BITCOIN_/DIGIBYTE_/g' \
    -e 's/_BITCOIN_H/_DIGIBYTE_H/g'

# Class and namespace names
find . -type f -name "*.cpp" -o -name "*.h" | xargs sed -i \
    -e 's/BitcoinGUI/DigiByteGUI/g' \
    -e 's/BitcoinUnits/DigiByteUnits/g' \
    -e 's/BitcoinApplication/DigiByteApplication/g' \
    -e 's/BitcoinCore/DigiByteCore/g' \
    -e 's/BitcoinTestFramework/DigiByteTestFramework/g' \
    -e 's/BITCOIN_CONF_FILENAME/DIGIBYTE_CONF_FILENAME/g'

# Currency codes
find . -type f -name "*.cpp" -o -name "*.h" -o -name "*.py" -o -name "*.md" | xargs sed -i \
    -e 's/\bBTC\b/DGB/g' \
    -e 's/\bbtc\b/dgb/g' \
    -e 's/\bXBT\b/DGB/g'

# Network and project names
find . -type f -name "*.cpp" -o -name "*.h" -o -name "*.py" -o -name "*.md" -o -name "*.txt" | xargs sed -i \
    -e 's/Bitcoin Core/DigiByte Core/g' \
    -e 's/Bitcoin network/DigiByte network/g' \
    -e 's/Bitcoin protocol/DigiByte protocol/g' \
    -e 's/Bitcoin address/DigiByte address/g' \
    -e 's/bitcoin\.org/digibyte\.org/g' \
    -e 's/bitcoin\.it/digibyte\.it/g'

# Config and data directory names
find . -type f -name "*.cpp" -o -name "*.h" -o -name "*.py" | xargs sed -i \
    -e 's/\.bitcoin/\.digibyte/g' \
    -e 's/"bitcoin"/"digibyte"/g' \
    -e 's/bitcoin\.conf/digibyte\.conf/g'
```

#### Step 4: Documentation and Comments
```bash
# Update documentation
find . -type f -name "*.md" -o -name "*.txt" | xargs sed -i \
    -e 's/Bitcoin/DigiByte/g' \
    -e 's/bitcoin/digibyte/g' \
    -e 's/BITCOIN/DIGIBYTE/g'

# Update copyright headers (add DigiByte copyright)
find . -type f -name "*.cpp" -o -name "*.h" | xargs sed -i \
    '/Copyright.*The Bitcoin Core developers/a\
// Copyright (c) 2014-2025 The DigiByte Core developers'
```

#### Step 5: Build System Updates
```bash
# Update Makefiles and build configs
find . -name "Makefile.am" -o -name "configure.ac" | xargs sed -i \
    -e 's/bitcoin/digibyte/g' \
    -e 's/Bitcoin/DigiByte/g' \
    -e 's/BITCOIN/DIGIBYTE/g'

# Update package names
sed -i 's/org\.bitcoin/org\.digibyte/g' configure.ac
sed -i 's/package="bitcoin"/package="digibyte"/g' configure.ac
```

#### Step 6: Additional DigiByte-Specific Replacements
```bash
# Replace satoshi references with digi-satoshi where appropriate
find . -type f -name "*.cpp" -o -name "*.h" -o -name "*.py" | xargs sed -i \
    -e 's/satoshi/digi-satoshi/g' \
    -e 's/Satoshi/Digi-Satoshi/g' \
    -e 's/SATOSHI/DIGI-SATOSHI/g'

# Update test framework references
find test/ -type f -name "*.py" | xargs sed -i \
    -e 's/BitcoinTestFramework/DigiByteTestFramework/g' \
    -e 's/bitcoin_test_framework/digibyte_test_framework/g'

# Update RPC help text
find . -type f -name "*.cpp" | xargs sed -i \
    -e 's/"bitcoin:/"digibyte:/g' \
    -e 's/bitcoind/digibyted/g'
```

### 1.3 DO NOT CONVERT - Preserve for Merge Conflict Resolution

These will be handled during merge to preserve DigiByte functionality:

- Network parameters (ports, magic bytes)
- Genesis block data
- Block timing constants
- Address prefixes
- Consensus parameters
- Algorithm-specific code (will be added, not replaced)

### 1.4 Commit the Pre-converted Bitcoin

```bash
git add -A
git commit -m "Pre-convert Bitcoin v23.2 to DigiByte naming conventions

This commit applies DigiByte naming conventions to the entire Bitcoin v23.2
codebase to simplify the merge process. No functional changes were made.

Conversions applied:
- Binary names: bitcoind -> digibyted, etc.
- Library names: libbitcoin -> libdigibyte
- Currency codes: BTC -> DGB
- Project names: Bitcoin Core -> DigiByte Core
- Header guards: BITCOIN_ -> DIGIBYTE_
- Class names: Bitcoin* -> DigiByte*
- Config paths: .bitcoin -> .digibyte"
```

## Phase 2: Merge Process

### 2.1 Prepare DigiByte Repository

```bash
cd /path/to/digibyte
git checkout digibyte-8.22
git checkout -b digibyte-8.23-merge

# Create pre-merge backup
git tag pre-merge-backup

# Add pre-converted Bitcoin as remote
git remote add bitcoin-converted /path/to/bitcoin-v23.2-for-digibyte
git fetch bitcoin-converted

# Verify we're merging the right commit
git log bitcoin-converted/digibyte-naming-conversion --oneline -n 1
```

### 2.2 Execute the Merge

```bash
# Start with recursive merge using 'ours' strategy for conflicts
# This attempts a real merge but auto-resolves conflicts in our favor
git merge bitcoin-converted/digibyte-naming-conversion \
    --strategy=recursive \
    --strategy-option=ours \
    --no-commit \
    --no-ff

# Review the merge status
git status --porcelain > merge_status.txt

# For files that need manual attention, use mergetool
git mergetool --no-prompt
```

### 2.3 Selective File Restoration

Restore DigiByte-specific files that must not be replaced:

```bash
# Critical DigiByte files to preserve completely
git checkout HEAD -- src/primitives/block.h
git checkout HEAD -- src/primitives/block.cpp
git checkout HEAD -- src/chain.h
git checkout HEAD -- src/chain.cpp
git checkout HEAD -- src/crypto/hashgroestl.h
git checkout HEAD -- src/crypto/hashqubit.h
git checkout HEAD -- src/crypto/hashskein.h
git checkout HEAD -- src/crypto/hashodo.h
git checkout HEAD -- src/crypto/odocrypt.h
git checkout HEAD -- src/crypto/odocrypt.cpp
git checkout HEAD -- src/crypto/scrypt.h
git checkout HEAD -- src/crypto/scrypt.cpp
git checkout HEAD -- src/crypto/sph_*
git checkout HEAD -- src/dandelion.cpp
git checkout HEAD -- src/dandelion.h
git checkout HEAD -- src/chainparams.cpp
git checkout HEAD -- src/chainparamsbase.cpp
git checkout HEAD -- src/chainparamsseeds.h

# Additional DigiByte-specific files found in codebase analysis
git checkout HEAD -- src/crypto/KeccakP-800-SnP.h
git checkout HEAD -- src/consensus/params.h  # Partial restore - needs manual merge
git checkout HEAD -- src/validation.cpp      # Partial restore - needs manual merge
```

## Phase 3: Conflict Resolution Strategy

### 3.1 File Categories and Resolution Rules

#### Category A: PRESERVE DIGIBYTE (No Bitcoin Changes)
These files contain DigiByte-specific functionality that doesn't exist in Bitcoin:

```yaml
preserve_digibyte_completely:
  - src/crypto/hashgroestl.h
  - src/crypto/hashqubit.h
  - src/crypto/hashskein.h
  - src/crypto/hashodo.h
  - src/crypto/odocrypt.*
  - src/crypto/sph_*.h
  - src/crypto/KeccakP-800-SnP.h
  - src/dandelion.*
  - src/stempool.h  # Dandelion++ stem transaction pool
  - All multi-algo mining code
```

**Resolution**: `git checkout HEAD -- <file>`

#### Category B: CAREFUL MERGE (Manual Resolution Required)
These files need Bitcoin improvements while preserving DigiByte logic:

```yaml
careful_merge:
  - src/primitives/block.* # Keep multi-algo, merge other improvements
  - src/chain.* # Keep algo arrays, merge other improvements
  - src/pow.* # Keep V1-V4 difficulty algorithms, merge other improvements
  - src/validation.cpp # Keep custom rewards, merge validation improvements
  - src/consensus/params.h # Keep DigiByte parameters, add new Bitcoin params
  - src/rpc/mining.cpp # Keep multi-algo RPCs, merge other improvements
  - src/rpc/blockchain.cpp # Keep custom difficulty RPCs, merge others
  - src/miner.* # Keep multi-algo mining, merge efficiency improvements
```

**Resolution**: Manual merge, line by line

#### Category C: SMART MERGE (Network Parameters)
These files need special handling for network-specific values:

```yaml
network_parameters:
  - src/chainparams.cpp # Merge structure, keep DigiByte values
  - src/chainparamsbase.cpp # Merge structure, keep DigiByte values
  - src/chainparamsseeds.h # Keep DigiByte seeds
  - src/net.cpp # Keep DigiByte ports and magic bytes
  - src/protocol.cpp # Keep DigiByte protocol specifics
```

**Resolution**: Take Bitcoin structure, restore DigiByte values

#### Category D: TAKE BITCOIN (Already Pre-converted)
These files can take Bitcoin changes as they're already converted:

```yaml
take_bitcoin_improvements:
  - src/wallet/* # All wallet improvements
  - src/qt/* # All GUI improvements
  - src/util/* # All utility improvements
  - src/test/* # Test framework improvements
  - src/bench/* # Benchmarking improvements
  - Most RPC files (except mining/blockchain)
```

**Resolution**: Already handled by pre-conversion

### 3.2 Conflict Resolution Process

For each conflicted file:

1. **Identify category** using the rules above
2. **Apply resolution strategy**:
   ```bash
   # Category A: Preserve DigiByte
   git checkout HEAD -- <file>

   # Category B: Manual merge
   # Open in merge tool and carefully merge

   # Category C: Smart merge
   # Take Bitcoin version, then restore DigiByte values

   # Category D: Take Bitcoin
   git add <file>
   ```

3. **Validate the resolution**:
   ```bash
   # Compile check
   make -j$(nproc)

   # Feature check (varies by file)
   ./test/functional/digibyte_multialgo.py  # If mining-related
   ./test/functional/digibyte_dandelion.py  # If network-related
   ```

### 3.3 Key Merge Conflicts to Expect

#### 1. Block Header Structure
```cpp
// Bitcoin has:
class CBlockHeader {
    // Standard fields
};

// DigiByte needs:
class CBlockHeader {
    // Standard fields
    // + GetAlgo(), SetAlgo(), GetPoWAlgoHash()
};
```
**Resolution**: Merge Bitcoin improvements, add back DigiByte methods

#### 2. Consensus Parameters
```cpp
// Bitcoin has different:
consensus.nPowTargetSpacing = 10 * 60; // 10 minutes

// DigiByte needs:
consensus.nPowTargetSpacing = 15; // 15 seconds
consensus.multiAlgoTargetSpacing = 30 * 5; // 150 seconds
```
**Resolution**: Keep all DigiByte timing parameters

#### 3. Mining RPC Calls
```cpp
// DigiByte has additional:
- getmininginfo shows per-algorithm stats
- getdifficulty accepts algorithm parameter
- getnetworkhashps works per-algorithm
```
**Resolution**: Merge Bitcoin improvements, preserve DigiByte additions

## Phase 4: Testing and Validation

### 4.1 Compilation Testing

```bash
# Clean build
make clean
./autogen.sh
./configure --enable-debug
make -j$(nproc)

# Build all targets
make check
```

### 4.2 Unit Test Suite

```bash
# Run C++ unit tests
./src/test/test_digibyte

# Run specific DigiByte tests
./src/test/test_digibyte --run_test=multialgo_tests
./src/test/test_digibyte --run_test=odocrypt_tests
./src/test/test_digibyte --run_test=digishield_tests
```

### 4.3 Functional Test Suite

```bash
# Run all functional tests
./test/functional/test_runner.py --extended

# Run DigiByte-specific tests
./test/functional/digibyte_multialgo.py
./test/functional/digibyte_dandelion.py
./test/functional/digibyte_difficulty.py
./test/functional/digibyte_rewards.py
./test/functional/digibyte_odocrypt.py
```

### 4.4 Feature Validation Checklist

- [ ] Multi-algorithm mining works (all 5 algorithms)
- [ ] Difficulty adjustment algorithms (V1-V4) work correctly
- [ ] Dandelion++ transaction routing functions
- [ ] ODO/Odocrypt activates at correct height (9,112,320)
- [ ] Block rewards follow DigiByte schedule (6 periods)
- [ ] 15-second block timing maintained
- [ ] Network connections use DigiByte ports (12024/12025)
- [ ] DigiByte address formats work (D=30, S=63, dgb)
- [ ] Genesis block ("USA Today: 10/Jan/2014") valid
- [ ] All consensus rules enforced correctly
- [ ] No RBF (fRbfEnabled = false)
- [ ] Stempool functions for Dandelion++
- [ ] Custom fee policies for 15-second blocks
- [ ] All 30+ checkpoints validate correctly

### 4.5 Performance Validation

```bash
# Sync test (first 1 million blocks)
time ./src/digibyted -reindex-chainstate -stopatheight=1000000

# Mining test (each algorithm)
for algo in sha256d scrypt groestl skein qubit; do
    ./src/digibyte-cli generatetoaddress 100 <address> $algo
done

# Memory usage under load
valgrind --tool=massif ./src/digibyted
```

## Phase 5: Final Integration

### 5.1 Merge Commit

```bash
# After all conflicts resolved and tests pass
git add -A
git commit -m "Merge Bitcoin v23.2 into DigiByte v8.22

This merge brings Bitcoin v23.2 improvements into DigiByte while preserving
all DigiByte-specific functionality including:
- Multi-algorithm mining (5 algorithms)
- Dandelion++ privacy
- DigiShield/MultiShield difficulty adjustment
- ODO/Odocrypt dynamic PoW
- 15-second block timing
- Custom block reward schedule

The Bitcoin codebase was pre-converted to DigiByte naming conventions
before merging to minimize conflicts.

Conflicts resolved in:
[List all files with manual conflict resolution]

All tests passing:
- Unit tests: ✓
- Functional tests: ✓
- DigiByte feature tests: ✓"
```

### 5.2 Post-Merge Verification

1. **Full Mainnet Sync Test**
   ```bash
   ./src/digibyted
   ```

2. **Testnet Deployment**
   ```bash
   ./src/digibyted -testnet
   ```

3. **Community Testing Phase**
   - Release beta builds
   - Monitor for issues
   - Collect performance metrics

## Advantages of Merge Approach

1. **Fewer Conflicts**: Pre-conversion eliminates ~90% of conflicts
2. **Clearer History**: Single merge commit vs 3,000+ cherry-picks
3. **Easier Debugging**: Can diff against Bitcoin v23.2 directly
4. **Faster Process**: Bulk operations vs commit-by-commit
5. **Better Attribution**: Preserves Bitcoin's git history

## Risk Mitigation

1. **Backup Strategy**: Keep original v8.22 branch untouched
2. **Incremental Testing**: Test after each major subsystem merge
3. **Rollback Plan**: Can revert to cherry-pick approach if needed
4. **Feature Flags**: Add toggles for new Bitcoin features initially

## Timeline Estimate

- Phase 1 (Pre-conversion): 1-2 days
- Phase 2 (Initial merge): 1 day
- Phase 3 (Conflict resolution): 1-2 weeks
- Phase 4 (Testing): 1 week
- Phase 5 (Integration): 3-5 days

**Total: 3-4 weeks** (vs 2-3 months for cherry-pick approach)

## Critical DigiByte Constants Reference

```cpp
// Multi-Algorithm Mining
const int NUM_ALGOS = 5;
enum {
    ALGO_SHA256D = 0,
    ALGO_SCRYPT = 1,
    ALGO_GROESTL = 2,
    ALGO_SKEIN = 3,
    ALGO_QUBIT = 4,
    ALGO_ODO = 7
};

// Network Parameters
nDefaultPort = 12024;              // Mainnet
nDefaultPort = 12025;              // Testnet
pchMessageStart = {0xfa,0xc3,0xb6,0xda}; // Magic bytes

// Timing
nPowTargetSpacing = 60 / 4;       // 15 seconds
multiAlgoTargetSpacing = 30 * 5;  // 150 seconds

// Key Heights
multiAlgoDiffChangeTarget = 145000;
alwaysUpdateDiffChangeTarget = 400000;
workComputationChangeTarget = 1430000;
OdoHeight = 9112320;
```

## Success Criteria

- [ ] All Bitcoin v23.2 improvements integrated
- [ ] Zero regression in DigiByte functionality
- [ ] All tests passing
- [ ] Performance within 5% of v8.22
- [ ] Clean compilation on all platforms
- [ ] Successful mainnet sync
- [ ] Community acceptance
