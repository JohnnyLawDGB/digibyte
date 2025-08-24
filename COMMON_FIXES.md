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

## Update Log
- **2025-08-24**: Initial patterns documented from previous fixes
- Sub-agents will add new patterns as discovered