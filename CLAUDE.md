# CLAUDE.md - AI Assistant Guide for DigiByte Development

## Overview
This file provides context and guidance for AI assistants working on the DigiByte codebase, particularly for the Bitcoin Core v26.2 merge creating DigiByte v8.26.

## Repository Structure
**Required repositories:**
- `/Users/jt/Code/digibyte` (v8.26 merged code - BUILD/TEST HERE)
- `/Users/jt/Code/digibyte/bitcoin-v26.2-for-digibyte` (Bitcoin v26.2 reference)
- `/Users/jt/Code/digibyte/digibyte-v8.22.2` (DigiByte v8.22.2 - SOURCE OF TRUTH)

## Python Functional Test Fix Strategy

### Overview
We have 320 Python functional tests (243 unique test files) that need fixing after the Bitcoin v26.2 merge. Tests are failing due to DigiByte-specific differences in constants, RPC methods, and features. Currently only 1 test passes (rpc_bind.py --ipv4).

### Prerequisites
```bash
# Install required Python module
pip install --break-system-packages digibyte-scrypt
```

### Test Execution
```bash
# Run individual test
./test/functional/test_name.py

# Run test with specific wallet type
./test/functional/test_name.py --legacy-wallet
./test/functional/test_name.py --descriptors
```

### Fix Methodology

1. **Always Compare Three Codebases:**
   - v8.26 (current - what we're fixing)
   - v8.22.2 (SOURCE OF TRUTH for DigiByte values if exists)
   - Bitcoin v26.2 (to understand what changed)

2. **Common Failure Patterns:**
   - RPC Method Not Found (-32601): Missing DigiByte-specific methods
   - Assertion Failures: Wrong constants (fees, rewards, timing)
   - Address Format Issues: Need DigiByte prefixes
   - Import Errors: Missing test framework functions

3. **Application Bug Protocol:**
   - Fix bugs in application code when discovered
   - Document thoroughly
   - Report using standardized format

### Critical DigiByte Test Constants
```python
# Network
P2P_PORT = 12024  # Mainnet
P2P_PORT_TESTNET = 12025

# Timing
BLOCK_TIME = 15  # seconds
COINBASE_MATURITY = 100

# Fees
MIN_RELAY_FEE = Decimal('0.00001000')  # DGB/kB

# Supply
MAX_MONEY = 21000000000  # 21 billion DGB

# Current block reward
SUBSIDY = 72000  # DGB

# Address prefixes (testnet)
ADDRESS_BCTEST_UNSPENDABLE = 'swzkfmbaZb4KARFXeNvtECxhggYJnho4ud'
```

### Application Bug Reporting Format
```markdown
## APPLICATION BUG FIXED
**File**: src/[filename].cpp:XXX
**Test**: [test_name.py]::[function_name]
**Issue**: [description]
**Root Cause**: [Bitcoin v26.2 merge impact]
**Fix Applied**:
```python
# Code fix here
```
**Impact**: [consequence if unfixed]
**Testing**: [how verified]
```

## Python Functional Test Categorization for Parallel AI Assignment

**Total Test Files**: 320 tests (with variants) across 243 unique files
**Assignment Strategy**: Each AI agent gets a specific category to avoid conflicts

### Category 1: P2P Network Tests (55 failing tests)
**AI Agent 1 Assignment - Network Protocol & Dandelion++**
- `p2p_dandelion.py` - Dandelion++ privacy (RPC method missing)
- `p2p_segwit.py` - SegWit P2P (import error)
- `p2p_compactblocks.py` - Compact blocks protocol
- `p2p_timeouts.py` - Network timeouts
- `p2p_tx_download.py` - Transaction download
- `p2p_add_connections.py` - Connection management
- `p2p_addr_relay.py` - Address relay
- `p2p_addrfetch.py` - Address fetching
- `p2p_addrv2_relay.py` - AddrV2 protocol
- `p2p_block_sync.py` - Block synchronization
- `p2p_block_sync.py --v2transport` - V2 transport
- `p2p_blockfilters.py` - BIP157 filters
- `p2p_blocksonly.py` - Blocks-only mode
- `p2p_compactblocks_hb.py` - High bandwidth mode
- `p2p_compactblocks_hb.py --v2transport` - V2 transport
- `p2p_disconnect_ban.py` - Disconnect/ban behavior
- `p2p_disconnect_ban.py --v2transport` - V2 transport
- `p2p_dns_seeds.py` - DNS seed behavior
- `p2p_dos_header_tree.py` - DoS protection
- `p2p_eviction.py` - Peer eviction
- `p2p_feefilter.py` - Fee filter
- `p2p_filter.py` - Bloom filters
- `p2p_fingerprint.py` - Node fingerprinting
- `p2p_getaddr_caching.py` - GetAddr caching
- `p2p_getdata.py` - GetData handling
- `p2p_headers_sync_with_minchainwork.py` - Headers sync
- `p2p_i2p_ports.py` - I2P port handling
- `p2p_i2p_sessions.py` - I2P sessions
- `p2p_ibd_stalling.py` - IBD stalling
- `p2p_ibd_stalling.py --v2transport` - V2 transport
- `p2p_ibd_txrelay.py` - IBD tx relay
- `p2p_initial_headers_sync.py` - Initial headers
- `p2p_invalid_block.py` - Invalid block handling
- `p2p_invalid_block.py --v2transport` - V2 transport
- `p2p_invalid_locator.py` - Invalid locators
- `p2p_invalid_messages.py` - Invalid messages
- `p2p_invalid_tx.py` - Invalid transactions
- `p2p_invalid_tx.py --v2transport` - V2 transport
- `p2p_leak.py` - Memory leaks
- `p2p_leak_tx.py` - Transaction leaks
- `p2p_leak_tx.py --v2transport` - V2 transport
- `p2p_message_capture.py` - Message capture
- `p2p_mutated_blocks.py` - Mutated blocks
- `p2p_net_deadlock.py` - Network deadlocks
- `p2p_net_deadlock.py --v2transport` - V2 transport
- `p2p_nobloomfilter_messages.py` - No bloom filter
- `p2p_node_network_limited.py` - Limited nodes
- `p2p_orphan_handling.py` - Orphan handling
- `p2p_permissions.py` - P2P permissions
- `p2p_ping.py` - Ping/pong
- `p2p_sendheaders.py` - SendHeaders
- `p2p_sendtxrcncl.py` - TX reconciliation
- `p2p_tx_privacy.py` - Transaction privacy
- `p2p_unrequested_blocks.py` - Unrequested blocks
- `p2p_v2_transport.py` - V2 transport protocol

### Category 2: Wallet Tests (114 failing tests) - LARGEST
**AI Agent 2 Assignment - Wallet Functionality**
Split into 2A and 2B due to size:

#### Subcategory 2A: Basic Wallet Tests (57 tests)
- `wallet_miniscript.py --descriptors` - Miniscript support
- `wallet_hd.py --descriptors` - HD derivation paths
- `wallet_backup.py --descriptors` - Backup/restore
- `wallet_basic.py --legacy-wallet` - Basic operations
- `wallet_basic.py --descriptors` - Basic operations
- `wallet_abandonconflict.py --legacy-wallet` - Abandon conflicts
- `wallet_abandonconflict.py --descriptors` - Abandon conflicts
- `wallet_address_types.py --legacy-wallet` - Address types
- `wallet_address_types.py --descriptors` - Address types
- `wallet_avoid_mixing_output_types.py --descriptors` - Output mixing
- `wallet_avoidreuse.py --legacy-wallet` - Avoid reuse
- `wallet_avoidreuse.py --descriptors` - Avoid reuse
- `wallet_backup.py --legacy-wallet` - Backup/restore
- `wallet_backwards_compatibility.py --legacy-wallet` - Compatibility
- `wallet_backwards_compatibility.py --descriptors` - Compatibility
- `wallet_balance.py --legacy-wallet` - Balance calculation
- `wallet_balance.py --descriptors` - Balance calculation
- `wallet_blank.py --legacy-wallet` - Blank wallet
- `wallet_blank.py --descriptors` - Blank wallet
- `wallet_bumpfee.py --legacy-wallet` - Fee bumping (x2 entries)
- `wallet_bumpfee.py --descriptors` - Fee bumping (x2 entries)
- `wallet_change_address.py --legacy-wallet` - Change addresses
- `wallet_change_address.py --descriptors` - Change addresses
- `wallet_coinbase_category.py --legacy-wallet` - Coinbase category
- `wallet_coinbase_category.py --descriptors` - Coinbase category
- `wallet_conflicts.py --legacy-wallet` - Conflict handling
- `wallet_conflicts.py --descriptors` - Conflict handling
- `wallet_create_tx.py --legacy-wallet` - Transaction creation
- `wallet_create_tx.py --descriptors` - Transaction creation
- `wallet_createwallet.py --legacy-wallet` - Wallet creation
- `wallet_createwallet.py --descriptors` - Wallet creation
- `wallet_createwallet.py --usecli` - CLI wallet creation
- `wallet_crosschain.py` - Cross-chain detection
- `wallet_descriptor.py --descriptors` - Descriptor wallets
- `wallet_disable.py` - Disable wallet
- `wallet_disable.py --legacy-wallet` - Disable wallet
- `wallet_disable.py --descriptors` - Disable wallet
- `wallet_dump.py --legacy-wallet` - Wallet dump
- `wallet_encryption.py --legacy-wallet` - Encryption
- `wallet_encryption.py --descriptors` - Encryption
- `wallet_fallbackfee.py --legacy-wallet` - Fallback fee
- `wallet_fallbackfee.py --descriptors` - Fallback fee
- `wallet_fast_rescan.py --descriptors` - Fast rescan
- `wallet_fee_estimation_test.py` - Fee estimation
- `wallet_fundrawtransaction.py --legacy-wallet` - Fund raw tx
- `wallet_fundrawtransaction.py --descriptors` - Fund raw tx
- `wallet_groups.py --legacy-wallet` - Coin groups
- `wallet_groups.py --descriptors` - Coin groups
- `wallet_hd.py --legacy-wallet` - HD wallets
- `wallet_implicitsegwit.py --legacy-wallet` - Implicit segwit
- `wallet_import_rescan.py --legacy-wallet` - Import rescan (x2)
- `wallet_import_with_label.py --legacy-wallet` - Import labels
- `wallet_importdescriptors.py --descriptors` - Import descriptors
- `wallet_importmulti.py --legacy-wallet` - Import multi
- `wallet_importprunedfunds.py --legacy-wallet` - Import pruned
- `wallet_importprunedfunds.py --descriptors` - Import pruned

#### Subcategory 2B: Advanced Wallet Tests (57 tests)
- `wallet_inactive_hdchains.py --legacy-wallet` - Inactive HD chains
- `wallet_keypool.py --legacy-wallet` - Key pool
- `wallet_keypool.py --descriptors` - Key pool
- `wallet_keypool_topup.py --legacy-wallet` - Keypool topup
- `wallet_keypool_topup.py --descriptors` - Keypool topup
- `wallet_labels.py --legacy-wallet` - Label management
- `wallet_labels.py --descriptors` - Label management
- `wallet_listdescriptors.py --descriptors` - List descriptors
- `wallet_listreceivedby.py --legacy-wallet` - List received
- `wallet_listreceivedby.py --descriptors` - List received
- `wallet_listsinceblock.py --legacy-wallet` - List since block
- `wallet_listsinceblock.py --descriptors` - List since block
- `wallet_listtransactions.py --legacy-wallet` - List transactions
- `wallet_listtransactions.py --descriptors` - List transactions
- `wallet_migration.py` - Wallet migration
- `wallet_multisig_descriptor_psbt.py --descriptors` - Multisig PSBT
- `wallet_multiwallet.py --legacy-wallet` - Multi-wallet
- `wallet_multiwallet.py --descriptors` - Multi-wallet
- `wallet_multiwallet.py --usecli` - Multi-wallet CLI
- `wallet_orphanedreward.py` - Orphaned rewards
- `wallet_pruning.py --legacy-wallet` - Wallet pruning
- `wallet_reindex.py --legacy-wallet` - Reindex
- `wallet_reindex.py --descriptors` - Reindex
- `wallet_reorgsrestore.py` - Reorg restore
- `wallet_rescan_unconfirmed.py --descriptors` - Rescan unconfirmed
- `wallet_resendwallettransactions.py --legacy-wallet` - Resend txs
- `wallet_resendwallettransactions.py --descriptors` - Resend txs
- `wallet_send.py --legacy-wallet` - Send command
- `wallet_send.py --descriptors` - Send command
- `wallet_sendall.py --legacy-wallet` - Send all
- `wallet_sendall.py --descriptors` - Send all
- `wallet_sendmany_chain.py --legacy-wallet` - Send many
- `wallet_sendmany_chain.py --descriptors` - Send many
- `wallet_signer.py --descriptors` - External signer
- `wallet_signmessagewithaddress.py` - Sign messages
- `wallet_signrawtransactionwithwallet.py --legacy-wallet` - Sign raw tx
- `wallet_signrawtransactionwithwallet.py --descriptors` - Sign raw tx
- `wallet_simulaterawtx.py --legacy-wallet` - Simulate raw tx
- `wallet_simulaterawtx.py --descriptors` - Simulate raw tx
- `wallet_spend_unconfirmed.py` - Spend unconfirmed
- `wallet_startup.py` - Wallet startup
- `wallet_taproot.py` - Taproot wallet
- `wallet_taproot.py --descriptors` - Taproot descriptors
- `wallet_timelock.py` - Time locks
- `wallet_transactiontime_rescan.py --legacy-wallet` - Tx time rescan
- `wallet_transactiontime_rescan.py --descriptors` - Tx time rescan
- `wallet_txn_clone.py` - Transaction cloning
- `wallet_txn_clone.py --segwit` - SegWit cloning
- `wallet_txn_clone.py --mineblock` - Clone with mining
- `wallet_txn_doublespend.py --legacy-wallet` - Double spend
- `wallet_txn_doublespend.py --descriptors` - Double spend
- `wallet_txn_doublespend.py --mineblock` - Double spend mining
- `wallet_upgradewallet.py --legacy-wallet` - Upgrade wallet
- `wallet_watchonly.py --legacy-wallet` - Watch-only
- `wallet_watchonly.py --usecli --legacy-wallet` - Watch-only CLI

### Category 3: RPC Interface Tests (51 failing tests)
**AI Agent 3 Assignment - RPC Commands**
- `rpc_signer.py` - External signer RPC
- `rpc_psbt.py --descriptors` - PSBT handling (x2 entries)
- `rpc_psbt.py --legacy-wallet` - PSBT handling (x2 entries)
- `rpc_packages.py` - Package RPC
- `rpc_getblockreward.py` - DigiByte-specific RPC
- `rpc_addresses_deprecation.py` - Address deprecation
- `rpc_bind.py --ipv6` - IPv6 binding
- `rpc_bind.py --nonloopback` - Non-loopback binding
- `rpc_blockchain.py` - Blockchain RPCs
- `rpc_blockchain.py --v2transport` - V2 transport
- `rpc_createmultisig.py --legacy-wallet` - Create multisig
- `rpc_createmultisig.py --descriptors` - Create multisig
- `rpc_decodescript.py` - Decode script
- `rpc_deprecated.py` - Deprecated RPCs
- `rpc_deriveaddresses.py` - Derive addresses
- `rpc_deriveaddresses.py --usecli` - CLI derive addresses
- `rpc_dumptxoutset.py` - Dump UTXO set
- `rpc_estimatefee.py` - Fee estimation
- `rpc_generate.py` - Generate blocks
- `rpc_generateblock.py` - Generate block
- `rpc_getblockfilter.py` - Block filters
- `rpc_getblockfrompeer.py` - Get block from peer
- `rpc_getblockstats.py` - Block statistics
- `rpc_getchaintips.py` - Chain tips
- `rpc_getdescriptorinfo.py` - Descriptor info
- `rpc_help.py` - Help system
- `rpc_invalid_address_message.py` - Invalid addresses
- `rpc_invalidateblock.py` - Invalidate block
- `rpc_mempool_info.py` - Mempool info
- `rpc_misc.py` - Miscellaneous RPCs
- `rpc_named_arguments.py` - Named arguments
- `rpc_net.py` - Network RPCs
- `rpc_preciousblock.py` - Precious block
- `rpc_rawtransaction.py --legacy-wallet` - Raw transactions
- `rpc_rawtransaction.py --descriptors` - Raw transactions
- `rpc_scanblocks.py` - Scan blocks
- `rpc_scantxoutset.py` - Scan UTXO set
- `rpc_setban.py` - Set ban
- `rpc_signmessage.py` - Sign message
- `rpc_signmessagewithprivkey.py` - Sign with privkey
- `rpc_signrawtransaction.py --legacy-wallet` - Sign raw tx
- `rpc_signrawtransaction.py --descriptors` - Sign raw tx
- `rpc_signrawtransactionwithkey.py` - Sign with key
- `rpc_txoutproof.py` - Transaction proofs
- `rpc_uptime.py` - Uptime RPC
- `rpc_users.py` - RPC users
- `rpc_validateaddress.py` - Validate address
- `rpc_whitelist.py` - RPC whitelist
- NOTE: `rpc_bind.py --ipv4` PASSES (only passing test)

### Category 4: Mining Tests (3 failing tests)
**AI Agent 4 Assignment - Multi-Algorithm Mining**
- `mining_basic.py` - Basic mining (timeout - multi-algo issue)
- `mining_getblocktemplate_longpoll.py` - Block template
- `mining_prioritisetransaction.py` - Priority transactions

### Category 5: Mempool Tests (18 failing tests)
**AI Agent 5 Assignment - Memory Pool Management**
- `mempool_updatefromblock.py` - Update from block
- `mempool_persist.py --descriptors` - Persistence
- `mempool_limit.py` - Size limits (fee issue)
- `mempool_resurrect.py` - Resurrect transactions
- `mempool_spend_coinbase.py` - Spend coinbase
- `mempool_accept.py` - Accept transactions
- `mempool_accept_wtxid.py` - Accept by wtxid
- `mempool_compatibility.py` - Compatibility
- `mempool_datacarrier.py` - Data carrier
- `mempool_dust.py` - Dust threshold
- `mempool_expiry.py` - Expiration
- `mempool_package_limits.py` - Package limits
- `mempool_package_onemore.py` - Package onemore
- `mempool_packages.py` - Package handling
- `mempool_persist.py` - Persistence
- `mempool_reorg.py` - Reorg handling
- `mempool_sigoplimit.py` - Sigop limits
- `mempool_unbroadcast.py` - Unbroadcast txs

### Category 6: Feature Tests (62 failing tests) - LARGE
**AI Agent 6 Assignment - Core Features & Consensus**
Split into 6A and 6B due to size:

#### Subcategory 6A: Basic Feature Tests (31 tests)
- `feature_fee_estimation.py` - Fee estimation (RPC missing)
- `feature_taproot.py` - Taproot activation
- `feature_block.py` - Block validation
- `feature_segwit.py --legacy-wallet` - SegWit
- `feature_segwit.py --descriptors` - SegWit
- `feature_segwit.py --descriptors --v2transport` - SegWit V2
- `feature_abortnode.py` - Abort node
- `feature_addrman.py` - Address manager
- `feature_anchors.py` - Anchor connections
- `feature_asmap.py` - AS mapping
- `feature_assumeutxo.py` - Assume UTXO
- `feature_assumevalid.py` - Assume valid
- `feature_backwards_compatibility.py --legacy-wallet` - Compatibility
- `feature_backwards_compatibility.py --descriptors` - Compatibility
- `feature_bind_extra.py` - Extra bind
- `feature_bind_port_discover.py` - Port discovery
- `feature_bind_port_externalip.py` - External IP
- `feature_bip68_sequence.py` - BIP68 sequences
- `feature_blockfilterindex_prune.py` - Filter index
- `feature_blocksdir.py` - Blocks directory
- `feature_cltv.py` - Check lock time
- `feature_coinstatsindex.py` - Coin stats index
- `feature_config_args.py` - Config arguments
- `feature_csv_activation.py` - CSV activation
- `feature_dbcrash.py` - Database crash
- `feature_dersig.py` - DER signatures
- `feature_dirsymlinks.py` - Directory symlinks
- `feature_discover.py` - Peer discovery
- `feature_fastprune.py` - Fast pruning
- `feature_fee_estimator.py` - Fee estimator
- `feature_filelock.py` - File locking

#### Subcategory 6B: Advanced Feature Tests (31 tests)
- `feature_help.py` - Help system
- `feature_includeconf.py` - Include config
- `feature_index_prune.py` - Index pruning
- `feature_init.py` - Initialization
- `feature_loadblock.py` - Load blocks
- `feature_logging.py` - Logging system
- `feature_maxtipage.py` - Max tip age
- `feature_maxuploadtarget.py` - Upload target
- `feature_minchainwork.py` - Min chain work
- `feature_notifications.py` - Notifications
- `feature_nulldummy.py` - Null dummy
- `feature_nulldummy.py --legacy-wallet` - Null dummy
- `feature_nulldummy.py --descriptors` - Null dummy
- `feature_posix_fs_permissions.py` - FS permissions
- `feature_presegwit_node_upgrade.py` - Pre-SegWit upgrade
- `feature_proxy.py` - Proxy support
- `feature_pruning.py` - Pruning
- `feature_rbf.py` - Replace by fee
- `feature_reindex.py` - Reindex
- `feature_reindex_readonly.py` - Read-only reindex
- `feature_remove_pruned_files_on_startup.py` - Remove pruned
- `feature_settings.py` - Settings
- `feature_shutdown.py` - Shutdown
- `feature_signet.py` - Signet
- `feature_startupnotify.py` - Startup notify
- `feature_taproot.py --previous_release` - Taproot compat
- `feature_txindex_compatibility.py` - Txindex compat
- `feature_uacomment.py` - UA comment
- `feature_unsupported_utxo_db.py` - UTXO DB
- `feature_utxo_set_hash.py` - UTXO set hash
- `feature_versionbits_warning.py` - Version bits

### Category 7: Interface Tests (12 failing tests)
**AI Agent 7 Assignment - External Interfaces**
- `interface_digibyte_cli.py` - CLI interface (syntax error)
- `interface_digibyte_cli.py --legacy-wallet` - CLI with wallet
- `interface_digibyte_cli.py --descriptors` - CLI descriptors
- `interface_rest.py` - REST API
- `interface_http.py` - HTTP server
- `interface_rpc.py` - RPC interface
- `interface_usdt_coinselection.py` - USDT coin selection
- `interface_usdt_mempool.py` - USDT mempool
- `interface_usdt_net.py` - USDT network
- `interface_usdt_utxocache.py` - USDT UTXO cache
- `interface_usdt_validation.py` - USDT validation
- `interface_zmq.py` - ZMQ interface (skipped - no module)

### Category 8: Tool & Misc Tests (5 failing tests)
**AI Agent 8 Assignment - Tools and Examples**
- `tool_wallet.py --descriptors` - Wallet tool
- `tool_signet_miner.py --descriptors` - Signet miner
- `tool_wallet.py --legacy-wallet` - Wallet tool legacy
- `tool_signet_miner.py --legacy-wallet` - Signet miner legacy
- `example_test.py` - Test framework example

### Critical DigiByte-Specific Considerations

Each AI agent must check for:
1. **Address formats**: s... testnet prefixes, dgbt1... bech32
2. **Supply**: 21 billion (not 21 million)
3. **Block time**: 15 seconds (not 600)
4. **Fees**: 0.00001 DGB/kB (not 0.0001)
5. **Block rewards**: 72000 DGB current reward
6. **Algorithms**: 5 mining algorithms + Odocrypt
7. **Dandelion++**: Privacy protocol implementation
8. **Custom RPC**: getblockreward and enhanced commands

### Execution Rules for Each AI Agent
1. Use `PYTHON_TEST_FIX_PROMPT_TEMPLATE.md` for detailed instructions
2. Run only your assigned test files
3. Fix one test completely before moving to next
4. Always compare with v8.22.2 for DigiByte behavior
5. Document all application bugs found and fixed
6. Test each file: `./test/functional/TEST_NAME.py`
7. Verify fix: Test should pass completely

### Test Framework Fixes Already Applied
1. Updated private keys to DigiByte testnet format
2. Fixed address generation for dgbrt1 addresses
3. Added digibyte_scrypt module requirement

### Missing DigiByte Features Checklist

While fixing tests, actively look for missing DigiByte features:
- [ ] Dandelion++ RPC methods
- [ ] Multi-algorithm mining in tests
- [ ] Custom RPC commands (getblockreward, etc.)
- [ ] DigiSpeed difficulty adjustment
- [ ] Odocrypt algorithm activation
- [ ] DigiByte-specific fee calculations
- [ ] Enhanced getmininginfo/getdifficulty

## Test Assignment Instructions

1. Use `PYTHON_TEST_FIX_PROMPT_TEMPLATE.md` to create category-specific prompts
2. Assign each AI agent ONE category (split large categories)
3. Agents work independently on their assigned test files
4. All fixes must preserve test logic - no disabling tests

## Important Reminders
- Both Bitcoin and DigiByte copyrights must be preserved
- NEVER skip failing tests without valid reason
- ALWAYS verify fixes against expected DigiByte behavior
- Document every change and bug fix thoroughly
- Fix application bugs when discovered


## DigiByte Unique Features

### Multi-Algorithm Mining
DigiByte uses 5 mining algorithms that must be preserved:
- SHA256D (ALGO_SHA256D = 0)
- Scrypt (ALGO_SCRYPT = 1)
- Groestl (ALGO_GROESTL = 2)
- Skein (ALGO_SKEIN = 3)
- Qubit (ALGO_QUBIT = 4)
- Odocrypt (ALGO_ODO = 7) - Activates at height 9,112,320

Each algorithm targets a 75-second block time (15 seconds × 5 algorithms = 75 seconds per algo).

### Critical Constants
```cpp
// Never change these values
static const int POW_TARGET_SPACING = 15; // 15 seconds
static const CAmount MAX_MONEY = 21000000000 * COIN; // 21 billion DGB
static const int MAINNET_DEFAULT_PORT = 12024;
static const int TESTNET_DEFAULT_PORT = 12025;
static const unsigned char MAINNET_MESSAGE_START[4] = {0xfa, 0xc3, 0xb6, 0xda};
```

### Custom RPC Commands
When working with RPC commands, preserve these DigiByte-specific commands:
- `getblockreward` - Returns current block reward
- Enhanced `getmininginfo` - Shows per-algorithm statistics
- Enhanced `getdifficulty` - Returns object with all algorithm difficulties
- Mining commands with `algo` parameter support

### Dandelion++ Privacy
DigiByte implements Dandelion++ for transaction privacy. Key files:
- src/dandelion.cpp
- src/dandelion.h
- src/stempool.h

Preserve all Dandelion-related code, especially:
- `NetMsgType::DANDELIONTX` message handling
- Stem pool management
- Transaction routing logic

### Difficulty Adjustment
DigiByte uses custom difficulty algorithms:
- DigiShield V1 (block 67,200)
- MultiAlgo V2 (block 145,000)
- MultiShield V3 (block 400,000)
- DigiSpeed V4 (block 1,430,000)

## Development Commands

### Testing
```bash
# Run all tests
./test/functional/test_runner.py

# Run DigiByte-specific tests
./test/functional/digibyte_*.py

# Run unit tests
./src/test/test_digibyte

# Check for linting issues
./test/lint/all-lint.sh
```

## Python Functional Test Fix Strategy

### Overview
We have 320 Python functional tests (243 unique test files) that need fixing after the Bitcoin v26.2 merge. Tests are failing due to DigiByte-specific differences in constants, RPC methods, and features.

### Prerequisites
```bash
# Install required Python module
pip install --break-system-packages digibyte-scrypt
```

### Test Execution
```bash
# Run individual test
./test/functional/test_name.py

# Run test with specific wallet type
./test/functional/test_name.py --legacy-wallet
./test/functional/test_name.py --descriptors
```

### Common Failure Patterns

1. **RPC Method Not Found (-32601)**
   - Missing DigiByte-specific RPC methods (getblockreward, etc.)
   - Dandelion++ related methods

2. **Assertion Failures**
   - Block reward: 72000 DGB (not Bitcoin's values)
   - Fee rates: 0.00001 DGB/kB (not 0.0001)
   - Block time: 15 seconds (not 600)
   - Supply: 21 billion (not 21 million)

3. **Address Format Issues**
   - Testnet: s... prefixes (not m/n)
   - Bech32: dgbt1... (not tb1)
   - Mainnet: D.../S.../dgb1...

4. **Import Errors**
   - Missing or renamed test framework functions

### Python Functional Test Categorization

**Total Test Files**: 243 unique files (320 total with variants)
**Assignment Strategy**: Each AI agent gets specific test categories

#### Category 1: P2P Network Tests (55 tests)
**AI Agent 1 Assignment - Network Protocol & Dandelion++**
- Key tests: `p2p_dandelion.py` (DigiByte-specific)
- Common issues: Network magic, Dandelion++ implementation
- Sample failing tests:
  - `p2p_dandelion.py` - Method not found
  - `p2p_segwit.py` - Import error
  - `p2p_compactblocks.py` - Protocol differences
  - `p2p_timeouts.py` - Timing adjustments needed
  - `p2p_tx_download.py` - Transaction handling

#### Category 2: Wallet Tests (114 tests)
**AI Agent 2 Assignment - Wallet Functionality**
- Largest category with both legacy and descriptor wallets
- Common issues: Address formats, HD paths, fees
- Sample failing tests:
  - `wallet_miniscript.py` - Descriptor issues
  - `wallet_hd.py` - Derivation path differences
  - `wallet_backup.py` - Balance assertions
  - `wallet_basic.py` - Fee calculations
  - `wallet_address_types.py` - DigiByte addresses

#### Category 3: RPC Interface Tests (51 tests)
**AI Agent 3 Assignment - RPC Commands**
- Tests RPC interface and command responses
- Common issues: Custom DigiByte RPC methods
- Sample failing tests:
  - `rpc_blockchain.py` - Multi-algo support needed
  - `rpc_getblockreward.py` - DigiByte-specific
  - `rpc_packages.py` - Transaction validation
  - `rpc_psbt.py` - PSBT handling
  - `rpc_signer.py` - Signing differences

#### Category 4: Mining Tests (3 tests)
**AI Agent 4 Assignment - Multi-Algorithm Mining**
- Critical for DigiByte's 5 algorithms
- Common issues: Multi-algo, block rewards, timing
- All mining tests:
  - `mining_basic.py` - Multi-algo support
  - `mining_getblocktemplate_longpoll.py` - Template differences
  - `mining_prioritisetransaction.py` - Priority handling

#### Category 5: Mempool Tests (18 tests)
**AI Agent 5 Assignment - Memory Pool Management**
- Tests transaction pool behavior
- Common issues: Fee rates, Dandelion++ effects
- Sample failing tests:
  - `mempool_updatefromblock.py` - Block handling
  - `mempool_persist.py` - Persistence issues
  - `mempool_limit.py` - Fee rate differences
  - `mempool_resurrect.py` - Reorg handling
  - `mempool_spend_coinbase.py` - Maturity rules

#### Category 6: Feature Tests (62 tests)
**AI Agent 6 Assignment - Core Features & Consensus**
- Tests core functionality and features
- Common issues: Consensus rules, activation heights
- Sample failing tests:
  - `feature_fee_estimation.py` - Fee algorithm
  - `feature_taproot.py` - Activation differences
  - `feature_block.py` - Block validation
  - `feature_segwit.py` - SegWit rules
  - `feature_rbf.py` - RBF implementation

#### Category 7: Interface Tests (12 tests)
**AI Agent 7 Assignment - External Interfaces**
- Tests CLI, REST, ZMQ interfaces
- Common issues: Output format, custom commands
- Sample failing tests:
  - `interface_digibyte_cli.py` - CLI syntax
  - `interface_rest.py` - REST endpoints
  - `interface_zmq.py` - ZMQ notifications
  - `interface_rpc.py` - RPC interface
  - `interface_http.py` - HTTP server

#### Category 8: Tool & Misc Tests (5 tests)
**AI Agent 8 Assignment - Tools and Examples**
- Utility tools and example tests
- All tests in category:
  - `tool_wallet.py` - Wallet tool
  - `tool_signet_miner.py` - Signet mining
  - `example_test.py` - Test framework example
  - Other miscellaneous tests

### Critical DigiByte Test Constants
```python
# Network
P2P_PORT = 12024  # Mainnet
P2P_PORT_TESTNET = 12025

# Timing
BLOCK_TIME = 15  # seconds
COINBASE_MATURITY = 100

# Fees
MIN_RELAY_FEE = Decimal('0.00001000')  # DGB/kB

# Supply
MAX_MONEY = 21000000000  # 21 billion DGB

# Current block reward
SUBSIDY = 72000  # DGB

# Address prefixes (testnet)
ADDRESS_BCTEST_UNSPENDABLE = 'swzkfmbaZb4KARFXeNvtECxhggYJnho4ud'
```

### Test Framework Fixes Applied
1. Updated private keys to DigiByte testnet format
2. Fixed address generation for dgbrt1 addresses
3. Added digibyte_scrypt module requirement

### Execution Rules for Each AI Agent
1. Use `PYTHON_TEST_FIX_PROMPT_TEMPLATE.md` for detailed instructions
2. Run only assigned test categories
3. Fix tests systematically, not just assertions
4. Compare with v8.22.2 behavior when available
5. Document all application bugs found
6. Preserve test logic and coverage
