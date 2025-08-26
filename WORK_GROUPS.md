# Test Groups Organization

## Overall Status (2025-08-26)
- **Total Tests**: 278
- **Passing**: 198 (71.2%)
- **Failing**: 62 (22.3%)
- **Skipped**: 18 (6.5%)

---

## Group 1: Core Features & Consensus (10 tests)
**Status**: 🔴 Not Started
```
feature_block.py
feature_csv_activation.py  
feature_taproot.py
feature_segwit.py --legacy-wallet
feature_versionbits_warning.py
feature_coinstatsindex.py
feature_utxo_set_hash.py
rpc_blockchain.py
rpc_getblockstats.py
rpc_invalidateblock.py
```

## Group 2: Mempool Management (11 tests)
**Status**: 🔴 Not Started
```
mempool_limit.py
mempool_package_limits.py
mempool_package_onemore.py
mempool_packages.py
mempool_reorg.py
mempool_sigoplimit.py
mempool_unbroadcast.py
mempool_updatefromblock.py
feature_fee_estimation.py
feature_rbf.py
mining_prioritisetransaction.py
```

## Group 3: P2P Network & Block Propagation (10 tests)
**Status**: 🔴 Not Started
```
p2p_compactblocks.py
p2p_blocksonly.py
p2p_sendheaders.py
p2p_dos_header_tree.py
p2p_headers_sync_with_minchainwork.py
p2p_node_network_limited.py
p2p_ibd_stalling.py
p2p_ibd_txrelay.py
rpc_getblockfrompeer.py
mining_basic.py
```

## Group 4: P2P Security & Validation (9 tests)
**Status**: 🔴 Not Started
```
p2p_invalid_block.py
p2p_invalid_messages.py
p2p_leak.py
p2p_leak_tx.py
p2p_mutated_blocks.py
p2p_segwit.py
p2p_tx_download.py
rpc_psbt.py --descriptors
rpc_psbt.py --legacy-wallet
```

## Group 5: Wallet Transaction Creation (11 tests)
**Status**: 🔴 Not Started
```
wallet_bumpfee.py --descriptors
wallet_bumpfee.py --legacy-wallet
wallet_create_tx.py --descriptors
wallet_create_tx.py --legacy-wallet
wallet_fundrawtransaction.py --descriptors
wallet_fundrawtransaction.py --legacy-wallet
wallet_send.py --descriptors
wallet_send.py --legacy-wallet
wallet_sendall.py --descriptors
wallet_sendall.py --legacy-wallet
wallet_spend_unconfirmed.py
```

## Group 6: Wallet Management & Signing (11 tests)
**Status**: 🔴 Not Started
```
wallet_balance.py --descriptors
wallet_balance.py --legacy-wallet
wallet_blank.py --legacy-wallet
wallet_groups.py --descriptors
wallet_groups.py --legacy-wallet
wallet_importdescriptors.py --descriptors
wallet_orphanedreward.py
wallet_resendwallettransactions.py --descriptors
wallet_signer.py --descriptors
wallet_signrawtransactionwithwallet.py --descriptors
wallet_signrawtransactionwithwallet.py --legacy-wallet
```

## Common Fix Patterns

### Critical Constants
- Block reward: 50 BTC → 72000 DGB
- Block time: 600s → 15s
- Maturity: 100 → 8 blocks (or 100 for COINBASE_MATURITY_2)
- Fees: vB → KvB (multiply by 1000)
- Addresses: bcrt1 → dgbrt1

### Common Solutions
- Add `-dandelion=0` to disable Dandelion++
- Use `-maxtxfee=100` for high fee transactions
- Apply `maxfeerate=0` for sendrawtransaction calls

## Notes
- Always test both --descriptors and --legacy-wallet variants where applicable
- Check digibyte-v8.22.2 reference when in doubt
- Document any new fix patterns in COMMON_FIXES.md