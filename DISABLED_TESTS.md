# Disabled Tests Documentation

## Tests Disabled for DigiByte v8.26

### 1. rbf_tests.cpp
- **Reason**: DigiByte does not support Replace-by-Fee (RBF)
- **Status**: Test file exists but excluded from compilation
- **Location**: src/test/rbf_tests.cpp

### 2. bip324_tests.cpp  
- **Reason**: V2 transport protocol (BIP324) is optional and not ready
- **Status**: Test file exists but excluded from compilation
- **Location**: src/test/bip324_tests.cpp

### 3. Fuzz Tests
Also disabled the corresponding fuzz tests:
- test/fuzz/rbf.cpp
- test/fuzz/bip324.cpp

## How to Re-enable
If these features are implemented in the future, add the test files back to `src/Makefile.test.include`:
- In the `DIGIBYTE_TESTS` section for unit tests
- In the `test_fuzz_fuzz_SOURCES` section for fuzz tests

## Modified Files
- src/Makefile.test.include - Removed test entries from compilation list