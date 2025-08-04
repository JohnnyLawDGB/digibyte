# DigiByte v8.26 Test Fix Attack List - UPDATED

## Overview
This document organizes the remaining failing tests after Groups 1-10 completion. Current status as of 2025-08-04:
- **Total Tests**: 316
- **Passed**: 176 tests (56%)
- **Failed**: 125 tests (40%) - 76 unique test files with multiple configurations
- **Skipped**: 15 tests (5%)

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

## New Attack Groups for Remaining 76 Failed Tests

### Group 11: Wallet Core Functionality (20 tests)
**Priority: HIGH** - Basic wallet operations that many other tests depend on
- wallet_address_types.py
- wallet_backup.py
- wallet_balance.py
- wallet_basic.py
- wallet_conflicts.py
- wallet_create_tx.py
- wallet_descriptor.py
- wallet_disable.py
- wallet_groups.py
- wallet_keypool.py
- wallet_keypool_topup.py
- wallet_multiwallet.py
- wallet_orphanedreward.py
- wallet_reindex.py
- wallet_send.py
- wallet_sendall.py
- wallet_spend_unconfirmed.py
- wallet_transactiontime_rescan.py
- wallet_txn_clone.py
- wallet_txn_doublespend.py

### Group 12: Wallet Fee & Transaction Handling (18 tests)
**Priority: HIGH** - Fee calculation and transaction creation issues
- wallet_avoidreuse.py
- wallet_bumpfee.py
- wallet_change_address.py
- wallet_crosschain.py
- wallet_fallbackfee.py
- wallet_fee_estimation_test.py
- wallet_fundrawtransaction.py
- wallet_listreceivedby.py
- wallet_listsinceblock.py
- wallet_listtransactions.py
- wallet_reorgsrestore.py
- wallet_rescan_unconfirmed.py
- wallet_resendwallettransactions.py
- wallet_sendmany_chain.py
- wallet_signer.py
- wallet_signrawtransactionwithwallet.py
- wallet_simulaterawtx.py
- wallet_watchonly.py

### Group 13: Wallet Import & Descriptors (8 tests)
**Priority: MEDIUM** - Import functionality and descriptor wallets
- wallet_hd.py
- wallet_implicitsegwit.py
- wallet_import_rescan.py
- wallet_importdescriptors.py
- wallet_importmulti.py
- wallet_importprunedfunds.py
- wallet_listdescriptors.py
- wallet_migration.py

### Group 14: Advanced Wallet Features (3 tests)
**Priority: MEDIUM** - Complex wallet functionality
- wallet_miniscript.py
- wallet_multisig_descriptor_psbt.py
- wallet_taproot.py

### Group 15: Core Feature Tests (9 tests)
**Priority: HIGH** - Core blockchain functionality
- feature_assumeutxo.py
- feature_assumevalid.py
- feature_backwards_compatibility.py
- feature_bip68_sequence.py
- feature_block.py
- feature_coinstatsindex.py
- feature_fee_estimator.py
- feature_maxuploadtarget.py
- feature_taproot.py

### Group 16: RPC Interface Tests (7 tests)
**Priority: MEDIUM** - RPC command functionality
- rpc_addresses_deprecation.py
- rpc_createmultisig.py
- rpc_mempool_info.py
- rpc_psbt.py
- rpc_rawtransaction.py
- rpc_signmessage.py
- rpc_signrawtransaction.py

### Group 17: P2P & Network Tests (3 tests)
**Priority: MEDIUM** - Network protocol and peer communication
- p2p_dos_header_tree.py
- p2p_orphan_handling.py
- p2p_tx_download.py

### Group 18: Mempool & Mining Tests (4 tests)
**Priority: HIGH** - Transaction pool and mining functionality
- mempool_accept_wtxid.py
- mempool_persist.py
- mempool_updatefromblock.py
- mining_basic.py

### Group 19: Interface & Tool Tests (3 tests)
**Priority: LOW** - CLI and utility tools
- interface_digibyte_cli.py
- tool_signet_miner.py
- tool_wallet.py

### Group 20: SegWit Features (1 test)
**Priority: MEDIUM** - SegWit functionality
- feature_segwit.py

## Test Category Breakdown

**Total: 76 unique test files (125 total test instances with configurations)**

- **Wallet Tests**: 49 tests (64%) - Core wallet functionality, fees, transactions
- **Feature Tests**: 9 tests (12%) - Core blockchain features and protocol
- **RPC Tests**: 7 tests (9%) - Remote procedure call interface
- **Mempool/Mining**: 4 tests (5%) - Transaction pool and mining
- **P2P Network**: 3 tests (4%) - Peer-to-peer communication
- **Tools/Interface**: 3 tests (4%) - CLI and utility tools

## Common Failure Patterns

Based on the test categories and previous fixes, the main issues are:

1. **Fee Calculation & Transaction Creation** (Groups 11-12: 38 wallet tests)
   - MIN_RELAY_TX_FEE differences (0.00001 DGB/kB vs Bitcoin's rates)
   - Fee estimation and wallet transaction creation
   - Dust threshold calculations and UTXO selection

2. **SegWit & Taproot Implementation** (Groups 14, 20: 5 tests)
   - Taproot activation parameters for DigiByte
   - SegWit address format handling (dgbrt1 vs bc1)
   - Script validation for new address types

3. **Descriptor & Import Functionality** (Group 13: 8 tests)
   - Wallet descriptor format compatibility
   - Import functionality with DigiByte addresses
   - HD wallet key derivation paths

4. **Core Protocol Features** (Group 15: 9 tests)
   - Backward compatibility with DigiByte v8.22.2
   - Block validation and assumeUTXO functionality
   - Fee estimation algorithm differences

5. **PSBT & Raw Transactions** (Group 16: 7 RPC tests)
   - Partially Signed Bitcoin Transaction format
   - Raw transaction creation and signing
   - Address validation and multisig handling

## Recommended Fix Strategy

### Phase 1: High Priority (Groups 11, 12, 15, 18)
**Target: 51 tests** - Core functionality that blocks other tests
1. **Wallet Core & Fees** (Groups 11-12) - Fix fundamental fee calculation
2. **Core Features** (Group 15) - Protocol-level functionality
3. **Mempool/Mining** (Group 18) - Transaction processing

### Phase 2: Medium Priority (Groups 13, 14, 16, 17, 20)
**Target: 22 tests** - Advanced features and interfaces
4. **Wallet Advanced** (Groups 13-14) - Import, descriptors, taproot
5. **RPC Interface** (Group 16) - Command functionality
6. **P2P Network** (Group 17) - Network communication
7. **SegWit Features** (Group 20) - SegWit protocol

### Phase 3: Low Priority (Group 19)
**Target: 3 tests** - Tools and utilities
8. **Tools & CLI** (Group 19) - Command-line interfaces

### Key Focus Areas:
- **Fee Rate Standardization**: Update all fee calculations to use DigiByte's 0.00001 DGB/kB
- **Address Format Consistency**: Ensure dgbrt1 (DigiByte SegWit) vs bc1 (Bitcoin SegWit)
- **Dandelion++ Compatibility**: Check stempool vs mempool interactions
- **Multi-Algorithm Mining**: Verify mining tests work with all 5 algorithms

## Progress Since Groups 1-10 Completion

**Significant Improvement**: From 136 failed tests to 125 failed tests (8% reduction)
- **Tests Fixed**: 11 additional tests passed
- **Pass Rate Improved**: From 54% to 56% 
- **Failure Rate Reduced**: From 43% to 40%
- **Skip Rate Improved**: From 22% to 5% (BDB compilation issues resolved)

**Key Achievements**:
- All major P2P network issues resolved (Groups 2-4 complete)
- Core RPC interface stabilized (Groups 5-6 complete) 
- Basic mempool functionality working (Group 1 complete)
- Foundation wallet tests fixed (Group 10 complete)

**Remaining Challenge**: 49 wallet tests (64% of failures) indicate systematic fee/transaction issues that need coordinated fixes rather than individual test patches.