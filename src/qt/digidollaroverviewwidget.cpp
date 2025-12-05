// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollaroverviewwidget.h>

#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/digibyteunits.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <oracle/mock_oracle.h>
#include <consensus/dca.h>
#include <digidollar/health.h>
#include <chainparams.h>
#include <wallet/digidollarwallet.h>
#include <uint256.h>
#include <interfaces/wallet.h>
#include <interfaces/node.h>
#include <univalue.h>
#include <rpc/util.h>

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QProgressBar>
#include <QFont>
#include <QTimer>
#include <QSpacerItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QCursor>
#include <QBrush>
#include <QColor>
#include <QAbstractScrollArea>
#include <QAbstractItemView>
#include <QApplication>
#include <QPalette>
#include <QLocale>

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
    m_networkTotalDDLabel(nullptr),
    m_networkTotalDDValue(nullptr),
    m_networkTotalCollateralLabel(nullptr),
    m_networkTotalCollateralValue(nullptr),
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
    m_transactionsList(nullptr),
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
    // REMOVED: applyTheme() - Let CSS handle all theming

    // Add some demo transactions for display purposes
    addDemoTransactions();
}

DigiDollarOverviewWidget::~DigiDollarOverviewWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarOverviewWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);

    // Create horizontal layout for balance and system health side-by-side
    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->setSpacing(20);

    // Setup sections
    setupBalanceSection();
    setupSystemHealthSection();

    // Add balance and system health frames to horizontal layout
    topLayout->addWidget(m_balanceFrame);
    topLayout->addWidget(m_systemHealthFrame);

    // Add the horizontal layout to main layout
    m_mainLayout->addLayout(topLayout);

    // Add transactions section below
    setupRecentTransactionsSection();

    setLayout(m_mainLayout);
}

void DigiDollarOverviewWidget::setupBalanceSection()
{
    // Create balance frame with styling matching main wallet
    m_balanceFrame = new QFrame(this);
    m_balanceFrame->setFrameShape(QFrame::StyledPanel);
    m_balanceFrame->setFrameShadow(QFrame::Raised);
    m_balanceFrame->setObjectName("balanceFrame");
    m_balanceFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout* frameVLayout = new QVBoxLayout(m_balanceFrame);
    frameVLayout->setObjectName("frameVLayout");

    // Title with status indicator layout (similar to main wallet)
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setObjectName("titleLayout");

    QLabel* balanceTitle = new QLabel(tr("Your DigiDollar Balances"), this);
    QFont titleFont = balanceTitle->font();
    titleFont.setBold(true);
    titleFont.setWeight(75); // Match main wallet weight
    balanceTitle->setFont(titleFont);
    titleLayout->addWidget(balanceTitle);

    // Add spacer to push content left (matching main wallet layout)
    QSpacerItem* titleSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    titleLayout->addItem(titleSpacer);

    frameVLayout->addLayout(titleLayout);

    // Grid layout for balance items
    m_balanceLayout = new QGridLayout();
    m_balanceLayout->setSpacing(12); // Match main wallet spacing
    m_balanceLayout->setObjectName("balanceGridLayout");

    // DD Balance (Available)
    m_ddBalanceLabel = new QLabel(tr("Your DD Balance:"), this);
    m_ddBalanceLabel->setObjectName("ddBalanceLabel");
    m_ddBalanceValue = new QLabel("0.00000000 DD", this);
    m_ddBalanceValue->setObjectName("ddBalanceValue");
    m_ddBalanceValue->setCursor(QCursor(Qt::IBeamCursor));
    m_ddBalanceValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_ddBalanceValue->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_ddBalanceValue->setToolTip(tr("Your current spendable DigiDollar balance"));
    m_balanceLayout->addWidget(m_ddBalanceLabel, 1, 0);
    m_balanceLayout->addWidget(m_ddBalanceValue, 1, 1);

    // DGB Collateral (Pending/Locked)
    m_dgbCollateralLabel = new QLabel(tr("Your Locked Collateral:"), this);
    m_dgbCollateralLabel->setObjectName("dgbCollateralLabel");
    m_dgbCollateralValue = new QLabel("0.00000000 DGB", this);
    m_dgbCollateralValue->setObjectName("dgbCollateralValue");
    m_dgbCollateralValue->setCursor(QCursor(Qt::IBeamCursor));
    m_dgbCollateralValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_dgbCollateralValue->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_dgbCollateralValue->setToolTip(tr("Your DGB locked as collateral for DigiDollars in your wallet"));
    m_balanceLayout->addWidget(m_dgbCollateralLabel, 2, 0);
    m_balanceLayout->addWidget(m_dgbCollateralValue, 2, 1);

    // Add separator line
    QFrame* line = new QFrame(m_balanceFrame);
    line->setObjectName("line");
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_balanceLayout->addWidget(line, 3, 0, 1, 2);

    // USD Value (Total)
    m_usdValueLabel = new QLabel(tr("Total:"), this);
    m_usdValueLabel->setObjectName("usdValueLabel");
    m_usdValueValue = new QLabel("$0.00", this);
    m_usdValueValue->setObjectName("usdValueValue");
    m_usdValueValue->setCursor(QCursor(Qt::IBeamCursor));
    m_usdValueValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_usdValueValue->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_usdValueValue->setToolTip(tr("Your total DigiDollar value in USD"));
    m_balanceLayout->addWidget(m_usdValueLabel, 4, 0);
    m_balanceLayout->addWidget(m_usdValueValue, 4, 1);

    // Add horizontal spacer
    QSpacerItem* horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_balanceLayout->addItem(horizontalSpacer, 2, 2, 1, 1);

    frameVLayout->addLayout(m_balanceLayout);
    // REMOVED: m_mainLayout->addWidget(m_balanceFrame);
    // Frame is now added to horizontal layout in setupUI()
}

void DigiDollarOverviewWidget::setupSystemHealthSection()
{
    // Create system health frame with styling matching main wallet
    m_systemHealthFrame = new QFrame(this);
    m_systemHealthFrame->setFrameShape(QFrame::StyledPanel);
    m_systemHealthFrame->setFrameShadow(QFrame::Raised);
    m_systemHealthFrame->setObjectName("systemHealthFrame");
    m_systemHealthFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout* frameVLayout = new QVBoxLayout(m_systemHealthFrame);
    frameVLayout->setObjectName("healthFrameVLayout");

    // Title with status indicator layout
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setObjectName("healthTitleLayout");

    QLabel* healthTitle = new QLabel(tr("Network DigiDollar Status"), this);
    QFont titleFont = healthTitle->font();
    titleFont.setBold(true);
    titleFont.setWeight(75); // Match main wallet weight
    healthTitle->setFont(titleFont);
    titleLayout->addWidget(healthTitle);

    // Add spacer to push content left
    QSpacerItem* titleSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    titleLayout->addItem(titleSpacer);

    frameVLayout->addLayout(titleLayout);

    // ========================================
    // Split layout: Left stats | Right totals
    // ========================================
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(20);
    contentLayout->setObjectName("healthContentLayout");

    // --- LEFT SIDE: Other stats (DGB/USD, System Health, DCA, ERR) ---
    QFrame* leftStatsFrame = new QFrame(this);
    leftStatsFrame->setObjectName("leftStatsFrame");
    leftStatsFrame->setFrameShape(QFrame::NoFrame);

    m_systemHealthLayout = new QGridLayout(leftStatsFrame);
    m_systemHealthLayout->setSpacing(8);
    m_systemHealthLayout->setContentsMargins(0, 0, 0, 0);
    m_systemHealthLayout->setObjectName("healthGridLayout");

    // Oracle Price
    m_oraclePriceLabel = new QLabel(tr("DGB/USD Price:"), this);
    m_oraclePriceLabel->setObjectName("oraclePriceLabel");
    m_oraclePriceValue = new QLabel("Loading...", this);
    m_oraclePriceValue->setObjectName("oraclePriceValue");
    m_oraclePriceValue->setCursor(QCursor(Qt::IBeamCursor));
    m_oraclePriceValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_oraclePriceValue->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_oraclePriceValue->setToolTip(tr("Current DigiByte to USD exchange rate from oracle"));
    m_systemHealthLayout->addWidget(m_oraclePriceLabel, 0, 0);
    m_systemHealthLayout->addWidget(m_oraclePriceValue, 0, 1);

    // System Health Status
    m_systemHealthLabel = new QLabel(tr("System Health:"), this);
    m_systemHealthLabel->setObjectName("systemHealthLabel");
    m_systemHealthValue = new QLabel("Healthy", this);
    m_systemHealthValue->setObjectName("systemHealthValue");
    m_systemHealthValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_systemHealthValue->setToolTip(tr("Overall DigiDollar network-wide health status"));
    m_systemHealthLayout->addWidget(m_systemHealthLabel, 1, 0);
    m_systemHealthLayout->addWidget(m_systemHealthValue, 1, 1);

    // DCA Level
    m_dcaLevelLabel = new QLabel(tr("DCA Level:"), this);
    m_dcaLevelLabel->setObjectName("dcaLevelLabel");
    m_dcaLevelValue = new QLabel("0", this);
    m_dcaLevelValue->setObjectName("dcaLevelValue");
    m_dcaLevelValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_dcaLevelValue->setToolTip(tr("Current Dollar-Cost Averaging intervention level"));
    m_systemHealthLayout->addWidget(m_dcaLevelLabel, 2, 0);
    m_systemHealthLayout->addWidget(m_dcaLevelValue, 2, 1);

    // ERR Level
    m_errLevelLabel = new QLabel(tr("ERR Level:"), this);
    m_errLevelLabel->setObjectName("errLevelLabel");
    m_errLevelValue = new QLabel("0", this);
    m_errLevelValue->setObjectName("errLevelValue");
    m_errLevelValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_errLevelValue->setToolTip(tr("Current Emergency Response Reserve level"));
    m_systemHealthLayout->addWidget(m_errLevelLabel, 3, 0);
    m_systemHealthLayout->addWidget(m_errLevelValue, 3, 1);

    // --- RIGHT SIDE: Prominent Network Totals (DD Supply + Collateral) ---
    QFrame* rightTotalsFrame = new QFrame(this);
    rightTotalsFrame->setObjectName("networkTotalsFrame");
    rightTotalsFrame->setFrameShape(QFrame::StyledPanel);
    rightTotalsFrame->setFrameShadow(QFrame::Raised);

    QVBoxLayout* totalsLayout = new QVBoxLayout(rightTotalsFrame);
    totalsLayout->setSpacing(12);
    totalsLayout->setContentsMargins(15, 15, 15, 15);

    // Network Total DD Supply (prominent)
    m_networkTotalDDLabel = new QLabel(tr("Network DD Supply"), this);
    m_networkTotalDDLabel->setObjectName("networkTotalDDLabel");
    m_networkTotalDDLabel->setAlignment(Qt::AlignCenter);
    m_networkTotalDDLabel->setWordWrap(true);
    totalsLayout->addWidget(m_networkTotalDDLabel);

    m_networkTotalDDValue = new QLabel("Loading...", this);
    m_networkTotalDDValue->setObjectName("networkTotalDDValue");
    m_networkTotalDDValue->setCursor(QCursor(Qt::IBeamCursor));
    m_networkTotalDDValue->setAlignment(Qt::AlignCenter);
    m_networkTotalDDValue->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_networkTotalDDValue->setToolTip(tr("Total DigiDollar supply across the entire network"));
    totalsLayout->addWidget(m_networkTotalDDValue);

    // Separator between totals
    QFrame* separator = new QFrame(this);
    separator->setObjectName("totalsSeparator");
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    totalsLayout->addWidget(separator);

    // Network Total Collateral (prominent)
    m_networkTotalCollateralLabel = new QLabel(tr("Network DGB Locked"), this);
    m_networkTotalCollateralLabel->setObjectName("networkTotalCollateralLabel");
    m_networkTotalCollateralLabel->setAlignment(Qt::AlignCenter);
    m_networkTotalCollateralLabel->setWordWrap(true);
    totalsLayout->addWidget(m_networkTotalCollateralLabel);

    m_networkTotalCollateralValue = new QLabel("Loading...", this);
    m_networkTotalCollateralValue->setObjectName("networkTotalCollateralValue");
    m_networkTotalCollateralValue->setCursor(QCursor(Qt::IBeamCursor));
    m_networkTotalCollateralValue->setAlignment(Qt::AlignCenter);
    m_networkTotalCollateralValue->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_networkTotalCollateralValue->setToolTip(tr("Total DGB locked as collateral across the entire network"));
    totalsLayout->addWidget(m_networkTotalCollateralValue);

    // Add left and right to horizontal content layout
    contentLayout->addWidget(leftStatsFrame, 1); // stretch factor 1
    contentLayout->addWidget(rightTotalsFrame, 1); // stretch factor 1 (equal width)

    frameVLayout->addLayout(contentLayout);

    // System Health Progress Bar at bottom - spans full width
    m_systemHealthBar = new QProgressBar(this);
    m_systemHealthBar->setObjectName("systemHealthBar");
    m_systemHealthBar->setRange(0, 100);
    m_systemHealthBar->setValue(100); // Start at 100% healthy
    m_systemHealthBar->setTextVisible(true);
    m_systemHealthBar->setFormat("%p% Healthy");
    m_systemHealthBar->setMinimumHeight(20);
    m_systemHealthBar->setToolTip(tr("Visual indicator of overall network health"));
    frameVLayout->addWidget(m_systemHealthBar);

    // REMOVED: m_mainLayout->addWidget(m_systemHealthFrame);
    // Frame is now added to horizontal layout in setupUI()
}

void DigiDollarOverviewWidget::setupRecentTransactionsSection()
{
    // Create recent transactions frame with styling matching main wallet
    m_transactionsFrame = new QFrame(this);
    m_transactionsFrame->setFrameShape(QFrame::StyledPanel);
    m_transactionsFrame->setFrameShadow(QFrame::Raised);
    m_transactionsFrame->setObjectName("transactionsFrame");

    QVBoxLayout* frameVLayout = new QVBoxLayout(m_transactionsFrame);
    frameVLayout->setObjectName("transactionsFrameVLayout");

    // Title with status indicator layout
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setObjectName("transactionsTitleLayout");

    m_transactionsTitle = new QLabel(tr("Recent DigiDollar transactions"), this);
    QFont titleFont = m_transactionsTitle->font();
    titleFont.setBold(true);
    titleFont.setWeight(75); // Match main wallet weight
    m_transactionsTitle->setFont(titleFont);
    titleLayout->addWidget(m_transactionsTitle);

    // Add spacer to push content left
    QSpacerItem* titleSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    titleLayout->addItem(titleSpacer);

    frameVLayout->addLayout(titleLayout);

    // Create list widget for transactions (similar to main wallet)
    m_transactionsList = new QListWidget(this);
    m_transactionsList->setObjectName("transactionsList");
    m_transactionsList->setFrameShape(QFrame::NoFrame);
    m_transactionsList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_transactionsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transactionsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_transactionsList->setUniformItemSizes(true);
    m_transactionsList->setMinimumHeight(100); // Minimum height
    m_transactionsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Info label for when no transactions exist
    m_recentTransactionsInfo = new QLabel(tr("No recent DigiDollar transactions"), this);
    m_recentTransactionsInfo->setObjectName("recentTransactionsInfo");
    // Theme styling will be applied in applyTheme()
    m_recentTransactionsInfo->setAlignment(Qt::AlignCenter);
    m_recentTransactionsInfo->setVisible(true);

    frameVLayout->addWidget(m_transactionsList);
    frameVLayout->addWidget(m_recentTransactionsInfo);

    // Set transactions frame to expand vertically
    m_transactionsFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Add transactions frame with stretch factor to fill remaining space
    m_mainLayout->addWidget(m_transactionsFrame, 1); // stretch factor 1 = expand to fill
}

void DigiDollarOverviewWidget::connectSignals()
{
    // Connect update timer (every 30 seconds)
    QTimer* updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &DigiDollarOverviewWidget::updateView);
    updateTimer->start(30000); // 30 seconds

    // Note: Additional wallet and client model signals will be connected
    // in setWalletModel() and setClientModel() once models are available
}

void DigiDollarOverviewWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals for automatic updates
        connect(m_walletModel, &WalletModel::balanceChanged,
                this, &DigiDollarOverviewWidget::updateBalance);

        // Update transaction history when balance changes (indicates new transaction)
        connect(m_walletModel, &WalletModel::balanceChanged,
                this, &DigiDollarOverviewWidget::updateRecentTransactions);

        // Initial updates
        updateBalance();
        updateRecentTransactions();

        // Connect to options model for font updates
        if (m_walletModel->getOptionsModel()) {
            connect(m_walletModel->getOptionsModel(), &OptionsModel::useEmbeddedMonospacedFontChanged,
                    this, &DigiDollarOverviewWidget::setMonospacedFont);
            setMonospacedFont(m_walletModel->getOptionsModel()->getUseEmbeddedMonospacedFont());
            // REMOVED: applyTheme() - Let CSS handle all theming
        }
    }
}

void DigiDollarOverviewWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals for updates on new blocks
        connect(m_clientModel, &ClientModel::numBlocksChanged,
                this, &DigiDollarOverviewWidget::updateOraclePrice);
        connect(m_clientModel, &ClientModel::numBlocksChanged,
                this, &DigiDollarOverviewWidget::updateSystemHealth);

        // Update transaction confirmations on new blocks
        connect(m_clientModel, &ClientModel::numBlocksChanged,
                this, &DigiDollarOverviewWidget::updateRecentTransactions);

        // Initial updates
        updateOraclePrice();
        updateSystemHealth();

        // Connect to options model for font updates
        if (m_clientModel->getOptionsModel()) {
            connect(m_clientModel->getOptionsModel(), &OptionsModel::useEmbeddedMonospacedFontChanged,
                    this, &DigiDollarOverviewWidget::setMonospacedFont);
            setMonospacedFont(m_clientModel->getOptionsModel()->getUseEmbeddedMonospacedFont());
            // REMOVED: applyTheme() - Let CSS handle all theming
        }
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
    // Add transaction to list widget
    QListWidgetItem* item = new QListWidgetItem(m_transactionsList);
    QString transactionText = QString("%1 %2 DD - %3")
                                .arg(type)
                                .arg(amount)
                                .arg(date);
    item->setText(transactionText);
    item->setToolTip(QString("Address: %1").arg(address));

    // Color code transaction types with theme support
    QPalette palette = QApplication::palette();
    int lightness = palette.color(QPalette::WindowText).lightness();
    bool isDarkTheme = lightness > 127;

    if (type.contains("Received") || type.contains("Minted")) {
        QString successColor = isDarkTheme ? "#4caf50" : "#28a745";
        item->setForeground(QBrush(QColor(successColor)));
    } else if (type.contains("Sent") || type.contains("Redeemed")) {
        QString errorColor = isDarkTheme ? "#f44336" : "#dc3545";
        item->setForeground(QBrush(QColor(errorColor)));
    }

    // Insert at top (most recent first)
    m_transactionsList->insertItem(0, item);

    // Limit to 5 most recent transactions
    while (m_transactionsList->count() > 5) {
        delete m_transactionsList->takeItem(m_transactionsList->count() - 1);
    }

    // Hide the "no transactions" label and show the list
    m_recentTransactionsInfo->setVisible(false);
    m_transactionsList->setVisible(true);

    // Update balance after a short delay to allow for processing
    QTimer::singleShot(1000, this, &DigiDollarOverviewWidget::updateBalance);
}

void DigiDollarOverviewWidget::updateBalance()
{
    // Get actual DigiDollar balance from wallet
    if (m_walletModel) {
        // Get DD balance from wallet (in cents)
        CAmount balanceCents = m_walletModel->getDigiDollarBalance();
        m_ddBalance = balanceCents / 100.0; // Convert cents to DD

        // Get locked collateral from wallet positions (in satoshis)
        CAmount collateralSats = m_walletModel->getLockedCollateral();
        m_dgbCollateral = collateralSats / 100000000.0; // Convert satoshis to DGB
    } else {
        // No wallet connected
        m_ddBalance = 0.0;
        m_dgbCollateral = 0.0;
    }

    // Update display
    m_ddBalanceValue->setText(formatDDAmount(m_ddBalance));
    m_dgbCollateralValue->setText(formatDGBAmount(m_dgbCollateral));

    // Calculate USD value (DD should be pegged to $1)
    double usdValue = m_ddBalance * 1.0;
    m_usdValueValue->setText(formatUSDAmount(usdValue));
}

void DigiDollarOverviewWidget::updateOraclePrice()
{
    // Get price from MockOracleManager if in RegTest, otherwise use real oracle via RPC
    if (Params().GetChainType() == ChainType::REGTEST && MockOracleManager::GetInstance().IsEnabled()) {
        // Get price from mock oracle
        // Oracle price format: CENTS per DGB
        // Example: 1 = $0.01 per DGB, 50 = $0.50 per DGB
        CAmount priceCents = MockOracleManager::GetInstance().GetCurrentPrice();

        // Convert cents to dollars
        m_oraclePrice = priceCents / 100.0;
    } else if (m_clientModel) {
        // Get actual oracle price from RPC
        try {
            UniValue params(UniValue::VARR);
            UniValue result = m_clientModel->node().executeRpc("getoracleprice", params, "");

            // Price is returned in micro-USD (1,000,000 = $1.00)
            int64_t priceMicroUsd = result.find_value("price_micro_usd").getInt<int64_t>();
            m_oraclePrice = priceMicroUsd / 1000000.0; // Convert micro-USD to dollars
        } catch (const std::exception& e) {
            LogPrintf("DigiDollar: updateOraclePrice RPC error - %s\n", e.what());
            m_oraclePrice = 0.0; // Show "Loading..." on error
        }
    } else {
        m_oraclePrice = 0.0; // No client model available
    }

    if (m_oraclePrice > 0) {
        m_oraclePriceValue->setText(QString("$%1").arg(QString::number(m_oraclePrice, 'f', 6)));
    } else {
        m_oraclePriceValue->setText("Loading...");
    }
}

void DigiDollarOverviewWidget::updateSystemHealth()
{
    // NETWORK-WIDE TRACKING: Call RPC to get network-wide system health
    // This ensures Bob and Alice both see identical stats across the entire network

    if (!m_clientModel) {
        m_systemHealthValue->setText("No Connection");
        m_networkTotalDDValue->setText("N/A");
        m_networkTotalCollateralValue->setText("N/A");
        m_dcaLevelValue->setText("N/A");
        m_errLevelValue->setText("N/A");
        m_systemHealthBar->setValue(0);
        return;
    }

    try {
        // Execute RPC call to get network-wide system health
        UniValue params(UniValue::VARR); // No parameters needed
        UniValue result = m_clientModel->node().executeRpc("getdigidollarstats", params, "");

        // Extract values from RPC result
        int healthPercentage = result.find_value("health_percentage").getInt<int>();
        std::string healthStatus = result.find_value("health_status").get_str();
        CAmount totalCollateralSats = AmountFromValue(result.find_value("total_collateral_dgb"));
        CAmount totalDDCents = result.find_value("total_dd_supply").getInt<int64_t>();
        bool isEmergency = result.find_value("is_emergency").get_bool();

        // Get DCA tier info
        const UniValue& dcaTier = result.find_value("dca_tier");
        double dcaMultiplier = dcaTier.find_value("multiplier").get_real();

        // Update network-wide stats
        double totalDD = totalDDCents / 100.0; // Convert cents to DD
        double totalCollateralDGB = totalCollateralSats / 100000000.0; // Convert satoshis to DGB

        // Format with commas for readability
        QLocale locale(QLocale::English);
        QString ddFormatted = locale.toString(totalDD, 'f', 2);
        QString dgbFormatted = locale.toString(totalCollateralDGB, 'f', 2);

        // Use rich text for colored formatting: white $ and DD, green numbers
        m_networkTotalDDValue->setTextFormat(Qt::RichText);
        m_networkTotalDDValue->setText(QString("<span style='color: white;'>$</span><span style='color: #00FF88; font-weight: bold;'>%1</span><span style='color: white;'> DD</span>").arg(ddFormatted));

        m_networkTotalCollateralValue->setTextFormat(Qt::RichText);
        m_networkTotalCollateralValue->setText(QString("<span style='color: #00FF88; font-weight: bold;'>%1</span><span style='color: white;'> DGB</span>").arg(dgbFormatted));

        // Update system health display
        // RPC returns health_percentage as actual percentage (e.g., 151 = 151%)
        double healthPercent = static_cast<double>(healthPercentage);

        QString statusText;
        if (healthPercentage >= 150) {
            statusText = QString("%1% Healthy").arg(QString::number(healthPercent, 'f', 1));
        } else if (healthPercentage >= 120) {
            statusText = QString("%1% Warning").arg(QString::number(healthPercent, 'f', 1));
        } else if (healthPercentage >= 100) {
            statusText = QString("%1% Stressed").arg(QString::number(healthPercent, 'f', 1));
        } else {
            statusText = QString("%1% CRITICAL").arg(QString::number(healthPercent, 'f', 1));
        }

        m_systemHealthValue->setText(statusText);

        // Update DCA and ERR levels
        m_dcaLevelValue->setText(QString("%1x").arg(QString::number(dcaMultiplier, 'f', 1)));
        m_errLevelValue->setText(isEmergency ? "ACTIVE" : "Inactive");

        // Update progress bar (scale 0-500% to 0-100%)
        int barValue = std::min(100, static_cast<int>((healthPercent * 100) / 500));
        m_systemHealthBar->setValue(barValue);
        m_systemHealthBar->setFormat(QString("%1% Network Collateralization").arg(QString::number(healthPercent, 'f', 1)));

    } catch (const std::exception& e) {
        m_systemHealthValue->setText("Error");
        m_networkTotalDDValue->setText("Error");
        m_networkTotalCollateralValue->setText("Error");
        m_dcaLevelValue->setText("Unknown");
        m_errLevelValue->setText("Unknown");
        m_systemHealthBar->setValue(0);
        LogPrintf("DigiDollar: updateSystemHealth RPC error - %s\n", e.what());
    }
}

void DigiDollarOverviewWidget::updateRecentTransactions()
{
    if (!m_walletModel) {
        return;
    }

    // Get recent transactions from DigiDollarWallet
    DigiDollarWallet* ddWallet = m_walletModel->wallet().getDigiDollarWallet();
    if (!ddWallet) {
        return;
    }

    // Get transaction history
    std::vector<DDTransaction> transactions = ddWallet->GetDDTransactionHistory();

    // Update confirmations for all transactions based on current blockchain height
    if (m_clientModel) {
        int currentHeight = m_clientModel->getNumBlocks();
        for (auto& tx : transactions) {
            // Try to get actual confirmation count from wallet
            uint256 txHash;
            txHash.SetHex(tx.txid);

            // Get transaction details from wallet to update confirmations
            interfaces::WalletTxStatus tx_status;
            interfaces::WalletOrderForm order_form;
            bool in_mempool;
            int num_blocks;
            interfaces::WalletTx wtx = m_walletModel->wallet().getWalletTxDetails(
                txHash, tx_status, order_form, in_mempool, num_blocks);

            if (!wtx.tx) {
                // Transaction not found in wallet yet (might be in mempool)
                tx.confirmations = 0;
            } else {
                // Use depth_in_main_chain as confirmations
                tx.confirmations = tx_status.depth_in_main_chain;
            }
        }
    }

    // Clear existing items
    m_transactionsList->clear();

    // Show recent transactions (last 10)
    int count = 0;
    for (auto it = transactions.rbegin(); it != transactions.rend() && count < 10; ++it, ++count) {
        const DDTransaction& tx = *it;

        // Create transaction item widget
        QWidget* itemWidget = new QWidget();
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(10, 5, 10, 5);

        // Transaction type icon and category
        QString icon;
        QString categoryText;
        if (tx.category == "mint") {
            icon = "🏦";
            categoryText = tr("Mint");
        } else if (tx.category == "redeem") {
            icon = "💰";
            categoryText = tr("Redeem");
        } else if (tx.category == "send") {
            icon = "📤";
            categoryText = tr("Send");
        } else if (tx.category == "receive") {
            icon = "📥";
            categoryText = tr("Receive");
        } else {
            icon = "💵";
            categoryText = QString::fromStdString(tx.category);
        }

        QLabel* iconLabel = new QLabel(icon);
        iconLabel->setFixedWidth(30);
        layout->addWidget(iconLabel);

        QLabel* categoryLabel = new QLabel(categoryText);
        categoryLabel->setFixedWidth(80);
        layout->addWidget(categoryLabel);

        // Amount
        QLabel* amountLabel = new QLabel(QString("$%1").arg(tx.amount / 100.0, 0, 'f', 2));
        QFont monospaceFont = GUIUtil::fixedPitchFont();
        amountLabel->setFont(monospaceFont);
        amountLabel->setFixedWidth(100);
        amountLabel->setAlignment(Qt::AlignRight);
        layout->addWidget(amountLabel);

        // Confirmations
        QString confirmText;
        if (tx.confirmations == 0) {
            confirmText = tr("Pending");
        } else if (tx.confirmations < 6) {
            confirmText = QString("%1 conf").arg(tx.confirmations);
        } else {
            confirmText = tr("Confirmed");
        }
        QLabel* confirmLabel = new QLabel(confirmText);
        confirmLabel->setFixedWidth(100);
        layout->addWidget(confirmLabel);

        // Date/time
        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(tx.timestamp);
        QLabel* dateLabel = new QLabel(dateTime.toString("MMM dd, yyyy"));
        dateLabel->setAlignment(Qt::AlignRight);
        layout->addWidget(dateLabel);

        layout->addStretch();

        // Add to list
        QListWidgetItem* item = new QListWidgetItem(m_transactionsList);
        item->setSizeHint(itemWidget->sizeHint());
        m_transactionsList->setItemWidget(item, itemWidget);
    }

    // Show/hide based on whether we have transactions
    bool hasTransactions = (m_transactionsList->count() > 0);
    m_recentTransactionsInfo->setVisible(!hasTransactions);
    m_transactionsList->setVisible(hasTransactions);
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

void DigiDollarOverviewWidget::setMonospacedFont(bool use_embedded_font)
{
    QFont f = GUIUtil::fixedPitchFont(use_embedded_font);
    f.setWeight(QFont::Bold);

    // Apply to all value labels
    m_ddBalanceValue->setFont(f);
    m_dgbCollateralValue->setFont(f);
    m_usdValueValue->setFont(f);
    m_oraclePriceValue->setFont(f);
    m_networkTotalDDValue->setFont(f);
    m_networkTotalCollateralValue->setFont(f);
}

// REMOVED: updateTheme() and applyTheme() methods
// All theming is now handled by light.css and dark.css files
// This allows the DigiByte blue theme to work properly

void DigiDollarOverviewWidget::addDemoTransactions()
{
    // Demo transactions removed - only real transactions from wallet will be shown
    // Real transactions are loaded via wallet notification system
}