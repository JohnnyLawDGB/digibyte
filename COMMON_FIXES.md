# DigiByte Test Suite - Common Fixes

## Quick Reference
Most test failures are caused by these 8 issues (in order of frequency):
1. **Block Rewards & Fees** - 72000 DGB (not 50 BTC), fees in sat/kB (not sat/vB)
2. **Coinbase Maturity** - Use COINBASE_MATURITY_2 (100) for wallet tests, COINBASE_MATURITY (8) for initial setup
3. **Dandelion++** - Causes transaction propagation delays
4. **Address Prefixes** - DigiByte uses different address formats
5. **Multi-Algo Mining** - Different block versions
6. **Fork Heights** - Difficulty changes at blocks 100, 200, 334, 400, 600
7. **Network Ports** - DigiByte uses different ports than Bitcoin
8. **Address & Key Generation** - Must use proper DigiByte prefixes and encoding

---

## 1. BLOCK REWARDS & FEES (Most Common)

### Block Rewards in Regtest
```python
# DigiByte regtest block rewards (first 1440 blocks):
SUBSIDY = 72000  # DGB, NOT 50 BTC!

# Common fix:
- assert_equal(balance, 50)      # Bitcoin
+ assert_equal(balance, 72000)   # DigiByte
```

### DigiByte Fee Structure
```python
# DigiByte uses satoshis/kB (not vB like Bitcoin)
# IMPORTANT: DigiByte fees are 100x Bitcoin fees (NOT 1000x!)
MIN_RELAY_TX_FEE = Decimal('0.001')  # DGB/kB = 100000 sat/kB (Bitcoin: 1000 sat/kB)
DEFAULT_FEE = Decimal('0.1')         # DGB/kB = 10000000 sat/kB (Bitcoin: 100000 sat/kB)

# Common fixes (100x multiplier):
# Bitcoin: 0.00001 BTC → DigiByte: 0.001 DGB (100x)
# Bitcoin: fee_rate=10 → DigiByte: fee_rate=1000 (100x)
# Bitcoin: 1000 sat/kB → DigiByte: 100000 sat/kB (100x)
```

### Quick Fixes
```python
# Insufficient funds error - reduce output amount
output_amount = Decimal('0.999')  # Instead of 0.99999

# Fee rate adjustments (100x multiplier)
fee_rate = 1000  # 100x Bitcoin's rate (10 → 1000)

# Bump fee tests - use higher increments
bumped_fee = original_fee + Decimal('0.01')  # Not 0.00001

# Balance tracking differences
# DigiByte has different balance categorization logic than Bitcoin:
# - getbalance() includes untrusted transactions (Bitcoin excludes them)
# - getunconfirmedbalance() only includes truly unconfirmed inputs
# - Balance categories (trusted vs untrusted_pending) work differently
# Common fixes in balance tests:
# Bitcoin: assert_equal(node.getbalance(), 0)  # excludes untrusted
# DigiByte: assert_equal(node.getbalance(), untrusted_amount)  # includes untrusted

# fundrawtransaction fee rate (for auto-calculated fees)
funded_tx = node.fundrawtransaction(raw_tx, {"fee_rate": 1000})  # sat/kB
```

---

## 2. COINBASE MATURITY (Second Most Common)

```python
from test_framework.blocktools import COINBASE_MATURITY, COINBASE_MATURITY_2

# DigiByte uses TWO maturity values:
COINBASE_MATURITY = 8      # Used for initial setup
COINBASE_MATURITY_2 = 100  # Used for most wallet tests

# IMPORTANT: Many tests use COINBASE_MATURITY_2 (100) even at low heights!
# Check what the Bitcoin test originally used.
```

### When to use which:
- **Bitcoin test uses 100?** → Use COINBASE_MATURITY_2 (100)
- **Bitcoin test uses 101?** → Use COINBASE_MATURITY_2 + 1 (101)
- **Initial setup/funding?** → Try COINBASE_MATURITY (8) first
- **Wallet operations?** → Often need COINBASE_MATURITY_2 (100)

### Common fixes:
```python
# Bitcoin test expecting 100 blocks maturity:
- self.generate(node, 100)
+ self.generate(node, COINBASE_MATURITY_2)  # Keep 100 for wallet tests

# Waiting for coinbase to mature:
- self.generate(node, 101)  # Bitcoin: 100 + 1
+ self.generate(node, COINBASE_MATURITY_2 + 1)  # DigiByte: 100 + 1

# Initial funding setup:
- self.generate(node, 100)
+ self.generate(node, COINBASE_MATURITY)  # Try 8 first for simple setups
```

### Test Failure Patterns:
```
AssertionError: not(8 == 100)  # Test expects Bitcoin's 100
bad-txns-premature-spend-of-coinbase  # Used 100 but needed 8
```

---

## 3. DANDELION++ ISSUES

### The Problem
Dandelion++ delays transaction propagation through stempool → mempool phases.

### The ONLY Fix
```python
def set_test_params(self):
    self.num_nodes = 2
    self.extra_args = [["-dandelion=0"], ["-dandelion=0"]]  # Disable for ALL nodes
```

**No other workarounds work reliably.** If a test has transaction propagation issues, disable Dandelion++.

---

## 4. ADDRESS PREFIXES

```python
# DigiByte address prefixes
MAINNET_P2PKH = 'D'           # Bitcoin: '1'
MAINNET_P2SH = 'S' or '3'     # Bitcoin: '3'
TESTNET_P2PKH = 'y'           # Bitcoin: 'm' or 'n'
TESTNET_P2SH = 's'            # Bitcoin: '2'
REGTEST_BECH32 = 'dgbrt'      # Bitcoin: 'bcrt'

# In tests, replace:
assert address.startswith('bcrt1')  # Bitcoin
# With:
assert address.startswith('dgbrt1')  # DigiByte
```

---

## 5. MULTI-ALGO MINING

### Block Versions
```python
# DigiByte regtest uses different block versions
REGTEST_BLOCK_VERSION = 0x20000000  # Version 536870912
POW_BLOCK_VERSION = 0x00000204      # Version 516 for PoW tests

# When creating blocks:
block.nVersion = 0x00000204  # Not 4 or 0x20000000
```

---

## 6. FORK HEIGHTS & DIFFICULTY ADJUSTMENTS

### The Problem
DigiByte evolved through multiple consensus changes that affect mining and difficulty in regtest:

```python
# Regtest Fork Heights in v8.26 (affect difficulty & validation)
MULTIALGO_HEIGHT = 100       # Block 100: 5 mining algorithms activated
MULTISHIELD_HEIGHT = 200     # Block 200: Per-algo difficulty adjustment
DIGISHIELD_HEIGHT = 334      # Block 334: DigiShield (real-time difficulty)
DIGISPEED_HEIGHT = 400       # Block 400: Faster difficulty response
ODOCRYPT_HEIGHT = 600        # Block 600: Odocrypt algo activates

# Note: v8.22.2 used different heights (e.g., multiAlgo at 290)
```

### Impact on Tests
- **Blocks 0-99**: Single algo (Scrypt), simple difficulty
- **Blocks 100+**: Multi-algo activated, difficulty can spike if mining single algo
- **Blocks 200+**: Each algo has independent difficulty
- **Most tests use blocks 0-300**, so they hit multi-algo but rarely reach Odocrypt

### Common Fix
```python
# For mining tests that fail due to difficulty:
# Option 1: Mine before multi-algo activation
self.generate(node, 50)  # Stay below block 100

# Option 2: Disable multi-algo for testing
self.extra_args = [["-easypow"]]  # Postpones multi-algo activation

# Option 3: Use lower difficulty for regtest
self.extra_args = [["-minimumdifficultyblocks=1"]]
```

---

## 7. NETWORK PORTS

### DigiByte Port Configuration
```python
# DigiByte uses different ports than Bitcoin
MAINNET_P2P = 12024      # Bitcoin: 8333
MAINNET_RPC = 14022      # Bitcoin: 8332
TESTNET_P2P = 12026      # Bitcoin: 18333
TESTNET_RPC = 14023      # Bitcoin: 18332
REGTEST_P2P = 14022      # Bitcoin: 18444
REGTEST_RPC = 14122      # Bitcoin: 18443

# In tests, replace:
self.nodes[0].add_p2p_connection(P2PInterface(), port=18444)  # Bitcoin
# With:
self.nodes[0].add_p2p_connection(P2PInterface(), port=14022)  # DigiByte
```

---

## 8. DIGIBYTE ADDRESS & KEY GENERATION (Critical for Address Validation)

### Understanding DigiByte Address Prefixes
DigiByte uses different Base58 prefixes and Bech32 HRPs than Bitcoin. Tests that validate addresses or use hardcoded addresses MUST use DigiByte-specific formats.

### Address Prefix Locations in Code
```python
# Found in: src/kernel/chainparams.cpp
# Mainnet:
base58Prefixes[PUBKEY_ADDRESS] = 30    # Mainnet P2PKH addresses start with 'D'
base58Prefixes[SCRIPT_ADDRESS] = 63    # Mainnet P2SH addresses start with 'S' 
base58Prefixes[SECRET_KEY] = 128       # Mainnet private keys (WIF)

# Testnet:
base58Prefixes[PUBKEY_ADDRESS] = 126   # Testnet P2PKH addresses start with 's' or 't'
base58Prefixes[SCRIPT_ADDRESS] = 140   # Testnet P2SH addresses start with 'y'
base58Prefixes[SECRET_KEY] = 254       # Testnet private keys (WIF)

# Regtest (same as Testnet):
base58Prefixes[PUBKEY_ADDRESS] = 126   # Regtest P2PKH addresses start with 's' or 't'
base58Prefixes[SCRIPT_ADDRESS] = 140   # Regtest P2SH addresses start with 'y'
base58Prefixes[SECRET_KEY] = 254       # Regtest private keys (WIF)

# Bech32 Human-Readable Parts (HRP):
MAINNET_BECH32 = 'dgb'     # Bitcoin: 'bc'
TESTNET_BECH32 = 'dgbt'    # Bitcoin: 'tb'
REGTEST_BECH32 = 'dgbrt'   # Bitcoin: 'bcrt'
```

### Generating Valid DigiByte Addresses in Tests

#### Method 1: Use Node's Built-in Address Generation (Recommended)
```python
# Get a new address from the node's wallet
address = node.getnewaddress()  # Automatically uses DigiByte format

# Get deterministic address for testing (already configured for DigiByte)
from test_framework.test_node import TestNode
deterministic_addr = node.get_deterministic_priv_key().address
# Returns addresses like 'swzkfmbaZb4KARFXeNvtECxhggYJnho4ud' (regtest P2PKH)
```

#### Method 2: Using Test Framework Address Utilities
```python
from test_framework.address import (
    key_to_p2pkh,
    key_to_p2wpkh,
    script_to_p2sh,
)
from test_framework.key import ECKey

# Generate a P2PKH address
key = ECKey()
key.generate()
p2pkh_addr = key_to_p2pkh(key.get_pubkey().get_bytes(), main=False)  # False for testnet/regtest
# Returns: Address starting with 's' or 't' (DigiByte regtest)

# Generate a P2WPKH (Bech32) address
p2wpkh_addr = key_to_p2wpkh(key.get_pubkey().get_bytes(), main=False)
# Returns: Address starting with 'dgbrt1' (DigiByte regtest)

# Generate a P2SH address
from test_framework.script import CScript, OP_TRUE
script = CScript([OP_TRUE])
p2sh_addr = script_to_p2sh(script, main=False)
# Returns: Address starting with 'y' (DigiByte regtest)
```

#### Method 3: Pre-configured Test Addresses (from test_node.py)
```python
# DigiByte regtest deterministic addresses and private keys
# Located in: test/functional/test_framework/test_node.py
PRIV_KEYS = [
    AddressKeyPair('swzkfmbaZb4KARFXeNvtECxhggYJnho4ud', 'ebJ1XYGDYWLtzRvUkXXoVFCcfsgfH5SDyT1ahyJQaseF7eN4RB2s'),
    AddressKeyPair('smr6v5Zfys8DMCbWXGtzmsJn2WgyqbxFvb', 'egy4VKXfsrHAy4NB2YDnfBE7cfwuR7puyMoCZbqhXcoSL3bsrhf4'),
    # ... more addresses available
]
```

### Common Address Format Fixes

#### Fix 1: Replace Hardcoded Bitcoin Addresses
```python
# Bitcoin test with hardcoded address:
- ADDRESS = "2N7yv4p8G8yEaPddJxY41kPihnWvs39qCMf"  # Bitcoin P2SH
+ ADDRESS = node.getnewaddress()  # Let node generate valid DigiByte address

# Or use a known DigiByte regtest P2SH address:
+ ADDRESS = "yb48vjS8NsHWbMpTb3QAbNUeYGW3F8eRas"  # DigiByte P2SH (regtest)
```

#### Fix 2: Update Bech32 Address Prefixes
```python
# Bitcoin segwit addresses:
- assert address.startswith('bcrt1')    # Bitcoin regtest
+ assert address.startswith('dgbrt1')   # DigiByte regtest

# Hardcoded Bech32 addresses need recalculation:
- ADDRESS = "bcrt1q2nfxmhd4n3c8834pj72xagvyr9gl57n5r94fsl"
+ ADDRESS = node.getnewaddress(address_type="bech32")  # Generate valid dgbrt1 address
```

#### Fix 3: Private Key (WIF) Format
```python
# Import private key - use DigiByte format
- privkey = "cTpB4YiyKiBcPxnefsDpbnDxFDffjqJob8wGCEDXxgQ7zQoMXJdH"  # Bitcoin WIF
+ privkey = "ebJ1XYGDYWLtzRvUkXXoVFCcfsgfH5SDyT1ahyJQaseF7eN4RB2s"  # DigiByte WIF

# Or generate new key:
+ privkey = node.dumpprivkey(node.getnewaddress())
```

### Special Constants for Testing
```python
# From test/functional/test_framework/address.py
ADDRESS_BCRT1_UNSPENDABLE = 'dgbrt1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqn5txr0'
ADDRESS_BCRT1_P2WSH_OP_TRUE = 'dgbrt1qft5p2uhsdcdc3l2ua4ap5qqfg4pjaqlp250x7us7a8qqhrxrxfsqjt28qf'
```

### Validation Function Updates
```python
# If test validates address format:
def is_valid_address(addr):
    # P2PKH (regtest/testnet)
    if addr.startswith(('s', 't')):  # DigiByte regtest P2PKH
        return True
    # P2SH (regtest/testnet)  
    if addr.startswith('y'):  # DigiByte regtest P2SH
        return True
    # Bech32 (regtest)
    if addr.startswith('dgbrt1'):  # DigiByte regtest segwit
        return True
    return False
```

### Quick Reference Table
| Address Type | Bitcoin Regtest | DigiByte Regtest | Example |
|-------------|-----------------|------------------|----------|
| P2PKH | starts with 'm' or 'n' | starts with 's' or 't' | swzkfmbaZb4KARFXeNvtECxhggYJnho4ud |
| P2SH | starts with '2' | starts with 'y' | yb48vjS8NsHWbMpTb3QAbNUeYGW3F8eRas |
| P2WPKH/P2WSH | bcrt1... | dgbrt1... | dgbrt1qft5p2uhsdcdc3l2ua4ap5qqfg4pjaqlp250x7us7a8qqhrxrxfsqjt28qf |
| Private Key (WIF) | starts with 'c' | starts with 'e' | ebJ1XYGDYWLtzRvUkXXoVFCcfsgfH5SDyT1ahyJQaseF7eN4RB2s |

### Common Test Failures Related to Addresses
- `Invalid or unsupported Base58-encoded address (-5)` → Wrong address prefix
- `Invalid DigiByte address` → Using Bitcoin format address
- `Address does not refer to key` → Private key doesn't match address format
- `Checksum mismatch` → Cannot just replace prefix, need proper encoding

### Pro Tips
1. **Always use node.getnewaddress()** when possible - it handles all formats correctly
2. **Check test_node.py PRIV_KEYS** for pre-configured test addresses
3. **Use address.py utilities** for programmatic address generation
4. **Never just string-replace** prefixes - addresses have checksums that must be recalculated
5. **When importing addresses** from Bitcoin tests, regenerate them using DigiByte tools

---

## QUICK DIAGNOSTIC CHECKLIST

When a test fails, check in this order:

1. **Fee error?** → Multiply fee rates by 100x (NOT 1000x!)
2. **Maturity error?** → Use COINBASE_MATURITY (8) not 100
3. **Transaction not found?** → Add `-dandelion=0` to all nodes
4. **Address validation?** → Check prefix (dgbrt, not bcrt)
5. **Invalid address error?** → Use node.getnewaddress() or see Pattern #8
6. **Block rejected?** → Set block.nVersion = 0x00000204
7. **Mining difficulty spike?** → Check fork height, use `-easypow`
8. **Connection refused?** → Check port numbers (14022 not 18444)

---

## FOR SUB-AGENTS

**Only add to this file if you find a UNIQUE issue not covered above.**

Most test failures are variations of the 7 issues listed. Before adding a new pattern:
1. Check if it's really a fee, maturity, or Dandelion issue in disguise
2. Verify it affects multiple tests (not just one)
3. Keep additions brief - just the pattern and fix

Remember: 90% of test failures are fees or coinbase maturity issues.

---

## NEW PATTERNS FOUND BY GROUP 15

### Pattern: Multi-Algorithm Mining Issues
**Error**: Algorithm 'sha256d' is not currently active.
**Solution**: Add `-easypow` to extra_args to postpone multi-algo activation
**Affects**: feature_reindex_readonly.py, any tests using `generateblock()`
**Added by**: Sub-Agent Group 15


### Pattern: Bitcoin to DigiByte Address Migration Issues
**Error**: Invalid or unsupported Base58-encoded address (-5)
**Solution**: Replace hardcoded Bitcoin addresses with DigiByte equivalents:
- `2N7yv4p8G8yEaPddJxY41kPihnWvs39qCMf` → `yb48vjS8NsHWbMpTb3QAbNUeYGW3F8eRas`
- `bcrt1q...` → `dgbrt1q...` (with proper checksums)
**Affects**: wallet_importdescriptors.py, any tests with hardcoded addresses
**Added by**: Sub-Agent Group 12

### Pattern: DigiByte Balance/Maturity Issues
**Error**: AssertionError: not(0E-8 == 72000)
**Solution**: Use COINBASE_MATURITY_2 (100) for wallet tests instead of COINBASE_MATURITY (8)
**Affects**: wallet_backup.py, wallet_descriptor.py
**Added by**: Sub-Agent Group 12

### Pattern: Signet Network Not Supported
**Error**: "Fatal internal error occurred" when trying to use `-signet` chain parameter
**Solution**: Add `raise SkipTest('DigiByte does not support signet network')` in skip_test_if_missing_module()
**Affects**: tool_signet_miner.py, any signet-specific tests
**Added by**: Sub-Agent Group 13


---
