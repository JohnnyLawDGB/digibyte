# DigiDollar Send/Receive Implementation - README

## 🎯 Mission
Implement 100% functional, tested DigiDollar send/receive functionality that:
- ✅ Sends DD from wallet A to wallet B
- ✅ Receives DD in wallet B from wallet A
- ✅ Maintains accurate balances at all times
- ✅ Persists everything to wallet.dat
- ✅ Survives wallet restarts
- ✅ Tracks UTXOs correctly
- ✅ Works in Qt wallet GUI
- ✅ Passes all tests

## 📚 Complete Documentation (11 Files)

### 1. **INDEX.md** - Start Here
Navigation guide for all documentation.
```bash
cat DIGIDOLLAR_SENDRECEIVE_INDEX.md
```

### 2. **SUMMARY.md** - Overview
Quick reference and overview of the project.
```bash
cat DIGIDOLLAR_SENDRECEIVE_SUMMARY.md
```

### 3. **IMPLEMENTATION_CHECKLIST.md** - Master Checklist ⭐ NEW
47 tasks across 9 phases with completion checkboxes.
```bash
cat DIGIDOLLAR_SENDRECEIVE_IMPLEMENTATION_CHECKLIST.md
```

### 4. **TASKS.md** - Task List ⭐ CRITICAL
47 tasks across 9 phases with full details and dependencies.
```bash
cat DIGIDOLLAR_SENDRECEIVE_TASKS.md
```

### 5. **ORCHESTRATOR.md** - For Orchestrator Agent
Instructions for managing sub-agents and enforcing TDD.
**MUST READ**:
- DIGIDOLLAR_SENDRECEIVE_IMPLEMENTATION_CHECKLIST.md
- DIGIDOLLAR_SENDRECEIVE_TASKS.md
- DIGIDOLLAR_SENDRECEIVE_EXPLAINER.md
- DIGIDOLLAR_DB_PERSISTENCE_EXPLAINER.md
```bash
cat DIGIDOLLAR_SENDRECEIVE_ORCHESTRATOR.md
```

### 6. **SUBAGENT.md** - For Sub-Agents
TDD process, code patterns, integration requirements.
**MUST INTEGRATE WITH**:
- Existing persistence layer (WalletBatch, wallet.dat)
- Position tracking (GetPositions)
- UTXO structure (vout[1] = DD output)
```bash
cat DIGIDOLLAR_SENDRECEIVE_SUBAGENT.md
```

### 7. **TDD_GUIDE.md** - Methodology
Detailed TDD examples for each phase.
```bash
cat DIGIDOLLAR_SENDRECEIVE_TDD_GUIDE.md
```

### 8. **EXPLAINER.md** - Architecture
System design, data flow, integration points.
```bash
cat DIGIDOLLAR_SENDRECEIVE_EXPLAINER.md
```

### 9. **VERIFICATION.md** - Final Checklist ✅
Complete verification checklist for declaring done.
```bash
cat DIGIDOLLAR_SENDRECEIVE_VERIFICATION.md
```

## 📖 CRITICAL TERMINOLOGY

**IMPORTANT**: Use correct naming throughout implementation:

### What They Are Called
- ❌ **NOT** "positions" (too generic)
- ✅ **YES** "DD Time-Locks" or "Time-Locked DGB backing DD"
- ✅ **YES** "DDTimeLocks" in function names

### Structure
```
DD Time-Lock = Time-Locked DGB (collateral) + DigiDollar (spendable)

Mint Transaction creates:
├─ vout[0]: Time-Locked DGB (backing/collateral)
└─ vout[1]: DigiDollar (spendable DD token)
```

### Function Naming
- `GetDDTimeLocks()` → Returns DD time-locks (RENAMED from GetPositions in Phase 0)
- `dd_timelock_id` → Time-lock transaction ID (RENAMED from position_id in Phase 0)
- `WalletCollateralPosition` → Represents a DD time-lock (struct name unchanged for compatibility)

### In Documentation/Comments
Always say: "Time-Locked DGB backing DD" or "DD time-lock"
Never say: "position" (unless referring to existing code variable names)

## 🔍 Critical Integration Points

### Persistence Layer (Already Complete)
```cpp
// Database operations (wallet.dat)
WalletBatch batch(database);

// Write DD time-lock (renamed from WritePosition in Phase 0)
batch.WriteDDTimeLock(timelock);

// Write balance
batch.WriteDDBalance(address, amount);

// Write transaction
batch.WriteDDTransaction(ddtx);

// Read on startup
wallet.LoadFromDatabase();
```

### UTXO Structure (MUST UNDERSTAND)
```
Mint Transaction:
├─ vout[0]: Collateral (DGB, locked)
└─ vout[1]: DigiDollar (DD, spendable) ← THIS IS THE DD UTXO

To spend DD:
COutPoint dd_utxo(dd_timelock_id, 1);  // Always index 1!
```

### DD Time-Lock Tracking
```cpp
// Get active DD time-locks (in-memory cache)
std::vector<WalletCollateralPosition> timelocks = GetDDTimeLocks(true);

// Each time-lock has:
timelock.dd_timelock_id   // txid of mint transaction (renamed from position_id)
timelock.dd_amount        // DD amount in cents
timelock.dgb_locked       // Time-Locked DGB collateral amount
timelock.is_active        // true if not redeemed
```

## ✅ Success Criteria (ALL MUST PASS)

### Compilation
```bash
make clean
make -j$(nproc) src/qt/digibyte-qt  # Must succeed
```

### Unit Tests
```bash
./src/test/test_digibyte --run_test=digidollar_*  # All pass
```

### Functional Tests
```bash
./test/functional/digidollar_transfer.py          # Pass
./test/functional/digidollar_wallet.py            # Pass
./test/functional/wallet_digidollar_*.py          # Pass (new)
```

### Manual Qt Test
```bash
./src/qt/digibyte-qt -regtest

# Steps:
1. Mint 1000 DD
2. Get receive address from another wallet
3. Send 500 DD
4. Verify sender: 500 DD remaining
5. Verify receiver: 500 DD received
6. Restart both wallets
7. Verify balances persist
8. Verify transaction history intact
9. Verify UTXOs spendable
```

## 🚀 How to Begin

### Orchestrator Reads (in order):
1. DIGIDOLLAR_SENDRECEIVE_INDEX.md (navigation)
2. DIGIDOLLAR_SENDRECEIVE_SUMMARY.md (overview)
3. DIGIDOLLAR_SENDRECEIVE_TASKS.md (task list)
4. DIGIDOLLAR_SENDRECEIVE_ORCHESTRATOR.md (workflow)
5. DIGIDOLLAR_SENDRECEIVE_EXPLAINER.md (architecture)
6. DIGIDOLLAR_DB_PERSISTENCE_EXPLAINER.md (persistence - CRITICAL!)
7. Existing code:
   - src/wallet/digidollarwallet.cpp
   - src/wallet/walletdb.cpp
   - src/digidollar/txbuilder.cpp
   - src/test/digidollar_transfer_tests.cpp

### Sub-Agent Receives Task Assignment:
```markdown
## Task Assignment: Phase 1.1 - DD UTXO Tracking

**Objective**: Implement GetDDUTXOs() to extract spendable DD UTXOs from active positions

**Files to Modify**:
- src/wallet/digidollarwallet.h (add DDUtxo struct, GetDDUTXOs() declaration)
- src/wallet/digidollarwallet.cpp (implement GetDDUTXOs())
- src/test/digidollar_wallet_tests.cpp (add test_get_dd_utxos)

**TDD Process**:
1. RED: Write failing test in digidollar_wallet_tests.cpp
2. GREEN: Implement GetDDUTXOs() to pass test
3. REFACTOR: Clean up, add logging, document

**Integration**:
- MUST use GetPositions(true) for active positions
- MUST create COutPoint(position_id, 1) for DD UTXOs
- MUST return vector of DDUtxo structs

**Context Documents**:
- DIGIDOLLAR_SENDRECEIVE_SUBAGENT.md (TDD process)
- DIGIDOLLAR_SENDRECEIVE_EXPLAINER.md (UTXO structure)
- DIGIDOLLAR_DB_PERSISTENCE_EXPLAINER.md (how positions work)
```

## 🎯 Key Success Factors

### 1. TDD Discipline
Every task MUST follow RED → GREEN → REFACTOR:
- ❌ No implementation without failing test first
- ❌ No skipping refactor phase
- ✅ Every line of code has a test

### 2. Persistence Integration
MUST integrate with existing wallet.dat storage:
- ✅ Use WalletBatch for all database operations
- ✅ Update in-memory cache AND database
- ✅ Ensure atomic updates (all or nothing)
- ✅ Support auto-loading on wallet startup

### 3. UTXO Consistency
MUST maintain accurate UTXO state:
- ✅ Track spent UTXOs correctly
- ✅ Add new UTXOs from received transactions
- ✅ Never lose track of spendable DD
- ✅ Prevent double-spending

### 4. Qt Wallet Integration
MUST work in Qt GUI:
- ✅ Send dialog functional
- ✅ Balance displays update
- ✅ Transaction list shows history
- ✅ Error messages user-friendly

### 5. Test Coverage
MUST have comprehensive tests:
- ✅ Unit tests for all functions
- ✅ Functional tests for end-to-end flows
- ✅ Integration tests with persistence
- ✅ Manual Qt wallet testing

## 📊 Progress Tracking

Orchestrator creates: `DIGIDOLLAR_SENDRECEIVE_PROGRESS.md`

Format:
```markdown
# Phase 1: Coin Selection Foundation
- [✅] Task 1.1: DD UTXO Tracking (SubAgent-1, Complete)
- [🔄] Task 1.2: DD UTXO Value Lookup (SubAgent-2, In Progress)
- [⏸️] Task 1.3: SelectDDCoins (Blocked: needs 1.1, 1.2)
- [⏳] Task 1.4: DGB UTXO Selection (Not Started)
- [⏳] Task 1.5: Change Calculation (Not Started)

Metrics:
- Tasks: 1/5 complete (20%)
- Tests: 1/5 passing
- Coverage: 15%
```

## 🔐 Security Requirements

### MUST Enforce:
1. **Private Keys**: NEVER log private keys
2. **Amount Overflow**: Validate all amount calculations
3. **UTXO Locking**: Lock UTXOs during tx building
4. **Signature Verification**: Always verify before broadcast
5. **Double-Spend Prevention**: Check UTXO not already spent

### Code Patterns:
```cpp
// ✅ CORRECT
LogPrintf("Signing with pubkey: %s\n", pubkey.ToString());

// ❌ WRONG - NEVER DO THIS
// LogPrintf("Private key: %s\n", privkey.ToString());
```

## 🐛 Troubleshooting

### Compilation Fails
```bash
# Clean and rebuild
make clean
make -j$(nproc) src/qt/digibyte-qt 2>&1 | tee build.log
# Check build.log for errors
```

### Tests Fail
```bash
# Run with verbose output
./src/test/test_digibyte --run_test=failing_test --log_level=all

# Check debug log
tail -f ~/.digibyte/regtest/debug.log | grep "DigiDollar:"
```

### UTXO Issues
```bash
# Verify positions loaded
grep "Loaded.*positions" ~/.digibyte/regtest/debug.log

# Check UTXO count
# In Qt console:
getdigidollarbalance  # Should match sum of positions
```

## 📞 Escalation Path

### When to Escalate to User:
1. Critical blocker preventing all progress
2. Architectural decision needed
3. Conflicting requirements found
4. Existing code has bugs that prevent implementation

### What to Report:
```markdown
**Issue**: SelectDDCoins cannot access positions - GetPositions() returns empty

**Context**: 
- Task: Phase 1.3 (SelectDDCoins Enhancement)
- File: src/wallet/digidollarwallet.cpp:1335
- Blocker: Position cache not populating from database

**Attempted**:
- Verified LoadFromDatabase() called in constructor
- Checked database has positions (it does)
- Added logging - cache remains empty

**Recommendation**:
Need to debug LoadPositionsFromDatabase() - may have key mismatch issue
```

## 🎉 Declaration of Success

When DIGIDOLLAR_SENDRECEIVE_VERIFICATION.md checklist 100% complete:

```
✅ DIGIDOLLAR SEND/RECEIVE COMPLETE

Functional:
✅ Send DD between wallets
✅ Receive DD in wallet
✅ Balances accurate
✅ Persistence across restarts
✅ UTXO tracking correct
✅ Qt wallet functional

Technical:
✅ Wallet compiles clean
✅ All unit tests pass
✅ All functional tests pass
✅ Manual testing successful
✅ Test coverage ≥ 80%
✅ No memory leaks

Quality:
✅ TDD methodology followed
✅ Code documented
✅ Error handling complete
✅ Security enforced

READY FOR PRODUCTION 🚀
```

---

**This README is your command center. Reference it throughout implementation.**

**Start Command**: `"Begin Send/Receive Implementation"`
