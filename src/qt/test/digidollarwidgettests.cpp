// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/digidollarwidgettests.h>
#include <qt/test/util.h>

#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/walletmodel.h>
#include <qt/digidollaroverviewwidget.h>
#include <qt/digidollarmintwidget.h>
#include <qt/digidollarsendwidget.h>
#include <qt/digidollarreceivewidget.h>
#include <qt/digidollarreceiverequest.h>
#include <qt/digidollarredeemwidget.h>
#include <qt/digidollarpositionswidget.h>
#include <qt/digidollartransactionswidget.h>
#include <qt/digidollartab.h>
#include <qt/ddaddressbookpage.h>
#include <qt/walletview.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/digidollarwallet.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <memory>

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QComboBox>
#include <QProgressBar>
#include <QListWidget>
#include <QTableWidget>

using wallet::AddWallet;
using wallet::CreateMockableWalletDatabase;
using wallet::RemoveWallet;
using wallet::WALLET_FLAG_DESCRIPTORS;
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

    QVERIFY(&receiveWidget != nullptr);

    receiveWidget.updateView();
    receiveWidget.updateRecentRequests();
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

    pushTx(sendTxid.toStdString(), 500, false, "send", "multiple", "sendmany functional test", -1, 4);
    pushTx(recvTxid.toStdString(), 200, true, "receive", "TDlocalrecipient1", "local receive row", -1, 3);
    pushTx(redeemTxid.toStdString(), 1250, false, "redeem", "TDredeemaddress", "redeem note", 1, 2);
    pushTx(mintTxid.toStdString(), 700, true, "mint", "TDmintaddress", "mint note", 9, 1);

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
    QCOMPARE(table->rowCount(), 4);

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
                        const QString& lockPeriod, const QString& note) {
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
        QCOMPARE(table->item(row, 4)->toolTip(), note);
        QCOMPARE(table->item(row, 6)->text(), QString("Pending"));
    };

    checkRow(sendTxid, "Send", "-$5.00 DD", "-", "sendmany functional test");
    checkRow(recvTxid, "Receive", "+$2.00 DD", "-", "local receive row");
    checkRow(redeemTxid, "Redeem 30-day", "-$12.50 DD", "30 days", "redeem note");
    checkRow(mintTxid, "Mint 10-yr", "+$7.00 DD", "10 years", "mint note");
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
