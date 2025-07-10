# DigiByte v8.23 Bitcoin Merge Migration Prompt

You are tasked with upgrading DigiByte from v8.22 to v8.23 by merging Bitcoin v23.2 using a pre-conversion approach that eliminates naming conflicts.

## Context

DigiByte is a Bitcoin fork with significant unique features that MUST be preserved:
- Multi-algorithm mining (5 algorithms: SHA256D, Scrypt, Groestl, Skein, Qubit + ODO)
- 15-second block times (vs Bitcoin's 10 minutes)
- Dandelion++ privacy protocol
- Custom difficulty adjustment (DigiShield/MultiShield)
- Unique block reward schedule (6 periods)
- 21 billion total supply cap

## Your Mission

Execute a merge-based migration that:
1. Pre-converts Bitcoin v23.2 to DigiByte naming conventions
2. Merges the converted Bitcoin into DigiByte v8.22
3. Resolves only functional conflicts (naming already handled)
4. Preserves ALL DigiByte functionality

## Step-by-Step Process

### Phase 1: Pre-Convert Bitcoin v23.2

1. **Clone and prepare Bitcoin v23.2:**
```bash
git clone https://github.com/bitcoin/bitcoin.git bitcoin-v23.2-for-digibyte
cd bitcoin-v23.2-for-digibyte
git checkout v23.2
git checkout -b digibyte-naming-conversion
git branch backup-original-bitcoin-v23.2
```

2. **Apply naming conversions IN THIS EXACT ORDER:**

**File Renaming:**
```bash
# Rename files containing "bitcoin"
find . -name "*bitcoin*" -o -name "*Bitcoin*" | while read file; do
    newfile=$(echo "$file" | sed -e 's/bitcoin/digibyte/g' -e 's/Bitcoin/DigiByte/g')
    if [ "$file" != "$newfile" ]; then
        git mv "$file" "$newfile"
    fi
done

# Rename BTC references
find . -name "*btc*" -o -name "*BTC*" | while read file; do
    newfile=$(echo "$file" | sed -e 's/btc/dgb/g' -e 's/BTC/DGB/g')
    if [ "$file" != "$newfile" ]; then
        git mv "$file" "$newfile"
    fi
done
```

**Binary Names:**
```bash
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.mk" -o -name "*.am" -o -name "*.ac" -o -name "*.py" -o -name "*.sh" \) | xargs sed -i \
    -e 's/bitcoind/digibyted/g' \
    -e 's/bitcoin-cli/digibyte-cli/g' \
    -e 's/bitcoin-tx/digibyte-tx/g' \
    -e 's/bitcoin-wallet/digibyte-wallet/g' \
    -e 's/bitcoin-qt/digibyte-qt/g' \
    -e 's/bitcoin-util/digibyte-util/g' \
    -e 's/bitcoin-chainstate/digibyte-chainstate/g'
```

**Library Names:**
```bash
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.mk" -o -name "*.am" -o -name "*.ac" \) | xargs sed -i \
    -e 's/libbitcoin/libdigibyte/g' \
    -e 's/LIBBITCOIN/LIBDIGIBYTE/g' \
    -e 's/bitcoinconsensus/digibyteconsensus/g' \
    -e 's/BITCOINCONSENSUS/DIGIBYTECONSENSUS/g'
```

**Code Elements:**
```bash
# Header guards
find . -type f -name "*.h" | xargs sed -i \
    -e 's/BITCOIN_/DIGIBYTE_/g' \
    -e 's/_BITCOIN_H/_DIGIBYTE_H/g'

# Classes
find . -type f \( -name "*.cpp" -o -name "*.h" \) | xargs sed -i \
    -e 's/BitcoinGUI/DigiByteGUI/g' \
    -e 's/BitcoinUnits/DigiByteUnits/g' \
    -e 's/BitcoinApplication/DigiByteApplication/g' \
    -e 's/BitcoinCore/DigiByteCore/g' \
    -e 's/BitcoinTestFramework/DigiByteTestFramework/g' \
    -e 's/BITCOIN_CONF_FILENAME/DIGIBYTE_CONF_FILENAME/g'

# Currency
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.py" -o -name "*.md" \) | xargs sed -i \
    -e 's/\bBTC\b/DGB/g' \
    -e 's/\bbtc\b/dgb/g' \
    -e 's/\bXBT\b/DGB/g'

# Project names
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.py" -o -name "*.md" -o -name "*.txt" \) | xargs sed -i \
    -e 's/Bitcoin Core/DigiByte Core/g' \
    -e 's/Bitcoin network/DigiByte network/g' \
    -e 's/Bitcoin protocol/DigiByte protocol/g' \
    -e 's/Bitcoin address/DigiByte address/g' \
    -e 's/bitcoin\.org/digibyte\.org/g' \
    -e 's/bitcoin\.it/digibyte\.it/g'

# Paths
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.py" \) | xargs sed -i \
    -e 's/\.bitcoin/\.digibyte/g' \
    -e 's/"bitcoin"/"digibyte"/g' \
    -e 's/bitcoin\.conf/digibyte\.conf/g'
```

**Documentation:**
```bash
find . -type f \( -name "*.md" -o -name "*.txt" \) | xargs sed -i \
    -e 's/Bitcoin/DigiByte/g' \
    -e 's/bitcoin/digibyte/g' \
    -e 's/BITCOIN/DIGIBYTE/g'
```

**Copyright:**
```bash
find . -type f \( -name "*.cpp" -o -name "*.h" \) | xargs sed -i \
    '/Copyright.*The Bitcoin Core developers/a\
// Copyright (c) 2014-2025 The DigiByte Core developers'
```

3. **Commit the pre-converted Bitcoin:**
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

### Phase 2: Execute the Merge

1. **Prepare DigiByte repository:**
```bash
cd /path/to/digibyte
git checkout digibyte-8.22
git checkout -b digibyte-8.23-merge
git tag pre-merge-backup
git remote add bitcoin-converted /path/to/bitcoin-v23.2-for-digibyte
git fetch bitcoin-converted
```

2. **Perform the merge:**
```bash
git merge bitcoin-converted/digibyte-naming-conversion \
    --strategy=recursive \
    --strategy-option=ours \
    --no-commit \
    --no-ff
```

3. **Restore DigiByte-specific files:**
```bash
# Files that MUST NOT be modified
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
git checkout HEAD -- src/crypto/KeccakP-800-SnP.h
git checkout HEAD -- src/dandelion.cpp
git checkout HEAD -- src/dandelion.h
git checkout HEAD -- src/chainparams.cpp
git checkout HEAD -- src/chainparamsbase.cpp
git checkout HEAD -- src/chainparamsseeds.h
```

### Phase 3: Resolve Conflicts

For each conflicted file, categorize and resolve:

**Category A - PRESERVE DIGIBYTE:**
```bash
# DigiByte-only features (no Bitcoin equivalent)
git checkout HEAD -- <file>
```

**Category B - MANUAL MERGE:**
Files requiring careful line-by-line merge:
- `src/pow.cpp` - Keep DigiShield V1-V4
- `src/validation.cpp` - Keep custom rewards
- `src/consensus/params.h` - Keep DigiByte parameters
- `src/rpc/mining.cpp` - Keep multi-algo RPCs

**Category C - SMART MERGE:**
Take Bitcoin structure, restore DigiByte values:
- Network ports: 12024 (main), 12025 (test)
- Block time: 15 seconds
- Address prefixes: D (30), S (63), dgb

**Category D - TAKE BITCOIN:**
Already pre-converted files can use Bitcoin version

### Phase 4: Validate

1. **Compile:**
```bash
make clean
./autogen.sh
./configure --enable-debug
make -j$(nproc)
```

2. **Run tests:**
```bash
# Unit tests
make check

# Functional tests
./test/functional/test_runner.py --extended

# DigiByte-specific tests
./test/functional/digibyte_multialgo.py
./test/functional/digibyte_dandelion.py
./test/functional/digibyte_difficulty.py
./test/functional/digibyte_rewards.py
```

3. **Feature checklist:**
- [ ] Multi-algorithm mining (5 algos)
- [ ] 15-second blocks
- [ ] Dandelion++ privacy
- [ ] ODO activates at 9,112,320
- [ ] Custom rewards (6 periods)
- [ ] Network ports (12024/12025)
- [ ] Address formats (D/S/dgb)
- [ ] Genesis block valid
- [ ] No RBF enabled

### Phase 5: Finalize

```bash
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

All tests passing:
- Unit tests: ✓
- Functional tests: ✓
- DigiByte feature tests: ✓"
```

## Critical DigiByte Values

```cpp
// Algorithms
ALGO_SHA256D = 0, ALGO_SCRYPT = 1, ALGO_GROESTL = 2
ALGO_SKEIN = 3, ALGO_QUBIT = 4, ALGO_ODO = 7

// Network
nDefaultPort = 12024 (mainnet), 12025 (testnet)
pchMessageStart = {0xfa,0xc3,0xb6,0xda}

// Timing
nPowTargetSpacing = 15 seconds
multiAlgoTargetSpacing = 150 seconds

// Heights
multiAlgoDiffChangeTarget = 145000
alwaysUpdateDiffChangeTarget = 400000
workComputationChangeTarget = 1430000
OdoHeight = 9112320

// Addresses
base58Prefixes[PUBKEY_ADDRESS] = 30 // 'D'
base58Prefixes[SCRIPT_ADDRESS] = 63 // 'S'
bech32_hrp = "dgb"
```

## Success Criteria

- All Bitcoin v23.2 improvements integrated
- Zero regression in DigiByte functionality
- All tests passing
- Clean compilation
- Successful mainnet sync

## Common Issues

1. **Multi-algo conflicts:** Always preserve DigiByte's GetAlgo(), SetAlgo(), GetPoWAlgoHash()
2. **Difficulty adjustment:** Keep V1-V4 functions intact
3. **Block timing:** Maintain 15-second target
4. **Rewards:** Preserve 6-period schedule
5. **Network magic:** Keep DigiByte's unique values

Remember: When in doubt, preserve DigiByte functionality over Bitcoin improvements.