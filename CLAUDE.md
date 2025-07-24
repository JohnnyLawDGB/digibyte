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

### Category 1: Address, Keys & Encoding Tests (9 files)
**AI Agent 1 Assignment - Critical Foundation**
- `key_io_tests.cpp` - DigiByte address/key validation
- `key_tests.cpp` - Key generation and signing
- `base58_tests.cpp` - Base58 encoding (addresses)
- `base32_tests.cpp` - Base32 encoding
- `base64_tests.cpp` - Base64 encoding
- `bech32_tests.cpp` - Bech32 addresses (dgb1 prefix)
- `bip32_tests.cpp` - HD wallet key derivation
- `descriptor_tests.cpp` - Output descriptors
- `compress_tests.cpp` - Amount compression (21B supply)

### Category 2: Transaction & Script Tests (15 files)
**AI Agent 2 Assignment - Transaction Processing**
- `transaction_tests.cpp` - Transaction validation
- `txvalidation_tests.cpp` - Transaction acceptance rules
- `txvalidationcache_tests.cpp` - Validation caching
- `script_tests.cpp` - Script interpreter
- `script_p2sh_tests.cpp` - P2SH scripts
- `script_segwit_tests.cpp` - SegWit scripts
- `script_standard_tests.cpp` - Standard scripts
- `script_parse_tests.cpp` - Script parsing
- `scriptnum_tests.cpp` - Script number handling
- `sighash_tests.cpp` - Signature hash computation
- `sigopcount_tests.cpp` - Signature operation counting
- `multisig_tests.cpp` - Multisignature scripts
- `psbt_tests.cpp` - PSBT handling (if exists)
- `txpackage_tests.cpp` - Package relay
- `rbf_tests.cpp` - Replace-by-fee

### Category 3: Mining & Consensus Tests (10 files)
**AI Agent 3 Assignment - DigiByte Multi-Algo Mining**
- `miner_tests.cpp` - Multi-algorithm mining
- `pow_tests.cpp` - Proof of work (5 algorithms)
- `blockchain_tests.cpp` - Blockchain utilities
- `versionbits_tests.cpp` - Soft fork deployment
- `validation_tests.cpp` - Consensus validation
- `validation_block_tests.cpp` - Block validation
- `validation_chainstate_tests.cpp` - Chainstate management
- `validation_chainstatemanager_tests.cpp` - Chainstate manager
- `validation_flush_tests.cpp` - Flush operations
- `validationinterface_tests.cpp` - Validation callbacks

### Category 4: Network & P2P Tests (12 files)
**AI Agent 4 Assignment - Network Protocol**
- `net_tests.cpp` - Network layer
- `netbase_tests.cpp` - Network utilities
- `net_peer_eviction_tests.cpp` - Peer management
- `bip324_tests.cpp` - V2 transport protocol
- `i2p_tests.cpp` - I2P integration
- `torcontrol_tests.cpp` - Tor integration
- `sock_tests.cpp` - Socket handling
- `httpserver_tests.cpp` - HTTP server
- `rest_tests.cpp` - REST interface
- `rpc_tests.cpp` - RPC interface
- `denialofservice_tests.cpp` - DoS protection
- `banman_tests.cpp` - Ban management

### Category 5: Database & Storage Tests (11 files)
**AI Agent 5 Assignment - Data Storage**
- `coins_tests.cpp` - UTXO set management
- `dbwrapper_tests.cpp` - Database wrapper
- `flatfile_tests.cpp` - Flat file storage
- `blockfilter_tests.cpp` - Compact block filters
- `blockfilter_index_tests.cpp` - Block filter indexing
- `txindex_tests.cpp` - Transaction indexing
- `coinstatsindex_tests.cpp` - Coin statistics index
- `blockencodings_tests.cpp` - Compact blocks
- `blockmanager_tests.cpp` - Block storage management
- `fs_tests.cpp` - Filesystem operations
- `streams_tests.cpp` - Data streams

### Category 6: Memory Pool Tests (8 files)
**AI Agent 6 Assignment - Mempool Management**
- `mempool_tests.cpp` - Memory pool logic
- `miniminer_tests.cpp` - Mini miner for mempool
- `policy_fee_tests.cpp` - Fee policies
- `policyestimator_tests.cpp` - Fee estimation
- `txrequest_tests.cpp` - Transaction requests
- `txreconciliation_tests.cpp` - Transaction reconciliation
- `orphanage_tests.cpp` - Orphan transactions
- `pool_tests.cpp` - Memory pool utilities

### Category 7: Crypto & Hash Tests (8 files)
**AI Agent 7 Assignment - Cryptography**
- `crypto_tests.cpp` - Cryptographic functions
- `hash_tests.cpp` - Hash functions (including DigiByte algos)
- `bloom_tests.cpp` - Bloom filters
- `merkle_tests.cpp` - Merkle trees
- `merkleblock_tests.cpp` - Merkle blocks
- `pmt_tests.cpp` - Partial merkle trees
- `muhash_tests.cpp` - MuHash for UTXO set (if exists)
- `siphash_tests.cpp` - SipHash (if exists)

### Category 8: Utility & System Tests (15 files)
**AI Agent 8 Assignment - Core Utilities**
- `util_tests.cpp` - General utilities
- `util_threadnames_tests.cpp` - Thread naming
- `system_tests.cpp` - System utilities
- `argsman_tests.cpp` - Argument parsing
- `getarg_tests.cpp` - Command line arguments
- `settings_tests.cpp` - Settings management
- `logging_tests.cpp` - Logging system
- `random_tests.cpp` - Random number generation
- `sync_tests.cpp` - Synchronization primitives
- `scheduler_tests.cpp` - Task scheduling
- `reverselock_tests.cpp` - Lock utilities
- `translation_tests.cpp` - Translation system
- `interfaces_tests.cpp` - Interface boundaries
- `result_tests.cpp` - Result type handling
- `timedata_tests.cpp` - Time adjustment

### Category 9: Data Structure Tests (12 files)
**AI Agent 9 Assignment - Core Data Structures**
- `uint256_tests.cpp` - 256-bit integers
- `arith_uint256_tests.cpp` - Arithmetic on uint256
- `amount_tests.cpp` - CAmount handling (21B supply)
- `prevector_tests.cpp` - Optimized vector
- `skiplist_tests.cpp` - Skip list implementation
- `limitedmap_tests.cpp` - Size-limited map
- `cuckoocache_tests.cpp` - Cuckoo cache
- `allocator_tests.cpp` - Memory allocation
- `serialize_tests.cpp` - Serialization
- `serfloat_tests.cpp` - Float serialization
- `bswap_tests.cpp` - Byte swapping
- `compilerbug_tests.cpp` - Compiler workarounds

### Category 10: Specialized Tests (remaining files)
**AI Agent 10 Assignment - Miscellaneous**
- `checkqueue_tests.cpp` - Parallel validation queue
- `headers_sync_chainwork_tests.cpp` - Headers sync
- `raii_event_tests.cpp` - RAII event handling
- `miniscript_tests.cpp` - Miniscript
- `minisketch_tests.cpp` - Set reconciliation
- `xoroshiro128plusplus_tests.cpp` - PRNG
- `sanity_tests.cpp` - Sanity checks

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
