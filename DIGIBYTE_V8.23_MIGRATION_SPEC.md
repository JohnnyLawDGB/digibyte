# DigiByte v8.23 Migration Specification

## Executive Summary

This specification defines the precise process for upgrading DigiByte from v8.22 to v8.23 by cherry-picking 3,031 commits from Bitcoin v22-final to v23.2, while preserving all DigiByte-specific functionality.

## Migration Objectives

- Incorporate Bitcoin v23.x improvements
- Preserve ALL DigiByte unique features
- Maintain 100% backward compatibility
- Zero regression in functionality
- Proper attribution to Bitcoin developers

## Critical DigiByte Features to Preserve

### 1. Multi-Algorithm Mining System

**Core Files (NEVER MODIFY):**
- `src/primitives/block.h` - Algorithm definitions and constants
- `src/primitives/block.cpp` - Algorithm implementation functions
- `src/chain.h` - CBlockIndex with `lastAlgoBlocks[NUM_ALGOS_IMPL]`
- `src/chain.cpp` - Algorithm work factor calculations

**Key Constants:**
```cpp
enum {
    ALGO_SHA256D  = 0,
    ALGO_SCRYPT   = 1,
    ALGO_GROESTL  = 2,
    ALGO_SKEIN    = 3,
    ALGO_QUBIT    = 4,
    ALGO_ODO      = 7,
    NUM_ALGOS_IMPL
};

const int NUM_ALGOS = 5;

enum {
    BLOCK_VERSION_ALGO           = (15 << 8),
    BLOCK_VERSION_SCRYPT         = (0 << 8),
    BLOCK_VERSION_SHA256D        = (2 << 8),
    BLOCK_VERSION_GROESTL        = (4 << 8),
    BLOCK_VERSION_SKEIN          = (6 << 8),
    BLOCK_VERSION_QUBIT          = (8 << 8),
    BLOCK_VERSION_ODO            = (14 << 8),
};
```

**Critical Functions:**
- `GetAlgoByName()`, `GetAlgoName()` - Algorithm string/enum conversion
- `GetVersionForAlgo()` - Block version bit encoding
- `CBlockHeader::GetAlgo()` - Extract algorithm from block version
- `CBlockHeader::SetAlgo()` - Set algorithm in block version
- `GetAlgoWorkFactor()` - Algorithm-specific work multipliers
- `IsAlgoActive()` - Determine which algorithms are active at given height

### 2. Multi-Algorithm Hash Functions

**Hash Implementation Files (PRESERVE EXACTLY):**
- `src/crypto/hashgroestl.h` - Groestl-SHA256 hash
- `src/crypto/hashqubit.h` - 5-function Qubit hash
- `src/crypto/hashskein.h` - Skein-SHA256 hash
- `src/crypto/hashodo.h` - ODO (Odocrypt) hash
- `src/crypto/scrypt.h/.cpp` - Scrypt implementation
- All SPH (sphlib) implementations: `sph_*.h` files

**PoW Hash Function:**
```cpp
uint256 CBlockHeader::GetPoWAlgoHash(const Consensus::Params& params) const
{
    switch (GetAlgo()) {
        case ALGO_SHA256D: return GetHash();
        case ALGO_SCRYPT: // Scrypt implementation
        case ALGO_GROESTL: return HashGroestl(BEGIN(nVersion), END(nNonce));
        case ALGO_SKEIN: return HashSkein(BEGIN(nVersion), END(nNonce));
        case ALGO_QUBIT: return HashQubit(BEGIN(nVersion), END(nNonce));
        case ALGO_ODO: return HashOdo(BEGIN(nVersion), END(nNonce), key);
    }
}
```

### 3. ODO/Odocrypt Dynamic PoW

**Core Files (CRITICAL):**
- `src/crypto/odocrypt.h/.cpp` - Main ODO implementation
- `src/crypto/hashodo.h` - ODO hash wrapper
- `src/crypto/KeccakP-800-SnP.h` - Keccak permutation

**Key Functions:**
- `OdoKey()` - Time-based key generation
- `OdoCrypt::Encrypt()` - Main encryption function
- `nOdoShapechangeInterval` - Algorithm change interval (10 days)

### 4. Advanced Difficulty Adjustment (DigiShield/MultiShield)

**Core Files (PRESERVE LOGIC):**
- `src/pow.cpp` - All difficulty adjustment versions
- `src/pow.h` - Function declarations

**Difficulty Adjustment Versions:**
- `GetNextWorkRequiredV1()` - Original DigiShield (pre-145,000)
- `GetNextWorkRequiredV2()` - MultiShield (145,000-400,000)
- `GetNextWorkRequiredV3()` - Enhanced MultiShield (400,000-1,430,000)
- `GetNextWorkRequiredV4()` - Current system (1,430,000+)

**Key Parameters:**
```cpp
consensus.multiAlgoDiffChangeTarget = 145000;    // MultiAlgo activation
consensus.alwaysUpdateDiffChangeTarget = 400000; // MultiShield activation
consensus.workComputationChangeTarget = 1430000; // DigiSpeed activation
consensus.nAveragingInterval = 10;               // 10 blocks averaging
consensus.multiAlgoTargetSpacing = 30*5;        // 150 seconds
consensus.multiAlgoTargetSpacingV4 = 15*5;      // 75 seconds
```

### 5. Dandelion++ Privacy Implementation

**Core Files (PRESERVE COMPLETELY):**
- `src/dandelion.cpp` - Main Dandelion implementation
- `src/net.h` - Dandelion networking constants

**Key Functions:**
- `isDandelionInbound()`, `isLocalDandelionDestinationSet()`
- `setLocalDandelionDestination()`, `getDandelionDestination()`
- `localDandelionDestinationPushInventory()`
- `insertDandelionEmbargo()`, `isTxDandelionEmbargoed()`
- `DandelionShuffle()`, `ThreadDandelionShuffle()`

### 6. Custom Block Timing and Rewards

**Block Timing:**
- 15-second block target (vs Bitcoin's 10 minutes)
- Multi-algorithm balancing (15 seconds per algorithm = 75 seconds total cycle)

**Reward Schedule (`GetBlockSubsidy()`):**
```cpp
// Period I (0-1440): 72,000 DGB
// Period II (1440-5760): 16,000 DGB
// Period III (5760-67200): 8,000 DGB
// Period IV (67200-400000): 8,000 DGB with 0.5% reduction every 10,080 blocks
// Period V (400000-1430000): 2,459 DGB with 1% monthly reduction
// Period VI (1430000+): Fixed ~21 year emission schedule
```

### 7. Network Parameters & Consensus

**Message Start Bytes:**
```cpp
// Mainnet
pchMessageStart[0] = 0xfa;
pchMessageStart[1] = 0xc3;
pchMessageStart[2] = 0xb6;
pchMessageStart[3] = 0xda;
nDefaultPort = 12024;
```

**Address Prefixes:**
```cpp
base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,30); // 'D'
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,63); // 'S'
bech32_hrp = "dgb"; // Bech32 prefix
```

**Genesis Block:**
```cpp
genesis = CreateGenesisBlock(1389388394, 2447652, 0x1e0ffff0, 1, 8000);
```

### 8. Activation Heights & Hard Forks

**Critical Heights:**
```cpp
consensus.multiAlgoDiffChangeTarget = 145000;    // MultiAlgo Hard Fork
consensus.alwaysUpdateDiffChangeTarget = 400000; // MultiShield Hard Fork
consensus.workComputationChangeTarget = 1430000; // DigiSpeed Hard Fork
consensus.algoSwapChangeTarget = 9100000;       // Odo preparation
consensus.OdoHeight = 9112320;                  // Odo activation
consensus.BIP34Height = consensus.BIP65Height = consensus.BIP66Height = 4394880;
consensus.CSVHeight = consensus.SegwitHeight = 4394880;
consensus.ReserveAlgoBitsHeight = 8547840;      // Algorithm bit reservation
```

## Commit Processing Specification

### Batch Processing Rules

1. Process commits continuously in chronological order
2. Testing checkpoint every 250 commits
3. No skipping of failed commits - must resolve before continuing
4. Maintain chronological order of commits
5. Document all conflicts and resolutions
6. Must pass ALL tests at checkpoint before proceeding to next 250 commits

### Testing Gates (Every 250 Commits)

**Testing Order (MUST be followed sequentially):**

1. **C++ Unit Tests First:**
```bash
make check
```

2. **Python Functional Tests Second:**
```bash
test/functional/test_runner.py --extended

# DigiByte-specific functional tests
test/functional/digibyte_multialgo.py
test/functional/digibyte_dandelion.py
test/functional/digibyte_difficulty.py
test/functional/digibyte_rewards.py
```

3. **Compilation Test Last:**
```bash
make clean && ./autogen.sh && ./configure && make -j$(nproc)
```

**Testing Protocol:**
- If C++ tests fail, fix issues before proceeding to Python tests
- If Python tests fail, fix issues before proceeding to compilation
- If compilation fails, fix issues before proceeding to next batch
- Only proceed to next 250 commits when ALL tests pass

## Automated Conflict Resolution System

### File-Specific Preservation Rules

```yaml
# NEVER MODIFY - Preserve DigiByte implementation
preserve_completely:
  - src/primitives/block.h
  - src/primitives/block.cpp
  - src/chain.h
  - src/chain.cpp
  - src/crypto/hashgroestl.h
  - src/crypto/hashqubit.h
  - src/crypto/hashskein.h
  - src/crypto/hashodo.h
  - src/crypto/odocrypt.h
  - src/crypto/odocrypt.cpp
  - src/crypto/scrypt.h
  - src/crypto/scrypt.cpp
  - src/dandelion.cpp
  - src/dandelion.h
  - src/chainparams.cpp
  - src/chainparams.h
  - src/chainparamsbase.cpp
  - src/chainparamsseeds.h

# CAREFUL MERGE - Preserve DigiByte logic, adapt Bitcoin improvements
careful_merge:
  - src/pow.cpp
  - src/pow.h
  - src/validation.cpp
  - src/validation.h
  - src/consensus/params.h
  - src/script/digibyteconsensus.cpp
  - src/script/digibyteconsensus.h

# PREFER BITCOIN - Take Bitcoin changes, apply DigiByte naming
prefer_bitcoin:
  - src/rpc/*.cpp (except mining-related)
  - src/wallet/*.cpp
  - src/qt/*.cpp
  - src/util/*.cpp
  - src/test/*.cpp
  - src/bench/*.cpp

# MINING SPECIFIC - Preserve DigiByte multi-algo functionality
mining_specific:
  - src/rpc/mining.cpp
  - src/rpc/blockchain.cpp (difficulty/hashrate functions)
  - src/miner.cpp
  - src/miner.h
```

### Conflict Resolution Process

1. **Automatic Resolution:**
   - Apply preservation rules based on file classification
   - Use automated tools to resolve naming conflicts
   - Preserve DigiByte-specific constants and enums

2. **Manual Resolution Required:**
   - Changes to consensus-critical functions
   - Modifications to algorithm-specific code
   - Alterations to difficulty adjustment logic
   - Changes to block reward calculations

3. **Validation Steps:**
   - Verify all DigiByte features still function
   - Check multi-algorithm mining works
   - Validate Dandelion routing
   - Confirm difficulty adjustments
   - Test block reward calculations

## Naming Convention Rules

### Automated Replacements

```yaml
binaries:
  bitcoind: digibyted
  bitcoin-cli: digibyte-cli
  bitcoin-tx: digibyte-tx
  bitcoin-wallet: digibyte-wallet
  bitcoin-qt: digibyte-qt

libraries:
  libbitcoin*: libdigibyte*
  bitcoinconsensus: digibyteconsensus

code_elements:
  BITCOIN_*_H: DIGIBYTE_*_H
  BitcoinGUI: DigiByteGUI
  BitcoinUnits: DigiByteUnits
  BTC: DGB
  "Bitcoin Core": "DigiByte Core"
```

### Copyright Format

```cpp
// Copyright (c) <year-range> The Bitcoin Core developers
// Copyright (c) 2014-2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
```

## Cherry-Pick Process

### Step 1: Prepare Commit

```bash
git cherry-pick -n <commit-hash>  # No auto-commit
```

### Step 2: Apply Conflict Resolution

```bash
# Check file classification
./tools/classify_changes.py --commit <hash>

# Apply preservation rules
./tools/apply_preservation_rules.py --commit <hash>

# Apply naming conventions
./tools/apply_naming_conventions.py

# Update copyright headers
./tools/update_copyright_headers.py
```

### Step 3: Validate Changes

```bash
# Verify DigiByte features
./tools/validate_digibyte_features.py

# Check compilation
make clean && ./autogen.sh && ./configure && make -j$(nproc)

# Run affected tests
./tools/run_affected_tests.py --commit <hash>
```

### Step 4: Commit with Attribution

```bash
git commit -m "Merge bitcoin/<hash>: <original message>

Cherry-picked from Bitcoin Core commit <hash>
Original author: <author>

[DigiByte: Applied preservation rules and naming conventions]
Conflicts resolved in: <list of files>
Features preserved: <list of features>"
```

## Migration History & Tracking

### Commit Log Format

```markdown
## Checkpoint <N> (Commits <start>-<end>)
- Started: <timestamp>
- Commits processed: 250
- Clean merges: <count>
- Conflicts resolved: <count>
- Status: COMPLETE/FAILED
- Duration: <time>

### Conflicts Resolved:
- <commit-hash>: <file> - <resolution description>
- <commit-hash>: <file> - <resolution description>

### Testing Results (Sequential Order):
1. C++ Unit Tests (make check): PASS/FAIL
   - Failures fixed: <count>
   - Retry attempts: <count>
2. Python Functional Tests: PASS/FAIL
   - test_runner.py --extended: PASS/FAIL
   - digibyte_multialgo.py: PASS/FAIL
   - digibyte_dandelion.py: PASS/FAIL
   - digibyte_difficulty.py: PASS/FAIL
   - digibyte_rewards.py: PASS/FAIL
   - Failures fixed: <count>
   - Retry attempts: <count>
3. Compilation Test: PASS/FAIL
   - Build errors fixed: <count>
   - Retry attempts: <count>

### Features Validated:
- ✓ Multi-algorithm mining
- ✓ Dandelion routing
- ✓ Difficulty adjustments
- ✓ Block rewards
- ✓ Network parameters
```

### Issue Tracking

```markdown
## Issue: <commit-hash>
- Type: Conflict/Test Failure/Build Error
- File: <path>
- Description: <detailed description>
- Resolution: <how it was resolved>
- Impact: <any changes to DigiByte behavior>
- Validation: <tests run to verify fix>
```

## Success Criteria

### Per Commit

- [ ] Clean cherry-pick or resolved conflicts
- [ ] Naming conventions applied
- [ ] Copyright headers updated
- [ ] No compilation warnings
- [ ] Unit tests pass for affected modules

### Per Testing Checkpoint (250 commits)

- [ ] All 250 commits successfully applied
- [ ] C++ unit tests pass (make check)
- [ ] Python functional tests pass (test_runner.py --extended)
- [ ] DigiByte-specific tests pass
- [ ] Clean compilation on all platforms
- [ ] DigiByte features intact
- [ ] No performance regression
- [ ] All conflict resolutions documented

### Final Release

- [ ] All 3,031 commits migrated
- [ ] Zero functionality regression
- [ ] Performance within 5% of v8.22
- [ ] Full mainnet sync successful
- [ ] All DigiByte features working
- [ ] Community testing approved

---

# Master AI Prompt for Cherry-Pick Process

You are a DigiByte Core developer tasked with cherry-picking Bitcoin commits to upgrade DigiByte from v8.22 to v8.23. You must preserve ALL DigiByte-specific features while incorporating Bitcoin improvements.

## Your Mission

Cherry-pick commits from Bitcoin v22-final to v23.2 (3,031 total commits) into DigiByte, with testing checkpoints every 250 commits.

## CRITICAL: DigiByte Features to Preserve

### 1. Multi-Algorithm Mining (5 algorithms)
- **SHA256D, Scrypt, Groestl, Skein, Qubit/ODO**
- **Files**: `src/primitives/block.*`, `src/chain.*`, `src/crypto/hash*.h`
- **Functions**: `GetAlgoByName()`, `GetAlgoWorkFactor()`, `CBlockHeader::GetAlgo()`
- **Constants**: `ALGO_*` enums, `BLOCK_VERSION_*` constants

### 2. Difficulty Adjustment Algorithms (V1-V4)
- **Files**: `src/pow.cpp`, `src/pow.h`
- **Functions**: `GetNextWorkRequiredV1()` through `GetNextWorkRequiredV4()`
- **Heights**: 145000, 400000, 1430000 (hard fork activation points)

### 3. Dandelion++ Privacy
- **Files**: `src/dandelion.cpp`, `src/dandelion.h`
- **Functions**: All Dandelion-related functions
- **Feature**: Transaction routing privacy

### 4. ODO/Odocrypt Dynamic PoW
- **Files**: `src/crypto/odocrypt.*`, `src/crypto/hashodo.h`
- **Activation**: Block 9,112,320
- **Feature**: Shape-changing PoW every 10 days

### 5. Network Parameters
- **Ports**: 12024 (mainnet), 14022 (RPC)
- **Block time**: 15 seconds
- **Address prefixes**: 'D' (P2PKH), 'S' (P2SH)
- **Bech32**: "dgb"
- **Genesis**: Unique DigiByte genesis block

### 6. Block Rewards
- **Custom schedule**: 72,000 → 16,000 → 8,000 → 2,459 → exponential decay
- **File**: `src/validation.cpp` (`GetBlockSubsidy()`)

## File Classification Rules

### NEVER MODIFY (Preserve DigiByte completely):
- `src/primitives/block.*`
- `src/chain.*`
- `src/crypto/hash*.h`
- `src/crypto/odocrypt.*`
- `src/crypto/scrypt.*`
- `src/dandelion.*`
- `src/chainparams.*`

### CAREFUL MERGE (Preserve DigiByte logic):
- `src/pow.cpp` - Difficulty adjustments
- `src/validation.cpp` - Block rewards, consensus
- `src/consensus/params.h` - Consensus parameters
- `src/rpc/mining.cpp` - Mining-related RPC

### PREFER BITCOIN (Apply naming conventions):
- `src/rpc/*.cpp` (except mining)
- `src/wallet/*.cpp`
- `src/qt/*.cpp`
- `src/util/*.cpp`

## Process for Each Commit

### 1. Cherry-pick the commit
```bash
git cherry-pick -n <commit-hash>
```

### 2. Apply conflict resolution
```bash
# Check which files are affected
git status

# Apply preservation rules based on file classification
# For NEVER MODIFY files: Reject Bitcoin changes, keep DigiByte
# For CAREFUL MERGE files: Merge carefully, preserve DigiByte logic
# For PREFER BITCOIN files: Take Bitcoin changes, apply naming
```

### 3. Apply DigiByte naming conventions
```bash
# Replace binary names
sed -i 's/bitcoind/digibyted/g' <files>
sed -i 's/bitcoin-cli/digibyte-cli/g' <files>

# Replace library names
sed -i 's/libbitcoin/libdigibyte/g' <files>
sed -i 's/bitcoinconsensus/digibyteconsensus/g' <files>

# Replace header guards
sed -i 's/BITCOIN_/DIGIBYTE_/g' <files>

# Replace class names
sed -i 's/BitcoinGUI/DigiByteGUI/g' <files>
sed -i 's/BitcoinUnits/DigiByteUnits/g' <files>

# Replace currency codes
sed -i 's/BTC/DGB/g' <files>

# Replace project names
sed -i 's/Bitcoin Core/DigiByte Core/g' <files>
```

### 4. Update copyright headers
```cpp
// Add DigiByte copyright below Bitcoin copyright
// Copyright (c) 2009-2023 The Bitcoin Core developers
// Copyright (c) 2014-2024 The DigiByte Core developers
```

### 5. Validate changes
```bash
# Check compilation
make clean && ./autogen.sh && ./configure && make -j$(nproc)

# Run quick tests
make check

# Verify DigiByte features
./test/functional/digibyte_multialgo.py
./test/functional/digibyte_dandelion.py
```

### 6. Commit with attribution
```bash
git commit -m "Merge bitcoin/<hash>: <original message>

Cherry-picked from Bitcoin Core commit <hash>
Original author: <author>

[DigiByte: Applied preservation rules and naming conventions]
Conflicts resolved in: <list of files>
Features preserved: Multi-algo mining, Dandelion++, DigiShield"
```

## Every 250 Commits - MANDATORY CHECKPOINT

### 1. C++ Unit Tests (Run First)
```bash
make check  # Must pass 100%
```
**If this fails:** Debug and fix all C++ unit test failures before proceeding.

### 2. Python Functional Tests (Run Second)
```bash
test/functional/test_runner.py --extended  # Must pass 100%

# DigiByte-specific validation
./test/functional/digibyte_multialgo.py    # Multi-algorithm mining
./test/functional/digibyte_dandelion.py    # Dandelion routing
./test/functional/digibyte_difficulty.py   # Difficulty adjustments
./test/functional/digibyte_rewards.py      # Block rewards
```
**If this fails:** Debug and fix all Python functional test failures before proceeding.

### 3. Compilation Test (Run Last)
```bash
make clean && ./autogen.sh && ./configure && make -j$(nproc)
```
**If this fails:** Debug and fix all compilation errors before proceeding.

### 4. Only proceed if ALL tests pass
If any test in the sequence fails, you must:
1. Stop the testing sequence
2. Debug and fix the failing test
3. Restart the testing sequence from step 1
4. Continue until all tests pass before moving to the next 250 commits

## Conflict Resolution Examples

### Example 1: pow.cpp conflict
```cpp
// Bitcoin change: modifies GetNextWorkRequired
// DigiByte: MUST preserve V1-V4 difficulty algorithms
// Resolution: Integrate Bitcoin improvement without breaking multi-algo logic
```

### Example 2: validation.cpp conflict
```cpp
// Bitcoin change: new validation rule
// DigiByte: Must ensure compatibility with 15-second blocks and custom rewards
// Resolution: Adapt validation for DigiByte parameters
```

### Example 3: chainparams.cpp conflict
```cpp
// Bitcoin change: updates network parameters
// DigiByte: MUST keep DigiByte-specific values
// Resolution: Reject Bitcoin changes, keep DigiByte parameters
```

## Progress Tracking

### Log each checkpoint completion:
```markdown
## Checkpoint <N> Completed (Commits <start>-<end>)
- Commits processed: 250
- Conflicts resolved: <count>
- C++ tests: ✓ PASSED
- Python tests: ✓ PASSED
- Compilation: ✓ PASSED
- DigiByte features: ✓ All validated
- Next: Checkpoint <N+1> (commits <end+1>-<end+250>)
```

### Document all conflicts:
```markdown
## Conflict: <commit-hash>
- File: <path>
- Type: <conflict type>
- Resolution: <how resolved>
- Impact: <none/minimal/significant>
```

## Command Sequence

```bash
# Setup
git remote add bitcoin-upstream https://github.com/bitcoin/bitcoin.git
git fetch bitcoin-upstream --tags

# Get commit list
git log --oneline bitcoin-upstream/v22-final..bitcoin-upstream/v23.2 > bitcoin_commits.txt

# Begin migration
git checkout -b digibyte-8.23-migration

# Process first batch
# ... (follow process above for each commit)
```

## REMEMBER

- **NEVER** compromise DigiByte's unique features
- **ALWAYS** maintain Bitcoin attribution
- **EVERY 250 commits** must pass ALL tests in sequence
- **DOCUMENT** every conflict resolution
- **PRESERVE** multi-algo mining at all costs
- **PROTECT** Dandelion++ privacy features
- **MAINTAIN** difficulty adjustment algorithms
- **KEEP** all network parameters unchanged

Begin with the first 250 commits and process them methodically, testing at the checkpoint.
