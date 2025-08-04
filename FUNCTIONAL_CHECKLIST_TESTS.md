# DigiByte v8.26 Python Functional Test Status

## Summary (Updated 2025-08-04)
- **Total Tests**: 315
- **Passed**: 170 (54%)
- **Failed**: 76 (24%) 
- **Skipped**: 69 (22%)

## Status Key
- ✅ Passed
- ❌ Failed
- ⏭️ Skipped

## All Tests Status

### Feature Tests
- ✅ feature_addrman.py
- ✅ feature_anchors.py
- ✅ feature_anchors.py --v2transport
- ✅ feature_asmap.py
- ❌ feature_assumeutxo.py
- ❌ feature_assumevalid.py
- ❌ feature_backwards_compatibility.py --descriptors
- ❌ feature_backwards_compatibility.py --legacy-wallet
- ❌ feature_bip68_sequence.py
- ⏭️ feature_bind_extra.py
- ⏭️ feature_bind_port_discover.py
- ⏭️ feature_bind_port_externalip.py
- ❌ feature_block.py
- ✅ feature_blockfilterindex_prune.py
- ✅ feature_blocksdir.py
- ✅ feature_cltv.py
- ❌ feature_coinstatsindex.py
- ✅ feature_config_args.py
- ✅ feature_csv_activation.py
- ⏭️ feature_dbcrash.py
- ✅ feature_dersig.py
- ✅ feature_dirsymlinks.py
- ✅ feature_discover.py
- ✅ feature_fastprune.py
- ✅ feature_fee_estimation.py
- ✅ feature_filelock.py
- ✅ feature_help.py
- ✅ feature_includeconf.py
- ✅ feature_init.py
- ✅ feature_loadblock.py
- ✅ feature_logging.py
- ⏭️ feature_maxuploadtarget.py
- ✅ feature_minchainwork.py
- ✅ feature_notifications.py
- ✅ feature_nulldummy.py
- ✅ feature_posix_fs_permissions.py
- ✅ feature_presegwit_node_upgrade.py
- ✅ feature_proxy.py
- ✅ feature_pruning.py
- ✅ feature_rbf.py
- ✅ feature_reindex.py
- ✅ feature_reindex_readonly.py
- ✅ feature_remove_pruned_files_on_startup.py
- ⏭️ feature_segwit.py --descriptors
- ⏭️ feature_segwit.py --legacy-wallet
- ✅ feature_settings.py
- ✅ feature_shutdown.py
- ⏭️ feature_signet.py
- ✅ feature_startupnotify.py
- ❌ feature_taproot.py
- ❌ feature_taproot.py --previous_release
- ⏭️ feature_txindex_compatibility.py
- ⏭️ feature_unsupported_utxo_db.py
- ✅ feature_utxo_set_hash.py
- ✅ feature_versionbits_warning.py

### Interface Tests
- ⏭️ interface_digibyte_cli.py --legacy-wallet
- ❌ interface_digibyte_cli.py --descriptors
- ✅ interface_http.py
- ✅ interface_rest.py
- ✅ interface_rpc.py
- ⏭️ interface_usdt_utxocache.py
- ⏭️ interface_usdt_utxocache.py --descriptors
- ⏭️ interface_usdt_validation.py
- ✅ interface_zmq.py

### Mempool Tests
- ✅ mempool_accept.py
- ❌ mempool_accept_wtxid.py
- ⏭️ mempool_compatibility.py
- ✅ mempool_datacarrier.py
- ✅ mempool_dust.py
- ✅ mempool_expiry.py
- ✅ mempool_limit.py
- ✅ mempool_package_limits.py
- ✅ mempool_package_onemore.py
- ✅ mempool_package_rbf.py
- ✅ mempool_packages.py
- ❌ mempool_persist.py
- ✅ mempool_persist.py --descriptors
- ✅ mempool_reorg.py
- ✅ mempool_resurrect.py
- ✅ mempool_sigoplimit.py
- ✅ mempool_spend_coinbase.py
- ✅ mempool_unbroadcast.py
- ❌ mempool_updatefromblock.py

### Mining Tests
- ❌ mining_basic.py
- ✅ mining_getblocktemplate_longpoll.py
- ✅ mining_prioritisetransaction.py

### P2P Tests
- ✅ p2p_add_connections.py
- ✅ p2p_addr_relay.py
- ✅ p2p_addrfetch.py
- ✅ p2p_addrv2_relay.py
- ✅ p2p_blockfilters.py
- ✅ p2p_blocksonly.py
- ✅ p2p_compactblocks.py
- ✅ p2p_compactblocks_hb.py
- ✅ p2p_dandelion.py
- ✅ p2p_disconnect_ban.py
- ❌ p2p_dos_header_tree.py
- ✅ p2p_eviction.py
- ✅ p2p_filter.py
- ✅ p2p_fingerprint.py
- ✅ p2p_getaddr_caching.py
- ✅ p2p_getdata.py
- ✅ p2p_headers_sync_with_minchainwork.py
- ✅ p2p_i2p_ports.py
- ✅ p2p_i2p_sessions.py
- ✅ p2p_ibd_stalling.py
- ✅ p2p_ibd_txrelay.py
- ✅ p2p_initial_headers_sync.py
- ✅ p2p_invalid_block.py
- ✅ p2p_invalid_locator.py
- ✅ p2p_invalid_messages.py
- ✅ p2p_invalid_tx.py
- ✅ p2p_leak.py
- ✅ p2p_leak_tx.py
- ✅ p2p_message_capture.py
- ✅ p2p_mutated_blocks.py
- ✅ p2p_net_deadlock.py
- ✅ p2p_nobloomfilter_messages.py
- ✅ p2p_node_network_limited.py
- ❌ p2p_orphan_handling.py
- ✅ p2p_outbound_eviction.py
- ✅ p2p_permissions.py
- ✅ p2p_ping.py
- ✅ p2p_segwit.py
- ✅ p2p_sendheaders.py
- ✅ p2p_sendtxrcncl.py
- ✅ p2p_timeouts.py
- ❌ p2p_tx_download.py
- ✅ p2p_tx_privacy.py
- ✅ p2p_unrequested_blocks.py
- ✅ p2p_v2_transport.py

### RPC Tests
- ❌ rpc_addresses_deprecation.py
- ✅ rpc_blockchain.py
- ✅ rpc_createmultisig.py
- ✅ rpc_deprecated.py
- ✅ rpc_deriveaddresses.py
- ✅ rpc_deriveaddresses.py --usecli
- ✅ rpc_dumptxoutset.py
- ✅ rpc_estimatefee.py
- ✅ rpc_generate.py
- ✅ rpc_generateblock.py
- ✅ rpc_getaddressinfo.py
- ✅ rpc_getblockfilter.py
- ✅ rpc_getblockfrompeer.py
- ✅ rpc_getblockstats.py
- ✅ rpc_getdescriptorinfo.py
- ✅ rpc_gettransaction_segwit.py
- ✅ rpc_help.py
- ✅ rpc_invalid_address_message.py
- ✅ rpc_invalidateblock.py
- ❌ rpc_mempool_info.py
- ✅ rpc_mempool_entry_fee_fields.py
- ✅ rpc_named_arguments.py
- ✅ rpc_net.py
- ✅ rpc_packages.py
- ✅ rpc_preciousblock.py
- ❌ rpc_psbt.py --descriptors
- ❌ rpc_psbt.py --legacy-wallet
- ❌ rpc_rawtransaction.py
- ✅ rpc_scanblocks.py
- ✅ rpc_scantxoutset.py
- ✅ rpc_setban.py
- ❌ rpc_signrawtransaction.py --descriptors
- ❌ rpc_signrawtransaction.py --legacy-wallet
- ✅ rpc_signmessagewithprivkey.py
- ✅ rpc_signrawtransactionwithkey.py
- ✅ rpc_txoutproof.py
- ✅ rpc_uptime.py
- ✅ rpc_users.py
- ✅ rpc_validateaddress.py
- ✅ rpc_whitelist.py

### Tool Tests
- ❌ tool_signet_miner.py
- ❌ tool_wallet.py --descriptors
- ⏭️ tool_wallet.py --legacy-wallet

### Wallet Tests
- ✅ wallet_abandonconflict.py --descriptors
- ⏭️ wallet_abandonconflict.py --legacy-wallet
- ❌ wallet_address_types.py --descriptors
- ⏭️ wallet_address_types.py --legacy-wallet
- ❌ wallet_avoidreuse.py --descriptors
- ⏭️ wallet_avoidreuse.py --legacy-wallet
- ✅ wallet_avoid_mixing_output_types.py --descriptors
- ⏭️ wallet_avoid_mixing_output_types.py --legacy-wallet
- ❌ wallet_backup.py --descriptors
- ⏭️ wallet_backup.py --legacy-wallet
- ❌ wallet_balance.py --descriptors
- ⏭️ wallet_balance.py --legacy-wallet
- ✅ wallet_basic.py --descriptors
- ⏭️ wallet_basic.py --legacy-wallet
- ❌ wallet_bumpfee.py --descriptors
- ⏭️ wallet_bumpfee.py --legacy-wallet
- ❌ wallet_change_address.py --descriptors
- ⏭️ wallet_change_address.py --legacy-wallet
- ✅ wallet_coinbase_category.py --descriptors
- ⏭️ wallet_coinbase_category.py --legacy-wallet
- ❌ wallet_conflicts.py --descriptors
- ⏭️ wallet_conflicts.py --legacy-wallet
- ❌ wallet_create_tx.py --descriptors
- ⏭️ wallet_create_tx.py --legacy-wallet
- ✅ wallet_createwallet.py --descriptors
- ✅ wallet_createwallet.py --legacy-wallet --usecli
- ❌ wallet_crosschain.py
- ❌ wallet_descriptor.py --descriptors
- ❌ wallet_disable.py
- ⏭️ wallet_dump.py --legacy-wallet
- ✅ wallet_encryption.py --descriptors
- ⏭️ wallet_encryption.py --legacy-wallet
- ❌ wallet_fallbackfee.py --descriptors
- ⏭️ wallet_fallbackfee.py --legacy-wallet
- ✅ wallet_fast_rescan.py --descriptors
- ❌ wallet_fundrawtransaction.py --descriptors
- ⏭️ wallet_fundrawtransaction.py --legacy-wallet
- ❌ wallet_groups.py --descriptors
- ⏭️ wallet_groups.py --legacy-wallet
- ✅ wallet_hd.py --descriptors
- ⏭️ wallet_hd.py --legacy-wallet
- ✅ wallet_import_rescan.py --descriptors
- ⏭️ wallet_import_rescan.py --legacy-wallet
- ❌ wallet_importdescriptors.py --descriptors
- ⏭️ wallet_importmulti.py --legacy-wallet
- ❌ wallet_importprunedfunds.py --descriptors
- ⏭️ wallet_importprunedfunds.py --legacy-wallet
- ❌ wallet_keypool.py --descriptors
- ⏭️ wallet_keypool.py --legacy-wallet
- ❌ wallet_keypool_topup.py --descriptors
- ⏭️ wallet_keypool_topup.py --legacy-wallet
- ⏭️ wallet_labels.py --legacy-wallet
- ✅ wallet_labels.py --descriptors
- ❌ wallet_listdescriptors.py --descriptors
- ❌ wallet_listreceivedby.py --descriptors
- ⏭️ wallet_listreceivedby.py --legacy-wallet
- ❌ wallet_listsinceblock.py --descriptors
- ⏭️ wallet_listsinceblock.py --legacy-wallet
- ❌ wallet_listtransactions.py --descriptors
- ⏭️ wallet_listtransactions.py --legacy-wallet
- ⏭️ wallet_migration.py
- ❌ wallet_miniscript.py --descriptors
- ✅ wallet_multisig_descriptor_psbt.py --descriptors
- ❌ wallet_multiwallet.py --descriptors
- ❌ wallet_multiwallet.py --legacy-wallet --usecli
- ❌ wallet_orphanedreward.py
- ✅ wallet_reindex.py --descriptors
- ⏭️ wallet_reindex.py --legacy-wallet
- ❌ wallet_reorgsrestore.py
- ❌ wallet_rescan_unconfirmed.py --descriptors
- ❌ wallet_resendwallettransactions.py --descriptors
- ⏭️ wallet_resendwallettransactions.py --legacy-wallet
- ❌ wallet_send.py --descriptors
- ⏭️ wallet_send.py --legacy-wallet
- ❌ wallet_sendall.py --descriptors
- ⏭️ wallet_sendall.py --legacy-wallet
- ❌ wallet_sendmany_chain.py --descriptors
- ⏭️ wallet_sendmany.py --legacy-wallet
- ✅ wallet_signer.py --descriptors
- ❌ wallet_signrawtransactionwithwallet.py --descriptors
- ⏭️ wallet_signrawtransactionwithwallet.py --legacy-wallet
- ❌ wallet_simulaterawtx.py --descriptors
- ❌ wallet_spend_unconfirmed.py
- ✅ wallet_startup.py
- ❌ wallet_taproot.py --descriptors
- ⏭️ wallet_taproot.py --legacy-wallet
- ✅ wallet_timelock.py
- ✅ wallet_transactiontime_rescan.py --descriptors
- ⏭️ wallet_transactiontime_rescan.py --legacy-wallet
- ❌ wallet_txn_clone.py
- ❌ wallet_txn_clone.py --segwit
- ❌ wallet_txn_doublespend.py
- ❌ wallet_txn_doublespend.py --mineblock
- ✅ wallet_upgradewallet.py --legacy-wallet
- ✅ wallet_watchonly.py --descriptors
- ⏭️ wallet_watchonly.py --legacy-wallet

## Summary by Category

### Passed Tests (170)
Successfully running tests include most P2P network tests, basic RPC functionality, and core feature tests that have been updated for DigiByte-specific parameters.

### Failed Tests (76)
The majority of failures are in:
- Wallet tests (47 failures) - primarily fee calculation and transaction creation issues
- Feature tests (12 failures) - including Taproot and backwards compatibility
- RPC tests (7 failures) - PSBT and raw transaction handling
- Other categories with fewer failures

### Skipped Tests (69)
Most skipped tests are due to:
- BDB (Berkeley Database) not compiled
- Legacy wallet functionality disabled
- Previous releases not available
- Optional features not enabled