# DigiDollar Send/Receive Implementation Tasks

## Overview
This document outlines the complete implementation plan for DigiDollar Send/Receive functionality using Test-Driven Development (TDD) methodology. The goal is to achieve 100% working, tested send/receive operations that integrate seamlessly with the existing persistence layer.

## Current State Analysis

### ✅ Already Implemented (COMPLETE)
1. **Address Generation** (Receive)
   - `getNewDigiDollarAddress()` in WalletModel ✅
   - P2TR (Taproot) address generation ✅
   - DD/TD/RD prefix encoding (Base58) ✅
   - Address book integration ✅
   - Payment request persistence ✅

2. **Address Validation**
   - Base58 format validation ✅
   - Prefix validation (DD/TD/RD) ✅
   - Length validation (42-62 chars) ✅

3. **Database Persistence**
   - DD time-lock storage (WriteDDTimeLock/ReadDDTimeLock) ✅
   - Balance tracking ✅
   - Transaction history ✅
   - Auto-load on startup ✅

4. **Backend Infrastructure**
   - `TransferTxBuilder` class ✅
   - `TxBuilderTransferParams` structure ✅
   - `RedeemTxBuilder` class ✅
   - RPC commands (getdigidollaraddress, transferdigidollar) ✅

### ⚠️ Needs Refactoring (PHASE 0)
1. **Function Naming**
   - `GetPositions()` → Rename to `GetDDTimeLocks()` ⚠️
   - `position_id` → Rename to `dd_timelock_id` ⚠️
   - `WritePosition()` → Rename to `WriteDDTimeLock()` ⚠️
   - `ReadPosition()` → Rename to `ReadDDTimeLock()` ⚠️
   - Update all tests to use correct terminology ⚠️

### ⚠️ Partially Implemented (NEEDS COMPLETION)
1. **Coin Selection**
   - `SelectDDCoins()` - Basic implementation, needs testing ⚠️
   - `SelectFeeCoins()` - Placeholder only (RED phase) ❌

2. **Transfer Logic**
   - `TransferDigiDollar()` - Commented out (RED phase) ⚠️
   - Transaction building logic exists but disabled ⚠️
   - Validation logic present ✅

3. **Qt Integration**
   - `sendDigiDollar()` in WalletModel - Basic structure ⚠️
   - Balance checking present ✅
   - Error handling partial ⚠️

### ❌ Missing (NEEDS IMPLEMENTATION)
1. **UTXO Management**
   - DD UTXO tracking/scanning ❌
   - DD UTXO value lookup ❌
   - DGB UTXO selection for fees ❌
   - Change output creation ❌

2. **Transaction Signing**
   - DD input signing ❌
   - Fee input signing ❌
   - Witness data construction ❌

3. **Transaction Broadcasting**
   - Mempool submission ❌
   - Network propagation ❌
   - Confirmation tracking ❌

4. **Balance Updates**
   - Post-send balance recalculation ❌
   - UTXO set updates ❌
   - Position status updates ❌

5. **Receive Detection**
   - Incoming DD transaction detection ❌
   - Balance updates on receive ❌
   - Notification system ❌

6. **Testing Infrastructure**
   - Unit tests for coin selection ❌
   - Functional tests for send/receive ❌
   - Integration tests with persistence ❌

## Implementation Phases (TDD Methodology)

### Phase 0: Foundation Refactoring (PREREQUISITE)
**Goal**: Rename existing functions to use correct terminology before implementing send/receive

#### Task 0.1: Rename GetPositions() to GetDDTimeLocks()
- **Test**: Update all existing tests that reference GetPositions()
- **Impl**: Rename `GetPositions()` to `GetDDTimeLocks()` in src/wallet/digidollarwallet.cpp and digidollarwallet.h
- **Verify**: All existing tests still pass with new function name
- **Files**:
  - src/wallet/digidollarwallet.cpp
  - src/wallet/digidollarwallet.h
  - src/test/digidollar_wallet_tests.cpp
  - src/test/digidollar_*.cpp (all test files using GetPositions)
- **Note**: This establishes correct terminology foundation (Time-Locked DGB backing DD, NOT "positions")

#### Task 0.2: Rename position_id to dd_timelock_id
- **Test**: Update all tests that reference position_id
- **Impl**: Rename `position_id` member variable to `dd_timelock_id` in WalletCollateralPosition struct
- **Verify**: All existing tests still pass with new variable name
- **Files**:
  - src/wallet/digidollarwallet.h (struct definition)
  - src/wallet/digidollarwallet.cpp (all usages)
  - src/wallet/walletdb.cpp (serialization)
  - All test files
- **Note**: Reinforces that these IDs reference DD time-locks, not generic "positions"

#### Task 0.3: Update Database Read/Write Functions
- **Test**: Verify database persistence still works with renamed fields
- **Impl**: Update `WritePosition()` to `WriteDDTimeLock()` and `ReadPosition()` to `ReadDDTimeLock()`
- **Verify**: Data persists and loads correctly, existing wallet.dat files still readable
- **Files**:
  - src/wallet/walletdb.h
  - src/wallet/walletdb.cpp
  - src/wallet/digidollarwallet.cpp (callers)
- **Note**: Maintains backward compatibility with existing wallet.dat format while using correct naming

### Phase 1: Coin Selection Foundation (CRITICAL PATH)
**Goal**: Implement robust UTXO selection for DD and DGB coins

#### Task 1.1: DD UTXO Tracking
- **Test**: Test DD UTXO identification from DD time-locks
- **Impl**: Implement `GetDDUTXOs()` to scan DD time-locks (Time-Locked DGB backing DD) for spendable DD outputs
- **Verify**: Unit test confirms UTXOs match persisted DD time-locks
- **Note**: Uses GetDDTimeLocks() to retrieve active time-locks (Time-Locked DGB backing DigiDollars)

#### Task 1.2: DD UTXO Value Lookup
- **Test**: Test DD amount retrieval from UTXO
- **Impl**: Implement `GetDDFromUTXO()` using DD time-lock cache
- **Verify**: Lookup returns correct DD amount for time-lock outputs
- **Note**: Cache stores Time-Locked DGB time-locks (collateral) + DD amounts

#### Task 1.3: SelectDDCoins Enhancement
- **Test**: Test greedy coin selection algorithm
- **Impl**: Enhance `SelectDDCoins()` with proper UTXO selection
- **Verify**: Selects minimum UTXOs to cover target amount

#### Task 1.4: DGB UTXO Selection
- **Test**: Test DGB coin selection for fees
- **Impl**: Implement `SelectFeeCoins()` using wallet UTXO set
- **Verify**: Selects sufficient DGB for estimated fees

#### Task 1.5: Change Calculation
- **Test**: Test DD and DGB change calculations
- **Impl**: Add change output creation logic
- **Verify**: Correct change amounts returned to sender

### Phase 2: Transaction Building (CORE LOGIC)
**Goal**: Enable complete transfer transaction construction

#### Task 2.1: Input Assembly
- **Test**: Test DD input creation from selected UTXOs
- **Impl**: Build CTxIn objects from DD UTXOs
- **Verify**: Inputs reference correct position outputs

#### Task 2.2: Output Assembly
- **Test**: Test DD output creation for recipients
- **Impl**: Create DD outputs with proper scripts and amounts
- **Verify**: Outputs have correct DD amounts and addresses

#### Task 2.3: Fee Calculation
- **Test**: Test transaction fee estimation
- **Impl**: Implement accurate fee calculation based on tx size
- **Verify**: Fees match expected size-based calculation

#### Task 2.4: Transaction Finalization
- **Test**: Test complete transaction structure
- **Impl**: Assemble inputs, outputs, witnesses
- **Verify**: Transaction passes basic validation

### Phase 3: Transaction Signing (SECURITY CRITICAL)
**Goal**: Properly sign all transaction inputs

#### Task 3.1: DD Input Signing
- **Test**: Test P2TR signature generation for DD inputs
- **Impl**: Sign DD inputs using position owner keys
- **Verify**: Signatures validate against scriptPubKey

#### Task 3.2: Fee Input Signing
- **Test**: Test DGB input signatures
- **Impl**: Sign fee inputs using wallet keys
- **Verify**: All inputs properly signed

#### Task 3.3: Witness Construction
- **Test**: Test witness data assembly
- **Impl**: Build complete witness stack for P2TR
- **Verify**: Transaction passes script verification

### Phase 4: Broadcasting & Confirmation (NETWORK)
**Goal**: Submit transactions to network and track status

#### Task 4.1: Mempool Submission
- **Test**: Test transaction submission to mempool
- **Impl**: Implement `CommitDDTransaction()`
- **Verify**: Transaction enters mempool successfully

#### Task 4.2: Network Propagation
- **Test**: Test transaction relay to peers
- **Impl**: Ensure proper inv/tx message handling
- **Verify**: Transaction propagates to connected nodes

#### Task 4.3: Confirmation Tracking
- **Test**: Test confirmation counting
- **Impl**: Track confirmations in transaction history
- **Verify**: Confirmations update on new blocks

#### Task 4.4: Reorganization Handling
- **Test**: Test reorg impact on DD transactions
- **Impl**: Handle chain reorganizations properly
- **Verify**: Transactions update correctly after reorg

### Phase 5: Balance & State Updates (DATA INTEGRITY)
**Goal**: Maintain accurate balances and UTXO state

#### Task 5.1: Post-Send Balance Update
- **Test**: Test balance reduction after send
- **Impl**: Update DD balance after successful send
- **Verify**: Balance reflects spent amount

#### Task 5.2: UTXO Set Updates
- **Test**: Test UTXO spent/created tracking
- **Impl**: Mark spent UTXOs, add new outputs
- **Verify**: UTXO set accurately reflects chain state

#### Task 5.3: Position Status Management
- **Test**: Test position status after partial spend
- **Impl**: Update position status when DD spent
- **Verify**: Positions marked correctly (active/spent/partial)

#### Task 5.4: Transaction History Recording
- **Test**: Test outgoing transaction persistence
- **Impl**: Save send transaction to database
- **Verify**: Transaction persists and loads correctly

### Phase 6: Receive Operations (INCOMING)
**Goal**: Detect and process incoming DD transactions

#### Task 6.1: Incoming Transaction Detection
- **Test**: Test detection of DD transactions to our addresses
- **Impl**: Scan blocks for transactions to wallet addresses
- **Verify**: Incoming DD transactions identified

#### Task 6.2: Balance Credit
- **Test**: Test balance increase on receive
- **Impl**: Credit DD balance for received outputs
- **Verify**: Balance increases by received amount

#### Task 6.3: UTXO Addition
- **Test**: Test new UTXO tracking
- **Impl**: Add received outputs to spendable UTXO set
- **Verify**: Received UTXOs become spendable

#### Task 6.4: Receive History
- **Test**: Test incoming transaction history
- **Impl**: Record received transactions with metadata
- **Verify**: History shows incoming transactions

### Phase 7: Qt Wallet Integration (USER INTERFACE)
**Goal**: Complete Qt send/receive UI functionality

#### Task 7.1: Send Dialog Enhancement
- **Test**: Test full send flow from UI
- **Impl**: Wire up send dialog to backend
- **Verify**: Can send DD from Qt wallet

#### Task 7.2: Transaction Confirmation
- **Test**: Test user confirmation before send
- **Impl**: Show transaction preview dialog
- **Verify**: User can review and confirm

#### Task 7.3: Error Handling & Display
- **Test**: Test error message display
- **Impl**: Show user-friendly error messages
- **Verify**: All errors properly communicated

#### Task 7.4: Success Notification
- **Test**: Test success message and txid display
- **Impl**: Show success dialog with transaction ID
- **Verify**: User sees confirmation

#### Task 7.5: Balance Refresh
- **Test**: Test UI balance updates
- **Impl**: Refresh balance display after send/receive
- **Verify**: Balance updates in real-time

#### Task 7.6: Transaction List Updates
- **Test**: Test transaction list refresh
- **Impl**: Update transaction list after send/receive
- **Verify**: New transactions appear in list

### Phase 8: Comprehensive Testing (QUALITY ASSURANCE)
**Goal**: Ensure complete test coverage for send/receive

#### Task 8.1: Unit Tests - Coin Selection
- Test suite for SelectDDCoins
- Test suite for SelectFeeCoins
- Edge cases (exact match, insufficient funds, etc.)

#### Task 8.2: Unit Tests - Transaction Building
- Test suite for TransferTxBuilder
- Test various recipient configurations
- Test change calculations

#### Task 8.3: Unit Tests - Signing
- Test suite for signature generation
- Test witness construction
- Test multi-input scenarios

#### Task 8.4: Functional Tests - Basic Send/Receive
- Test simple send between wallets
- Test receive detection
- Test balance updates

#### Task 8.5: Functional Tests - Edge Cases
- Test insufficient balance scenarios
- Test invalid addresses
- Test network failures
- Test double-spend prevention

#### Task 8.6: Functional Tests - Multi-Node
- Test send/receive across network
- Test mempool propagation
- Test confirmation tracking

#### Task 8.7: Integration Tests - Persistence
- Test send/receive with wallet restart
- Test transaction history persistence
- Test UTXO state recovery

#### Task 8.8: Stress Tests
- Test high-volume sends
- Test concurrent transactions
- Test large transaction sizes

## Critical Dependencies

### Must Be Working Before Starting
1. ✅ Database persistence (Phase 1-7 complete)
2. ✅ DD time-lock tracking (complete, needs Phase 0 refactoring)
3. ✅ Address generation (complete)
4. ✅ Address validation (complete)
5. ⚠️ Phase 0 refactoring (rename functions to correct terminology)

### External Dependencies
1. TransferTxBuilder (exists, needs integration)
2. RedeemTxBuilder (exists, needs integration)
3. Wallet UTXO interface
4. Mempool interface
5. Chain state interface

## Success Criteria

### Phase Completion Requirements
- ✅ All unit tests pass (RED → GREEN → REFACTOR)
- ✅ All functional tests pass
- ✅ No regressions in existing tests
- ✅ Code coverage ≥ 80% for new code
- ✅ Manual Qt testing successful
- ✅ Documentation updated

### Overall Success Criteria

#### Functional Requirements (MUST ALL PASS)
1. ✅ **Send DD from wallet A to wallet B**
   - Select DD UTXOs from active time-locks via GetDDTimeLocks()
   - Build valid transfer transaction with TransferTxBuilder
   - Sign with P2TR (Taproot) signatures
   - Broadcast to network successfully
   - Transaction enters mempool and gets mined

2. ✅ **Balance correctly updates on both sides**
   - Sender balance decreases by sent amount
   - Receiver balance increases by received amount
   - Balances persist to wallet.dat via WriteDDBalance()
   - Balances reload correctly after wallet restart

3. ✅ **Transaction persists and survives restart**
   - Sent transactions saved via WriteDDTransaction()
   - Received transactions saved via WriteDDTransaction()
   - Transaction history loads via LoadTransactionsFromDatabase()
   - Confirmations update correctly

4. ✅ **UTXO set remains consistent**
   - Spent DD UTXOs marked as spent
   - New DD UTXOs from received transactions tracked
   - UTXO state persists to wallet.dat
   - No double-spending possible
   - GetDDUTXOs() always returns accurate spendable set

5. ✅ **Mempool and chain state remain valid**
   - Transactions validate correctly
   - No orphan transactions
   - Reorg handling works correctly
   - Network propagation successful

6. ✅ **Qt UI provides smooth UX**
   - Send dialog works end-to-end
   - Balance displays update in real-time
   - Transaction list shows sent/received
   - Error messages are user-friendly
   - Success confirmations clear

7. ✅ **All error cases handled gracefully**
   - Insufficient balance → clear error
   - Invalid address → validation error
   - Network failure → retry/abort options
   - Wallet locked → unlock prompt
   - Fee too low → suggest higher fee

8. ✅ **Works on regtest, testnet, and mainnet**
   - Proper address prefixes (RD/TD/DD)
   - Chain-specific parameters
   - Network-specific validation

#### Technical Requirements (MUST ALL PASS)
1. ✅ **Wallet compiles without errors**
   ```bash
   make -j$(nproc) src/qt/digibyte-qt  # Must succeed
   ```

2. ✅ **All unit tests pass**
   ```bash
   ./src/test/test_digibyte --run_test=digidollar_*  # All pass
   ```

3. ✅ **All functional tests pass**
   ```bash
   ./test/functional/digidollar_transfer.py         # Pass
   ./test/functional/digidollar_wallet.py           # Pass
   ./test/functional/wallet_digidollar_*.py         # Pass (new)
   ```

4. ✅ **Code quality standards**
   - Test coverage ≥ 80%
   - No memory leaks (valgrind clean)
   - No compiler warnings
   - Follows DigiByte coding standards
   - All functions documented

5. ✅ **Integration with persistence layer**
   - Uses WalletBatch for all database operations
   - Maintains in-memory cache consistency
   - Auto-loads on wallet startup via LoadFromDatabase()
   - Atomic database updates (no partial states)

## Testing Strategy

### Test Pyramid
```
                  /\
                 /  \
                /E2E \      ← 5 comprehensive scenarios
               /------\
              /  Func  \    ← 20 functional tests
             /----------\
            / Unit Tests \  ← 50+ unit tests
           /--------------\
```

### Test Categories
1. **Unit Tests** (C++ in src/test/)
   - Coin selection algorithms
   - Transaction building logic
   - Signature generation
   - UTXO management

2. **Functional Tests** (Python in test/functional/)
   - wallet_digidollar_send.py
   - wallet_digidollar_receive.py
   - wallet_digidollar_sendreceive_persistence.py
   - wallet_digidollar_sendreceive_multinode.py

3. **Integration Tests** (Python)
   - With persistence layer
   - With mempool
   - With block validation
   - Multi-wallet scenarios

4. **Manual Tests** (Qt Wallet)
   - Send flow walkthrough
   - Receive flow walkthrough
   - Error handling verification
   - UI/UX validation

## Risk Assessment

### High Risk Areas
1. **UTXO Consistency** - Critical for preventing loss of funds
   - Mitigation: Extensive UTXO state testing
   - Mitigation: Atomic database updates

2. **Signature Security** - Invalid signatures = failed transactions
   - Mitigation: Comprehensive signing tests
   - Mitigation: Use proven cryptographic libraries

3. **Double Spend Prevention** - Must prevent spending same UTXO twice
   - Mitigation: Proper UTXO locking
   - Mitigation: Transaction conflict detection

4. **Balance Accuracy** - Must always reflect true spendable amount
   - Mitigation: Balance derivation from UTXO set only
   - Mitigation: No cached balance without validation

### Medium Risk Areas
1. Network propagation failures
2. Chain reorganization handling
3. Fee estimation accuracy
4. Qt UI state management

### Low Risk Areas
1. Address validation (already complete)
2. Database persistence (already tested)
3. Position tracking (already working)

## File Change Summary

### New Files to Create
- `test/functional/wallet_digidollar_send.py`
- `test/functional/wallet_digidollar_receive.py`
- `test/functional/wallet_digidollar_sendreceive_persistence.py`
- `test/functional/wallet_digidollar_sendreceive_multinode.py`
- `src/test/digidollar_coinselection_tests.cpp`
- `src/test/digidollar_signing_tests.cpp`

### Files to Modify
- `src/wallet/digidollarwallet.cpp` (SelectDDCoins, SelectFeeCoins, TransferDigiDollar)
- `src/wallet/digidollarwallet.h` (Add UTXO management methods)
- `src/qt/walletmodel.cpp` (Complete sendDigiDollar implementation)
- `src/qt/digidollarsendwidget.cpp` (Wire to backend)
- `src/rpc/digidollar_transactions.cpp` (Complete transferdigidollar RPC)

## Notes for Sub-Agents

### Code Style Guidelines
- Follow existing DigiByte Core coding standards
- Use TDD: Write test first (RED), implement (GREEN), refactor
- Add comprehensive logging with LogPrintf
- Handle all error cases explicitly
- Document all public methods

### Common Patterns
- UTXO selection: Greedy algorithm (smallest UTXOs first)
- Error handling: Return false + error string out parameter
- Logging: Prefix all logs with "DigiDollar: "
- Testing: Use BOOST_CHECK_* macros for C++, assert_* for Python

### Integration Points
- DigiDollarWallet ↔ WalletModel (Qt layer)
- DigiDollarWallet ↔ WalletBatch (persistence)
- DigiDollarWallet ↔ TransferTxBuilder (tx building)
- DigiDollarWallet ↔ CWallet (UTXO/mempool)

---

**Document Version**: 1.0
**Last Updated**: 2025-10-03
**Status**: Ready for Orchestrator
