# DigiByte v8.26 RPC Fix Implementation Plan
## Minimal Changes to Restore v8.22 Multi-Algorithm Functionality

**Date**: 2025-08-30  
**Objective**: Fix 4 critical RPC issues with minimal code changes, maintaining v8.26 architecture while restoring v8.22 functionality

---

## Quick Task Summary for Sub-Agent Assignment

### 🔴 CRITICAL RPC FIXES (Priority 1)

| Task # | Fix | File | Lines | Issue |
|--------|-----|------|-------|-------|
| 1 | `getdifficulty` | `src/rpc/blockchain.cpp` | 428-446 | Returns single value instead of multi-algo object |
| 2 | `getmininginfo` | `src/rpc/mining.cpp` | After 505 | Missing "networkhashesps" field |
| 3 | `blockheaderToJSON` | `src/rpc/blockchain.cpp` | 170-172 | Wrong difficulty + missing algo fields |
| 4 | `generateblock` | `src/rpc/mining.cpp` | 400 | Hardcoded to SHA256D instead of miningAlgo |

### 🟡 HIGH PRIORITY FIXES (Priority 2)

| Task # | Fix | File | Lines | Issue |
|--------|-----|------|-------|-------|
| 5 | `generatetodescriptor` | `src/rpc/mining.cpp` | 236-273 | Missing algorithm parameter |
| 6 | Example addresses | `src/rpc/util.cpp` | 26 | Bitcoin addresses instead of DigiByte |

### 🟢 TESTING & VALIDATION (Priority 3)

| Task # | Action | Details |
|--------|--------|----------|
| 7 | Fix test constants | Change `MULTIALGO_HEIGHT = 290` to `100` in test file |
| 8 | Run test suite | Execute `feature_digibyte_multialgo_mining.py` |

**Total Tasks**: 8 | **Critical**: 4 | **Estimated Time**: 2-4 hours | **Risk**: LOW

---

## Critical Fix #1: Restore Multi-Algorithm `getdifficulty`

### Current Issue
- **Location**: `src/rpc/blockchain.cpp:443`
- **Problem**: Returns single difficulty value instead of per-algorithm object
- **Impact**: Mining pools cannot query individual algorithm difficulties

### Implementation Fix

```cpp
// src/rpc/blockchain.cpp - Replace lines 428-446

static RPCHelpMan getdifficulty()
{
    return RPCHelpMan{"getdifficulty",
                "\nReturns the proof-of-work difficulty for all active DigiByte mining algorithms.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::OBJ, "difficulties", "The current difficulty for all active DigiByte algorithms",
                            {
                                {RPCResult::Type::NUM, "sha256d", /*optional=*/true, "SHA256D difficulty"},
                                {RPCResult::Type::NUM, "scrypt", /*optional=*/true, "Scrypt difficulty"},
                                {RPCResult::Type::NUM, "groestl", /*optional=*/true, "Groestl difficulty (before Odocrypt)"},
                                {RPCResult::Type::NUM, "skein", /*optional=*/true, "Skein difficulty"},
                                {RPCResult::Type::NUM, "qubit", /*optional=*/true, "Qubit difficulty"},
                                {RPCResult::Type::NUM, "odo", /*optional=*/true, "Odocrypt difficulty (after activation)"},
                            }},
                    }},
                RPCExamples{
                    HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    
    const CBlockIndex* tip = chainman.ActiveChain().Tip();
    const Consensus::Params& consensusParams = chainman.GetParams().GetConsensus();
    
    UniValue obj(UniValue::VOBJ);
    UniValue difficulties(UniValue::VOBJ);
    
    // Add difficulty for each active algorithm
    for (int algo = 0; algo < NUM_ALGOS_IMPL; algo++) {
        if (IsAlgoActive(tip, consensusParams, algo)) {
            difficulties.pushKV(GetAlgoName(algo), GetDifficulty(tip, nullptr, algo));
        }
    }
    
    obj.pushKV("difficulties", difficulties);
    return obj;
},
    };
}
```

### Testing
```bash
# Expected output format:
./digibyted getdifficulty
{
  "difficulties": {
    "sha256d": 12345.67,
    "scrypt": 23456.78,
    "skein": 34567.89,
    "qubit": 45678.90,
    "odo": 56789.01
  }
}
```

---

## Critical Fix #2: Add `networkhashesps` to `getmininginfo`

### Current Issue
- **Location**: `src/rpc/mining.cpp:441-513`
- **Problem**: Missing per-algorithm network hashrate field
- **Impact**: Cannot monitor individual algorithm network hashrates

### Implementation Fix

```cpp
// src/rpc/mining.cpp - Add after line 505 (after difficulties object)

    // Add per-algorithm network hashrates (DigiByte specific)
    UniValue networkhashesps(UniValue::VOBJ);
    for (int algo = 0; algo < NUM_ALGOS_IMPL; algo++) {
        if (IsAlgoActive(tip, consensusParams, algo)) {
            // Calculate hashrate for last 120 blocks of this algorithm
            networkhashesps.pushKV(GetAlgoName(algo), 
                GetNetworkHashPS(120, -1, active_chain, algo));
        }
    }
    obj.pushKV("networkhashesps", networkhashesps);
```

### Update RPCResult definition (around line 463)
```cpp
// Add after the "difficulties" field definition
{RPCResult::Type::OBJ, "networkhashesps", "Network hashes per second for each algorithm",
    {
        {RPCResult::Type::NUM, "sha256d", /*optional=*/true, "SHA256D network hashrate"},
        {RPCResult::Type::NUM, "scrypt", /*optional=*/true, "Scrypt network hashrate"},
        {RPCResult::Type::NUM, "groestl", /*optional=*/true, "Groestl network hashrate"},
        {RPCResult::Type::NUM, "skein", /*optional=*/true, "Skein network hashrate"},
        {RPCResult::Type::NUM, "qubit", /*optional=*/true, "Qubit network hashrate"},
        {RPCResult::Type::NUM, "odo", /*optional=*/true, "Odocrypt network hashrate"},
    }},
```

---

## Critical Fix #3: Correct `blockheaderToJSON` Difficulty and Add Algorithm Fields

### Current Issue
- **Location**: `src/rpc/blockchain.cpp:170-172`
- **Problem 1**: Uses default algorithm (Groestl) for all blocks
- **Problem 2**: Missing algorithm fields that exist in `blockToJSON`
- **Impact**: Wrong difficulty and missing algorithm information in block headers

### Implementation Fix

```cpp
// src/rpc/blockchain.cpp - Replace line 170 and add algorithm fields after it

    // OLD: result.pushKV("difficulty", GetDifficulty(nullptr, blockindex));
    
    // NEW: Fix difficulty to use block's actual algorithm
    result.pushKV("difficulty", GetDifficulty(nullptr, blockindex, blockindex->GetAlgo()));
    
    // REQUIRED: Add algorithm fields (these exist in blockToJSON, must be in header too)
    result.pushKV("pow_algo_id", blockindex->GetAlgo());
    result.pushKV("pow_algo", GetAlgoName(blockindex->GetAlgo()));
    
    // Note: pow_hash requires the full block, not available in blockheaderToJSON
    // Note: odo_key would require consensus params - could be added if needed
```

**IMPORTANT**: These algorithm fields are NOT optional. The v8.26 `blockToJSON` already includes `pow_algo_id`, `pow_algo`, and `pow_hash` (lines 190-193). For consistency, `blockheaderToJSON` should have at least the algo_id and algo name.

### Optional Enhancement: Add Algorithm Fields
```cpp
// Add after difficulty line (171+)
    result.pushKV("pow_algo_id", blockindex->GetAlgo());
    result.pushKV("pow_algo", GetAlgoName(blockindex->GetAlgo()));
    
    // Add Odocrypt key if applicable
    if (blockindex->GetAlgo() == ALGO_ODO) {
        result.pushKV("odo_key", (int64_t)OdoKey(chainman.GetParams().GetConsensus(), blockindex->nTime));
    }
```

---

## Critical Fix #4: Fix `generateblock` Algorithm

### Current Issue
- **Location**: `src/rpc/mining.cpp:400`
- **Problem**: Hardcoded to SHA256D instead of using miningAlgo
- **Impact**: Cannot generate blocks with configured algorithm

### Implementation Fix

```cpp
// src/rpc/mining.cpp - Replace line 400

    // OLD: 
    // std::unique_ptr<CBlockTemplate> blocktemplate(
    //     BlockAssembler{chainman.ActiveChainstate(), nullptr}.CreateNewBlock(coinbase_script, ALGO_SHA256D));
    
    // NEW: Use miningAlgo (defaults to ALGO_SCRYPT in DigiByte)
    std::unique_ptr<CBlockTemplate> blocktemplate(
        BlockAssembler{chainman.ActiveChainstate(), nullptr}.CreateNewBlock(coinbase_script, miningAlgo));
```

---

## Additional Fix: Add Algorithm Parameter to `generatetodescriptor`

### Current Issue
- **Location**: `src/rpc/mining.cpp:236-273`
- **Problem**: Missing algorithm parameter that exists in v8.22

### Implementation Fix

```cpp
// src/rpc/mining.cpp - Add after line 244 (after maxtries parameter)

            {"algo", RPCArg::Type::STR, RPCArg::Default{GetAlgoName(ALGO_SCRYPT)}, 
             "The mining algorithm to use (sha256d, scrypt, groestl, skein, qubit, odo)"},

// Then update the function body (around line 260) to parse the algorithm:

    const int num_blocks{request.params[0].get_int()};
    const uint64_t max_tries{request.params[2].isNull() ? DEFAULT_MAX_TRIES : request.params[2].get_int()};
    
    // Add algorithm parsing
    int algo = ALGO_SCRYPT;  // Default
    if (!request.params[3].isNull()) {
        std::string strAlgo = request.params[3].get_str();
        algo = GetAlgoByName(strAlgo, ALGO_SCRYPT);
    }
    
    // Then use 'algo' when calling generateBlocks:
    return generateBlocks(chainman, mempool, coinbase_script, num_blocks, max_tries, algo);
```

---

## Testing Strategy

### 1. Unit Tests
Add to `src/test/rpc_tests.cpp`:
```cpp
BOOST_AUTO_TEST_CASE(rpc_getdifficulty_multialgo)
{
    // Test that getdifficulty returns an object with difficulties field
    UniValue r = CallRPC("getdifficulty");
    BOOST_CHECK(r.isObject());
    BOOST_CHECK(r.exists("difficulties"));
    BOOST_CHECK(r["difficulties"].isObject());
}
```

### 2. Functional Test (see next section for complete test)

### 3. Manual Testing Commands
```bash
# Test getdifficulty
./digibyte-cli getdifficulty

# Test getmininginfo
./digibyte-cli getmininginfo | jq '.networkhashesps'

# Test block header
./digibyte-cli getblockheader $(./digibyte-cli getbestblockhash)

# Test block generation with algorithm
./digibyte-cli generatetoaddress 1 $(./digibyte-cli getnewaddress) 100000 scrypt
./digibyte-cli generatetoaddress 1 $(./digibyte-cli getnewaddress) 100000 sha256d
```

---

## Implementation Order

1. **Phase 1: Core Fixes (Day 1)**
   - Fix `getdifficulty` (30 mins)
   - Fix `blockheaderToJSON` difficulty (15 mins)
   - Test basic functionality

2. **Phase 2: Mining Info (Day 1)**
   - Add `networkhashesps` to `getmininginfo` (30 mins)
   - Fix `generateblock` algorithm (15 mins)
   - Test mining operations

3. **Phase 3: Enhancements (Day 2)**
   - Add algorithm parameter to `generatetodescriptor` (30 mins)
   - Add algorithm fields to `blockheaderToJSON` (20 mins)
   - Complete testing suite

4. **Phase 4: Validation (Day 2)**
   - Run full functional test suite
   - Test with mining pool software
   - Verify block explorer compatibility

---

## Risk Assessment

| Fix | Risk Level | Mitigation |
|-----|------------|------------|
| `getdifficulty` | LOW | Simple output format change |
| `networkhashesps` | LOW | Adding field, not modifying existing |
| `blockheaderToJSON` | MEDIUM | Ensure GetAlgo() works correctly |
| `generateblock` | LOW | Uses existing miningAlgo variable |
| `generatetodescriptor` | MEDIUM | Parameter parsing needs validation |

---

## Validation Checklist

- [ ] All algorithms report correct difficulty
- [ ] Network hashrates display for active algorithms
- [ ] Block headers show correct difficulty for their algorithm
- [ ] Block generation uses configured algorithm
- [ ] Mining pools can connect and query statistics
- [ ] Block explorers display algorithm information
- [ ] Regression tests pass
- [ ] No performance degradation

---

## Notes

1. **Minimal Changes**: All fixes restore v8.22 functionality with minimal code changes
2. **Backward Compatible**: Changes add fields/functionality without breaking existing
3. **Algorithm Constants**: Ensure NUM_ALGOS_IMPL is properly defined (should be 6)
4. **Odocrypt Handling**: Test after activation height (mainnet: 9,112,320, regtest: 600)

---

### Key References for Sub-Agents

**Algorithm IDs:**
- ALGO_SHA256D = 0
- ALGO_SCRYPT = 1 (default)
- ALGO_GROESTL = 2 (replaced by Odocrypt)
- ALGO_SKEIN = 3
- ALGO_QUBIT = 4
- ALGO_ODO = 7

**Important Functions:**
- `IsAlgoActive(tip, consensusParams, algo)`
- `GetAlgoName(algo)`
- `GetAlgoByName(name, fallback)`
- `GetDifficulty(tip, blockindex, algo)`
- `GetNetworkHashPS(lookup, height, chain, algo)`

**Success Criteria:**
✅ `getdifficulty` returns object with "difficulties" field
✅ `getmininginfo` includes "networkhashesps" field
✅ Block headers show correct difficulty
✅ `generateblock` uses miningAlgo
✅ All tests pass

---

## Code Review Checklist

Before committing:
1. Verify all algorithm IDs match v8.22 definitions
2. Check that IsAlgoActive() is called correctly
3. Ensure GetAlgoName() handles all algorithm IDs
4. Test with both pre and post-Odocrypt blocks
5. Verify no hardcoded algorithm assumptions remain