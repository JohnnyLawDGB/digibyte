// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarreceivewidget.h>

#include <qt/walletmodel.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/qrimagewidget.h>
#include <qt/sendcoinsrecipient.h>
#include <qt/recentrequeststablemodel.h>
#include <consensus/amount.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QToolButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QPalette>
#include <QUrl>
#include <QStandardPaths>

DigiDollarReceiveWidget::DigiDollarReceiveWidget(QWidget *parent) :
    QWidget(parent),
    m_mainLayout(nullptr),
    m_generateFrame(nullptr),
    m_generateLayout(nullptr),
    m_labelLabel(nullptr),
    m_labelEdit(nullptr),
    m_amountLabel(nullptr),
    m_amountEdit(nullptr),
    m_messageLabel(nullptr),
    m_messageEdit(nullptr),
    m_generateButton(nullptr),
    m_clearButton(nullptr),
    m_qrFrame(nullptr),
    m_qrLayout(nullptr),
    m_qrTitle(nullptr),
    m_qrImage(nullptr),
    m_addressLabel(nullptr),
    m_addressEdit(nullptr),
    m_qrButtonLayout(nullptr),
    m_copyAddressButton(nullptr),
    m_copyQRButton(nullptr),
    m_saveQRButton(nullptr),
    m_requestsFrame(nullptr),
    m_requestsLayout(nullptr),
    m_requestsTitle(nullptr),
    m_requestsTable(nullptr),
    m_requestsButtonLayout(nullptr),
    m_showRequestButton(nullptr),
    m_removeRequestButton(nullptr),
    m_noRequestsLabel(nullptr),
    m_walletModel(nullptr),
    m_clientModel(nullptr)
{
    setupUI();
    connectSignals();
    // applyTheme(); // REMOVED: Now handled by CSS files
}

DigiDollarReceiveWidget::~DigiDollarReceiveWidget()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarReceiveWidget::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);

    // Setup sections
    setupGenerateSection();
    setupQRSection();
    setupRecentRequestsSection();

    // Add stretch to push content to top
    m_mainLayout->addStretch();

    setLayout(m_mainLayout);
}

void DigiDollarReceiveWidget::setupGenerateSection()
{
    // Create generate frame
    m_generateFrame = new QFrame(this);
    m_generateFrame->setFrameStyle(QFrame::StyledPanel);
    m_generateFrame->setFrameShadow(QFrame::Sunken);
    m_generateFrame->setObjectName("generateFrame");

    m_generateLayout = new QGridLayout(m_generateFrame);
    m_generateLayout->setSpacing(8);
    m_generateLayout->setContentsMargins(10, 10, 10, 10);

    // Label field (optional)
    m_labelLabel = new QLabel(tr("&Label:"), this);
    m_labelLabel->setObjectName("labelLabel");
    m_labelLabel->setToolTip(tr("Optional label to identify this request"));

    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setObjectName("labelEdit");
    m_labelEdit->setPlaceholderText(tr("e.g., Invoice #123"));
    m_labelEdit->setToolTip(tr("An optional label to identify this payment request"));
    m_labelLabel->setBuddy(m_labelEdit);

    // Amount field (optional)
    m_amountLabel = new QLabel(tr("&Amount:"), this);
    m_amountLabel->setObjectName("amountLabel");
    m_amountLabel->setToolTip(tr("Optional amount in DigiDollar"));

    m_amountEdit = new QLineEdit(this);
    m_amountEdit->setObjectName("amountEdit");
    m_amountEdit->setPlaceholderText(tr("0.00"));
    m_amountEdit->setToolTip(tr("An optional amount to request (in DD)"));
    m_amountLabel->setBuddy(m_amountEdit);

    // Message field (optional)
    m_messageLabel = new QLabel(tr("&Message:"), this);
    m_messageLabel->setObjectName("messageLabel");
    m_messageLabel->setToolTip(tr("Optional message for the payment"));

    m_messageEdit = new QLineEdit(this);
    m_messageEdit->setObjectName("messageEdit");
    m_messageEdit->setPlaceholderText(tr("Payment for goods or services"));
    m_messageEdit->setToolTip(tr("An optional message to attach to the payment request"));
    m_messageLabel->setBuddy(m_messageEdit);

    // Buttons
    m_generateButton = new QPushButton(tr("&Generate New Address"), this);
    m_generateButton->setObjectName("generateButton");
    m_generateButton->setToolTip(tr("Generate a new DigiDollar receiving address"));

    m_clearButton = new QPushButton(tr("C&lear"), this);
    m_clearButton->setObjectName("clearButton");
    m_clearButton->setToolTip(tr("Clear all input fields"));

    // Add to layout
    m_generateLayout->addWidget(m_labelLabel, 0, 0);
    m_generateLayout->addWidget(m_labelEdit, 0, 1, 1, 2);

    m_generateLayout->addWidget(m_amountLabel, 1, 0);
    m_generateLayout->addWidget(m_amountEdit, 1, 1, 1, 2);

    m_generateLayout->addWidget(m_messageLabel, 2, 0);
    m_generateLayout->addWidget(m_messageEdit, 2, 1, 1, 2);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_generateButton);

    m_generateLayout->addLayout(buttonLayout, 3, 0, 1, 3);

    m_mainLayout->addWidget(m_generateFrame);
}

void DigiDollarReceiveWidget::setupQRSection()
{
    // Create QR frame
    m_qrFrame = new QFrame(this);
    m_qrFrame->setFrameStyle(QFrame::StyledPanel);
    m_qrFrame->setFrameShadow(QFrame::Sunken);
    m_qrFrame->setObjectName("qrFrame");
    m_qrFrame->setVisible(false); // Hidden until address is generated

    m_qrLayout = new QVBoxLayout(m_qrFrame);
    m_qrLayout->setSpacing(8);
    m_qrLayout->setContentsMargins(10, 10, 10, 10);

    // Title
    m_qrTitle = new QLabel(tr("Your DigiDollar Address"), this);
    m_qrTitle->setObjectName("qrTitle");
    QFont titleFont = m_qrTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_qrTitle->setFont(titleFont);
    m_qrTitle->setAlignment(Qt::AlignCenter);

    // QR code image
    m_qrImage = new QRImageWidget(this);
    m_qrImage->setObjectName("qrImage");
    m_qrImage->setMinimumSize(250, 250);
    m_qrImage->setMaximumSize(250, 250);

    // Center the QR code
    QHBoxLayout* qrCenterLayout = new QHBoxLayout();
    qrCenterLayout->addStretch();
    qrCenterLayout->addWidget(m_qrImage);
    qrCenterLayout->addStretch();

    // Address label and edit
    m_addressLabel = new QLabel(tr("Address:"), this);
    m_addressLabel->setObjectName("addressLabel");

    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setObjectName("addressEdit");
    m_addressEdit->setReadOnly(true);
    m_addressEdit->setFont(GUIUtil::fixedPitchFont());
    m_addressEdit->setAlignment(Qt::AlignCenter);

    // QR action buttons
    m_qrButtonLayout = new QHBoxLayout();
    m_qrButtonLayout->setSpacing(10);

    m_copyAddressButton = new QPushButton(tr("Copy &Address"), this);
    m_copyAddressButton->setObjectName("copyAddressButton");
    m_copyAddressButton->setToolTip(tr("Copy the DigiDollar address to the clipboard"));

    m_copyQRButton = new QPushButton(tr("Copy &QR Code"), this);
    m_copyQRButton->setObjectName("copyQRButton");
    m_copyQRButton->setToolTip(tr("Copy the QR code image to the clipboard"));

    m_saveQRButton = new QPushButton(tr("&Save QR Code"), this);
    m_saveQRButton->setObjectName("saveQRButton");
    m_saveQRButton->setToolTip(tr("Save the QR code as an image file"));

    m_qrButtonLayout->addStretch();
    m_qrButtonLayout->addWidget(m_copyAddressButton);
    m_qrButtonLayout->addWidget(m_copyQRButton);
    m_qrButtonLayout->addWidget(m_saveQRButton);
    m_qrButtonLayout->addStretch();

    // Add to layout
    m_qrLayout->addWidget(m_qrTitle);
    m_qrLayout->addLayout(qrCenterLayout);
    m_qrLayout->addWidget(m_addressLabel);
    m_qrLayout->addWidget(m_addressEdit);
    m_qrLayout->addLayout(m_qrButtonLayout);

    m_mainLayout->addWidget(m_qrFrame);
}

void DigiDollarReceiveWidget::setupRecentRequestsSection()
{
    // Create requests frame
    m_requestsFrame = new QFrame(this);
    m_requestsFrame->setFrameStyle(QFrame::StyledPanel);
    m_requestsFrame->setFrameShadow(QFrame::Sunken);
    m_requestsFrame->setObjectName("requestsFrame");

    m_requestsLayout = new QVBoxLayout(m_requestsFrame);
    m_requestsLayout->setSpacing(8);
    m_requestsLayout->setContentsMargins(10, 10, 10, 10);

    // Title
    m_requestsTitle = new QLabel(tr("Recent Payment Requests"), this);
    m_requestsTitle->setObjectName("requestsTitle");
    QFont titleFont = m_requestsTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setBold(true);
    m_requestsTitle->setFont(titleFont);

    // Table
    m_requestsTable = new QTableWidget(this);
    m_requestsTable->setObjectName("requestsTable");
    m_requestsTable->setColumnCount(4);
    m_requestsTable->setHorizontalHeaderLabels({tr("Date"), tr("Label"), tr("Amount"), tr("Address")});
    m_requestsTable->horizontalHeader()->setStretchLastSection(true);
    m_requestsTable->verticalHeader()->setVisible(false);
    m_requestsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_requestsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_requestsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_requestsTable->setAlternatingRowColors(true);

    // No requests label (shown when table is empty)
    m_noRequestsLabel = new QLabel(tr("No recent payment requests"), this);
    m_noRequestsLabel->setObjectName("noRequestsLabel");
    m_noRequestsLabel->setAlignment(Qt::AlignCenter);
    m_noRequestsLabel->setMinimumHeight(100);

    // Buttons
    m_requestsButtonLayout = new QHBoxLayout();

    m_showRequestButton = new QPushButton(tr("&Show"), this);
    m_showRequestButton->setObjectName("showRequestButton");
    m_showRequestButton->setToolTip(tr("Show the selected request"));
    m_showRequestButton->setEnabled(false);

    m_removeRequestButton = new QPushButton(tr("&Remove"), this);
    m_removeRequestButton->setObjectName("removeRequestButton");
    m_removeRequestButton->setToolTip(tr("Remove the selected request from history"));
    m_removeRequestButton->setEnabled(false);

    m_requestsButtonLayout->addWidget(m_showRequestButton);
    m_requestsButtonLayout->addWidget(m_removeRequestButton);
    m_requestsButtonLayout->addStretch();

    // Add to layout
    m_requestsLayout->addWidget(m_requestsTitle);
    m_requestsLayout->addWidget(m_requestsTable);
    m_requestsLayout->addWidget(m_noRequestsLabel);
    m_requestsLayout->addLayout(m_requestsButtonLayout);

    // Initially show "no requests" label
    m_requestsTable->setVisible(false);
    m_noRequestsLabel->setVisible(true);

    m_mainLayout->addWidget(m_requestsFrame);
}

void DigiDollarReceiveWidget::connectSignals()
{
    // Generate section
    connect(m_generateButton, &QPushButton::clicked,
            this, &DigiDollarReceiveWidget::onGenerateAddressClicked);
    connect(m_clearButton, &QPushButton::clicked,
            this, &DigiDollarReceiveWidget::onClearClicked);
    connect(m_labelEdit, &QLineEdit::textChanged,
            this, &DigiDollarReceiveWidget::onLabelChanged);
    connect(m_amountEdit, &QLineEdit::textChanged,
            this, &DigiDollarReceiveWidget::onAmountChanged);
    connect(m_messageEdit, &QLineEdit::textChanged,
            this, &DigiDollarReceiveWidget::onMessageChanged);

    // QR section
    connect(m_copyAddressButton, &QPushButton::clicked,
            this, &DigiDollarReceiveWidget::onCopyAddressClicked);
    connect(m_copyQRButton, &QPushButton::clicked,
            this, &DigiDollarReceiveWidget::onCopyQRClicked);
    connect(m_saveQRButton, &QPushButton::clicked,
            this, &DigiDollarReceiveWidget::onSaveQRClicked);

    // Recent requests section
    connect(m_requestsTable, &QTableWidget::itemSelectionChanged,
            this, &DigiDollarReceiveWidget::onRecentRequestSelected);
    connect(m_showRequestButton, &QPushButton::clicked,
            this, &DigiDollarReceiveWidget::onShowRequestClicked);
    connect(m_removeRequestButton, &QPushButton::clicked,
            this, &DigiDollarReceiveWidget::onRemoveRequestClicked);
}

void DigiDollarReceiveWidget::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    if (m_walletModel) {
        updateRecentRequests();
        // applyTheme(); // REMOVED: Now handled by CSS files
    }
}

void DigiDollarReceiveWidget::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    if (m_clientModel) {
        // applyTheme(); // REMOVED: Now handled by CSS files
    }
}

void DigiDollarReceiveWidget::updateView()
{
    updateRecentRequests();
}

void DigiDollarReceiveWidget::updateRecentRequests()
{
    // TODO: Implement recent requests from wallet model
    // For now, keep the table empty
    populateRecentRequests();
}

// REMOVED: applyTheme() - All styling now handled by CSS files (light.css/dark.css)
// This method was overriding the CSS theme with programmatic styling
void DigiDollarReceiveWidget::applyTheme()
{
    // Method disabled - CSS handles all theming now
}

void DigiDollarReceiveWidget::setMonospacedFont(bool use_embedded_font)
{
    if (use_embedded_font) {
        m_addressEdit->setFont(GUIUtil::fixedPitchFont());
    }
}

void DigiDollarReceiveWidget::onGenerateAddressClicked()
{
    if (!m_walletModel) {
        Q_EMIT message(tr("Error"), tr("No wallet model available"), QMessageBox::Critical);
        return;
    }

    generateNewAddress();
}

void DigiDollarReceiveWidget::generateNewAddress()
{
    if (!m_walletModel) {
        return;
    }

    // Get label from input field
    QString label = m_labelEdit->text();
    if (label.isEmpty()) {
        label = tr("Payment request");
    }

    // Generate new DD address using wallet model
    QString newAddress = m_walletModel->getNewDigiDollarAddress(label);

    if (newAddress.isEmpty()) {
        Q_EMIT message(tr("Error"), tr("Failed to generate new DigiDollar address"), QMessageBox::Critical);
        return;
    }

    m_currentAddress = newAddress;
    m_currentLabel = m_labelEdit->text();
    m_currentAmount = m_amountEdit->text();
    m_currentMessage = m_messageEdit->text();

    // Update address display
    m_addressEdit->setText(m_currentAddress);

    // Update QR code
    updateQRCode();

    // Show QR section
    m_qrFrame->setVisible(true);

    // Create payment request and save to wallet
    SendCoinsRecipient recipient;
    recipient.address = m_currentAddress;
    recipient.label = m_currentLabel;

    // Parse amount if provided
    bool ok = false;
    double amountValue = m_currentAmount.toDouble(&ok);
    if (ok && amountValue > 0) {
        // Convert DD amount to satoshis (cents to satoshis)
        // 1 DD = 100 cents, 1 DGB = 100,000,000 satoshis
        // For display purposes, store as cents
        recipient.amount = static_cast<CAmount>(amountValue * 100);
    } else {
        recipient.amount = 0; // No specific amount requested
    }

    recipient.message = m_currentMessage;

    // Add to recent requests table model (this persists to wallet.dat)
    if (m_walletModel && m_walletModel->getRecentRequestsTableModel()) {
        m_walletModel->getRecentRequestsTableModel()->addNewRequest(recipient);
    }

    // Add to UI table for immediate display
    QString dateStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
    QString amountStr = m_currentAmount.isEmpty() ? tr("Any") : formatDDAmount(m_currentAmount.toDouble());
    addRequestToTable(dateStr, m_currentLabel, amountStr, m_currentAddress);

    Q_EMIT message(tr("Success"), tr("New DigiDollar address generated"), QMessageBox::Information);
}

void DigiDollarReceiveWidget::updateQRCode()
{
    if (m_currentAddress.isEmpty()) {
        return;
    }

    // Create payment URI
    QString uri = formatDDURI(m_currentAddress, m_currentLabel, m_currentAmount, m_currentMessage);

    // Update QR code widget
    m_qrImage->setQR(uri, m_currentAddress);
}

void DigiDollarReceiveWidget::onCopyAddressClicked()
{
    QApplication::clipboard()->setText(m_currentAddress);
    Q_EMIT message(tr("Address Copied"), tr("DigiDollar address copied to clipboard"), QMessageBox::Information);
}

void DigiDollarReceiveWidget::onCopyQRClicked()
{
    if (m_qrImage) {
        m_qrImage->copyImage();
        Q_EMIT message(tr("QR Code Copied"), tr("QR code image copied to clipboard"), QMessageBox::Information);
    }
}

void DigiDollarReceiveWidget::onSaveQRClicked()
{
    if (!m_qrImage) {
        return;
    }

    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString defaultFileName = defaultPath + "/digidollar-" + m_currentAddress.left(8) + ".png";

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save QR Code"),
                                                    defaultFileName, tr("PNG Image (*.png)"));
    if (fileName.isEmpty()) {
        return;
    }

    QImage qrImage = m_qrImage->exportImage();
    if (qrImage.save(fileName, "PNG")) {
        Q_EMIT message(tr("QR Code Saved"),
                      tr("QR code saved successfully to:\n%1").arg(fileName),
                      QMessageBox::Information);
    } else {
        Q_EMIT message(tr("Error"), tr("Failed to save QR code"), QMessageBox::Critical);
    }
}

void DigiDollarReceiveWidget::onClearClicked()
{
    clearFields();
}

void DigiDollarReceiveWidget::clearFields()
{
    m_labelEdit->clear();
    m_amountEdit->clear();
    m_messageEdit->clear();
    m_addressEdit->clear();
    m_currentAddress.clear();
    m_currentLabel.clear();
    m_currentAmount.clear();
    m_currentMessage.clear();
    m_qrFrame->setVisible(false);
}

void DigiDollarReceiveWidget::onLabelChanged()
{
    m_currentLabel = m_labelEdit->text();
    if (!m_currentAddress.isEmpty()) {
        updateQRCode();
    }
}

void DigiDollarReceiveWidget::onAmountChanged()
{
    m_currentAmount = m_amountEdit->text();
    if (!m_currentAddress.isEmpty()) {
        updateQRCode();
    }
}

void DigiDollarReceiveWidget::onMessageChanged()
{
    m_currentMessage = m_messageEdit->text();
    if (!m_currentAddress.isEmpty()) {
        updateQRCode();
    }
}

void DigiDollarReceiveWidget::onRecentRequestSelected()
{
    bool hasSelection = !m_requestsTable->selectedItems().isEmpty();
    m_showRequestButton->setEnabled(hasSelection);
    m_removeRequestButton->setEnabled(hasSelection);
}

void DigiDollarReceiveWidget::onShowRequestClicked()
{
    int row = m_requestsTable->currentRow();
    if (row < 0) {
        return;
    }

    QString label = m_requestsTable->item(row, 1)->text();
    QString amount = m_requestsTable->item(row, 2)->text();
    QString address = m_requestsTable->item(row, 3)->text();

    // Load the selected request
    m_labelEdit->setText(label);
    if (amount != tr("Any")) {
        // Parse amount back from formatted string
        m_amountEdit->setText(amount.left(amount.indexOf(" DD")));
    }
    m_currentAddress = address;
    m_addressEdit->setText(address);

    updateQRCode();
    m_qrFrame->setVisible(true);
}

void DigiDollarReceiveWidget::onRemoveRequestClicked()
{
    int row = m_requestsTable->currentRow();
    if (row < 0) {
        return;
    }

    m_requestsTable->removeRow(row);

    // Update visibility based on remaining rows
    if (m_requestsTable->rowCount() == 0) {
        m_requestsTable->setVisible(false);
        m_noRequestsLabel->setVisible(true);
    }
}

void DigiDollarReceiveWidget::populateRecentRequests()
{
    // TODO: Load from wallet model
    // For now, table starts empty
    if (m_requestsTable->rowCount() == 0) {
        m_requestsTable->setVisible(false);
        m_noRequestsLabel->setVisible(true);
    } else {
        m_requestsTable->setVisible(true);
        m_noRequestsLabel->setVisible(false);
    }
}

void DigiDollarReceiveWidget::addRequestToTable(const QString& date, const QString& label,
                                                const QString& amount, const QString& address)
{
    int row = m_requestsTable->rowCount();
    m_requestsTable->insertRow(row);

    m_requestsTable->setItem(row, 0, new QTableWidgetItem(date));
    m_requestsTable->setItem(row, 1, new QTableWidgetItem(label));
    m_requestsTable->setItem(row, 2, new QTableWidgetItem(amount));
    m_requestsTable->setItem(row, 3, new QTableWidgetItem(address));

    // Show table, hide "no requests" label
    m_requestsTable->setVisible(true);
    m_noRequestsLabel->setVisible(false);
}

QString DigiDollarReceiveWidget::formatDDAmount(double amount) const
{
    return QString::number(amount, 'f', 8) + " DD";
}

QString DigiDollarReceiveWidget::formatDDURI(const QString& address, const QString& label,
                                             const QString& amount, const QString& message) const
{
    QString uri = "digidollar:" + address;
    QStringList params;

    if (!label.isEmpty()) {
        params << "label=" + QString(QUrl::toPercentEncoding(label));
    }
    if (!amount.isEmpty()) {
        params << "amount=" + amount;
    }
    if (!message.isEmpty()) {
        params << "message=" + QString(QUrl::toPercentEncoding(message));
    }

    if (!params.isEmpty()) {
        uri += "?" + params.join("&");
    }

    return uri;
}