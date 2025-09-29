// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarredeemwidget.h>

#include <qt/digidollarsendwidget.h> // For AmountValidator
#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/digibyteunits.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QProgressBar>
#include <QFont>
#include <QMessageBox>
#include <QRegularExpression>

DigiDollarRedeemWidget::DigiDollarRedeemWidget(QWidget *parent) :
    QWidget(parent),
    m_mainLayout(nullptr),
    m_positionFrame(nullptr),
    m_positionLayout(nullptr),
    m_positionIdLabel(nullptr),
    m_positionIdEdit(nullptr),
    m_positionValidationLabel(nullptr),
    m_amountFrame(nullptr),
    m_amountLayout(nullptr),
    m_amountLabel(nullptr),
    m_amountEdit(nullptr),
    m_amountSuffix(nullptr),
    m_redeemableLabel(nullptr),
    m_redeemableValue(nullptr),
    m_positionInfoFrame(nullptr),
    m_positionInfoLayout(nullptr),
    m_positionInfoLabel(nullptr),
    m_ddMintedLabel(nullptr),
    m_ddMintedValue(nullptr),
    m_dgbCollateralLabel(nullptr),
    m_dgbCollateralValue(nullptr),
    m_lockTierLabel(nullptr),
    m_lockTierValue(nullptr),
    m_timeRemainingLabel(nullptr),
    m_timeRemainingValue(nullptr),
    m_healthStatusLabel(nullptr),
    m_healthStatusValue(nullptr),
    m_healthBar(nullptr),
    m_buttonFrame(nullptr),
    m_buttonLayout(nullptr),
    m_redeemButton(nullptr),
    m_redeemAllButton(nullptr),
    m_clearButton(nullptr),
    m_amountValidator(nullptr),
    m_walletModel(nullptr),
    m_clientModel(nullptr),
    m_selectedPositionId(""),
    m_positionDDMinted(0.0),
    m_positionDGBCollateral(0.0),
    m_positionLockTier(0),
    m_positionBlocksRemaining(0),
    m_positionHealth(100.0),
    m_redeemableAmount(0.0),
    m_positionFound(false)
{
    setupUI();
    connectSignals();
}

DigiDollarRedeemWidget::~DigiDollarRedeemWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarRedeemWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Create validators
    m_amountValidator = new AmountValidator(0.00000001, 999999999.99999999, this);

    // Setup sections
    setupPositionSection();
    setupAmountSection();
    setupPositionInfoSection();
    setupButtonSection();

    // Add stretch to push content to top
    m_mainLayout->addStretch();

    setLayout(m_mainLayout);
}

void DigiDollarRedeemWidget::setupPositionSection()
{
    // Create position frame
    m_positionFrame = new QFrame(this);
    m_positionFrame->setFrameStyle(QFrame::StyledPanel);
    m_positionFrame->setObjectName("positionFrame");

    m_positionLayout = new QGridLayout(m_positionFrame);
    m_positionLayout->setSpacing(10);
    m_positionLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* positionTitle = new QLabel(tr("Select Position"), this);
    QFont titleFont = positionTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    positionTitle->setFont(titleFont);
    m_positionLayout->addWidget(positionTitle, 0, 0, 1, 2);

    // Position ID input
    m_positionIdLabel = new QLabel(tr("Position ID:"), this);
    m_positionIdEdit = new QLineEdit(this);
    m_positionIdEdit->setObjectName("positionIdEdit");
    m_positionIdEdit->setPlaceholderText("Enter position ID");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_positionIdEdit->setFont(monospaceFont);

    m_positionLayout->addWidget(m_positionIdLabel, 1, 0);
    m_positionLayout->addWidget(m_positionIdEdit, 1, 1);

    // Position validation label
    m_positionValidationLabel = new QLabel(this);
    m_positionValidationLabel->setObjectName("positionValidationLabel");
    m_positionValidationLabel->setStyleSheet("QLabel { color: #666666; }");
    m_positionValidationLabel->setText(tr("Enter a position ID to load details"));
    m_positionLayout->addWidget(m_positionValidationLabel, 2, 0, 1, 2);

    m_mainLayout->addWidget(m_positionFrame);
}

void DigiDollarRedeemWidget::setupAmountSection()
{
    // Create amount frame
    m_amountFrame = new QFrame(this);
    m_amountFrame->setFrameStyle(QFrame::StyledPanel);
    m_amountFrame->setObjectName("amountFrame");

    m_amountLayout = new QGridLayout(m_amountFrame);
    m_amountLayout->setSpacing(10);
    m_amountLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* amountTitle = new QLabel(tr("Redeem Amount"), this);
    QFont titleFont = amountTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    amountTitle->setFont(titleFont);
    m_amountLayout->addWidget(amountTitle, 0, 0, 1, 3);

    // Amount input
    m_amountLabel = new QLabel(tr("DD to Redeem:"), this);
    m_amountEdit = new QLineEdit(this);
    m_amountEdit->setObjectName("amountEdit");
    m_amountEdit->setValidator(m_amountValidator);
    m_amountEdit->setPlaceholderText("0.00000000");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_amountEdit->setFont(monospaceFont);

    m_amountSuffix = new QLabel("DD", this);
    m_amountSuffix->setObjectName("amountSuffix");

    m_amountLayout->addWidget(m_amountLabel, 1, 0);
    m_amountLayout->addWidget(m_amountEdit, 1, 1);
    m_amountLayout->addWidget(m_amountSuffix, 1, 2);

    // Redeemable amount
    m_redeemableLabel = new QLabel(tr("Max Redeemable:"), this);
    m_redeemableLabel->setObjectName("redeemableLabel");
    m_redeemableValue = new QLabel("0.00000000 DD", this);
    m_redeemableValue->setObjectName("redeemableValue");
    m_redeemableValue->setFont(monospaceFont);

    m_amountLayout->addWidget(m_redeemableLabel, 2, 0);
    m_amountLayout->addWidget(m_redeemableValue, 2, 1, 1, 2);

    m_mainLayout->addWidget(m_amountFrame);
}

void DigiDollarRedeemWidget::setupPositionInfoSection()
{
    // Create position info frame
    m_positionInfoFrame = new QFrame(this);
    m_positionInfoFrame->setFrameStyle(QFrame::StyledPanel);
    m_positionInfoFrame->setObjectName("positionInfoFrame");

    m_positionInfoLayout = new QGridLayout(m_positionInfoFrame);
    m_positionInfoLayout->setSpacing(10);
    m_positionInfoLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    m_positionInfoLabel = new QLabel(tr("Position Details"), this);
    QFont titleFont = m_positionInfoLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_positionInfoLabel->setFont(titleFont);
    m_positionInfoLayout->addWidget(m_positionInfoLabel, 0, 0, 1, 2);

    QFont monospaceFont = GUIUtil::fixedPitchFont();

    // DD Minted
    m_ddMintedLabel = new QLabel(tr("DD Minted:"), this);
    m_ddMintedValue = new QLabel("0.00000000 DD", this);
    m_ddMintedValue->setFont(monospaceFont);
    m_positionInfoLayout->addWidget(m_ddMintedLabel, 1, 0);
    m_positionInfoLayout->addWidget(m_ddMintedValue, 1, 1);

    // DGB Collateral
    m_dgbCollateralLabel = new QLabel(tr("DGB Collateral:"), this);
    m_dgbCollateralValue = new QLabel("0.00000000 DGB", this);
    m_dgbCollateralValue->setFont(monospaceFont);
    m_positionInfoLayout->addWidget(m_dgbCollateralLabel, 2, 0);
    m_positionInfoLayout->addWidget(m_dgbCollateralValue, 2, 1);

    // Lock Tier
    m_lockTierLabel = new QLabel(tr("Lock Tier:"), this);
    m_lockTierValue = new QLabel("N/A", this);
    m_lockTierValue->setFont(monospaceFont);
    m_positionInfoLayout->addWidget(m_lockTierLabel, 3, 0);
    m_positionInfoLayout->addWidget(m_lockTierValue, 3, 1);

    // Time Remaining
    m_timeRemainingLabel = new QLabel(tr("Time Remaining:"), this);
    m_timeRemainingValue = new QLabel("N/A", this);
    m_timeRemainingValue->setFont(monospaceFont);
    m_positionInfoLayout->addWidget(m_timeRemainingLabel, 4, 0);
    m_positionInfoLayout->addWidget(m_timeRemainingValue, 4, 1);

    // Health Status
    m_healthStatusLabel = new QLabel(tr("Health:"), this);
    m_healthStatusValue = new QLabel("N/A", this);
    m_healthStatusValue->setFont(monospaceFont);
    m_positionInfoLayout->addWidget(m_healthStatusLabel, 5, 0);
    m_positionInfoLayout->addWidget(m_healthStatusValue, 5, 1);

    // Health progress bar
    m_healthBar = new QProgressBar(this);
    m_healthBar->setObjectName("healthBar");
    m_healthBar->setRange(0, 100);
    m_healthBar->setValue(0);
    m_healthBar->setFormat("%v%");
    m_positionInfoLayout->addWidget(m_healthBar, 6, 0, 1, 2);

    m_mainLayout->addWidget(m_positionInfoFrame);
}

void DigiDollarRedeemWidget::setupButtonSection()
{
    // Create button frame
    m_buttonFrame = new QFrame(this);
    m_buttonFrame->setObjectName("buttonFrame");

    m_buttonLayout = new QHBoxLayout(m_buttonFrame);
    m_buttonLayout->setSpacing(10);
    m_buttonLayout->setContentsMargins(15, 15, 15, 15);

    // Clear button
    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setObjectName("clearButton");
    m_buttonLayout->addWidget(m_clearButton);

    // Add stretch
    m_buttonLayout->addStretch();

    // Redeem all button
    m_redeemAllButton = new QPushButton(tr("Redeem All"), this);
    m_redeemAllButton->setObjectName("redeemAllButton");
    m_redeemAllButton->setEnabled(false);
    m_redeemAllButton->setStyleSheet("QPushButton:enabled { background-color: #ff6600; color: white; font-weight: bold; }");
    m_buttonLayout->addWidget(m_redeemAllButton);

    // Redeem button
    m_redeemButton = new QPushButton(tr("Redeem"), this);
    m_redeemButton->setObjectName("redeemButton");
    m_redeemButton->setEnabled(false);
    m_redeemButton->setStyleSheet("QPushButton:enabled { background-color: #006600; color: white; font-weight: bold; }");
    m_buttonLayout->addWidget(m_redeemButton);

    m_mainLayout->addWidget(m_buttonFrame);
}

void DigiDollarRedeemWidget::connectSignals()
{
    // Connect position ID validation
    connect(m_positionIdEdit, &QLineEdit::textChanged,
            this, &DigiDollarRedeemWidget::onPositionIdChanged);

    // Connect amount validation
    connect(m_amountEdit, &QLineEdit::textChanged,
            this, &DigiDollarRedeemWidget::onAmountChanged);

    // Connect buttons
    connect(m_redeemButton, &QPushButton::clicked,
            this, &DigiDollarRedeemWidget::onRedeemClicked);
    connect(m_redeemAllButton, &QPushButton::clicked,
            this, &DigiDollarRedeemWidget::onRedeemAllClicked);
    connect(m_clearButton, &QPushButton::clicked,
            this, &DigiDollarRedeemWidget::onClearClicked);
}

void DigiDollarRedeemWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals
        updateBalance();
        updatePositions();
    }
}

void DigiDollarRedeemWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals
        updatePositions();
    }
}

void DigiDollarRedeemWidget::updateView()
{
    updateBalance();
    updatePositions();
    updatePositionInfo();
}

void DigiDollarRedeemWidget::updateBalance()
{
    // In a real implementation, this would query the wallet for DigiDollar balance
    if (m_walletModel) {
        // TODO: Update balance displays if needed
    }
}

void DigiDollarRedeemWidget::updatePositions()
{
    // In a real implementation, this would refresh position data
    if (m_walletModel && !m_selectedPositionId.isEmpty()) {
        loadPositionDetails();
    }
}

void DigiDollarRedeemWidget::onPositionIdChanged()
{
    QString positionId = m_positionIdEdit->text().trimmed();
    m_selectedPositionId = positionId;

    if (positionId.isEmpty()) {
        m_positionValidationLabel->setText(tr("Enter a position ID to load details"));
        m_positionValidationLabel->setStyleSheet("QLabel { color: #666666; }");
        m_positionFound = false;
    } else if (validatePositionId()) {
        loadPositionDetails();
        if (m_positionFound) {
            m_positionValidationLabel->setText(tr("✓ Position found and loaded"));
            m_positionValidationLabel->setStyleSheet("QLabel { color: #006600; }");
        } else {
            m_positionValidationLabel->setText(tr("✗ Position not found"));
            m_positionValidationLabel->setStyleSheet("QLabel { color: #cc0000; }");
        }
    } else {
        m_positionValidationLabel->setText(tr("✗ Invalid position ID format"));
        m_positionValidationLabel->setStyleSheet("QLabel { color: #cc0000; }");
        m_positionFound = false;
    }

    updatePositionInfo();
    updateRedeemButtons();
}

void DigiDollarRedeemWidget::onAmountChanged()
{
    updateRedeemButtons();
}

void DigiDollarRedeemWidget::onRedeemClicked()
{
    if (!validatePositionId() || !validateAmount() || !validateRedeemable()) {
        return;
    }

    QString amountText = m_amountEdit->text();
    double amount = amountText.toDouble();

    // In a real implementation, this would create and broadcast the redeem transaction
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Redeem"));
    msgBox.setText(tr("Redeem %1 DD from position %2?")
                  .arg(formatDDAmount(amount))
                  .arg(m_selectedPositionId));
    msgBox.setInformativeText(tr("This will release proportional DGB collateral."));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        // TODO: Actually create the redeem transaction
        emit message(tr("Redeem Transaction Created"),
                    tr("DigiDollar redeem transaction created successfully!"),
                    QMessageBox::Information);
        onClearClicked();
    }
}

void DigiDollarRedeemWidget::onRedeemAllClicked()
{
    if (!m_positionFound || m_redeemableAmount <= 0) {
        return;
    }

    // In a real implementation, this would create and broadcast the full redeem transaction
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Redeem All"));
    msgBox.setText(tr("Redeem entire position %1?")
                  .arg(m_selectedPositionId));
    msgBox.setInformativeText(tr("Amount: %1 DD\nThis will close the position and release all collateral.")
                             .arg(formatDDAmount(m_redeemableAmount)));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        // TODO: Actually create the full redeem transaction
        emit message(tr("Position Closed"),
                    tr("DigiDollar position closed successfully!"),
                    QMessageBox::Information);
        onClearClicked();
    }
}

void DigiDollarRedeemWidget::onClearClicked()
{
    m_positionIdEdit->clear();
    m_amountEdit->clear();
    onPositionIdChanged(); // Reset position validation
    onAmountChanged();     // Reset amount validation
}

void DigiDollarRedeemWidget::updateRedeemButtons()
{
    bool positionValid = m_positionFound;
    bool amountValid = validateAmount();
    bool redeemableValid = validateRedeemable();

    m_redeemButton->setEnabled(positionValid && amountValid && redeemableValid);
    m_redeemAllButton->setEnabled(positionValid && m_redeemableAmount > 0);
}

void DigiDollarRedeemWidget::updatePositionInfo()
{
    if (m_positionFound) {
        m_ddMintedValue->setText(formatDDAmount(m_positionDDMinted));
        m_dgbCollateralValue->setText(formatDGBAmount(m_positionDGBCollateral));
        m_lockTierValue->setText(QString("Tier %1").arg(m_positionLockTier));
        m_timeRemainingValue->setText(formatBlockTime(m_positionBlocksRemaining));
        m_healthStatusValue->setText(QString("%1%").arg(QString::number(m_positionHealth, 'f', 1)));
        m_redeemableValue->setText(formatDDAmount(m_redeemableAmount));

        // Update health bar
        m_healthBar->setValue(static_cast<int>(m_positionHealth));

        // Color code health status
        if (m_positionHealth >= 80) {
            m_healthStatusValue->setStyleSheet("QLabel { color: #006600; }");
            m_healthBar->setStyleSheet("QProgressBar::chunk { background-color: #006600; }");
        } else if (m_positionHealth >= 50) {
            m_healthStatusValue->setStyleSheet("QLabel { color: #ff6600; }");
            m_healthBar->setStyleSheet("QProgressBar::chunk { background-color: #ff6600; }");
        } else {
            m_healthStatusValue->setStyleSheet("QLabel { color: #cc0000; }");
            m_healthBar->setStyleSheet("QProgressBar::chunk { background-color: #cc0000; }");
        }
    } else {
        // Reset to default values
        m_ddMintedValue->setText("0.00000000 DD");
        m_dgbCollateralValue->setText("0.00000000 DGB");
        m_lockTierValue->setText("N/A");
        m_timeRemainingValue->setText("N/A");
        m_healthStatusValue->setText("N/A");
        m_redeemableValue->setText("0.00000000 DD");
        m_healthBar->setValue(0);
    }
}

void DigiDollarRedeemWidget::loadPositionDetails()
{
    // In a real implementation, this would query the wallet/blockchain for position details
    // For now, we'll simulate some data for testing

    if (m_selectedPositionId.length() >= 8) {
        // Simulate found position with sample data
        m_positionFound = true;
        m_positionDDMinted = 100.0;
        m_positionDGBCollateral = 15000.0;
        m_positionLockTier = 3;
        m_positionBlocksRemaining = 256;
        m_positionHealth = 85.0;
        m_redeemableAmount = m_positionDDMinted; // Can redeem full amount
    } else {
        m_positionFound = false;
        m_positionDDMinted = 0.0;
        m_positionDGBCollateral = 0.0;
        m_positionLockTier = 0;
        m_positionBlocksRemaining = 0;
        m_positionHealth = 0.0;
        m_redeemableAmount = 0.0;
    }
}

bool DigiDollarRedeemWidget::validatePositionId() const
{
    QString positionId = m_positionIdEdit->text().trimmed();
    if (positionId.isEmpty()) return false;

    // Basic validation for position ID format (hexadecimal, at least 8 characters)
    QRegularExpression hexRegex("^[0-9a-fA-F]{8,}$");
    return hexRegex.match(positionId).hasMatch();
}

bool DigiDollarRedeemWidget::validateAmount() const
{
    QString amountText = m_amountEdit->text();
    if (amountText.isEmpty()) return false;

    int pos = 0;
    QString amountCopy = amountText;
    return m_amountValidator->validate(amountCopy, pos) == QValidator::Acceptable;
}

bool DigiDollarRedeemWidget::validateRedeemable() const
{
    if (!m_positionFound) return false;

    QString amountText = m_amountEdit->text();
    if (amountText.isEmpty()) return false;

    double amount = amountText.toDouble();
    return amount > 0 && amount <= m_redeemableAmount;
}

QString DigiDollarRedeemWidget::formatDDAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DD";
}

QString DigiDollarRedeemWidget::formatDGBAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DGB";
}

QString DigiDollarRedeemWidget::formatBlockTime(int blocks) const
{
    if (blocks <= 0) return "Expired";

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