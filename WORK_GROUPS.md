# Test Groups Organization

## Overall Status (2025-08-26)
- **Total Tests**: 278
- **Passing**: 222 (79.9%)
- **Failing**: 38 (13.7%)
- **Skipped**: 18 (6.4%)

---

## Group 1: Core Features & Consensus (7 tests)
**Status**: 🔴 Not Started
```
feature_block.py
feature_csv_activation.py
feature_taproot.py
feature_versionbits_warning.py
mining_basic.py
rpc_getblockstats.py
wallet_txn_doublespend.py --mineblock
```

## Group 2: Fee & Segwit Features (6 tests)
**Status**: 🔴 Not Started
```
feature_fee_estimation.py
feature_segwit.py --legacy-wallet
mempool_package_limits.py
mempool_sigoplimit.py
wallet_orphanedreward.py
wallet_spend_unconfirmed.py
```

## Group 3: P2P Network Core (6 tests)
**Status**: 🔴 Not Started
```
p2p_compactblocks.py
p2p_dos_header_tree.py
p2p_headers_sync_with_minchainwork.py
p2p_ibd_stalling.py
p2p_invalid_messages.py
p2p_node_network_limited.py
```

## Group 4: Wallet Balance & Import (7 tests)
**Status**: 🔴 Not Started
```
wallet_balance.py --descriptors
wallet_balance.py --legacy-wallet
wallet_importdescriptors.py --descriptors
wallet_resendwallettransactions.py --descriptors
wallet_signer.py --descriptors
rpc_psbt.py --descriptors
rpc_psbt.py --legacy-wallet
```

## Group 5: Wallet Fee Management (6 tests)
**Status**: 🔴 Not Started
```
wallet_bumpfee.py --descriptors
wallet_bumpfee.py --legacy-wallet
wallet_create_tx.py --descriptors
wallet_create_tx.py --legacy-wallet
wallet_groups.py --descriptors
wallet_groups.py --legacy-wallet
```

## Group 6: Wallet Send Operations (6 tests)
**Status**: 🔴 Not Started
```
wallet_fundrawtransaction.py --descriptors
wallet_fundrawtransaction.py --legacy-wallet
wallet_send.py --descriptors
wallet_send.py --legacy-wallet
wallet_sendall.py --descriptors
wallet_sendall.py --legacy-wallet
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