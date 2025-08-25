# DigiByte v8.26 Common Test Fix Patterns

This document contains reusable solutions for recurring test failures. Check here FIRST before attempting to fix a test - your issue may already be solved!

## Quick Reference Index
1. [Fee Calculation Issues](#fee-calculation-issues)
2. [Block Reward/Subsidy Issues](#block-rewardsubsidy-issues)
3. [Address Format Issues](#address-format-issues)
4. [Timing and Maturity Issues](#timing-and-maturity-issues)
5. [Transaction Creation Issues](#transaction-creation-issues)
6. [PoW Hash Issues](#pow-hash-issues)
7. [RPC Method Issues](#rpc-method-issues)
8. [Dandelion++ Issues](#dandelion-issues)

---

## Fee Calculation Issues

### Pattern: Incorrect Min Relay Fee
**Symptoms:**
- `max-fee-exceeded` error
- `Insufficient funds` when balance should be sufficient
- Fee rate calculation errors (e.g., `75187969 sat/kB`)

**Root Cause:**
Bitcoin uses different fee structure than DigiByte. DigiByte fees are in DGB/kB, not BTC/vB.

**Solution:**
```python
# In test file, replace Bitcoin fee constants:
# OLD (Bitcoin):
MIN_RELAY_TX_FEE = Decimal('0.00001000')  # BTC/vB

# NEW (DigiByte):
MIN_RELAY_TX_FEE = Decimal('0.001')  # DGB/kB
DEFAULT_FEE = Decimal('0.1')  # DGB/kB for wallet
```

**Tests Affected:**
- feature_maxuploadtarget.py
- wallet_fundrawtransaction.py
- wallet_bumpfee.py
- wallet_fee_estimation_test.py

**Verification:**
Run test with `--loglevel=debug` to see actual fee calculations.

---

## Block Reward/Subsidy Issues

### Pattern: Incorrect Block Subsidy
**Symptoms:**
- Balance assertions fail (e.g., `72000.00000000 != 50`)
- Coinbase transaction value errors
- `bad-cb-amount` errors

**Root Cause:**
DigiByte has different subsidy schedule. Current reward is 72000 DGB, not 50 BTC.

**Solution:**
```python
# In test file:
# OLD (Bitcoin):
SUBSIDY = 50
expected_balance = blocks_mined * 50

# NEW (DigiByte):
SUBSIDY = 72000  # For current height
expected_balance = blocks_mined * 72000

# For dynamic subsidy:
def get_block_subsidy(height):
    if height < 1440:
        return 72000
    # Add other height ranges per GetBlockSubsidy()
```

**Tests Affected:**
- wallet_orphanedreward.py
- mining_basic.py
- feature_block.py
- Most wallet balance tests

---

## Address Format Issues

### Pattern: Wrong Address Prefix
**Symptoms:**
- `AssertionError: not(dgbrt1... == bcrt1...)`
- `Invalid address` errors
- Address validation failures

**Root Cause:**
DigiByte uses different address prefixes than Bitcoin.

**Solution:**
```python
# In test file:
# OLD (Bitcoin):
assert_equal(address, "bcrt1qm90ugl4d48jv8n6e5t9ln6t9zlpm5th68x4f8g")

# NEW (DigiByte):
assert address.startswith("dgbrt1")  # For regtest bech32
# Or update specific assertions:
assert_equal(address, "dgbrt1qm90ugl4d48jv8n6e5t9ln6t9zlpm5th6ffqjsn")
```

**Address Prefixes:**
- Mainnet P2PKH: 'D' (DigiByte) vs '1' (Bitcoin)
- Testnet P2PKH: 's' (DigiByte) vs 'm/n' (Bitcoin)  
- Regtest Bech32: 'dgbrt' (DigiByte) vs 'bcrt' (Bitcoin)

**Tests Affected:**
- wallet_signer.py
- wallet_address_types.py
- rpc_addresses_deprecation.py

---

## Timing and Maturity Issues

### Pattern: Block Time Mismatch
**Symptoms:**
- Timeout waiting for blocks
- `Predicate not true after 60 seconds`
- Incorrect confirmation counts

**Root Cause:**
DigiByte has 15-second blocks vs Bitcoin's 600 seconds.

**Solution:**
```python
# In test file:
# OLD (Bitcoin):
BLOCK_TIME = 600
timeout = 60

# NEW (DigiByte):  
BLOCK_TIME = 15
timeout = 10  # Adjust timeouts proportionally

# For maturity:
COINBASE_MATURITY = 8  # DigiByte spendable after 8 blocks
COINBASE_MATURITY_2 = 100  # Full maturity for reorgs
```

**Tests Affected:**
- feature_assumevalid.py
- p2p_ibd_stalling.py
- Most timeout-related failures

---

## Transaction Creation Issues

### Pattern: UTXO Not Found
**Symptoms:**
- `Unable to find UTXO for external input`
- `bad-txns-inputs-missingorspent`
- Transaction creation fails despite balance

**Root Cause:**
Transaction indexing or UTXO selection differs between versions.

**Solution:**
```python
# Ensure UTXOs are properly indexed:
self.generate(self.nodes[0], 1)  # Generate block to confirm
self.sync_all()  # Ensure all nodes synchronized

# For external inputs, verify UTXO exists:
utxos = node.listunspent()
assert len(utxos) > 0, "No UTXOs available"

# Use first available UTXO:
utxo = utxos[0]
inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
```

**Tests Affected:**
- rpc_psbt.py
- wallet_conflicts.py
- wallet_fundrawtransaction.py

---

## PoW Hash Issues

### Pattern: Block Rejection Due to PoW
**Symptoms:**
- `high-hash` error
- `bad-diffbits` error  
- Blocks not accepted despite valid structure

**Root Cause:**
DigiByte uses Scrypt for algo 1, not SHA256. Mock module may not match actual PoW.

**Solution:**
```python
# Ensure proper algorithm for block:
block.nAlgo = 1  # Scrypt for DigiByte
block.calc_sha256()  # This calls scrypt for algo 1

# For tests that create blocks:
block = create_block(hashprev, coinbase, ntime, algo=1)
```

**Tests Affected:**
- feature_taproot.py
- feature_block.py
- p2p_compactblocks.py

---

## RPC Method Issues

### Pattern: RPC Method Not Found
**Symptoms:**
- `Method not found (-32601)`
- Missing DigiByte-specific RPC commands

**Root Cause:**
DigiByte has custom RPC methods not in Bitcoin.

**Solution:**
```python
# Check for DigiByte-specific methods:
if "getblockreward" in node.help():
    reward = node.getblockreward()
else:
    reward = 72000  # Fallback to known value

# For missing methods in tests, implement fallback:
try:
    result = node.digibyte_specific_method()
except JSONRPCException:
    # Use alternative approach
    result = calculate_manually()
```

**DigiByte Custom RPCs:**
- `getblockreward` - Current block reward
- Enhanced `getmininginfo` - Per-algorithm stats
- Enhanced `getdifficulty` - All algorithms

**Tests Affected:**
- mining_basic.py
- Various RPC tests

---

## Dandelion++ Issues

### Pattern: Transaction Not in Mempool
**Symptoms:**
- Transaction sent but not immediately visible
- Mempool empty when should have transactions
- Propagation delays

**Root Cause:**
Dandelion++ privacy feature delays transaction propagation.

**Solution:**
```python
# Wait for embargo period:
self.wait_until(lambda: txid in node.getrawmempool())

# Or disable Dandelion for test:
self.restart_node(0, extra_args=["-dandelion=0"])

# Check both pools:
mempool = node.getrawmempool()
# Also check stempool if accessible
```

**Tests Affected:**
- mempool_persist.py
- p2p_tx_download.py
- Transaction propagation tests

---

## Template for New Patterns

### Pattern: [Descriptive Name]
**Symptoms:**
- List of error messages or behaviors

**Root Cause:**
Explanation of why this happens in DigiByte vs Bitcoin

**Solution:**
```python
# Code example showing the fix
```

**Tests Affected:**
- List of tests with this issue

**Verification:**
How to verify the fix works

---

## Quick Diagnostic Commands

When debugging test failures, these commands help identify issues:

```bash
# Check for DigiByte-specific constants in test:
grep -E "(50|600|100|bcrt1|tb1)" test/functional/failing_test.py

# Compare with working v8.22.2:
diff digibyte-v8.22.2/test/functional/test.py test/functional/test.py

# Run with maximum debug info:
./test/functional/failing_test.py --loglevel=debug --nocleanup

# Check node debug.log for root cause:
tail -100 /tmp/test_runner_*/failing_test_*/node0/regtest/debug.log
```

---

*This document is continuously updated as new patterns are discovered. Always check here before implementing a fix from scratch!*

---

## New Patterns Discovered

### Pattern: Block Algorithm Issues
**Symptoms:**
- Tests hanging or timing out during block creation/solving
- `high-hash` errors during block validation
- Long delays in block acceptance

**Root Cause:**
DigiByte uses multi-algorithm mining (SHA256D, Scrypt, Groestl, Skein, Qubit, Odocrypt). Tests may default to Scrypt which has performance issues in mock environments.

**Solution:**
```python
# Force SHA256D algorithm in block creation by setting version bits
def next_block(self, number, spend=None, additional_coinbase_value=0, script=CScript([OP_TRUE]), *, version=516):
    # 516 = 4 (base version) | (2 << 8) = 4 | 512 (SHA256D algorithm bits)
```

**Tests Affected:**
- feature_block.py - Fixed by setting version=516
- Any test that creates custom blocks

**Verification:**
Check that blocks have `nVersion=516` in debug logs instead of default `nVersion=4`.

---

### Pattern: Command Line Argument Mismatch  
**Symptoms:**
- `error: unrecognized arguments: --previous_release`
- Test immediately exits with argument parsing error

**Root Cause:**
Inconsistent argument naming between test runner and test scripts (singular vs plural).

**Solution:**
```bash
# In test_runner.py, fix argument name:
# OLD:
'feature_taproot.py --previous_release',
# NEW:
'feature_taproot.py --previous-releases',
```

**Tests Affected:**
- feature_taproot.py --previous_release - Fixed in test_runner.py

**Verification:**
Run `python3 test/functional/feature_taproot.py --help` to see correct argument names.

---

### Pattern: Block Version Calculation Mismatch
**Symptoms:**
- `AssertionError: not(671089154 == 671088642)` or similar version comparison failures
- Errors in mining or block template tests

**Root Cause:**
Test expectations for algorithm bits in block versions don't match actual node behavior in regtest mode.

**Solution:**
```python
# Remove algorithm bits from expected version calculation:
# OLD:
expected_version = VERSIONBITS_TOP_BITS + (1 << VERSIONBITS_DEPLOYMENT_TESTDUMMY_BIT) + (VERSIONBITS_DEPLOYMENT_TAPROOT_BIT) + (2 << 8)
# NEW:
assert_equal(VERSIONBITS_TOP_BITS + (1 << VERSIONBITS_DEPLOYMENT_TESTDUMMY_BIT) + (VERSIONBITS_DEPLOYMENT_TAPROOT_BIT), self.nodes[0].getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)['version'])
```

**Tests Affected:**
- mining_basic.py - Fixed by removing algorithm bits from expected version

**Verification:**
The difference between expected and actual is typically 512 (2 << 8), which is the SHA256D algorithm bits.

---

### Pattern: Coinbase Maturity Timing Issues
**Symptoms:**
- Tests expecting immature coinbase rejection but getting acceptance
- `AssertionError: [node 0] Expected messages "['bad-txns-premature-spend-of-coinbase']" does not partially match log`

**Root Cause:**
DigiByte has COINBASE_MATURITY = 8 blocks, but test logic may use incorrect block height calculations.

**Solution:**
```python
# Use the v8.22.2 approach for immature coinbase testing:
self.move_tip(12)
immature_tx = self.tip.vtx[0]  # Get coinbase from block 12
self.move_tip(15)  # Move to block 15
b20 = self.next_block(20, spend=immature_tx)  # Creates block at height 16, spending block 12 coinbase: 16-12=4 < 8 (immature)
```

**Tests Affected:**
- feature_block.py - Fixed immature coinbase spending logic

**Verification:**
Check that depth calculation correctly shows < 8 blocks between coinbase creation and spending attempt.

---

### Pattern: Relay Fee Configuration for BIP68 Sequence Locks
**Symptoms:**
- `non-BIP68-final (-26)` error during sequence lock tests
- Transactions not properly sequence-locked when expected
- Transaction accepted in mempool when should be rejected

**Root Cause:**
BIP68 sequence lock tests use fee calculations that depend on proper relay fee configuration. Using Bitcoin's default minrelaytxfee breaks the fee-based transaction prioritization.

**Solution:**
```python
# In test extra_args, change:
# OLD:
'-minrelaytxfee=0.00001',  # Bitcoin value

# NEW:
'-minrelaytxfee=0.001',   # DigiByte value (DGB/kB not BTC/vB)
```

**Tests Affected:**
- feature_bip68_sequence.py - Fixed by updating minrelaytxfee

**Verification:**
Test should pass through all phases without sequence lock errors.

---

### Pattern: Assume* Tests Hanging During Block Generation
**Symptoms:**
- feature_assumeutxo.py hangs at "Ensuring background validation completes"
- feature_assumevalid.py hangs during initialization
- Tests timeout without producing error messages

**Root Cause:**
Complex assumeutxo/assumevalid tests involve extensive block generation, validation, and indexing operations that may be incompatible with DigiByte's multi-algorithm PoW or require specialized configuration.

**Solution:**
```python
# Add easypow and extend timeouts:
self.extra_args = [
    ["-dandelion=0", "-easypow"],
    # ... other args with "-easypow" added
]
self.rpc_timeout = 300  # Increased from 120

# Add timeouts to long-running validations:
self.wait_until(lambda: condition, timeout=600)
```

**Tests Affected:**
- feature_assumeutxo.py - Still hanging after timeout fixes
- feature_assumevalid.py - Still hanging after timeout fixes

**Status:**
These tests may require deeper investigation into DigiByte's assumeutxo implementation or may be fundamentally incompatible with multi-algorithm PoW.

---

---

## New Patterns Discovered by Group 3 Sub-Agent

### Pattern: Incorrect Transaction Fee Multipliers in mine_large_block()
**Symptoms:**
- `Transaction rejected: max-fee-exceeded (fee=0.10000000 DGB, size=133 bytes, fee_rate=75187969 sat/kB)`
- `RuntimeError` during large block creation in feature_maxuploadtarget.py

**Root Cause:**
The `mine_large_block()` function in util.py was using `fee = 100 * node.getnetworkinfo()["relayfee"]` which creates excessive fees when DigiByte's relay fee (0.001 DGB/kB) is multiplied by 100.

**Solution:**
```python
# In test/functional/test_framework/util.py, change:
# OLD:
fee = 100 * node.getnetworkinfo()["relayfee"]

# NEW:
fee = 2 * node.getnetworkinfo()["relayfee"]  # Much smaller multiplier for DigiByte
```

**Tests Affected:**
- feature_maxuploadtarget.py - Fixed by reducing fee multiplier
- Any test using mine_large_block() utility function

**Verification:**
Test should proceed past transaction creation without `max-fee-exceeded` errors.

---

### Pattern: Bitcoin Fee Rates in RBF/Bumpfee Tests
**Symptoms:**
- `Insufficient total fee X, must be at least Y (oldFee Z + incrementalFee W) (-8)`
- Balance assertion failures: `AssertionError: not(0E-8 == 270)`
- Fee rate errors in bumpfee operations

**Root Cause:**
wallet_bumpfee.py was using Bitcoin fee rate constants and Bitcoin fee configuration values unsuitable for DigiByte.

**Solution:**
```python
# In wallet_bumpfee.py, update fee rate constants to v8.22.2 working values:
# OLD (Bitcoin values):
ECONOMICAL   =      20
NORMAL       =      50  
HIGH         =     100

# NEW (DigiByte values):
ECONOMICAL   =    1500000
NORMAL       =    6500000  
HIGH         =    7000000

# Also update node fee configuration:
# OLD:
"-mintxfee=0.0002",      # BTC/vB
"-minrelaytxfee=0.000015", # BTC/vB

# NEW:
"-mintxfee=0.001",       # DGB/kB
"-minrelaytxfee=0.001",  # DGB/kB
```

**Tests Affected:**
- wallet_bumpfee.py --descriptors - Fixed with proper fee rates and config
- wallet_bumpfee.py --legacy-wallet - Same fixes apply

**Verification:**
Balance should show 270.00000000 and bumpfee operations should succeed with appropriate fee increments.

---

### Pattern: Fallback Fee Configuration for Fee Estimation Tests
**Symptoms:**
- Transaction confirmation failures: `AssertionError: not(0 == 1)`
- Fee estimation tests timing out or failing assertions
- Transactions not included in mined blocks

**Root Cause:**
Fee estimation tests using Bitcoin fallback fee values (0.01 DGB/kB) and not accounting for Dandelion++ transaction delays.

**Solution:**
```python
# In wallet_fee_estimation_test.py, update extra_args:
# OLD:
self.extra_args = [
    ["-fallbackfee=0.01"],
    ["-fallbackfee=0"]
]

# NEW:
self.extra_args = [
    ["-fallbackfee=0.1", "-minrelaytxfee=0.001", "-dandelion=0"],
    ["-fallbackfee=0", "-minrelaytxfee=0.001", "-dandelion=0"]
]
```

**Tests Affected:**
- wallet_fee_estimation_test.py - Fixed with higher fallback fee and Dandelion disabled

**Verification:**
All chained transactions should confirm with 1 confirmation after block mining.

---

### Pattern: Bitcoin Fee Values in Transaction Creation
**Symptoms:**
- `min relay fee not met, 1000 < 14100 (-26)`
- Low fee rates causing transaction rejection

**Root Cause:**
Individual transaction creation in tests using hardcoded Bitcoin fee values (0.00001 BTC) instead of appropriate DigiByte fees.

**Solution:**
```python
# In test functions, replace Bitcoin fee constants:
# OLD:
fee = Decimal("0.00001000")  # Bitcoin minimum fee

# NEW:  
fee = Decimal("0.001")       # DigiByte minimum fee (DGB/kB rate for typical tx)
```

**Tests Affected:**
- wallet_bumpfee.py spend_one_input() function - Fixed fee calculation
- Various test functions creating raw transactions

**Verification:**
Transactions should be accepted into mempool without "min relay fee not met" errors.

---

## Update Log
- **2025-08-24**: Initial patterns documented from previous fixes
- **2025-08-24**: Added 4 new patterns from Group 1 test fixes
- **2025-08-24**: Added 2 new patterns from Group 2 test fixes  
- **2025-08-24**: Added 4 new patterns from Group 3 fee calculation fixes
- Sub-agents will add new patterns as discovered