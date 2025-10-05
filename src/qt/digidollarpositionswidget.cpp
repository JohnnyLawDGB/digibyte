// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarpositionswidget.h>

#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/digibyteunits.h>
#include <interfaces/wallet.h>
#include <wallet/digidollarwallet.h>
#include <consensus/amount.h>
#include <algorithm>

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QProgressBar>
#include <QFont>
#include <QMessageBox>
#include <QTimer>
#include <QFrame>
#include <QApplication>
#include <QPalette>
#include <QMenu>
#include <QAction>

DigiDollarPositionsWidget::DigiDollarPositionsWidget(QWidget *parent) :
    QWidget(parent),
    m_mainLayout(nullptr),
    m_headerLayout(nullptr),
    m_titleLabel(nullptr),
    m_refreshButton(nullptr),
    m_positionsTable(nullptr),
    m_statusLabel(nullptr),
    m_walletModel(nullptr),
    m_clientModel(nullptr)
{
    setupUI();
    connectSignals();
    // applyTheme(); // REMOVED: Now handled by CSS files
}

DigiDollarPositionsWidget::~DigiDollarPositionsWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarPositionsWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);

    // Header section
    m_headerLayout = new QHBoxLayout();

    m_titleLabel = new QLabel(tr("DigiDollar Time Lock DGB Vault"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_titleLabel->setFont(titleFont);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setObjectName("refreshButton");
    m_refreshButton->setFixedSize(100, 30);

    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addStretch();
    m_headerLayout->addWidget(m_refreshButton);

    m_mainLayout->addLayout(m_headerLayout);

    // Positions table
    m_positionsTable = new QTableWidget(this);
    m_positionsTable->setObjectName("positionsTable");
    m_positionsTable->setColumnCount(NUM_COLUMNS);
    m_positionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_positionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_positionsTable->setAlternatingRowColors(true);
    m_positionsTable->setSortingEnabled(true);
    m_positionsTable->verticalHeader()->setVisible(false);

    // Setup table header
    setupTableHeader();

    m_mainLayout->addWidget(m_positionsTable);

    // Status label
    m_statusLabel = new QLabel(tr("Loading vaults..."), this);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    // Theme styling will be applied in applyTheme()
    m_mainLayout->addWidget(m_statusLabel);

    setLayout(m_mainLayout);
}

void DigiDollarPositionsWidget::setupTableHeader()
{
    QStringList headers;
    headers << tr("Vault ID")
            << tr("DD Minted")
            << tr("DGB Collateral")
            << tr("Lock Period")
            << tr("Time Remaining")
            << tr("Health")
            << tr("Actions");

    m_positionsTable->setHorizontalHeaderLabels(headers);

    // Enhanced header configuration
    QHeaderView* header = m_positionsTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setDefaultAlignment(Qt::AlignCenter);
    header->setHighlightSections(true);
    header->setMinimumSectionSize(80);

    // Set column resize modes for optimal layout
    header->setSectionResizeMode(COL_POSITION_ID, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_DD_MINTED, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_DGB_COLLATERAL, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_LOCK_TIER, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_TIME_REMAINING, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_HEALTH, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_ACTIONS, QHeaderView::Fixed);

    // Set optimized column widths for better readability
    m_positionsTable->setColumnWidth(COL_POSITION_ID, 110);
    m_positionsTable->setColumnWidth(COL_DD_MINTED, 130);
    m_positionsTable->setColumnWidth(COL_DGB_COLLATERAL, 150);
    m_positionsTable->setColumnWidth(COL_LOCK_TIER, 100);
    m_positionsTable->setColumnWidth(COL_TIME_REMAINING, 120);
    m_positionsTable->setColumnWidth(COL_HEALTH, 110);
    m_positionsTable->setColumnWidth(COL_ACTIONS, 100);

    // Add sort indicators to sortable columns
    m_positionsTable->setSortingEnabled(true);
    header->setSortIndicatorShown(true);

    // Set sorting behavior - enable section clicking
    header->setSectionsClickable(true);

    // Add tooltips to headers for better UX
    QTableWidgetItem* idHeaderItem = m_positionsTable->horizontalHeaderItem(COL_POSITION_ID);
    if (idHeaderItem) idHeaderItem->setToolTip(tr("Unique identifier for this DigiDollar vault"));

    QTableWidgetItem* ddHeaderItem = m_positionsTable->horizontalHeaderItem(COL_DD_MINTED);
    if (ddHeaderItem) ddHeaderItem->setToolTip(tr("Amount of DigiDollar tokens minted for this position"));

    QTableWidgetItem* dgbHeaderItem = m_positionsTable->horizontalHeaderItem(COL_DGB_COLLATERAL);
    if (dgbHeaderItem) dgbHeaderItem->setToolTip(tr("DigiByte collateral locked in this position"));

    QTableWidgetItem* tierHeaderItem = m_positionsTable->horizontalHeaderItem(COL_LOCK_TIER);
    if (tierHeaderItem) tierHeaderItem->setToolTip(tr("Time lock period for this vault"));

    QTableWidgetItem* timeHeaderItem = m_positionsTable->horizontalHeaderItem(COL_TIME_REMAINING);
    if (timeHeaderItem) timeHeaderItem->setToolTip(tr("Time remaining before position can be redeemed"));

    QTableWidgetItem* healthHeaderItem = m_positionsTable->horizontalHeaderItem(COL_HEALTH);
    if (healthHeaderItem) healthHeaderItem->setToolTip(tr("Position health based on collateralization ratio"));

    QTableWidgetItem* actionsHeaderItem = m_positionsTable->horizontalHeaderItem(COL_ACTIONS);
    if (actionsHeaderItem) actionsHeaderItem->setToolTip(tr("Available actions for this position"));
}

void DigiDollarPositionsWidget::connectSignals()
{
    // Connect refresh button
    connect(m_refreshButton, &QPushButton::clicked,
            this, &DigiDollarPositionsWidget::onRefreshClicked);

    // Connect table clicks
    connect(m_positionsTable, &QTableWidget::cellClicked,
            this, &DigiDollarPositionsWidget::onPositionClicked);

    // Connect context menu
    connect(m_positionsTable, &QTableWidget::customContextMenuRequested,
            this, &DigiDollarPositionsWidget::showContextMenu);

    // Auto-refresh timer (every 60 seconds)
    QTimer* autoRefreshTimer = new QTimer(this);
    connect(autoRefreshTimer, &QTimer::timeout,
            this, &DigiDollarPositionsWidget::updatePositions);
    autoRefreshTimer->start(60000); // 60 seconds
}

void DigiDollarPositionsWidget::connectWalletSignals()
{
    if (!m_walletModel) {
        return;
    }

    // Connect to wallet balance changes (indicates new transactions)
    connect(m_walletModel, &WalletModel::balanceChanged,
            this, &DigiDollarPositionsWidget::updatePositions);
}

void DigiDollarPositionsWidget::connectClientSignals()
{
    if (!m_clientModel) {
        return;
    }

    // Connect to new blocks (for timelock countdown and health updates)
    connect(m_clientModel, &ClientModel::numBlocksChanged,
            this, &DigiDollarPositionsWidget::updatePositions);
}

void DigiDollarPositionsWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals for real-time updates
        connectWalletSignals();
        updatePositions();
        // applyTheme(); // REMOVED: Now handled by CSS files
    }
}

void DigiDollarPositionsWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals for block updates
        connectClientSignals();
        updatePositions();
        // applyTheme(); // REMOVED: Now handled by CSS files
    }
}

void DigiDollarPositionsWidget::updateView()
{
    updatePositions();
}

void DigiDollarPositionsWidget::updatePositions()
{
    loadPositionsFromWallet();
    populatePositionsTable();
}

void DigiDollarPositionsWidget::onRefreshClicked()
{
    m_statusLabel->setText(tr("Refreshing vaults..."));
    updatePositions();
}

void DigiDollarPositionsWidget::onPositionClicked(int row, int column)
{
    if (row < 0 || row >= m_positionsTable->rowCount()) {
        return;
    }

    // Get position ID from the first column
    QTableWidgetItem* idItem = m_positionsTable->item(row, COL_POSITION_ID);
    if (!idItem) return;

    QString positionId = idItem->text();

    // Handle different column clicks
    switch (column) {
    case COL_POSITION_ID:
        // Copy vault ID to clipboard
        GUIUtil::setClipboard(positionId);
        Q_EMIT message(tr("Vault ID Copied"),
                    tr("Vault ID copied to clipboard: %1").arg(positionId),
                    QMessageBox::Information);
        break;
    default:
        // Other columns - could add more functionality here
        break;
    }
}

void DigiDollarPositionsWidget::onRedeemPositionClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QString positionId = button->property("positionId").toString();

    // Find the position in our list
    DigiDollarPosition position;
    bool found = false;
    for (const auto& pos : m_positions) {
        if (pos.positionId == positionId) {
            position = pos;
            found = true;
            break;
        }
    }

    if (!found) {
        Q_EMIT message(tr("Error"), tr("Position not found: %1").arg(positionId), QMessageBox::Warning);
        return;
    }

    // Emit signal to request redemption (will be handled by parent tab)
    Q_EMIT redeemRequested(positionId);
}

void DigiDollarPositionsWidget::showContextMenu(const QPoint& point)
{
    QTableWidgetItem* item = m_positionsTable->itemAt(point);
    if (!item) return;

    int row = item->row();
    if (row < 0 || row >= m_positions.size()) return;

    const DigiDollarPosition& position = m_positions[row];

    QMenu contextMenu(this);

    // Copy Vault ID action
    QAction* copyIdAction = contextMenu.addAction(tr("📋 Copy Vault ID"));
    connect(copyIdAction, &QAction::triggered, [this, position]() {
        GUIUtil::setClipboard(position.positionId);
        Q_EMIT message(tr("Vault ID Copied"),
                    tr("Vault ID copied to clipboard: %1").arg(position.positionId),
                    QMessageBox::Information);
    });

    contextMenu.addSeparator();

    // Show Details action
    QAction* detailsAction = contextMenu.addAction(tr("📊 Show Details"));
    connect(detailsAction, &QAction::triggered, [this, position]() {
        // Get lock period name
        QString lockPeriodName;
        switch(position.lockTier) {
            case 1: lockPeriodName = tr("30 days"); break;
            case 2: lockPeriodName = tr("3 months"); break;
            case 3: lockPeriodName = tr("6 months"); break;
            case 4: lockPeriodName = tr("1 year"); break;
            case 5: lockPeriodName = tr("3 years"); break;
            case 6: lockPeriodName = tr("5 years"); break;
            case 7: lockPeriodName = tr("7 years"); break;
            case 8: lockPeriodName = tr("10 years"); break;
            default: lockPeriodName = tr("1 year"); break;
        }

        QString details = tr("Vault Details\n\n"
                           "Vault ID: %1\n"
                           "DD Minted: %2\n"
                           "DGB Collateral: %3\n"
                           "Lock Period: %4\n"
                           "Blocks Remaining: %5\n"
                           "Health: %6%\n"
                           "Can Redeem: %7")
                           .arg(position.positionId)
                           .arg(formatDDAmount(position.ddMinted))
                           .arg(formatDGBAmount(position.dgbCollateral))
                           .arg(lockPeriodName)
                           .arg(position.blocksRemaining)
                           .arg(QString::number(position.health, 'f', 1))
                           .arg(position.canRedeem ? tr("Yes") : tr("No"));

        QMessageBox::information(this, tr("Vault Details"), details);
    });

    contextMenu.addSeparator();

    // Redeem action (if applicable)
    if (position.canRedeem) {
        QAction* redeemAction = contextMenu.addAction(tr("💰 Redeem Vault"));
        connect(redeemAction, &QAction::triggered, [this, position]() {
            // Emit signal to request redemption
            Q_EMIT redeemRequested(position.positionId);
        });
    }

    // Style the context menu to match the theme
    QPalette palette = QApplication::palette();
    int lightness = palette.color(QPalette::WindowText).lightness();
    bool isDarkTheme = lightness > 127;

    QString menuStyle = QString(
        "QMenu { "
        "  background-color: %1; "
        "  color: %2; "
        "  border: 1px solid %3; "
        "  border-radius: 4px; "
        "  padding: 4px; "
        "} "
        "QMenu::item { "
        "  padding: 8px 16px; "
        "  border-radius: 3px; "
        "} "
        "QMenu::item:selected { "
        "  background-color: %4; "
        "  color: white; "
        "}")
        .arg(isDarkTheme ? "#3a3a3a" : "#ffffff")
        .arg(isDarkTheme ? "#e0e0e0" : "#333333")
        .arg(isDarkTheme ? "#555555" : "#cccccc")
        .arg("#0078d4");

    contextMenu.setStyleSheet(menuStyle);
    contextMenu.exec(m_positionsTable->mapToGlobal(point));
}

void DigiDollarPositionsWidget::loadPositionsFromWallet()
{
    m_positions.clear();

    if (!m_walletModel || !m_clientModel) {
        return;
    }

    // Get wallet backend interface
    interfaces::Wallet& wallet = m_walletModel->wallet();

    // Get current blockchain height for timelock calculations
    int currentHeight = m_clientModel->getNumBlocks();

    // Get current oracle price (RegTest: $0.01 = 1000000 cents per DGB)
    // In RegTest, we use the mock oracle price
    CAmount oraclePrice = GetMockOraclePrice();

    // Get positions from wallet backend
    std::vector<WalletCollateralPosition> walletPositions = GetWalletPositions();

    for (const auto& wp : walletPositions) {
        if (!wp.is_active) {
            continue; // Skip inactive positions
        }

        DigiDollarPosition pos;

        // Position ID (use txid as string)
        pos.positionId = QString::fromStdString(wp.dd_timelock_id.ToString());

        // DD minted (convert from cents to DD with 8 decimals)
        pos.ddMinted = wp.dd_minted / 100.0; // cents to dollars

        // DGB collateral (convert from satoshis to DGB)
        pos.dgbCollateral = wp.dgb_collateral / 100000000.0;

        // Lock tier
        pos.lockTier = wp.lock_tier;

        // Calculate blocks remaining until unlock
        pos.blocksRemaining = std::max<int64_t>(0, wp.unlock_height - currentHeight);

        // Calculate health ratio using actual oracle price
        pos.health = CalculatePositionHealth(wp.dd_minted, wp.dgb_collateral, oraclePrice);

        // Can redeem if timelock expired
        pos.canRedeem = (pos.blocksRemaining == 0);

        m_positions.append(pos);
    }
}

void DigiDollarPositionsWidget::populatePositionsTable()
{
    // Clear existing rows
    m_positionsTable->setRowCount(0);

    if (m_positions.isEmpty()) {
        m_statusLabel->setText(tr("📋 No DigiDollar vaults found\n\nYou haven't created any DigiDollar vaults yet.\nUse the 'Mint' tab to create your first DigiDollar vault."));
        m_statusLabel->show();
        return;
    }

    // Hide status label when we have positions
    m_statusLabel->hide();

    // Add positions to table
    m_positionsTable->setRowCount(m_positions.size());
    for (int i = 0; i < m_positions.size(); ++i) {
        addPositionToTable(m_positions[i], i);
    }

    // Resize table to fit content
    m_positionsTable->resizeRowsToContents();
}

void DigiDollarPositionsWidget::addPositionToTable(const DigiDollarPosition& position, int row)
{
    QFont monospaceFont = GUIUtil::fixedPitchFont();

    // Vault ID
    QTableWidgetItem* idItem = new QTableWidgetItem(position.positionId);
    idItem->setFont(monospaceFont);
    idItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    idItem->setToolTip(tr("Vault ID: %1\nClick to copy to clipboard").arg(position.positionId));
    m_positionsTable->setItem(row, COL_POSITION_ID, idItem);

    // DD Minted
    QTableWidgetItem* ddItem = new QTableWidgetItem(formatDDAmount(position.ddMinted));
    ddItem->setFont(monospaceFont);
    ddItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ddItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ddItem->setToolTip(tr("DigiDollar Minted: %1\nThis is the amount of DD tokens you received for this position")
                     .arg(formatDDAmount(position.ddMinted)));
    m_positionsTable->setItem(row, COL_DD_MINTED, ddItem);

    // DGB Collateral
    QTableWidgetItem* dgbItem = new QTableWidgetItem(formatDGBAmount(position.dgbCollateral));
    dgbItem->setFont(monospaceFont);
    dgbItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    dgbItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dgbItem->setToolTip(tr("DGB Collateral: %1\nThis is your locked DigiByte collateral that backs this position")
                       .arg(formatDGBAmount(position.dgbCollateral)));
    m_positionsTable->setItem(row, COL_DGB_COLLATERAL, dgbItem);

    // Lock Period
    QString lockPeriodName;
    QString lockPeriodTooltip;
    switch(position.lockTier) {
        case 0:
            lockPeriodName = tr("1 hour");
            lockPeriodTooltip = tr("1 hour time lock (1000% collateral) - TESTING ONLY");
            break;
        case 1:
            lockPeriodName = tr("30 days");
            lockPeriodTooltip = tr("30 day time lock (500% collateral)");
            break;
        case 2:
            lockPeriodName = tr("3 months");
            lockPeriodTooltip = tr("3 month time lock (400% collateral)");
            break;
        case 3:
            lockPeriodName = tr("6 months");
            lockPeriodTooltip = tr("6 month time lock (350% collateral)");
            break;
        case 4:
            lockPeriodName = tr("1 year");
            lockPeriodTooltip = tr("1 year time lock (300% collateral)");
            break;
        case 5:
            lockPeriodName = tr("3 years");
            lockPeriodTooltip = tr("3 year time lock (250% collateral)");
            break;
        case 6:
            lockPeriodName = tr("5 years");
            lockPeriodTooltip = tr("5 year time lock (225% collateral)");
            break;
        case 7:
            lockPeriodName = tr("7 years");
            lockPeriodTooltip = tr("7 year time lock (212% collateral)");
            break;
        case 8:
            lockPeriodName = tr("10 years");
            lockPeriodTooltip = tr("10 year time lock (200% collateral)");
            break;
        default:
            lockPeriodName = tr("1 year");
            lockPeriodTooltip = tr("1 year time lock (300% collateral)");
    }

    QTableWidgetItem* tierItem = new QTableWidgetItem(lockPeriodName);
    tierItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    tierItem->setTextAlignment(Qt::AlignCenter);
    tierItem->setToolTip(lockPeriodTooltip);
    m_positionsTable->setItem(row, COL_LOCK_TIER, tierItem);

    // Time Remaining
    QTableWidgetItem* timeItem = new QTableWidgetItem(formatBlockTime(position.blocksRemaining));
    timeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    timeItem->setTextAlignment(Qt::AlignCenter);

    // Enhanced styling for expired positions
    if (position.blocksRemaining <= 0) {
        QPalette palette = QApplication::palette();
        int lightness = palette.color(QPalette::WindowText).lightness();
        bool isDarkTheme = lightness > 127;
        QString errorColor = isDarkTheme ? "#f44336" : "#dc3545";
        QString errorBg = isDarkTheme ? "#4a1f1f" : "#fee";

        timeItem->setForeground(QBrush(QColor(errorColor)));
        timeItem->setBackground(QBrush(QColor(errorBg)));
        timeItem->setFont(QFont(timeItem->font().family(), timeItem->font().pointSize(), QFont::Bold));
        timeItem->setToolTip(tr("⚠️ This position has expired and can be redeemed immediately"));
    } else if (position.blocksRemaining <= 100) { // Warning for positions expiring soon
        QPalette palette = QApplication::palette();
        int lightness = palette.color(QPalette::WindowText).lightness();
        bool isDarkTheme = lightness > 127;
        QString warningColor = isDarkTheme ? "#ff9800" : "#856404";
        QString warningBg = isDarkTheme ? "#4a3d1a" : "#fff3cd";

        timeItem->setForeground(QBrush(QColor(warningColor)));
        timeItem->setBackground(QBrush(QColor(warningBg)));
        timeItem->setToolTip(tr("⏰ This position expires soon (less than 100 blocks remaining)"));
    } else {
        timeItem->setToolTip(tr("Time remaining until this position can be redeemed"));
    }
    m_positionsTable->setItem(row, COL_TIME_REMAINING, timeItem);

    // Health Status (using a custom widget with progress bar)
    QWidget* healthWidget = createHealthWidget(position.health);
    m_positionsTable->setCellWidget(row, COL_HEALTH, healthWidget);

    // Actions (Redeem button)
    QPushButton* redeemButton = createRedeemButton(position.positionId);
    redeemButton->setEnabled(position.canRedeem);
    m_positionsTable->setCellWidget(row, COL_ACTIONS, redeemButton);
}

QPushButton* DigiDollarPositionsWidget::createRedeemButton(const QString& positionId)
{
    QPushButton* button = new QPushButton(tr("Redeem"), this);
    button->setProperty("positionId", positionId);
    button->setFixedSize(80, 28);

    // Apply enhanced theme-aware styling
    QPalette palette = QApplication::palette();
    int lightness = palette.color(QPalette::WindowText).lightness();
    bool isDarkTheme = lightness > 127;

    QString successColor = isDarkTheme ? "#4caf50" : "#28a745";
    QString successHover = isDarkTheme ? "#5cbf60" : "#34ce57";
    QString successPressed = isDarkTheme ? "#449d48" : "#1e7e34";
    QString disabledBg = isDarkTheme ? "#555555" : "#cccccc";
    QString disabledText = isDarkTheme ? "#999999" : "#888888";

    QString redeemButtonStyle = QString(
        "QPushButton { "
        "  background-color: %1; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 5px; "
        "  padding: 6px 12px; "
        "  font-weight: 600; "
        "  font-size: 11px; "
        "  min-width: 60px; "
        "} "
        "QPushButton:hover { "
        "  background-color: %2; "
        "  transform: translateY(-1px); "
        "} "
        "QPushButton:pressed { "
        "  background-color: %3; "
        "  transform: translateY(0px); "
        "} "
        "QPushButton:disabled { "
        "  background-color: %4; "
        "  color: %5; "
        "  transform: none; "
        "}")
        .arg(successColor)
        .arg(successHover)
        .arg(successPressed)
        .arg(disabledBg)
        .arg(disabledText);

    button->setStyleSheet(redeemButtonStyle);

    // Add tooltip
    button->setToolTip(tr("Click to redeem this DigiDollar position\nThis will return your DGB collateral and burn the DD tokens"));

    connect(button, &QPushButton::clicked,
            this, &DigiDollarPositionsWidget::onRedeemPositionClicked);

    return button;
}

QWidget* DigiDollarPositionsWidget::createHealthWidget(double health) const
{
    QWidget* widget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(0);

    QProgressBar* healthBar = new QProgressBar(widget);
    healthBar->setRange(0, 200);  // Allow up to 200% collateralization
    healthBar->setValue(static_cast<int>(health));
    healthBar->setFormat(QString("%1%").arg(QString::number(health, 'f', 0)));
    healthBar->setFixedHeight(20);
    healthBar->setMinimumWidth(80);

    // Use system palette for theme-aware styling
    QPalette palette = QApplication::palette();
    int lightness = palette.color(QPalette::WindowText).lightness();
    bool isDarkTheme = lightness > 127;

    QString borderColor = palette.color(QPalette::Mid).name();
    QString textColor = palette.color(QPalette::WindowText).name();
    QString healthBg = palette.color(QPalette::Base).name();

    // Determine health color and gradient (only custom colors for status indicators)
    // 100%+ = green (healthy, over-collateralized)
    // 80-99% = yellow (warning, under-collateralized)
    // <80% = red (at risk, severely under-collateralized)
    QString healthColor, gradientEnd;
    if (health >= 100) {
        healthColor = isDarkTheme ? "#4caf50" : "#28a745";
        gradientEnd = isDarkTheme ? "#66bb6a" : "#4caf50";
    } else if (health >= 80) {
        healthColor = isDarkTheme ? "#ff9800" : "#ffc107";
        gradientEnd = isDarkTheme ? "#ffb74d" : "#ff9800";
    } else {
        healthColor = isDarkTheme ? "#f44336" : "#dc3545";
        gradientEnd = isDarkTheme ? "#ef5350" : "#f44336";
    }

    // Apply professional health bar styling with gradient
    QString healthBarStyle = QString(
        "QProgressBar { "
        "  background-color: %1; "
        "  border: 1px solid %2; "
        "  border-radius: 5px; "
        "  text-align: center; "
        "  color: %3; "
        "  font-weight: bold; "
        "  font-size: 10px; "
        "  padding: 1px; "
        "} "
        "QProgressBar::chunk { "
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "    stop:0 %4, stop:0.5 %5, stop:1 %4); "
        "  border-radius: 4px; "
        "  margin: 1px; "
        "}")
        .arg(healthBg)
        .arg(borderColor)
        .arg(textColor)
        .arg(healthColor)
        .arg(gradientEnd);

    healthBar->setStyleSheet(healthBarStyle);

    // Add tooltip with health information
    QString healthStatus;
    if (health >= 120) {
        healthStatus = tr("Healthy - Over-Collateralized");
    } else if (health >= 100) {
        healthStatus = tr("Adequate - At Required Ratio");
    } else if (health >= 80) {
        healthStatus = tr("Warning - Below Required Ratio");
    } else {
        healthStatus = tr("At Risk - Under-Collateralized");
    }

    healthBar->setToolTip(tr("Vault Health: %1% (%2)\n100% = Required collateral ratio\nAbove 100% = Over-collateralized (safer)\nBelow 100% = Under-collateralized (at risk)")
                         .arg(QString::number(health, 'f', 1))
                         .arg(healthStatus));

    layout->addWidget(healthBar);
    widget->setLayout(layout);

    return widget;
}

QString DigiDollarPositionsWidget::formatDDAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DD";
}

QString DigiDollarPositionsWidget::formatDGBAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DGB";
}

QString DigiDollarPositionsWidget::formatBlockTime(int blocks) const
{
    if (blocks <= 0) return tr("Expired");

    // Estimate time remaining (15 seconds per block)
    int totalSeconds = blocks * 15;
    int days = totalSeconds / (24 * 3600);
    int hours = (totalSeconds % (24 * 3600)) / 3600;
    int minutes = (totalSeconds % 3600) / 60;

    if (days > 0) {
        return QString("%1d %2h").arg(days).arg(hours);
    } else if (hours > 0) {
        return QString("%1h %2m").arg(hours).arg(minutes);
    } else {
        return QString("%1m").arg(minutes);
    }
}

QString DigiDollarPositionsWidget::formatHealthStatus(double health) const
{
    return QString("%1%").arg(QString::number(health, 'f', 1));
}

// REMOVED: applyTheme() - All styling now handled by CSS files (light.css/dark.css)
// This method was overriding the CSS theme with programmatic styling
void DigiDollarPositionsWidget::applyTheme()
{
    // Method disabled - CSS handles all theming now
}

// =============================================================================
// Backend Integration Helper Functions
// =============================================================================

CAmount DigiDollarPositionsWidget::GetMockOraclePrice() const
{
    // For RegTest, we use a mock oracle price
    // Default: $0.01 per DGB = 1000000 cents per 100M satoshis
    // This means 1 DGB (100M sats) = $0.01 = 1 cent

    // Try to get price from oracle system if available
    // For now, use default RegTest price
    // TODO: Integrate with mock oracle RPC when available

    return 1000000; // $0.01 per DGB in RegTest
}

std::vector<WalletCollateralPosition> DigiDollarPositionsWidget::GetWalletPositions() const
{
    std::vector<WalletCollateralPosition> positions;

    if (!m_walletModel) {
        return positions;
    }

    // Access DigiDollarWallet directly from wallet model
    DigiDollarWallet* ddWallet = m_walletModel->wallet().getDigiDollarWallet();
    if (ddWallet) {
        positions = ddWallet->GetDDTimeLocks(true); // active time locks only
    }

    return positions;
}

double DigiDollarPositionsWidget::CalculatePositionHealth(CAmount ddAmount, CAmount dgbCollateral, CAmount oraclePrice) const
{
    if (ddAmount == 0) {
        return 300.0; // Perfect health if no DD issued
    }

    // Calculate DGB collateral value in cents
    // dgbCollateral is in satoshis (1 DGB = 100,000,000 satoshis)
    // oraclePrice is in cents per DGB
    // So: collateralValue = (dgbCollateral * oraclePrice) / 100,000,000

    CAmount collateralValueCents = (dgbCollateral * oraclePrice) / 100000000LL;

    // Health ratio = (Collateral Value / DD Value) * 100
    // ddAmount is already in cents
    double healthRatio = (static_cast<double>(collateralValueCents) * 100.0) / static_cast<double>(ddAmount);

    // Cap at 300% for display purposes
    return std::min(healthRatio, 300.0);
}