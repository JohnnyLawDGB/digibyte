// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollaroverviewwidget.h>

#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/digibyteunits.h>

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QProgressBar>
#include <QFont>
#include <QTimer>

DigiDollarOverviewWidget::DigiDollarOverviewWidget(QWidget *parent) :
    QWidget(parent),
    m_mainLayout(nullptr),
    m_balanceFrame(nullptr),
    m_balanceLayout(nullptr),
    m_ddBalanceLabel(nullptr),
    m_ddBalanceValue(nullptr),
    m_dgbCollateralLabel(nullptr),
    m_dgbCollateralValue(nullptr),
    m_usdValueLabel(nullptr),
    m_usdValueValue(nullptr),
    m_systemHealthFrame(nullptr),
    m_systemHealthLayout(nullptr),
    m_oraclePriceLabel(nullptr),
    m_oraclePriceValue(nullptr),
    m_systemHealthLabel(nullptr),
    m_systemHealthValue(nullptr),
    m_dcaLevelLabel(nullptr),
    m_dcaLevelValue(nullptr),
    m_errLevelLabel(nullptr),
    m_errLevelValue(nullptr),
    m_systemHealthBar(nullptr),
    m_transactionsFrame(nullptr),
    m_transactionsLayout(nullptr),
    m_transactionsTitle(nullptr),
    m_recentTransactionsInfo(nullptr),
    m_walletModel(nullptr),
    m_clientModel(nullptr),
    m_ddBalance(0.0),
    m_dgbCollateral(0.0),
    m_oraclePrice(0.0),
    m_systemHealthStatus("Loading..."),
    m_dcaLevel(0),
    m_errLevel(0)
{
    setupUI();
    connectSignals();
}

DigiDollarOverviewWidget::~DigiDollarOverviewWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarOverviewWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Setup sections
    setupBalanceSection();
    setupSystemHealthSection();
    setupRecentTransactionsSection();

    setLayout(m_mainLayout);
}

void DigiDollarOverviewWidget::setupBalanceSection()
{
    // Create balance frame
    m_balanceFrame = new QFrame(this);
    m_balanceFrame->setFrameStyle(QFrame::StyledPanel);
    m_balanceFrame->setObjectName("balanceFrame");

    m_balanceLayout = new QGridLayout(m_balanceFrame);
    m_balanceLayout->setSpacing(10);
    m_balanceLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* balanceTitle = new QLabel(tr("DigiDollar Balance"), this);
    QFont titleFont = balanceTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    balanceTitle->setFont(titleFont);
    m_balanceLayout->addWidget(balanceTitle, 0, 0, 1, 2);

    // DD Balance
    m_ddBalanceLabel = new QLabel(tr("DD Balance:"), this);
    m_ddBalanceLabel->setObjectName("ddBalanceLabel");
    m_ddBalanceValue = new QLabel("0.00000000 DD", this);
    m_ddBalanceValue->setObjectName("ddBalanceValue");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_ddBalanceValue->setFont(monospaceFont);
    m_balanceLayout->addWidget(m_ddBalanceLabel, 1, 0);
    m_balanceLayout->addWidget(m_ddBalanceValue, 1, 1);

    // DGB Collateral
    m_dgbCollateralLabel = new QLabel(tr("DGB Collateral:"), this);
    m_dgbCollateralLabel->setObjectName("dgbCollateralLabel");
    m_dgbCollateralValue = new QLabel("0.00000000 DGB", this);
    m_dgbCollateralValue->setObjectName("dgbCollateralValue");
    m_dgbCollateralValue->setFont(monospaceFont);
    m_balanceLayout->addWidget(m_dgbCollateralLabel, 2, 0);
    m_balanceLayout->addWidget(m_dgbCollateralValue, 2, 1);

    // USD Value
    m_usdValueLabel = new QLabel(tr("USD Value:"), this);
    m_usdValueLabel->setObjectName("usdValueLabel");
    m_usdValueValue = new QLabel("$0.00", this);
    m_usdValueValue->setObjectName("usdValueValue");
    m_usdValueValue->setFont(monospaceFont);
    m_balanceLayout->addWidget(m_usdValueLabel, 3, 0);
    m_balanceLayout->addWidget(m_usdValueValue, 3, 1);

    m_mainLayout->addWidget(m_balanceFrame);
}

void DigiDollarOverviewWidget::setupSystemHealthSection()
{
    // Create system health frame
    m_systemHealthFrame = new QFrame(this);
    m_systemHealthFrame->setFrameStyle(QFrame::StyledPanel);
    m_systemHealthFrame->setObjectName("systemHealthFrame");

    m_systemHealthLayout = new QGridLayout(m_systemHealthFrame);
    m_systemHealthLayout->setSpacing(10);
    m_systemHealthLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* healthTitle = new QLabel(tr("System Health"), this);
    QFont titleFont = healthTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    healthTitle->setFont(titleFont);
    m_systemHealthLayout->addWidget(healthTitle, 0, 0, 1, 2);

    // Oracle Price
    m_oraclePriceLabel = new QLabel(tr("Oracle Price:"), this);
    m_oraclePriceLabel->setObjectName("oraclePriceLabel");
    m_oraclePriceValue = new QLabel("Loading...", this);
    m_oraclePriceValue->setObjectName("oraclePriceValue");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_oraclePriceValue->setFont(monospaceFont);
    m_systemHealthLayout->addWidget(m_oraclePriceLabel, 1, 0);
    m_systemHealthLayout->addWidget(m_oraclePriceValue, 1, 1);

    // System Health Status
    m_systemHealthLabel = new QLabel(tr("System Health:"), this);
    m_systemHealthLabel->setObjectName("systemHealthLabel");
    m_systemHealthValue = new QLabel("Healthy", this);
    m_systemHealthValue->setObjectName("systemHealthValue");
    m_systemHealthLayout->addWidget(m_systemHealthLabel, 2, 0);
    m_systemHealthLayout->addWidget(m_systemHealthValue, 2, 1);

    // DCA Level
    m_dcaLevelLabel = new QLabel(tr("DCA Level:"), this);
    m_dcaLevelLabel->setObjectName("dcaLevelLabel");
    m_dcaLevelValue = new QLabel("0", this);
    m_dcaLevelValue->setObjectName("dcaLevelValue");
    m_systemHealthLayout->addWidget(m_dcaLevelLabel, 3, 0);
    m_systemHealthLayout->addWidget(m_dcaLevelValue, 3, 1);

    // ERR Level
    m_errLevelLabel = new QLabel(tr("ERR Level:"), this);
    m_errLevelLabel->setObjectName("errLevelLabel");
    m_errLevelValue = new QLabel("0", this);
    m_errLevelValue->setObjectName("errLevelValue");
    m_systemHealthLayout->addWidget(m_errLevelLabel, 4, 0);
    m_systemHealthLayout->addWidget(m_errLevelValue, 4, 1);

    // System Health Progress Bar
    m_systemHealthBar = new QProgressBar(this);
    m_systemHealthBar->setObjectName("systemHealthBar");
    m_systemHealthBar->setRange(0, 100);
    m_systemHealthBar->setValue(100); // Start at 100% healthy
    m_systemHealthBar->setTextVisible(false);
    m_systemHealthLayout->addWidget(m_systemHealthBar, 5, 0, 1, 2);

    m_mainLayout->addWidget(m_systemHealthFrame);
}

void DigiDollarOverviewWidget::setupRecentTransactionsSection()
{
    // Create recent transactions frame
    m_transactionsFrame = new QFrame(this);
    m_transactionsFrame->setFrameStyle(QFrame::StyledPanel);
    m_transactionsFrame->setObjectName("transactionsFrame");

    m_transactionsLayout = new QVBoxLayout(m_transactionsFrame);
    m_transactionsLayout->setSpacing(10);
    m_transactionsLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    m_transactionsTitle = new QLabel(tr("Recent DigiDollar Transactions"), this);
    QFont titleFont = m_transactionsTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_transactionsTitle->setFont(titleFont);
    m_transactionsLayout->addWidget(m_transactionsTitle);

    // Info
    m_recentTransactionsInfo = new QLabel(tr("No recent DigiDollar transactions"), this);
    m_recentTransactionsInfo->setObjectName("recentTransactionsInfo");
    m_recentTransactionsInfo->setStyleSheet("QLabel { color: #666666; }");
    m_transactionsLayout->addWidget(m_recentTransactionsInfo);

    m_mainLayout->addWidget(m_transactionsFrame);

    // Add stretch to push content to top
    m_mainLayout->addStretch();
}

void DigiDollarOverviewWidget::connectSignals()
{
    // Connect update timer (every 30 seconds)
    QTimer* updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &DigiDollarOverviewWidget::updateView);
    updateTimer->start(30000); // 30 seconds
}

void DigiDollarOverviewWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals
        // These would connect to actual wallet balance change signals
        // For now, we'll update periodically
        updateBalance();
    }
}

void DigiDollarOverviewWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals for oracle price updates
        updateOraclePrice();
        updateSystemHealth();
    }
}

void DigiDollarOverviewWidget::updateView()
{
    updateBalance();
    updateOraclePrice();
    updateSystemHealth();
    updateRecentTransactions();
}

void DigiDollarOverviewWidget::incomingDDTransaction(const QString& date, const QString& amount,
                                                    const QString& type, const QString& address)
{
    // Update recent transactions info
    QString transactionInfo = QString("Latest: %1 %2 DD (%3)")
                                .arg(type)
                                .arg(amount)
                                .arg(date);
    m_recentTransactionsInfo->setText(transactionInfo);
    m_recentTransactionsInfo->setStyleSheet("QLabel { color: #006600; }");

    // Update balance after a short delay to allow for processing
    QTimer::singleShot(1000, this, &DigiDollarOverviewWidget::updateBalance);
}

void DigiDollarOverviewWidget::updateBalance()
{
    // In a real implementation, this would query the wallet for DigiDollar balances
    // For now, we'll show placeholder values

    if (m_walletModel) {
        // TODO: Get actual DigiDollar balance from wallet
        // m_ddBalance = m_walletModel->getDDBalance();
        // m_dgbCollateral = m_walletModel->getDGBCollateral();
    }

    // Update display
    m_ddBalanceValue->setText(formatDDAmount(m_ddBalance));
    m_dgbCollateralValue->setText(formatDGBAmount(m_dgbCollateral));

    // Calculate USD value
    double usdValue = m_ddBalance * 1.0; // DD should be pegged to $1
    m_usdValueValue->setText(formatUSDAmount(usdValue));
}

void DigiDollarOverviewWidget::updateOraclePrice()
{
    // In a real implementation, this would query the oracle price
    // For now, we'll show a placeholder

    if (m_clientModel) {
        // TODO: Get actual oracle price
        // m_oraclePrice = m_clientModel->getOraclePrice();
    }

    if (m_oraclePrice > 0) {
        m_oraclePriceValue->setText(formatUSDAmount(m_oraclePrice) + " USD/DGB");
    } else {
        m_oraclePriceValue->setText("Loading...");
    }
}

void DigiDollarOverviewWidget::updateSystemHealth()
{
    // In a real implementation, this would query system health metrics
    // For now, we'll show healthy status

    QString healthStatus = "Healthy";
    int healthPercentage = 100;

    if (m_clientModel) {
        // TODO: Get actual system health metrics
        // healthStatus = m_clientModel->getSystemHealthStatus();
        // m_dcaLevel = m_clientModel->getDCALevel();
        // m_errLevel = m_clientModel->getERRLevel();

        // Determine health percentage based on DCA/ERR levels
        if (m_dcaLevel > 0 || m_errLevel > 0) {
            healthStatus = "Monitoring";
            healthPercentage = 75;
        }
    }

    m_systemHealthValue->setText(healthStatus);
    m_dcaLevelValue->setText(QString::number(m_dcaLevel));
    m_errLevelValue->setText(QString::number(m_errLevel));
    m_systemHealthBar->setValue(healthPercentage);

    // Color code the health status
    if (healthStatus == "Healthy") {
        m_systemHealthValue->setStyleSheet("QLabel { color: #006600; }");
        m_systemHealthBar->setStyleSheet("QProgressBar::chunk { background-color: #006600; }");
    } else if (healthStatus == "Monitoring") {
        m_systemHealthValue->setStyleSheet("QLabel { color: #ff6600; }");
        m_systemHealthBar->setStyleSheet("QProgressBar::chunk { background-color: #ff6600; }");
    } else {
        m_systemHealthValue->setStyleSheet("QLabel { color: #cc0000; }");
        m_systemHealthBar->setStyleSheet("QProgressBar::chunk { background-color: #cc0000; }");
    }
}

void DigiDollarOverviewWidget::updateRecentTransactions()
{
    // In a real implementation, this would query recent DigiDollar transactions
    // For now, we'll keep the current display
}

QString DigiDollarOverviewWidget::formatDDAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DD";
}

QString DigiDollarOverviewWidget::formatDGBAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DGB";
}

QString DigiDollarOverviewWidget::formatUSDAmount(double amount) const
{
    return "$" + QString::number(amount, 'f', 2);
}