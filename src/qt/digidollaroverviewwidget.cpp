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

    QVBoxLayout* frameVLayout = new QVBoxLayout(m_balanceFrame);
    frameVLayout->setObjectName("frameVLayout");

    // Title with status indicator layout (similar to main wallet)
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setObjectName("titleLayout");

    QLabel* balanceTitle = new QLabel(tr("DigiDollar Balances"), this);
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
    m_ddBalanceLabel = new QLabel(tr("Available:"), this);
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
    m_dgbCollateralLabel = new QLabel(tr("Collateral:"), this);
    m_dgbCollateralLabel->setObjectName("dgbCollateralLabel");
    m_dgbCollateralValue = new QLabel("0.00000000 DGB", this);
    m_dgbCollateralValue->setObjectName("dgbCollateralValue");
    m_dgbCollateralValue->setCursor(QCursor(Qt::IBeamCursor));
    m_dgbCollateralValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_dgbCollateralValue->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_dgbCollateralValue->setToolTip(tr("Total DGB locked as collateral for DigiDollars"));
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

    QVBoxLayout* frameVLayout = new QVBoxLayout(m_systemHealthFrame);
    frameVLayout->setObjectName("healthFrameVLayout");

    // Title with status indicator layout
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setObjectName("healthTitleLayout");

    QLabel* healthTitle = new QLabel(tr("System Health & Oracle"), this);
    QFont titleFont = healthTitle->font();
    titleFont.setBold(true);
    titleFont.setWeight(75); // Match main wallet weight
    healthTitle->setFont(titleFont);
    titleLayout->addWidget(healthTitle);

    // Add spacer to push content left
    QSpacerItem* titleSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    titleLayout->addItem(titleSpacer);

    frameVLayout->addLayout(titleLayout);

    // Grid layout for health items
    m_systemHealthLayout = new QGridLayout();
    m_systemHealthLayout->setSpacing(12); // Match main wallet spacing
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
    m_systemHealthLayout->addWidget(m_oraclePriceLabel, 1, 0);
    m_systemHealthLayout->addWidget(m_oraclePriceValue, 1, 1);

    // System Health Status
    m_systemHealthLabel = new QLabel(tr("System Status:"), this);
    m_systemHealthLabel->setObjectName("systemHealthLabel");
    m_systemHealthValue = new QLabel("Healthy", this);
    m_systemHealthValue->setObjectName("systemHealthValue");
    m_systemHealthValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_systemHealthValue->setToolTip(tr("Overall DigiDollar system health status"));
    m_systemHealthLayout->addWidget(m_systemHealthLabel, 2, 0);
    m_systemHealthLayout->addWidget(m_systemHealthValue, 2, 1);

    // DCA Level
    m_dcaLevelLabel = new QLabel(tr("DCA Level:"), this);
    m_dcaLevelLabel->setObjectName("dcaLevelLabel");
    m_dcaLevelValue = new QLabel("0", this);
    m_dcaLevelValue->setObjectName("dcaLevelValue");
    m_dcaLevelValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_dcaLevelValue->setToolTip(tr("Current Dollar-Cost Averaging intervention level"));
    m_systemHealthLayout->addWidget(m_dcaLevelLabel, 3, 0);
    m_systemHealthLayout->addWidget(m_dcaLevelValue, 3, 1);

    // ERR Level
    m_errLevelLabel = new QLabel(tr("ERR Level:"), this);
    m_errLevelLabel->setObjectName("errLevelLabel");
    m_errLevelValue = new QLabel("0", this);
    m_errLevelValue->setObjectName("errLevelValue");
    m_errLevelValue->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_errLevelValue->setToolTip(tr("Current Emergency Response Reserve level"));
    m_systemHealthLayout->addWidget(m_errLevelLabel, 4, 0);
    m_systemHealthLayout->addWidget(m_errLevelValue, 4, 1);

    // System Health Progress Bar with improved styling
    m_systemHealthBar = new QProgressBar(this);
    m_systemHealthBar->setObjectName("systemHealthBar");
    m_systemHealthBar->setRange(0, 100);
    m_systemHealthBar->setValue(100); // Start at 100% healthy
    m_systemHealthBar->setTextVisible(true);
    m_systemHealthBar->setFormat("%p% Healthy");
    m_systemHealthBar->setMinimumHeight(20);
    m_systemHealthBar->setToolTip(tr("Visual indicator of overall system health"));
    m_systemHealthLayout->addWidget(m_systemHealthBar, 5, 0, 1, 2);

    // Add horizontal spacer
    QSpacerItem* horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_systemHealthLayout->addItem(horizontalSpacer, 2, 2, 1, 1);

    frameVLayout->addLayout(m_systemHealthLayout);
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
    m_transactionsList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transactionsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transactionsList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_transactionsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_transactionsList->setUniformItemSizes(true);
    m_transactionsList->setMinimumHeight(150); // Set reasonable height

    // Info label for when no transactions exist
    m_recentTransactionsInfo = new QLabel(tr("No recent DigiDollar transactions"), this);
    m_recentTransactionsInfo->setObjectName("recentTransactionsInfo");
    // Theme styling will be applied in applyTheme()
    m_recentTransactionsInfo->setAlignment(Qt::AlignCenter);
    m_recentTransactionsInfo->setVisible(true);

    frameVLayout->addWidget(m_transactionsList);
    frameVLayout->addWidget(m_recentTransactionsInfo);

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
        // Connect client model signals for oracle price updates
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

        // TODO: Get locked collateral from wallet positions
        // For now, use mock value as this requires position tracking
        m_dgbCollateral = 0.0;

        // Try to get actual available DGB balance
        CAmount dgbBalance = m_walletModel->getAvailableDGBBalance();
        // Store this for display purposes if needed
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
    // Get price from MockOracleManager if in RegTest, otherwise use real oracle
    if (Params().GetChainType() == ChainType::REGTEST && MockOracleManager::GetInstance().IsEnabled()) {
        // Get price from mock oracle (price is in cents per DGB)
        // e.g., 1000000 cents/DGB = $10,000/DGB
        // e.g., 1 cent/DGB = $0.01/DGB
        CAmount priceCents = MockOracleManager::GetInstance().GetCurrentPrice();

        // Convert cents to dollars
        m_oraclePrice = priceCents / 100.0;
    } else {
        // TODO: Get actual oracle price from production oracle system
        // For now, use a default price
        m_oraclePrice = 0.015; // Default: $0.015 per DGB
    }

    if (m_oraclePrice > 0) {
        m_oraclePriceValue->setText(QString("$%1").arg(QString::number(m_oraclePrice, 'f', 6)));
    } else {
        m_oraclePriceValue->setText("Loading...");
    }
}

void DigiDollarOverviewWidget::updateSystemHealth()
{
    // Get real system health metrics from DigiDollar system
    QString healthStatus = "Healthy";
    int healthPercentage = 100;
    m_dcaLevel = 0;
    m_errLevel = 0;

    try {
        // Get current system health using DCA functions
        int systemHealth = DigiDollar::DCA::DynamicCollateralAdjustment::GetCurrentSystemHealth();

        // Get current tier information
        auto tier = DigiDollar::DCA::DynamicCollateralAdjustment::GetCurrentTier(systemHealth);

        // Determine health status based on tier
        if (systemHealth >= 150) {
            healthStatus = "Healthy";
            healthPercentage = 100;
        } else if (systemHealth >= 120) {
            healthStatus = "Good";
            healthPercentage = 85;
        } else if (systemHealth >= 100) {
            healthStatus = "Monitoring";
            healthPercentage = 70;
            m_dcaLevel = 1;
        } else {
            healthStatus = "Critical";
            healthPercentage = 50;
            m_dcaLevel = 2;
            m_errLevel = 1;
        }

        // Check if system is in emergency
        bool isEmergency = DigiDollar::DCA::DynamicCollateralAdjustment::IsSystemEmergency(systemHealth);
        if (isEmergency) {
            healthStatus = "Emergency";
            healthPercentage = 25;
            m_errLevel = 2;
        }

    } catch (const std::exception& e) {
        // If there's an error getting system health, show unknown status
        healthStatus = "Unknown";
        healthPercentage = 50;
    }

    m_systemHealthValue->setText(healthStatus);
    m_dcaLevelValue->setText(QString::number(m_dcaLevel));
    m_errLevelValue->setText(QString::number(m_errLevel));
    m_systemHealthBar->setValue(healthPercentage);
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
}

// REMOVED: updateTheme() and applyTheme() methods
// All theming is now handled by light.css and dark.css files
// This allows the DigiByte blue theme to work properly

void DigiDollarOverviewWidget::addDemoTransactions()
{
    // Demo transactions removed - only real transactions from wallet will be shown
    // Real transactions are loaded via wallet notification system
}