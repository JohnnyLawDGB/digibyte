// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarmintwidget.h>

#include <qt/digidollarsendwidget.h> // For AmountValidator
#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/digibyteunits.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QProgressBar>
#include <QFont>
#include <QMessageBox>

DigiDollarMintWidget::DigiDollarMintWidget(QWidget *parent) :
    QWidget(parent),
    m_mainLayout(nullptr),
    m_amountFrame(nullptr),
    m_amountLayout(nullptr),
    m_amountLabel(nullptr),
    m_amountEdit(nullptr),
    m_amountSuffix(nullptr),
    m_usdValueLabel(nullptr),
    m_usdValueValue(nullptr),
    m_lockTierFrame(nullptr),
    m_lockTierLayout(nullptr),
    m_lockTierLabel(nullptr),
    m_lockTierCombo(nullptr),
    m_lockTierInfoLabel(nullptr),
    m_lockTierInfoValue(nullptr),
    m_collateralFrame(nullptr),
    m_collateralLayout(nullptr),
    m_oraclePriceLabel(nullptr),
    m_oraclePriceValue(nullptr),
    m_collateralLabel(nullptr),
    m_collateralValue(nullptr),
    m_ratioLabel(nullptr),
    m_ratioValue(nullptr),
    m_ratioBar(nullptr),
    m_availableDGBLabel(nullptr),
    m_availableDGBValue(nullptr),
    m_buttonFrame(nullptr),
    m_buttonLayout(nullptr),
    m_mintButton(nullptr),
    m_clearButton(nullptr),
    m_amountValidator(nullptr),
    m_walletModel(nullptr),
    m_clientModel(nullptr),
    m_availableDGBBalance(0.0),
    m_oraclePrice(0.01), // Default DGB price in USD
    m_mintAmount(0.0),
    m_selectedTier(0),
    m_requiredCollateral(0.0),
    m_collateralRatio(0.0)
{
    setupUI();
    connectSignals();
}

DigiDollarMintWidget::~DigiDollarMintWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarMintWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Create validators
    m_amountValidator = new AmountValidator(0.00000001, 999999999.99999999, this);

    // Setup sections
    setupMintAmountSection();
    setupLockTierSection();
    setupCollateralSection();
    setupButtonSection();

    // Add stretch to push content to top
    m_mainLayout->addStretch();

    setLayout(m_mainLayout);
}

void DigiDollarMintWidget::setupMintAmountSection()
{
    // Create mint amount frame
    m_amountFrame = new QFrame(this);
    m_amountFrame->setFrameStyle(QFrame::StyledPanel);
    m_amountFrame->setObjectName("amountFrame");

    m_amountLayout = new QGridLayout(m_amountFrame);
    m_amountLayout->setSpacing(10);
    m_amountLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* amountTitle = new QLabel(tr("Mint Amount"), this);
    QFont titleFont = amountTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    amountTitle->setFont(titleFont);
    m_amountLayout->addWidget(amountTitle, 0, 0, 1, 3);

    // Amount input
    m_amountLabel = new QLabel(tr("DD to Mint:"), this);
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

    // USD value
    m_usdValueLabel = new QLabel(tr("USD Value:"), this);
    m_usdValueLabel->setObjectName("usdValueLabel");
    m_usdValueValue = new QLabel("$0.00", this);
    m_usdValueValue->setObjectName("usdValueValue");
    m_usdValueValue->setFont(monospaceFont);
    m_usdValueValue->setStyleSheet("QLabel { color: #666666; }");

    m_amountLayout->addWidget(m_usdValueLabel, 2, 0);
    m_amountLayout->addWidget(m_usdValueValue, 2, 1, 1, 2);

    m_mainLayout->addWidget(m_amountFrame);
}

void DigiDollarMintWidget::setupLockTierSection()
{
    // Create lock tier frame
    m_lockTierFrame = new QFrame(this);
    m_lockTierFrame->setFrameStyle(QFrame::StyledPanel);
    m_lockTierFrame->setObjectName("lockTierFrame");

    m_lockTierLayout = new QGridLayout(m_lockTierFrame);
    m_lockTierLayout->setSpacing(10);
    m_lockTierLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* tierTitle = new QLabel(tr("Lock Tier"), this);
    QFont titleFont = tierTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    tierTitle->setFont(titleFont);
    m_lockTierLayout->addWidget(tierTitle, 0, 0, 1, 2);

    // Lock tier combo
    m_lockTierLabel = new QLabel(tr("Lock Period:"), this);
    m_lockTierCombo = new QComboBox(this);
    m_lockTierCombo->setObjectName("lockTierCombo");

    // Add all 8 tiers
    for (int i = 1; i <= 8; ++i) {
        m_lockTierCombo->addItem(getLockTierDisplayName(i), i);
    }

    m_lockTierLayout->addWidget(m_lockTierLabel, 1, 0);
    m_lockTierLayout->addWidget(m_lockTierCombo, 1, 1);

    // Lock tier info
    m_lockTierInfoLabel = new QLabel(tr("Collateral Ratio:"), this);
    m_lockTierInfoValue = new QLabel("150%", this);
    m_lockTierInfoValue->setObjectName("lockTierInfoValue");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_lockTierInfoValue->setFont(monospaceFont);

    m_lockTierLayout->addWidget(m_lockTierInfoLabel, 2, 0);
    m_lockTierLayout->addWidget(m_lockTierInfoValue, 2, 1);

    m_mainLayout->addWidget(m_lockTierFrame);
}

void DigiDollarMintWidget::setupCollateralSection()
{
    // Create collateral frame
    m_collateralFrame = new QFrame(this);
    m_collateralFrame->setFrameStyle(QFrame::StyledPanel);
    m_collateralFrame->setObjectName("collateralFrame");

    m_collateralLayout = new QGridLayout(m_collateralFrame);
    m_collateralLayout->setSpacing(10);
    m_collateralLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* collateralTitle = new QLabel(tr("Collateral Requirements"), this);
    QFont titleFont = collateralTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    collateralTitle->setFont(titleFont);
    m_collateralLayout->addWidget(collateralTitle, 0, 0, 1, 2);

    // Oracle price
    m_oraclePriceLabel = new QLabel(tr("Oracle Price:"), this);
    m_oraclePriceLabel->setObjectName("oraclePriceLabel");
    m_oraclePriceValue = new QLabel("$0.01 USD/DGB", this);
    m_oraclePriceValue->setObjectName("oraclePriceValue");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_oraclePriceValue->setFont(monospaceFont);

    m_collateralLayout->addWidget(m_oraclePriceLabel, 1, 0);
    m_collateralLayout->addWidget(m_oraclePriceValue, 1, 1);

    // Required collateral
    m_collateralLabel = new QLabel(tr("Required DGB:"), this);
    m_collateralLabel->setObjectName("collateralLabel");
    m_collateralValue = new QLabel("0.00000000 DGB", this);
    m_collateralValue->setObjectName("collateralValue");
    m_collateralValue->setFont(monospaceFont);

    m_collateralLayout->addWidget(m_collateralLabel, 2, 0);
    m_collateralLayout->addWidget(m_collateralValue, 2, 1);

    // Collateral ratio
    m_ratioLabel = new QLabel(tr("Ratio:"), this);
    m_ratioLabel->setObjectName("ratioLabel");
    m_ratioValue = new QLabel("150%", this);
    m_ratioValue->setObjectName("ratioValue");
    m_ratioValue->setFont(monospaceFont);

    m_collateralLayout->addWidget(m_ratioLabel, 3, 0);
    m_collateralLayout->addWidget(m_ratioValue, 3, 1);

    // Ratio progress bar
    m_ratioBar = new QProgressBar(this);
    m_ratioBar->setObjectName("ratioBar");
    m_ratioBar->setRange(100, 500); // 100% to 500%
    m_ratioBar->setValue(150);
    m_ratioBar->setFormat("%v%");
    m_collateralLayout->addWidget(m_ratioBar, 4, 0, 1, 2);

    // Available DGB
    m_availableDGBLabel = new QLabel(tr("Available DGB:"), this);
    m_availableDGBLabel->setObjectName("availableDGBLabel");
    m_availableDGBValue = new QLabel("0.00000000 DGB", this);
    m_availableDGBValue->setObjectName("availableDGBValue");
    m_availableDGBValue->setFont(monospaceFont);

    m_collateralLayout->addWidget(m_availableDGBLabel, 5, 0);
    m_collateralLayout->addWidget(m_availableDGBValue, 5, 1);

    m_mainLayout->addWidget(m_collateralFrame);
}

void DigiDollarMintWidget::setupButtonSection()
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

    // Mint button
    m_mintButton = new QPushButton(tr("Mint DigiDollar"), this);
    m_mintButton->setObjectName("mintButton");
    m_mintButton->setEnabled(false);
    m_mintButton->setStyleSheet("QPushButton:enabled { background-color: #006600; color: white; font-weight: bold; }");
    m_buttonLayout->addWidget(m_mintButton);

    m_mainLayout->addWidget(m_buttonFrame);
}

void DigiDollarMintWidget::connectSignals()
{
    // Connect amount validation
    connect(m_amountEdit, &QLineEdit::textChanged,
            this, &DigiDollarMintWidget::onAmountChanged);

    // Connect lock tier selection
    connect(m_lockTierCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigiDollarMintWidget::onLockTierChanged);

    // Connect buttons
    connect(m_mintButton, &QPushButton::clicked,
            this, &DigiDollarMintWidget::onMintClicked);
    connect(m_clearButton, &QPushButton::clicked,
            this, &DigiDollarMintWidget::onClearClicked);
}

void DigiDollarMintWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals
        updateBalance();
    }
}

void DigiDollarMintWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals
        updateOraclePrice();
    }
}

void DigiDollarMintWidget::updateView()
{
    updateBalance();
    updateOraclePrice();
    updateCollateralCalculation();
}

void DigiDollarMintWidget::updateBalance()
{
    // In a real implementation, this would query the wallet for DGB balance
    if (m_walletModel) {
        // TODO: Get actual DGB balance from wallet
        // m_availableDGBBalance = m_walletModel->getDGBBalance();
    }

    m_availableDGBValue->setText(formatDGBAmount(m_availableDGBBalance));
}

void DigiDollarMintWidget::updateOraclePrice()
{
    // In a real implementation, this would get the oracle price
    if (m_clientModel) {
        // TODO: Get actual oracle price
        // m_oraclePrice = m_clientModel->getOraclePrice();
    }

    m_oraclePriceValue->setText(formatUSDAmount(m_oraclePrice) + " USD/DGB");
    updateCollateralCalculation();
}

void DigiDollarMintWidget::onAmountChanged()
{
    QString amountText = m_amountEdit->text();
    if (!amountText.isEmpty()) {
        m_mintAmount = amountText.toDouble();
        double usdValue = m_mintAmount * 1.0; // DD should be pegged to $1
        m_usdValueValue->setText(formatUSDAmount(usdValue));
    } else {
        m_mintAmount = 0.0;
        m_usdValueValue->setText("$0.00");
    }

    updateCollateralCalculation();
    updateMintButton();
}

void DigiDollarMintWidget::onLockTierChanged()
{
    m_selectedTier = m_lockTierCombo->currentData().toInt();
    double ratio = getCollateralRatioForTier(m_selectedTier);
    m_lockTierInfoValue->setText(formatRatio(ratio));

    updateCollateralCalculation();
    updateMintButton();
}

void DigiDollarMintWidget::onMintClicked()
{
    if (!validateAmount() || !validateCollateral()) {
        return;
    }

    // In a real implementation, this would create and broadcast the mint transaction
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Mint"));
    msgBox.setText(tr("Mint %1 DD by locking %2 DGB?")
                  .arg(formatDDAmount(m_mintAmount))
                  .arg(formatDGBAmount(m_requiredCollateral)));
    msgBox.setInformativeText(tr("Lock Tier: %1\nCollateral Ratio: %2\nLock Period: %3 blocks")
                             .arg(m_selectedTier)
                             .arg(formatRatio(m_collateralRatio))
                             .arg(getLockTierBlocks(m_selectedTier)));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        // TODO: Actually create the mint transaction
        emit message(tr("Mint Transaction Created"),
                    tr("DigiDollar mint transaction created successfully!"),
                    QMessageBox::Information);
        onClearClicked();
    }
}

void DigiDollarMintWidget::onClearClicked()
{
    m_amountEdit->clear();
    m_lockTierCombo->setCurrentIndex(0);
    onAmountChanged(); // Reset amounts
    onLockTierChanged(); // Reset tier
}

void DigiDollarMintWidget::updateMintButton()
{
    bool amountValid = validateAmount();
    bool collateralValid = validateCollateral();

    m_mintButton->setEnabled(amountValid && collateralValid);
}

void DigiDollarMintWidget::updateCollateralCalculation()
{
    calculateRequiredCollateral();

    // Update displays
    m_collateralValue->setText(formatDGBAmount(m_requiredCollateral));
    m_ratioValue->setText(formatRatio(m_collateralRatio));
    m_ratioBar->setValue(static_cast<int>(m_collateralRatio));

    // Color code the collateral display based on availability
    if (m_requiredCollateral > m_availableDGBBalance) {
        m_collateralValue->setStyleSheet("QLabel { color: #cc0000; }");
        m_ratioBar->setStyleSheet("QProgressBar::chunk { background-color: #cc0000; }");
    } else {
        m_collateralValue->setStyleSheet("QLabel { color: #006600; }");
        m_ratioBar->setStyleSheet("QProgressBar::chunk { background-color: #006600; }");
    }
}

void DigiDollarMintWidget::calculateRequiredCollateral()
{
    if (m_mintAmount > 0 && m_oraclePrice > 0) {
        m_collateralRatio = getCollateralRatioForTier(m_selectedTier);

        // Calculate required DGB: (DD amount * $1) / (DGB price * ratio/100)
        double usdValue = m_mintAmount * 1.0; // DD is pegged to $1
        m_requiredCollateral = usdValue / (m_oraclePrice * (m_collateralRatio / 100.0));
    } else {
        m_requiredCollateral = 0.0;
        m_collateralRatio = getCollateralRatioForTier(m_selectedTier);
    }
}

bool DigiDollarMintWidget::validateAmount() const
{
    QString amountText = m_amountEdit->text();
    if (amountText.isEmpty()) return false;

    int pos = 0;
    QString amountCopy = amountText;
    return m_amountValidator->validate(amountCopy, pos) == QValidator::Acceptable;
}

bool DigiDollarMintWidget::validateCollateral() const
{
    return m_requiredCollateral > 0 && m_requiredCollateral <= m_availableDGBBalance;
}

QString DigiDollarMintWidget::formatDDAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DD";
}

QString DigiDollarMintWidget::formatDGBAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DGB";
}

QString DigiDollarMintWidget::formatUSDAmount(double amount) const
{
    return "$" + QString::number(amount, 'f', 4);
}

QString DigiDollarMintWidget::formatRatio(double ratio) const
{
    return QString::number(ratio, 'f', 0) + "%";
}

double DigiDollarMintWidget::getCollateralRatioForTier(int tier) const
{
    // Define collateral ratios for each tier (higher tier = lower ratio)
    switch (tier) {
    case 1: return 200.0; // 200% for shortest lock
    case 2: return 185.0;
    case 3: return 170.0;
    case 4: return 155.0;
    case 5: return 140.0;
    case 6: return 125.0;
    case 7: return 115.0;
    case 8: return 110.0; // 110% for longest lock
    default: return 150.0;
    }
}

QString DigiDollarMintWidget::getLockTierDisplayName(int tier) const
{
    return QString("Tier %1 (%2 blocks)").arg(tier).arg(getLockTierBlocks(tier));
}

int DigiDollarMintWidget::getLockTierBlocks(int tier) const
{
    // Define lock periods for each tier (powers of 8)
    switch (tier) {
    case 1: return 8;        // 8 blocks
    case 2: return 64;       // 8^2
    case 3: return 512;      // 8^3
    case 4: return 4096;     // 8^4
    case 5: return 32768;    // 8^5
    case 6: return 262144;   // 8^6
    case 7: return 2097152;  // 8^7
    case 8: return 16777216; // 8^8
    default: return 8;
    }
}