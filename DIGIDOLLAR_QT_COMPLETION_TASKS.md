# DigiDollar Qt Wallet Completion Task List

## Overview
This document outlines the tasks required to complete full DigiDollar functionality in the Qt wallet for RegTest testing. The implementation follows a three-phase approach with sub-agents handling specific components.

**Current Status:**
- ✅ Qt GUI widgets created (all 6 tabs)
- ✅ Mock data displays properly
- ⚠️ Backend wallet functionality incomplete
- ⚠️ Transaction creation/validation needs connection
- ⚠️ Oracle data needs mocking for RegTest
- ✅ RPC commands exist but need integration

## Phase 1: Qt Wallet Backend Integration (5 Sub-agents)

### Sub-Agent 1: Overview Tab Backend
**Objective:** Complete functionality for the Overview tab to display real DD balances and system status

**Tasks:**
1. Connect wallet backend to Overview widget
   - [ ] Implement `GetDigiDollarBalance()` to return actual wallet DD balance
   - [ ] Implement `GetLockedCollateral()` to return actual locked DGB
   - [ ] Implement `GetSystemHealth()` for real-time health metrics
   - [ ] Connect to blockchain for transaction history

2. Mock Oracle Integration
   - [ ] Create `MockOracleManager` class for RegTest
   - [ ] Implement `GetMockPrice()` returning configurable price (default: $0.01/DGB)
   - [ ] Add RPC command `setmockoracleprice` for testing price changes
   - [ ] Update price display in real-time

3. System Status Updates
   - [ ] Implement DCA status calculation
   - [ ] Implement ERR status monitoring
   - [ ] Calculate system-wide collateral ratio
   - [ ] Update health indicators based on actual data

4. Transaction History
   - [ ] Fetch recent DD transactions from wallet
   - [ ] Format transactions for display
   - [ ] Update on new blocks

**Files to Modify:**
- `src/qt/digidollaroverviewwidget.cpp`
- `src/wallet/digidollarwallet.cpp`
- `src/oracle/mock_oracle.cpp` (new)
- `src/rpc/digidollar.cpp`

**Success Criteria:**
- Overview shows real DD balance from wallet
- Oracle price updates via RPC
- Health metrics reflect actual system state
- Transaction history updates automatically

---

### Sub-Agent 2: Send Tab Backend
**Objective:** Enable sending DigiDollars to DD addresses with full validation

**Tasks:**
1. Transaction Creation
   - [ ] Implement `CreateSendTransaction()` in wallet backend
   - [ ] Connect to `TransferTxBuilder` for transaction construction
   - [ ] Implement UTXO selection for DD outputs
   - [ ] Calculate and add DGB fees

2. Address Validation
   - [ ] Validate DD addresses (DD/TD/RD prefixes)
   - [ ] Check address network compatibility
   - [ ] Verify recipient address is valid P2TR

3. Balance Verification
   - [ ] Check sufficient DD balance
   - [ ] Check sufficient DGB for fees
   - [ ] Reserve UTXOs during transaction creation
   - [ ] Update balance after sending

4. Transaction Broadcasting
   - [ ] Sign transaction with wallet keys
   - [ ] Broadcast to mempool
   - [ ] Monitor confirmation status
   - [ ] Handle broadcast errors

**Files to Modify:**
- `src/qt/digidollarsendwidget.cpp`
- `src/wallet/digidollarwallet.cpp`
- `src/digidollar/txbuilder.cpp`
- `src/validation.cpp`

**Success Criteria:**
- Can send DD to valid addresses
- Proper balance updates
- Transactions appear in mempool
- Confirmations tracked correctly

---

### Sub-Agent 3: Mint Tab Backend
**Objective:** Enable minting DigiDollars by locking DGB collateral

**Tasks:**
1. Collateral Calculation
   - [ ] Implement dynamic collateral calculation with mock oracle price
   - [ ] Apply 8-tier lock period ratios (500% to 200%)
   - [ ] Calculate DCA multiplier based on system health
   - [ ] Display required DGB in real-time

2. Mint Transaction Creation
   - [ ] Implement `CreateMintTransaction()` in wallet
   - [ ] Connect to `MintTxBuilder` for P2TR script creation
   - [ ] Create collateral output with CHECKLOCKTIMEVERIFY
   - [ ] Create DD output with proper witness data

3. Lock Period Management
   - [ ] Calculate unlock height from lock period selection
   - [ ] Store collateral position in wallet database
   - [ ] Track position ID for future redemption

4. Validation & Broadcasting
   - [ ] Verify sufficient DGB balance
   - [ ] Validate mint parameters
   - [ ] Sign and broadcast transaction
   - [ ] Update DD balance after confirmation

**Files to Modify:**
- `src/qt/digidollarmintwidget.cpp`
- `src/wallet/digidollarwallet.cpp`
- `src/digidollar/txbuilder.cpp`
- `src/digidollar/scripts.cpp`

**Success Criteria:**
- Can mint DD with all 8 lock tiers
- Collateral locked with proper timelock
- DD balance increases after minting
- Positions tracked in vault

---

### Sub-Agent 4: Redeem Tab Backend
**Objective:** Enable redeeming collateral by burning DigiDollars

**Tasks:**
1. Position Selection
   - [ ] Load user's collateral positions from wallet
   - [ ] Filter positions by redemption eligibility
   - [ ] Check timelock expiry status
   - [ ] Calculate ERR requirements if applicable

2. Redemption Transaction Creation
   - [ ] Implement `CreateRedemptionTransaction()` in wallet
   - [ ] Connect to `RedeemTxBuilder` for transaction construction
   - [ ] Support all 4 redemption paths (Normal, Emergency, Partial, ERR)
   - [ ] Burn required DD amount

3. Path-Specific Logic
   - [ ] Normal: Check timelock expired
   - [ ] Emergency: Mock 8-of-15 oracle signatures
   - [ ] Partial: Calculate proportional redemption
   - [ ] ERR: Apply emergency ratio if system <100%

4. Collateral Release
   - [ ] Unlock DGB from P2TR script
   - [ ] Update wallet balances
   - [ ] Remove/update position in database
   - [ ] Handle partial redemptions

**Files to Modify:**
- `src/qt/digidollarredeemwidget.cpp`
- `src/wallet/digidollarwallet.cpp`
- `src/digidollar/txbuilder.cpp`
- `src/consensus/err.cpp`

**Success Criteria:**
- Can redeem expired positions
- All 4 redemption paths work
- DGB returned to wallet
- DD properly burned

---

### Sub-Agent 5: Vault Tab Backend
**Objective:** Display and manage user's collateral positions with health monitoring

**Tasks:**
1. Position Loading
   - [ ] Fetch all positions from wallet database
   - [ ] Calculate real-time health for each position
   - [ ] Determine time remaining until unlock
   - [ ] Sort and filter positions

2. Health Calculation
   - [ ] Calculate current collateral ratio using mock oracle price
   - [ ] Apply health status thresholds (Healthy/Adequate/Warning/At Risk)
   - [ ] Update health bars (0-200% range)
   - [ ] Monitor for liquidation risk

3. Position Management
   - [ ] Enable position selection for redemption
   - [ ] Show detailed position information
   - [ ] Track position history
   - [ ] Export position data

4. Real-time Updates
   - [ ] Update on new blocks
   - [ ] Refresh on price changes
   - [ ] Monitor system health changes
   - [ ] Alert on critical conditions

**Files to Modify:**
- `src/qt/digidollarpositionswidget.cpp`
- `src/wallet/digidollarwallet.cpp`
- `src/digidollar/health.cpp`

**Success Criteria:**
- Shows all user positions
- Health updates in real-time
- Can select positions for redemption
- Accurate time remaining display

---

## Phase 2: Unit Test Updates (1 Sub-agent)

### Sub-Agent 6: C++ Unit Test Suite
**Objective:** Ensure all unit tests pass and cover new functionality

**Tasks:**
1. Fix Existing Test Failures
   - [ ] Review all `digidollar_*_tests.cpp` files
   - [ ] Fix static member definition issues
   - [ ] Update tests for mock oracle
   - [ ] Ensure RegTest compatibility

2. Add Missing Test Coverage
   - [ ] Test wallet DD balance tracking
   - [ ] Test position management
   - [ ] Test mock oracle functionality
   - [ ] Test all transaction types

3. Integration Tests
   - [ ] Test Qt widget to backend connections
   - [ ] Test RPC command integration
   - [ ] Test transaction lifecycle
   - [ ] Test wallet database operations

**Files to Modify:**
- `src/test/digidollar_*_tests.cpp` (all 21 files)
- `src/test/util/setup_common.cpp`

**Success Criteria:**
- All unit tests pass
- Coverage > 80%
- Mock oracle works in tests
- No memory leaks

---

## Phase 3: Functional Test Updates (1 Sub-agent)

### Sub-Agent 7: Python Functional Test Suite
**Objective:** Ensure functional tests validate end-to-end workflows

**Tasks:**
1. RegTest Environment Setup
   - [ ] Configure RegTest for DD activation at low height
   - [ ] Setup mock oracle for price feeds
   - [ ] Create test wallets with DD addresses
   - [ ] Mine past activation height

2. Workflow Tests
   - [ ] Test complete mint → send → redeem cycle
   - [ ] Test all 8 lock tiers
   - [ ] Test all 4 redemption paths
   - [ ] Test DCA and ERR scenarios

3. Qt Integration Tests
   - [ ] Test GUI interactions via RPC
   - [ ] Verify balance updates
   - [ ] Test error conditions
   - [ ] Validate transaction confirmations

**Files to Modify:**
- `test/functional/digidollar_*.py` (all 11 files)
- `test/functional/test_framework/test_framework.py`

**Success Criteria:**
- All functional tests pass
- Complete workflows validated
- Qt functionality verified
- RegTest fully operational

---

## Implementation Order & Dependencies

### Parallel Execution Plan
```
Phase 1 (Parallel - Max 5 agents):
├── Sub-Agent 1: Overview Tab
├── Sub-Agent 2: Send Tab
├── Sub-Agent 3: Mint Tab
├── Sub-Agent 4: Redeem Tab
└── Sub-Agent 5: Vault Tab

Phase 2 (Sequential - After Phase 1):
└── Sub-Agent 6: Unit Tests

Phase 3 (Sequential - After Phase 2):
└── Sub-Agent 7: Functional Tests
```

### Critical Dependencies
1. **Mock Oracle** (Sub-Agent 1) - Required by all other tabs
2. **Wallet Backend** - Shared by all tabs
3. **Transaction Builders** - Used by Send/Mint/Redeem
4. **RegTest Configuration** - Needed for all testing

---

## Mock Oracle Specification

### Implementation Details
```cpp
class MockOracleManager {
private:
    CAmount mockPrice = 1000000; // $0.01 per DGB default
    bool enabled = true;

public:
    // Get current mock price
    CAmount GetCurrentPrice() const;

    // Set mock price via RPC
    void SetMockPrice(CAmount price);

    // Simulate price volatility
    void SimulateVolatility(int percentChange);

    // Create mock oracle bundle for blocks
    COracleBundle CreateMockBundle(int height);
};
```

### RPC Commands
- `setmockoracleprice <price>` - Set mock oracle price
- `getmockoracleprice` - Get current mock price
- `simulatepricevolatility <percent>` - Simulate price change
- `enablemockoracle <true/false>` - Enable/disable mock oracle

---

## RegTest Configuration

### Activation Parameters
```cpp
// In chainparams.cpp for RegTest
consensus.DigiDollarHeight = 650; // Activate at block 650
consensus.defaultOraclePrice = 1000000; // $0.01 per DGB
consensus.allowMockOracle = true; // Enable mock oracle
```

### Complete RegTest Testing Workflow

#### Initial Setup
1. Start RegTest node:
   ```bash
   ./digibyte-qt -regtest -digidollar=1 -server -rpcuser=test -rpcpassword=test
   ```

2. Mine blocks to activate DigiDollar:
   ```bash
   ./digibyte-cli -regtest generatetoaddress 650 $(./digibyte-cli -regtest getnewaddress)
   ```

3. Verify DigiDollar activation:
   ```bash
   ./digibyte-cli -regtest getdigidollardeploymentinfo
   ```

4. Set mock oracle price:
   ```bash
   ./digibyte-cli -regtest setmockoracleprice 1000000  # $0.01 per DGB
   ```

#### Wallet Preparation
5. Generate DD addresses for testing:
   ```bash
   # Create sender wallet
   ./digibyte-cli -regtest getdigidollaraddress "sender"

   # Create receiver wallet
   ./digibyte-cli -regtest getdigidollaraddress "receiver"
   ```

6. Fund wallet with DGB for collateral:
   ```bash
   ./digibyte-cli -regtest generatetoaddress 10 $(./digibyte-cli -regtest getnewaddress)
   ```

#### Testing Each Tab

##### Overview Tab Testing
- Verify DD balance shows 0 initially
- Verify oracle price displays correctly
- Check system health indicators
- Confirm DCA status shows "Normal"
- Ensure transaction history is empty

##### Mint Tab Testing
1. Select lock period (test each tier):
   - 30 days: 500% collateral
   - 3 months: 400% collateral
   - 6 months: 350% collateral
   - 1 year: 300% collateral
   - 3 years: 250% collateral
   - 5 years: 225% collateral
   - 7 years: 212% collateral
   - 10 years: 200% collateral

2. Enter DD amount to mint (e.g., 1000 DD = $10)
3. Verify collateral calculation is correct
4. Click Mint button
5. Confirm transaction in mempool
6. Mine 1 block to confirm
7. Verify DD balance increases

##### Send Tab Testing
1. Enter receiver's DD address
2. Enter amount to send (e.g., 500 DD)
3. Verify fee estimation
4. Click Send button
5. Confirm transaction
6. Mine 1 block
7. Check balance updates on both sender and receiver

##### Vault Tab Testing
1. Verify minted position appears
2. Check health calculation (should be ~100% at mint)
3. Verify time remaining display
4. Test sorting by columns
5. Check position details dialog

##### Redeem Tab Testing (Time-locked)
1. For testing immediate redemption:
   ```bash
   # Generate blocks to pass timelock (e.g., for 30-day lock)
   ./digibyte-cli -regtest generatetoaddress 172800 $(./digibyte-cli -regtest getnewaddress)
   ```
2. Select expired position
3. Choose Normal redemption path
4. Verify required DD amount
5. Click Redeem button
6. Mine 1 block
7. Verify DGB returned and DD burned

#### Price Volatility Testing
1. Change oracle price to simulate volatility:
   ```bash
   # Increase price by 50%
   ./digibyte-cli -regtest setmockoracleprice 1500000
   ```

2. Check vault health updates (should show over-collateralized)

3. Decrease price to trigger warnings:
   ```bash
   # Decrease price by 80%
   ./digibyte-cli -regtest setmockoracleprice 200000
   ```

4. Verify health warnings appear

#### DCA Testing
1. Simulate system stress by minting large amounts
2. Check DCA multiplier increases
3. Verify new mints require more collateral

#### ERR Testing
1. Set system collateral below 100%:
   ```bash
   # This would require minting lots of DD and crashing price
   # Or use special test RPC command if implemented
   ./digibyte-cli -regtest setsystemcollateral 95
   ```

2. Try redemption and verify ERR applies
3. Check that more DD is required for redemption

#### Complete Cycle Test
1. Start fresh RegTest
2. Mine to block 650
3. Mint 10000 DD with 1-year lock
4. Send 5000 DD to another address
5. Change oracle price
6. Check vault health
7. Mine blocks to pass timelock
8. Redeem partial amount
9. Redeem remaining amount
10. Verify all balances correct

---

## RegTest Helper Functions for Sub-Agents

### Mock Oracle Manager Implementation
Each sub-agent should ensure the mock oracle singleton is available:

```cpp
// src/oracle/mock_oracle.cpp
class MockOracleManager {
private:
    static MockOracleManager* instance;
    CAmount mockPrice;
    int64_t lastUpdateHeight;
    mutable CCriticalSection cs_price;

    MockOracleManager() : mockPrice(1000000), lastUpdateHeight(0) {}

public:
    static MockOracleManager& GetInstance() {
        if (!instance) {
            instance = new MockOracleManager();
        }
        return *instance;
    }

    CAmount GetCurrentPrice() const {
        LOCK(cs_price);
        return mockPrice;
    }

    void SetMockPrice(CAmount newPrice) {
        LOCK(cs_price);
        mockPrice = newPrice;
        lastUpdateHeight = ::ChainActive().Height();
        LogPrintf("Mock oracle price updated to %d at height %d\n", newPrice, lastUpdateHeight);
    }

    COracleBundle CreateMockBundle(int height) {
        COracleBundle bundle;
        bundle.nHeight = height;
        bundle.nPrice = GetCurrentPrice();
        bundle.nTimestamp = GetTime();
        // Mock 8 of 15 signatures
        for (int i = 0; i < 8; i++) {
            bundle.vSignatures.push_back(std::vector<unsigned char>(64, 0)); // Mock signature
        }
        return bundle;
    }
};

// Global instance pointer
MockOracleManager* MockOracleManager::instance = nullptr;
```

### RegTest Block Mining Helper
```cpp
// Helper function for quick block generation
bool MineBlocksForTesting(int numBlocks) {
    if (!Params().IsRegTest()) {
        return false;
    }

    // Get a new address for coinbase
    CTxDestination dest;
    if (!pwallet->GetNewDestination(OutputType::LEGACY, "", dest, error)) {
        return false;
    }

    // Generate blocks
    for (int i = 0; i < numBlocks; i++) {
        std::unique_ptr<CBlockTemplate> pblocktemplate(
            BlockAssembler(chainman.ActiveChainstate(), *node.mempool)
                .CreateNewBlock(GetScriptForDestination(dest)));

        if (!pblocktemplate) {
            return false;
        }

        CBlock* pblock = &pblocktemplate->block;
        GenerateBlock(chainman, pblock);
    }

    return true;
}
```

### Fast Time-lock Testing
For testing redemptions without waiting for actual timelock:

```cpp
// src/digidollar/validation.cpp - Add test-only bypass
bool BypassTimelockForTesting(const CTransaction& tx) {
    if (!Params().IsRegTest()) {
        return false;
    }

    // Check for special test flag in transaction
    if (tx.nVersion & 0x80000000) { // High bit set = test mode
        LogPrintf("REGTEST: Bypassing timelock for testing\n");
        return true;
    }

    return false;
}
```

### System Health Manipulation for Testing
```cpp
// RPC command for testing DCA/ERR scenarios
static RPCHelpMan setsystemhealth()
{
    return RPCHelpMan{"setsystemhealth",
        "\nSet system health for testing (RegTest only)\n",
        {
            {"collateral_ratio", RPCArg::Type::NUM, RPCArg::Optional::NO, "System collateral ratio (50-200)"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "collateral_ratio", "New system collateral ratio"},
                {RPCResult::Type::STR, "dca_status", "DCA status (Normal/Stressed/Warning/Critical)"},
                {RPCResult::Type::BOOL, "err_active", "Whether ERR is active"},
            }
        },
        RPCExamples{
            HelpExampleCli("setsystemhealth", "95") +
            HelpExampleRpc("setsystemhealth", "95")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            if (!Params().IsRegTest()) {
                throw JSONRPCError(RPC_MISC_ERROR, "This command is only available in RegTest");
            }

            int ratio = request.params[0].get_int();
            if (ratio < 50 || ratio > 200) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Collateral ratio must be between 50 and 200");
            }

            // Update mock system health
            SystemHealthMonitor::GetInstance().SetMockCollateralRatio(ratio);

            UniValue result(UniValue::VOBJ);
            result.pushKV("collateral_ratio", ratio);
            result.pushKV("dca_status", GetDCAStatusString(ratio));
            result.pushKV("err_active", ratio < 100);

            return result;
        }
    };
}
```

### DD Balance Initialization for Testing
```cpp
// Quick DD balance setup for testing
static RPCHelpMan mintddfortest()
{
    return RPCHelpMan{"mintddfortest",
        "\nQuickly mint DD for testing without collateral (RegTest only)\n",
        {
            {"amount", RPCArg::Type::AMOUNT, RPCArg::Optional::NO, "DD amount to create"},
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "DD address to receive"},
        },
        RPCResult{
            RPCResult::Type::STR_HEX, "txid", "Transaction ID"
        },
        RPCExamples{
            HelpExampleCli("mintddfortest", "1000 RD1q2c3d4e5...")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            if (!Params().IsRegTest()) {
                throw JSONRPCError(RPC_MISC_ERROR, "This command is only available in RegTest");
            }

            CAmount amount = AmountFromValue(request.params[0]);
            std::string address = request.params[1].get_str();

            // Create test DD without collateral requirement
            // This bypasses normal minting for testing only
            CMutableTransaction mtx;
            mtx.nVersion = MakeDigiDollarVersion(DD_TX_MINT, 0, 2);

            // Add dummy input
            mtx.vin.push_back(CTxIn(COutPoint(), CScript()));

            // Create DD output
            CTxDestination dest = DecodeDigiDollarAddress(address);
            CScript ddScript = GetScriptForDestination(dest);
            mtx.vout.push_back(CTxOut(0, ddScript)); // 0 DGB value, DD in witness

            // Sign and send
            uint256 txid = mtx.GetHash();

            return txid.GetHex();
        }
    };
}
```

---

## Success Metrics

### Phase 1 Completion
- [ ] All 5 Qt tabs fully functional
- [ ] Can mint DD with mock collateral
- [ ] Can send DD between addresses
- [ ] Can redeem collateral after timelock
- [ ] Vault shows all positions with health

### Phase 2 Completion
- [ ] All unit tests pass
- [ ] Test coverage > 80%
- [ ] No compilation warnings

### Phase 3 Completion
- [ ] All functional tests pass
- [ ] Complete workflows validated
- [ ] RegTest fully operational

### Overall Success
- [ ] Qt wallet compiles without errors
- [ ] Full DD functionality in RegTest
- [ ] Ready for testnet deployment

---

## Notes for Orchestrator

1. **Sub-agents can work in parallel** during Phase 1 (max 5 concurrent)
2. **Mock oracle must be implemented first** as it's a dependency
3. **Each sub-agent should create tests** for their functionality
4. **Use existing code** from `src/digidollar/` where possible
5. **Maintain backward compatibility** with existing DigiByte functionality
6. **Document all new RPC commands** added for testing
7. **Update relevant GUI labels** to reflect actual functionality
8. **Ensure thread safety** for wallet operations
9. **Log all DD operations** for debugging
10. **Create example transactions** for testing

---

## Estimated Timeline

- **Phase 1**: 3-4 days (with 5 parallel agents)
- **Phase 2**: 1-2 days
- **Phase 3**: 1-2 days
- **Total**: 5-8 days for complete implementation

---

## Post-Completion Tasks

After all phases complete:
1. Create user documentation
2. Record demo video
3. Prepare testnet deployment plan
4. Document known limitations
5. Create integration guide for exchanges

---

*This task list is designed for systematic completion of DigiDollar Qt wallet functionality using parallel sub-agents for maximum efficiency.*