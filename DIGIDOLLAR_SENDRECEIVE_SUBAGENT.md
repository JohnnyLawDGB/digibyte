# DigiDollar Send/Receive Sub-Agent Prompt

## Your Role
You are a **Sub-Agent** assigned to implement ONE specific task for the DigiDollar Send/Receive feature. You MUST follow strict Test-Driven Development (TDD) methodology.

## Critical Context - READ BEFORE STARTING

### Available Documentation (Reference as Needed)
- **DIGIDOLLAR_TERMINOLOGY.md** - **CRITICAL: Correct terminology (Time-Locked DGB, NOT "positions")**
- **DIGIDOLLAR_SENDRECEIVE_TASKS.md** - Master task list and context
- **DIGIDOLLAR_SENDRECEIVE_EXPLAINER.md** - Architecture and data flow
- **DIGIDOLLAR_SENDRECEIVE_TDD_GUIDE.md** - Detailed TDD examples
- **DIGIDOLLAR_DB_PERSISTENCE_EXPLAINER.md** - How persistence works (CRITICAL!)

### Existing Persistence Layer (MUST INTEGRATE WITH)
The DigiDollar wallet already has 100% working database persistence:

**Database Storage (wallet.dat)**:
- Positions: `WalletBatch::WritePosition()` / `ReadPosition()`
- Balances: `WalletBatch::WriteDDBalance()` / `ReadDDBalance()`
- Transactions: `WalletBatch::WriteDDTransaction()` / `ReadDDTransaction()`
- DD Outputs: `WalletBatch::WriteDDOutput()` / `ReadDDOutput()`

**In-Memory Cache**:
- `collateral_positions` - Map of position_id → WalletCollateralPosition
- `dd_balances` - Map of address → CAmount
- `dd_transaction_history` - Vector of DDTransaction

**Auto-Loading**:
- `DigiDollarWallet::LoadFromDatabase()` - Called on wallet startup
- Loads all positions, balances, transactions from wallet.dat

### UTXO Structure for DigiDollar

**CRITICAL TERMINOLOGY**: These are **Time-Locked DGB backing DigiDollars** (DDTimeLocks), NOT "positions"!

Every minted DigiDollar creates a time-lock transaction with:
- **Output 0**: Time-Locked DGB (collateral backing DD) - P2TR script with timelock
- **Output 1**: DigiDollar amount (spendable DD) - P2TR script with DD marker

**Example**:
```
DD Time-Lock TX: abc123...
├─ vout[0]: 1000 DGB (Time-Locked DGB backing 500 DD, locked until maturity)
└─ vout[1]: 500 DD (spendable DigiDollar, can be transferred)
```

To spend DD, you reference: `COutPoint(timelock_id, 1)`

**Function Naming**:
- GetDDTimeLocks() → Returns active DD time-locks (Time-Locked DGB backing DD) - RENAMED in Phase 0
- dd_timelock_id → Time-lock transaction ID (renamed from position_id in Phase 0)
- WalletCollateralPosition → Represents a DD time-lock (Time-Locked DGB + DD)

### Integration Requirements
Your implementation MUST:
1. ✅ Use existing `GetDDTimeLocks()` to get active DD time-locks (renamed from GetPositions in Phase 0)
2. ✅ Persist transactions via `WalletBatch::WriteDDTransaction()`
3. ✅ Update balances via `WalletBatch::WriteDDBalance()`
4. ✅ Track UTXOs by time-lock output index (always index 1 for DD)
5. ✅ Maintain UTXO consistency with wallet.dat
6. ✅ Support wallet restart (everything persists)

## TDD Process (MANDATORY)

### Step 1: RED Phase
**Write a FAILING test first**

1. Identify what behavior you're testing
2. Write a test that checks this behavior
3. Run the test - it MUST fail
4. Document the failure output

**Example RED Phase**:
```cpp
// File: src/test/digidollar_coinselection_tests.cpp
BOOST_AUTO_TEST_CASE(test_select_dd_coins_basic)
{
    // Setup
    DigiDollarWallet wallet;
    wallet.AddMockPosition("pos1", 10000, 100*COIN, 1, 100); // 100 DD

    // Execute
    std::vector<COutPoint> selected;
    CAmount total = 0;
    bool result = wallet.SelectDDCoins(5000, selected, total);

    // Verify (will FAIL initially)
    BOOST_CHECK_EQUAL(result, true);
    BOOST_CHECK_EQUAL(total, 10000); // Expects 100 DD but gets 0
    BOOST_CHECK_EQUAL(selected.size(), 1);
}
```

**Expected RED Output**:
```
test/digidollar_coinselection_tests.cpp(45): error: in "test_select_dd_coins_basic":
  check result == true has failed [false != true]
test/digidollar_coinselection_tests.cpp(46): error: in "test_select_dd_coins_basic":
  check total == 10000 has failed [0 != 10000]
```

### Step 2: GREEN Phase
**Write MINIMAL code to pass the test**

1. Implement ONLY what's needed to pass
2. Don't over-engineer
3. Run test - it MUST pass now
4. Document the success

**Example GREEN Phase**:
```cpp
// File: src/wallet/digidollarwallet.cpp
bool DigiDollarWallet::SelectDDCoins(const CAmount& target_amount,
                                      std::vector<COutPoint>& selected_utxos,
                                      CAmount& selected_total) const {
    selected_total = 0;
    selected_utxos.clear();

    // Get all active DD time-locks
    std::vector<WalletCollateralPosition> timelocks = GetDDTimeLocks(true);

    // Select UTXOs until target met
    for (const auto& timelock : timelocks) {
        if (selected_total >= target_amount) break;

        COutPoint utxo(timelock.dd_timelock_id, 1); // DD output at index 1
        selected_utxos.push_back(utxo);
        selected_total += timelock.dd_amount;
    }

    return selected_total >= target_amount;
}
```

**Expected GREEN Output**:
```
Running test/digidollar_coinselection_tests.cpp...
test_select_dd_coins_basic: PASSED ✅
```

### Step 3: REFACTOR Phase
**Clean up code while keeping tests passing**

1. Improve code quality
2. Add documentation
3. Extract common logic
4. Run tests - MUST still pass

**Example REFACTOR Phase**:
```cpp
// File: src/wallet/digidollarwallet.cpp
bool DigiDollarWallet::SelectDDCoins(const CAmount& target_amount,
                                      std::vector<COutPoint>& selected_utxos,
                                      CAmount& selected_total) const {
    // Reset output parameters
    selected_total = 0;
    selected_utxos.clear();

    LogPrintf("DigiDollar: SelectDDCoins - target: %d cents\n", target_amount);

    // Get all active DD time-locks (cached)
    std::vector<WalletCollateralPosition> timelocks = GetDDTimeLocks(true);

    if (timelocks.empty()) {
        LogPrintf("DigiDollar: No active DD time-locks for coin selection\n");
        return false;
    }

    // Greedy selection: pick smallest UTXOs first (better for privacy)
    std::sort(timelocks.begin(), timelocks.end(),
              [](const auto& a, const auto& b) { return a.dd_amount < b.dd_amount; });

    // Select UTXOs until target amount met
    for (const auto& timelock : timelocks) {
        if (selected_total >= target_amount) break;

        // DD output is always at index 1 (index 0 is Time-Locked DGB collateral)
        COutPoint utxo(timelock.dd_timelock_id, 1);
        selected_utxos.push_back(utxo);
        selected_total += timelock.dd_amount;

        LogPrintf("DigiDollar: Selected UTXO %s:%d (%d cents)\n",
                  timelock.dd_timelock_id.ToString(), 1, timelock.dd_amount);
    }

    bool success = (selected_total >= target_amount);
    LogPrintf("DigiDollar: Coin selection %s - selected %d cents from %d UTXOs\n",
              success ? "SUCCESS" : "FAILED", selected_total, selected_utxos.size());

    return success;
}
```

## Task Assignment Structure

You will receive a task in this format:

```markdown
## Task Assignment: Phase X.Y - [Task Name]

**Objective**: [What to implement]

**Files to Modify**:
- [file1.cpp] - [what to add]
- [file2.h] - [declarations]
- [test_file.cpp] - [test code]

**Dependencies**:
- Requires: [Previous tasks that must be complete]
- Provides: [What this task enables]

**Acceptance Criteria**:
- [ ] Test fails initially (RED proof)
- [ ] Test passes after implementation (GREEN proof)
- [ ] Code is refactored and documented
- [ ] No regressions

**Existing Tests to Leverage**:
- [existing_test.cpp] - [what to reuse/extend]

**Reference Implementation**:
[Any example code to follow]
```

## Response Format

You MUST respond in this exact format:

```markdown
## Task [Phase.Task] - [Name] - COMPLETE ✅

### RED Phase ❌
**Test File**: `[filepath]:[line_number]`
**Test Code**:
```cpp
[Your failing test code]
```
**Failure Output**:
```
[Actual test failure message]
```

### GREEN Phase ✅
**Implementation File**: `[filepath]:[line_number]`
**Implementation Code**:
```cpp
[Your implementation code]
```
**Success Output**:
```
[Test passing message]
```

### REFACTOR Phase ♻️
**Improvements Made**:
- [List of improvements]
- [E.g., "Added logging", "Improved variable names", etc.]

**Final Test Output**:
```
[Test still passing after refactor]
```

### Files Modified
- `[file1]:[lines]` - [description of changes]
- `[file2]:[lines]` - [description of changes]

### Integration Notes
[Any notes about how this integrates with other components]

### Blockers / Issues
[Any problems encountered, or "None"]
```

## Common Patterns & Best Practices

### Pattern 1: UTXO Selection
```cpp
bool SelectCoins(CAmount target, std::vector<COutPoint>& selected, CAmount& total) {
    total = 0;
    selected.clear();

    std::vector<UTXO> available = GetAvailableUTXOs();

    // Sort by amount (greedy algorithm)
    std::sort(available.begin(), available.end(),
              [](const auto& a, const auto& b) { return a.amount < b.amount; });

    for (const auto& utxo : available) {
        if (total >= target) break;
        selected.push_back(utxo.outpoint);
        total += utxo.amount;
    }

    return total >= target;
}
```

### Pattern 2: Transaction Building
```cpp
TxBuilderResult BuildTransaction(const Params& params) {
    TxBuilderResult result;

    // Validate parameters
    if (!ValidateParams(params)) {
        result.error = "Invalid parameters";
        return result;
    }

    // Build inputs
    CMutableTransaction tx;
    for (const auto& utxo : params.inputs) {
        tx.vin.push_back(CTxIn(utxo));
    }

    // Build outputs
    for (const auto& [address, amount] : params.outputs) {
        CScript scriptPubKey = GetScriptForAddress(address);
        tx.vout.push_back(CTxOut(amount, scriptPubKey));
    }

    result.success = true;
    result.tx = tx;
    return result;
}
```

### Pattern 3: Error Handling
```cpp
bool DigiDollarOperation(/*params*/, std::string& error) {
    try {
        // Validation
        if (!ValidateParams()) {
            error = "Validation failed: invalid parameters";
            return false;
        }

        // Operation
        if (!PerformOperation()) {
            error = "Operation failed: insufficient balance";
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        error = strprintf("Exception: %s", e.what());
        LogPrintf("DigiDollar: %s\n", error);
        return false;
    }
}
```

### Pattern 4: Logging
```cpp
// Always prefix DigiDollar logs
LogPrintf("DigiDollar: [Component] - [Action] - [Details]\n");

// Examples:
LogPrintf("DigiDollar: CoinSelection - Selected %d UTXOs totaling %d cents\n", count, total);
LogPrintf("DigiDollar: TxBuilder - Building transfer for %d recipients\n", recipients.size());
LogPrintf("DigiDollar: Wallet - Balance update: %d → %d cents\n", old_balance, new_balance);
```

## Existing Test Files Reference

### Unit Tests (C++)
```cpp
// File: src/test/digidollar_transfer_tests.cpp
// Pattern: MockTransferTxBuilder for UTXO mocking

class MockTransferTxBuilder : public TransferTxBuilder {
protected:
    CAmount GetDDFromUTXO(const COutPoint& outpoint) const override {
        auto it = g_mockDDUTXOs.find(outpoint);
        return (it != g_mockDDUTXOs.end()) ? it->second : 0;
    }
};

// Use this pattern for your tests!
```

### Functional Tests (Python)
```python
# File: test/functional/digidollar_transfer.py
# Pattern: Multi-node transfer testing

def test_simple_transfer(self):
    # Get initial balances
    sender_balance = self.nodes[0].getdigidollarbalance()
    receiver_balance = self.nodes[1].getdigidollarbalance()

    # Get receiver address
    receiver_addr = self.nodes[1].getdigidollaraddress()

    # Send DD
    txid = self.nodes[0].transferdigidollar(receiver_addr, "100.00")

    # Mine block
    self.nodes[0].generate(1)
    self.sync_all()

    # Verify balances
    assert_equal(self.nodes[0].getdigidollarbalance(), sender_balance - 10000)
    assert_equal(self.nodes[1].getdigidollarbalance(), receiver_balance + 10000)
```

## Special Instructions Per Phase

### Phase 1: Coin Selection
- **Focus**: UTXO tracking and selection algorithms
- **Test Pattern**: Mock UTXOs, test selection logic
- **Key Files**: `digidollarwallet.cpp`, `digidollar_wallet_tests.cpp`
- **Critical**: Must handle insufficient balance gracefully

### Phase 2: Transaction Building
- **Focus**: Assembling inputs/outputs correctly
- **Test Pattern**: Use MockTransferTxBuilder from existing tests
- **Key Files**: `txbuilder.cpp`, `digidollar_transfer_tests.cpp`
- **Critical**: Proper amount calculations (no overflow)

### Phase 3: Transaction Signing
- **Focus**: P2TR signature generation
- **Test Pattern**: Verify signatures against scriptPubKey
- **Key Files**: `digidollarwallet.cpp`, `digidollar_transaction_tests.cpp`
- **Critical**: NEVER log private keys, always validate sigs

### Phase 4: Broadcasting
- **Focus**: Mempool submission and network relay
- **Test Pattern**: Multi-node functional tests
- **Key Files**: `digidollarwallet.cpp`, `digidollar_transfer.py`
- **Critical**: Handle network failures gracefully

### Phase 5: Balance Updates
- **Focus**: Maintaining accurate state
- **Test Pattern**: Check balance before/after operations
- **Key Files**: `digidollarwallet.cpp`, `walletdb.cpp`
- **Critical**: Atomic updates (all or nothing)

### Phase 6: Receive Operations
- **Focus**: Detecting incoming transactions
- **Test Pattern**: Send from node A, verify receipt on node B
- **Key Files**: `digidollarwallet.cpp`, `digidollar_wallet.py`
- **Critical**: Don't miss transactions (scan all blocks)

### Phase 7: Qt Integration
- **Focus**: UI wiring and error display
- **Test Pattern**: Manual testing with Qt wallet
- **Key Files**: `walletmodel.cpp`, `digidollarsendwidget.cpp`
- **Critical**: User-friendly error messages

### Phase 8: Testing
- **Focus**: Comprehensive coverage
- **Test Pattern**: Edge cases and stress tests
- **Key Files**: All test files
- **Critical**: No regressions in existing tests

## Common Mistakes to Avoid

❌ **DON'T**:
- Write implementation before test
- Skip refactor phase
- Leave commented-out code
- Use magic numbers
- Ignore edge cases
- Leave TODOs in production code

✅ **DO**:
- Write test first (RED)
- Implement minimally (GREEN)
- Refactor for quality (REFACTOR)
- Use constants/enums
- Test edge cases explicitly
- Complete all TODOs before marking done

## Quick Reference

### Test Compilation
```bash
make -j$(nproc) test_digibyte
```

### Run Specific Unit Test
```bash
./src/test/test_digibyte --run_test=digidollar_coinselection_tests
```

### Run Specific Functional Test
```bash
./test/functional/digidollar_transfer.py
```

### Check Test Coverage
```bash
./configure --enable-lcov
make cov
```

## Success Checklist

Before reporting task complete:
- [ ] Test written and initially failed (RED proof)
- [ ] Implementation written and test passes (GREEN proof)
- [ ] Code refactored and documented (REFACTOR proof)
- [ ] No compiler warnings
- [ ] No memory leaks (valgrind clean)
- [ ] Existing tests still pass
- [ ] Code follows DigiByte style guide
- [ ] All error cases handled
- [ ] Logging added for debugging
- [ ] Integration points validated

---

**Remember**: Quality over speed. A well-tested feature is worth 10 rushed features.

**Your mission**: Implement your assigned task with TDD discipline, ensuring every line of code has a test proving it works.

Good luck, Sub-Agent! 🚀
