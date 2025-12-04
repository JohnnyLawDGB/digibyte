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
#include <logging.h>
#include <kernel/chainparams.h>
#include <oracle/mock_oracle.h>
#include <interfaces/node.h>
#include <univalue.h>

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
#include <QProgressDialog>
#include <QAbstractButton>

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
    m_addressEdit->setFocusPolicy(Qt::StrongFocus);
    m_addressEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
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
    m_amountEdit->setFocusPolicy(Qt::StrongFocus);
    m_amountEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
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
    // Get oracle price from RPC for testnet/mainnet, MockOracleManager for regtest
    if (Params().GetChainType() == ChainType::REGTEST && MockOracleManager::GetInstance().IsEnabled()) {
        // Get price from mock oracle (cents per DGB)
        CAmount priceCents = MockOracleManager::GetInstance().GetCurrentPrice();
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
            LogPrintf("DigiDollar Send: updateOraclePrice RPC error - %s\n", e.what());
            m_oraclePrice = 0.0;
        }
    } else {
        m_oraclePrice = 0.0;
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
    // PHASE 7.3: Comprehensive input validation with user-friendly messages
    QString address = m_addressEdit->text().trimmed();
    QString amountText = m_amountEdit->text().trimmed();

    // Error: Empty fields
    if (address.isEmpty()) {
        showError(tr("Missing Address"),
                  tr("Please enter a DigiDollar address.\n\nThe recipient's DD address is required to send DigiDollar."));
        m_addressEdit->setFocus();
        return;
    }

    if (amountText.isEmpty()) {
        showError(tr("Missing Amount"),
                  tr("Please enter an amount to send.\n\nSpecify how much DigiDollar you want to send (e.g., 100.00)."));
        m_amountEdit->setFocus();
        return;
    }

    // Error: Invalid address format
    if (!validateAddress()) {
        showError(tr("Invalid DigiDollar Address"),
                  tr("The address format is invalid.\n\n"
                     "Valid DigiDollar addresses:\n"
                     "• Start with DD (Mainnet)\n"
                     "• Start with TD (Testnet)\n"
                     "• Start with RD (Regtest)\n\n"
                     "Please check the address and try again."));
        m_addressEdit->setFocus();
        return;
    }

    // Error: Invalid amount format
    if (!validateAmount()) {
        showError(tr("Invalid Amount"),
                  tr("The amount is invalid.\n\n"
                     "Valid amount format:\n"
                     "• Positive number\n"
                     "• Maximum 8 decimal places\n"
                     "• Between 0.00000001 and 999,999,999 DD\n\n"
                     "Please enter a valid amount."));
        m_amountEdit->setFocus();
        return;
    }

    double amount = amountText.toDouble();
    double total = amount + m_estimatedFee;

    // Error: Insufficient balance
    if (!validateBalance()) {
        showError(tr("Insufficient DigiDollar Balance"),
                  tr("You don't have enough DigiDollar for this transfer.\n\n"
                     "Available balance: %1\n"
                     "Amount to send: %2\n"
                     "Transaction fee: %3\n"
                     "Total required: %4\n\n"
                     "Please enter a smaller amount or add more DD to your wallet.")
                  .arg(formatDDAmount(m_availableBalance))
                  .arg(formatDDAmount(amount))
                  .arg(formatDDAmount(m_estimatedFee))
                  .arg(formatDDAmount(total)));
        m_amountEdit->setFocus();
        return;
    }

    // PHASE 7.3: Wallet state validation
    if (!checkWalletState()) {
        return; // Error already displayed by checkWalletState()
    }

    // PHASE 7.2: Enhanced confirmation dialog with fee display
    if (!showConfirmationDialog(address, amount)) {
        return; // User cancelled
    }

    // PHASE 7.3: Execute transfer with progress indicator
    executeTransfer(address, amount);
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

// PHASE 7.3: Error display helper
void DigiDollarSendWidget::showError(const QString& title, const QString& message)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();

    // Log for debugging
    LogPrintf("DigiDollar GUI Error: %s - %s\n",
              title.toStdString(), message.toStdString());
}

// PHASE 7.3: Warning display helper
void DigiDollarSendWidget::showWarning(const QString& title, const QString& message)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();

    // Log for debugging
    LogPrintf("DigiDollar GUI Warning: %s - %s\n",
              title.toStdString(), message.toStdString());
}

// PHASE 7.3: Wallet state validation
bool DigiDollarSendWidget::checkWalletState()
{
    if (!m_walletModel) {
        showError(tr("Wallet Error"),
                  tr("Wallet is not available.\n\nPlease ensure your wallet is properly loaded."));
        return false;
    }

    // Check if wallet is locked
    WalletModel::EncryptionStatus encStatus = m_walletModel->getEncryptionStatus();
    if (encStatus == WalletModel::Locked) {
        // Prompt for unlock
        WalletModel::UnlockContext ctx(m_walletModel->requestUnlock());
        if (!ctx.isValid()) {
            showWarning(tr("Wallet Locked"),
                       tr("Your wallet is locked.\n\n"
                          "Please unlock your wallet to send DigiDollar.\n\n"
                          "Go to Settings > Unlock Wallet to unlock."));
            return false;
        }
    }

    // Check if DigiDollar wallet initialized (balance check serves as proxy)
    if (m_availableBalance < 0) {
        showError(tr("DigiDollar Not Available"),
                 tr("DigiDollar wallet is not initialized.\n\n"
                    "This may indicate a wallet initialization error."));
        return false;
    }

    return true;
}

// PHASE 7.2: Enhanced confirmation dialog
bool DigiDollarSendWidget::showConfirmationDialog(const QString& address, double amount)
{
    double total = amount + m_estimatedFee;
    double usdEquivalent = amount * m_oraclePrice; // DD should be pegged to $1

    // Create confirmation message with detailed breakdown
    QString confirmMsg = tr(
        "<b style='font-size: 14px;'>Confirm DigiDollar Transfer</b><br/><br/>"
        "<table cellpadding='4' style='font-size: 12px;'>"
        "<tr><td><b>Send to:</b></td><td style='font-family: monospace;'>%1</td></tr>"
        "<tr><td colspan='2'><hr/></td></tr>"
        "<tr><td><b>Amount:</b></td><td align='right'><b style='font-size: 13px;'>%2</b></td></tr>"
        "<tr><td>Network Fee:</td><td align='right'>%3</td></tr>"
        "<tr><td colspan='2'><hr/></td></tr>"
        "<tr><td><b>Total Deducted:</b></td><td align='right'><b style='font-size: 13px;'>%4</b></td></tr>"
        "<tr><td>USD Equivalent:</td><td align='right'>%5</td></tr>"
        "</table><br/>"
        "<span style='color: #666; font-size: 11px;'>This transaction cannot be reversed once sent.</span>"
    ).arg(address)
     .arg(formatDDAmount(amount))
     .arg(formatDDAmount(m_estimatedFee))
     .arg(formatDDAmount(total))
     .arg(formatUSDAmount(usdEquivalent));

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm DigiDollar Transfer"));
    msgBox.setText(confirmMsg);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No); // Safety: default to cancel
    msgBox.setIcon(QMessageBox::Question);

    // Customize button text
    QAbstractButton* yesButton = msgBox.button(QMessageBox::Yes);
    yesButton->setText(tr("&Send DigiDollar"));
    QAbstractButton* noButton = msgBox.button(QMessageBox::No);
    noButton->setText(tr("&Cancel"));

    int result = msgBox.exec();

    if (result == QMessageBox::Yes) {
        LogPrintf("DigiDollar: User confirmed transfer of %f DD to %s\n",
                  amount, address.toStdString());
        return true;
    } else {
        LogPrintf("DigiDollar: User cancelled transfer\n");
        return false;
    }
}

// PHASE 7.3: Execute transfer with progress indicator and error handling
void DigiDollarSendWidget::executeTransfer(const QString& address, double amount)
{
    // Show progress dialog
    QProgressDialog progress(tr("Sending DigiDollar..."),
                            tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0); // Show immediately
    progress.setCancelButton(nullptr); // No cancel during transfer
    progress.show();
    QApplication::processEvents(); // Force update

    // Disable UI during transfer
    m_sendButton->setEnabled(false);
    m_addressEdit->setEnabled(false);
    m_amountEdit->setEnabled(false);
    m_clearButton->setEnabled(false);
    m_useAvailableBalanceButton->setEnabled(false);

    // Convert amount from double to CAmount (cents)
    CAmount amountCents = static_cast<CAmount>(amount * 100);

    // Call the wallet model to send DigiDollar
    WalletModel::DigiDollarSendResult result = m_walletModel->sendDigiDollar(address, amountCents, "");

    // Close progress dialog
    progress.close();

    // Re-enable UI
    m_sendButton->setEnabled(true);
    m_addressEdit->setEnabled(true);
    m_amountEdit->setEnabled(true);
    m_clearButton->setEnabled(true);
    m_useAvailableBalanceButton->setEnabled(true);

    // Handle result
    if (result.status == WalletModel::OK) {
        // Success!
        showSuccess(result.txid, amount);
        onClearClicked();
        updateBalance(); // Refresh balance display
    } else {
        // Error occurred - map to user-friendly message
        showBackendError(static_cast<int>(result.status), result.reasonFailed);
    }
}

// PHASE 7.3: Success notification
void DigiDollarSendWidget::showSuccess(const QString& txid, double amount)
{
    QString successMsg = tr(
        "<b style='font-size: 14px; color: green;'>✓ DigiDollar Transfer Successful</b><br/><br/>"
        "<table cellpadding='4' style='font-size: 12px;'>"
        "<tr><td><b>Amount Sent:</b></td><td align='right'>%1</td></tr>"
        "<tr><td><b>Transaction ID:</b></td><td style='font-family: monospace; font-size: 10px;'>%2</td></tr>"
        "</table><br/>"
        "<span style='color: #666; font-size: 11px;'>"
        "Your transaction has been broadcast to the network.<br/>"
        "It will be confirmed in the next block."
        "</span>"
    ).arg(formatDDAmount(amount))
     .arg(txid);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Transfer Successful"));
    msgBox.setText(successMsg);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();

    LogPrintf("DigiDollar: Transfer successful - txid: %s\n", txid.toStdString());
}

// PHASE 7.3: Backend error message mapping
void DigiDollarSendWidget::showBackendError(int status, const QString& reasonFailed)
{
    QString errorTitle;
    QString errorMessage;

    // Map backend errors to user-friendly messages
    switch (status) {
    case WalletModel::InvalidAddress:
        errorTitle = tr("Invalid Address");
        errorMessage = tr(
            "The recipient address is invalid.\n\n"
            "Please check the address format and try again.\n\n"
            "Technical details: %1"
        ).arg(reasonFailed);
        break;

    case WalletModel::InvalidAmount:
        errorTitle = tr("Invalid Amount");
        errorMessage = tr(
            "The transfer amount is invalid.\n\n"
            "Please ensure the amount is:\n"
            "• Greater than 0\n"
            "• Within the maximum limit\n"
            "• Properly formatted\n\n"
            "Technical details: %1"
        ).arg(reasonFailed);
        break;

    case WalletModel::AmountExceedsBalance:
        errorTitle = tr("Insufficient Balance");
        errorMessage = tr(
            "You don't have enough DigiDollar for this transfer.\n\n"
            "%1\n\n"
            "Please:\n"
            "• Enter a smaller amount, or\n"
            "• Add more DD to your wallet"
        ).arg(reasonFailed);
        break;

    case WalletModel::TransactionCreationFailed:
        errorTitle = tr("Transaction Failed");

        // Parse specific error types
        if (reasonFailed.contains("locked", Qt::CaseInsensitive)) {
            errorMessage = tr(
                "Your wallet is locked.\n\n"
                "Please unlock your wallet to send DigiDollar.\n\n"
                "Go to Settings > Unlock Wallet"
            );
        } else if (reasonFailed.contains("coin selection", Qt::CaseInsensitive) ||
                   reasonFailed.contains("SelectDDCoins", Qt::CaseInsensitive)) {
            errorMessage = tr(
                "Unable to select DigiDollar for transfer.\n\n"
                "This may occur if:\n"
                "• Your DD is locked in pending transactions\n"
                "• The requested amount requires too many inputs\n\n"
                "Please try:\n"
                "• Waiting for pending transactions to confirm\n"
                "• Sending a smaller amount\n\n"
                "Technical details: %1"
            ).arg(reasonFailed);
        } else if (reasonFailed.contains("signing", Qt::CaseInsensitive)) {
            errorMessage = tr(
                "Transaction signing failed.\n\n"
                "Please ensure:\n"
                "• Your wallet is unlocked\n"
                "• You have the required private keys\n\n"
                "Technical details: %1"
            ).arg(reasonFailed);
        } else if (reasonFailed.contains("mempool", Qt::CaseInsensitive) ||
                   reasonFailed.contains("broadcast", Qt::CaseInsensitive)) {
            errorMessage = tr(
                "Transaction was rejected by the network.\n\n"
                "This may occur if:\n"
                "• Network fees are too low\n"
                "• The transaction conflicts with another\n"
                "• Network connectivity issues\n\n"
                "Please try:\n"
                "• Waiting a few moments and trying again\n"
                "• Checking your network connection\n\n"
                "Technical details: %1"
            ).arg(reasonFailed);
        } else {
            // Generic transaction failure
            errorMessage = tr(
                "Failed to create or send the transaction.\n\n"
                "Technical details: %1\n\n"
                "If this problem persists, please check:\n"
                "• Wallet synchronization status\n"
                "• Network connection\n"
                "• Available DigiDollar balance"
            ).arg(reasonFailed);
        }
        break;

    default:
        errorTitle = tr("Transfer Error");
        errorMessage = tr(
            "An unexpected error occurred during transfer.\n\n"
            "Error details: %1\n\n"
            "Please try again or contact support if the issue persists."
        ).arg(reasonFailed);
        break;
    }

    showError(errorTitle, errorMessage);
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