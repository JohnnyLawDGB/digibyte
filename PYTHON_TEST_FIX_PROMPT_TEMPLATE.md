# DigiByte v8.26 Python Functional Test Fix Instructions

## Quick Start Workflow
Your task is to fix Python functional tests by addressing the underlying issues.
**NEVER skip or disable tests - always fix the root cause.**

### Initial Setup (Run Once)
```bash
# Prerequisites
pip install --break-system-packages digibyte-scrypt

# Build binaries if needed
cd /mnt/c/Users/Jared/code/digibyte
./configure --without-gui && make -j8
```

### Main Workflow (Repeat Until All Tests Pass)
1. **Run test runner to find next failing test:**
   ```bash
   ./test/functional/test_runner.py --failfast
   ```

2. **Fix the failing test properly**
   - Analyze the error and find root cause
   - Compare with reference code
   - Apply DigiByte-specific fixes
   - Fix application bugs if discovered
   - Update test data/constants as needed
   - NEVER skip or disable tests

3. **Verify and move to next test**

## Reference Directories
Always compare these three codebases when fixing tests:
- **Current v8.26**: `/mnt/c/Users/Jared/code/digibyte/` (what we're fixing)
- **DigiByte v8.22.2**: `/mnt/c/Users/Jared/code/digibyte/digibyte-v8.22.2/` (SOURCE OF TRUTH for DigiByte behavior)
- **Bitcoin v26.2**: `/mnt/c/Users/Jared/code/digibyte/bitcoin-v26.2-for-digibyte/` (to understand what changed)

## Critical DigiByte Constants
These cause the most test failures:

### 1. Coinbase Maturity (TWO VALUES!)
```python
# CRITICAL: DigiByte has TWO maturity values
COINBASE_MATURITY = 8      # DigiByte spendable maturity (NOT 100!)
COINBASE_MATURITY_2 = 100  # Full maturity for some operations
```

### 2. Fees
```python
# Transaction fees (per kB)
DEFAULT_TRANSACTION_MINFEE = Decimal('0.1')     # 0.1 DGB/kB (wallet minimum fee)
DEFAULT_FALLBACK_FEE = Decimal('0.01')          # 0.01 DGB/kB (fallback fee)
MIN_RELAY_TX_FEE = Decimal('0.0001')            # 0.0001 DGB/kB (relay minimum)
DUST_RELAY_TX_FEE = Decimal('0.0003')           # 0.0003 DGB/kB (dust threshold)

# IMPORTANT: v8.26 reduced MIN_RELAY_TX_FEE from 0.001 to 0.0001 DGB/kB
```

### 3. Block Timing
```python
BLOCK_TIME = 15  # 15 seconds (NOT 600)
```

### 4. Supply
```python
MAX_MONEY = 21000000000  # 21 billion DGB (NOT 21 million BTC)
```

### 5. Network Ports
```python
P2P_PORT = 12024         # Mainnet
P2P_PORT_TESTNET = 12025 # Testnet
```

### 6. Address Formats
```python
# Testnet addresses
TESTNET_ADDRESS_PREFIX = 's'    # P2PKH/P2SH (NOT 'm' or 'n')
TESTNET_BECH32_HRP = 'dgbt'    # Bech32 (NOT 'tb')
REGTEST_BECH32_HRP = 'dgbrt'   # Regtest (NOT 'bcrt')
```

### 7. Current Block Reward
```python
SUBSIDY = 72000  # Current reward in DGB
```

## Common Test Failures and Solutions

### 1. Coinbase Maturity Assertions
```python
# WRONG (Bitcoin):
assert_equal(wallet.getbalance(), 50)  # After 100 blocks

# CORRECT (DigiByte):
assert_equal(wallet.getbalance(), 72000)  # After 8 blocks for spending
# BUT some operations still need 100 blocks!
```

### 2. Fee Calculations
```python
# WRONG (Bitcoin):
fee = Decimal('0.0001')  # Bitcoin's default

# CORRECT (DigiByte):
# Use the appropriate fee based on context:
min_fee = Decimal('0.1')      # DEFAULT_TRANSACTION_MINFEE for wallet operations
relay_fee = Decimal('0.0001') # MIN_RELAY_TX_FEE for relay checks
fallback = Decimal('0.01')    # DEFAULT_FALLBACK_FEE when fee estimation fails
```

### 3. Timing Adjustments
```python
# WRONG (Bitcoin):
self.generate(self.nodes[0], 144)  # One day of blocks

# CORRECT (DigiByte):
self.generate(self.nodes[0], 5760)  # One day: 86400/15 = 5760 blocks
```

### 4. RPC Method Not Found
- Check for DigiByte-specific RPC: `getblockreward`, enhanced `getmininginfo`, etc.
- Check if Bitcoin methods were renamed or removed

### 5. Address Format Errors
Always use DigiByte address formats in tests. Update hardcoded addresses.

## DigiByte-Specific Features

### Dandelion++ Privacy
Tests may fail due to Dandelion++ transaction routing. Look for:
- `NetMsgType::DANDELIONTX` messages
- Stem pool behavior
- Different mempool propagation

### Multi-Algorithm Mining
DigiByte uses 5 algorithms (SHA256D, Scrypt, Groestl, Skein, Qubit) + Odocrypt:
```python
# Mining tests may need algorithm specification
node.generatetoaddress(1, address, algo=ALGO_SHA256D)
```

## Test Fixing Process

### 1. Run Individual Test
```bash
# Basic run
./test/functional/test_name.py

# With wallet variant
./test/functional/test_name.py --legacy-wallet
./test/functional/test_name.py --descriptors

# Debug mode
./test/functional/test_name.py --loglevel=debug
```

### 2. Compare Reference Code
```bash
# Check if test exists in v8.22.2
diff test/functional/test_name.py digibyte-v8.22.2/test/functional/test_name.py

# Compare with Bitcoin v26.2
diff test/functional/test_name.py bitcoin-v26.2-for-digibyte/test/functional/test_name.py
```

### 3. Apply Fixes
Focus on:
1. Update constants (maturity, fees, timing)
2. Fix address formats
3. Handle DigiByte-specific RPC
4. Adjust for Dandelion++ behavior
5. Account for multi-algo mining

### 4. Document Application Bugs
If you find bugs in application code (not just tests):

```markdown
## APPLICATION BUG FIXED
**File**: src/validation.cpp:XXX
**Test**: test_name.py
**Issue**: [description]
**Root Cause**: Bitcoin v26.2 merge impact
**Fix Applied**: [code changes]
**Impact**: [what happens if not fixed]
```

## Multi-Algorithm Mining in RegTest

### The Problem
DigiByte uses 5 mining algorithms (SHA256D, Scrypt, Groestl, Skein, Qubit) + Odocrypt.
This causes test failures due to:
1. **Mining timeouts**: Tests expect fast block generation but multi-algo switching slows it down
2. **Algorithm activation**: Different algorithms activate at different block heights
3. **Difficulty adjustments**: DigiShield adjusts difficulty between algorithms
4. **Version bits**: Algorithm bits in block version conflict with test expectations

### The Solution (Already Applied in chainparams.cpp)
```cpp
// Set initial targets for all algorithms to maximum (easiest) difficulty
consensus.initialTarget[ALGO_SHA256D] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
consensus.initialTarget[ALGO_SCRYPT] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
// ... same for all algorithms

// Enable easy mining for tests
consensus.fEasyPow = true;
consensus.fPowNoRetargeting = true; // Disable retargeting to avoid difficulty spikes
```

### Hard Fork Heights in RegTest
- Block 100: MultiAlgo activation
- Block 200: MultiShield activation  
- Block 400: DigiSpeed activation
- Block 600: Odocrypt activation

**IMPORTANT**: Do NOT change these heights as C++ unit tests depend on them.

### RPC Mining with Algorithm
DigiByte's `generatetoaddress` RPC accepts an optional `algo` parameter:
```python
node.generatetoaddress(nblocks=1, address=addr, algo="sha256d")
```
However, algorithms must be active at the current height to be used.

## Common Pitfalls
1. **Coinbase maturity**: Remember there are TWO values (8 and 100)
2. **Fee confusion**: 
   - Wallet min fee: 0.1 DGB/kB (DEFAULT_TRANSACTION_MINFEE)
   - Relay min fee: 0.0001 DGB/kB (MIN_RELAY_TX_FEE - reduced from 0.001 in v8.26!)
   - Don't confuse them!
3. **Block timing**: Many timeouts need adjustment for 15-second blocks
4. **Supply math**: 21 billion vs 21 million affects many calculations
5. **Address validation**: Must use DigiByte prefixes
6. **Algorithm version bits**: Tests checking version bits must account for algorithm bits
7. **Mining before block 100**: Only Scrypt is available before multi-algo activation

## Verification
After fixing a test:
```bash
# Run the single test
./test/functional/test_name.py

# Continue to next failing test
./test/functional/test_runner.py --failfast
```

Keep fixing tests one by one until all pass!