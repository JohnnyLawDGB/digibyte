// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarsendwidget.h>

#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/digibyteunits.h>
#include <consensus/amount.h>
#include <base58.h>

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
#include <QToolButton>
#include <QApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QPalette>

DigiDollarSendWidget::DigiDollarSendWidget(QWidget *parent) :
    QWidget(parent),
    m_mainLayout(nullptr),
    m_addressFrame(nullptr),
    m_addressLayout(nullptr),
    m_addressLabel(nullptr),
    m_addressEdit(nullptr),
    m_pasteAddressButton(nullptr),
    m_addressValidationLabel(nullptr),
    m_amountFrame(nullptr),
    m_amountLayout(nullptr),
    m_amountLabel(nullptr),
    m_amountEdit(nullptr),
    m_amountSuffix(nullptr),
    m_useAvailableBalanceButton(nullptr),
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
    m_estimatedFee(0.001)  // TODO: Implement dynamic fee estimation based on transaction size and network conditions
{
    setupUI();
    connectSignals();
    // REMOVED: applyTheme() - Let CSS handle all theming
}

DigiDollarSendWidget::~DigiDollarSendWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarSendWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);

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
    m_addressFrame->setFrameShadow(QFrame::Sunken);
    m_addressFrame->setObjectName("addressFrame");

    m_addressLayout = new QGridLayout(m_addressFrame);
    m_addressLayout->setSpacing(8);
    m_addressLayout->setContentsMargins(10, 10, 10, 10);
    m_addressLayout->setHorizontalSpacing(12);
    m_addressLayout->setVerticalSpacing(8);

    // Address label and input with paste button
    m_addressLabel = new QLabel(tr("Pay To:"), this);
    m_addressLabel->setToolTip(tr("Enter the DigiDollar address of the recipient"));
    m_addressLabel->setBuddy(m_addressEdit);

    // Create horizontal layout for address input and paste button
    QHBoxLayout* addressInputLayout = new QHBoxLayout();
    addressInputLayout->setSpacing(0);

    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setObjectName("addressEdit");
    m_addressEdit->setValidator(m_addressValidator);
    m_addressEdit->setPlaceholderText("DD1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
    m_addressEdit->setToolTip(tr("The DigiDollar address to send the payment to.\n\nValid formats:\n• DD... (Mainnet)\n• TD... (Testnet)\n• RD... (Regtest)"));
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_addressEdit->setFont(monospaceFont);

    m_pasteAddressButton = new QToolButton(this);
    m_pasteAddressButton->setText("");
    m_pasteAddressButton->setToolTip(tr("Paste address from clipboard (Alt+P)"));
    m_pasteAddressButton->setIconSize(QSize(16, 16));
    m_pasteAddressButton->setShortcut(QKeySequence("Alt+P"));
    // Note: Icon would be set by platform style in real implementation
    m_pasteAddressButton->setText("♾"); // Clipboard symbol as fallback

    addressInputLayout->addWidget(m_addressEdit);
    addressInputLayout->addWidget(m_pasteAddressButton);

    m_addressLayout->addWidget(m_addressLabel, 0, 0);
    m_addressLayout->addLayout(addressInputLayout, 0, 1);

    // Address validation label
    m_addressValidationLabel = new QLabel(this);
    m_addressValidationLabel->setObjectName("addressValidationLabel");
    m_addressValidationLabel->setText(tr("Enter a valid DigiDollar address (DD, TD, or RD prefix)"));
    m_addressValidationLabel->setWordWrap(true);
    m_addressLayout->addWidget(m_addressValidationLabel, 1, 1);

    m_mainLayout->addWidget(m_addressFrame);
}

void DigiDollarSendWidget::setupAmountSection()
{
    // Create amount frame
    m_amountFrame = new QFrame(this);
    m_amountFrame->setFrameStyle(QFrame::StyledPanel);
    m_amountFrame->setFrameShadow(QFrame::Sunken);
    m_amountFrame->setObjectName("amountFrame");

    m_amountLayout = new QGridLayout(m_amountFrame);
    m_amountLayout->setSpacing(8);
    m_amountLayout->setContentsMargins(10, 10, 10, 10);
    m_amountLayout->setHorizontalSpacing(12);
    m_amountLayout->setVerticalSpacing(8);

    // Amount input with use available balance button
    m_amountLabel = new QLabel(tr("Amount:"), this);
    m_amountLabel->setToolTip(tr("Enter the amount of DigiDollar to send"));
    m_amountLabel->setBuddy(m_amountEdit);

    // Create horizontal layout for amount input and buttons
    QHBoxLayout* amountInputLayout = new QHBoxLayout();
    amountInputLayout->setSpacing(8);

    m_amountEdit = new QLineEdit(this);
    m_amountEdit->setObjectName("amountEdit");
    m_amountEdit->setValidator(m_amountValidator);
    m_amountEdit->setPlaceholderText("0.00000000");
    m_amountEdit->setToolTip(tr("The amount to send in DigiDollar.\n\nSupported formats:\n• 0.00000001 (minimum)\n• Up to 8 decimal places\n• Maximum: 999,999,999.99999999"));
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_amountEdit->setFont(monospaceFont);

    // Add stretch to push the button to the right
    m_useAvailableBalanceButton = new QPushButton(tr("Use available balance"), this);
    m_useAvailableBalanceButton->setObjectName("useAvailableBalanceButton");
    m_useAvailableBalanceButton->setToolTip(tr("Use the full available DigiDollar balance minus transaction fee"));

    amountInputLayout->addWidget(m_amountEdit, 0);
    amountInputLayout->addWidget(m_useAvailableBalanceButton, 1);

    m_amountLayout->addWidget(m_amountLabel, 0, 0);
    m_amountLayout->addLayout(amountInputLayout, 0, 1);

    // USD equivalent display
    m_usdEquivalentLabel = new QLabel(tr("USD Equivalent:"), this);
    m_usdEquivalentLabel->setObjectName("usdEquivalentLabel");
    m_usdEquivalentLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_usdEquivalentLabel->setToolTip(tr("Equivalent value in US Dollars (DigiDollar is pegged to $1 USD)"));
    m_usdEquivalentValue = new QLabel("$0.00", this);
    m_usdEquivalentValue->setObjectName("usdEquivalentValue");
    m_usdEquivalentValue->setFont(monospaceFont);
    m_usdEquivalentValue->setToolTip(tr("USD value updates in real-time as you type"));

    m_amountLayout->addWidget(m_usdEquivalentLabel, 1, 0);
    m_amountLayout->addWidget(m_usdEquivalentValue, 1, 1);

    // Available balance display
    m_availableBalanceLabel = new QLabel(tr("Available:"), this);
    m_availableBalanceLabel->setObjectName("availableBalanceLabel");
    m_availableBalanceLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_availableBalanceLabel->setToolTip(tr("Your current available DigiDollar balance"));
    m_availableBalanceValue = new QLabel("0.00000000 DD", this);
    m_availableBalanceValue->setObjectName("availableBalanceValue");
    m_availableBalanceValue->setFont(monospaceFont);
    m_availableBalanceValue->setToolTip(tr("Your current spendable DigiDollar balance"));

    m_amountLayout->addWidget(m_availableBalanceLabel, 2, 0);
    m_amountLayout->addWidget(m_availableBalanceValue, 2, 1);

    m_mainLayout->addWidget(m_amountFrame);
}

void DigiDollarSendWidget::setupFeeSection()
{
    // Create fee frame
    m_feeFrame = new QFrame(this);
    m_feeFrame->setFrameStyle(QFrame::StyledPanel);
    m_feeFrame->setFrameShadow(QFrame::Sunken);
    m_feeFrame->setObjectName("feeFrame");

    m_feeLayout = new QGridLayout(m_feeFrame);
    m_feeLayout->setSpacing(8);
    m_feeLayout->setContentsMargins(10, 10, 10, 10);
    m_feeLayout->setHorizontalSpacing(12);
    m_feeLayout->setVerticalSpacing(8);

    // Fee display
    m_feeLabel = new QLabel(tr("Transaction fee:"), this);
    m_feeLabel->setObjectName("feeLabel");
    m_feeLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_feeLabel->setToolTip(tr("Network fee required to process this DigiDollar transaction"));
    m_feeValue = new QLabel("0.001 DD", this);
    m_feeValue->setObjectName("feeValue");
    QFont monospaceFont = GUIUtil::fixedPitchFont();
    m_feeValue->setFont(monospaceFont);
    m_feeValue->setToolTip(tr("Estimated network fee for this transaction"));

    m_feeLayout->addWidget(m_feeLabel, 0, 0);
    m_feeLayout->addWidget(m_feeValue, 0, 1);

    // Total amount display
    m_totalLabel = new QLabel(tr("Total:"), this);
    m_totalLabel->setObjectName("totalLabel");
    m_totalLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    m_totalLabel->setToolTip(tr("Total amount that will be deducted from your wallet (amount + fee)"));
    QFont boldFont = m_totalLabel->font();
    boldFont.setBold(true);
    m_totalLabel->setFont(boldFont);

    m_totalValue = new QLabel("0.000 DD", this);
    m_totalValue->setObjectName("totalValue");
    m_totalValue->setFont(monospaceFont);
    m_totalValue->setToolTip(tr("Total amount to be deducted from your available balance"));

    m_feeLayout->addWidget(m_totalLabel, 1, 0);
    m_feeLayout->addWidget(m_totalValue, 1, 1);

    m_mainLayout->addWidget(m_feeFrame);
}

void DigiDollarSendWidget::setupButtonSection()
{
    // Create button frame
    m_buttonFrame = new QFrame(this);
    m_buttonFrame->setObjectName("buttonFrame");
    m_buttonFrame->setFrameStyle(QFrame::NoFrame);

    m_buttonLayout = new QHBoxLayout(m_buttonFrame);
    m_buttonLayout->setSpacing(10);
    m_buttonLayout->setContentsMargins(10, 10, 10, 10);

    // Clear button
    m_clearButton = new QPushButton(tr("&Clear"), this);
    m_clearButton->setObjectName("clearButton");
    m_clearButton->setToolTip(tr("Clear all fields"));
    m_clearButton->setAutoDefault(false);
    m_clearButton->setMinimumHeight(32);
    m_buttonLayout->addWidget(m_clearButton);

    // Add stretch to push send button to the right
    m_buttonLayout->addStretch();

    // Send button - styled to match main wallet
    m_sendButton = new QPushButton(tr("S&end DigiDollar"), this);
    m_sendButton->setObjectName("sendButton");
    m_sendButton->setEnabled(false);
    m_sendButton->setDefault(true);
    m_sendButton->setAutoDefault(true);
    m_sendButton->setMinimumHeight(32);
    m_sendButton->setToolTip(tr("Confirm and send this DigiDollar transaction"));
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
    connect(m_useAvailableBalanceButton, &QPushButton::clicked,
            this, &DigiDollarSendWidget::onUseAvailableBalanceClicked);
    connect(m_pasteAddressButton, &QToolButton::clicked,
            this, &DigiDollarSendWidget::onPasteAddressClicked);
}

void DigiDollarSendWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        // Connect wallet model signals
        updateBalance();
        // REMOVED: applyTheme() - Let CSS handle all theming
    }
}

void DigiDollarSendWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // Connect client model signals
        updateOraclePrice();
        // REMOVED: applyTheme() - Let CSS handle all theming
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
    if (m_walletModel) {
        // Get actual DigiDollar balance from wallet (in cents)
        CAmount balanceCents = m_walletModel->getDigiDollarBalance();
        m_availableBalance = balanceCents / 100.0; // Convert cents to DD
    } else {
        m_availableBalance = 0.0;
    }

    m_availableBalanceValue->setText(formatDDAmount(m_availableBalance));

    // Update button state
    m_useAvailableBalanceButton->setEnabled(m_availableBalance > m_estimatedFee);
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

    updateAddressValidation();

    updateSendButton();
}

void DigiDollarSendWidget::onAmountChanged()
{
    QString amountText = m_amountEdit->text();

    // Validate amount format first
    if (!amountText.isEmpty()) {
        updateAmountValidation();
    } else {
        updateAmountValidation();
    }

    updateUSDEquivalent();
    updateFeeDisplay();
    updateSendButton();
}

void DigiDollarSendWidget::onSendClicked()
{
    if (!validateAddress() || !validateAmount() || !validateBalance()) {
        // Show specific error message
        QString error;
        if (!validateAddress()) {
            error = tr("Please enter a valid DigiDollar address.");
        } else if (!validateAmount()) {
            error = tr("Please enter a valid amount (0.00000001 to 999,999,999.99999999 DD).");
        } else if (!validateBalance()) {
            error = tr("Insufficient balance. Total needed: %1 DD (including %2 DD fee).")
                   .arg(formatDDAmount(m_amountEdit->text().toDouble() + m_estimatedFee))
                   .arg(formatDDAmount(m_estimatedFee));
        }

        Q_EMIT message(tr("Invalid Input"), error, QMessageBox::Warning);
        return;
    }

    QString address = m_addressEdit->text();
    QString amountText = m_amountEdit->text();
    double amount = amountText.toDouble();
    double total = amount + m_estimatedFee;

    // In a real implementation, this would create and broadcast the transaction
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm DigiDollar Transaction"));
    msgBox.setText(tr("<b>Send %1 to:</b><br/>%2")
                  .arg(formatDDAmount(amount))
                  .arg(address));
    msgBox.setInformativeText(tr("<table>"
                               "<tr><td>Amount:</td><td align='right'>%1</td></tr>"
                               "<tr><td>Network fee:</td><td align='right'>%2</td></tr>"
                               "<tr><td><b>Total:</b></td><td align='right'><b>%3</b></td></tr>"
                               "<tr><td>USD Equivalent:</td><td align='right'>$%4</td></tr>"
                               "</table>")
                             .arg(formatDDAmount(amount))
                             .arg(formatDDAmount(m_estimatedFee))
                             .arg(formatDDAmount(total))
                             .arg(QString::number(amount, 'f', 2)));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Question);

    QAbstractButton* yesButton = msgBox.button(QMessageBox::Yes);
    yesButton->setText(tr("&Send DigiDollar"));
    QAbstractButton* noButton = msgBox.button(QMessageBox::No);
    noButton->setText(tr("&Cancel"));

    if (msgBox.exec() == QMessageBox::Yes) {
        if (!m_walletModel) {
            Q_EMIT message(tr("Error"), tr("No wallet model available"), QMessageBox::Critical);
            return;
        }

        // Convert amount from double to CAmount (cents)
        CAmount amountCents = static_cast<CAmount>(amount * 100);

        // Call the wallet model to send DigiDollar
        WalletModel::DigiDollarSendResult result = m_walletModel->sendDigiDollar(address, amountCents, "");

        if (result.status == WalletModel::OK) {
            Q_EMIT message(tr("Transaction Sent"),
                        tr("DigiDollar transaction sent successfully!\n\nTransaction ID: %1")
                        .arg(result.txid),
                        QMessageBox::Information);
            onClearClicked();
            updateBalance(); // Refresh balance display
        } else {
            QString errorTitle;
            QString errorMessage = result.reasonFailed;

            switch (result.status) {
            case WalletModel::InvalidAddress:
                errorTitle = tr("Invalid Address");
                break;
            case WalletModel::InvalidAmount:
                errorTitle = tr("Invalid Amount");
                break;
            case WalletModel::AmountExceedsBalance:
                errorTitle = tr("Insufficient Balance");
                break;
            case WalletModel::TransactionCreationFailed:
                errorTitle = tr("Transaction Failed");
                break;
            default:
                errorTitle = tr("Send Error");
                break;
            }

            Q_EMIT message(errorTitle, errorMessage, QMessageBox::Critical);
        }
    }
}

void DigiDollarSendWidget::onClearClicked()
{
    m_addressEdit->clear();
    m_amountEdit->clear();
    onAddressChanged(); // Reset validation
    onAmountChanged();  // Reset amounts
}

void DigiDollarSendWidget::onUseAvailableBalanceClicked()
{
    if (m_availableBalance > m_estimatedFee) {
        double maxSendable = m_availableBalance - m_estimatedFee;
        m_amountEdit->setText(QString::number(maxSendable, 'f', 8));
        onAmountChanged();
    }
}

void DigiDollarSendWidget::onPasteAddressClicked()
{
    m_addressEdit->setText(QApplication::clipboard()->text());
    onAddressChanged();
}

// REMOVED: applyTheme() method
// All theming is now handled by light.css and dark.css files
// This allows the DigiByte blue theme to work properly

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
    // REMOVED: All programmatic styling - Let CSS handle theming
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
    if (amountText.isEmpty()) return true; // Empty is valid for enabling/disabling

    double amount = amountText.toDouble();
    double total = amount + m_estimatedFee;
    bool valid = total <= m_availableBalance && amount > 0;

    // Note: Cannot call updateAmountValidation() from const method

    return valid;
}

QString DigiDollarSendWidget::formatDDAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DD";
}

QString DigiDollarSendWidget::formatUSDAmount(double amount) const
{
    return "$" + QString::number(amount, 'f', 2);
}

void DigiDollarSendWidget::updateAddressValidation()
{
    QString address = m_addressEdit->text();
    QPalette palette = QApplication::palette();
    QString midColor = palette.color(QPalette::Mid).name();
    int lightness = palette.color(QPalette::WindowText).lightness();
    bool isDarkTheme = lightness > 127;

    QString successColor = isDarkTheme ? "#4caf50" : "#28a745";
    QString errorColor = isDarkTheme ? "#f44336" : "#dc3545";

    if (address.isEmpty()) {
        m_addressValidationLabel->setText(tr("Enter a valid DigiDollar address (DD, TD, or RD prefix)"));
        m_addressValidationLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; }").arg(midColor));
        m_addressEdit->setStyleSheet("");
    } else if (validateAddress()) {
        m_addressValidationLabel->setText(tr("✓ Valid DigiDollar address"));
        m_addressValidationLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; font-weight: bold; }").arg(successColor));
        m_addressEdit->setStyleSheet(QString("QLineEdit { border: 2px solid %1; }").arg(successColor));
    } else {
        m_addressValidationLabel->setText(tr("✗ Invalid address format - must start with DD, TD, or RD"));
        m_addressValidationLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; font-weight: bold; }").arg(errorColor));
        m_addressEdit->setStyleSheet(QString("QLineEdit { border: 2px solid %1; }").arg(errorColor));
    }
}

void DigiDollarSendWidget::updateAmountValidation()
{
    QString amountText = m_amountEdit->text();
    QPalette palette = QApplication::palette();
    int lightness = palette.color(QPalette::WindowText).lightness();
    bool isDarkTheme = lightness > 127;

    QString successColor = isDarkTheme ? "#4caf50" : "#28a745";
    QString warningColor = isDarkTheme ? "#ff9800" : "#ffc107";
    QString errorColor = isDarkTheme ? "#f44336" : "#dc3545";

    if (!amountText.isEmpty()) {
        bool isValid = validateAmount();
        bool hasBalance = validateBalance();

        if (!isValid) {
            // Invalid format
            m_amountEdit->setStyleSheet(QString("QLineEdit { border: 2px solid %1; }").arg(errorColor));
        } else if (!hasBalance) {
            // Valid format but insufficient balance
            m_amountEdit->setStyleSheet(QString("QLineEdit { border: 2px solid %1; }").arg(warningColor));
        } else {
            // Valid and sufficient balance
            m_amountEdit->setStyleSheet(QString("QLineEdit { border: 2px solid %1; }").arg(successColor));
        }
    } else {
        m_amountEdit->setStyleSheet("");
    }
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
    // Use the proper CDigiDollarAddress validation function
    // This ensures full validation including checksum verification
    return CDigiDollarAddress::IsValidDigiDollarAddress(address.toStdString());
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