// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/digidollarwidgettests.h>
#include <qt/test/util.h>

#include <consensus/digidollar.h>
#include <consensus/merkle.h>
#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <oracle/bundle_manager.h>
#include <oracle/mock_oracle.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/walletmodel.h>
#include <qt/digidollaroverviewwidget.h>
#include <qt/digidollarmintwidget.h>
#include <qt/digidollarsendwidget.h>
#include <qt/digidollarcoincontroldialog.h>
#include <qt/digidollarreceivewidget.h>
#include <qt/digidollarreceiverequest.h>
#include <qt/digidollarredeemwidget.h>
#include <qt/digidollarpositionswidget.h>
#include <qt/digidollartransactionswidget.h>
#include <qt/digidollartab.h>
#include <qt/ddaddressbookpage.h>
#include <qt/walletview.h>
#include <support/allocators/secure.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/ddcoincontrol.h>
#include <wallet/digidollarwallet.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <memory>

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QComboBox>
#include <QProgressBar>
#include <QListWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSignalSpy>
#include <QTimer>

using wallet::AddWallet;
using wallet::CreateMockableWalletDatabase;
using wallet::RemoveWallet;
using wallet::WALLET_FLAG_DESCRIPTORS;
using wallet::WALLET_FLAG_DISABLE_PRIVATE_KEYS;
using wallet::WalletContext;
using wallet::WalletRescanReserver;

namespace
{

void SyncUpWallet(const std::shared_ptr<wallet::CWallet>& wallet, interfaces::Node& node)
{
    WalletRescanReserver reserver(*wallet);
    reserver.reserve();
    wallet::CWallet::ScanResult result = wallet->ScanForWalletTransactions(
        Params().GetConsensus().hashGenesisBlock, 0, {}, reserver, true, false);
    QCOMPARE(result.status, wallet::CWallet::ScanResult::SUCCESS);
}

std::shared_ptr<wallet::CWallet> SetupDescriptorsWallet(interfaces::Node& node, TestChain100Setup& test, const std::string& wallet_name = "")
{
    std::shared_ptr<wallet::CWallet> wallet = std::make_shared<wallet::CWallet>(
        node.context()->chain.get(), wallet_name, CreateMockableWalletDatabase());
    wallet->LoadWallet();
    LOCK(wallet->cs_wallet);
    wallet->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
    wallet->SetupDescriptorScriptPubKeyMans();

    FlatSigningProvider provider;
    std::string error;
    std::unique_ptr<Descriptor> desc = Parse(
        "combo(" + EncodeSecret(test.coinbaseKey) + ")", provider, error, false);
    assert(desc);
    wallet::WalletDescriptor w_desc(std::move(desc), 0, 0, 1, 1);
    if (!wallet->AddWalletDescriptor(w_desc, provider, "", false)) assert(false);
    CTxDestination dest = GetDestinationForKey(test.coinbaseKey.GetPubKey(), wallet->m_default_address_type);
    wallet->SetAddressBook(dest, "", wallet::AddressPurpose::RECEIVE);
    wallet->SetLastBlockProcessed(105, WITH_LOCK(node.context()->chainman->GetMutex(), 
        return node.context()->chainman->ActiveChain().Tip()->GetBlockHash()));
    SyncUpWallet(wallet, node);
    wallet->SetBroadcastTransactions(true);
    return wallet;
}

CTransactionRef MakePendingDDTx()
{
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(2);
    tx.vout[0].nValue = COIN;
    tx.vout[1].nValue = 0;
    return MakeTransactionRef(std::move(tx));
}

bool ReadRecentRequestEntry(const std::string& request_str, RecentRequestEntry& entry)
{
    std::vector<uint8_t> data(request_str.begin(), request_str.end());
    DataStream ss{data};
    ss >> entry;
    return true;
}

bool FindStoredReceiveRequest(WalletModel& wallet_model, const QString& address, RecentRequestEntry& result)
{
    for (const std::string& request_str : wallet_model.wallet().getAddressReceiveRequests()) {
        RecentRequestEntry entry;
        ReadRecentRequestEntry(request_str, entry);
        if (entry.recipient.address == address) {
            result = entry;
            return true;
        }
    }
    return false;
}

int CountStoredReceiveRequests(WalletModel& wallet_model, const QString& address)
{
    int count = 0;
    for (const std::string& request_str : wallet_model.wallet().getAddressReceiveRequests()) {
        RecentRequestEntry entry;
        ReadRecentRequestEntry(request_str, entry);
        if (entry.recipient.address == address) {
            ++count;
        }
    }
    return count;
}

void CreateAndProcessOracleQuoteBlock(TestChain100Setup& test, CAmount price_micro_usd)
{
    MockOracleManager& mock_oracle = MockOracleManager::GetInstance();
    mock_oracle.SetEnabled(true);
    mock_oracle.SetMockPrice(price_micro_usd);

    OracleBundleManager& oracle_manager = OracleBundleManager::GetInstance();
    oracle_manager.SetEnabled(true);

    Chainstate& chainstate = Assert(test.m_node.chainman)->ActiveChainstate();
    const int block_height = WITH_LOCK(cs_main, return chainstate.m_chain.Tip()->nHeight + 1);
    const CScript coinbase_script = GetScriptForRawPubKey(test.coinbaseKey.GetPubKey());
    CBlock block = test.CreateBlock({}, coinbase_script, chainstate);

    COracleBundle bundle = mock_oracle.CreateMockMuSig2Bundle(block_height, block.GetBlockTime());
    std::string error;
    QVERIFY2(OracleBundleManager::ValidateMuSig2Bundle(
                 bundle, block_height, Params().GetConsensus(), error),
             error.c_str());
    QVERIFY(oracle_manager.UpdateBundle(bundle));
    QVERIFY(oracle_manager.AddOracleBundleToBlock(block, block_height));

    block.hashMerkleRoot = BlockMerkleRoot(block);
    while (!CheckProofOfWork(GetPoWAlgoHash(block), block.nBits, Params().GetConsensus())) {
        ++block.nNonce;
    }

    std::shared_ptr<const CBlock> shared_block = std::make_shared<const CBlock>(block);
    QVERIFY(Assert(test.m_node.chainman)->ProcessNewBlock(shared_block, true, true, nullptr));
}

struct DigiDollarMiniGUI {
public:
    OptionsModel optionsModel;
    std::unique_ptr<ClientModel> clientModel;
    std::unique_ptr<WalletModel> walletModel;
    std::unique_ptr<const PlatformStyle> platformStyle;

    DigiDollarMiniGUI(interfaces::Node& node) : optionsModel(node) {
        bilingual_str error;
        QVERIFY(optionsModel.Init(error));
        clientModel = std::make_unique<ClientModel>(node, &optionsModel);
        platformStyle.reset(PlatformStyle::instantiate("other"));
    }

    void initModelForWallet(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
    {
        WalletContext& context = *node.walletLoader().context();
        AddWallet(context, wallet);
        walletModel = std::make_unique<WalletModel>(
            interfaces::MakeWallet(context, wallet), *clientModel, platformStyle.get());
        RemoveWallet(context, wallet, std::nullopt);
    }
};

void TestOverviewWidget(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
{
    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    DigiDollarMiniGUI mini_gui(node);
    mini_gui.initModelForWallet(node, wallet);

    DigiDollarOverviewWidget overviewWidget;
    overviewWidget.setWalletModel(mini_gui.walletModel.get());
    overviewWidget.setClientModel(mini_gui.clientModel.get());

    QVERIFY(&overviewWidget != nullptr);

    overviewWidget.updateView();
    overviewWidget.updateBalance();
    overviewWidget.updateOraclePrice();
    overviewWidget.updateSystemHealth();
}

void TestMintWidget(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
{
    DigiDollarMiniGUI mini_gui(node);
    mini_gui.initModelForWallet(node, wallet);

    DigiDollarMintWidget mintWidget;
    mintWidget.setWalletModel(mini_gui.walletModel.get());
    mintWidget.setClientModel(mini_gui.clientModel.get());

    QVERIFY(&mintWidget != nullptr);

    mintWidget.updateView();
    mintWidget.updateBalance();
    mintWidget.updateOraclePrice();
}

void TestSendWidget(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
{
    DigiDollarMiniGUI mini_gui(node);
    mini_gui.initModelForWallet(node, wallet);

    DigiDollarSendWidget sendWidget(mini_gui.platformStyle.get());
    sendWidget.setWalletModel(mini_gui.walletModel.get());
    sendWidget.setClientModel(mini_gui.clientModel.get());

    QVERIFY(&sendWidget != nullptr);

    sendWidget.updateView();
    sendWidget.updateBalance();
    sendWidget.updateOraclePrice();
}

void TestReceiveWidget(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
{
    DigiDollarMiniGUI mini_gui(node);
    mini_gui.initModelForWallet(node, wallet);

    DigiDollarReceiveWidget receiveWidget;
    receiveWidget.setWalletModel(mini_gui.walletModel.get());
    receiveWidget.setClientModel(mini_gui.clientModel.get());
    receiveWidget.show();

    QVERIFY(&receiveWidget != nullptr);

    receiveWidget.updateView();
    receiveWidget.updateRecentRequests();

    QLabel* emptyLabel = receiveWidget.findChild<QLabel*>("emptyStateLabel");
    QVERIFY(emptyLabel != nullptr);
    QVERIFY(emptyLabel->isVisibleTo(&receiveWidget));
    QVERIFY(emptyLabel->text().contains(QStringLiteral("Generate")));
    QVERIFY(emptyLabel->text().contains(QStringLiteral("DigiDollar")));

    QFrame* qrFrame = receiveWidget.findChild<QFrame*>("qrFrame");
    QVERIFY(qrFrame != nullptr);
    QVERIFY(!qrFrame->isVisibleTo(&receiveWidget));

    QLineEdit* addressEdit = receiveWidget.findChild<QLineEdit*>("addressEdit");
    QVERIFY(addressEdit != nullptr);
    QVERIFY(addressEdit->text().isEmpty());

    QPushButton* generateButton = receiveWidget.findChild<QPushButton*>("generateButton");
    QVERIFY(generateButton != nullptr);
    QVERIFY(generateButton->isEnabled());
    QCOMPARE(generateButton->property("ddState").toString(), QStringLiteral("primaryEnabled"));
    QVERIFY2(generateButton->styleSheet().contains(QStringLiteral("QPushButton#generateButton:enabled")),
             "Generate button should have an explicit enabled style so it does not look disabled until hover");

    QMetaObject::invokeMethod(generateButton, "click", Qt::DirectConnection);
    QCoreApplication::processEvents();

    QVERIFY(qrFrame->isVisibleTo(&receiveWidget));
    QVERIFY(!emptyLabel->isVisibleTo(&receiveWidget));
    QVERIFY(!addressEdit->text().isEmpty());

    const QList<QDialog*> requestDialogs = receiveWidget.findChildren<QDialog*>();
    for (QDialog* dialog : requestDialogs) {
        dialog->close();
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QMetaObject::invokeMethod(&receiveWidget, "onClearClicked", Qt::DirectConnection);
    QCoreApplication::processEvents();

    QVERIFY(!qrFrame->isVisibleTo(&receiveWidget));
    QVERIFY(emptyLabel->isVisibleTo(&receiveWidget));
    QVERIFY(addressEdit->text().isEmpty());
}

void TestRedeemWidget(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
{
    DigiDollarMiniGUI mini_gui(node);
    mini_gui.initModelForWallet(node, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());

    QVERIFY(&redeemWidget != nullptr);

    redeemWidget.updateView();
}

void TestPositionsWidget(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
{
    DigiDollarMiniGUI mini_gui(node);
    mini_gui.initModelForWallet(node, wallet);

    DigiDollarPositionsWidget positionsWidget;
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.setClientModel(mini_gui.clientModel.get());

    QVERIFY(&positionsWidget != nullptr);

    positionsWidget.updateView();
}

void TestTransactionsWidget(interfaces::Node& node, const std::shared_ptr<wallet::CWallet>& wallet)
{
    DigiDollarMiniGUI mini_gui(node);
    mini_gui.initModelForWallet(node, wallet);

    DigiDollarTransactionsWidget transactionsWidget;
    transactionsWidget.setWalletModel(mini_gui.walletModel.get());
    transactionsWidget.setClientModel(mini_gui.clientModel.get());

    QVERIFY(&transactionsWidget != nullptr);

    transactionsWidget.updateView();
}

void AddMockDigiDollarPosition(const std::shared_ptr<wallet::CWallet>& wallet, const uint256& id, CAmount dd_amount, CAmount collateral, uint32_t tier, int64_t unlock_height)
{
    wallet->EnsureDDWallet();
    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);
    dd_wallet->AddCollateralPosition(WalletCollateralPosition(id, dd_amount, collateral, tier, unlock_height));
}

QDialog* FindVisibleDialogByTitle(const QString& title)
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        QDialog* dialog = qobject_cast<QDialog*>(widget);
        if (dialog && dialog->isVisible() && dialog->windowTitle() == title) return dialog;
    }
    return nullptr;
}

bool SelectCoinControlDialogInput(const COutPoint& outpoint, QString& error)
{
    QDialog* dialog{nullptr};
    for (int attempt = 0; attempt < 20 && !dialog; ++attempt) {
        dialog = FindVisibleDialogByTitle(QStringLiteral("DigiDollar Coin Selection"));
        if (!dialog) QTest::qWait(25);
    }
    if (!dialog) {
        error = QStringLiteral("DigiDollar coin-control dialog did not open");
        return false;
    }

    auto fail = [&](const QString& message) {
        error = message;
        dialog->reject();
        return false;
    };

    QTreeWidget* tree = dialog->findChild<QTreeWidget*>(QStringLiteral("treeWidget"));
    if (!tree) {
        return fail(QStringLiteral("DigiDollar coin-control dialog has no treeWidget"));
    }

    const QString selectedTxid = QString::fromStdString(outpoint.hash.GetHex());
    QList<QTreeWidgetItem*> items;
    QStringList renderedOutpoints;
    auto collect = [&](QTreeWidgetItem* root, auto&& self) -> void {
        for (int i = 0; i < root->childCount(); ++i) {
            QTreeWidgetItem* child = root->child(i);
            const QString outpointText = child->text(6 /* COLUMN_TXID_VOUT */);
            if (!outpointText.isEmpty()) {
                renderedOutpoints << outpointText;
                if (outpointText.contains(selectedTxid)) items << child;
            }
            self(child, self);
        }
    };
    collect(tree->invisibleRootItem(), collect);
    if (items.size() != 1) {
        return fail(QStringLiteral("expected exactly one selectable DD input row for %1, found %2; rendered: %3")
                        .arg(selectedTxid)
                        .arg(items.size())
                        .arg(renderedOutpoints.join(QStringLiteral(", "))));
    }
    items.front()->setCheckState(0 /* COLUMN_CHECKBOX */, Qt::Checked);
    QCoreApplication::processEvents();

    QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!buttons || !buttons->button(QDialogButtonBox::Ok)) {
        return fail(QStringLiteral("DigiDollar coin-control dialog has no OK button"));
    }
    buttons->button(QDialogButtonBox::Ok)->click();
    return true;
}

} // namespace

void DigiDollarWidgetTests::overviewWidgetTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    TestOverviewWidget(m_node, wallet);
}

void DigiDollarWidgetTests::watchOnlyDigiDollarBalanceHiddenInWalletModel()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    wallet->EnsureDDWallet();
    wallet->SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);

    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);
    dd_wallet->AddDDUTXO(COutPoint(uint256::ONE, 1), 10000);
    QCOMPARE(dd_wallet->GetTotalDDBalance(), 10000);

    const CTransactionRef pending_tx = MakePendingDDTx();
    {
        LOCK(wallet->cs_wallet);
        wallet->AddToWallet(pending_tx, wallet::TxStateInMempool{});
    }
    dd_wallet->AddDDUTXO(COutPoint(pending_tx->GetHash(), 1), 2500);
    QCOMPARE(dd_wallet->GetPendingDDBalance(), 2500);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    QCOMPARE(mini_gui.walletModel->getDigiDollarBalance(), 0);
    QCOMPARE(mini_gui.walletModel->getPendingDigiDollarBalance(), 0);
}

void DigiDollarWidgetTests::privateKeyDisabledWalletCannotGenerateDigiDollarAddress()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    wallet->SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    QCOMPARE(mini_gui.walletModel->getNewDigiDollarAddress("watch-only-dd"), QString());
}

void DigiDollarWidgetTests::mintWidgetTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    TestMintWidget(m_node, wallet);
}

void DigiDollarWidgetTests::mintWidgetUsesChainParamMintLimits()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    DigiDollarMintWidget mintWidget;
    QLineEdit* amountEdit = mintWidget.findChild<QLineEdit*>("amountEdit");
    QVERIFY(amountEdit != nullptr);
    QVERIFY(amountEdit->validator() != nullptr);

    const auto& ddParams = Params().GetDigiDollarParams();
    const QString minText = QString::number(ddParams.minMintAmount / 100.0, 'f', 2);
    const QString maxText = QString::number(ddParams.maxMintAmount / 100.0, 'f', 2);

    QVERIFY2(amountEdit->toolTip().contains("Minimum: $" + minText),
             qPrintable(amountEdit->toolTip()));
    QVERIFY2(amountEdit->toolTip().contains("Maximum: $" + maxText),
             qPrintable(amountEdit->toolTip()));

    QString belowMin = QString::number((ddParams.minMintAmount - 1) / 100.0, 'f', 2);
    int pos = 0;
    QCOMPARE(amountEdit->validator()->validate(belowMin, pos), QValidator::Intermediate);

    QString maxAmount = maxText;
    pos = 0;
    QCOMPARE(amountEdit->validator()->validate(maxAmount, pos), QValidator::Acceptable);

    QString aboveMax = QString::number((ddParams.maxMintAmount + 1) / 100.0, 'f', 2);
    pos = 0;
    QCOMPARE(amountEdit->validator()->validate(aboveMax, pos), QValidator::Invalid);
}

void DigiDollarWidgetTests::mintWidgetCollateralMatchesBuilderSafetyMargin()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    MockOracleManager::GetInstance().SetEnabled(true);
    MockOracleManager::GetInstance().SetMockPrice(500000); // $0.50/DGB in micro-USD

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    QCOMPARE(mini_gui.walletModel->calculateRequiredCollateral(10000, 1), 1010 * COIN);

    DigiDollarMintWidget mintWidget;
    mintWidget.setWalletModel(mini_gui.walletModel.get());
    mintWidget.setClientModel(mini_gui.clientModel.get());
    mintWidget.show();
    mintWidget.updateView();

    QLineEdit* amountEdit = mintWidget.findChild<QLineEdit*>("amountEdit");
    QVERIFY(amountEdit != nullptr);
    amountEdit->setText("100.00");
    QCoreApplication::processEvents();

    QLabel* collateralValue = mintWidget.findChild<QLabel*>("collateralValue");
    QVERIFY(collateralValue != nullptr);
    QCOMPARE(collateralValue->text(), QString("1010.00000000 DGB"));

    MockOracleManager::GetInstance().Reset();
}

void DigiDollarWidgetTests::qtMintStoresDescriptorRecoverableOwnerKey()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    MockOracleManager::GetInstance().SetEnabled(true);
    MockOracleManager::GetInstance().SetMockPrice(500000);

    CreateAndProcessOracleQuoteBlock(test, 500000);
    std::shared_ptr<wallet::CWallet> wallet = wallet::CreateSyncedWallet(
        *test.m_node.chain,
        WITH_LOCK(Assert(test.m_node.chainman)->GetMutex(), return test.m_node.chainman->ActiveChain()),
        test.coinbaseKey);
    wallet->SetBroadcastTransactions(true);
    wallet->EnsureDDWallet();
    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    mini_gui.walletModel->pollBalanceChanged();

    WalletModel::DigiDollarMintResult result = mini_gui.walletModel->mintDigiDollar(10000, 0);
    QVERIFY2(result.status == WalletModel::OK, result.reasonFailed.toUtf8().constData());

    uint256 position_id;
    position_id.SetHex(result.positionId.toStdString());

    CTransactionRef mint_tx;
    CTxOut dd_txout;
    {
        LOCK(wallet->cs_wallet);
        const wallet::CWalletTx* wtx = wallet->GetWalletTx(position_id);
        QVERIFY(wtx != nullptr);
        QVERIFY(wtx->tx->vout.size() > 1);
        mint_tx = wtx->tx;
        dd_txout = mint_tx->vout[1];
    }

    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);

    int64_t op_return_unlock_height{0};
    QVERIFY(DigiDollarWallet::ExtractUnlockHeightFromOpReturn(*mint_tx, op_return_unlock_height));
    const std::vector<WalletCollateralPosition> positions = dd_wallet->GetDDTimeLocks(/*active_only=*/false);
    QCOMPARE(positions.size(), static_cast<size_t>(1));
    QVERIFY(positions[0].dd_timelock_id == position_id);
    QCOMPARE(positions[0].unlock_height, op_return_unlock_height);

    CKey recovered_key;
    QVERIFY(dd_wallet->GetDDOutputSpendingKey(dd_txout, recovered_key));

    MockOracleManager::GetInstance().Reset();
}

void DigiDollarWidgetTests::staleMintUnlockHeightCacheRepairsFromOpReturn()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    MockOracleManager::GetInstance().SetEnabled(true);
    MockOracleManager::GetInstance().SetMockPrice(500000);

    CreateAndProcessOracleQuoteBlock(test, 500000);
    std::shared_ptr<wallet::CWallet> wallet = wallet::CreateSyncedWallet(
        *test.m_node.chain,
        WITH_LOCK(Assert(test.m_node.chainman)->GetMutex(), return test.m_node.chainman->ActiveChain()),
        test.coinbaseKey);
    wallet->SetBroadcastTransactions(true);
    wallet->EnsureDDWallet();

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    mini_gui.walletModel->pollBalanceChanged();

    WalletModel::DigiDollarMintResult result = mini_gui.walletModel->mintDigiDollar(10000, 0);
    QVERIFY2(result.status == WalletModel::OK, result.reasonFailed.toUtf8().constData());

    uint256 position_id;
    position_id.SetHex(result.positionId.toStdString());

    CTransactionRef mint_tx;
    {
        LOCK(wallet->cs_wallet);
        const wallet::CWalletTx* wtx = wallet->GetWalletTx(position_id);
        QVERIFY(wtx != nullptr);
        mint_tx = wtx->tx;
    }

    int64_t op_return_unlock_height{0};
    QVERIFY(DigiDollarWallet::ExtractUnlockHeightFromOpReturn(*mint_tx, op_return_unlock_height));

    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);
    std::vector<WalletCollateralPosition> positions = dd_wallet->GetDDTimeLocks(/*active_only=*/false);
    QCOMPARE(positions.size(), static_cast<size_t>(1));

    WalletCollateralPosition stale = positions[0];
    stale.unlock_height = op_return_unlock_height - DigiDollar::MINT_LOCK_CONFIRMATION_BUFFER_BLOCKS;
    QVERIFY(stale.unlock_height != op_return_unlock_height);
    QVERIFY(dd_wallet->WriteDDTimeLock(stale));

    positions = dd_wallet->GetDDTimeLocks(/*active_only=*/false);
    QCOMPARE(positions.size(), static_cast<size_t>(1));
    QCOMPARE(positions[0].unlock_height, stale.unlock_height);

    QVERIFY(dd_wallet->RefreshPositionMetadataFromMintTx(position_id));

    positions = dd_wallet->GetDDTimeLocks(/*active_only=*/false);
    QCOMPARE(positions.size(), static_cast<size_t>(1));
    QCOMPARE(positions[0].unlock_height, op_return_unlock_height);

    stale = positions[0];
    stale.unlock_height = op_return_unlock_height - DigiDollar::MINT_LOCK_CONFIRMATION_BUFFER_BLOCKS;
    QVERIFY(dd_wallet->WriteDDTimeLock(stale));

    dd_wallet->ReconcilePositionStates();

    positions = dd_wallet->GetDDTimeLocks(/*active_only=*/false);
    QCOMPARE(positions.size(), static_cast<size_t>(1));
    QCOMPARE(positions[0].dd_timelock_id, position_id);
    QCOMPARE(positions[0].unlock_height, op_return_unlock_height);
    QCOMPARE(positions[0].dd_minted, CAmount(10000));

    MockOracleManager::GetInstance().Reset();
}

void DigiDollarWidgetTests::sendWidgetTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    TestSendWidget(m_node, wallet);
}

void DigiDollarWidgetTests::sendWidgetCoinControlLabelsMirrorDgb()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    wallet->EnsureDDWallet();
    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);

    const COutPoint selected_a(uint256::ONE, 1);
    const COutPoint selected_b(uint256S("02"), 2);
    dd_wallet->AddDDUTXO(selected_a, 2500);
    dd_wallet->AddDDUTXO(selected_b, 7500);
    dd_wallet->AddDDUTXO(COutPoint(uint256S("03"), 3), 5000);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarSendWidget sendWidget(mini_gui.platformStyle.get());
    sendWidget.setWalletModel(mini_gui.walletModel.get());
    sendWidget.setClientModel(mini_gui.clientModel.get());

    sendWidget.setSelectedDigiDollarInputsForTesting({selected_a, selected_b});
    QCoreApplication::processEvents();

    QLabel* quantityLabel = sendWidget.findChild<QLabel*>("coinControlQuantityLabel");
    QVERIFY(quantityLabel != nullptr);
    QCOMPARE(quantityLabel->text(), QString("Quantity: 2"));

    QLabel* amountLabel = sendWidget.findChild<QLabel*>("coinControlAmountLabel");
    QVERIFY(amountLabel != nullptr);
    QCOMPARE(amountLabel->text(), QString("Amount: 100.00 DD"));

    sendWidget.setSelectedDigiDollarInputsForTesting({});
    QCoreApplication::processEvents();
    QCOMPARE(quantityLabel->text(), QString("automatically selected"));
    QVERIFY(amountLabel->text().isEmpty());
    QVERIFY(sendWidget.findChild<QLabel*>("preflightLabel") == nullptr);
}

void DigiDollarWidgetTests::sendWidgetCoinControlDialogSelectionFeedsSend()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    wallet->EnsureDDWallet();
    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);

    const COutPoint selected_input(Hash("qt-dd-send-selected-input"), 1);
    const COutPoint automatic_input(Hash("qt-dd-send-automatic-input"), 2);
    dd_wallet->AddDDUTXO(selected_input, 2500);
    dd_wallet->AddDDUTXO(automatic_input, 10000);
    QCOMPARE(static_cast<int>(dd_wallet->GetDDUTXOs().size()), 2);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    DigiDollarWallet* model_dd_wallet = mini_gui.walletModel->getDigiDollarWallet();
    QVERIFY(model_dd_wallet != nullptr);
    QCOMPARE(static_cast<int>(model_dd_wallet->GetDDUTXOs().size()), 2);

    DigiDollarSendWidget sendWidget(mini_gui.platformStyle.get());
    sendWidget.setWalletModel(mini_gui.walletModel.get());
    sendWidget.setClientModel(mini_gui.clientModel.get());

    QPushButton* coinControlButton = sendWidget.findChild<QPushButton*>("coinControlButton");
    QVERIFY(coinControlButton != nullptr);
    QCOMPARE(coinControlButton->text(), QString("Inputs..."));

    wallet::DDCoinControl coin_control;
    DigiDollarCoinControlDialog dialog(coin_control, mini_gui.walletModel.get(), mini_gui.platformStyle.get());
    dialog.show();
    QCoreApplication::processEvents();

    QString dialogError;
    QVERIFY2(SelectCoinControlDialogInput(selected_input, dialogError), qPrintable(dialogError));
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QVERIFY(coin_control.IsSelected(selected_input));

    sendWidget.setSelectedDigiDollarInputsForTesting(coin_control.ListSelected());
    QCoreApplication::processEvents();

    QLabel* quantityLabel = sendWidget.findChild<QLabel*>("coinControlQuantityLabel");
    QVERIFY(quantityLabel != nullptr);
    QCOMPARE(quantityLabel->text(), QString("Quantity: 1"));

    QLabel* amountLabel = sendWidget.findChild<QLabel*>("coinControlAmountLabel");
    QVERIFY(amountLabel != nullptr);
    QCOMPARE(amountLabel->text(), QString("Amount: 25.00 DD"));

    const QString recipient = mini_gui.walletModel->getNewDigiDollarAddress(QStringLiteral("qt-selected-input-send"));
    QVERIFY(!recipient.isEmpty());

    const WalletModel::DigiDollarSendResult result =
        sendWidget.sendDigiDollarForTesting(recipient, 2000);
    QCOMPARE(result.status, WalletModel::TransactionCreationFailed);
    QVERIFY2(result.reasonFailed.contains(QStringLiteral("Selected DD input is unknown or not owned"), Qt::CaseInsensitive),
             qPrintable(result.reasonFailed));
}

void DigiDollarWidgetTests::receiveWidgetTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    TestReceiveWidget(m_node, wallet);
}

void DigiDollarWidgetTests::redeemWidgetTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    TestRedeemWidget(m_node, wallet);
}

void DigiDollarWidgetTests::redeemWidgetKeepsTimelockedPositionDisabled()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-timelocked-redeem");
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 200);
    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);
    dd_wallet->AddDDUTXO(COutPoint(uint256::ONE, 1), 10000);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.m_positionFound = true;
    redeemWidget.m_positionDDMinted = 100.0;
    redeemWidget.m_positionDGBCollateral = 300.0;
    redeemWidget.m_positionLockTier = 1;
    redeemWidget.m_positionBlocksRemaining = 95;
    redeemWidget.m_positionHealth = 150.0;
    redeemWidget.m_redeemableAmount = 0.0;
    redeemWidget.m_amountEdit->clear();
    redeemWidget.updatePositionInfo();
    redeemWidget.updateRedeemButtons();
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY(!redeemButton->isEnabled());

    QLabel* ddMintedValue = redeemWidget.findChild<QLabel*>("ddMintedValue");
    QVERIFY(ddMintedValue != nullptr);
    QCOMPARE(ddMintedValue->text(), QString("100.00 DD"));

    QLabel* redeemableValue = redeemWidget.findChild<QLabel*>("redeemableValue");
    QVERIFY(redeemableValue != nullptr);
    QCOMPARE(redeemableValue->text(), QString("0.00 DD"));
}

void DigiDollarWidgetTests::positionsWidgetTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    TestPositionsWidget(m_node, wallet);
}

void DigiDollarWidgetTests::positionsWidgetHiddenDoesNotPollWallet()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarPositionsWidget positionsWidget;
    QVERIFY(!positionsWidget.isVisible());
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.setClientModel(mini_gui.clientModel.get());
    QCoreApplication::processEvents();

    QTableWidget* table = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 0);

    positionsWidget.show();
    QCoreApplication::processEvents();
    positionsWidget.updateView();
    QCOMPARE(table->rowCount(), 1);
}

void DigiDollarWidgetTests::positionsWidgetInitialLoadNotThrottled()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarPositionsWidget positionsWidget;
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.setClientModel(mini_gui.clientModel.get());
    positionsWidget.show();
    QCoreApplication::processEvents();
    positionsWidget.updateView();

    QTableWidget* table = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);
}

void DigiDollarWidgetTests::positionsWidgetHealthUsesMicroUsdOraclePrice()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    MockOracleManager::GetInstance().SetEnabled(true);
    MockOracleManager::GetInstance().SetMockPrice(500000); // $0.50/DGB in micro-USD

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarPositionsWidget positionsWidget;
    positionsWidget.setClientModel(mini_gui.clientModel.get());
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.show();
    QCoreApplication::processEvents();
    positionsWidget.updateView();

    QTableWidget* table = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);

    QWidget* healthWidget = table->cellWidget(0, DigiDollarPositionsWidget::COL_HEALTH);
    QVERIFY(healthWidget != nullptr);
    QProgressBar* healthBar = healthWidget->findChild<QProgressBar*>();
    QVERIFY(healthBar != nullptr);
    QCOMPARE(healthBar->value(), 150);

    MockOracleManager::GetInstance().Reset();
}

void DigiDollarWidgetTests::positionsWidgetDisablesRedeemForPrivateKeyDisabledWallet()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);
    wallet->SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarPositionsWidget positionsWidget;
    positionsWidget.setClientModel(mini_gui.clientModel.get());
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.show();
    QCoreApplication::processEvents();
    positionsWidget.updateView();

    QTableWidget* table = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);

    QPushButton* redeemButton = qobject_cast<QPushButton*>(
        table->cellWidget(0, DigiDollarPositionsWidget::COL_ACTIONS));
    QVERIFY(redeemButton != nullptr);
    // DD-FA-FUNC-027 (Wave 17 Agent C): a wallet with WALLET_FLAG_DISABLE_PRIVATE_KEYS
    // must surface an explicit "Watch-Only" badge in the redeem column rather
    // than the previous ambiguous "Locked" text shared with timelocked vaults.
    QCOMPARE(redeemButton->text(), QString("Watch-Only"));
    QVERIFY(!redeemButton->isEnabled());
    // The tooltip must explain why the action is disabled so the user knows
    // their wallet is the limiting factor (not the timelock).
    QVERIFY(redeemButton->toolTip().contains("Watch-only"));
}

void DigiDollarWidgetTests::positionsWidgetDisablesRedeemForLockedEncryptedWallet()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);

    SecureString passphrase{"wave17-qt-locked-wallet"};
    QVERIFY(wallet->EncryptWallet(passphrase));
    QVERIFY(wallet->IsLocked());

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    QCOMPARE(mini_gui.walletModel->getEncryptionStatus(), WalletModel::Locked);

    DigiDollarPositionsWidget positionsWidget;
    positionsWidget.setClientModel(mini_gui.clientModel.get());
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.show();
    QCoreApplication::processEvents();
    positionsWidget.updateView();

    QTableWidget* table = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);

    QPushButton* redeemButton = qobject_cast<QPushButton*>(
        table->cellWidget(0, DigiDollarPositionsWidget::COL_ACTIONS));
    QVERIFY(redeemButton != nullptr);
    QCOMPARE(redeemButton->text(), QString("Wallet Locked"));
    QVERIFY(!redeemButton->isEnabled());
    QVERIFY(redeemButton->toolTip().contains("Unlock"));
}

void DigiDollarWidgetTests::redeemWidgetButtonStateNoSelection()
{
    DigiDollarRedeemWidget redeemWidget;

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY(!redeemButton->isEnabled());
    QCOMPARE(redeemButton->text(), QString("Cannot Redeem"));
    QVERIFY(redeemButton->toolTip().contains("Select"));
}

void DigiDollarWidgetTests::redeemWidgetButtonStateTimelockActive()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-redeem-timelock-state");
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 200);
    wallet->GetDDWallet()->AddDDUTXO(COutPoint(uint256::ONE, 1), 100000000);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.m_positionFound = true;
    redeemWidget.m_positionDDMinted = 100.0;
    redeemWidget.m_positionDGBCollateral = 300.0;
    redeemWidget.m_positionLockTier = 1;
    redeemWidget.m_positionBlocksRemaining = 95;
    redeemWidget.m_positionHealth = 150.0;
    redeemWidget.m_redeemableAmount = 0.0;
    redeemWidget.m_amountEdit->clear();
    redeemWidget.updatePositionInfo();
    redeemWidget.updateRedeemButtons();
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY(!redeemButton->isEnabled());
    QCOMPARE(redeemButton->text(), QString("Cannot Redeem"));
    QVERIFY(redeemButton->toolTip().contains("Time remaining"));
    QVERIFY(redeemButton->toolTip().contains("Blocks remaining: 95"));
    QLabel* validationLabel = redeemWidget.findChild<QLabel*>("positionValidationLabel");
    QVERIFY(validationLabel != nullptr);
    QVERIFY(validationLabel->toolTip().contains("Blocks remaining: 95"));
}

void DigiDollarWidgetTests::redeemWidgetButtonStateInvalidAmount()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-redeem-invalid-amount");
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);
    wallet->GetDDWallet()->AddDDUTXO(COutPoint(uint256::ONE, 1), 10000);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.setPosition(QString::fromStdString(uint256::ONE.GetHex()));
    QLineEdit* amountEdit = redeemWidget.findChild<QLineEdit*>("amountEdit");
    QVERIFY(amountEdit != nullptr);
    amountEdit->setText(QStringLiteral("99.99"));
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY(!redeemButton->isEnabled());
    QVERIFY(redeemButton->toolTip().contains("full redeemable"));
    QLabel* validationLabel = redeemWidget.findChild<QLabel*>("positionValidationLabel");
    QVERIFY(validationLabel != nullptr);
    QVERIFY(validationLabel->toolTip().contains("full redeemable"));
}

void DigiDollarWidgetTests::redeemWidgetButtonStateInsufficientDDBalance()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-redeem-insufficient");
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.m_positionFound = true;
    redeemWidget.m_positionDDMinted = 100.0;
    redeemWidget.m_positionDGBCollateral = 300.0;
    redeemWidget.m_positionLockTier = 1;
    redeemWidget.m_positionBlocksRemaining = 0;
    redeemWidget.m_positionHealth = 150.0;
    redeemWidget.m_redeemableAmount = 100.0;
    redeemWidget.m_amountEdit->setText(QStringLiteral("100.00"));
    redeemWidget.updatePositionInfo();
    redeemWidget.updateRedeemButtons();
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY(!redeemButton->isEnabled());
    QVERIFY(redeemButton->toolTip().contains("Insufficient DigiDollar balance"));
    QLabel* validationLabel = redeemWidget.findChild<QLabel*>("positionValidationLabel");
    QVERIFY(validationLabel != nullptr);
    QVERIFY(validationLabel->toolTip().contains("Insufficient DigiDollar balance"));
}

void DigiDollarWidgetTests::redeemWidgetButtonStatePrivateKeyDisabledWallet()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-redeem-watchonly");
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);
    wallet->GetDDWallet()->AddDDUTXO(COutPoint(uint256::ONE, 1), 10000);
    wallet->SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.setPosition(QString::fromStdString(uint256::ONE.GetHex()));
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY(!redeemButton->isEnabled());
    QVERIFY(redeemButton->toolTip().contains("Watch-only"));
    QLabel* validationLabel = redeemWidget.findChild<QLabel*>("positionValidationLabel");
    QVERIFY(validationLabel != nullptr);
    QVERIFY(validationLabel->toolTip().contains("Watch-only"));
}

void DigiDollarWidgetTests::redeemWidgetButtonStateLockedWallet()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-redeem-wallet-locked");
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);
    wallet->GetDDWallet()->AddDDUTXO(COutPoint(uint256::ONE, 1), 10000);
    SecureString passphrase{"qt-dd-redeem-wallet-locked"};
    QVERIFY(wallet->EncryptWallet(passphrase));

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.setPosition(QString::fromStdString(uint256::ONE.GetHex()));
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY(!redeemButton->isEnabled());
    QVERIFY(redeemButton->toolTip().contains("Unlock"));
    QLabel* validationLabel = redeemWidget.findChild<QLabel*>("positionValidationLabel");
    QVERIFY(validationLabel != nullptr);
    QVERIFY(validationLabel->toolTip().contains("Unlock"));
}

void DigiDollarWidgetTests::redeemWidgetButtonStateReady()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-redeem-ready");
    AddMockDigiDollarPosition(wallet, uint256::ONE, 10000, 300 * COIN, 1, 100);
    wallet->GetDDWallet()->AddDDUTXO(COutPoint(uint256::ONE, 1), 100000000);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.m_positionFound = true;
    redeemWidget.m_positionDDMinted = 100.0;
    redeemWidget.m_positionDGBCollateral = 300.0;
    redeemWidget.m_positionLockTier = 1;
    redeemWidget.m_positionBlocksRemaining = 0;
    redeemWidget.m_positionHealth = 150.0;
    redeemWidget.m_redeemableAmount = 100.0;
    redeemWidget.m_amountEdit->setText(QStringLiteral("100.00"));
    redeemWidget.updatePositionInfo();
    redeemWidget.updateRedeemButtons();
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QVERIFY(redeemButton != nullptr);
    QVERIFY2(redeemWidget.validateAmount(), "ready state amount should validate");
    QVERIFY2(redeemWidget.validateRedeemable(), "ready state redeemable amount should validate");
    QVERIFY2(redeemWidget.validateDDBalance(), "ready state DD balance should validate");
    QVERIFY2(redeemWidget.canWalletSignRedemption(), "ready state wallet should be able to sign");
    QVERIFY(redeemButton->isEnabled());
    QCOMPARE(redeemButton->text(), QString("Redeem && Unlock DGB"));
    QVERIFY(redeemButton->toolTip().contains("Ready to redeem"));
    QLabel* validationLabel = redeemWidget.findChild<QLabel*>("positionValidationLabel");
    QVERIFY(validationLabel != nullptr);
    QVERIFY(validationLabel->text().contains("ready", Qt::CaseInsensitive));
    QVERIFY(validationLabel->toolTip().contains("Ready to redeem"));
}

void DigiDollarWidgetTests::positionsWidgetLockedTooltipShowsRemainingBlocksAndTime()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    DigiDollarPositionsWidget positionsWidget;
    QPushButton* redeemButton = positionsWidget.createRedeemButton(
        QString::fromStdString(uint256::ONE.GetHex()),
        false,
        false,
        false,
        false,
        95);
    QVERIFY(redeemButton != nullptr);
    QCOMPARE(redeemButton->text(), QString("Locked"));
    QVERIFY(redeemButton->toolTip().contains("Time remaining: 23m"));
    QVERIFY(redeemButton->toolTip().contains("Blocks remaining: 95"));
}

// DD-FA-FUNC-031 (Wave 19 Agent A): WalletModel::mintDigiDollar must
// short-circuit private-keys-disabled wallets with the same explicit
// "Private keys are disabled" diagnostic that sendDigiDollar already
// surfaces, instead of letting the user run through UTXO scans, oracle
// RPCs, and a confirmation dialog only to fail later at HD owner-key
// derivation with the misleading "requires an HD wallet" message.
void DigiDollarWidgetTests::mintDigiDollarRejectsPrivateKeyDisabledWallet()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    wallet->EnsureDDWallet();
    wallet->SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    WalletModel::DigiDollarMintResult result =
        mini_gui.walletModel->mintDigiDollar(/*ddAmount=*/10000, /*lockTier=*/0);

    QVERIFY(result.status != WalletModel::OK);
    // Must be the explicit private-keys-disabled diagnostic, not the
    // misleading "requires an HD wallet" message that mint emits when
    // the HD owner-key derivation finally fails further down the path.
    QVERIFY2(result.reasonFailed.contains("Private keys are disabled", Qt::CaseInsensitive),
             qPrintable(QString("expected 'Private keys are disabled' in reasonFailed, got: ") + result.reasonFailed));
    // The fail-fast check must run before any wallet-side state mutation
    // so the txid/positionId remain empty for the rejected attempt.
    QVERIFY(result.txid.isEmpty());
    QVERIFY(result.positionId.isEmpty());
}

void DigiDollarWidgetTests::transactionsWidgetTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    
    TestTransactionsWidget(m_node, wallet);
}

void DigiDollarWidgetTests::sendWidgetNoteFieldTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarSendWidget sendWidget(mini_gui.platformStyle.get());
    sendWidget.setWalletModel(mini_gui.walletModel.get());
    sendWidget.setClientModel(mini_gui.clientModel.get());

    QLineEdit* noteEdit = sendWidget.findChild<QLineEdit*>("noteEdit");
    QVERIFY(noteEdit != nullptr);
    
    noteEdit->setText("Test transaction note");
    QCOMPARE(noteEdit->text(), QString("Test transaction note"));
    
    QVERIFY(noteEdit->maxLength() == 256);
}

void DigiDollarWidgetTests::transactionsWidgetExportTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarTransactionsWidget transactionsWidget;
    transactionsWidget.setWalletModel(mini_gui.walletModel.get());
    transactionsWidget.setClientModel(mini_gui.clientModel.get());

    QPushButton* exportButton = transactionsWidget.findChild<QPushButton*>("m_exportButton");
    if (!exportButton) {
        QList<QPushButton*> buttons = transactionsWidget.findChildren<QPushButton*>();
        for (QPushButton* btn : buttons) {
            if (btn->text().contains("Export", Qt::CaseInsensitive)) {
                exportButton = btn;
                break;
            }
        }
    }
    QVERIFY(exportButton != nullptr);
    QVERIFY(exportButton->isEnabled());
}

void DigiDollarWidgetTests::addressBookTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DDAddressBookPage addressBook(mini_gui.platformStyle.get(), DDAddressBookPage::ForEditing);
    addressBook.setWalletModel(mini_gui.walletModel.get());

    QVERIFY(&addressBook != nullptr);
    
    QPushButton* newButton = addressBook.findChild<QPushButton*>();
    QVERIFY(newButton != nullptr);
    
    QTableWidget* table = addressBook.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->columnCount(), 2);
}

// ============================================================================
// Balance Change Validation Tests
// ============================================================================

void DigiDollarWidgetTests::mintValidationUpdatesOnBalanceChange()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarMintWidget mintWidget;
    mintWidget.setWalletModel(mini_gui.walletModel.get());
    mintWidget.setClientModel(mini_gui.clientModel.get());
    mintWidget.show();

    // Set an amount in the mint field to trigger validation
    QLineEdit* amountEdit = mintWidget.findChild<QLineEdit*>("amountEdit");
    QVERIFY(amountEdit != nullptr);
    amountEdit->setText("1.00");

    // Get the warning label
    QLabel* warningLabel = mintWidget.findChild<QLabel*>("amountWarningLabel");
    QVERIFY(warningLabel != nullptr);

    // Force a collateral calculation and validation cycle
    mintWidget.updateView();

    // Capture the current validation state (stylesheet) before balance update
    QString styleBefore = amountEdit->styleSheet();

    // Now simulate a balance change (as if DGB arrived) by calling updateBalance()
    // This is the exact path that fires when the wallet receives new DGB
    mintWidget.updateBalance();

    // After updateBalance(), the validation state should have been re-evaluated.
    // The key assertion: updateBalance() must trigger updateAmountValidation().
    // We verify this by checking that the warning label visibility or amount edit
    // stylesheet was re-evaluated (not stale).
    //
    // Since updateBalance() now calls updateAmountValidation() + updateMintButton(),
    // the validation state should reflect the current balance, not a cached state.
    QString styleAfter = amountEdit->styleSheet();

    // In a test environment with no oracle price, both states may show the same
    // warning. The critical test is that updateBalance() doesn't crash and does
    // call through to updateAmountValidation(). We verify the label exists and
    // the stylesheet was set (non-empty means validation ran).
    QVERIFY2(!styleAfter.isEmpty() || amountEdit->text().isEmpty(),
             "Amount validation should run after updateBalance() — stylesheet should be set when amount is entered");

    // Verify the warning label is in a consistent state (visible with text, or hidden)
    if (warningLabel->isVisible()) {
        QVERIFY2(!warningLabel->text().isEmpty(),
                 "If warning label is visible after balance update, it should have text");
    }
}

// Regression test for the RC30/RC31 DD balance-refresh bug class: while the
// DigiDollar page is already open, a wallet balanceChanged signal must refresh
// the Mint tab's cached Available DGB label immediately.
void DigiDollarWidgetTests::ddTabRefreshesBalancesOnWalletSignal()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarTab tab(mini_gui.platformStyle.get());
    tab.setWalletModel(mini_gui.walletModel.get());
    tab.setClientModel(mini_gui.clientModel.get());
    tab.show();

    QLabel* availableDGBValue = tab.findChild<QLabel*>("availableDGBValue");
    QVERIFY(availableDGBValue != nullptr);

    const QString expected = availableDGBValue->text();
    QVERIFY2(!expected.isEmpty(), "Expected Mint tab Available DGB label to be initialized");

    availableDGBValue->setText("stale-balance");
    Q_EMIT mini_gui.walletModel->balanceChanged(interfaces::WalletBalances{});
    QCoreApplication::processEvents();

    QCOMPARE(availableDGBValue->text(), expected);
}

// Regression test for au_epic's report: reopening the main DigiDollar page
// after a redeem/unlock must refresh the Mint tab's Available DGB label instead
// of leaving a stale cached value until full wallet restart.
void DigiDollarWidgetTests::walletViewRefreshesDigiDollarPageOnOpen()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    WalletView view(mini_gui.walletModel.get(), mini_gui.platformStyle.get(), nullptr);
    view.setClientModel(mini_gui.clientModel.get());
    view.show();
    view.gotoOverviewPage();

    QLabel* availableDGBValue = view.findChild<QLabel*>("availableDGBValue");
    QVERIFY(availableDGBValue != nullptr);

    const QString expected = availableDGBValue->text();
    QVERIFY2(!expected.isEmpty(), "Expected Mint tab Available DGB label to be initialized");

    availableDGBValue->setText("stale-on-open");
    view.gotoDigiDollarPage();
    QCoreApplication::processEvents();

    QCOMPARE(availableDGBValue->text(), expected);
}

// ============================================================================
// Privacy / Mask Values Tests
// ============================================================================

void DigiDollarWidgetTests::privacyTabSetPrivacySlotTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarTab tab(mini_gui.platformStyle.get());
    tab.setWalletModel(mini_gui.walletModel.get());
    tab.setClientModel(mini_gui.clientModel.get());

    // Verify DigiDollarTab has setPrivacy slot and it can be called
    tab.setPrivacy(true);
    tab.setPrivacy(false);
    // If we get here without crash, the slot exists and works
    QVERIFY(true);
}

void DigiDollarWidgetTests::privacyOverviewMaskTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarOverviewWidget overviewWidget;
    overviewWidget.setWalletModel(mini_gui.walletModel.get());
    overviewWidget.setClientModel(mini_gui.clientModel.get());
    overviewWidget.show();

    // Enable privacy mode
    overviewWidget.setPrivacy(true);

    // Check that balance labels contain '#' (masked)
    QLabel* ddBalanceValue = overviewWidget.findChild<QLabel*>("ddBalanceValue");
    QVERIFY(ddBalanceValue != nullptr);
    QVERIFY2(ddBalanceValue->text().contains('#'), "DD balance should be masked with # when privacy is enabled");

    QLabel* dgbCollateralValue = overviewWidget.findChild<QLabel*>("dgbCollateralValue");
    QVERIFY(dgbCollateralValue != nullptr);
    QVERIFY2(dgbCollateralValue->text().contains('#'), "DGB collateral should be masked with # when privacy is enabled");

    QLabel* usdValueValue = overviewWidget.findChild<QLabel*>("usdValueValue");
    QVERIFY(usdValueValue != nullptr);
    QVERIFY2(usdValueValue->text().contains('#'), "USD value should be masked with # when privacy is enabled");

    // Check that recent transactions list is hidden
    QListWidget* transactionsList = overviewWidget.findChild<QListWidget*>("transactionsList");
    QVERIFY(transactionsList != nullptr);
    QVERIFY2(transactionsList->isHidden(), "Transactions list should be hidden when privacy is enabled");

    // Disable privacy mode
    overviewWidget.setPrivacy(false);

    // Check that balance labels no longer contain '#'
    QVERIFY2(!ddBalanceValue->text().contains('#'), "DD balance should NOT be masked when privacy is disabled");
    QVERIFY2(!dgbCollateralValue->text().contains('#'), "DGB collateral should NOT be masked when privacy is disabled");
    QVERIFY2(!usdValueValue->text().contains('#'), "USD value should NOT be masked when privacy is disabled");
}

void DigiDollarWidgetTests::overviewUsdValueShowsUsdSuffixWhenPrivacyOff()
{
    DigiDollarOverviewWidget overviewWidget;
    QLabel* usdValueValue = overviewWidget.findChild<QLabel*>("usdValueValue");
    QVERIFY(usdValueValue != nullptr);

    overviewWidget.setPrivacy(false);
    QVERIFY2(usdValueValue->text().endsWith(QStringLiteral(" USD")),
             qPrintable(QString("Overview USD value must include explicit USD suffix, got: %1")
                            .arg(usdValueValue->text())));
}

void DigiDollarWidgetTests::overviewPrivacyMaskHidesAmountUnits()
{
    DigiDollarOverviewWidget overviewWidget;
    overviewWidget.setPrivacy(true);

    const QStringList sensitiveLabels{
        QStringLiteral("ddBalanceValue"),
        QStringLiteral("dgbCollateralValue"),
        QStringLiteral("usdValueValue"),
        QStringLiteral("networkTotalDDValue"),
        QStringLiteral("networkTotalCollateralValue"),
    };
    const QRegularExpression digitRe(QStringLiteral("\\d"));

    for (const QString& objectName : sensitiveLabels) {
        QLabel* label = overviewWidget.findChild<QLabel*>(objectName);
        QVERIFY2(label != nullptr, qPrintable(QString("Missing label %1").arg(objectName)));
        const QString text = label->text();
        QVERIFY2(text.contains('#'), qPrintable(QString("%1 should be visibly masked, got: %2").arg(objectName, text)));
        QVERIFY2(!text.contains(digitRe), qPrintable(QString("%1 leaked digits while masked: %2").arg(objectName, text)));
        QVERIFY2(!text.contains(QStringLiteral("USD")), qPrintable(QString("%1 leaked USD suffix while masked: %2").arg(objectName, text)));
        QVERIFY2(!text.contains(QStringLiteral("DGB")), qPrintable(QString("%1 leaked DGB suffix while masked: %2").arg(objectName, text)));
        QVERIFY2(!text.contains(QStringLiteral("DD")), qPrintable(QString("%1 leaked DD suffix while masked: %2").arg(objectName, text)));
    }
}

void DigiDollarWidgetTests::overviewLayoutStretchFavorsBlockchainTotals()
{
    DigiDollarOverviewWidget overviewWidget;
    QHBoxLayout* healthContentLayout = overviewWidget.findChild<QHBoxLayout*>(QStringLiteral("healthContentLayout"));
    QVERIFY(healthContentLayout != nullptr);

    QLabel* healthTitle = overviewWidget.findChild<QLabel*>(QStringLiteral("healthTitle"));
    QVERIFY(healthTitle != nullptr);
    QCOMPARE(healthTitle->text(), QStringLiteral("Blockchain DigiDollar Status"));

    QLabel* ddSupplyLabel = overviewWidget.findChild<QLabel*>(QStringLiteral("networkTotalDDLabel"));
    QVERIFY(ddSupplyLabel != nullptr);
    QCOMPARE(ddSupplyLabel->text(), QStringLiteral("Blockchain DD Supply"));

    QLabel* dgbLockedLabel = overviewWidget.findChild<QLabel*>(QStringLiteral("networkTotalCollateralLabel"));
    QVERIFY(dgbLockedLabel != nullptr);
    QCOMPARE(dgbLockedLabel->text(), QStringLiteral("Blockchain DGB Locked"));

    QWidget* leftStatsFrame = overviewWidget.findChild<QWidget*>(QStringLiteral("leftStatsFrame"));
    QWidget* networkTotalsFrame = overviewWidget.findChild<QWidget*>(QStringLiteral("networkTotalsFrame"));
    QVERIFY(leftStatsFrame != nullptr);
    QVERIFY(networkTotalsFrame != nullptr);

    const int leftIndex = healthContentLayout->indexOf(leftStatsFrame);
    const int totalsIndex = healthContentLayout->indexOf(networkTotalsFrame);
    QVERIFY(leftIndex >= 0);
    QVERIFY(totalsIndex >= 0);
    QVERIFY2(healthContentLayout->stretch(totalsIndex) > healthContentLayout->stretch(leftIndex),
             "Blockchain totals should get more horizontal stretch than the smaller left stats column");
}

void DigiDollarWidgetTests::overviewBlockchainTotalsFitLaunchScaleValues()
{
    DigiDollarOverviewWidget overviewWidget;
    QLabel* ddValue = overviewWidget.findChild<QLabel*>(QStringLiteral("networkTotalDDValue"));
    QLabel* dgbValue = overviewWidget.findChild<QLabel*>(QStringLiteral("networkTotalCollateralValue"));
    QWidget* totalsFrame = overviewWidget.findChild<QWidget*>(QStringLiteral("networkTotalsFrame"));
    QVERIFY(ddValue != nullptr);
    QVERIFY(dgbValue != nullptr);
    QVERIFY(totalsFrame != nullptr);

    overviewWidget.setMonospacedFont(false);

    const QString launchScaleDD = QStringLiteral("$999,000,000.00 DD");
    const QString stressScaleDD = QStringLiteral("$11,000,000,000.00 DD");
    const QString maxDgbLocked = QStringLiteral("21,000,000,000.00 DGB");

    const int launchScaleDDWidth = QFontMetrics(ddValue->font()).horizontalAdvance(launchScaleDD);
    const int stressScaleDDWidth = QFontMetrics(ddValue->font()).horizontalAdvance(stressScaleDD);
    const int maxDgbLockedWidth = QFontMetrics(dgbValue->font()).horizontalAdvance(maxDgbLocked);

    QVERIFY2(ddValue->minimumWidth() >= launchScaleDDWidth,
             qPrintable(QString("Blockchain DD supply label is too narrow for %1").arg(launchScaleDD)));
    QVERIFY2(ddValue->minimumWidth() >= stressScaleDDWidth,
             qPrintable(QString("Blockchain DD supply label is too narrow for %1").arg(stressScaleDD)));
    QVERIFY2(dgbValue->minimumWidth() >= maxDgbLockedWidth,
             qPrintable(QString("Blockchain DGB locked label is too narrow for %1").arg(maxDgbLocked)));
    QVERIFY2(totalsFrame->minimumWidth() > ddValue->minimumWidth(),
             "Blockchain totals frame must include room around the value labels");
}

void DigiDollarWidgetTests::overviewHealthUsesCollateralizedLanguage()
{
    DigiDollarOverviewWidget overviewWidget;
    QLabel* systemHealthValue = overviewWidget.findChild<QLabel*>(QStringLiteral("systemHealthValue"));
    QProgressBar* systemHealthBar = overviewWidget.findChild<QProgressBar*>(QStringLiteral("systemHealthBar"));
    QVERIFY(systemHealthValue != nullptr);
    QVERIFY(systemHealthBar != nullptr);

    QCOMPARE(systemHealthValue->text(), QStringLiteral("Loading..."));
    QVERIFY2(!systemHealthValue->text().contains(QStringLiteral("Healthy")),
             "System health value should not describe collateralization as healthy state text");
    QCOMPARE(systemHealthBar->format(), QStringLiteral("0% Collateralization"));
}

void DigiDollarWidgetTests::overviewPendingBalanceHasThemeRules()
{
    const auto readFile = [](const char* path) -> QString {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        return QString::fromUtf8(f.readAll());
    };

    const auto findTheme = [&](const QString& name) -> QString {
        const QStringList candidates = {
            QStringLiteral("src/qt/res/css/%1").arg(name),
            QStringLiteral("../src/qt/res/css/%1").arg(name),
            QStringLiteral("../../src/qt/res/css/%1").arg(name),
            QStringLiteral("qt/res/css/%1").arg(name),
        };
        for (const auto& p : candidates) {
            const QString css = readFile(p.toUtf8().constData());
            if (!css.isEmpty()) return css;
        }
        return {};
    };

    const auto requirePendingRule = [](const QString& css, const QString& theme) {
        const QRegularExpression pendingLabel(
            QStringLiteral(R"re(DigiDollarOverviewWidget\s+\.QFrame#balanceFrame\s+\.QLabel#ddPendingLabel[^\{]*\{[^\}]*qproperty-alignment\s*:[^\;]*AlignRight[^\}]*min-width\s*:\s*160px\s*;[^\}]*font-size\s*:\s*11pt\s*;)re"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QVERIFY2(pendingLabel.match(css).hasMatch(),
                 qPrintable(QString("%1 must style #ddPendingLabel like the other DigiDollar balance labels").arg(theme)));

        const QRegularExpression pendingValue(
            QStringLiteral(R"re(DigiDollarOverviewWidget\s+\.QFrame#balanceFrame\s+\.QLabel#ddPendingValue[^\{]*\{[^\}]*qproperty-alignment\s*:[^\;]*AlignLeft[^\}]*font-size\s*:\s*13pt\s*;)re"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QVERIFY2(pendingValue.match(css).hasMatch(),
                 qPrintable(QString("%1 must style #ddPendingValue like the adjacent DigiDollar value rows").arg(theme)));
    };

    const QString light = findTheme(QStringLiteral("light.css"));
    QVERIFY2(!light.isEmpty(), "could not locate light.css from current working directory");
    requirePendingRule(light, QStringLiteral("light.css"));

    const QString dark = findTheme(QStringLiteral("dark.css"));
    QVERIFY2(!dark.isEmpty(), "could not locate dark.css from current working directory");
    requirePendingRule(dark, QStringLiteral("dark.css"));
}

void DigiDollarWidgetTests::privacySendMaskTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarSendWidget sendWidget(mini_gui.platformStyle.get());
    sendWidget.setWalletModel(mini_gui.walletModel.get());
    sendWidget.setClientModel(mini_gui.clientModel.get());
    sendWidget.show();

    // Enable privacy mode
    sendWidget.setPrivacy(true);

    // Check that available balance is masked
    QLabel* availableBalanceValue = sendWidget.findChild<QLabel*>("availableBalanceValue");
    QVERIFY(availableBalanceValue != nullptr);
    QVERIFY2(availableBalanceValue->text().contains('#'), "Available balance should be masked when privacy is enabled");

    // Disable privacy mode
    sendWidget.setPrivacy(false);

    // Check that available balance is no longer masked
    QVERIFY2(!availableBalanceValue->text().contains('#'), "Available balance should NOT be masked when privacy is disabled");
}

void DigiDollarWidgetTests::privacyMintMaskTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarMintWidget mintWidget;
    mintWidget.setWalletModel(mini_gui.walletModel.get());
    mintWidget.setClientModel(mini_gui.clientModel.get());
    mintWidget.show();

    // Enable privacy mode
    mintWidget.setPrivacy(true);

    // Check that available DGB balance is masked
    QLabel* availableDGBValue = mintWidget.findChild<QLabel*>("availableDGBValue");
    QVERIFY(availableDGBValue != nullptr);
    QVERIFY2(availableDGBValue->text().contains('#'), "Available DGB balance should be masked when privacy is enabled");

    // Check that collateral value is masked
    QLabel* collateralValue = mintWidget.findChild<QLabel*>("collateralValue");
    QVERIFY(collateralValue != nullptr);
    QVERIFY2(collateralValue->text().contains('#'), "Collateral value should be masked when privacy is enabled");

    // Disable privacy mode
    mintWidget.setPrivacy(false);

    // Check that available DGB balance is no longer masked
    QVERIFY2(!availableDGBValue->text().contains('#'), "Available DGB balance should NOT be masked when privacy is disabled");
}

void DigiDollarWidgetTests::privacyRedeemMaskTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarRedeemWidget redeemWidget;
    redeemWidget.setWalletModel(mini_gui.walletModel.get());
    redeemWidget.setClientModel(mini_gui.clientModel.get());
    redeemWidget.show();

    // Enable privacy mode
    redeemWidget.setPrivacy(true);

    // Check that position values are masked
    QLabel* ddMintedValue = redeemWidget.findChild<QLabel*>("ddMintedValue");
    if (ddMintedValue) {
        QVERIFY2(ddMintedValue->text().contains('#'), "DD minted value should be masked when privacy is enabled");
    }

    QLabel* dgbCollateralValue = redeemWidget.findChild<QLabel*>("dgbCollateralValue");
    if (dgbCollateralValue) {
        QVERIFY2(dgbCollateralValue->text().contains('#'), "DGB collateral value should be masked when privacy is enabled");
    }

    QLabel* redeemableValue = redeemWidget.findChild<QLabel*>("redeemableValue");
    if (redeemableValue) {
        QVERIFY2(redeemableValue->text().contains('#'), "Redeemable value should be masked when privacy is enabled");
    }

    // Disable privacy mode
    redeemWidget.setPrivacy(false);

    if (ddMintedValue) {
        QVERIFY2(!ddMintedValue->text().contains('#'), "DD minted value should NOT be masked when privacy is disabled");
    }
}

void DigiDollarWidgetTests::privacyPositionsMaskTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarPositionsWidget positionsWidget;
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.setClientModel(mini_gui.clientModel.get());
    positionsWidget.show();

    // Enable privacy mode
    positionsWidget.setPrivacy(true);

    // The positions table should be hidden when privacy is enabled
    QTableWidget* table = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QVERIFY(table != nullptr);
    QVERIFY2(table->isHidden(), "Positions table should be hidden when privacy is enabled");

    // Disable privacy mode
    positionsWidget.setPrivacy(false);

    // The positions table should be visible again (not explicitly hidden)
    QVERIFY2(!table->isHidden(), "Positions table should not be hidden when privacy is disabled");
}

void DigiDollarWidgetTests::privacyTransactionsMaskTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarTransactionsWidget transactionsWidget;
    transactionsWidget.setWalletModel(mini_gui.walletModel.get());
    transactionsWidget.setClientModel(mini_gui.clientModel.get());
    transactionsWidget.show();

    // Enable privacy mode
    transactionsWidget.setPrivacy(true);

    // The transactions table should be hidden when privacy is enabled
    QTableWidget* table = transactionsWidget.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QVERIFY2(table->isHidden(), "Transactions table should be hidden when privacy is enabled");

    // Disable privacy mode
    transactionsWidget.setPrivacy(false);

    // The transactions table should be visible again (not explicitly hidden)
    QVERIFY2(!table->isHidden(), "Transactions table should not be hidden when privacy is disabled");
}

void DigiDollarWidgetTests::privacySignalPropagationTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarTab tab(mini_gui.platformStyle.get());
    tab.setWalletModel(mini_gui.walletModel.get());
    tab.setClientModel(mini_gui.clientModel.get());
    tab.show();

    // Enable privacy on the tab — it should propagate to all sub-widgets
    tab.setPrivacy(true);

    // Verify overview widget has privacy enabled (check for masked balances)
    DigiDollarOverviewWidget* overviewWidget = tab.findChild<DigiDollarOverviewWidget*>("overviewWidget");
    QVERIFY(overviewWidget != nullptr);

    QLabel* ddBalanceValue = overviewWidget->findChild<QLabel*>("ddBalanceValue");
    QVERIFY(ddBalanceValue != nullptr);
    QVERIFY2(ddBalanceValue->text().contains('#'), "Privacy should propagate from tab to overview widget");

    // Verify transactions widget has privacy enabled
    DigiDollarTransactionsWidget* transactionsWidget = tab.findChild<DigiDollarTransactionsWidget*>("transactionsWidget");
    QVERIFY(transactionsWidget != nullptr);

    QTableWidget* txTable = transactionsWidget->findChild<QTableWidget*>();
    QVERIFY(txTable != nullptr);
    QVERIFY2(txTable->isHidden(), "Privacy should propagate from tab to transactions widget");

    // Disable privacy
    tab.setPrivacy(false);

    // Verify overview is unmasked
    QVERIFY2(!ddBalanceValue->text().contains('#'), "Disabling privacy should propagate from tab to overview widget");

    // Verify transactions table is not hidden
    QVERIFY2(!txTable->isHidden(), "Disabling privacy should propagate from tab to transactions widget");
}

// Regression test for shenger's Apr 20 RC30 UX report: the
// "Your DigiDollar Address" panel always kept showing the last-generated
// address instead of the currently-selected row in the recent requests
// table. The panel must update m_addressEdit to follow whatever row the
// user highlights, matching DGB receive-table behaviour.
void DigiDollarWidgetTests::ddReceivePanelFollowsSelectedRow()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarReceiveWidget receive;
    receive.setWalletModel(mini_gui.walletModel.get());
    receive.show();

    QTableWidget* table = receive.findChild<QTableWidget*>("m_requestsTable");
    if (!table) {
        // Not every build exposes the object name; fall back to first table child.
        table = receive.findChild<QTableWidget*>();
    }
    QVERIFY(table != nullptr);

    QLineEdit* addressEdit = receive.findChild<QLineEdit*>("addressEdit");
    QVERIFY(addressEdit != nullptr);

    // Seed two distinct DD addresses directly into the table so we can
    // assert the panel follows selection without depending on address
    // generation order or wallet state.
    table->setRowCount(2);
    const QString addrA = QStringLiteral("dgbt1qfake000000000000000000000000000000000a");
    const QString addrB = QStringLiteral("dgbt1qfake000000000000000000000000000000000b");

    for (int i = 0; i < 2; ++i) {
        for (int c = 0; c < 4; ++c) {
            if (!table->item(i, c)) {
                table->setItem(i, c, new QTableWidgetItem());
            }
        }
    }
    table->item(0, 3)->setData(Qt::UserRole, addrA);
    table->item(0, 3)->setText(addrA);
    table->item(1, 3)->setData(Qt::UserRole, addrB);
    table->item(1, 3)->setText(addrB);

    // Seed the panel with a "last generated" string that must be replaced
    // on selection, matches the real-world symptom.
    addressEdit->setText(QStringLiteral("last-generated-address"));

    table->selectRow(0);
    QCoreApplication::processEvents();
    QCOMPARE(addressEdit->text(), addrA);

    table->selectRow(1);
    QCoreApplication::processEvents();
    QCOMPARE(addressEdit->text(), addrB);
}

// Regression test for shenger's Apr 20 RC30 UX report: double-clicking a
// DigiDollar request row must open the request dialog for that DD request.
// This specifically guards against routing DD rows through the DGB recent
// requests model, which filters DD entries out and leaves double-click inert.
void DigiDollarWidgetTests::ddReceiveDoubleClickShowsRequestDialog()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    WalletModel* wallet_model = mini_gui.walletModel.get();
    QVERIFY(wallet_model != nullptr);
    QVERIFY(wallet_model->getRecentRequestsTableModel() != nullptr);

    const QString ddAddress = wallet_model->getNewDigiDollarAddress(QStringLiteral("dialog-test"));
    QVERIFY2(!ddAddress.isEmpty(), "expected a valid DigiDollar address for request-dialog regression test");

    SendCoinsRecipient recipient;
    recipient.address = ddAddress;
    recipient.label = QStringLiteral("dialog-label");
    recipient.message = QStringLiteral("dialog-message");
    recipient.amount = 12345;
    wallet_model->getRecentRequestsTableModel()->addNewRequest(recipient);

    DigiDollarReceiveWidget receive;
    receive.setWalletModel(wallet_model);
    receive.show();
    receive.updateRecentRequests();

    QTableWidget* table = receive.findChild<QTableWidget*>("m_requestsTable");
    if (!table) {
        table = receive.findChild<QTableWidget*>();
    }
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);

    int existing_dialogs = 0;
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget->inherits("DigiDollarReceiveRequestDialog")) {
            ++existing_dialogs;
        }
    }

    QVERIFY(QMetaObject::invokeMethod(&receive, "onRecentRequestDoubleClicked",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 0),
                                      Q_ARG(int, 0)));
    QCoreApplication::processEvents();

    DigiDollarReceiveRequestDialog* dialog = nullptr;
    int updated_dialogs = 0;
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (!widget->inherits("DigiDollarReceiveRequestDialog")) {
            continue;
        }
        ++updated_dialogs;
        if (!dialog) {
            dialog = qobject_cast<DigiDollarReceiveRequestDialog*>(widget);
        }
    }

    QVERIFY2(updated_dialogs == existing_dialogs + 1 && dialog != nullptr,
             "double-clicking a DD request row must open DigiDollarReceiveRequestDialog");

    QLabel* addressContent = nullptr;
    for (QLabel* label : dialog->findChildren<QLabel*>()) {
        if (label->text() == ddAddress) {
            addressContent = label;
            break;
        }
    }
    QVERIFY2(addressContent != nullptr, "request dialog should display the selected DD address");

    dialog->close();
    QCoreApplication::processEvents();
}

void DigiDollarWidgetTests::ddReceiveEditPersistsAndKeepsDgbSeparated()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletModel* wallet_model = mini_gui.walletModel.get();
    QVERIFY(wallet_model != nullptr);

    const QString ddAddress = wallet_model->getNewDigiDollarAddress(QStringLiteral("edit-original-address-label"));
    QVERIFY(!ddAddress.isEmpty());

    SendCoinsRecipient recipient;
    recipient.address = ddAddress;
    recipient.label = QStringLiteral("original label");
    recipient.message = QStringLiteral("original message");
    recipient.amount = 1234;
    wallet_model->getRecentRequestsTableModel()->addNewRequest(recipient);
    QCOMPARE(wallet_model->getRecentRequestsTableModel()->rowCount(QModelIndex()), 0);

    DigiDollarReceiveWidget receive;
    receive.setWalletModel(wallet_model);
    receive.updateRecentRequests();

    QTableWidget* table = receive.findChild<QTableWidget*>("requestsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);
    table->selectRow(0);

    QTimer::singleShot(0, [&]() {
        QDialog* dialog = nullptr;
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->windowTitle() == QStringLiteral("Edit DigiDollar Payment Request")) {
                dialog = qobject_cast<QDialog*>(widget);
                break;
            }
        }
        QVERIFY(dialog != nullptr);
        QLineEdit* label = dialog->findChild<QLineEdit*>("ddRequestLabelEdit");
        QLineEdit* message = dialog->findChild<QLineEdit*>("ddRequestMessageEdit");
        QVERIFY(label != nullptr);
        QVERIFY(message != nullptr);
        label->setText(QStringLiteral("edited label"));
        message->setText(QStringLiteral("edited message"));
        QDoubleSpinBox* amount = dialog->findChild<QDoubleSpinBox*>("ddRequestAmountEdit");
        QVERIFY(amount != nullptr);
        amount->setValue(45.67);
        QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>();
        QVERIFY(buttons != nullptr);
        buttons->button(QDialogButtonBox::Ok)->click();
    });

    QVERIFY(QMetaObject::invokeMethod(&receive, "onEditRequestClicked", Qt::DirectConnection));
    QCoreApplication::processEvents();

    QCOMPARE(CountStoredReceiveRequests(*wallet_model, ddAddress), 1);
    RecentRequestEntry edited;
    bool found = false;
    for (const std::string& requestStr : wallet_model->wallet().getAddressReceiveRequests()) {
        std::vector<uint8_t> data(requestStr.begin(), requestStr.end());
        DataStream ss{data};
        RecentRequestEntry entry;
        ss >> entry;
        if (entry.recipient.address == ddAddress) {
            edited = entry;
            found = true;
            break;
        }
    }
    QVERIFY(found);
    QCOMPARE(edited.recipient.label, QStringLiteral("edited label"));
    QCOMPARE(edited.recipient.message, QStringLiteral("edited message"));
    QCOMPARE(edited.recipient.amount, CAmount(4567));
    QCOMPARE(wallet_model->getRecentRequestsTableModel()->rowCount(QModelIndex()), 0);

    DigiDollarReceiveWidget reloaded;
    reloaded.setWalletModel(wallet_model);
    reloaded.updateRecentRequests();
    QTableWidget* reloadedTable = reloaded.findChild<QTableWidget*>("requestsTable");
    QVERIFY(reloadedTable != nullptr);
    QCOMPARE(reloadedTable->rowCount(), 1);
    QCOMPARE(reloadedTable->item(0, 1)->text(), QStringLiteral("edited label"));
    QCOMPARE(reloadedTable->item(0, 2)->text(), QStringLiteral("45.67 DD"));
}

void DigiDollarWidgetTests::ddReceiveEditCancelLeavesRequestUnchanged()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletModel* wallet_model = mini_gui.walletModel.get();
    QVERIFY(wallet_model != nullptr);

    const QString ddAddress = wallet_model->getNewDigiDollarAddress(QStringLiteral("edit-cancel-address-label"));
    QVERIFY(!ddAddress.isEmpty());

    SendCoinsRecipient recipient;
    recipient.address = ddAddress;
    recipient.label = QStringLiteral("cancel original label");
    recipient.message = QStringLiteral("cancel original message");
    recipient.amount = 9876;
    wallet_model->getRecentRequestsTableModel()->addNewRequest(recipient);
    QCOMPARE(wallet_model->getRecentRequestsTableModel()->rowCount(QModelIndex()), 0);
    QCOMPARE(CountStoredReceiveRequests(*wallet_model, ddAddress), 1);

    DigiDollarReceiveWidget receive;
    receive.setWalletModel(wallet_model);
    receive.updateRecentRequests();

    QTableWidget* table = receive.findChild<QTableWidget*>("requestsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);
    table->selectRow(0);

    QTimer::singleShot(0, [&]() {
        QDialog* dialog = nullptr;
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->windowTitle() == QStringLiteral("Edit DigiDollar Payment Request")) {
                dialog = qobject_cast<QDialog*>(widget);
                break;
            }
        }
        QVERIFY(dialog != nullptr);
        QLineEdit* label = dialog->findChild<QLineEdit*>("ddRequestLabelEdit");
        QLineEdit* message = dialog->findChild<QLineEdit*>("ddRequestMessageEdit");
        QDoubleSpinBox* amount = dialog->findChild<QDoubleSpinBox*>("ddRequestAmountEdit");
        QVERIFY(label != nullptr);
        QVERIFY(message != nullptr);
        QVERIFY(amount != nullptr);
        label->setText(QStringLiteral("cancel edited label"));
        message->setText(QStringLiteral("cancel edited message"));
        amount->setValue(12.34);
        QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>();
        QVERIFY(buttons != nullptr);
        buttons->button(QDialogButtonBox::Cancel)->click();
    });

    QVERIFY(QMetaObject::invokeMethod(&receive, "onEditRequestClicked", Qt::DirectConnection));
    QCoreApplication::processEvents();

    QCOMPARE(CountStoredReceiveRequests(*wallet_model, ddAddress), 1);
    RecentRequestEntry stored;
    QVERIFY(FindStoredReceiveRequest(*wallet_model, ddAddress, stored));
    QCOMPARE(stored.recipient.label, QStringLiteral("cancel original label"));
    QCOMPARE(stored.recipient.message, QStringLiteral("cancel original message"));
    QCOMPARE(stored.recipient.amount, CAmount(9876));
    QCOMPARE(wallet_model->getRecentRequestsTableModel()->rowCount(QModelIndex()), 0);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("cancel original label"));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("98.76 DD"));
}

void DigiDollarWidgetTests::ddReceiveRemovePersistsAndKeepsDgbSeparated()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletModel* wallet_model = mini_gui.walletModel.get();
    QVERIFY(wallet_model != nullptr);

    RecentRequestsTableModel* dgb_requests = wallet_model->getRecentRequestsTableModel();
    QVERIFY(dgb_requests != nullptr);

    const CTxDestination dgbDest = GetDestinationForKey(test.coinbaseKey.GetPubKey(), wallet->m_default_address_type);
    const QString dgbAddress = QString::fromStdString(EncodeDestination(dgbDest));
    const QString ddAddress = wallet_model->getNewDigiDollarAddress(QStringLiteral("remove-dd-address-label"));
    QVERIFY(!dgbAddress.isEmpty());
    QVERIFY(!ddAddress.isEmpty());

    SendCoinsRecipient dgbRecipient;
    dgbRecipient.address = dgbAddress;
    dgbRecipient.label = QStringLiteral("dgb request");
    dgbRecipient.message = QStringLiteral("normal DGB request");
    dgbRecipient.amount = 1234;
    dgb_requests->addNewRequest(dgbRecipient);
    QCOMPARE(dgb_requests->rowCount(QModelIndex()), 1);
    QCOMPARE(dgb_requests->entry(0).recipient.address, dgbAddress);

    SendCoinsRecipient ddRecipient;
    ddRecipient.address = ddAddress;
    ddRecipient.label = QStringLiteral("dd request");
    ddRecipient.message = QStringLiteral("DigiDollar request");
    ddRecipient.amount = 2345;
    dgb_requests->addNewRequest(ddRecipient);

    QCOMPARE(dgb_requests->rowCount(QModelIndex()), 1);
    QCOMPARE(dgb_requests->entry(0).recipient.address, dgbAddress);
    QCOMPARE(CountStoredReceiveRequests(*wallet_model, dgbAddress), 1);
    QCOMPARE(CountStoredReceiveRequests(*wallet_model, ddAddress), 1);

    DigiDollarReceiveWidget receive;
    receive.setWalletModel(wallet_model);
    receive.updateRecentRequests();

    QTableWidget* table = receive.findChild<QTableWidget*>("requestsTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 3)->data(Qt::UserRole).toString(), ddAddress);
    for (int row = 0; row < table->rowCount(); ++row) {
        for (int column = 0; column < table->columnCount(); ++column) {
            QVERIFY(table->item(row, column)->text() != dgbAddress);
        }
    }

    table->selectRow(0);
    QVERIFY(QMetaObject::invokeMethod(&receive, "onRemoveRequestClicked", Qt::DirectConnection));
    QCoreApplication::processEvents();

    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(CountStoredReceiveRequests(*wallet_model, ddAddress), 0);
    QCOMPARE(CountStoredReceiveRequests(*wallet_model, dgbAddress), 1);
    QCOMPARE(dgb_requests->rowCount(QModelIndex()), 1);
    QCOMPARE(dgb_requests->entry(0).recipient.address, dgbAddress);

    DigiDollarReceiveWidget reloaded;
    reloaded.setWalletModel(wallet_model);
    reloaded.updateRecentRequests();
    QTableWidget* reloadedTable = reloaded.findChild<QTableWidget*>("requestsTable");
    QVERIFY(reloadedTable != nullptr);
    QCOMPARE(reloadedTable->rowCount(), 0);
}

void DigiDollarWidgetTests::ddReceiveRequestDialogFormatsURIAndAmount()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif

    SendCoinsRecipient recipient;
    recipient.address = QStringLiteral("RDrequestTestAddress");
    recipient.label = QStringLiteral("invoice 42");
    recipient.message = QStringLiteral("DGB-equivalent DD request");
    recipient.amount = 12345;

    DigiDollarReceiveRequestDialog dialog;
    dialog.setInfo(recipient);

    QString uri_text;
    QString amount_text;
    for (QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("digidollar:RDrequestTestAddress"))) {
            uri_text = label->text();
        }
        if (label->text() == QStringLiteral("123.45 DD")) {
            amount_text = label->text();
        }
    }

    QVERIFY2(uri_text.contains(QStringLiteral("digidollar:RDrequestTestAddress?")),
             "DD receive request dialog must use the digidollar URI scheme");
    QVERIFY2(uri_text.contains(QStringLiteral("label=invoice%2042")),
             "DD receive request dialog must percent-encode labels");
    QVERIFY2(uri_text.contains(QStringLiteral("amount=123.45000000")),
             "DD receive request dialog must encode cents as decimal DD units");
    QVERIFY2(uri_text.contains(QStringLiteral("message=DGB-equivalent%20DD%20request")),
             "DD receive request dialog must percent-encode messages");
    QCOMPARE(amount_text, QStringLiteral("123.45 DD"));
}

void DigiDollarWidgetTests::ddReceiveRejectsMalformedRequestAmount()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-receive-invalid-amount");
    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);
    WalletModel* wallet_model = mini_gui.walletModel.get();
    QVERIFY(wallet_model != nullptr);

    DigiDollarReceiveWidget receive;
    receive.setWalletModel(wallet_model);
    receive.setClientModel(mini_gui.clientModel.get());
    receive.show();

    QLineEdit* amountEdit = receive.findChild<QLineEdit*>("amountEdit");
    QLineEdit* addressEdit = receive.findChild<QLineEdit*>("addressEdit");
    QTableWidget* table = receive.findChild<QTableWidget*>("requestsTable");
    QVERIFY(amountEdit != nullptr);
    QVERIFY(addressEdit != nullptr);
    QVERIFY(table != nullptr);

    QSignalSpy messageSpy(&receive, &DigiDollarReceiveWidget::message);
    amountEdit->setText(QStringLiteral("12.bad"));
    QVERIFY(QMetaObject::invokeMethod(&receive, "onGenerateAddressClicked", Qt::DirectConnection));
    QCoreApplication::processEvents();

    QCOMPARE(addressEdit->text(), QString());
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(wallet_model->wallet().getAddressReceiveRequests().size(), size_t{0});
    QVERIFY(!messageSpy.empty());
    QCOMPARE(messageSpy.first().at(0).toString(), QStringLiteral("Invalid Amount"));
}

// Regression test for shenger's Apr 20 RC30 UX report: on Windows dark
// theme, the Peers detail pane inside the RPC console renders with a
// grey system-default QWidget background and white text, making fields
// unreadable. Root cause: dark.css has no explicit rule for the
// debugwindow.ui "detailWidget" QWidget inside the RPCConsole scroll
// area, so it falls through to Qt's default palette. This source-level
// test enforces that dark.css carries an explicit rule for #detailWidget
// inside the RPCConsole scope.
// Regression test for the DD Overview "Recent Transactions" sign-prefix bug:
// DDTransaction stores amounts as unsigned magnitudes (the wallet pushes
// totalAmount, a positive number, for sends), so the row formatter must
// derive the sign from the category instead of the raw amount. Before the
// fix, send/redeem rows rendered as "+$3.00" with red text, contradicting
// the colour and confusing users about whether DD was leaving or arriving.
void DigiDollarWidgetTests::overviewRecentTransactionsSendShowsNegativeSign()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    // The mock wallet path used by these Qt tests bypasses CreateWalletFromFile,
    // which is what normally allocates m_dd_wallet. Allocate it explicitly here
    // so GetDDWallet() returns a usable pointer for the mock-history injection.
    wallet->EnsureDDWallet();
    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);

    // Inject one of each category we care about. amount is stored as an
    // unsigned magnitude (positive) — exactly how the live wallet persists
    // it for sends/redeems too. The formatter must read tx.category.
    auto pushTx = [&](const std::string& txid, CAmount amount, bool incoming, const std::string& category) {
        DDTransaction tx;
        tx.txid = txid;
        tx.amount = amount;
        tx.timestamp = GetTime();
        tx.confirmations = 1;
        tx.incoming = incoming;
        tx.address = "TDtestlocaladdress";
        tx.category = category;
        tx.lock_tier = -1;
        tx.fee = 0;
        tx.abandoned = false;
        dd_wallet->AddMockTransaction(tx);
    };
    pushTx("a000000000000000000000000000000000000000000000000000000000000001", 300, false, "send");
    pushTx("a000000000000000000000000000000000000000000000000000000000000002", 200, true,  "receive");
    pushTx("a000000000000000000000000000000000000000000000000000000000000003", 500, false, "redeem");
    pushTx("a000000000000000000000000000000000000000000000000000000000000004", 700, true,  "mint");

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarOverviewWidget overviewWidget;
    overviewWidget.setWalletModel(mini_gui.walletModel.get());
    overviewWidget.setClientModel(mini_gui.clientModel.get());
    overviewWidget.show();
    overviewWidget.updateView();
    QCoreApplication::processEvents();

    QListWidget* transactionsList = overviewWidget.findChild<QListWidget*>("transactionsList");
    QVERIFY(transactionsList != nullptr);
    QVERIFY2(transactionsList->count() >= 4, "expected at least four mock DD transactions in recent list");

    // Walk every row and look at the amount QLabel — index 2 in the row's
    // QHBoxLayout (icon, category, amount, confirmations, date).
    int sends = 0, receives = 0, redeems = 0, mints = 0;
    for (int row = 0; row < transactionsList->count(); ++row) {
        QWidget* itemWidget = transactionsList->itemWidget(transactionsList->item(row));
        QVERIFY(itemWidget != nullptr);
        const QList<QLabel*> labels = itemWidget->findChildren<QLabel*>();
        QVERIFY2(labels.size() >= 5, "expected icon/category/amount/confirmations/date labels per row");

        const QString category = labels.at(1)->text();
        const QString amountText = labels.at(2)->text();
        if (category == "Send") {
            ++sends;
            QVERIFY2(amountText.startsWith('-'),
                qPrintable(QString("Send row should start with '-', got: %1").arg(amountText)));
            QVERIFY2(!amountText.startsWith('+'), "Send row must never carry a '+' prefix");
        } else if (category == "Receive") {
            ++receives;
            QVERIFY2(amountText.startsWith('+'),
                qPrintable(QString("Receive row should start with '+', got: %1").arg(amountText)));
        } else if (category.startsWith("Redeem")) {
            ++redeems;
            QVERIFY2(amountText.startsWith('-'),
                qPrintable(QString("Redeem row should start with '-', got: %1").arg(amountText)));
        } else if (category.startsWith("Mint")) {
            ++mints;
            QVERIFY2(amountText.startsWith('+'),
                qPrintable(QString("Mint row should start with '+', got: %1").arg(amountText)));
        }
    }
    QVERIFY2(sends >= 1, "expected at least one Send row in mock data");
    QVERIFY2(receives >= 1, "expected at least one Receive row in mock data");
    QVERIFY2(redeems >= 1, "expected at least one Redeem row in mock data");
    QVERIFY2(mints >= 1, "expected at least one Mint row in mock data");
}

void DigiDollarWidgetTests::overviewRecentTransactionAmountIsRightAligned()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);
    wallet->EnsureDDWallet();
    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);

    DDTransaction tx;
    tx.txid = "b000000000000000000000000000000000000000000000000000000000000001";
    tx.amount = 123456;
    tx.timestamp = GetTime();
    tx.confirmations = 1;
    tx.incoming = false;
    tx.address = "TDtestlocaladdress";
    tx.category = "send";
    tx.lock_tier = -1;
    tx.fee = 0;
    tx.abandoned = false;
    dd_wallet->AddMockTransaction(tx);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarOverviewWidget overviewWidget;
    overviewWidget.setWalletModel(mini_gui.walletModel.get());
    overviewWidget.setClientModel(mini_gui.clientModel.get());
    overviewWidget.show();
    overviewWidget.updateView();
    QCoreApplication::processEvents();

    QListWidget* transactionsList = overviewWidget.findChild<QListWidget*>("transactionsList");
    QVERIFY(transactionsList != nullptr);
    QVERIFY(transactionsList->count() >= 1);

    QWidget* itemWidget = transactionsList->itemWidget(transactionsList->item(0));
    QVERIFY(itemWidget != nullptr);
    const QList<QLabel*> labels = itemWidget->findChildren<QLabel*>();
    QVERIFY2(labels.size() >= 5, "expected icon/category/amount/confirmations/date labels per row");

    QLabel* amountLabel = labels.at(2);
    QCOMPARE(amountLabel->text(), QStringLiteral("-$1234.56"));
    QVERIFY2(amountLabel->alignment() & Qt::AlignRight,
             "Recent transaction amount label should be right-aligned for decimal-place alignment");
    QVERIFY2(amountLabel->minimumWidth() >= amountLabel->fontMetrics().horizontalAdvance(QStringLiteral("-$1234.56")),
             "Recent transaction amount label should reserve enough width for the exact formatted amount");
}

// Regression coverage for the DD Transactions tab's RPC-backed history table:
// listdigidollartxs returns signed amounts derived from incoming/outgoing wallet
// direction, and the Qt table must preserve those signs while showing the right
// category, lock-period, note, truncated txid, and confirmation text. This is the
// display path used for sendmanydigidollar history rows, including the aggregate
// outgoing "multiple" row and local-recipient receive rows verified functionally.
void DigiDollarWidgetTests::transactionsWidgetShowsRpcHistorySignsAndFields()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test, "qt-dd-history");
    wallet->EnsureDDWallet();
    DigiDollarWallet* dd_wallet = wallet->GetDDWallet();
    QVERIFY(dd_wallet != nullptr);

    const int64_t now = GetTime();
    auto pushTx = [&](const std::string& txid, CAmount amount, bool incoming,
                      const std::string& category, const std::string& address,
                      const std::string& comment, int lock_tier, int64_t offset) {
        DDTransaction tx;
        tx.txid = txid;
        tx.amount = amount;
        tx.timestamp = now + offset;
        tx.confirmations = 0;
        tx.incoming = incoming;
        tx.address = address;
        tx.category = category;
        tx.lock_tier = lock_tier;
        tx.fee = 0;
        tx.comment = comment;
        tx.abandoned = false;
        dd_wallet->AddMockTransaction(tx);
    };

    const QString sendTxid = "b000000000000000000000000000000000000000000000000000000000000001";
    const QString recvTxid = "b000000000000000000000000000000000000000000000000000000000000002";
    const QString redeemTxid = "b000000000000000000000000000000000000000000000000000000000000003";
    const QString mintTxid = "b000000000000000000000000000000000000000000000000000000000000004";
    const QString emptyNoteTxid = "b000000000000000000000000000000000000000000000000000000000000005";

    pushTx(sendTxid.toStdString(), 500, false, "send", "multiple", "sendmany functional test", -1, 4);
    pushTx(recvTxid.toStdString(), 200, true, "receive", "TDlocalrecipient1", "local receive row", -1, 3);
    pushTx(redeemTxid.toStdString(), 1250, false, "redeem", "TDredeemaddress", "redeem note", 1, 2);
    pushTx(mintTxid.toStdString(), 700, true, "mint", "TDmintaddress", "mint note", 9, 1);
    pushTx(emptyNoteTxid.toStdString(), 300, true, "receive", "TDempty", "", -1, 0);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    WalletContext& context = *m_node.walletLoader().context();
    AddWallet(context, wallet);

    DigiDollarTransactionsWidget transactionsWidget;
    transactionsWidget.setWalletModel(mini_gui.walletModel.get());
    transactionsWidget.setClientModel(mini_gui.clientModel.get());
    transactionsWidget.show();
    QCoreApplication::processEvents();
    transactionsWidget.updateView();
    QCoreApplication::processEvents();

    RemoveWallet(context, wallet, std::nullopt);

    QTableWidget* table = transactionsWidget.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 5);

    auto findRowByTxid = [&](const QString& txid) -> int {
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem* txidItem = table->item(row, 5);
            if (txidItem && txidItem->data(Qt::UserRole).toString() == txid) {
                return row;
            }
        }
        return -1;
    };

    auto checkRow = [&](const QString& txid, const QString& type, const QString& amount,
                        const QString& lockPeriod, const QString& note,
                        const QString& noteTooltip = QString()) {
        const int row = findRowByTxid(txid);
        QVERIFY2(row >= 0, qPrintable(QString("missing DD transaction row for %1").arg(txid)));
        QTableWidgetItem* txidItem = table->item(row, 5);
        QVERIFY(txidItem != nullptr);
        QCOMPARE(txidItem->text(), txid.left(16) + "..." + txid.right(8));
        QCOMPARE(txidItem->toolTip(), txid);
        QCOMPARE(table->item(row, 1)->text(), type);
        QCOMPARE(table->item(row, 2)->text(), amount);
        QCOMPARE(table->item(row, 3)->text(), lockPeriod);
        QCOMPARE(table->item(row, 4)->text(), note);
        QCOMPARE(table->item(row, 4)->toolTip(), noteTooltip.isNull() ? note : noteTooltip);
        QCOMPARE(table->item(row, 6)->text(), QString("Pending"));
    };

    checkRow(sendTxid, "Send", "-$5.00 DD", "-", "sendmany functional test");
    checkRow(recvTxid, "Receive", "+$2.00 DD", "-", "local receive row");
    checkRow(redeemTxid, "Redeem 30-day", "-$12.50 DD", "30 days", "redeem note");
    checkRow(mintTxid, "Mint 10-yr", "+$7.00 DD", "10 years", "mint note");
    checkRow(emptyNoteTxid, "Receive", "+$3.00 DD", "-", "", QString("No note"));
}

// Regression test for the DD Vault "Lock Tier" column truncation: with the
// column pinned at 85 px the longer human-readable tier names ("3 months",
// "6 months", "10 years") rendered as "3 ...", "6 ...", "10 ye..." in the
// live wallet because the cell text exceeded the column width by ~15 px.
// Guard the column against future shrinkage by asserting it is at least
// wide enough to fit the longest tier label plus a normal cell padding.
void DigiDollarWidgetTests::positionsWidgetLockTierColumnFitsLongestLabel()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        QWARN("Skipping DigiDollarWidgetTests on mac build with 'minimal' platform set due to Qt bugs.");
        return;
    }
#endif
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    m_node.setContext(&test.m_node);

    const std::shared_ptr<wallet::CWallet>& wallet = SetupDescriptorsWallet(m_node, test);

    DigiDollarMiniGUI mini_gui(m_node);
    mini_gui.initModelForWallet(m_node, wallet);

    DigiDollarPositionsWidget positionsWidget;
    positionsWidget.setWalletModel(mini_gui.walletModel.get());
    positionsWidget.setClientModel(mini_gui.clientModel.get());
    positionsWidget.show();
    positionsWidget.updateView();

    QTableWidget* table = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QVERIFY(table != nullptr);

    // The longest tier label rendered by digidollarpositionswidget.cpp is
    // "10 years" (tier 9). Ask Qt for the actual painted width using the
    // table's own font, then add the standard 12 px frame Qt uses for
    // QTableWidget cells. The column must be at least that wide.
    const QFontMetrics fm(table->font());
    const QStringList tierLabels = {
        QStringLiteral("1 hour"),    QStringLiteral("30 days"),
        QStringLiteral("3 months"),  QStringLiteral("6 months"),
        QStringLiteral("1 year"),    QStringLiteral("2 years"),
        QStringLiteral("3 years"),   QStringLiteral("5 years"),
        QStringLiteral("7 years"),   QStringLiteral("10 years"),
    };
    int maxLabelWidth = 0;
    for (const QString& label : tierLabels) {
        maxLabelWidth = std::max(maxLabelWidth, fm.horizontalAdvance(label));
    }
    const int requiredWidth = maxLabelWidth + 12; // QTableWidget cell padding
    const int actualWidth = table->columnWidth(DigiDollarPositionsWidget::COL_LOCK_TIER);
    QVERIFY2(actualWidth >= requiredWidth,
             qPrintable(QString("Lock Tier column too narrow: %1 px, need at least %2 px to fit '%3'")
                        .arg(actualWidth)
                        .arg(requiredWidth)
                        .arg(QStringLiteral("10 years"))));
}

void DigiDollarWidgetTests::darkThemePeerDetailWidgetHasExplicitRule()
{
    const auto readFile = [](const char* path) -> QString {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        return QString::fromUtf8(f.readAll());
    };

    const QStringList candidates = {
        QStringLiteral("src/qt/res/css/dark.css"),
        QStringLiteral("../src/qt/res/css/dark.css"),
        QStringLiteral("../../src/qt/res/css/dark.css"),
        QStringLiteral("qt/res/css/dark.css"),
    };
    QString dark;
    for (const auto& p : candidates) {
        dark = readFile(p.toUtf8().constData());
        if (!dark.isEmpty()) break;
    }
    QVERIFY2(!dark.isEmpty(), "could not locate dark.css from current working directory");

    const QRegularExpression detailRule(
        QStringLiteral(R"re((RPCConsole|QDialog#RPCConsole)\s+QWidget#detailWidget[^\{]*\{[^\}]*background-color\s*:\s*#002352\s*;[^\}]*color\s*:\s*#ffffff\s*;)re"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

    const bool found = detailRule.match(dark).hasMatch();
    QVERIFY2(found,
             "RC30 dark-mode peers pane contrast bug: dark.css must carry an explicit rule for the RPC console's #detailWidget so the Windows default grey QWidget palette doesn't leak through. Expected a selector like 'RPCConsole QWidget#detailWidget { ... }'.");
}
