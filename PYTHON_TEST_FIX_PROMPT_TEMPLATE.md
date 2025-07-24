# DigiByte v8.26 Python Functional Test Fix Instructions

## Your Mission
You are tasked with systematically fixing Python functional tests for DigiByte v8.26 (Bitcoin v26.2 merge). Your goal is to ensure all tests pass while identifying and fixing any real bugs discovered during the process.

## Your Assigned Category
**[CATEGORY_NAME_HERE]**

Test files to fix:
[LIST_TEST_FILES_HERE]

## Sub-Agent Task Distribution (For Large Categories)
For categories with many tests, you may use up to 3 sub-agents to work in parallel:

### When to Use Sub-Agents
- Categories with 20+ test files
- Complex test suites requiring specialized knowledge
- Time-critical fixes requiring parallel work

### Sub-Agent Assignment Example
```
Main Agent: Coordinates and reviews all fixes
Sub-Agent 1: Tests 1-20 (e.g., wallet_basic.py through wallet_groups.py)
Sub-Agent 2: Tests 21-40 (e.g., wallet_hd.py through wallet_send.py)
Sub-Agent 3: Tests 41+ (e.g., wallet_sendall.py through wallet_watchonly.py)
```

### Sub-Agent Coordination Protocol
1. **Main Agent**: 
   - Assigns specific test ranges to each sub-agent
   - Reviews all fixes for consistency
   - Handles cross-test dependencies
   - Merges and validates all changes

2. **Sub-Agents**:
   - Work only on assigned test files
   - Report findings to main agent
   - Flag any dependencies on other tests
   - Document all changes thoroughly

### Communication Format
```markdown
## SUB-AGENT [N] REPORT
**Tests Fixed**: [list]
**Application Bugs Found**: [count]
**Dependencies Noted**: [list]
**Status**: [In Progress/Complete]
```

### Pre-Split Categories in CLAUDE.md
Some categories are already pre-split for convenience:
- **Category 2 (Wallet)**: Split into 2A (57 tests) and 2B (57 tests)
- **Category 6 (Feature)**: Split into 6A (31 tests) and 6B (31 tests)

For these pre-split categories:
- Treat each subcategory as a separate assignment
- OR use sub-agents within a subcategory if needed
- Coordinate between subcategories for shared functionality

## Initial Setup

1. **Read CLAUDE.md First**
   ```bash
   cat CLAUDE.md
   ```
   This contains critical information about DigiByte-specific values and constants.

2. **Install Required Python Module**
   ```bash
   pip install --break-system-packages digibyte-scrypt
   ```

3. **Verify Build**
   ```bash
   cd /mnt/c/Users/Jared/code/digibyte
   ./src/digibyted --version  # Should show DigiByte Core version
   ```

## Test Fixing Process

### Step 1: Run Each Test File Individually
For each test file in your assigned category:

```bash
# Example for a test file
./test/functional/TEST_NAME.py 2>&1 | tee TEST_NAME_output.log

# For tests with variants
./test/functional/TEST_NAME.py --legacy-wallet
./test/functional/TEST_NAME.py --descriptors
```

### Step 2: Analyze Failures
Common failure patterns and their solutions:

#### 1. **RPC Method Not Found**
```
Error: Method not found (-32601)
```
**Solution**: Check if DigiByte has custom RPC methods that need to be implemented or if Bitcoin methods were renamed.

#### 2. **Assertion Failed (Values)**
```
AssertionError: not(0 == 199)
AssertionError: not(0.00010000 == 0.00001000)
```
**Solution**: These are usually DigiByte-specific constant differences:
- Block rewards: DigiByte uses different reward schedule
- Fee rates: 0.00001000 DGB/kB (not 0.00010000)
- Block times: 15 seconds (not 600)
- Max supply: 21 billion (not 21 million)

#### 3. **Import Errors**
```
ImportError: cannot import name 'xxx'
```
**Solution**: Check if the import exists in the test framework or if it needs to be updated.

#### 4. **Address Format Issues**
```
Error: Invalid address
```
**Solution**: Update to DigiByte address formats:
- Mainnet: D... (P2PKH), S... (P2SH), dgb1... (Bech32)
- Testnet: s... (P2PKH/P2SH), dgbt1... (Bech32) 
- Regtest: s... (P2PKH/P2SH), dgbrt1... (Bech32)

### Step 3: Compare with Reference Code
**CRITICAL**: Always check three codebases:

```bash
# Current v8.26 test
cat test/functional/YOUR_TEST.py

# DigiByte v8.22.2 reference (if exists)
cat digibyte-v8.22.2/test/functional/YOUR_TEST.py 2>/dev/null || echo "No v8.22.2 version"

# Bitcoin v26.2 reference
cat bitcoin-v26.2-for-digibyte/test/functional/YOUR_TEST.py
```

### Step 4: Common DigiByte Replacements

#### Constants to Update:
```python
# Block time
BLOCK_TIME = 15  # Not 600

# Fees
MIN_RELAY_FEE = Decimal('0.00001000')  # Not 0.00010000

# Supply
MAX_MONEY = 21000000000  # 21 billion, not 21 million

# Coinbase maturity
COINBASE_MATURITY = 100  # Same as Bitcoin

# Port numbers
P2P_PORT = 12024  # Mainnet
P2P_PORT_TESTNET = 12025
```

#### Address Updates:
```python
# Old Bitcoin
ADDRESS = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"

# New DigiByte
ADDRESS = "DGSbdXzKqPNLBpPDWK7MXgXN45LxYvPqFD"

# Bech32
OLD: "bc1qxxx..."
NEW: "dgb1qxxx..."
```

#### Genesis Block:
```python
# DigiByte mainnet genesis
GENESIS_HASH = "7497ea1b465eb39f1c8f507bc877078fe016d6fcb6dfad3a64c98dcc6e1e8e1c"

# DigiByte testnet genesis  
GENESIS_HASH_TESTNET = "308ea0711d5763be2995670dd9ca9b4b9df05dc3d60ea85306ef7e8d9e2daac2"
```

### Step 5: Handle Test-Specific Issues

#### Dandelion Tests
DigiByte implements Dandelion++ for privacy. Tests may need:
- Custom RPC methods for Dandelion
- Different mempool behavior
- Stem phase handling

#### Multi-Algorithm Mining
DigiByte uses 5 mining algorithms:
```python
ALGO_SHA256D = 0
ALGO_SCRYPT = 1
ALGO_GROESTL = 2
ALGO_SKEIN = 3
ALGO_QUBIT = 4
ALGO_ODO = 7  # Odocrypt
```

#### Custom RPC Commands
DigiByte adds:
- `getblockreward` - Returns current block reward
- Enhanced `getmininginfo` - Shows per-algorithm stats
- Enhanced `getdifficulty` - Returns all algorithm difficulties

### Step 6: Fix Application Bugs
If you discover bugs in the actual application code:

1. **Fix the Bug** in the application code
2. **Document** with detailed comments
3. **Report** using this format:

```markdown
## APPLICATION BUG FIXED
**File**: src/rpc/blockchain.cpp:123
**Test**: rpc_blockchain.py::test_getdifficulty
**Issue**: Missing DigiByte multi-algo difficulty handling
**Root Cause**: Bitcoin v26.2 merge removed custom logic
**Fix Applied**: 
```python
# Added multi-algo difficulty support
if (IsDigiByteMultiAlgo(height)) {
    return GetMultiAlgoDifficulty();
}
```
**Impact**: getdifficulty would return incorrect values
**Testing**: Verified with multi-algo blocks
```

### Step 7: Verify Your Fix
```bash
# Run the specific test again
./test/functional/YOUR_TEST.py

# Run with verbose output if needed
./test/functional/YOUR_TEST.py --loglevel=debug
```

## Test Categories and Common Issues

### P2P Tests
- Dandelion++ implementation differences
- Network magic bytes
- Protocol version differences

### Wallet Tests  
- Address format differences
- HD derivation paths
- Fee calculation differences

### RPC Tests
- Custom DigiByte RPC methods
- Modified response formats
- Multi-algo additions

### Mining Tests
- Multi-algorithm support
- Different block rewards
- 15-second block time

### Feature Tests
- DigiByte-specific features
- Different activation heights
- Custom consensus rules

## Critical DigiByte Test Values

```python
# Test framework values
TESTNET_ADDRESS_PREFIX = 's'  # Not 'm' or 'n'
TESTNET_SCRIPT_PREFIX = 's'   # Not '2'
TESTNET_BECH32_HRP = 'dgbt'  # Not 'tb'

# Common test amounts
SUBSIDY = 72000  # Current block reward in DGB
FEE_RATE = Decimal('0.00001')  # DGB per kB

# Timing
BLOCK_TIME = 15
TIMEOUT_FACTOR = 15 / 600  # Adjust timeouts for faster blocks
```

## Final Checklist
- [ ] All tests in category pass
- [ ] No tests disabled or skipped without valid reason
- [ ] DigiByte-specific values verified
- [ ] Application bugs documented and fixed
- [ ] Test logic preserved (not just making tests pass)
- [ ] Compared with v8.22.2 behavior where applicable

## Common Pitfalls to Avoid
1. **Don't just change assertions** - Understand why values differ
2. **Don't skip Dandelion tests** - They're important for DigiByte
3. **Remember faster blocks** - Adjust timeouts and wait times
4. **Check for multi-algo** - Many tests assume single algorithm
5. **Verify addresses** - Must use DigiByte format

## Best Practices for Sub-Agent Usage

### Efficient Task Distribution
1. **Group Related Tests**: Assign tests that share functionality to the same sub-agent
2. **Balance Workload**: Distribute tests evenly based on complexity, not just count
3. **Consider Dependencies**: Tests that depend on each other should go to the same sub-agent

### Example Sub-Agent Assignment for Large Categories

#### Wallet Tests (114 tests) - Using 3 Sub-Agents:
```
Sub-Agent 1: Basic wallet operations (38 tests)
- wallet_basic.py, wallet_balance.py, wallet_create*.py, etc.

Sub-Agent 2: Transaction handling (38 tests)  
- wallet_send*.py, wallet_fund*.py, wallet_txn*.py, etc.

Sub-Agent 3: Advanced features (38 tests)
- wallet_import*.py, wallet_hd.py, wallet_multisig*.py, etc.
```

#### Feature Tests (62 tests) - Using 3 Sub-Agents:
```
Sub-Agent 1: Consensus features (21 tests)
- feature_block.py, feature_segwit.py, feature_taproot.py, etc.

Sub-Agent 2: Network features (21 tests)
- feature_maxuploadtarget.py, feature_proxy.py, feature_bind*.py, etc.

Sub-Agent 3: Node features (20 tests)
- feature_pruning.py, feature_reindex.py, feature_init.py, etc.
```

### Coordination Tips
1. **Share Common Fixes**: If one sub-agent finds a pattern (e.g., fee constant), share immediately
2. **Track Application Bugs**: Maintain a shared list of application bugs found
3. **Test Interdependencies**: Run related tests together after fixes
4. **Final Integration**: Main agent must run full category test suite after merging

Your systematic approach will ensure DigiByte v8.26 functional tests are production-ready.