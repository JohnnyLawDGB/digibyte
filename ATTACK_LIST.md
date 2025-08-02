# DigiByte v8.26 Test Fix Attack List

## Overview
This document organizes the 136 failing tests into logical groups for systematic fixing. Each group contains related tests that likely share similar issues.

## Group 1: Mempool Tests (15 tests)
Focus: Transaction pool behavior, fee calculations, and Dandelion++ integration
- mempool_accept.py
- mempool_compatibility.py
- mempool_datacarrier.py
- mempool_dust.py
- mempool_ephemeral_dust.py
- mempool_expiry.py
- mempool_limit.py
- mempool_package_limits.py
- mempool_package_onemore.py
- mempool_package_rbf.py
- mempool_packages.py
- mempool_reorg.py
- mempool_resurrect.py
- mempool_sigoplimit.py
- mempool_truc.py

## Group 2: P2P Network - Basic (15 tests)
Focus: Basic peer-to-peer networking, connection handling
- p2p_add_connections.py
- p2p_addr_relay.py
- p2p_addrfetch.py
- p2p_addrv2_relay.py
- p2p_blocksonly.py
- p2p_disconnect_ban.py
- p2p_eviction.py
- p2p_getaddr_caching.py
- p2p_getdata.py
- p2p_handshake.py
- p2p_handshake.py --v2transport
- p2p_i2p_ports.py
- p2p_i2p_sessions.py
- p2p_net_deadlock.py
- p2p_nobloomfilter_messages.py

## Group 3: P2P Network - Advanced (15 tests)
Focus: Block sync, transaction handling, protocol features
- p2p_bip152_sendheaders.py
- p2p_feefilter.py
- p2p_filter.py
- p2p_filterload.py
- p2p_headers_sync_with_minchainwork.py
- p2p_ibd_stalling.py
- p2p_ibd_txrelay.py
- p2p_initial_headers_sync.py
- p2p_invalid_block.py
- p2p_invalid_locator.py
- p2p_invalid_messages.py
- p2p_invalid_tx.py
- p2p_leak.py
- p2p_leak_tx.py
- p2p_message_capture.py

## Group 4: P2P Network - Security & Special (14 tests)
Focus: Security features, orphan handling, special protocols
- p2p_mutated_blocks.py
- p2p_node_network_limited.py
- p2p_orphan_handling.py
- p2p_outbound_eviction.py
- p2p_permissions.py
- p2p_ping.py
- p2p_seednode.py
- p2p_sendtxrcncl.py
- p2p_tx_download.py
- p2p_tx_privacy.py
- p2p_txreconciliation.py
- p2p_unrequested_blocks.py
- p2p_v2_misbehaving.py
- p2p_v2_transport.py

## Group 5: RPC Interface - Core (15 tests)
Focus: Basic RPC functionality, blockchain queries
- rpc_blockchain.py
- rpc_createmultisig.py
- rpc_decodescript.py
- rpc_deriveaddresses.py
- rpc_deriveaddresses.py --usecli
- rpc_dumptxoutset.py
- rpc_estimatefee.py
- rpc_generate.py
- rpc_getblockfilter.py
- rpc_getblockfrompeer.py
- rpc_getblockstats.py
- rpc_getdescriptorinfo.py
- rpc_gettransaction_segwit.py
- rpc_help.py
- rpc_invalid_address_message.py

## Group 6: RPC Interface - Advanced (15 tests)
Focus: Transaction handling, network commands, validation
- rpc_invalidateblock.py
- rpc_mempool_entry_fee_fields.py
- rpc_named_arguments.py
- rpc_net.py
- rpc_packages.py
- rpc_preciousblock.py
- rpc_rawtransaction.py
- rpc_scanblocks.py
- rpc_scantxoutset.py
- rpc_setban.py
- rpc_signmessagewithprivkey.py
- rpc_signrawtransactionwithkey.py
- rpc_txoutproof.py
- rpc_uptime.py
- rpc_users.py

## Group 7: Feature Tests - Core (15 tests)
Focus: Core features, validation, chain mechanics
- feature_anchors.py
- feature_anchors.py --v2transport
- feature_assumeutxo.py
- feature_assumevalid.py
- feature_bip68_sequence.py
- feature_block.py
- feature_blockfilterindex_prune.py
- feature_cltv.py
- feature_coinstatsindex.py
- feature_config_args.py
- feature_csv_activation.py
- feature_dersig.py
- feature_digiassets.py
- feature_filelock.py
- feature_help.py

## Group 8: Feature Tests - System (15 tests)
Focus: System features, initialization, configuration
- feature_includeconf.py
- feature_index_prune.py
- feature_init.py
- feature_loadblock.py
- feature_minchainwork.py
- feature_notifications.py
- feature_nulldummy.py
- feature_posix_fs_permissions.py
- feature_presegwit_node_upgrade.py
- feature_proxy.py
- feature_pruning.py
- feature_rbf.py
- feature_reindex_readonly.py
- feature_remove_pruned_files_on_startup.py
- feature_settings.py

## Group 9: Feature & Interface Tests (12 tests)
Focus: Remaining features, REST API, special interfaces
- feature_shutdown.py
- feature_startupnotify.py
- feature_unsupported_utxo_db.py
- feature_utxo_set_hash.py
- feature_versionbits_warning.py
- interface_rest.py
- interface_usdt_utxocache.py
- interface_usdt_utxocache.py --descriptors
- mempool_unbroadcast.py
- mining_basic.py
- mining_prioritisetransaction.py
- rpc_validateaddress.py

## Group 10: Wallet Tests (10 tests)
Focus: Wallet functionality that's failing with descriptors
- wallet_abandonconflict.py --descriptors
- wallet_avoid_mixing_output_types.py --descriptors
- wallet_backup.py --descriptors
- wallet_balance.py --descriptors
- wallet_balance.py --descriptors --legacy-wallet
- wallet_coinbase_category.py --descriptors
- wallet_disable.py
- wallet_fast_rescan.py --descriptors
- wallet_fundrawtransaction.py --descriptors
- wallet_gethdkeys.py --descriptors
- wallet_keypool.py --descriptors
- wallet_migrate.py

## Fix Strategy
1. Start with Group 1 (Mempool) - These often fail due to fee calculations and Dandelion++
2. Move to Groups 5-6 (RPC) - Often simple constant/naming issues
3. Then Groups 2-4 (P2P) - May have Dandelion++ and protocol timing issues
4. Groups 7-8 (Features) - Various blockchain mechanics
5. Groups 9-10 (Remaining) - Mixed issues

## Common Issues to Check
- Fee calculations (MIN_RELAY_TX_FEE changed from 0.001 to 0.0001)
- Coinbase maturity (8 blocks for spending, 100 for some operations)
- Block timing (15 seconds vs 600)
- Address formats (dgbt prefix for testnet)
- Dandelion++ transaction routing
- Multi-algorithm mining impacts
- DigiByte-specific RPC methods