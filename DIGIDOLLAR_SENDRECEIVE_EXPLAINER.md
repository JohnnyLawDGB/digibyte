# DigiDollar Send/Receive Architecture Explainer

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Data Flow](#data-flow)
4. [Component Details](#component-details)
5. [Transaction Lifecycle](#transaction-lifecycle)
6. [Integration Points](#integration-points)
7. [Error Handling](#error-handling)
8. [Security Considerations](#security-considerations)

## Overview

### What Is Send/Receive?
Send/Receive is the core functionality that allows DigiDollar users to transfer DD tokens between wallets. It consists of:

- **Send**: Creating and broadcasting a transaction that transfers DD from your wallet to another address
- **Receive**: Detecting incoming DD transactions and crediting your wallet balance

### Key Components
```
┌─────────────────────────────────────────────────────┐
│                 DigiDollar Send/Receive             │
│                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │ Coin         │  │ Transaction  │  │ Broadcast │ │
│  │ Selection    │→ │ Building     │→ │ & Confirm │ │
│  └──────────────┘  └──────────────┘  └───────────┘ │
│         ↓                  ↓                ↓       │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │ UTXO         │  │ Signing      │  │ Balance   │ │
│  │ Tracking     │  │ & Witness    │  │ Updates   │ │
│  └──────────────┘  └──────────────┘  └───────────┘ │
│                                                     │
│         Persistence Layer (wallet.dat)             │
│  ┌─────────────────────────────────────────────┐   │
│  │  Positions │ Balances │ Transactions │ UTXOs│   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

## Architecture

### Layer Architecture

```
┌─────────────────────────────────────────────────┐
│              User Interface Layer               │
│  ┌─────────────────────────────────────────┐   │
│  │  Qt Wallet (digidollarsendwidget.cpp)   │   │
│  │  - Send dialog                          │   │
│  │  - Address validation                   │   │
│  │  - Amount input                         │   │
│  │  - Transaction confirmation             │   │
│  └─────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│           Wallet Model Layer                    │
│  ┌─────────────────────────────────────────┐   │
│  │  WalletModel (walletmodel.cpp)          │   │
│  │  - sendDigiDollar()                     │   │
│  │  - getDigiDollarBalance()               │   │
│  │  - validateDigiDollarAddress()          │   │
│  └─────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│         DigiDollar Wallet Layer                 │
│  ┌─────────────────────────────────────────┐   │
│  │  DigiDollarWallet (digidollarwallet.cpp)│   │
│  │  - TransferDigiDollar()                 │   │
│  │  - SelectDDCoins()                      │   │
│  │  - GetDDUTXOs()                         │   │
│  │  - UpdateBalance()                      │   │
│  └─────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│        Transaction Builder Layer                │
│  ┌─────────────────────────────────────────┐   │
│  │  TransferTxBuilder (txbuilder.cpp)      │   │
│  │  - BuildTransferTransaction()           │   │
│  │  - SelectCoins()                        │   │
│  │  - CreateOutputs()                      │   │
│  │  - CalculateFees()                      │   │
│  └─────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│           Persistence Layer                     │
│  ┌─────────────────────────────────────────┐   │
│  │  WalletBatch (walletdb.cpp)             │   │
│  │  - WriteDDTransaction()                 │   │
│  │  - WritePosition()                      │   │
│  │  - WriteDDBalance()                     │   │
│  └─────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│          Network & Validation Layer             │
│  ┌─────────────────────────────────────────┐   │
│  │  - Mempool submission                   │   │
│  │  - Network propagation                  │   │
│  │  - Block validation                     │   │
│  │  - Confirmation tracking                │   │
│  └─────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
```

## Data Flow

### Send Transaction Flow

```
User Action: Click "Send 100 DD to DD1abc..."
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 1. Qt Send Dialog                               │
│    - Validate address format (Base58, DD prefix)│
│    - Check amount > 0                           │
│    - Verify balance sufficient                  │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 2. WalletModel::sendDigiDollar()                │
│    - Create DigiDollarWallet instance           │
│    - Call TransferDigiDollar()                  │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 3. DigiDollarWallet::TransferDigiDollar()       │
│    - Validate parameters                        │
│    - Select DD coins (SelectDDCoins)            │
│    - Select fee coins (SelectFeeCoins)          │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 4. Coin Selection                               │
│    - GetDDUTXOs() from active positions         │
│    - Greedy selection: pick UTXOs until target  │
│    - Calculate change amount                    │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 5. TransferTxBuilder::BuildTransferTransaction()│
│    - Create inputs from selected UTXOs          │
│    - Create DD output to recipient              │
│    - Create DD change output (if needed)        │
│    - Add fee inputs                             │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 6. Sign Transaction                             │
│    - Sign DD inputs with position owner key     │
│    - Sign fee inputs with wallet key            │
│    - Build witness data (P2TR)                  │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 7. Broadcast Transaction                        │
│    - Submit to mempool                          │
│    - Relay to network                           │
│    - Return txid to user                        │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 8. Update Wallet State                          │
│    - Mark spent UTXOs                           │
│    - Reduce balance                             │
│    - Save transaction to database               │
│    - Update Qt UI                               │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 9. Wait for Confirmation                        │
│    - Monitor mempool                            │
│    - Track confirmations on new blocks          │
│    - Update transaction status                  │
└─────────────────────────────────────────────────┘
```

### Receive Transaction Flow

```
Incoming Transaction in Mempool
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 1. Transaction Detection                        │
│    - Scan mempool for transactions              │
│    - Check outputs for our DD addresses         │
│    - Identify DD outputs by script pattern      │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 2. Transaction Validation                       │
│    - Verify DD transaction structure            │
│    - Validate DD marker                         │
│    - Check amounts and scripts                  │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 3. UTXO Addition                                │
│    - Add new DD UTXO to spendable set           │
│    - Record output amount                       │
│    - Mark as unconfirmed                        │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 4. Balance Update                               │
│    - Increase DD balance                        │
│    - Recalculate total balance                  │
│    - Update Qt UI                               │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 5. Transaction History                          │
│    - Create DDTransaction record                │
│    - Set category = "receive"                   │
│    - Save to database (wallet.dat)              │
└─────────────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────┐
│ 6. Confirmation Tracking                        │
│    - On new block: check if tx included         │
│    - Increment confirmation count               │
│    - Update UTXO status (0 conf → 1 conf → ...) │
└─────────────────────────────────────────────────┘
```

## Component Details

### 1. Coin Selection (SelectDDCoins)

**Purpose**: Select which DD UTXOs to spend for a transaction

**Algorithm** (Greedy Selection):
```cpp
bool DigiDollarWallet::SelectDDCoins(
    const CAmount& target_amount,
    std::vector<COutPoint>& selected_utxos,
    CAmount& selected_total) const
{
    selected_total = 0;
    selected_utxos.clear();

    // Get all spendable DD UTXOs
    std::vector<DDUtxo> utxos = GetDDUTXOs();

    // Sort by amount (smallest first for better privacy)
    std::sort(utxos.begin(), utxos.end(),
              [](const auto& a, const auto& b) {
                  return a.dd_amount < b.dd_amount;
              });

    // Greedy selection
    for (const auto& utxo : utxos) {
        if (selected_total >= target_amount) break;

        selected_utxos.push_back(utxo.outpoint);
        selected_total += utxo.dd_amount;
    }

    return selected_total >= target_amount;
}
```

**Why Greedy?**
- Simple and fast
- Privacy: uses smaller UTXOs first (less fingerprinting)
- Minimizes change outputs

### 2. Transaction Building (TransferTxBuilder)

**Purpose**: Construct a valid DD transfer transaction

**Structure**:
```
Transfer Transaction:
├── Inputs
│   ├── DD Input 1 (from position A, output index 1)
│   ├── DD Input 2 (from position B, output index 1)
│   └── DGB Fee Input (from wallet UTXO)
├── Outputs
│   ├── DD Output (to recipient address)
│   ├── DD Change (back to sender, if needed)
│   └── DGB Change (back to sender, if needed)
└── Witnesses
    ├── DD Input 1 Witness (P2TR signature)
    ├── DD Input 2 Witness (P2TR signature)
    └── Fee Input Witness (P2TR/P2WPKH signature)
```

**Example Transaction**:
```
Transaction ID: abc123...
Inputs:
  [0] pos_xyz:1 (DD input, 150 DD)
  [1] pos_def:1 (DD input, 100 DD)
  [2] utxo_ghi:0 (DGB fee input, 1 DGB)

Outputs:
  [0] DD1recipient... (200 DD) ← to receiver
  [1] DD1sender...    (50 DD)  ← change back to sender
  [2] dgb1sender...   (0.999 DGB) ← DGB change (1 DGB - 0.001 fee)
```

### 3. Transaction Signing

**Purpose**: Create valid signatures for all inputs

**P2TR (Taproot) Signing**:
```cpp
bool SignDDInput(CMutableTransaction& tx, int input_index, const CKey& key) {
    // Get sighash for this input
    uint256 sighash = CalculateDDInputSighash(tx, input_index);

    // Sign with Schnorr (BIP340)
    std::vector<unsigned char> sig;
    key.SignSchnorr(sighash, sig);

    // Build witness: [signature]
    tx.vin[input_index].scriptWitness.stack.clear();
    tx.vin[input_index].scriptWitness.stack.push_back(sig);

    return true;
}
```

**Signature Verification**:
```cpp
bool VerifyDDSignature(const CTransaction& tx, int input_index) {
    // Extract signature from witness
    const auto& witness = tx.vin[input_index].scriptWitness;
    if (witness.stack.empty()) return false;

    const auto& sig = witness.stack[0];

    // Get public key from scriptPubKey
    CPubKey pubkey = ExtractPubKeyFromScript(prevout.scriptPubKey);

    // Verify Schnorr signature
    uint256 sighash = CalculateDDInputSighash(tx, input_index);
    return pubkey.VerifySchnorr(sighash, sig);
}
```

### 4. Broadcasting

**Purpose**: Submit transaction to network

**Flow**:
1. **Local Validation**: Check transaction structure
2. **Mempool Submission**: Add to local mempool
3. **Network Relay**: Send `inv` messages to peers
4. **Peer Validation**: Other nodes validate and relay
5. **Miner Inclusion**: Transaction included in next block

**Code**:
```cpp
bool BroadcastDDTransaction(const CTransactionRef& tx) {
    // Validate locally
    TxValidationState state;
    if (!AcceptToMemoryPool(state, tx)) {
        return false;
    }

    // Relay to network
    RelayTransaction(tx->GetHash());

    return true;
}
```

### 5. Balance Updates

**Purpose**: Keep wallet balance accurate

**Update Triggers**:
- ✅ After sending (reduce balance)
- ✅ After receiving (increase balance)
- ✅ On new block (update confirmations)
- ✅ On reorg (revert invalid transactions)

**Balance Calculation**:
```cpp
CAmount DigiDollarWallet::GetTotalDDBalance() const {
    CAmount total = 0;

    // Sum all spendable DD UTXOs
    std::vector<DDUtxo> utxos = GetDDUTXOs();
    for (const auto& utxo : utxos) {
        if (utxo.is_spendable && utxo.confirmations >= MIN_CONFIRMATIONS) {
            total += utxo.dd_amount;
        }
    }

    return total;
}
```

**Database Update**:
```cpp
bool DigiDollarWallet::UpdateBalanceAfterSend(const CAmount& sent_amount) {
    // Recalculate balance from UTXO set
    CAmount new_balance = GetTotalDDBalance();

    // Persist to database
    WalletBatch batch(m_wallet->GetDatabase());
    if (!batch.WriteDDBalance(GetAddress(), new_balance)) {
        return false;
    }

    // Update in-memory cache
    dd_balances[GetAddress()] = new_balance;

    return true;
}
```

## Transaction Lifecycle

### Complete Send/Receive Cycle

```
TIME  | SENDER WALLET              | NETWORK              | RECEIVER WALLET
------+----------------------------+----------------------+---------------------------
T+0   | User: "Send 100 DD"        |                      |
      | SelectCoins: pos1 (150 DD) |                      |
      | Build Tx: 150 DD input     |                      |
      |           100 DD output    |                      |
      |           50 DD change     |                      |
      | Sign inputs                |                      |
------+----------------------------+----------------------+---------------------------
T+1   | Broadcast tx (abc123)      | → Mempool ←          | Detect incoming tx
      | Balance: 500 → 400 DD      |   [abc123]           | Unconfirmed: +100 DD
      | Mark pos1 spent            |                      | Add UTXO: abc123:0
------+----------------------------+----------------------+---------------------------
T+15s |                            | Block N mined        |
      |                            | [abc123 included]    |
------+----------------------------+----------------------+---------------------------
T+16s | Conf: 0 → 1                | ← Confirmed →        | Conf: 0 → 1
      | UTXO abc123:1 confirmed    |                      | UTXO abc123:0 confirmed
      | (50 DD change spendable)   |                      | (100 DD spendable)
------+----------------------------+----------------------+---------------------------
```

### UTXO State Transitions

```
Position Created (Mint):
┌──────────────────────────────────────────┐
│  Position: pos1                          │
│  Output 0: 1000 DGB (collateral)         │
│  Output 1: 500 DD (spendable) ←───┐      │
└──────────────────────────────────────────┘
                                      │
                                 (Track as UTXO)
                                      │
                                      ↓
Send Transaction (Partial Spend):
┌──────────────────────────────────────────┐
│  Input:  pos1:1 (500 DD) ← Spent         │
│  Output: DD1recipient (300 DD)           │
│  Output: DD1sender (200 DD) ←───┐        │
└──────────────────────────────────────────┘
                                      │
                                 (New UTXO)
                                      │
                                      ↓
New Spendable UTXO:
┌──────────────────────────────────────────┐
│  Transaction: tx_abc                     │
│  Output 1: 200 DD (spendable) ←───┐      │
└──────────────────────────────────────────┘
                                      │
                                 (Continue cycle)
```

## Integration Points

### With Persistence Layer
```cpp
// After successful send
WalletBatch batch(database);

// Save transaction
DDTransaction tx;
tx.txid = transaction->GetHash();
tx.amount = sent_amount;
tx.category = "send";
batch.WriteDDTransaction(tx);

// Update balance
batch.WriteDDBalance(address, new_balance);

// Mark UTXOs spent
for (const auto& spent : spent_utxos) {
    batch.EraseDDOutput(spent);
}
```

### With Qt Interface
```cpp
// In WalletModel::sendDigiDollar()
DigiDollarWallet ddWallet;

// Perform send
std::string txid, error;
bool success = ddWallet.TransferDigiDollar(address, amount, txid, error);

if (success) {
    // Update UI
    Q_EMIT balanceChanged();
    Q_EMIT transactionCreated(txid);
} else {
    // Show error
    Q_EMIT message(tr("Send Failed"), QString::fromStdString(error));
}
```

### With RPC Interface
```cpp
// RPC: transferdigidollar
UniValue transferdigidollar(const JSONRPCRequest& request) {
    // Get wallet
    std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
    DigiDollarWallet ddWallet(wallet);

    // Parse parameters
    std::string address = request.params[0].get_str();
    CAmount amount = AmountFromValue(request.params[1]);

    // Execute transfer
    std::string txid, error;
    if (!ddWallet.TransferDigiDollar(address, amount, txid, error)) {
        throw JSONRPCError(RPC_WALLET_ERROR, error);
    }

    // Return result
    UniValue result(UniValue::VOBJ);
    result.pushKV("txid", txid);
    return result;
}
```

## Error Handling

### Error Categories

#### 1. Validation Errors
```cpp
if (amount <= 0) {
    error = "Send amount must be positive";
    return false;
}

if (!IsValidDDAddress(address)) {
    error = "Invalid DigiDollar address format";
    return false;
}

if (amount > GetBalance()) {
    error = strprintf("Insufficient balance: have %d, need %d",
                      GetBalance(), amount);
    return false;
}
```

#### 2. Coin Selection Errors
```cpp
if (!SelectDDCoins(amount, utxos, total)) {
    error = strprintf("Insufficient DD UTXOs: need %d, have %d",
                      amount, total);
    return false;
}

if (!SelectFeeCoins(fee, fee_utxos, fee_total)) {
    error = "Insufficient DGB for transaction fees";
    return false;
}
```

#### 3. Transaction Building Errors
```cpp
TxBuilderResult result = builder.BuildTransferTransaction(params);
if (!result.success) {
    error = strprintf("Transaction build failed: %s", result.error);
    return false;
}
```

#### 4. Signing Errors
```cpp
if (!SignDDInputs(tx, key)) {
    error = "Failed to sign DD inputs";
    return false;
}

if (!VerifySignatures(tx)) {
    error = "Signature verification failed";
    return false;
}
```

#### 5. Broadcasting Errors
```cpp
TxValidationState state;
if (!AcceptToMemoryPool(state, tx)) {
    error = strprintf("Mempool rejection: %s", state.GetRejectReason());
    return false;
}
```

### Error Recovery

```cpp
bool DigiDollarWallet::TransferDigiDollar(/*...*/) {
    // Capture initial state
    auto checkpoint = CaptureWalletState();

    try {
        // Attempt transaction
        if (!BuildAndBroadcastTx(/*...*/)) {
            // Restore state on failure
            RestoreWalletState(checkpoint);
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        // Restore on exception
        RestoreWalletState(checkpoint);
        error = e.what();
        return false;
    }
}
```

## Security Considerations

### 1. Private Key Protection
```cpp
// ✅ CORRECT: Never log private keys
LogPrintf("Signing with public key: %s\n", pubkey.ToString());

// ❌ WRONG: Never do this!
// LogPrintf("Using private key: %s\n", privkey.ToString());
```

### 2. Amount Validation
```cpp
// Prevent overflow
if (amount > MAX_DD_AMOUNT) {
    return false;
}

// Prevent negative amounts
if (amount < 0) {
    return false;
}

// Check addition overflow
if (selected_total > MAX_DD_AMOUNT - utxo.amount) {
    // Would overflow
    return false;
}
```

### 3. UTXO Locking
```cpp
// Lock UTXOs during transaction building
std::vector<COutPoint> locked_utxos;
for (const auto& utxo : selected_utxos) {
    wallet->LockCoin(utxo);
    locked_utxos.push_back(utxo);
}

// Always unlock on completion/failure
BOOST_SCOPE_EXIT(&wallet, &locked_utxos) {
    for (const auto& utxo : locked_utxos) {
        wallet->UnlockCoin(utxo);
    }
} BOOST_SCOPE_EXIT_END
```

### 4. Double-Spend Prevention
```cpp
// Check UTXO not already spent
if (IsUTXOSpent(utxo)) {
    error = "UTXO already spent";
    return false;
}

// Mark as spent atomically
if (!MarkUTXOSpent(utxo)) {
    error = "Failed to mark UTXO spent";
    return false;
}
```

## Testing Strategy

### Unit Tests
- Coin selection algorithms
- Transaction building logic
- Signature generation
- Balance calculations

### Functional Tests
- End-to-end send/receive
- Multi-node scenarios
- Network propagation
- Confirmation tracking

### Integration Tests
- With persistence layer
- With Qt interface
- With RPC interface
- Across wallet restarts

### Manual Tests
- Qt wallet send flow
- Error message display
- Balance updates
- Transaction history

---

## Summary

DigiDollar Send/Receive is a multi-layered system that:

1. **Selects UTXOs** from active positions
2. **Builds transactions** with proper inputs/outputs
3. **Signs inputs** using P2TR (Taproot) signatures
4. **Broadcasts** to the network
5. **Tracks confirmations** and updates state
6. **Persists everything** to wallet.dat
7. **Updates UI** in real-time

**Key Principle**: Every operation is tested, every error is handled, and every state change is persisted. This ensures users can trust the wallet with their DigiDollars.
