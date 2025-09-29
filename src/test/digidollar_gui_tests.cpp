// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#ifdef ENABLE_WALLET

#include <qt/test/util.h>
#include <qt/digidollartab.h>
#include <qt/digidollaroverviewwidget.h>
#include <qt/digidollarsendwidget.h>
#include <qt/digidollarmintwidget.h>
#include <qt/digidollarredeemwidget.h>
#include <qt/digidollarpositionswidget.h>
#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <test/util/setup_common.h>
#include <interfaces/node.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QValidator>

BOOST_FIXTURE_TEST_SUITE(digidollar_gui_tests, TestSetup)

class DigiDollarGUITestFixture : public TestChain100Setup
{
public:
    DigiDollarGUITestFixture()
    {
        m_wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), m_args, GetWalletContext());

        // Initialize Qt application if not already done
        if (!QApplication::instance()) {
            int argc = 1;
            char name[] = "digibyte-qt-test";
            char* argv[] = {name, nullptr};
            app = std::make_unique<QApplication>(argc, argv);
        }
    }

    ~DigiDollarGUITestFixture()
    {
        // Clean up wallet
        if (m_wallet) {
            m_wallet.reset();
        }
    }

    std::shared_ptr<CWallet> m_wallet;
    std::unique_ptr<QApplication> app;
};

BOOST_FIXTURE_TEST_CASE(digidollar_tab_creation, DigiDollarGUITestFixture)
{
    // Test DigiDollarTab creation
    DigiDollarTab tab;

    // Verify tab widget is created
    BOOST_CHECK(tab.findChild<QTabWidget*>() != nullptr);

    // Verify all sub-widgets are created
    BOOST_CHECK(tab.findChild<DigiDollarOverviewWidget*>() != nullptr);
    BOOST_CHECK(tab.findChild<DigiDollarSendWidget*>() != nullptr);
    BOOST_CHECK(tab.findChild<DigiDollarMintWidget*>() != nullptr);
    BOOST_CHECK(tab.findChild<DigiDollarRedeemWidget*>() != nullptr);
    BOOST_CHECK(tab.findChild<DigiDollarPositionsWidget*>() != nullptr);

    // Verify tab structure
    QTabWidget* tabWidget = tab.findChild<QTabWidget*>();
    BOOST_CHECK_EQUAL(tabWidget->count(), 5);

    // Verify tab names
    BOOST_CHECK_EQUAL(tabWidget->tabText(0).toStdString(), "Overview");
    BOOST_CHECK_EQUAL(tabWidget->tabText(1).toStdString(), "Send");
    BOOST_CHECK_EQUAL(tabWidget->tabText(2).toStdString(), "Mint");
    BOOST_CHECK_EQUAL(tabWidget->tabText(3).toStdString(), "Redeem");
    BOOST_CHECK_EQUAL(tabWidget->tabText(4).toStdString(), "Positions");
}

BOOST_FIXTURE_TEST_CASE(digidollar_overview_widget, DigiDollarGUITestFixture)
{
    DigiDollarOverviewWidget overview;

    // Test balance display labels
    QLabel* ddBalanceLabel = overview.findChild<QLabel*>("ddBalanceLabel");
    QLabel* dgbCollateralLabel = overview.findChild<QLabel*>("dgbCollateralLabel");
    QLabel* oraclePriceLabel = overview.findChild<QLabel*>("oraclePriceLabel");
    QLabel* systemHealthLabel = overview.findChild<QLabel*>("systemHealthLabel");
    QLabel* dcaLevelLabel = overview.findChild<QLabel*>("dcaLevelLabel");
    QLabel* errLevelLabel = overview.findChild<QLabel*>("errLevelLabel");

    BOOST_CHECK(ddBalanceLabel != nullptr);
    BOOST_CHECK(dgbCollateralLabel != nullptr);
    BOOST_CHECK(oraclePriceLabel != nullptr);
    BOOST_CHECK(systemHealthLabel != nullptr);
    BOOST_CHECK(dcaLevelLabel != nullptr);
    BOOST_CHECK(errLevelLabel != nullptr);

    // Test initial values
    BOOST_CHECK_EQUAL(ddBalanceLabel->text().toStdString(), "0.00000000 DD");
    BOOST_CHECK_EQUAL(dgbCollateralLabel->text().toStdString(), "0.00000000 DGB");
    BOOST_CHECK_EQUAL(oraclePriceLabel->text().toStdString(), "Loading...");
    BOOST_CHECK_EQUAL(systemHealthLabel->text().toStdString(), "Healthy");
}

BOOST_FIXTURE_TEST_CASE(digidollar_send_widget, DigiDollarGUITestFixture)
{
    DigiDollarSendWidget sendWidget;

    // Test UI components
    QLineEdit* addressEdit = sendWidget.findChild<QLineEdit*>("addressEdit");
    QLineEdit* amountEdit = sendWidget.findChild<QLineEdit*>("amountEdit");
    QLabel* usdEquivalentLabel = sendWidget.findChild<QLabel*>("usdEquivalentLabel");
    QLabel* feeLabel = sendWidget.findChild<QLabel*>("feeLabel");
    QPushButton* sendButton = sendWidget.findChild<QPushButton*>("sendButton");

    BOOST_CHECK(addressEdit != nullptr);
    BOOST_CHECK(amountEdit != nullptr);
    BOOST_CHECK(usdEquivalentLabel != nullptr);
    BOOST_CHECK(feeLabel != nullptr);
    BOOST_CHECK(sendButton != nullptr);

    // Test DD address validation
    addressEdit->setText("DD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
    BOOST_CHECK(addressEdit->hasAcceptableInput());

    addressEdit->setText("TD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
    BOOST_CHECK(addressEdit->hasAcceptableInput());

    addressEdit->setText("RD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
    BOOST_CHECK(addressEdit->hasAcceptableInput());

    // Test invalid address
    addressEdit->setText("DG1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
    BOOST_CHECK(!addressEdit->hasAcceptableInput());

    // Test amount validation
    amountEdit->setText("100.12345678");
    BOOST_CHECK(amountEdit->hasAcceptableInput());

    amountEdit->setText("-5.0");
    BOOST_CHECK(!amountEdit->hasAcceptableInput());

    // Test send button state
    BOOST_CHECK(!sendButton->isEnabled()); // Should be disabled initially
}

BOOST_FIXTURE_TEST_CASE(digidollar_mint_widget, DigiDollarGUITestFixture)
{
    DigiDollarMintWidget mintWidget;

    // Test UI components
    QLineEdit* amountEdit = mintWidget.findChild<QLineEdit*>("amountEdit");
    QComboBox* lockTierCombo = mintWidget.findChild<QComboBox*>("lockTierCombo");
    QLabel* collateralLabel = mintWidget.findChild<QLabel*>("collateralLabel");
    QLabel* ratioLabel = mintWidget.findChild<QLabel*>("ratioLabel");
    QLabel* oraclePriceLabel = mintWidget.findChild<QLabel*>("oraclePriceLabel");
    QPushButton* mintButton = mintWidget.findChild<QPushButton*>("mintButton");

    BOOST_CHECK(amountEdit != nullptr);
    BOOST_CHECK(lockTierCombo != nullptr);
    BOOST_CHECK(collateralLabel != nullptr);
    BOOST_CHECK(ratioLabel != nullptr);
    BOOST_CHECK(oraclePriceLabel != nullptr);
    BOOST_CHECK(mintButton != nullptr);

    // Test lock tier dropdown has 8 options
    BOOST_CHECK_EQUAL(lockTierCombo->count(), 8);

    // Test amount validation
    amountEdit->setText("100.0");
    BOOST_CHECK(amountEdit->hasAcceptableInput());

    amountEdit->setText("0.00000001");
    BOOST_CHECK(amountEdit->hasAcceptableInput());

    amountEdit->setText("0");
    BOOST_CHECK(!amountEdit->hasAcceptableInput());

    // Test mint button state
    BOOST_CHECK(!mintButton->isEnabled()); // Should be disabled initially
}

BOOST_FIXTURE_TEST_CASE(digidollar_redeem_widget, DigiDollarGUITestFixture)
{
    DigiDollarRedeemWidget redeemWidget;

    // Test UI components
    QLineEdit* positionIdEdit = redeemWidget.findChild<QLineEdit*>("positionIdEdit");
    QLineEdit* amountEdit = redeemWidget.findChild<QLineEdit*>("amountEdit");
    QLabel* positionInfoLabel = redeemWidget.findChild<QLabel*>("positionInfoLabel");
    QLabel* redeemableLabel = redeemWidget.findChild<QLabel*>("redeemableLabel");
    QPushButton* redeemButton = redeemWidget.findChild<QPushButton*>("redeemButton");
    QPushButton* redeemAllButton = redeemWidget.findChild<QPushButton*>("redeemAllButton");

    BOOST_CHECK(positionIdEdit != nullptr);
    BOOST_CHECK(amountEdit != nullptr);
    BOOST_CHECK(positionInfoLabel != nullptr);
    BOOST_CHECK(redeemableLabel != nullptr);
    BOOST_CHECK(redeemButton != nullptr);
    BOOST_CHECK(redeemAllButton != nullptr);

    // Test position ID validation
    positionIdEdit->setText("abc123def456");
    BOOST_CHECK(positionIdEdit->hasAcceptableInput());

    positionIdEdit->setText("invalid@id");
    BOOST_CHECK(!positionIdEdit->hasAcceptableInput());

    // Test redeem buttons state
    BOOST_CHECK(!redeemButton->isEnabled()); // Should be disabled initially
    BOOST_CHECK(!redeemAllButton->isEnabled()); // Should be disabled initially
}

BOOST_FIXTURE_TEST_CASE(digidollar_positions_widget, DigiDollarGUITestFixture)
{
    DigiDollarPositionsWidget positionsWidget;

    // Test UI components
    QTableWidget* positionsTable = positionsWidget.findChild<QTableWidget*>("positionsTable");
    QPushButton* refreshButton = positionsWidget.findChild<QPushButton*>("refreshButton");

    BOOST_CHECK(positionsTable != nullptr);
    BOOST_CHECK(refreshButton != nullptr);

    // Test table structure
    BOOST_CHECK_EQUAL(positionsTable->columnCount(), 7);

    // Test column headers
    QStringList expectedHeaders = {"Position ID", "DD Minted", "DGB Collateral",
                                   "Lock Tier", "Time Remaining", "Health", "Actions"};
    for (int i = 0; i < expectedHeaders.size(); ++i) {
        BOOST_CHECK_EQUAL(positionsTable->horizontalHeaderItem(i)->text().toStdString(),
                         expectedHeaders[i].toStdString());
    }

    // Test initial state
    BOOST_CHECK_EQUAL(positionsTable->rowCount(), 0);
    BOOST_CHECK(refreshButton->isEnabled());
}

BOOST_FIXTURE_TEST_CASE(digidollar_tab_wallet_model_integration, DigiDollarGUITestFixture)
{
    DigiDollarTab tab;

    // Create mock wallet model (would normally be created by the wallet)
    // This test verifies the tab can accept a wallet model
    // The actual wallet model integration would be tested in functional tests

    // Test setWalletModel method exists and doesn't crash
    try {
        tab.setWalletModel(nullptr); // Test with nullptr first
        BOOST_CHECK(true); // If we get here, method exists and handles nullptr
    } catch (...) {
        BOOST_FAIL("setWalletModel should handle nullptr gracefully");
    }
}

BOOST_FIXTURE_TEST_CASE(digidollar_address_validator, DigiDollarGUITestFixture)
{
    DigiDollarSendWidget sendWidget;
    QLineEdit* addressEdit = sendWidget.findChild<QLineEdit*>("addressEdit");

    // Test various DD address formats
    struct TestCase {
        std::string address;
        bool valid;
    };

    std::vector<TestCase> testCases = {
        // Valid DD addresses
        {"DD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", true},
        {"TD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", true},
        {"RD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", true},

        // Invalid addresses
        {"DG1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", false}, // Wrong prefix
        {"DD", false}, // Too short
        {"", false}, // Empty
        {"not_an_address", false}, // Invalid format
        {"BC1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", false}, // Bitcoin address
    };

    for (const auto& testCase : testCases) {
        addressEdit->setText(QString::fromStdString(testCase.address));
        if (testCase.valid) {
            BOOST_CHECK_MESSAGE(addressEdit->hasAcceptableInput(),
                              "Address should be valid: " + testCase.address);
        } else {
            BOOST_CHECK_MESSAGE(!addressEdit->hasAcceptableInput(),
                              "Address should be invalid: " + testCase.address);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(digidollar_amount_validator, DigiDollarGUITestFixture)
{
    DigiDollarSendWidget sendWidget;
    QLineEdit* amountEdit = sendWidget.findChild<QLineEdit*>("amountEdit");

    struct TestCase {
        std::string amount;
        bool valid;
    };

    std::vector<TestCase> testCases = {
        // Valid amounts
        {"0.00000001", true},
        {"1.0", true},
        {"100.12345678", true},
        {"999999.99999999", true},

        // Invalid amounts
        {"0", false}, // Zero
        {"-1.0", false}, // Negative
        {"abc", false}, // Non-numeric
        {"1.000000001", false}, // Too many decimals
        {"", false}, // Empty
    };

    for (const auto& testCase : testCases) {
        amountEdit->setText(QString::fromStdString(testCase.amount));
        if (testCase.valid) {
            BOOST_CHECK_MESSAGE(amountEdit->hasAcceptableInput(),
                              "Amount should be valid: " + testCase.amount);
        } else {
            BOOST_CHECK_MESSAGE(!amountEdit->hasAcceptableInput(),
                              "Amount should be invalid: " + testCase.amount);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(digidollar_lock_tier_options, DigiDollarGUITestFixture)
{
    DigiDollarMintWidget mintWidget;
    QComboBox* lockTierCombo = mintWidget.findChild<QComboBox*>("lockTierCombo");

    // Verify all 8 lock tiers are present
    BOOST_CHECK_EQUAL(lockTierCombo->count(), 8);

    // Verify tier names/values (these should match the DigiDollar specification)
    QStringList expectedTiers = {
        "Tier 1 (8 blocks)", "Tier 2 (64 blocks)", "Tier 3 (512 blocks)",
        "Tier 4 (4096 blocks)", "Tier 5 (32768 blocks)", "Tier 6 (262144 blocks)",
        "Tier 7 (2097152 blocks)", "Tier 8 (16777216 blocks)"
    };

    for (int i = 0; i < expectedTiers.size(); ++i) {
        BOOST_CHECK_EQUAL(lockTierCombo->itemText(i).toStdString(),
                         expectedTiers[i].toStdString());
    }
}

BOOST_FIXTURE_TEST_CASE(digidollar_ui_responsiveness, DigiDollarGUITestFixture)
{
    DigiDollarTab tab;

    // Test updateView method exists and doesn't crash
    try {
        tab.updateView();
        BOOST_CHECK(true);
    } catch (...) {
        BOOST_FAIL("updateView should not throw exceptions");
    }

    // Test widget responsiveness
    tab.resize(800, 600);
    BOOST_CHECK(tab.size().width() == 800);
    BOOST_CHECK(tab.size().height() == 600);

    // All child widgets should exist and be properly laid out
    QTabWidget* tabWidget = tab.findChild<QTabWidget*>();
    BOOST_CHECK(tabWidget->size().width() > 0);
    BOOST_CHECK(tabWidget->size().height() > 0);
}

BOOST_AUTO_TEST_SUITE_END()

#endif // ENABLE_WALLET