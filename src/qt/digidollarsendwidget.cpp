// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarsendwidget.h>

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
#include <QValidator>
#include <QFont>
#include <QRegularExpression>
#include <QMessageBox>

DigiDollarSendWidget::DigiDollarSendWidget(QWidget *parent) :
    QWidget(parent),
    m_mainLayout(nullptr),
    m_addressFrame(nullptr),
    m_addressLayout(nullptr),
    m_addressLabel(nullptr),
    m_addressEdit(nullptr),
    m_addressValidationLabel(nullptr),
    m_amountFrame(nullptr),
    m_amountLayout(nullptr),
    m_amountLabel(nullptr),
    m_amountEdit(nullptr),
    m_amountSuffix(nullptr),
    m_usdEquivalentLabel(nullptr),
    m_usdEquivalentValue(nullptr),
    m_availableBalanceLabel(nullptr),
    m_availableBalanceValue(nullptr),
    m_feeFrame(nullptr),
    m_feeLayout(nullptr),
    m_feeLabel(nullptr),
    m_feeValue(nullptr),
    m_totalLabel(nullptr),
    m_totalValue(nullptr),
    m_buttonFrame(nullptr),
    m_buttonLayout(nullptr),
    m_sendButton(nullptr),
    m_clearButton(nullptr),
    m_addressValidator(nullptr),
    m_amountValidator(nullptr),
    m_walletModel(nullptr),
    m_clientModel(nullptr),
    m_availableBalance(0.0),
    m_oraclePrice(1.0),
    m_estimatedFee(0.001)
{
    setupUI();
    connectSignals();
}

DigiDollarSendWidget::~DigiDollarSendWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarSendWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Create validators
    m_addressValidator = new DigiDollarAddressValidator(this);
    m_amountValidator = new AmountValidator(0.00000001, 999999999.99999999, this);

    // Setup sections
    setupAddressSection();
    setupAmountSection();
    setupFeeSection();
    setupButtonSection();

    // Add stretch to push content to top
    m_mainLayout->addStretch();

    setLayout(m_mainLayout);
}

void DigiDollarSendWidget::setupAddressSection()
{
    // Create address frame
    m_addressFrame = new QFrame(this);
    m_addressFrame->setFrameStyle(QFrame::StyledPanel);
    m_addressFrame->setObjectName("addressFrame");

    m_addressLayout = new QGridLayout(m_addressFrame);
    m_addressLayout->setSpacing(10);
    m_addressLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* addressTitle = new QLabel(tr("Send To"), this);
    QFont titleFont = addressTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    addressTitle->setFont(titleFont);
    m_addressLayout->addWidget(addressTitle, 0, 0, 1, 2);

    // Address label and input
    m_addressLabel = new QLabel(tr("DigiDollar Address:"), this);
    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setObjectName("addressEdit");
    m_addressEdit->setValidator(m_addressValidator);
    m_addressEdit->setPlaceholderText("DD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_addressEdit->setFont(monospaceFont);

    m_addressLayout->addWidget(m_addressLabel, 1, 0);
    m_addressLayout->addWidget(m_addressEdit, 1, 1);

    // Address validation label
    m_addressValidationLabel = new QLabel(this);
    m_addressValidationLabel->setObjectName("addressValidationLabel");
    m_addressValidationLabel->setStyleSheet("QLabel { color: #666666; }");
    m_addressValidationLabel->setText(tr("Enter a valid DigiDollar address (DD, TD, or RD prefix)"));
    m_addressLayout->addWidget(m_addressValidationLabel, 2, 0, 1, 2);

    m_mainLayout->addWidget(m_addressFrame);
}

void DigiDollarSendWidget::setupAmountSection()
{
    // Create amount frame
    m_amountFrame = new QFrame(this);
    m_amountFrame->setFrameStyle(QFrame::StyledPanel);
    m_amountFrame->setObjectName("amountFrame");

    m_amountLayout = new QGridLayout(m_amountFrame);
    m_amountLayout->setSpacing(10);
    m_amountLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* amountTitle = new QLabel(tr("Amount"), this);
    QFont titleFont = amountTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    amountTitle->setFont(titleFont);
    m_amountLayout->addWidget(amountTitle, 0, 0, 1, 3);

    // Amount input
    m_amountLabel = new QLabel(tr("Amount:"), this);
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

    // USD equivalent
    m_usdEquivalentLabel = new QLabel(tr("USD Equivalent:"), this);
    m_usdEquivalentLabel->setObjectName("usdEquivalentLabel");
    m_usdEquivalentValue = new QLabel("$0.00", this);
    m_usdEquivalentValue->setObjectName("usdEquivalentValue");
    m_usdEquivalentValue->setFont(monospaceFont);
    m_usdEquivalentValue->setStyleSheet("QLabel { color: #666666; }");

    m_amountLayout->addWidget(m_usdEquivalentLabel, 2, 0);
    m_amountLayout->addWidget(m_usdEquivalentValue, 2, 1, 1, 2);

    // Available balance
    m_availableBalanceLabel = new QLabel(tr("Available:"), this);
    m_availableBalanceLabel->setObjectName("availableBalanceLabel");
    m_availableBalanceValue = new QLabel("0.00000000 DD", this);
    m_availableBalanceValue->setObjectName("availableBalanceValue");
    m_availableBalanceValue->setFont(monospaceFont);

    m_amountLayout->addWidget(m_availableBalanceLabel, 3, 0);
    m_amountLayout->addWidget(m_availableBalanceValue, 3, 1, 1, 2);

    m_mainLayout->addWidget(m_amountFrame);
}

void DigiDollarSendWidget::setupFeeSection()
{
    // Create fee frame
    m_feeFrame = new QFrame(this);
    m_feeFrame->setFrameStyle(QFrame::StyledPanel);
    m_feeFrame->setObjectName("feeFrame");

    m_feeLayout = new QGridLayout(m_feeFrame);
    m_feeLayout->setSpacing(10);
    m_feeLayout->setContentsMargins(15, 15, 15, 15);

    // Title
    QLabel* feeTitle = new QLabel(tr("Transaction Fee"), this);
    QFont titleFont = feeTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    feeTitle->setFont(titleFont);
    m_feeLayout->addWidget(feeTitle, 0, 0, 1, 2);

    // Fee
    m_feeLabel = new QLabel(tr("Network Fee:"), this);
    m_feeLabel->setObjectName("feeLabel");
    m_feeValue = new QLabel("0.001 DD", this);
    m_feeValue->setObjectName("feeValue");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_feeValue->setFont(monospaceFont);

    m_feeLayout->addWidget(m_feeLabel, 1, 0);
    m_feeLayout->addWidget(m_feeValue, 1, 1);

    // Total
    m_totalLabel = new QLabel(tr("Total:"), this);
    m_totalLabel->setObjectName("totalLabel");
    QFont boldFont = m_totalLabel->font();
    boldFont.setBold(true);
    m_totalLabel->setFont(boldFont);

    m_totalValue = new QLabel("0.000 DD", this);
    m_totalValue->setObjectName("totalValue");
    m_totalValue->setFont(boldFont);
    m_totalValue->setFont(monospaceFont);

    m_feeLayout->addWidget(m_totalLabel, 2, 0);
    m_feeLayout->addWidget(m_totalValue, 2, 1);

    m_mainLayout->addWidget(m_feeFrame);
}

void DigiDollarSendWidget::setupButtonSection()
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

    // Send button
    m_sendButton = new QPushButton(tr("Send DigiDollar"), this);
    m_sendButton->setObjectName("sendButton");
    m_sendButton->setEnabled(false);
    m_sendButton->setStyleSheet("QPushButton:enabled { background-color: #006600; color: white; font-weight: bold; }");
    m_buttonLayout->addWidget(m_sendButton);

    m_mainLayout->addWidget(m_buttonFrame);
}

void DigiDollarSendWidget::connectSignals()
{
    // Connect address validation
    connect(m_addressEdit, &QLineEdit::textChanged,
            this, &DigiDollarSendWidget::onAddressChanged);

    // Connect amount validation
    connect(m_amountEdit, &QLineEdit::textChanged,
            this, &DigiDollarSendWidget::onAmountChanged);

    // Connect buttons
    connect(m_sendButton, &QPushButton::clicked,
            this, &DigiDollarSendWidget::onSendClicked);
    connect(m_clearButton, &QPushButton::clicked,
            this, &DigiDollarSendWidget::onClearClicked);
}

void DigiDollarSendWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals
        updateBalance();
    }
}

void DigiDollarSendWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals
        updateOraclePrice();
    }
}

void DigiDollarSendWidget::updateView()
{
    updateBalance();
    updateOraclePrice();
    updateFeeDisplay();
}

void DigiDollarSendWidget::updateBalance()
{
    // In a real implementation, this would query the wallet for DigiDollar balance
    if (m_walletModel) {
        // TODO: Get actual DigiDollar balance from wallet
        // m_availableBalance = m_walletModel->getDDBalance();
    }

    m_availableBalanceValue->setText(formatDDAmount(m_availableBalance));
}

void DigiDollarSendWidget::updateOraclePrice()
{
    // In a real implementation, this would get the oracle price
    if (m_clientModel) {
        // TODO: Get actual oracle price
        // m_oraclePrice = m_clientModel->getOraclePrice();
    }

    updateUSDEquivalent();
}

void DigiDollarSendWidget::onAddressChanged()
{
    QString address = m_addressEdit->text();

    if (address.isEmpty()) {
        m_addressValidationLabel->setText(tr("Enter a valid DigiDollar address (DD, TD, or RD prefix)"));
        m_addressValidationLabel->setStyleSheet("QLabel { color: #666666; }");
    } else if (validateAddress()) {
        m_addressValidationLabel->setText(tr("✓ Valid DigiDollar address"));
        m_addressValidationLabel->setStyleSheet("QLabel { color: #006600; }");
    } else {
        m_addressValidationLabel->setText(tr("✗ Invalid address format"));
        m_addressValidationLabel->setStyleSheet("QLabel { color: #cc0000; }");
    }

    updateSendButton();
}

void DigiDollarSendWidget::onAmountChanged()
{
    updateUSDEquivalent();
    updateFeeDisplay();
    updateSendButton();
}

void DigiDollarSendWidget::onSendClicked()
{
    if (!validateAddress() || !validateAmount() || !validateBalance()) {
        return;
    }

    QString address = m_addressEdit->text();
    QString amountText = m_amountEdit->text();
    double amount = amountText.toDouble();

    // In a real implementation, this would create and broadcast the transaction
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Send"));
    msgBox.setText(tr("Send %1 DD to %2?").arg(formatDDAmount(amount)).arg(address));
    msgBox.setInformativeText(tr("Network fee: %1\nTotal: %2")
                             .arg(formatDDAmount(m_estimatedFee))
                             .arg(formatDDAmount(amount + m_estimatedFee)));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        // TODO: Actually send the transaction
        emit message(tr("Transaction Sent"),
                    tr("DigiDollar transaction sent successfully!"),
                    QMessageBox::Information);
        onClearClicked();
    }
}

void DigiDollarSendWidget::onClearClicked()
{
    m_addressEdit->clear();
    m_amountEdit->clear();
    onAddressChanged(); // Reset validation
    onAmountChanged();  // Reset amounts
}

void DigiDollarSendWidget::updateSendButton()
{
    bool addressValid = validateAddress();
    bool amountValid = validateAmount();
    bool balanceValid = validateBalance();

    m_sendButton->setEnabled(addressValid && amountValid && balanceValid);
}

void DigiDollarSendWidget::updateUSDEquivalent()
{
    QString amountText = m_amountEdit->text();
    if (!amountText.isEmpty()) {
        double amount = amountText.toDouble();
        double usdValue = amount * 1.0; // DD should be pegged to $1
        m_usdEquivalentValue->setText(formatUSDAmount(usdValue));
    } else {
        m_usdEquivalentValue->setText("$0.00");
    }
}

void DigiDollarSendWidget::updateFeeDisplay()
{
    m_feeValue->setText(formatDDAmount(m_estimatedFee));

    QString amountText = m_amountEdit->text();
    if (!amountText.isEmpty()) {
        double amount = amountText.toDouble();
        double total = amount + m_estimatedFee;
        m_totalValue->setText(formatDDAmount(total));
    } else {
        m_totalValue->setText(formatDDAmount(m_estimatedFee));
    }
}

bool DigiDollarSendWidget::validateAddress() const
{
    QString address = m_addressEdit->text();
    int pos = 0;
    QString addressCopy = address;
    return m_addressValidator->validate(addressCopy, pos) == QValidator::Acceptable;
}

bool DigiDollarSendWidget::validateAmount() const
{
    QString amountText = m_amountEdit->text();
    if (amountText.isEmpty()) return false;

    int pos = 0;
    QString amountCopy = amountText;
    return m_amountValidator->validate(amountCopy, pos) == QValidator::Acceptable;
}

bool DigiDollarSendWidget::validateBalance() const
{
    QString amountText = m_amountEdit->text();
    if (amountText.isEmpty()) return false;

    double amount = amountText.toDouble();
    double total = amount + m_estimatedFee;
    return total <= m_availableBalance;
}

QString DigiDollarSendWidget::formatDDAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DD";
}

QString DigiDollarSendWidget::formatUSDAmount(double amount) const
{
    return "$" + QString::number(amount, 'f', 2);
}

// DigiDollarAddressValidator implementation
DigiDollarAddressValidator::DigiDollarAddressValidator(QObject* parent) :
    QValidator(parent)
{
}

QValidator::State DigiDollarAddressValidator::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos)

    if (input.isEmpty()) {
        return QValidator::Intermediate;
    }

    if (isValidDDAddress(input)) {
        return QValidator::Acceptable;
    }

    // Check if it could become valid with more characters
    if (input.length() < 3) {
        if (input.startsWith("D") || input.startsWith("T") || input.startsWith("R")) {
            return QValidator::Intermediate;
        }
    } else if (input.length() < 42) {
        if (input.startsWith("DD") || input.startsWith("TD") || input.startsWith("RD")) {
            return QValidator::Intermediate;
        }
    }

    return QValidator::Invalid;
}

bool DigiDollarAddressValidator::isValidDDAddress(const QString& address) const
{
    // Basic validation for DigiDollar addresses
    // DD = mainnet, TD = testnet, RD = regtest
    if (address.length() < 42 || address.length() > 62) {
        return false;
    }

    if (!address.startsWith("DD") && !address.startsWith("TD") && !address.startsWith("RD")) {
        return false;
    }

    // Check if the rest contains only valid bech32 characters
    QRegularExpression bech32Regex("^[023456789acdefghjklmnpqrstuvwxyz]+$");
    QString addressBody = address.mid(2); // Skip the prefix
    return bech32Regex.match(addressBody).hasMatch();
}

// AmountValidator implementation
AmountValidator::AmountValidator(double min, double max, QObject* parent) :
    QValidator(parent), m_min(min), m_max(max)
{
}

QValidator::State AmountValidator::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos)

    if (input.isEmpty()) {
        return QValidator::Intermediate;
    }

    // Check for negative numbers
    if (input.startsWith("-")) {
        return QValidator::Invalid;
    }

    // Check for valid number format
    QRegularExpression numRegex("^\\d*\\.?\\d*$");
    if (!numRegex.match(input).hasMatch()) {
        return QValidator::Invalid;
    }

    // Check decimal places (max 8)
    int decimalPos = input.indexOf('.');
    if (decimalPos != -1) {
        if (input.length() - decimalPos - 1 > 8) {
            return QValidator::Invalid;
        }
    }

    // Convert to double and check range
    bool ok;
    double value = input.toDouble(&ok);
    if (!ok) {
        return QValidator::Intermediate;
    }

    if (value < m_min) {
        return QValidator::Invalid;
    }

    if (value > m_max) {
        return QValidator::Invalid;
    }

    return QValidator::Acceptable;
}