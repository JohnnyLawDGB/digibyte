# CLAUDE.md - AI Assistant Guide for DigiByte Development

## Overview
This file provides context and guidance for AI assistants working on the DigiByte codebase, particularly for the Bitcoin Core v26.2 merge creating DigiByte v8.26.

## Reference Directories
- **Current merged code**: `/Users/jt/Code/digibyte` (DigiByte v8.26 work in progress)
- **Original DigiByte**: `/Users/jt/Code/digibyte/digibyte-v8.22.2` (v8.22.2 for comparison)
- **Bitcoin v26.2**: `/Users/jt/Code/bitcoin` (Bitcoin Core v26.2 reference)

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
make -j$(nproc)
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

### Common Tasks

#### Check Multi-Algorithm Mining
```bash
./src/digibyte-cli getmininginfo
# Should show difficulties for all 5 algorithms
```

#### Get Current Block Reward
```bash
./src/digibyte-cli getblockreward
# Returns current DGB block reward based on 6-period schedule
```

#### Generate Blocks (Regtest)
```bash
# Generate with specific algorithm
./src/digibyte-cli -regtest generatetoaddress 1 <address> SHA256D
./src/digibyte-cli -regtest generatetoaddress 1 <address> SCRYPT
```

## Merge Guidelines

When merging Bitcoin Core updates:

1. **ALWAYS PRESERVE**:
   - Multi-algorithm mining code
   - 15-second block time
   - 21 billion max supply
   - Custom RPC commands
   - Dandelion++ implementation
   - DigiShield difficulty algorithms
   - Network ports and magic bytes

2. **NEVER CHANGE**:
   - Core consensus parameters
   - Block reward schedule
   - Algorithm identifiers
   - Network protocol version (unless required)

3. **CAREFUL MERGE AREAS**:
   - src/miner.cpp (multi-algo block creation)
   - src/pow.cpp (difficulty algorithms)
   - src/validation.cpp (consensus rules)
   - src/net_processing.cpp (Dandelion++ hooks)
   - src/rpc/mining.cpp (custom commands)

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

## Branch Naming (GitFlow)
DigiByte follows GitFlow branching model:
- Feature branches: `feature/description`
- Release branches: `release/X.Y.Z`
- Hotfix branches: `hotfix/description`
- Main branches: `master` (stable) and `develop` (integration)

For this merge: `feature/bitcoin-v26.2-merge`

## Support Resources
- Specification: digibyte-btc-v26-2-merge-spec.md
- Merge Prompt: DIGIBYTE_V8.26_MERGE_PROMPT.md
- GitHub: https://github.com/digibyte-core/digibyte
- Website: https://digibyte.org/

## Important Reminders
- Both Bitcoin and DigiByte copyrights must be preserved
- Test on testnet before mainnet
- Document all merge decisions
- Run linting and formatting checks
- Ensure reproducible builds work