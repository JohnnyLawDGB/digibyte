# Quick Test Guide - Enhanced DigiDollar Qt Test

## Prerequisites

1. Build the DigiBytes node:
```bash
make -j$(nproc)
```

2. Ensure required tools are installed:
```bash
# Check for jq
which jq || sudo apt-get install jq

# Check for bc (calculator)
which bc || sudo apt-get install bc

# Check for curl
which curl || sudo apt-get install curl
```

## Running the Test

```bash
# Make script executable (if not already)
chmod +x test_digidollar_qt.sh

# Run the test
./test_digidollar_qt.sh
```

## What to Expect

### Test Duration
- Approximately **5-7 minutes** total runtime
- Includes blockchain generation, syncing, and verification

### Test Stages

1. **Setup Phase** (1-2 min)
   - Starts 3 Qt wallet instances
   - Creates wallets for Bob, Alice, and Charlie
   - Generates initial blockchain

2. **Minting Phase** (2-3 min)
   - Bob mints 4 DigiDollar vaults
   - Network monitoring at each step

3. **Redemption Phase** (1-2 min)
   - Tests early redemption (should fail)
   - Waits for lock expiry
   - Tests successful redemption

4. **Transfer Phase** (1-2 min)
   - Bob sends $34.67 to Alice
   - Bob sends $12.53 to Charlie
   - Verifies all balances

5. **Final Verification** (instant)
   - Comprehensive balance checks
   - DD conservation verification

### Output Monitoring Points

Look for these 6 monitoring checkpoints:

```
NETWORK MONITOR: After Bob's 3 Mints
NETWORK MONITOR: After Bob's 4th Mint ($10 DD, 1-hour lock)
NETWORK MONITOR: Halfway Through Lock Period (120 blocks)
NETWORK MONITOR: After Redemption of 4th Mint
NETWORK MONITOR: After Transfer #1 (Bob → Alice $34.67)
NETWORK MONITOR: After Transfer #2 (Bob → Charlie $12.53)
```

Each checkpoint shows:
- Network stats from all 3 nodes
- Individual wallet balances
- Verification results
- DD conservation check

## Success Indicators

### Console Output

At the end, you should see:

```
========================================
ALL TESTS PASSED!
========================================

Test Summary:
  1. ✓ Bob minted $175.00 DD (3 long-term vaults)
  2. ✓ Bob minted $10.00 DD with 1-hour lock
  3. ✓ Alice and Charlie synced and saw same network stats
  4. ✓ Network stats matched across all 3 nodes at every step
  5. ✓ Early redemption correctly rejected
  6. ✓ Redemption succeeded after lock expired
  7. ✓ Exact collateral amount returned on redemption
  8. ✓ Bob sent $34.67 DD to Alice
  9. ✓ Bob sent $12.53 DD to Charlie
 10. ✓ All final balances verified correct
 11. ✓ DD conservation verified (sum = network total)
```

### Qt GUI Windows

The script leaves 3 Qt windows open for manual verification:

#### Window 1: Bob's Wallet
- **Personal DD Balance**: $127.80
- **Network Total DD**: $175.00
- **Network Collateral**: 57,500 DGB
- **Health**: 328%
- **Transactions**: 2 outgoing DD transfers

#### Window 2: Alice's Wallet
- **Personal DD Balance**: $34.67
- **Network Total DD**: $175.00 (same as Bob)
- **Network Collateral**: 57,500 DGB (same as Bob)
- **Health**: 328% (same as Bob)
- **Transactions**: 1 incoming DD transfer

#### Window 3: Charlie's Wallet
- **Personal DD Balance**: $12.53
- **Network Total DD**: $175.00 (same as Bob and Alice)
- **Network Collateral**: 57,500 DGB (same as Bob and Alice)
- **Health**: 328% (same as Bob and Alice)
- **Transactions**: 1 incoming DD transfer

## Expected Final State

### Network-Wide Statistics (Identical on All 3 Wallets)
| Metric | Value |
|--------|-------|
| Total DD Supply | 17,500 cents ($175.00) |
| Total Collateral | 57,500 DGB |
| System Health | 328% |
| Active Vaults | 3 (Bob's long-term vaults) |

### Individual Wallet Balances
| Wallet | DD Balance | Description |
|--------|------------|-------------|
| Bob | 12,780 cents ($127.80) | Original minter, sent DD to others |
| Alice | 3,467 cents ($34.67) | Received from Bob |
| Charlie | 1,253 cents ($12.53) | Received from Bob |
| **Total** | **17,500 cents ($175.00)** | **Matches network supply** |

### Balance Verification
```
Bob:     17,500 - 3,467 - 1,253 = 12,780 ✓
Alice:        0 + 3,467         =  3,467 ✓
Charlie:      0 + 1,253         =  1,253 ✓
                                 -------
Total:                            17,500 ✓
```

## Troubleshooting

### Script Fails with "Network Total DD Supply MISMATCH!"
- **Cause**: Nodes are not in sync
- **Solution**: Wait longer for sync, increase sleep times in script

### Script Fails with "DD CONSERVATION VIOLATION"
- **Cause**: Bug in DD transfer logic
- **Solution**: Check logs in `/tmp/bob_qt.log`, `/tmp/alice_qt.log`, `/tmp/charlie_qt.log`

### Qt Windows Don't Show DD Tab
- **Cause**: DigiDollar not enabled or not compiled in
- **Solution**: Ensure `-digidollar=1` flag is working and feature is compiled

### Transfer Fails
- **Cause**: Wallet doesn't have enough DD balance
- **Solution**: Check console output for Bob's balance before transfer

## Cleaning Up

### Stop Qt Windows
Press `Ctrl+C` in the terminal running the script, or:
```bash
pkill -f "digibyte-qt.*regtest"
```

### Clean Test Data
```bash
rm -rf /tmp/bob_regtest
rm -rf /tmp/alice_regtest
rm -rf /tmp/charlie_regtest
rm -rf ~/Library/Application\ Support/DigiByte/regtest
```

### View Logs
```bash
# Bob's log
tail -f /tmp/bob_qt.log

# Alice's log
tail -f /tmp/alice_qt.log

# Charlie's log
tail -f /tmp/charlie_qt.log
```

## Advanced Verification

### Check Individual Wallet via CLI

```bash
# Bob's wallet
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getdigidollarbalance

# Alice's wallet
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getdigidollarbalance

# Charlie's wallet
./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 getdigidollarbalance
```

### Check Network Stats

```bash
# Bob's view
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getdigidollarstats

# Alice's view
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getdigidollarstats

# Charlie's view
./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 getdigidollarstats
```

### Verify All Three Match

```bash
# Quick comparison script
echo "=== Bob ==="
./src/digibyte-cli -regtest -datadir=/tmp/bob_regtest -rpcport=18443 getdigidollarstats | jq '.total_dd_supply, .total_collateral_dgb'

echo "=== Alice ==="
./src/digibyte-cli -regtest -datadir=/tmp/alice_regtest -rpcport=18446 getdigidollarstats | jq '.total_dd_supply, .total_collateral_dgb'

echo "=== Charlie ==="
./src/digibyte-cli -regtest -datadir=/tmp/charlie_regtest -rpcport=18447 getdigidollarstats | jq '.total_dd_supply, .total_collateral_dgb'
```

## Notes

- The script automatically exits on any error
- All 3 nodes must agree on network stats or test fails
- DD conservation is checked at every monitoring point
- Final balances are verified against expected values
- Qt windows remain open for manual GUI verification
