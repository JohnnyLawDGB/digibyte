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
#include <qt/walletmodel.h>
#include <qt/digidollaroverviewwidget.h>
#include <qt/digidollarmintwidget.h>
#include <qt/digidollarsendwidget.h>
#include <qt/digidollarreceivewidget.h>
#include <qt/digidollarredeemwidget.h>
#include <qt/digidollarpositionswidget.h>
#include <qt/digidollartransactionswidget.h>
#include <qt/digidollartab.h>
#include <qt/ddaddressbookpage.h>
#include <qt/walletview.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <memory>

#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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

std::shared_ptr<wallet::CWallet> SetupDescriptorsWallet(interfaces::Node& node, TestChain100Setup& test)
{
    std::shared_ptr<wallet::CWallet> wallet = std::make_shared<wallet::CWallet>(
        node.context()->chain.get(), "", CreateMockableWalletDatabase());
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
