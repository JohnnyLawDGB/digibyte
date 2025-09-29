// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarpositionswidget.h>

#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/digibyteunits.h>

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
}

DigiDollarPositionsWidget::~DigiDollarPositionsWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarPositionsWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header section
    m_headerLayout = new QHBoxLayout();

    m_titleLabel = new QLabel(tr("DigiDollar Positions"), this);
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
    m_statusLabel = new QLabel(tr("Loading positions..."), this);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("QLabel { color: #666666; padding: 20px; }");
    m_mainLayout->addWidget(m_statusLabel);

    setLayout(m_mainLayout);
}

void DigiDollarPositionsWidget::setupTableHeader()
{
    QStringList headers;
    headers << tr("Position ID")
            << tr("DD Minted")
            << tr("DGB Collateral")
            << tr("Lock Tier")
            << tr("Time Remaining")
            << tr("Health")
            << tr("Actions");

    m_positionsTable->setHorizontalHeaderLabels(headers);

    // Set column widths
    QHeaderView* header = m_positionsTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(COL_POSITION_ID, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_DD_MINTED, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_DGB_COLLATERAL, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_LOCK_TIER, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_TIME_REMAINING, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_HEALTH, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_ACTIONS, QHeaderView::Fixed);

    // Set initial column widths
    m_positionsTable->setColumnWidth(COL_POSITION_ID, 120);
    m_positionsTable->setColumnWidth(COL_DD_MINTED, 120);
    m_positionsTable->setColumnWidth(COL_DGB_COLLATERAL, 120);
    m_positionsTable->setColumnWidth(COL_LOCK_TIER, 80);
    m_positionsTable->setColumnWidth(COL_TIME_REMAINING, 120);
    m_positionsTable->setColumnWidth(COL_HEALTH, 100);
    m_positionsTable->setColumnWidth(COL_ACTIONS, 100);
}

void DigiDollarPositionsWidget::connectSignals()
{
    // Connect refresh button
    connect(m_refreshButton, &QPushButton::clicked,
            this, &DigiDollarPositionsWidget::onRefreshClicked);

    // Connect table clicks
    connect(m_positionsTable, &QTableWidget::cellClicked,
            this, &DigiDollarPositionsWidget::onPositionClicked);

    // Auto-refresh timer (every 60 seconds)
    QTimer* autoRefreshTimer = new QTimer(this);
    connect(autoRefreshTimer, &QTimer::timeout,
            this, &DigiDollarPositionsWidget::updatePositions);
    autoRefreshTimer->start(60000); // 60 seconds
}

void DigiDollarPositionsWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals
        updatePositions();
    }
}

void DigiDollarPositionsWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals
        updatePositions();
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
    m_statusLabel->setText(tr("Refreshing positions..."));
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
        // Copy position ID to clipboard
        GUIUtil::setClipboard(positionId);
        emit message(tr("Position ID Copied"),
                    tr("Position ID copied to clipboard: %1").arg(positionId),
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
        emit message(tr("Error"), tr("Position not found: %1").arg(positionId), QMessageBox::Warning);
        return;
    }

    // Confirm redeem
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Redeem"));
    msgBox.setText(tr("Redeem position %1?").arg(positionId));
    msgBox.setInformativeText(tr("DD Minted: %1\nDGB Collateral: %2\nHealth: %3%")
                             .arg(formatDDAmount(position.ddMinted))
                             .arg(formatDGBAmount(position.dgbCollateral))
                             .arg(QString::number(position.health, 'f', 1)));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        // TODO: Actually create the redeem transaction
        emit message(tr("Redeem Transaction Created"),
                    tr("Redeem transaction created for position %1").arg(positionId),
                    QMessageBox::Information);

        // Refresh the table after a delay
        QTimer::singleShot(2000, this, &DigiDollarPositionsWidget::updatePositions);
    }
}

void DigiDollarPositionsWidget::loadPositionsFromWallet()
{
    // In a real implementation, this would query the wallet for all DigiDollar positions
    // For now, we'll create some sample data

    m_positions.clear();

    if (m_walletModel) {
        // TODO: Load actual positions from wallet
        // For demonstration, add some sample positions
        DigiDollarPosition pos1;
        pos1.positionId = "abc12345";
        pos1.ddMinted = 100.0;
        pos1.dgbCollateral = 15000.0;
        pos1.lockTier = 3;
        pos1.blocksRemaining = 256;
        pos1.health = 85.0;
        pos1.canRedeem = true;
        m_positions.append(pos1);

        DigiDollarPosition pos2;
        pos2.positionId = "def67890";
        pos2.ddMinted = 250.0;
        pos2.dgbCollateral = 35000.0;
        pos2.lockTier = 5;
        pos2.blocksRemaining = 1024;
        pos2.health = 92.0;
        pos2.canRedeem = true;
        m_positions.append(pos2);

        DigiDollarPosition pos3;
        pos3.positionId = "ghi11111";
        pos3.ddMinted = 50.0;
        pos3.dgbCollateral = 8000.0;
        pos3.lockTier = 2;
        pos3.blocksRemaining = 0; // Expired
        pos3.health = 65.0;
        pos3.canRedeem = true;
        m_positions.append(pos3);
    }
}

void DigiDollarPositionsWidget::populatePositionsTable()
{
    // Clear existing rows
    m_positionsTable->setRowCount(0);

    if (m_positions.isEmpty()) {
        m_statusLabel->setText(tr("No DigiDollar positions found"));
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

    // Position ID
    QTableWidgetItem* idItem = new QTableWidgetItem(position.positionId);
    idItem->setFont(monospaceFont);
    idItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    idItem->setToolTip(tr("Click to copy position ID"));
    m_positionsTable->setItem(row, COL_POSITION_ID, idItem);

    // DD Minted
    QTableWidgetItem* ddItem = new QTableWidgetItem(formatDDAmount(position.ddMinted));
    ddItem->setFont(monospaceFont);
    ddItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ddItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_positionsTable->setItem(row, COL_DD_MINTED, ddItem);

    // DGB Collateral
    QTableWidgetItem* dgbItem = new QTableWidgetItem(formatDGBAmount(position.dgbCollateral));
    dgbItem->setFont(monospaceFont);
    dgbItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    dgbItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_positionsTable->setItem(row, COL_DGB_COLLATERAL, dgbItem);

    // Lock Tier
    QTableWidgetItem* tierItem = new QTableWidgetItem(QString("Tier %1").arg(position.lockTier));
    tierItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    tierItem->setTextAlignment(Qt::AlignCenter);
    m_positionsTable->setItem(row, COL_LOCK_TIER, tierItem);

    // Time Remaining
    QTableWidgetItem* timeItem = new QTableWidgetItem(formatBlockTime(position.blocksRemaining));
    timeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    timeItem->setTextAlignment(Qt::AlignCenter);
    if (position.blocksRemaining <= 0) {
        timeItem->setForeground(QBrush(QColor("#cc0000"))); // Red for expired
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
    button->setFixedSize(80, 25);
    button->setStyleSheet("QPushButton { background-color: #006600; color: white; border: none; border-radius: 3px; } "
                         "QPushButton:hover { background-color: #008800; } "
                         "QPushButton:disabled { background-color: #cccccc; }");

    connect(button, &QPushButton::clicked,
            this, &DigiDollarPositionsWidget::onRedeemPositionClicked);

    return button;
}

QWidget* DigiDollarPositionsWidget::createHealthWidget(double health) const
{
    QWidget* widget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(5, 2, 5, 2);
    layout->setSpacing(5);

    QProgressBar* healthBar = new QProgressBar(widget);
    healthBar->setRange(0, 100);
    healthBar->setValue(static_cast<int>(health));
    healthBar->setFormat("%v%");
    healthBar->setFixedHeight(18);

    // Color code the health bar
    if (health >= 80) {
        healthBar->setStyleSheet("QProgressBar::chunk { background-color: #006600; }");
    } else if (health >= 50) {
        healthBar->setStyleSheet("QProgressBar::chunk { background-color: #ff6600; }");
    } else {
        healthBar->setStyleSheet("QProgressBar::chunk { background-color: #cc0000; }");
    }

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