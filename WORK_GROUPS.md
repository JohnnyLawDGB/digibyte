# DigiByte v8.26 Test Fix Work Groups

This file organizes failing tests into logical groups for systematic fixing. Each group contains related tests sharing common issues.

## Overall Status (2025-08-25)
- **Total Tests**: 278 entries
- **Passing**: 120 (43.2%)
- **Failing**: 144 (51.8%)
- **Skipped**: 14 (5.0%)

## Work Groups Overview

Groups are organized by functionality with ~10 tests each. Work on one group at a time to avoid conflicts.

---

## Group 1: Core Block & Mining (11 tests)
**Status**: 🔴 Not Started
**Common Issues**: Block validation, PoW, subsidy (72000 DGB), block time (15s)
```
feature_block.py
mining_basic.py
mining_prioritisetransaction.py
p2p_invalid_block.py
p2p_mutated_blocks.py
feature_coinstatsindex.py
feature_utxo_set_hash.py
rpc_blockchain.py
rpc_getblockstats.py
rpc_getblockfrompeer.py
rpc_invalidateblock.py
```

## Group 2: Consensus & Activation (7 tests)
**Status**: 🔴 Not Started
**Common Issues**: Sequence locks, CSV/CLTV, version bits
```
feature_bip68_sequence.py
feature_csv_activation.py
feature_versionbits_warning.py
feature_segwit.py --descriptors
feature_segwit.py --legacy-wallet
feature_taproot.py
feature_signet.py
```

## Group 3: Fee & RBF (9 tests)
**Status**: 🔴 Not Started
**Common Issues**: KvB vs vB, fee estimation, RBF rules
```
feature_fee_estimation.py
feature_rbf.py
p2p_feefilter.py
wallet_bumpfee.py --descriptors
wallet_bumpfee.py --legacy-wallet
wallet_fallbackfee.py --descriptors
wallet_fallbackfee.py --legacy-wallet
wallet_groups.py --descriptors
wallet_groups.py --legacy-wallet
```

## Group 4: Mempool Core (15 tests)
**Status**: 🔴 Not Started
**Common Issues**: Dandelion++, mempool acceptance, package limits
```
mempool_accept.py
mempool_accept_wtxid.py
mempool_datacarrier.py
mempool_dust.py
mempool_limit.py
mempool_package_limits.py
mempool_package_onemore.py
mempool_packages.py
mempool_persist.py --descriptors
mempool_reorg.py
mempool_sigoplimit.py
mempool_spend_coinbase.py
mempool_unbroadcast.py
mempool_updatefromblock.py
rpc_mempool_info.py
```

## Group 5: P2P Network Core (12 tests)
**Status**: 🔴 Not Started
**Common Issues**: Headers sync, compact blocks, network messages
```
p2p_compactblocks.py
p2p_dos_header_tree.py
p2p_headers_sync_with_minchainwork.py
p2p_ibd_stalling.py
p2p_ibd_txrelay.py
p2p_invalid_messages.py
p2p_leak.py
p2p_node_network_limited.py
p2p_orphan_handling.py
p2p_segwit.py
p2p_sendheaders.py
p2p_tx_privacy.py
```

## Group 6: P2P Network Extra (3 tests)
**Status**: 🔴 Not Started
**Common Issues**: Peer eviction, filtering
```
p2p_eviction.py
p2p_filter.py
rpc_packages.py
```

## Group 7: RPC Transaction (11 tests)
**Status**: 🔴 Not Started
**Common Issues**: PSBT, raw transactions, signing
```
rpc_psbt.py --descriptors
rpc_psbt.py --legacy-wallet
rpc_rawtransaction.py --legacy-wallet
rpc_signrawtransactionwithkey.py
rpc_txoutproof.py
wallet_signrawtransactionwithwallet.py --descriptors
wallet_signrawtransactionwithwallet.py --legacy-wallet
wallet_simulaterawtx.py --descriptors
wallet_simulaterawtx.py --legacy-wallet
rpc_createmultisig.py
rpc_generate.py
```

## Group 8: RPC Utilities (7 tests)
**Status**: 🔴 Not Started
**Common Issues**: Address validation, script decoding, scanning
```
rpc_decodescript.py
rpc_invalid_address_message.py
rpc_validateaddress.py
rpc_signmessagewithprivkey.py
rpc_scanblocks.py
rpc_scantxoutset.py
interface_rest.py
```

## Group 9: Wallet Core (12 tests)
**Status**: 🔴 Not Started
**Common Issues**: Basic wallet operations, balance calculation
```
wallet_basic.py --descriptors
wallet_basic.py --legacy-wallet
wallet_balance.py --descriptors
wallet_balance.py --legacy-wallet
wallet_abandonconflict.py --descriptors
wallet_abandonconflict.py --legacy-wallet
wallet_conflicts.py --descriptors
wallet_conflicts.py --legacy-wallet
wallet_blank.py --legacy-wallet
wallet_disable.py
wallet_spend_unconfirmed.py
wallet_crosschain.py
```

## Group 10: Wallet Transactions (14 tests)
**Status**: 🔴 Not Started
**Common Issues**: Transaction creation, funding, sending
```
wallet_create_tx.py --descriptors
wallet_create_tx.py --legacy-wallet
wallet_fundrawtransaction.py --descriptors
wallet_fundrawtransaction.py --legacy-wallet
wallet_send.py --descriptors
wallet_send.py --legacy-wallet
wallet_sendall.py --descriptors
wallet_sendall.py --legacy-wallet
wallet_txn_clone.py
wallet_txn_clone.py --mineblock
wallet_txn_clone.py --segwit
wallet_txn_doublespend.py --descriptors
wallet_txn_doublespend.py --legacy-wallet
wallet_txn_doublespend.py --mineblock
```

## Group 11: Wallet Address (13 tests)
**Status**: 🔴 Not Started
**Common Issues**: Address types, change addresses, reuse avoidance
```
wallet_address_types.py --descriptors
wallet_address_types.py --legacy-wallet
wallet_change_address.py --descriptors
wallet_change_address.py --legacy-wallet
wallet_avoidreuse.py --descriptors
wallet_avoidreuse.py --legacy-wallet
wallet_avoid_mixing_output_types.py --descriptors
wallet_labels.py --descriptors
wallet_labels.py --legacy-wallet
wallet_watchonly.py --legacy-wallet
wallet_watchonly.py --usecli --legacy-wallet
wallet_hd.py --descriptors
wallet_hd.py --legacy-wallet
```

## Group 12: Wallet Import/Export (13 tests)
**Status**: 🔴 Not Started
**Common Issues**: Keypool, imports, descriptors
```
wallet_keypool.py --descriptors
wallet_keypool.py --legacy-wallet
wallet_keypool_topup.py --descriptors
wallet_keypool_topup.py --legacy-wallet
wallet_importdescriptors.py --descriptors
wallet_importprunedfunds.py --descriptors
wallet_importprunedfunds.py --legacy-wallet
wallet_import_rescan.py --legacy-wallet
wallet_rescan_unconfirmed.py --descriptors
wallet_backup.py --descriptors
wallet_backup.py --legacy-wallet
wallet_descriptor.py --descriptors
wallet_migration.py
```

## Group 13: Wallet Advanced (12 tests)
**Status**: 🔴 Not Started
**Common Issues**: Taproot, miniscript, signer support
```
wallet_taproot.py --descriptors
wallet_miniscript.py --descriptors
wallet_signer.py --descriptors
wallet_implicitsegwit.py --legacy-wallet
wallet_orphanedreward.py
wallet_reorgsrestore.py
wallet_transactiontime_rescan.py --legacy-wallet
wallet_resendwallettransactions.py --descriptors
wallet_resendwallettransactions.py --legacy-wallet
tool_signet_miner.py --descriptors
tool_signet_miner.py --legacy-wallet
feature_notifications.py
```

## Group 14: Wallet Lists & History (8 tests)
**Status**: ✅ Complete (All 8 passing)
**Common Issues**: Transaction history, received amounts
```
wallet_listreceivedby.py --descriptors
wallet_listreceivedby.py --legacy-wallet
wallet_listsinceblock.py --descriptors
wallet_listsinceblock.py --legacy-wallet
wallet_listtransactions.py --descriptors
wallet_listtransactions.py --legacy-wallet
interface_digibyte_cli.py --descriptors
interface_digibyte_cli.py --legacy-wallet
```

## Group 15: File & Tool Operations (5 tests)
**Status**: ✅ Complete
**Common Issues**: Tool operations, file loading
```
feature_loadblock.py
feature_reindex_readonly.py
tool_wallet.py --descriptors
tool_wallet.py --legacy-wallet
rpc_packages.py
```

## Common Fix Patterns

### Critical Constants
- Block reward: 50 BTC → 72000 DGB
- Block time: 600s → 15s
- Maturity: 100 → 8 blocks (or 100 for COINBASE_MATURITY_2)
- Fees: vB → KvB (multiply by 1000)
- Addresses: bcrt1 → dgbrt1

### Dandelion++ Issues
Many transaction propagation failures are due to Dandelion++. Consider adding `-dandelion=0` to test setup.

### PoW Validation
Currently using mock scrypt causing validation issues. Real digibyte-scrypt implementation needed.

## Work Strategy

1. **Sequential**: Complete Groups 1-3 first (foundation)
2. **Parallel**: Groups 4-15 can be worked in parallel
3. **One agent per group**: Avoid conflicts
4. **Apply patterns**: Check COMMON_FIXES.md first
5. **Document bugs**: Update APPLICATION_BUGS.md for real issues

## Notes
- Always test both --descriptors and --legacy-wallet variants
- Some tests may have multiple failure causes
- Track progress in TEST_FIX_PROGRESS.md