# Test Groups Organization

## Overall Status (2025-08-27)
- **Total Tests**: 278
- **Passing**: 258 (92.8%)
- **Failing**: 20 (7.2%)
- **Skipped**: 60 (not included in totals)

---

## Group 1: Core Features & Mining (5 tests)
**Status**: 🟡 In Progress
```
feature_block.py
feature_csv_activation.py
feature_segwit.py --legacy-wallet
feature_taproot.py
mining_basic.py
wallet_spend_unconfirmed.py
```
**Tests Completed from Original Group**: 
- feature_coinstatsindex.py ✓
- feature_utxo_set_hash.py ✓
- mining_getblocktemplate_longpoll.py ✓
- rpc_getblockstats.py ✓

## Group 2: P2P Network & RPC (6 tests)
**Status**: 🟡 In Progress
```
p2p_dos_header_tree.py
p2p_headers_sync_with_minchainwork.py
p2p_invalid_messages.py
p2p_node_network_limited.py
rpc_createmultisig.py
rpc_psbt.py --descriptors
rpc_psbt.py --legacy-wallet
wallet_signer.py --descriptors
```
**Tests Completed from Original Group**: 
- p2p_ibd_stalling.py ✓
- wallet_groups.py --descriptors ✓

## Group 3: Wallet Transactions & Fees (9 tests)
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
**Note**: Legacy wallet variants are skipped in test suite

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