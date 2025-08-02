# DigiByte v8.26 Python Functional Test Status

## Summary
- **Total Tests**: 315
- **Passed**: 109
- **Failed**: 136
- **Skipped**: 68 (mostly due to BDB not compiled)

## Failed Tests (136)
feature_anchors.py
feature_anchors.py --v2transport
feature_assumeutxo.py
feature_assumevalid.py
feature_bip68_sequence.py
feature_block.py
feature_blockfilterindex_prune.py
feature_cltv.py
feature_coinstatsindex.py
feature_config_args.py
feature_csv_activation.py
feature_dersig.py
feature_digiassets.py
feature_filelock.py
feature_help.py
feature_includeconf.py
feature_index_prune.py
feature_init.py
feature_loadblock.py
feature_minchainwork.py
feature_notifications.py
feature_nulldummy.py
feature_posix_fs_permissions.py
feature_presegwit_node_upgrade.py
feature_proxy.py
feature_pruning.py
feature_rbf.py
feature_reindex_readonly.py
feature_remove_pruned_files_on_startup.py
feature_settings.py
feature_shutdown.py
feature_startupnotify.py
feature_unsupported_utxo_db.py
feature_utxo_set_hash.py
feature_versionbits_warning.py
interface_rest.py
interface_usdt_utxocache.py
interface_usdt_utxocache.py --descriptors
mempool_accept.py
mempool_compatibility.py
mempool_datacarrier.py
mempool_dust.py
mempool_ephemeral_dust.py
mempool_expiry.py
mempool_limit.py
mempool_package_limits.py
mempool_package_onemore.py
mempool_package_rbf.py
mempool_packages.py
mempool_reorg.py
mempool_resurrect.py
mempool_sigoplimit.py
mempool_truc.py
mempool_unbroadcast.py
mining_basic.py
mining_prioritisetransaction.py
p2p_add_connections.py
p2p_addr_relay.py
p2p_addrfetch.py
p2p_addrv2_relay.py
p2p_bip152_sendheaders.py
p2p_blocksonly.py
p2p_disconnect_ban.py
p2p_eviction.py
p2p_feefilter.py
p2p_filter.py
p2p_filterload.py
p2p_getaddr_caching.py
p2p_getdata.py
p2p_handshake.py
p2p_handshake.py --v2transport
p2p_headers_sync_with_minchainwork.py
p2p_i2p_ports.py
p2p_i2p_sessions.py
p2p_ibd_stalling.py
p2p_ibd_txrelay.py
p2p_initial_headers_sync.py
p2p_invalid_block.py
p2p_invalid_locator.py
p2p_invalid_messages.py
p2p_invalid_tx.py
p2p_leak.py
p2p_leak_tx.py
p2p_message_capture.py
p2p_mutated_blocks.py
p2p_net_deadlock.py
p2p_node_network_limited.py
p2p_nobloomfilter_messages.py
p2p_orphan_handling.py
p2p_outbound_eviction.py
p2p_permissions.py
p2p_ping.py
p2p_seednode.py
p2p_sendtxrcncl.py
p2p_tx_download.py
p2p_tx_privacy.py
p2p_txreconciliation.py
p2p_unrequested_blocks.py
p2p_v2_misbehaving.py
p2p_v2_transport.py
rpc_blockchain.py
rpc_createmultisig.py
rpc_decodescript.py
rpc_deriveaddresses.py
rpc_deriveaddresses.py --usecli
rpc_dumptxoutset.py
rpc_estimatefee.py
rpc_generate.py
rpc_getblockfilter.py
rpc_getblockfrompeer.py
rpc_getblockstats.py
rpc_getdescriptorinfo.py
rpc_gettransaction_segwit.py
rpc_help.py
rpc_invalid_address_message.py
rpc_invalidateblock.py
rpc_mempool_entry_fee_fields.py
rpc_named_arguments.py
rpc_net.py
rpc_packages.py
rpc_preciousblock.py
rpc_rawtransaction.py
rpc_scanblocks.py
rpc_scantxoutset.py
rpc_setban.py
rpc_signmessagewithprivkey.py
rpc_signrawtransactionwithkey.py
rpc_txoutproof.py
rpc_uptime.py
rpc_users.py
rpc_validateaddress.py
wallet_abandonconflict.py --descriptors
wallet_avoid_mixing_output_types.py --descriptors
wallet_backup.py --descriptors
wallet_balance.py --descriptors
wallet_balance.py --descriptors --legacy-wallet
wallet_coinbase_category.py --descriptors
wallet_disable.py
wallet_fast_rescan.py --descriptors
wallet_fundrawtransaction.py --descriptors
wallet_gethdkeys.py --descriptors
wallet_keypool.py --descriptors
wallet_migrate.py

## Passed Tests (109)
feature_abortnode.py
feature_bind_extra.py
feature_fee_estimation.py
feature_maxtipage.py
feature_reindex.py
feature_segwit.py --descriptors
feature_segwit.py --descriptors --v2transport
feature_taproot.py
interface_digibyte_cli.py
interface_http.py
interface_rpc.py
mempool_persist.py --descriptors
mempool_spend_coinbase.py
mempool_updatefromblock.py
mining_getblocktemplate_longpoll.py
p2p_block_sync.py
p2p_block_sync.py --v2transport
p2p_compactblocks.py
p2p_dandelion.py
p2p_dns_seeds.py
p2p_segwit.py
p2p_sendheaders.py
p2p_timeouts.py
rpc_bind.py --ipv4
rpc_bind.py --ipv6
rpc_bind.py --nonloopback
rpc_getchaintips.py
rpc_misc.py
rpc_signer.py
tool_signet_miner.py --descriptors
wallet_address_types.py --descriptors
wallet_avoidreuse.py --descriptors
wallet_basic.py --descriptors
wallet_blank.py --descriptors
wallet_bumpfee.py --descriptors
wallet_change_address.py --descriptors
wallet_changetype.py --descriptors
wallet_coinbase_category.py --descriptors --legacy-wallet
wallet_conflicts.py --descriptors
wallet_create_tx.py --descriptors
wallet_createwallet.py --descriptors
wallet_createwallet.py --usecli
wallet_crosschain.py
wallet_descriptor.py --descriptors
wallet_disable.py --legacy-wallet
wallet_encryption.py --descriptors
wallet_fallbackfee.py --descriptors
wallet_groups.py --descriptors
wallet_hd.py --descriptors
wallet_import_rescan.py --descriptors
wallet_importdescriptors.py --descriptors
wallet_importmulti.py --descriptors
wallet_importprunedfunds.py --descriptors
wallet_inactive_hdchains.py --descriptors
wallet_keypool.py --descriptors --legacy-wallet
wallet_keypool_hd.py --descriptors
wallet_labels.py --descriptors
wallet_listdescriptors.py --descriptors
wallet_listreceivedby.py --descriptors
wallet_listsinceblock.py --descriptors
wallet_listtransactions.py --descriptors
wallet_migration.py
wallet_miniscript.py --descriptors
wallet_multisig_descriptor_psbt.py --descriptors
wallet_multiwallet.py --descriptors
wallet_multiwallet.py --usecli
wallet_orphanedreward.py --descriptors
wallet_reindex.py --descriptors
wallet_reorg.py --descriptors
wallet_reorgsrestore.py
wallet_rescan_unconfirmed.py --descriptors
wallet_resendwallettransactions.py --descriptors
wallet_send.py --descriptors
wallet_sendall.py --descriptors
wallet_sendmany.py --descriptors
wallet_signer.py --descriptors
wallet_signmessagewithaddress.py --descriptors
wallet_signrawtransactionwithwallet.py --descriptors
wallet_simulaterawtx.py --descriptors
wallet_spend_unconfirmed.py --descriptors
wallet_startup.py --descriptors
wallet_taproot.py --descriptors
wallet_timelock.py --descriptors
wallet_transactiontime_rescan.py --descriptors
wallet_txn_clone.py
wallet_txn_clone.py --segwit
wallet_txn_doublespend.py --descriptors
wallet_watchonly.py --descriptors
wallet_watchonly.py --usecli --descriptors

## Skipped Tests (68)
feature_maxuploadtarget.py (BDB has not been compiled.)
interface_usdt_coinselection.py (bcc python module not available)
interface_usdt_mempool.py (bcc python module not available)
interface_usdt_net.py (bcc python module not available)
interface_usdt_validation.py (bcc python module not available)
interface_zmq.py (python3-zmq module not available.)
rpc_psbt.py --legacy-wallet (BDB has not been compiled.)
tool_signet_miner.py --legacy-wallet (BDB has not been compiled.)
tool_wallet.py --legacy-wallet (BDB has not been compiled.)
wallet_abandonconflict.py --legacy-wallet (BDB has not been compiled.)
wallet_address_types.py --legacy-wallet (BDB has not been compiled.)
wallet_avoidreuse.py --legacy-wallet (BDB has not been compiled.)
wallet_backup.py --legacy-wallet (BDB has not been compiled.)
wallet_basic.py --legacy-wallet (BDB has not been compiled.)
wallet_blank.py --legacy-wallet (BDB has not been compiled.)
wallet_bumpfee.py --legacy-wallet (BDB has not been compiled.)
wallet_change_address.py --legacy-wallet (BDB has not been compiled.)
wallet_changetype.py --legacy-wallet (BDB has not been compiled.)
wallet_conflicts.py --legacy-wallet (BDB has not been compiled.)
wallet_create_tx.py --legacy-wallet (BDB has not been compiled.)
wallet_createwallet.py --legacy-wallet (BDB has not been compiled.)
wallet_dump.py --legacy-wallet (BDB has not been compiled.)
wallet_encryption.py --legacy-wallet (BDB has not been compiled.)
wallet_fallbackfee.py --legacy-wallet (BDB has not been compiled.)
wallet_fundrawtransaction.py --legacy-wallet (BDB has not been compiled.)
wallet_groups.py --legacy-wallet (BDB has not been compiled.)
wallet_hd.py --legacy-wallet (BDB has not been compiled.)
wallet_import_rescan.py --legacy-wallet (BDB has not been compiled.)
wallet_importmulti.py --legacy-wallet (BDB has not been compiled.)
wallet_importprunedfunds.py --legacy-wallet (BDB has not been compiled.)
wallet_keypool.py --legacy-wallet (BDB has not been compiled.)
wallet_keypool_hd.py --legacy-wallet (BDB has not been compiled.)
wallet_keypool_topup.py --legacy-wallet (BDB has not been compiled.)
wallet_labels.py --legacy-wallet (BDB has not been compiled.)
wallet_listreceivedby.py --legacy-wallet (BDB has not been compiled.)
wallet_listsinceblock.py --legacy-wallet (BDB has not been compiled.)
wallet_listtransactions.py --legacy-wallet (BDB has not been compiled.)
wallet_multisig_descriptor_psbt.py --legacy-wallet (BDB has not been compiled.)
wallet_multiwallet.py --legacy-wallet (BDB has not been compiled.)
wallet_orphanedreward.py --legacy-wallet (BDB has not been compiled.)
wallet_reindex.py --legacy-wallet (BDB has not been compiled.)
wallet_reorg.py --legacy-wallet (BDB has not been compiled.)
wallet_rescan_unconfirmed.py --legacy-wallet (BDB has not been compiled.)
wallet_resendwallettransactions.py --legacy-wallet (BDB has not been compiled.)
wallet_send.py --legacy-wallet (BDB has not been compiled.)
wallet_sendall.py --legacy-wallet (BDB has not been compiled.)
wallet_sendmany.py --legacy-wallet (BDB has not been compiled.)
wallet_signmessagewithaddress.py --legacy-wallet (BDB has not been compiled.)
wallet_signrawtransactionwithwallet.py --legacy-wallet (BDB has not been compiled.)
wallet_simulaterawtx.py --legacy-wallet (BDB has not been compiled.)
wallet_spend_unconfirmed.py --legacy-wallet (BDB has not been compiled.)
wallet_startup.py --legacy-wallet (BDB has not been compiled.)
wallet_taproot.py --legacy-wallet (BDB has not been compiled.)
wallet_timelock.py --legacy-wallet (BDB has not been compiled.)
wallet_transactiontime_rescan.py --legacy-wallet (BDB has not been compiled.)
wallet_txn_doublespend.py --legacy-wallet (BDB has not been compiled.)
wallet_upgradewallet.py --legacy-wallet (BDB has not been compiled.)
wallet_watchonly.py --legacy-wallet (BDB has not been compiled.)
wallet_watchonly.py --usecli --legacy-wallet (BDB has not been compiled.)

## Notes
- Tests marked as skipped are mostly due to BDB (Berkeley DB) not being compiled
- Some tests may appear multiple times with different flags (--descriptors, --legacy-wallet, --v2transport)
- The test runner timed out after 10 minutes, so a few tests at the end may not have completed