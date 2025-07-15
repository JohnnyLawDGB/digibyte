# Test Fix Task: Category 1 - Address & Key Format Tests

## Objective
Fix all 228 failing test assertions in `src/test/key_io_tests.cpp` to properly validate DigiByte addresses and keys instead of Bitcoin addresses.

## Context
You are fixing C++ unit tests for DigiByte v8.26 after merging Bitcoin Core v26.2. Based on `make check` results, we have 7 failing test files with ~2,227 total failures:

1. **coins_tests.cpp** - 1,651 failures (LARGEST - coins_cache_simulation_test)
2. **bip324_tests.cpp** - 280 failures (packet_test_vectors)
3. **key_io_tests.cpp** - 228 failures (YOUR CURRENT TASK)
4. **transaction_tests.cpp** - 46 failures (test_IsStandard)
5. **compress_tests.cpp** - 1 failure (21 billion supply compression)
6. **miner_tests.cpp** - 1 failure (multi-algo initialization)
7. **blockfilter_index_tests.cpp** - 1 failure (genesis block sync)

Your current focus is fixing Category 1: the 228 failures in key_io_tests.cpp.

## Key Information

### DigiByte Address Prefixes
- **Mainnet P2PKH**: 30 (produces 'D' addresses)
- **Mainnet P2SH**: 63 (produces 'S' addresses) 
- **Testnet P2PKH**: 126
- **Testnet P2SH**: 140

### Bech32 Prefixes (HRP)
- **Mainnet**: "dgb"
- **Testnet**: "dgbt"
- **Regtest**: "dgbrt"

### Reference Repository
Use `digibyte-v8.22.2` as the source of truth for all DigiByte-specific values. Compare test files and test data.

## Specific Tasks

### 1. Update test data files (CRITICAL - Most failures come from here):
   
**File: `src/test/data/key_io_valid.json`**
- Contains valid address/key test vectors
- Currently has Bitcoin addresses that fail DigiByte validation
- Must replace ALL Bitcoin addresses with DigiByte equivalents:
  - Bitcoin mainnet (1xxx, 3xxx, bc1xxx) → DigiByte mainnet (Dxxx, Sxxx, dgb1xxx)
  - Bitcoin testnet (mxxx/nxxx, 2xxx, tb1xxx) → DigiByte testnet addresses
  - Bitcoin regtest → DigiByte regtest with dgbrt1 prefix
- Update private key entries if WIF prefixes differ

**File: `src/test/data/key_io_invalid.json`**
- Contains invalid address/key test vectors
- Some Bitcoin addresses that are invalid might be valid DigiByte addresses
- Review and update accordingly

### 2. Fix key_io_tests.cpp if needed:
- The test code itself may be correct; most failures come from test data
- Check for any hardcoded Bitcoin addresses in the C++ code
- Ensure test logic works with DigiByte parameters

### 3. Verify chainparams.cpp has correct values:
- Mainnet: base58Prefixes[PUBKEY_ADDRESS] = 30, [SCRIPT_ADDRESS] = 63, bech32_hrp = "dgb"
- Testnet: base58Prefixes[PUBKEY_ADDRESS] = 126, [SCRIPT_ADDRESS] = 140, bech32_hrp = "dgbt"  
- Regtest: bech32_hrp = "dgbrt"

## Common Test Failures You'll See
The test is failing with errors like:
```
error: in "key_io_tests/key_io_valid_parse": !IsValid:["t3RJSU3BGXUMgtUpbhjNX4w1BNsNJvnrG5",...]
error: in "key_io_tests/key_io_valid_gen": check address == exp_base58string has failed [dgb1qhxt04s5xnpy0kxw4x99n5hpdf5pmtzpq80wpgy != dgb1qhxt04s5xnpy0kxw4x99n5hpdf5pmtzpqs52es2]
```

These failures occur because:
1. Bitcoin signet/testnet addresses (starting with 't', 'n', '2') are not valid for DigiByte
2. Bech32 checksums are different between Bitcoin and DigiByte HRPs
3. WIF private key prefixes may differ

## Testing
After making changes:
```bash
make -j6
./src/test/test_digibyte --log_level=message --run_test=key_io_tests
```

## Success Criteria
- All 228 test failures in key_io_tests are resolved
- Three test cases pass: key_io_valid_parse, key_io_valid_gen, key_io_invalid
- No hardcoded Bitcoin addresses remain
- All DigiByte address formats (legacy and bech32) validate correctly
- Private key import/export works with DigiByte WIF format

## Important Rules
- DO NOT comment out failing tests
- DO NOT skip tests or add early returns
- ONLY modify test files and test data
- If you discover bugs in application code, document and ask permission before fixing
- Preserve all copyright headers

## Example Valid DigiByte Addresses
From v8.22.2 tests:
- P2PKH: "DGSbdXzKqPNLBpPDWK7MXgXN45LxYvPqFD"
- P2SH: "SfBbrV3yCGjKW52dac8Tgkvqd6zMgvPZFG"
- Bech32: Should start with "dgb1" for mainnet

Start by examining the current test failures and comparing with v8.22.2 test data.