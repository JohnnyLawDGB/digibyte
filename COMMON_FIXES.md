# DigiByte Test Suite - Common Fixes

## Quick Reference
Most test failures are caused by these 5 issues (in order of frequency):
1. **Fees** - DigiByte uses different fee structure than Bitcoin
2. **Coinbase Maturity** - 8 blocks (COINBASE_MATURITY) vs 100 blocks (COINBASE_MATURITY_2)
3. **Dandelion++** - Causes transaction propagation delays
4. **Address Prefixes** - DigiByte uses different address formats
5. **Multi-Algo Mining** - Different block versions and fork heights

---

## 1. FEE ISSUES (Most Common)

### DigiByte Fee Structure
```python
# DigiByte uses satoshis/KvB (not vB like Bitcoin)
MIN_RELAY_TX_FEE = Decimal('0.001')  # DGB/kB = 1000 sat/kB
DEFAULT_FEE = Decimal('0.1')         # DGB/kB = 100000 sat/kB

# Common fixes:
# Bitcoin: 0.00001 BTC → DigiByte: 0.001 DGB
# Bitcoin: fee_rate=10 → DigiByte: fee_rate=1000
```

### Quick Fixes
```python
# Insufficient funds error - reduce output amount
output_amount = Decimal('0.999')  # Instead of 0.99999

# Fee rate adjustments
fee_rate = 1000  # sat/kB instead of 10 sat/vB

# Bump fee tests - use higher increments
bumped_fee = original_fee + Decimal('0.01')  # Not 0.00001
```

---

## 2. COINBASE MATURITY (Second Most Common)

```python
from test_framework.blocktools import COINBASE_MATURITY, COINBASE_MATURITY_2

# DigiByte uses TWO maturity values:
COINBASE_MATURITY = 8      # Default for most operations
COINBASE_MATURITY_2 = 100  # After certain height, for some operations

# Common pattern in tests:
self.generate(self.nodes[0], COINBASE_MATURITY)  # Use 8, not 100
```

### When to use which:
- **COINBASE_MATURITY (8)**: Most test setups, initial funding
- **COINBASE_MATURITY_2 (100)**: Later blocks, specific height-dependent tests

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

### Fork Heights (Regtest)
```python
# Key activation heights for regtest
CSV_ACTIVATION_HEIGHT = 500
SEGWIT_HEIGHT = 0  # Always active in regtest
```

---

## QUICK DIAGNOSTIC CHECKLIST

When a test fails, check in this order:

1. **Fee error?** → Multiply fee rates by 100-1000x
2. **Maturity error?** → Use COINBASE_MATURITY (8) not 100
3. **Transaction not found?** → Add `-dandelion=0` to all nodes
4. **Address validation?** → Check prefix (dgbrt, not bcrt)
5. **Block rejected?** → Set block.nVersion = 0x00000204

---

## FOR SUB-AGENTS

**Only add to this file if you find a UNIQUE issue not covered above.**

Most test failures are variations of the 5 issues listed. Before adding a new pattern:
1. Check if it's really a fee, maturity, or Dandelion issue in disguise
2. Verify it affects multiple tests (not just one)
3. Keep additions brief - just the pattern and fix

Remember: 90% of test failures are fees or coinbase maturity issues.