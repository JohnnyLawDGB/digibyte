# DigiDollar Send/Receive TDD Implementation Guide

## Overview
This guide provides detailed Test-Driven Development (TDD) methodology for implementing DigiDollar send/receive functionality. It builds on the successful persistence layer implementation and applies the same rigorous testing approach.

## TDD Philosophy

### Why TDD for Send/Receive?
1. **Financial Accuracy**: Handling money requires zero tolerance for bugs
2. **UTXO Integrity**: Any mistake in UTXO tracking = loss of funds
3. **Network Reliability**: Must work across network failures and reorgs
4. **User Trust**: Users must have confidence in send/receive

### The RED-GREEN-REFACTOR Cycle

```
┌─────────────────────────────────────────────┐
│              TDD CYCLE                      │
│                                             │
│  ┌──────┐      ┌───────┐      ┌──────────┐ │
│  │ RED  │ ───> │ GREEN │ ───> │ REFACTOR │ │
│  └──────┘      └───────┘      └──────────┘ │
│     │              │               │        │
│     │              │               │        │
│     v              v               v        │
│  Write         Implement        Improve     │
│  Failing       Minimal          Code        │
│  Test          Code             Quality     │
│                                             │
└─────────────────────────────────────────────┘
```

## Detailed Phase Guides

### Phase 1: Coin Selection Foundation

#### Why This Is Critical
Coin selection is the foundation of sending. Without proper UTXO tracking and selection, nothing else works.

#### Task 1.1: DD UTXO Tracking

**RED Phase - Write Failing Test**:
```cpp
// File: src/test/digidollar_coinselection_tests.cpp

BOOST_FIXTURE_TEST_SUITE(digidollar_coinselection_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(test_get_dd_utxos_from_positions)
{
    // Setup: Create wallet with mock positions
    DigiDollarWallet wallet;

    // Add 3 positions with different DD amounts
    uint256 pos1 = InsecureRand256();
    uint256 pos2 = InsecureRand256();
    uint256 pos3 = InsecureRand256();

    wallet.AddMockPosition(pos1, 10000, 100*COIN, 1, 100);  // 100 DD
    wallet.AddMockPosition(pos2, 25000, 250*COIN, 2, 100);  // 250 DD
    wallet.AddMockPosition(pos3, 50000, 500*COIN, 3, 100);  // 500 DD

    // Execute: Get DD UTXOs
    std::vector<DDUtxo> utxos = wallet.GetDDUTXOs();

    // Verify: Should have 3 UTXOs matching positions
    BOOST_CHECK_EQUAL(utxos.size(), 3);

    // Verify UTXO 1
    BOOST_CHECK_EQUAL(utxos[0].outpoint.hash, pos1);
    BOOST_CHECK_EQUAL(utxos[0].outpoint.n, 1);  // DD at index 1
    BOOST_CHECK_EQUAL(utxos[0].dd_amount, 10000);

    // Verify UTXO 2
    BOOST_CHECK_EQUAL(utxos[1].outpoint.hash, pos2);
    BOOST_CHECK_EQUAL(utxos[1].dd_amount, 25000);

    // Verify UTXO 3
    BOOST_CHECK_EQUAL(utxos[2].outpoint.hash, pos3);
    BOOST_CHECK_EQUAL(utxos[2].dd_amount, 50000);
}

BOOST_AUTO_TEST_SUITE_END()
```

**Expected RED Output**:
```
Running 1 test case...
test/digidollar_coinselection_tests.cpp:25: error: in "test_get_dd_utxos_from_positions":
  check utxos.size() == 3 has failed [0 != 3]

*** 1 failure is detected in the test module "DigiByte Test Suite"
```

**GREEN Phase - Minimal Implementation**:
```cpp
// File: src/wallet/digidollarwallet.h

struct DDUtxo {
    COutPoint outpoint;
    CAmount dd_amount;
    bool is_spendable;

    DDUtxo(const COutPoint& out, CAmount amt)
        : outpoint(out), dd_amount(amt), is_spendable(true) {}
};

class DigiDollarWallet {
public:
    std::vector<DDUtxo> GetDDUTXOs() const;
};
```

```cpp
// File: src/wallet/digidollarwallet.cpp

std::vector<DDUtxo> DigiDollarWallet::GetDDUTXOs() const {
    std::vector<DDUtxo> utxos;

    // Get all active positions
    std::vector<WalletCollateralPosition> positions = GetPositions(true);

    // Convert positions to DD UTXOs
    for (const auto& pos : positions) {
        // DD output is always at index 1 (collateral at index 0)
        COutPoint dd_outpoint(pos.position_id, 1);
        utxos.emplace_back(dd_outpoint, pos.dd_amount);
    }

    return utxos;
}
```

**Expected GREEN Output**:
```
Running 1 test case...
*** No errors detected
```

**REFACTOR Phase - Improve Quality**:
```cpp
// Enhanced with logging, error checking, and documentation

/**
 * Get all spendable DigiDollar UTXOs from active positions
 * @return Vector of DD UTXOs with amounts and spendability
 */
std::vector<DDUtxo> DigiDollarWallet::GetDDUTXOs() const {
    std::vector<DDUtxo> utxos;

    LogPrintf("DigiDollar: GetDDUTXOs - Scanning active positions\n");

    // Get all active positions from cache
    std::vector<WalletCollateralPosition> positions = GetPositions(true);

    if (positions.empty()) {
        LogPrintf("DigiDollar: No active positions, returning empty UTXO set\n");
        return utxos;
    }

    // Convert each position to a spendable DD UTXO
    // Position structure: vout[0] = collateral, vout[1] = DD output
    for (const auto& pos : positions) {
        COutPoint dd_outpoint(pos.position_id, 1);
        utxos.emplace_back(dd_outpoint, pos.dd_amount);

        LogPrintf("DigiDollar: Found DD UTXO %s:%d (%d cents)\n",
                  pos.position_id.ToString(), 1, pos.dd_amount);
    }

    LogPrintf("DigiDollar: GetDDUTXOs - Found %d spendable UTXOs\n", utxos.size());
    return utxos;
}
```

**Test Still Passes**:
```
Running 1 test case...
*** No errors detected
```

#### Task 1.2: DD UTXO Value Lookup

**RED → GREEN → REFACTOR** (Similar pattern)

Test lookup of DD amount from UTXO outpoint, implement caching for performance.

#### Task 1.3: SelectDDCoins Enhancement

**Complex Algorithm Testing**:

```cpp
// Test edge cases
BOOST_AUTO_TEST_CASE(test_select_dd_coins_exact_match)
BOOST_AUTO_TEST_CASE(test_select_dd_coins_insufficient_balance)
BOOST_AUTO_TEST_CASE(test_select_dd_coins_multiple_utxos)
BOOST_AUTO_TEST_CASE(test_select_dd_coins_change_needed)
```

Each test follows RED-GREEN-REFACTOR.

### Phase 2: Transaction Building

#### Task 2.1: Input Assembly

**Using Existing Test Patterns**:
```cpp
// Reference: src/test/digidollar_transfer_tests.cpp
// Reuse MockTransferTxBuilder pattern

BOOST_AUTO_TEST_CASE(test_build_transfer_inputs)
{
    // Setup mock builder
    MockTransferTxBuilder builder(chainParams, 100000, 2500);

    // Create mock DD UTXOs
    COutPoint utxo1 = CreateMockDDUTXO(10000);  // 100 DD
    COutPoint utxo2 = CreateMockDDUTXO(20000);  // 200 DD

    // Build transfer params
    TxBuilderTransferParams params;
    params.ddUtxos = {utxo1, utxo2};
    params.recipients = {{"DD1abc...", 15000}};

    // Execute
    TxBuilderResult result = builder.BuildTransferTransaction(params);

    // Verify inputs created correctly
    BOOST_CHECK_EQUAL(result.success, true);
    BOOST_CHECK_EQUAL(result.tx.vin.size(), 2);
    BOOST_CHECK_EQUAL(result.tx.vin[0].prevout, utxo1);
    BOOST_CHECK_EQUAL(result.tx.vin[1].prevout, utxo2);
}
```

### Phase 3: Transaction Signing

#### Critical Security Testing

```cpp
BOOST_AUTO_TEST_CASE(test_sign_dd_inputs_p2tr)
{
    // Setup keys
    CKey sender_key;
    sender_key.MakeNewKey(true);

    // Create unsigned transaction
    CMutableTransaction tx = CreateMockDDTransaction();

    // Sign DD inputs
    bool success = SignDDInputs(tx, sender_key);

    // Verify signatures
    BOOST_CHECK_EQUAL(success, true);
    BOOST_CHECK(VerifyDDSignature(tx, 0, sender_key.GetPubKey()));

    // Test invalid key (should fail)
    CKey wrong_key;
    wrong_key.MakeNewKey(true);
    BOOST_CHECK(!VerifyDDSignature(tx, 0, wrong_key.GetPubKey()));
}
```

### Phase 4: Broadcasting & Confirmation

#### Functional Test Example

```python
# File: test/functional/wallet_digidollar_send.py

def test_send_basic(self):
    """Test basic DD send operation"""

    # Setup: Generate blocks and mint DD
    self.nodes[0].generate(110)
    self.nodes[0].mintdigidollar("1000.00", 365)
    self.nodes[0].generate(1)
    self.sync_all()

    # Get initial state
    sender_balance = self.nodes[0].getdigidollarbalance()
    receiver_addr = self.nodes[1].getdigidollaraddress()

    # Execute: Send 500 DD
    txid = self.nodes[0].transferdigidollar(receiver_addr, "500.00")

    # Verify: Transaction in mempool
    assert txid in self.nodes[0].getrawmempool()

    # Mine block
    self.nodes[0].generate(1)
    self.sync_all()

    # Verify: Balances updated
    assert_equal(self.nodes[0].getdigidollarbalance(), sender_balance - 50000)
    assert_equal(self.nodes[1].getdigidollarbalance(), 50000)

    # Verify: Transaction confirmed
    tx_info = self.nodes[0].gettransaction(txid)
    assert_equal(tx_info['confirmations'], 1)
```

### Phase 5: Balance & State Updates

#### Testing State Consistency

```cpp
BOOST_AUTO_TEST_CASE(test_balance_update_after_send)
{
    DigiDollarWallet wallet;

    // Setup: Initial state
    wallet.AddMockPosition("pos1", 100000, 1000*COIN, 1, 100);
    CAmount initial_balance = wallet.GetTotalDDBalance();
    BOOST_CHECK_EQUAL(initial_balance, 100000);

    // Execute: Simulate send
    wallet.MarkUTXOSpent(COutPoint(pos1, 1), 50000);  // Spend 500 DD

    // Verify: Balance reduced
    CAmount new_balance = wallet.GetTotalDDBalance();
    BOOST_CHECK_EQUAL(new_balance, 50000);

    // Verify: UTXO marked spent
    std::vector<DDUtxo> utxos = wallet.GetDDUTXOs();
    BOOST_CHECK_EQUAL(utxos.size(), 0);  // Fully spent
}
```

### Phase 6: Receive Operations

#### Multi-Node Receive Test

```python
def test_receive_detection(self):
    """Test incoming DD transaction detection"""

    # Setup
    receiver_addr = self.nodes[1].getdigidollaraddress()
    initial_balance = self.nodes[1].getdigidollarbalance()

    # Execute: Node 0 sends to Node 1
    txid = self.nodes[0].transferdigidollar(receiver_addr, "250.00")

    # Verify: Node 1 sees transaction in mempool
    self.sync_mempools()
    assert txid in self.nodes[1].getrawmempool()

    # Mine and verify
    self.nodes[0].generate(1)
    self.sync_all()

    # Verify: Balance increased
    new_balance = self.nodes[1].getdigidollarbalance()
    assert_equal(new_balance, initial_balance + 25000)

    # Verify: Transaction in history
    txs = self.nodes[1].listdigidollartransactions()
    incoming_tx = next(tx for tx in txs if tx['txid'] == txid)
    assert_equal(incoming_tx['category'], 'receive')
    assert_equal(incoming_tx['amount'], 25000)
```

### Phase 7: Qt Integration

#### Manual Testing Checklist

```markdown
## Qt Send Dialog Test

### Test Case 1: Successful Send
1. ✅ Open Qt wallet in regtest
2. ✅ Navigate to DigiDollar → Send
3. ✅ Enter valid DD address
4. ✅ Enter amount (less than balance)
5. ✅ Click Send
6. ✅ Verify: Success dialog shows
7. ✅ Verify: Transaction ID displayed
8. ✅ Verify: Balance decreases
9. ✅ Verify: Transaction appears in list

### Test Case 2: Insufficient Balance
1. ✅ Enter amount > balance
2. ✅ Click Send
3. ✅ Verify: Error shows "Insufficient balance"
4. ✅ Verify: No transaction created

### Test Case 3: Invalid Address
1. ✅ Enter invalid address "INVALID"
2. ✅ Click Send
3. ✅ Verify: Error shows "Invalid address format"
4. ✅ Verify: No transaction created

### Test Case 4: Wallet Locked
1. ✅ Encrypt wallet
2. ✅ Lock wallet
3. ✅ Try to send
4. ✅ Verify: Unlock dialog appears
5. ✅ Enter passphrase
6. ✅ Verify: Send succeeds
```

### Phase 8: Comprehensive Testing

#### Test Coverage Matrix

```
┌──────────────────┬────────┬──────────┬────────────┐
│ Component        │ Unit   │ Func     │ Integration│
├──────────────────┼────────┼──────────┼────────────┤
│ Coin Selection   │ ✅ 95% │ ✅ 100%  │ ✅ 100%    │
│ Tx Building      │ ✅ 90% │ ✅ 100%  │ ✅ 100%    │
│ Signing          │ ✅ 100%│ ✅ 100%  │ ✅ 100%    │
│ Broadcasting     │ ✅ 80% │ ✅ 100%  │ ✅ 100%    │
│ Balance Updates  │ ✅ 100%│ ✅ 100%  │ ✅ 100%    │
│ Receive          │ ✅ 90% │ ✅ 100%  │ ✅ 100%    │
│ Qt Integration   │ N/A    │ Manual   │ Manual     │
│ Persistence      │ ✅ 100%│ ✅ 100%  │ ✅ 100%    │
└──────────────────┴────────┴──────────┴────────────┘
```

## Common TDD Patterns for Send/Receive

### Pattern 1: Amount Validation
```cpp
// Always test: valid, zero, negative, overflow
BOOST_AUTO_TEST_CASE(test_validate_send_amount) {
    BOOST_CHECK(ValidateAmount(10000));     // Valid
    BOOST_CHECK(!ValidateAmount(0));        // Zero
    BOOST_CHECK(!ValidateAmount(-1000));    // Negative
    BOOST_CHECK(!ValidateAmount(MAX_MONEY + 1)); // Overflow
}
```

### Pattern 2: State Verification
```cpp
// Always verify: before state, operation, after state
auto initial_state = CaptureWalletState(wallet);
PerformOperation();
auto final_state = CaptureWalletState(wallet);
VerifyStateTransition(initial_state, final_state, expected_changes);
```

### Pattern 3: Error Path Testing
```cpp
// Test every error condition
BOOST_AUTO_TEST_CASE(test_send_error_paths) {
    // Insufficient balance
    BOOST_CHECK_EXCEPTION(
        wallet.Send(address, TOO_MUCH),
        std::runtime_error,
        [](const auto& e) { return std::string(e.what()).find("Insufficient") != std::string::npos; }
    );

    // Invalid address
    BOOST_CHECK_EXCEPTION(
        wallet.Send("INVALID", amount),
        std::runtime_error,
        [](const auto& e) { return std::string(e.what()).find("Invalid address") != std::string::npos; }
    );
}
```

## Integration with Persistence Layer

### Persistence + Send Integration Test

```python
def test_send_persistence_across_restart(self):
    """Verify sent transactions persist across wallet restart"""

    # Setup
    receiver = self.nodes[1].getdigidollaraddress()

    # Send transaction
    txid = self.nodes[0].transferdigidollar(receiver, "100.00")
    self.nodes[0].generate(1)
    self.sync_all()

    # Verify transaction in history
    txs_before = self.nodes[0].listdigidollartransactions()
    send_tx = next(tx for tx in txs_before if tx['txid'] == txid)
    assert_equal(send_tx['category'], 'send')

    # Restart wallet
    self.restart_node(0)

    # Verify transaction still in history
    txs_after = self.nodes[0].listdigidollartransactions()
    send_tx_after = next(tx for tx in txs_after if tx['txid'] == txid)
    assert_equal(send_tx_after, send_tx)
```

## Debugging Failed Tests

### When RED Phase Fails Unexpectedly
```bash
# Run with debug output
./src/test/test_digibyte --run_test=digidollar_coinselection_tests --log_level=all

# Check logs
tail -f ~/.digibyte/regtest/debug.log | grep "DigiDollar:"

# Use gdb for segfaults
gdb --args ./src/test/test_digibyte --run_test=problematic_test
```

### When GREEN Phase Won't Pass
1. Verify test expectations are correct
2. Add logging to implementation
3. Check for off-by-one errors
4. Verify mock data setup
5. Check for async timing issues

### When REFACTOR Breaks Tests
1. Run tests after EACH refactor step
2. Use git bisect to find breaking change
3. Refactor in smaller increments
4. Keep test output visible

## Success Metrics

### Task-Level Success
- ✅ Test fails on first run (RED proof)
- ✅ Test passes after implementation (GREEN proof)
- ✅ Test still passes after refactor (quality proof)
- ✅ Code coverage ≥ 80%
- ✅ No memory leaks (valgrind clean)

### Phase-Level Success
- ✅ All tasks complete
- ✅ Integration test passes
- ✅ No regressions
- ✅ Documentation updated
- ✅ Code review approved

### Project-Level Success
- ✅ Can send DD from Qt wallet
- ✅ Can receive DD in Qt wallet
- ✅ Balances accurate at all times
- ✅ Transactions persist across restarts
- ✅ Works on regtest, testnet, mainnet
- ✅ All error cases handled gracefully
- ✅ User experience is smooth

## Final Checklist

Before declaring Send/Receive complete:

### Functionality
- [ ] Can send DD to valid address
- [ ] Can receive DD from another wallet
- [ ] Balance updates correctly
- [ ] Transaction history accurate
- [ ] UTXO set remains consistent
- [ ] Works across wallet restarts

### Testing
- [ ] Unit test coverage ≥ 80%
- [ ] All functional tests pass
- [ ] Integration tests pass
- [ ] Manual Qt testing complete
- [ ] Stress tests pass
- [ ] No memory leaks

### Quality
- [ ] Code reviewed
- [ ] Documentation complete
- [ ] No compiler warnings
- [ ] Logging comprehensive
- [ ] Error handling complete
- [ ] User-friendly error messages

### Security
- [ ] No private key leaks
- [ ] Signatures validated
- [ ] Amount overflow protected
- [ ] Double-spend prevented
- [ ] UTXO integrity maintained

---

**Remember**: TDD is not about writing tests. It's about **designing software through tests**. Each test is a specification. Each implementation is the solution. Each refactor is the polish.

**The TDD Mantra**:
- Make it fail (RED)
- Make it work (GREEN)
- Make it right (REFACTOR)
- Repeat until perfect ✨
