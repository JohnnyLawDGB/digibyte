# Test Groups Organization

## Overall Status (2025-08-27)
- **Total Tests**: 278
- **Passing**: 229 (82.4%)
- **Failing**: 31 (11.2%)
- **Skipped**: 18 (6.5%)

---

## Group 1: Core Features & Mining (10 tests)
**Status**: 🔴 Not Started
```
feature_block.py
feature_coinstatsindex.py
feature_csv_activation.py
feature_segwit.py --legacy-wallet
feature_taproot.py
feature_utxo_set_hash.py
mining_basic.py
mining_getblocktemplate_longpoll.py
rpc_getblockstats.py
wallet_spend_unconfirmed.py
```

## Group 2: P2P Network & PSBT (9 tests)
**Status**: 🔴 Not Started
```
p2p_dos_header_tree.py
p2p_headers_sync_with_minchainwork.py
p2p_ibd_stalling.py
p2p_invalid_messages.py
p2p_node_network_limited.py
rpc_psbt.py --descriptors
rpc_psbt.py --legacy-wallet
wallet_signer.py --descriptors
wallet_groups.py --descriptors
```

## Group 3: Wallet Transactions & Fees (12 tests)
**Status**: 🔴 Not Started
```
wallet_balance.py --descriptors
wallet_balance.py --legacy-wallet
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