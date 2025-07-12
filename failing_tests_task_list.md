# DigiByte v8.26 Failing Unit Tests Task List

## Summary
- **Total Test Cases**: 585 (584 excluding the crashing test)
- **Status**: Multiple test failures identified
- **Primary Issues**: Bitcoin-specific values need to be replaced with DigiByte values

## Critical Test Failures (Confirmed Failing)

### High Priority - Currently Failing Tests

- [ ] **bip324_tests.cpp** - BIP324 encrypted P2P connection tests
  - Multiple cipher session ID and garbage terminator failures
  - May need DigiByte-specific test vectors or adjustments

- [ ] **crypto_tests.cpp** - Cryptographic function tests
  - SHA256 test vector failures
  - Need to add DigiByte's multi-algo hash tests (Groestl, Skein, Qubit, Odocrypt)

- [ ] **blockfilter_index_tests.cpp** - Block filter index tests
  - Critical chainman process block failure
  - Likely needs DigiByte genesis block and chain parameters

- [ ] **coins_tests.cpp** - Coin cache simulation tests
  - Coin entry comparison failures
  - May need DigiByte-specific coin parameters

- [ ] **denialofservice_tests.cpp** - Network security tests
  - stale_tip_peer_management failure
  - DoS_mapOrphans causing assertion failure (double lock)
  - Needs DigiByte network parameters

- [ ] **key_io_tests.cpp** - Key/Address encoding tests
  - Needs DigiByte address prefixes (D=30, S=63)
  - Update test vectors with valid DigiByte addresses

## Tests Requiring DigiByte-Specific Updates

### Address and Key Tests
- [ ] **script_tests.cpp** - Update Bitcoin addresses to DigiByte addresses
- [ ] **script_standard_tests.cpp** - Update standard script templates
- [ ] **descriptor_tests.cpp** - Update descriptor tests with DigiByte addresses
- [ ] **miniscript_tests.cpp** - Update miniscript tests

### Mining and Consensus Tests
- [ ] **miner_tests.cpp** - Add multi-algorithm mining support
- [ ] **pow_tests.cpp** - Test all 5 DigiByte algorithms + Odocrypt
- [ ] **validation_tests.cpp** - Update consensus parameters
- [ ] **validation_chainstate_tests.cpp** - Update chainstate validation
- [ ] **validation_block_tests.cpp** - Update block validation rules
- [ ] **versionbits_tests.cpp** - Update version bits for DigiByte

### Network Protocol Tests
- [ ] **net_tests.cpp** - Update network magic bytes and ports
- [ ] **addrman_tests.cpp** - May need DigiByte-specific adjustments
- [ ] **netbase_tests.cpp** - Update network base parameters

### Transaction Tests
- [ ] **transaction_tests.cpp** - Update with DigiByte transaction parameters
- [ ] **sighash_tests.cpp** - Verify signature hash calculations

### Supply and Economics Tests
- [ ] **amount_tests.cpp** - Update MAX_MONEY to 21 billion DGB

### Test Data Files
- [ ] **test/data/key_io_valid.json** - Add DigiByte address test vectors
- [ ] **test/data/key_io_invalid.json** - Update invalid address tests
- [ ] **test/data/tx_valid.json** - Add DigiByte transaction vectors
- [ ] **test/data/tx_invalid.json** - Update invalid transaction tests
- [ ] **test/data/script_tests.json** - Update script test vectors

### Common Test Infrastructure
- [ ] **test/util/setup_common.cpp** - Update genesis block and chain params
- [ ] **test/util/mining.cpp** - Add multi-algo mining support
- [ ] **test/util/wallet.cpp** - Update with DigiByte addresses

## Key Values to Update

### Constants
- `MAX_MONEY`: 21000000 * COIN → 21000000000 * COIN
- `POW_TARGET_SPACING`: 600 → 15
- `COINBASE_MATURITY`: Check DigiByte value (100 or 8640)

### Network Parameters
- Main net port: 8333 → 12024
- Test net port: 18333 → 12025
- Network magic bytes: Update to DigiByte values

### Addresses
- P2PKH prefix: 0 (Bitcoin '1') → 30 (DigiByte 'D')
- P2SH prefix: 5 (Bitcoin '3') → 63 (DigiByte 'S')

### Genesis Block
- Hash: Update to DigiByte genesis hash
- Merkle root: Update to DigiByte genesis merkle root

### Mining Algorithms
- Add support for: SHA256D, Scrypt, Groestl, Skein, Qubit
- Add Odocrypt (ALGO_ODO = 7) activation at height 9,112,320

## Testing Strategy

1. Fix critical failures first (tests that crash or fail immediately)
2. Update address-related tests with DigiByte prefixes
3. Update mining tests for multi-algorithm support
4. Update network parameters
5. Update test data files
6. Run full test suite to verify all fixes

## Commands

```bash
# Run all tests except the crashing one
./src/test/test_digibyte --run_test="!denialofservice_tests/DoS_mapOrphans"

# Run specific test suite
./src/test/test_digibyte --run_test=TEST_SUITE_NAME --log_level=all

# Run make check (after fixing Qt test issue)
make check
```

## Notes
- The v8.22.2 codebase should be used as reference for all DigiByte-specific values
- Never comment out or skip tests - fix them properly
- Document each fix with reference to where the correct value was found