# DigiDollar Send/Receive Implementation Checklist

## 🎯 End Goal
**100% functional, tested DigiDollar send/receive in Qt wallet and RPC**

## ✅ Complete Implementation Path (47 Tasks)

### Phase 0: Foundation Refactoring (3 tasks)
- [ ] 0.1 - Rename GetPositions() → GetDDTimeLocks()
- [ ] 0.2 - Rename position_id → dd_timelock_id  
- [ ] 0.3 - Update database functions (WriteDDTimeLock, ReadDDTimeLock)
**Outcome**: Correct terminology throughout codebase

### Phase 1: Coin Selection Foundation (5 tasks)
- [ ] 1.1 - Implement GetDDUTXOs() for UTXO tracking
- [ ] 1.2 - Implement GetDDFromUTXO() for value lookup
- [ ] 1.3 - Enhance SelectDDCoins() with greedy algorithm
- [ ] 1.4 - Implement SelectFeeCoins() for DGB fees
- [ ] 1.5 - Implement change calculation logic
**Outcome**: Can select DD and DGB UTXOs for transactions

### Phase 2: Transaction Building (6 tasks)
- [ ] 2.1 - Enable TransferDigiDollar() function (uncomment existing code)
- [ ] 2.2 - Implement DD input assembly
- [ ] 2.3 - Implement DD output assembly
- [ ] 2.4 - Implement fee calculation
- [ ] 2.5 - Implement change output creation
- [ ] 2.6 - Finalize transaction structure
**Outcome**: Can build complete transfer transactions

### Phase 3: Transaction Signing (3 tasks)
- [ ] 3.1 - Implement P2TR signing for DD inputs
- [ ] 3.2 - Implement DGB input signing for fees
- [ ] 3.3 - Construct witness data for P2TR
**Outcome**: Transactions are properly signed

### Phase 4: Broadcasting & Confirmation (4 tasks)
- [ ] 4.1 - Implement CommitDDTransaction() for mempool submission
- [ ] 4.2 - Ensure network propagation works
- [ ] 4.3 - Implement confirmation tracking
- [ ] 4.4 - Handle chain reorganizations
**Outcome**: Transactions broadcast and confirm on network

### Phase 5: Balance & State Updates (4 tasks)
- [ ] 5.1 - Update sender balance after send
- [ ] 5.2 - Update UTXO set (mark spent, add new)
- [ ] 5.3 - Update time-lock status
- [ ] 5.4 - Record transaction history
**Outcome**: Balances and state remain accurate

### Phase 6: Receive Operations (5 tasks)
- [ ] 6.1 - Detect incoming DD transactions
- [ ] 6.2 - Credit receiver balance
- [ ] 6.3 - Add received UTXOs to spendable set
- [ ] 6.4 - Record receive history
- [ ] 6.5 - Emit wallet notification signals
**Outcome**: Receive DD and update wallet state

### Phase 7: Qt & RPC Integration (7 tasks)
- [ ] 7.1 - Wire send dialog to backend
- [ ] 7.2 - Implement transaction confirmation dialog
- [ ] 7.3 - Implement error handling & display
- [ ] 7.4 - Implement success notification
- [ ] 7.5 - Implement balance refresh in UI
- [ ] 7.6 - Implement transaction list updates
- [ ] 7.7 - Verify RPC transferdigidollar command works
**Outcome**: Qt wallet and RPC fully functional

### Phase 8: Comprehensive Testing (9 tasks)
- [ ] 8.1 - Unit tests: Coin selection
- [ ] 8.2 - Unit tests: Transaction building
- [ ] 8.3 - Unit tests: Signing
- [ ] 8.4 - Functional tests: Basic send/receive
- [ ] 8.5 - Functional tests: Edge cases
- [ ] 8.6 - Functional tests: Multi-node
- [ ] 8.7 - Integration tests: Persistence
- [ ] 8.8 - Stress tests
- [ ] 8.9 - **END-TO-END VALIDATION** (Qt wallet on regtest)
**Outcome**: All tests pass, ready for production

## 🔍 Final Validation (Task 8.9)
This is the FINAL step that proves everything works:

1. Start fresh regtest with Qt wallet
2. Mine 650+ blocks (activate DigiDollar)
3. Mint 1000 DD in Wallet A
4. Start Wallet B (second Qt instance)
5. Get DD receive address from Wallet B
6. **Send 500 DD from A → B**
7. ✅ Verify A balance: 500 DD
8. ✅ Verify B balance: 500 DD
9. **Restart both wallets**
10. ✅ Verify balances persist
11. ✅ Verify transaction history intact
12. **Send 250 DD from B → A**
13. ✅ Verify A balance: 750 DD
14. ✅ Verify B balance: 250 DD

**If all above pass: SEND/RECEIVE IS COMPLETE** ✅

## 📋 Success Criteria Summary

### Must Pass (All Required)
- ✅ Wallet compiles without errors
- ✅ All unit tests pass
- ✅ All functional tests pass
- ✅ Manual Qt test successful (Task 8.9)
- ✅ Balances persist across wallet restarts
- ✅ UTXO set remains consistent
- ✅ No memory leaks
- ✅ No double-spending possible
- ✅ Works on regtest, testnet, mainnet

### Critical Integration Points
- ✅ Uses GetDDTimeLocks() for time-lock tracking
- ✅ Uses WalletBatch for all database operations
- ✅ Maintains in-memory cache consistency
- ✅ Auto-loads on wallet startup
- ✅ DD UTXOs always at vout[1]
- ✅ Proper P2TR signing

## 📝 Documentation Complete
- [x] DIGIDOLLAR_TERMINOLOGY.md
- [x] DIGIDOLLAR_SENDRECEIVE_README.md
- [x] DIGIDOLLAR_SENDRECEIVE_INDEX.md
- [x] DIGIDOLLAR_SENDRECEIVE_SUMMARY.md
- [x] DIGIDOLLAR_SENDRECEIVE_TASKS.md (this file)
- [x] DIGIDOLLAR_SENDRECEIVE_ORCHESTRATOR.md
- [x] DIGIDOLLAR_SENDRECEIVE_SUBAGENT.md
- [x] DIGIDOLLAR_SENDRECEIVE_TDD_GUIDE.md
- [x] DIGIDOLLAR_SENDRECEIVE_EXPLAINER.md
- [x] DIGIDOLLAR_SENDRECEIVE_VERIFICATION.md

## 🚀 Ready to Start
All documentation is complete. The orchestrator can now begin deploying sub-agents to implement all 47 tasks across 9 phases.

**Start command**: Begin with Phase 0 (terminology refactoring), then proceed sequentially through all phases.
