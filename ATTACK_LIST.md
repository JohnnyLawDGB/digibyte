# DigiByte v8.26 Test Fix Attack List - UPDATED

## Overview
This document organizes the remaining 76 failing tests after Groups 1-10 fixes. Current status as of 2025-08-04:
- **Total Tests**: 315
- **Passed**: 170 tests (54%)
- **Failed**: 76 tests (24%)
- **Skipped**: 69 tests (22%)

## Progress Summary
- ✅ **Group 1: Mempool Tests** - COMPLETED (fixed by previous work)
- ✅ **Group 2: P2P Network - Basic** - COMPLETED (14/15 tests fixed)
- ✅ **Group 3: P2P Network - Advanced** - COMPLETED (fixed by previous work)
- ✅ **Group 4: P2P Network - Security** - COMPLETED (fixed by previous work) 
- ✅ **Group 5: RPC Interface - Core** - COMPLETED (1 test fixed)
- ✅ **Group 6: RPC Interface - Advanced** - COMPLETED (15 tests fixed)
- ✅ **Group 7: Feature Tests - Core** - COMPLETED (7 tests fixed)
- ✅ **Group 8: Feature Tests - System** - COMPLETED (2 tests fixed)
- ✅ **Group 9: Feature & Interface Tests** - COMPLETED (1 test fixed)
- ✅ **Group 10: Wallet Tests** - COMPLETED (4 tests fixed)

## Remaining Failing Tests by Category

### Core Feature Tests (12 tests)
- feature_taproot.py (2 instances - likely --descriptors and --legacy-wallet)
- feature_assumeutxo.py
- feature_assumevalid.py
- feature_backwards_compatibility.py (2 instances)
- feature_bip68_sequence.py
- feature_block.py
- feature_coinstatsindex.py

### Wallet Tests (47 tests - majority of failures)
- wallet_address_types.py
- wallet_avoidreuse.py
- wallet_backup.py
- wallet_balance.py
- wallet_bumpfee.py (2 instances)
- wallet_change_address.py
- wallet_conflicts.py
- wallet_create_tx.py
- wallet_crosschain.py
- wallet_descriptor.py
- wallet_disable.py (2 instances)
- wallet_fallbackfee.py
- wallet_fundrawtransaction.py
- wallet_groups.py
- wallet_importdescriptors.py
- wallet_importprunedfunds.py
- wallet_keypool.py
- wallet_keypool_topup.py
- wallet_listdescriptors.py
- wallet_listreceivedby.py
- wallet_listsinceblock.py
- wallet_listtransactions.py
- wallet_miniscript.py
- wallet_multisig_descriptor_psbt.py
- wallet_multiwallet.py (2 instances)
- wallet_orphanedreward.py
- wallet_reindex.py
- wallet_reorgsrestore.py
- wallet_rescan_unconfirmed.py
- wallet_resendwallettransactions.py
- wallet_send.py
- wallet_sendall.py
- wallet_sendmany_chain.py (2 instances)
- wallet_signer.py
- wallet_signrawtransactionwithwallet.py
- wallet_simulaterawtx.py
- wallet_spend_unconfirmed.py
- wallet_taproot.py (2 instances)
- wallet_transactiontime_rescan.py
- wallet_txn_clone.py (3 instances)
- wallet_txn_doublespend.py (2 instances)

### RPC Tests (7 tests)
- rpc_psbt.py (2 instances)
- rpc_rawtransaction.py
- rpc_signrawtransaction.py (2 instances)
- rpc_addresses_deprecation.py
- rpc_mempool_info.py

### Interface Tests (2 tests)
- interface_digibyte_cli.py (2 instances)

### Mempool Tests (3 tests)
- mempool_accept_wtxid.py
- mempool_persist.py
- mempool_updatefromblock.py

### P2P Tests (3 tests)
- p2p_dos_header_tree.py
- p2p_orphan_handling.py
- p2p_tx_download.py

### Tool Tests (2 tests)
- tool_signet_miner.py
- tool_wallet.py

### Mining Tests (1 test)
- mining_basic.py

## Common Failure Patterns

Based on the test names and previous fixes, the main issues are likely:

1. **Fee Calculation Issues** (majority of wallet tests)
   - MIN_RELAY_TX_FEE differences (0.001 DGB/kB vs Bitcoin's rates)
   - Fee estimation and calculation in wallet operations
   - Dust threshold calculations

2. **Taproot Implementation** (feature_taproot.py, wallet_taproot.py)
   - May need DigiByte-specific adjustments for Taproot activation

3. **PSBT (Partially Signed Bitcoin Transactions)**
   - rpc_psbt.py and wallet PSBT tests failing
   - Likely needs DigiByte-specific transaction format handling

4. **Backward Compatibility**
   - feature_backwards_compatibility.py failures suggest version migration issues

5. **Transaction Handling**
   - Many wallet transaction tests failing (clone, doublespend, etc.)
   - Likely related to fee calculations and Dandelion++ interference

## Recommended Fix Strategy

1. **Start with Fee-Related Issues** (High Impact)
   - Fix common fee calculation issues that affect many wallet tests
   - Update MIN_RELAY_TX_FEE and dust thresholds consistently

2. **Address Taproot Tests** (Medium Complexity)
   - Ensure Taproot activation parameters are correct for DigiByte
   - Fix any DigiByte-specific script validation issues

3. **Fix PSBT Implementation** (Medium Complexity)
   - Update PSBT handling for DigiByte transaction format
   - Ensure proper fee calculation in PSBT operations

4. **Handle Remaining Edge Cases** (Low Impact)
   - Tool tests (signet_miner, wallet tool)
   - Interface tests (CLI functionality)

The majority of failures (47/76) are in wallet tests, suggesting a systematic issue with wallet fee handling or transaction creation that should be addressed first.