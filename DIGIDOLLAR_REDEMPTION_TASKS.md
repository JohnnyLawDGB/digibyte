# DigiDollar Redemption - Complete Task Details

## Task 1: Redemption Eligibility Checker

### Goal
Create system to check if a collateral position can be redeemed

### Files
- **CREATE**: `src/digidollar/redemption.h`, `src/digidollar/redemption.cpp`
- **MODIFY**: `src/wallet/digidollarwallet.cpp/h`

### Implementation

```cpp
// src/digidollar/redemption.h
#ifndef DIGIDOLLAR_REDEMPTION_H
#define DIGIDOLLAR_REDEMPTION_H

#include <amount.h>
#include <primitives/transaction.h>

struct RedemptionEligibility {
    bool canRedeem;
    int blocksRemaining;
    CAmount requiredDD;
    CAmount willUnlockDGB;
    std::string statusMessage;
};

class DigiDollarRedemption {
public:
    RedemptionEligibility CheckEligibility(
        const CCollateralPosition& position,
        CAmount partialAmount = 0
    );

private:
    bool IsTimelockExpired(const CCollateralPosition& position);
    CAmount CalculateRequiredDD(const CCollateralPosition& position);
};

#endif
```

```cpp
// src/digidollar/redemption.cpp
#include <digidollar/redemption.h>
#include <chain.h>
#include <consensus/err.h>

bool DigiDollarRedemption::IsTimelockExpired(const CCollateralPosition& position) {
    return ::ChainActive().Height() >= position.unlockHeight;
}

CAmount DigiDollarRedemption::CalculateRequiredDD(const CCollateralPosition& position) {
    // Use ERR module (placeholder returns 1:1)
    return ERR::CalculateRequirement(position.ddMinted);
}

RedemptionEligibility DigiDollarRedemption::CheckEligibility(
    const CCollateralPosition& position,
    CAmount partialAmount)
{
    RedemptionEligibility result;
    result.requiredDD = position.ddMinted;
    result.willUnlockDGB = position.dgbLocked;
    result.blocksRemaining = 0;

    // CRITICAL: Check timelock first
    if (!IsTimelockExpired(position)) {
        result.canRedeem = false;
        result.blocksRemaining = position.unlockHeight - ::ChainActive().Height();
        result.statusMessage = strprintf("Timelock active: %d blocks remaining", result.blocksRemaining);
        return result;
    }

    // Timelock expired - calculate requirements
    result.canRedeem = true;
    result.requiredDD = CalculateRequiredDD(position);

    // Handle partial redemption
    if (partialAmount > 0 && partialAmount < position.ddMinted) {
        result.requiredDD = partialAmount;
        result.willUnlockDGB = (position.dgbLocked * partialAmount) / position.ddMinted;
        result.statusMessage = "Partial redemption available";
    } else {
        result.statusMessage = "Ready to redeem";
    }

    return result;
}
```

### Test
```python
# test/functional/test_digidollar_redeem.py
def test_eligibility(self):
    mint = self.nodes[0].mintdigidollar(1000, "1hour")

    # Before timelock - cannot redeem
    info = self.nodes[0].getredemptioninfo(mint['collateral_outpoint'])
    assert not info['can_redeem']
    assert info['blocks_remaining'] > 0

    # After timelock - can redeem
    self.nodes[0].generate(240)
    info = self.nodes[0].getredemptioninfo(mint['collateral_outpoint'])
    assert info['can_redeem']
    assert info['blocks_remaining'] == 0
```

---

## Task 2: Redemption Transaction Builder

### Goal
Build redemption transaction with correct structure

### Files
- **MODIFY**: `src/digidollar/txbuilder.cpp/h`

### Implementation

```cpp
// Add to src/digidollar/txbuilder.h
class DigiDollarTxBuilder {
public:
    // Existing methods...

    bool CreateRedemptionTx(
        const CCollateralPosition& position,
        CAmount partialAmount,
        CMutableTransaction& txOut,
        std::string& error
    );
};
```

```cpp
// src/digidollar/txbuilder.cpp
bool DigiDollarTxBuilder::CreateRedemptionTx(
    const CCollateralPosition& position,
    CAmount partialAmount,
    CMutableTransaction& tx,
    std::string& error)
{
    // Set transaction version
    tx.nVersion = DD_TX_VERSION | (DD_TX_REDEEM << 16);

    // CRITICAL: Set locktime for OP_CHECKLOCKTIMEVERIFY
    tx.nLockTime = position.unlockHeight;

    // Calculate amounts
    CAmount ddToBurn = (partialAmount > 0 && partialAmount < position.ddMinted)
        ? partialAmount
        : position.ddMinted;
    CAmount dgbToUnlock = (position.dgbLocked * ddToBurn) / position.ddMinted;

    // Input 0: Collateral UTXO (MUST be first)
    tx.vin.push_back(CTxIn(position.outpoint));

    // Input 1+: DD UTXOs to burn
    std::vector<COutput> ddUTXOs;
    if (!SelectDDInputs(ddToBurn, ddUTXOs)) {
        error = "Insufficient DD balance";
        return false;
    }
    for (const auto& utxo : ddUTXOs) {
        tx.vin.push_back(CTxIn(utxo.GetOutPoint()));
    }

    // Output 0: Unlocked DGB
    CTxDestination dest = m_wallet->GetNewDestination(OutputType::BECH32);
    tx.vout.push_back(CTxOut(dgbToUnlock, GetScriptForDestination(dest)));

    // Output 1 (optional): New collateral if partial
    if (ddToBurn < position.ddMinted) {
        CAmount remainingDGB = position.dgbLocked - dgbToUnlock;
        CAmount remainingDD = position.ddMinted - ddToBurn;

        // Create new collateral output
        CScript newCollateral = CreateCollateralScript(
            remainingDGB,
            remainingDD,
            position.unlockHeight,
            position.ownerKey
        );
        tx.vout.push_back(CTxOut(remainingDGB, newCollateral));
    }

    return true;
}
```

### Test
```cpp
// src/test/digidollar_redeem_tests.cpp
BOOST_AUTO_TEST_CASE(test_tx_building) {
    CCollateralPosition pos;
    pos.dgbLocked = 300000 * COIN;
    pos.ddMinted = 100000;
    pos.unlockHeight = 1000;

    CMutableTransaction tx;
    std::string error;

    BOOST_CHECK(builder.CreateRedemptionTx(pos, 0, tx, error));
    BOOST_CHECK_EQUAL(tx.nLockTime, 1000);
    BOOST_CHECK_EQUAL(tx.vin.size(), 2); // Collateral + DD
    BOOST_CHECK_EQUAL(tx.vout.size(), 1); // DGB unlock only
}
```

---

## Task 3: P2TR Signing for Redemption

### Goal
Sign collateral input correctly after timelock expires

### Files
- **MODIFY**: `src/wallet/digidollarwallet.cpp`

### Implementation

```cpp
// In src/wallet/digidollarwallet.cpp
bool DigiDollarWallet::SignRedemptionTransaction(CMutableTransaction& tx, const CCollateralPosition& position) {
    // Input 0 is always the collateral
    unsigned int nIn = 0;

    // Get owner key
    CKey ownerKey;
    if (!GetKey(position.ownerKey.GetID(), ownerKey)) {
        return error("Cannot find private key for redemption");
    }

    // Create Schnorr signature
    uint256 sighash;
    if (!SignatureHashSchnorr(sighash, tx, nIn, SIGHASH_DEFAULT,
                               SigVersion::TAPROOT, position.dgbLocked)) {
        return false;
    }

    std::vector<unsigned char> sig(64);
    if (!ownerKey.SignSchnorr(sighash, sig)) {
        return error("Failed to create Schnorr signature");
    }

    // Build witness: [signature]
    tx.vin[nIn].scriptWitness.stack.clear();
    tx.vin[nIn].scriptWitness.stack.push_back(sig);

    // Sign DD inputs (standard P2TR)
    for (unsigned int i = 1; i < tx.vin.size(); i++) {
        if (!SignDDInput(tx, i)) {
            return error("Failed to sign DD input");
        }
    }

    return true;
}
```

### Test
```cpp
BOOST_AUTO_TEST_CASE(test_signing) {
    // Create position at height 1000, unlock at 1240
    SetMockHeight(1240); // Timelock expired

    CMutableTransaction tx;
    // ... build transaction ...

    BOOST_CHECK(wallet.SignRedemptionTransaction(tx, position));
    BOOST_CHECK(!tx.vin[0].scriptWitness.IsNull());

    // Verify signature is valid
    BOOST_CHECK(VerifyScript(/* ... */));
}
```

---

## Task 4: RPC - `redeemdigidollar`

### Goal
Implement main redemption RPC command

### Files
- **MODIFY**: `src/rpc/digidollar.cpp`

### Implementation

```cpp
// In src/rpc/digidollar.cpp
static RPCHelpMan redeemdigidollar()
{
    return RPCHelpMan{"redeemdigidollar",
        "\nRedeem DigiDollars by burning them to unlock collateral.\n",
        {
            {"collateral_outpoint", RPCArg::Type::STR, RPCArg::Optional::NO, "Collateral outpoint (txid:vout)"},
            {"partial_amount", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Partial DD amount to redeem (optional)"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR_HEX, "txid", "The redemption transaction id"},
                {RPCResult::Type::NUM, "dgb_unlocked", "Amount of DGB unlocked"},
                {RPCResult::Type::NUM, "dd_burned", "Amount of DD burned"},
            }
        },
        RPCExamples{
            HelpExampleCli("redeemdigidollar", "\"abc123:0\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return NullUniValue;

            // Parse outpoint
            COutPoint outpoint;
            if (!ParseOutpoint(request.params[0].get_str(), outpoint)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid outpoint");
            }

            // Get position
            CCollateralPosition position;
            if (!pwallet->GetCollateralPosition(outpoint, position)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Collateral position not found");
            }

            // Check eligibility
            DigiDollarRedemption redeemer;
            CAmount partialAmount = request.params.size() > 1 ? AmountFromValue(request.params[1]) : 0;
            auto eligibility = redeemer.CheckEligibility(position, partialAmount);

            if (!eligibility.canRedeem) {
                throw JSONRPCError(RPC_WALLET_ERROR, eligibility.statusMessage);
            }

            // Build transaction
            DigiDollarTxBuilder builder(pwallet.get());
            CMutableTransaction tx;
            std::string error;

            if (!builder.CreateRedemptionTx(position, partialAmount, tx, error)) {
                throw JSONRPCError(RPC_WALLET_ERROR, error);
            }

            // Sign transaction
            if (!pwallet->SignRedemptionTransaction(tx, position)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign transaction");
            }

            // Broadcast
            CTransactionRef txRef = MakeTransactionRef(tx);
            std::string txid;
            if (!pwallet->CommitTransaction(txRef, {}, {}, txid)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Failed to broadcast transaction");
            }

            // Build result
            UniValue result(UniValue::VOBJ);
            result.pushKV("txid", txid);
            result.pushKV("dgb_unlocked", ValueFromAmount(eligibility.willUnlockDGB));
            result.pushKV("dd_burned", ValueFromAmount(eligibility.requiredDD));

            return result;
        }
    };
}
```

---

## Task 5: RPC - Helper Commands

### Goal
Add `getredemptioninfo` and `listredeemablepositions`

### Implementation

```cpp
// getredemptioninfo
static RPCHelpMan getredemptioninfo()
{
    return RPCHelpMan{"getredemptioninfo",
        "\nGet redemption information for a collateral position.\n",
        {
            {"outpoint", RPCArg::Type::STR, RPCArg::Optional::NO, "Collateral outpoint"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "can_redeem", "Whether position can be redeemed"},
                {RPCResult::Type::NUM, "blocks_remaining", "Blocks until redeemable"},
                {RPCResult::Type::NUM, "required_dd", "DD amount required to redeem"},
                {RPCResult::Type::STR, "status", "Status message"},
            }
        },
        RPCExamples{
            HelpExampleCli("getredemptioninfo", "\"abc:0\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            // ... implementation similar to redeemdigidollar but just returns info ...
        }
    };
}

// listredeemablepositions
static RPCHelpMan listredeemablepositions()
{
    return RPCHelpMan{"listredeemablepositions",
        "\nList all collateral positions that can be redeemed.\n",
        {},
        RPCResult{
            RPCResult::Type::ARR, "", "",
            {
                {RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR, "outpoint", "Position outpoint"},
                        {RPCResult::Type::NUM, "dgb_locked", "DGB locked"},
                        {RPCResult::Type::NUM, "dd_minted", "DD minted"},
                        {RPCResult::Type::STR, "status", "redeemable/locked"},
                    }
                },
            }
        },
        RPCExamples{
            HelpExampleCli("listredeemablepositions", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return NullUniValue;

            UniValue result(UniValue::VARR);
            DigiDollarRedemption redeemer;

            for (const auto& [outpoint, position] : pwallet->GetCollateralPositions()) {
                auto eligibility = redeemer.CheckEligibility(position);

                if (eligibility.canRedeem) {
                    UniValue obj(UniValue::VOBJ);
                    obj.pushKV("outpoint", outpoint.ToString());
                    obj.pushKV("dgb_locked", ValueFromAmount(position.dgbLocked));
                    obj.pushKV("dd_minted", ValueFromAmount(position.ddMinted));
                    obj.pushKV("status", "redeemable");
                    result.push_back(obj);
                }
            }

            return result;
        }
    };
}
```

---

## Task 6: DD Burning & Position Closure

### Goal
Remove burned DD and update positions

### Files
- **MODIFY**: `src/wallet/digidollarwallet.cpp`, `src/wallet/walletdb.cpp`

### Implementation

```cpp
// In src/wallet/digidollarwallet.cpp
bool DigiDollarWallet::BurnDDUTXOs(const std::vector<COutPoint>& utxos) {
    WalletBatch batch(m_wallet->GetDatabase());
    CAmount totalBurned = 0;

    for (const auto& outpoint : utxos) {
        auto it = m_dd_utxos.find(outpoint);
        if (it == m_dd_utxos.end()) continue;

        totalBurned += it->second;
        m_dd_utxos.erase(it);
        batch.EraseDDUTXO(outpoint);
    }

    m_total_burned += totalBurned;
    batch.WriteDDTotalBurned(m_total_burned);

    UpdateBalance();
    return true;
}

bool DigiDollarWallet::CloseCollateralPosition(const COutPoint& outpoint) {
    auto it = m_collateral_positions.find(outpoint);
    if (it == m_collateral_positions.end()) {
        return false;
    }

    m_collateral_positions.erase(it);

    WalletBatch batch(m_wallet->GetDatabase());
    batch.EraseCollateralPosition(outpoint);

    return true;
}

bool DigiDollarWallet::UpdateCollateralPosition(
    const COutPoint& oldOutpoint,
    const COutPoint& newOutpoint,
    CAmount newDGB,
    CAmount newDD)
{
    // Get old position
    auto it = m_collateral_positions.find(oldOutpoint);
    if (it == m_collateral_positions.end()) {
        return false;
    }

    CCollateralPosition oldPos = it->second;

    // Create new position
    CCollateralPosition newPos = oldPos;
    newPos.outpoint = newOutpoint;
    newPos.dgbLocked = newDGB;
    newPos.ddMinted = newDD;

    // Update maps
    m_collateral_positions.erase(it);
    m_collateral_positions[newOutpoint] = newPos;

    // Update database
    WalletBatch batch(m_wallet->GetDatabase());
    batch.EraseCollateralPosition(oldOutpoint);
    batch.WriteCollateralPosition(newPos);

    return true;
}
```

---

## Task 7: Add 1-Hour Lock + Fix Tests

### Goal
Add test timelock and ensure all tests pass

### Files
- **MODIFY**: `src/consensus/digidollar.cpp`, all test files

### Implementation

```cpp
// In src/consensus/digidollar.cpp
namespace DigiDollar {
    static const std::map<std::string, int64_t> LOCK_PERIODS = {
        {"1hour", 240},           // 1 hour = 240 blocks (TESTING ONLY)
        {"30days", 172800},       // 30 days
        {"3months", 518400},      // 3 months
        {"6months", 1036800},     // 6 months
        {"1year", 2102400},       // 1 year
        {"3years", 6307200},      // 3 years
        {"5years", 10512000},     // 5 years
        {"7years", 14716800},     // 7 years
        {"10years", 21024000},    // 10 years
    };

    static const std::map<std::string, int> COLLATERAL_RATIOS = {
        {"1hour", 500},      // Same as 30 days for safety
        {"30days", 500},
        {"3months", 400},
        {"6months", 350},
        {"1year", 300},
        {"3years", 250},
        {"5years", 225},
        {"7years", 212},
        {"10years", 200},
    };
}
```

### Test All Existing Tests
```bash
# Run all digidollar tests
./test/functional/test_digidollar_mint.py
./test/functional/test_digidollar_transfer.py
./test/functional/test_digidollar_wallet.py
./src/test/test_digibyte --run_test=digidollar_*

# Fix any failures
# Most common: Update expected values if logic changed
```

---

## Task 8: Vault Tab GUI

### Goal
Display collateral positions in Qt wallet

### Files
- **CREATE/MODIFY**: `src/qt/digidollarvaultwidget.cpp/h`

### Implementation

```cpp
// src/qt/digidollarvaultwidget.h
class DigiDollarVaultWidget : public QWidget {
    Q_OBJECT

public:
    explicit DigiDollarVaultWidget(QWidget *parent = nullptr);
    void setWalletModel(WalletModel *model);

private Q_SLOTS:
    void updatePositions();
    void onRedeemClicked();

private:
    WalletModel *walletModel;
    QTableWidget *positionsTable;
    QPushButton *redeemButton;

    void setupUI();
    QString getStatusText(const CCollateralPosition& pos);
};
```

```cpp
// src/qt/digidollarvaultwidget.cpp
void DigiDollarVaultWidget::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Positions table
    positionsTable = new QTableWidget();
    positionsTable->setColumnCount(6);
    positionsTable->setHorizontalHeaderLabels({
        "Outpoint", "DGB Locked", "DD Minted",
        "Status", "Blocks Remaining", "Action"
    });

    layout->addWidget(positionsTable);
}

void DigiDollarVaultWidget::updatePositions() {
    if (!walletModel) return;

    auto positions = walletModel->wallet().GetCollateralPositions();
    positionsTable->setRowCount(positions.size());

    int row = 0;
    for (const auto& [outpoint, pos] : positions) {
        DigiDollarRedemption redeemer;
        auto eligibility = redeemer.CheckEligibility(pos);

        positionsTable->setItem(row, 0, new QTableWidgetItem(
            QString::fromStdString(outpoint.ToString())));
        positionsTable->setItem(row, 1, new QTableWidgetItem(
            BitcoinUnits::format(BitcoinUnits::DGB, pos.dgbLocked)));
        positionsTable->setItem(row, 2, new QTableWidgetItem(
            BitcoinUnits::format(BitcoinUnits::DD, pos.ddMinted)));
        positionsTable->setItem(row, 3, new QTableWidgetItem(
            QString::fromStdString(eligibility.statusMessage)));
        positionsTable->setItem(row, 4, new QTableWidgetItem(
            QString::number(eligibility.blocksRemaining)));

        // Redeem button
        QPushButton *btn = new QPushButton(eligibility.canRedeem ? "Redeem" : "Locked");
        btn->setEnabled(eligibility.canRedeem);
        connect(btn, &QPushButton::clicked, this, &DigiDollarVaultWidget::onRedeemClicked);
        positionsTable->setCellWidget(row, 5, btn);

        row++;
    }
}
```

---

## Task 9: Redemption Dialog

### Goal
Create redemption confirmation dialog

### Files
- **CREATE**: `src/qt/digidollarredeemdialog.cpp/h`

### Implementation

```cpp
// src/qt/digidollarredeemdialog.h
class DigiDollarRedeemDialog : public QDialog {
    Q_OBJECT

public:
    explicit DigiDollarRedeemDialog(
        const COutPoint& outpoint,
        const RedemptionEligibility& eligibility,
        QWidget *parent = nullptr
    );

    CAmount getPartialAmount() const;
    bool isPartial() const { return partialCheckbox->isChecked(); }

private:
    QLabel *dgbLabel;
    QLabel *ddLabel;
    QCheckBox *partialCheckbox;
    QLineEdit *partialAmount;
    QPushButton *confirmButton;
};
```

```cpp
// src/qt/digidollarredeemdialog.cpp
DigiDollarRedeemDialog::DigiDollarRedeemDialog(
    const COutPoint& outpoint,
    const RedemptionEligibility& eligibility,
    QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Redeem DigiDollar");

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Show amounts
    dgbLabel = new QLabel(QString("Will unlock: %1 DGB")
        .arg(BitcoinUnits::format(BitcoinUnits::DGB, eligibility.willUnlockDGB)));
    ddLabel = new QLabel(QString("Will burn: %1 DD")
        .arg(BitcoinUnits::format(BitcoinUnits::DD, eligibility.requiredDD)));

    layout->addWidget(dgbLabel);
    layout->addWidget(ddLabel);

    // Partial redemption option
    partialCheckbox = new QCheckBox("Partial Redemption");
    partialAmount = new QLineEdit();
    partialAmount->setPlaceholderText("DD amount");
    partialAmount->setEnabled(false);

    connect(partialCheckbox, &QCheckBox::toggled,
            partialAmount, &QLineEdit::setEnabled);

    layout->addWidget(partialCheckbox);
    layout->addWidget(partialAmount);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttons);
}
```

---

## Task 10: Integration & Testing

### Goal
Wire everything together and ensure it all works

### Tasks
1. Connect vault widget to DigiDollar tab
2. Wire redeem button to execute redemption
3. Update GUI after redemption
4. Run complete test suite
5. Test E2E with Qt wallet

### Integration Code

```cpp
// In src/qt/digidollartab.cpp
void DigiDollarTab::setupVaultTab() {
    vaultWidget = new DigiDollarVaultWidget(this);
    vaultWidget->setWalletModel(walletModel);

    tabWidget->addTab(vaultWidget, tr("Vault"));

    // Connect signals
    connect(walletModel, &WalletModel::balanceChanged,
            vaultWidget, &DigiDollarVaultWidget::updatePositions);
}

// In DigiDollarVaultWidget::onRedeemClicked()
void DigiDollarVaultWidget::onRedeemClicked() {
    // Get selected position
    int row = positionsTable->currentRow();
    QString outpointStr = positionsTable->item(row, 0)->text();

    COutPoint outpoint;
    ParseOutpoint(outpointStr.toStdString(), outpoint);

    // Get position and check eligibility
    CCollateralPosition position;
    walletModel->wallet().GetCollateralPosition(outpoint, position);

    DigiDollarRedemption redeemer;
    auto eligibility = redeemer.CheckEligibility(position);

    // Show dialog
    DigiDollarRedeemDialog dlg(outpoint, eligibility, this);
    if (dlg.exec() == QDialog::Accepted) {
        // Execute redemption
        executeRedemption(outpoint, dlg.getPartialAmount());
    }
}
```

### Final Testing

```bash
# 1. Compile
make -j$(nproc) src/qt/digibyte-qt

# 2. Run unit tests
./src/test/test_digibyte --run_test=digidollar_*

# 3. Run functional tests
./test/functional/test_digidollar_redeem.py
./test/functional/test_digidollar_mint.py
./test/functional/test_digidollar_transfer.py

# 4. Manual Qt test
./src/qt/digibyte-qt -regtest
# - Mint DD with 1hour lock
# - Generate 240 blocks
# - Go to DigiDollar → Vault tab
# - Click Redeem
# - Verify success

# 5. E2E script (if exists)
./test-DigiDollar-QT-E2E.sh
```

---

## Success Checklist

- [ ] Task 1: Eligibility checker works
- [ ] Task 2: Transaction builder creates valid TXs
- [ ] Task 3: Signing produces valid signatures
- [ ] Task 4: `redeemdigidollar` RPC works
- [ ] Task 5: Helper RPCs work
- [ ] Task 6: DD burning and position closure work
- [ ] Task 7: 1-hour lock added, all tests pass
- [ ] Task 8: Vault tab displays positions
- [ ] Task 9: Redeem dialog works
- [ ] Task 10: Full integration works

**Version**: 3.0 - Complete Implementation Guide
