# DigiByte Validation Test Fix Documentation

## APPLICATION BUG FIXED
**File**: src/kernel/chainparams.cpp:718-732
**Test**: validation_chainstate_tests::chainstate_update_tip
**Issue**: Assumeutxo data contains Bitcoin's regtest block hashes, not DigiByte's
**Root Cause**: Bitcoin v26.2 merge brought in Bitcoin-specific assumeutxo snapshot data

## Problem Description
The validation tests are failing because:
1. DigiByte uses ALGO_SCRYPT for mining (vs Bitcoin's SHA256D)
2. DigiByte has a different genesis block (hash: 0x4598a0f2b823aaf9e77ee6d5e46f1edb824191dcd48b08437b7cec17e6ae6e26)
3. This results in completely different block hashes at every height
4. The assumeutxo data in chainparams.cpp contains Bitcoin's hardcoded block hashes for heights 110 and 299

## Current Bitcoin Values (INCORRECT for DigiByte)
```cpp
m_assumeutxo_data = {
    {
        .height = 110,
        .hash_serialized = AssumeutxoHash{uint256S("0x6657b736d4fe4db0cbc796789e812d5dba7f5c143764b1b6905612f1830609d1")},
        .nChainTx = 111,
        .blockhash = uint256S("0x696e92821f65549c7ee134edceeeeaaa4105647a3c4fd9f298c0aec0ab50425c")  // Bitcoin's block
    },
    {
        .height = 299,
        .hash_serialized = AssumeutxoHash{uint256S("0x61d9c2b29a2571a5fe285fe2d8554f91f93309666fc9b8223ee96338de25ff53")},
        .nChainTx = 300,
        .blockhash = uint256S("0x7e0517ef3ea6ecbed9117858e42eedc8eb39e8698a38dcbd1b3962a283233f4c")  // Bitcoin's block
    },
};
```

## Required Fix
To properly fix this, we need to:

1. Generate a DigiByte regtest chain to height 299
2. Get the actual block hashes at heights 110 and 299
3. Create UTXO snapshots at those heights
4. Extract the hash_serialized values from the snapshots
5. Update chainparams.cpp with the correct DigiByte values

## Temporary Workaround
For now, the assumeutxo data should be disabled for regtest until proper DigiByte values can be calculated:

```cpp
m_assumeutxo_data = {
    // TODO: Calculate proper DigiByte regtest values
    // Heights 110 and 299 need DigiByte-specific block hashes
};
```

## Steps to Generate Correct Values
```bash
# 1. Build and run a test that generates blocks
./src/test/test_digibyte --run_test=validation_tests/print_regtest_blocks

# 2. Or use digibyte-cli in regtest mode
./src/digibyted -regtest -daemon
./src/digibyte-cli -regtest generatetoaddress 110 <address>
./src/digibyte-cli -regtest getblockhash 110
./src/digibyte-cli -regtest dumptxoutset snapshot_110.dat

# 3. Extract metadata from snapshot
# The hash_serialized value will be in the snapshot metadata
```

## Impact
Without this fix:
- validation_chainstate_tests::chainstate_update_tip fails
- validation_chainstatemanager_tests fails at multiple points
- Any test relying on assumeutxo functionality will fail

## Related Tests Affected
- validation_chainstate_tests/chainstate_update_tip (line 87)
- validation_chainstatemanager_tests/chainstatemanager_activate_snapshot (line 260)
- validation_chainstatemanager_tests/chainstatemanager_loadblockindex (lines 529-531, 551)

## Testing
After applying the correct DigiByte values:
```bash
./src/test/test_digibyte --run_test=validation_chainstate_tests
./src/test/test_digibyte --run_test=validation_chainstatemanager_tests
```

Both test suites should pass completely.