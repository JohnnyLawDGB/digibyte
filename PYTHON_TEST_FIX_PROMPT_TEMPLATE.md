# DigiByte v8.26 Python Functional Test Fix Instructions

## Your Mission
You are tasked with systematically fixing Python functional tests for DigiByte v8.26 (Bitcoin v26.2 merge). The tests have been organized into groups in ATTACK_LIST.md for efficient parallel fixing.

## Test Group Assignment Process
1. The user will assign you a specific group from ATTACK_LIST.md (e.g., "Group 1: Mempool Tests")
2. You will receive a list of 10-15 related tests to fix
3. Use up to 3 sub-agents to work on different tests in parallel for efficiency

## Initial Setup

### Prerequisites
```bash
pip install --break-system-packages digibyte-scrypt
```

### Build Test Environment
```bash
cd /mnt/c/Users/Jared/code/digibyte
./configure --without-gui && make -j8
```

### Read Critical Documentation
1. **CLAUDE.md** - DigiByte-specific constants and test strategy
2. **ATTACK_LIST.md** - Test groupings and common issues per group
3. **FUNCTIONAL_CHECKLIST_TESTS.md** - Current test status
4. **doc/DANDELION_INFO.md** - For any Dandelion++ related issues

## Test Fixing Workflow

### Step 1: Receive Test Group Assignment
The user will provide you with a group like:
```
Fix Group 1: Mempool Tests
- mempool_accept.py
- mempool_compatibility.py
- mempool_datacarrier.py
... (etc)
```

### Step 2: Deploy Sub-Agents Strategy
Use up to 3 sub-agents to work in parallel:

**Sub-Agent 1**: Handle first 5 tests
**Sub-Agent 2**: Handle next 5 tests  
**Sub-Agent 3**: Handle remaining tests

Each sub-agent should:
1. Run the test to identify failures
2. Compare with reference codebases
3. Apply fixes
4. Verify the fix works
5. Report back with results

### Step 3: Fix Process for Each Test

#### 3.1 Run Test and Capture Output
```bash
./test/functional/test_name.py 2>&1 | tee test_name_output.log

# For wallet tests with variants:
./test/functional/test_name.py --descriptors
./test/functional/test_name.py --legacy-wallet
```

#### 3.2 Compare Three Codebases
**CRITICAL**: Always check these three versions IN THIS ORDER:

1. **FIRST - DigiByte v8.22.2** (SOURCE OF TRUTH - tests passed here!)
   ```bash
   # Check how the test SUCCESSFULLY works in v8.22.2
   cat /mnt/c/Users/Jared/code/digibyte/digibyte-v8.22.2/test/functional/test_name.py
   ```
   This shows you the CORRECT DigiByte-specific behavior and values.

2. **SECOND - Bitcoin v26.2** (understand what changed)
   ```bash
   # See what Bitcoin changed that might break DigiByte
   cat /mnt/c/Users/Jared/code/digibyte/bitcoin-v26.2-for-digibyte/test/functional/test_name.py
   ```
   This helps identify new features or changes from Bitcoin.

3. **THIRD - Current v8.26** (what we're fixing)
   ```bash
   # Now look at the broken merged version
   cat test/functional/test_name.py
   ```
   This is where you'll apply fixes based on learnings from steps 1 & 2.

**KEY INSIGHT**: Most test failures can be resolved quickly by seeing how they worked in v8.22.2, as all tests passed there with DigiByte-specific needs already handled!

#### 3.3 Common Fixes by Test Group

**Group 1: Mempool Tests**
- Fee calculations: MIN_RELAY_TX_FEE changed from 0.001 to 0.0001 DGB/kB
- Dandelion++ transaction routing differences
- Dust threshold calculations
- Package size limits

**Group 2-4: P2P Network Tests**
- Port numbers: 12024 (mainnet), 12025 (testnet)
- Protocol timing adjustments for 15-second blocks
- Dandelion++ message types (DANDELIONTX)
- Multi-algorithm version bits

**Group 5-6: RPC Tests**
- DigiByte-specific RPC methods (getblockreward, etc.)
- Address format validation
- Block reward calculations (72000 DGB)
- Chain parameter differences

**Group 7-8: Feature Tests**
- Consensus parameters
- Activation heights
- UTXO database format
- Pruning calculations

**Group 9-10: Wallet & Interface Tests**
- Address generation (dgbt1 prefix)
- Coinbase maturity (8 blocks for spending, 100 for full)
- Fee estimation parameters
- HD key derivation paths

### Step 4: Handle Application Bugs

**IMPORTANT**: When you discover a bug in the actual application code (not just test code):

1. **Fix the Bug** in the application code
2. **Document the Fix** comprehensively
3. **Report the Bug** using this format:

```markdown
## APPLICATION BUG FIXED
**File**: src/[filename].cpp:XXX
**Test**: [test_name.py]
**Issue**: [description]
**Root Cause**: [Bitcoin v26.2 merge impact]
**Fix Applied**: [code changes]
**Impact**: [consequence if unfixed]
**Testing**: [how verified]
```

### Step 5: Verify Fixes
```bash
# Run individual test
./test/functional/test_name.py

# If test has variants, run all:
./test/functional/test_name.py --descriptors
./test/functional/test_name.py --legacy-wallet

# Run with debug output if needed
./test/functional/test_name.py --loglevel=debug
```

## Critical DigiByte Constants Reference

### Consensus Parameters
```python
# Block timing
BLOCK_TIME = 15  # seconds (NOT 600)

# Supply
MAX_MONEY = 21000000000  # 21 billion DGB (NOT 21 million)
SUBSIDY = 72000  # Current block reward in DGB

# Maturity
COINBASE_MATURITY = 8      # Spendable after 8 blocks
COINBASE_MATURITY_2 = 100  # Full maturity for some operations
```

### Fee Structure
```python
# Transaction fees (per kB) - CRITICAL: These changed in v8.26!
DEFAULT_TRANSACTION_MINFEE = Decimal('0.1')     # Wallet minimum fee
DEFAULT_FALLBACK_FEE = Decimal('0.01')          # Fallback fee
MIN_RELAY_TX_FEE = Decimal('0.0001')            # Relay minimum (was 0.001)
DUST_RELAY_TX_FEE = Decimal('0.0003')           # Dust threshold
```

### Network Parameters
```python
# Ports
P2P_PORT = 12024         # Mainnet
P2P_PORT_TESTNET = 12025 # Testnet

# Address formats
TESTNET_ADDRESS_PREFIX = 's'    # P2PKH/P2SH (NOT 'm' or 'n')
TESTNET_BECH32_HRP = 'dgbt'    # Bech32 (NOT 'tb')
REGTEST_BECH32_HRP = 'dgbrt'   # Regtest (NOT 'bcrt')
```

### Mining Algorithms
```python
# DigiByte uses 5 algorithms + Odocrypt
ALGO_SHA256D = 0
ALGO_SCRYPT = 1
ALGO_GROESTL = 2
ALGO_SKEIN = 3
ALGO_QUBIT = 4
ALGO_ODO = 7  # Odocrypt (activates at height 9,112,320)
```

## Progress Tracking

### After Each Test Fixed
1. Update your internal tracking
2. Note any application bugs found
3. Save test output logs

### After Group Completion
Create a summary report:
```markdown
## Group X: [Group Name] - COMPLETED
**Total Tests**: X
**Fixed**: X
**Application Bugs Found**: X

### Test Results:
1. test_name.py - FIXED ✓
   - Issue: [brief description]
   - Fix: [brief description]
   
[... continue for all tests ...]

### Application Bugs Fixed:
[List all APPLICATION BUG FIXED reports]
```

## Remember
- **NEVER** skip or disable tests - always fix the root cause
- **ALWAYS** start by checking v8.22.2 FIRST - this is your SOURCE OF TRUTH where tests passed!
- **ALWAYS** compare with v8.22.2 for correct DigiByte behavior and values
- **ALWAYS** preserve DigiByte-specific functionality
- **FIX** application bugs when found (don't work around them)
- **DOCUMENT** every change and bug found
- **USE** sub-agents for parallel processing efficiency
- **REFERENCE** the full paths when comparing:
  - v8.22.2: `/mnt/c/Users/Jared/code/digibyte/digibyte-v8.22.2/`
  - Bitcoin v26.2: `/mnt/c/Users/Jared/code/digibyte/bitcoin-v26.2-for-digibyte/`
  - Current v8.26: `/mnt/c/Users/Jared/code/digibyte/`

## Final Notes
- Tests may interact with each other - be aware of side effects
- Some tests require specific node configurations
- Watch for hardcoded addresses, keys, and transactions
- Multi-algorithm mining affects many consensus tests
- Dandelion++ affects transaction propagation tests

Your systematic approach using parallel sub-agents will ensure efficient test fixing while maintaining DigiByte's unique features and catching all application bugs.