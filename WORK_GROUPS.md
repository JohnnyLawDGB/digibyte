# DigiByte v8.26 Test Fix Work Groups

This file organizes the failing tests into logical work groups for systematic fixing. Each group contains related tests that share common issues and solutions.

## Overall Status (2025-08-25 - Updated from latest test run)
- **Total Tests Run**: 296
- **Passing Tests**: 241 (81.4%)
- **Failed Tests**: 55 (18.6%)
- **Skipped Tests**: 17 (not included in totals)
- **Groups**: 13 work groups with varying completion rates
- **Strategy**: Phase 1 (Groups 1-3) sequential, then parallel

## Work Group Status Legend
- 🔴 Not Started - No work begun
- 🟡 In Progress - Agent actively working  
- 🟢 Complete - All tests passing
- 🔄 Blocked - Needs intervention

## Phase Breakdown
- **Phase 1**: Groups 1-3 (MUST complete sequentially)
- **Phase 2**: Groups 4-9 (Can run parallel, max 3 agents)
- **Phase 3**: Groups 10-13 (Cleanup, can parallel)

---

## Group 1: Core Block & Mining Operations
**Status**: ⚠️ Partial - 3/5 passing (60%)
**Priority**: CRITICAL (Phase 1) - Must fix first as other tests depend on these
**Common Issues**: PoW validation (mock scrypt), block creation, subsidy calculation (72000 DGB), high-hash errors
**Agent**: Sub-Agent Group 1 (needs revisit)
**Progress**: 3/5 tests passing (60%)
```
1. feature_block.py ❌ FAILING (timeout after 999s)
2. mining_getblocktemplate_longpoll.py ✅ PASSING
3. mining_basic.py ✅ PASSING
4. mining_prioritisetransaction.py ❌ FAILING
5. feature_csv_activation.py ✅ PASSING
```

## Group 2: Consensus Rules & Validation
**Status**: ⚠️ Partial - 4/7 passing (57%)
**Priority**: CRITICAL (Phase 1) - Core consensus mechanisms
**Common Issues**: Block validation, timing (15s blocks), maturity (8 blocks vs 100 blocks), COINBASE_MATURITY_2
**Agent**: Sub-Agent Group 2 (completed with blocks)
**Progress**: 4/7 tests passing (57%)
```
1. feature_bip68_sequence.py ✅ PASSING
2. feature_cltv.py ✅ PASSING
3. feature_dersig.py ✅ PASSING
4. feature_nulldummy.py ✅ PASSING (all variants)
5. feature_backwards_compatibility.py --descriptors ❌ FAILING
6. feature_backwards_compatibility.py --legacy-wallet ❌ FAILING
7. feature_coinstatsindex.py ❌ FAILING
```

## Group 3: Fee Calculation & Estimation
**Status**: ⚠️ Partial - 4/8 passing (50%)
**Priority**: CRITICAL (Phase 1) - Many tests depend on correct fees
**Common Issues**: KvB vs vB units, fee rate calculations, max-fee-exceeded errors
**Agent**: Sub-Agent Group 3 (completed 2025-08-25)
**Progress**: 4/8 tests passing (50%)
```
1. feature_fee_estimation.py ✅ PASSING
2. feature_fee_estimator.py ✅ PASSING
3. feature_maxuploadtarget.py ✅ PASSING
4. wallet_bumpfee.py --descriptors ❌ FAILING (appears twice in test results)
5. wallet_bumpfee.py --legacy-wallet ❌ FAILING (appears twice in test results)
6. wallet_fallbackfee.py --descriptors ❌ FAILING
7. wallet_fallbackfee.py --legacy-wallet ❌ FAILING
8. wallet_fee_estimation_test.py ✅ PASSING
```

## Group 4: Transaction Creation & PSBTs
**Status**: ⚠️ Partial - 2/9 passing (22%)
**Priority**: HIGH (Phase 2) - Foundation for wallet operations
**Common Issues**: UTXO selection, insufficient funds errors, min relay fee issues, Dandelion++ issues
**Agent**: Sub-Agent Group 4 (completed 2025-08-25)
**Progress**: 2/9 tests passing (22%)
```
1. rpc_psbt.py --descriptors ❌ FAILING (appears twice in test results)
2. rpc_psbt.py --legacy-wallet ❌ FAILING (appears twice in test results)
3. rpc_rawtransaction.py --descriptors ❌ FAILING
4. rpc_rawtransaction.py --legacy-wallet ✅ PASSING
5. rpc_signrawtransaction.py --descriptors ❌ FAILING
6. rpc_signrawtransaction.py --legacy-wallet ❌ FAILING
7. wallet_signrawtransactionwithwallet.py --descriptors ❌ FAILING
8. wallet_signrawtransactionwithwallet.py --legacy-wallet ✅ PASSING
```

## Group 5: Wallet Balance & Transaction Management
**Status**: ⚠️ Partial - 6/13 passing (46%)
**Priority**: HIGH (Phase 2) - Core wallet functionality
**Common Issues**: Balance calculations with DigiByte subsidy (72000), confirmations, Dandelion++ propagation
**Agent**: Previously attempted
**Progress**: 6/13 tests passing (46%)
```
1. wallet_listtransactions.py --descriptors ✅ PASSING
2. wallet_listtransactions.py --legacy-wallet ✅ PASSING
3. wallet_listreceivedby.py --descriptors ✅ PASSING
4. wallet_listreceivedby.py --legacy-wallet ✅ PASSING
5. wallet_conflicts.py --descriptors ❌ FAILING
6. wallet_conflicts.py --legacy-wallet ❌ FAILING
7. wallet_reorgsrestore.py ✅ PASSING
8. wallet_transactiontime_rescan.py --descriptors ✅ PASSING
9. wallet_transactiontime_rescan.py --legacy-wallet ❌ FAILING
10. wallet_balance.py --descriptors ❌ FAILING
11. wallet_balance.py --legacy-wallet ❌ FAILING
12. wallet_listsinceblock.py --descriptors ❌ FAILING
13. wallet_listsinceblock.py --legacy-wallet ❌ FAILING
```

## Group 6: Wallet Fund Management
**Status**: ⚠️ Partial - 1/6 passing (17%)
**Priority**: HIGH (Phase 2) - Transaction funding issues
**Common Issues**: Insufficient funds, fee calculations, Dandelion++ propagation
**Agent**: Sub-Agent Group 6 (attempted)
**Progress**: 1/6 tests passing (17%)
```
1. wallet_fundrawtransaction.py --descriptors ❌ FAILING
2. wallet_fundrawtransaction.py --legacy-wallet ❌ FAILING
3. wallet_create_tx.py --descriptors ❌ FAILING
4. wallet_create_tx.py --legacy-wallet ❌ FAILING
5. wallet_spend_unconfirmed.py ❌ FAILING
6. wallet_txn_doublespend.py --mineblock ✅ PASSING
```

## Group 7: Address Management
**Status**: ⚠️ Partial - 5/8 passing (63%)
**Priority**: MEDIUM (Phase 2) - Address types and formats
**Common Issues**: DigiByte address prefixes (dgbrt1), address generation
**Agent**: Sub-Agent Group 7 (attempted)
**Progress**: 5/8 tests passing (63%)
```
1. wallet_address_types.py --descriptors ✅ PASSING
2. wallet_address_types.py --legacy-wallet ✅ PASSING
3. wallet_watchonly.py --legacy-wallet ✅ PASSING
4. wallet_watchonly.py --usecli --legacy-wallet ✅ PASSING
5. wallet_avoid_mixing_output_types.py --descriptors ✅ PASSING
6. wallet_change_address.py --descriptors ❌ FAILING
7. wallet_change_address.py --legacy-wallet ❌ FAILING
8. rpc_addresses_deprecation.py ❌ FAILING
9. wallet_avoidreuse.py --descriptors ❌ FAILING
10. wallet_avoidreuse.py --legacy-wallet ❌ FAILING
```

## Group 8: Wallet Infrastructure
**Status**: ⚠️ Partial - Many passing but key tests failing
**Priority**: MEDIUM (Phase 2) - Wallet management and persistence
**Common Issues**: Wallet paths, backup/restore, keypool, import/export
**Agent**: Sub-Agent Group 8 (attempted)
**Progress**: Mixed results
```
PASSING:
- wallet_backup.py --descriptors ✅
- wallet_backup.py --legacy-wallet ✅
- wallet_multiwallet.py --descriptors ✅
- wallet_multiwallet.py --legacy-wallet ✅
- wallet_multiwallet.py --usecli ✅
- wallet_reindex.py --descriptors ✅
- wallet_reindex.py --legacy-wallet ✅
- wallet_hd.py --legacy-wallet ✅
- wallet_import_with_label.py --legacy-wallet ✅
- wallet_importmulti.py --legacy-wallet ✅
- wallet_importprunedfunds.py --descriptors ✅
- wallet_importprunedfunds.py --legacy-wallet ✅
- wallet_disable.py (no variant) ✅

FAILING:
- wallet_hd.py --descriptors ❌
- wallet_keypool.py --descriptors ❌
- wallet_keypool.py --legacy-wallet ❌
- wallet_keypool_topup.py --descriptors ❌
- wallet_keypool_topup.py --legacy-wallet ❌
- wallet_import_rescan.py --legacy-wallet ❌ (appears twice)
- wallet_importdescriptors.py --descriptors ❌
- wallet_rescan_unconfirmed.py --descriptors ❌
- wallet_disable.py --descriptors ❌
- wallet_disable.py --legacy-wallet ❌
```

## Group 9: Wallet Features
**Status**: ⚠️ Partial - Some passing but key features failing
**Priority**: MEDIUM (Phase 2) - Advanced wallet features
**Common Issues**: HD wallet, descriptor wallet issues, Taproot support
**Agent**: Sub-Agent Group 9 (attempted)
**Progress**: Mixed results
```
PASSING:
- wallet_descriptor.py --descriptors ✅
- wallet_migration.py ✅
- wallet_miniscript.py --descriptors ✅
- wallet_multisig_descriptor_psbt.py --descriptors ✅

FAILING:
- wallet_orphanedreward.py ❌
- wallet_taproot.py ❌ (no variant)
- wallet_taproot.py --descriptors ❌
- wallet_signer.py --descriptors ❌
- wallet_signrawtransactionwithwallet.py --descriptors ❌
- wallet_crosschain.py ❌
```

## Group 10: P2P Network Tests
**Status**: ⚠️ Mostly Complete - 2 tests still failing
**Priority**: LOW (Phase 3) - Network protocol issues
**Common Issues**: Connection drops, peer disconnections, block/tx propagation
**Agent**: Sub-Agent Group 10 (mostly complete)
**Progress**: Most p2p tests passing
```
FAILING:
- p2p_dos_header_tree.py ❌
- p2p_filter.py ❌

PASSING (many including):
- p2p_orphan_handling.py ✅
- p2p_sendheaders.py ✅
- p2p_tx_download.py ✅
- p2p_headers_sync_with_minchainwork.py ✅
- p2p_invalid_tx.py ✅
- All other p2p_* tests ✅
```

## Group 11: Interface & CLI Tests
**Status**: 🟢 Complete
**Priority**: LOW (Phase 3) - Command-line interface issues
**Common Issues**: Argument parsing, CLI flags, wallet variant support
**Agent**: Sub-Agent Group 11 (completed 2025-08-25)
**Progress**: All interface tests passing (100%)
```
1. interface_digibyte_cli.py ✅ PASSING
2. interface_digibyte_cli.py --descriptors ✅ PASSING
3. interface_digibyte_cli.py --legacy-wallet ✅ PASSING
4. All other interface_* tests ✅ PASSING
```

## Group 12: SegWit & Advanced Features
**Status**: 🟢 Complete
**Priority**: LOW (Phase 3) - Advanced features
**Common Issues**: SegWit implementation differences, Bitcoin key/address formats
**Agent**: Sub-Agent Group 12 (completed 2025-08-25)
**Progress**: All SegWit tests passing (100%)
```
1. feature_segwit.py --descriptors ✅ PASSING
2. feature_segwit.py --descriptors --v2transport ✅ PASSING
3. feature_segwit.py --legacy-wallet ✅ PASSING
4. feature_taproot.py ✅ PASSING
5. feature_taproot.py --previous-releases ✅ PASSING
```

## Group 13: Mempool Tests  
**Status**: ⚠️ Mostly Complete - 1 test failing
**Priority**: LOW (Phase 3) - Mempool management
**Common Issues**: Dandelion++, mempool persistence
**Agent**: Sub-Agent Group 13 (mostly complete)
**Progress**: Most mempool tests passing
```
FAILING:
- mempool_persist.py (no variant) ❌

PASSING:
- mempool_persist.py --descriptors ✅
- All other mempool_* tests ✅
```

## Summary of Remaining Failures

**Total Failing Tests**: 55 tests across multiple groups

**Critical Failures** (blocking other tests):
- feature_block.py (timeout after 999s)
- mining_prioritisetransaction.py
- feature_backwards_compatibility.py (both variants)
- feature_coinstatsindex.py

**Major Groups Still Needing Work**:
1. **Fee/Bumpfee Tests**: wallet_bumpfee.py, wallet_fallbackfee.py (all variants)
2. **PSBT/Transaction Tests**: rpc_psbt.py, rpc_signrawtransaction.py (all variants)
3. **Wallet Infrastructure**: keypool, import/export, rescan tests
4. **Fund Management**: wallet_fundrawtransaction.py, wallet_create_tx.py
5. **Address/Reuse**: wallet_avoidreuse.py, wallet_change_address.py

**Tests Potentially Fixed by Dandelion++ Disable**:
Many transaction propagation tests may be fixed by adding `-dandelion=0` to test setup

## Common Fix Patterns to Apply

### Pattern 1: Block Rewards
- Change: 50 BTC → 72000 DGB
- Files: All mining and balance tests

### Pattern 2: Block Maturity
- COINBASE_MATURITY = 8 (early blocks)
- COINBASE_MATURITY_2 = 100 (after hardforks)
- Check test context for which to use

### Pattern 3: Fee Calculations
- Bitcoin uses vB (virtual bytes)
- DigiByte uses KvB (kilovirtual bytes)
- Multiply fee rates by 1000

### Pattern 4: Block Time
- Change: 600 seconds → 15 seconds
- Affects timeouts and wait periods

### Pattern 5: Address Formats
- bcrt1 → dgbrt1 (regtest bech32)
- tb1 → dgbt1 (testnet bech32)

### Pattern 6: PoW Issues
- Currently using mock digibyte_scrypt
- May cause "high-hash" errors
- Real scrypt implementation needed for accurate testing

## Next Steps

1. **Install real digibyte-scrypt package** (requires python3-dev headers)
2. **Start with Group 1** - Core block operations are foundational
3. **Apply common patterns** systematically
4. **Document new patterns** in COMMON_FIXES.md
5. **Track progress** in TEST_FIX_PROGRESS.md

## Notes

- Some tests may have multiple failure modes
- Fix application bugs when found, not just test bugs
- Verify fixes work with both --descriptors and --legacy-wallet variants
- Consider Dandelion++ impacts on P2P tests