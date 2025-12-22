// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollarreceiverequest.h>

#include <qt/guiutil.h>
#include <qt/qrimagewidget.h>
#include <qt/walletmodel.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QFileDialog>
#include <QStandardPaths>
#include <QUrl>
#include <QFont>
#include <QStyle>

DigiDollarReceiveRequestDialog::DigiDollarReceiveRequestDialog(QWidget *parent)
    : QDialog(parent, GUIUtil::dialog_flags),
      m_model(nullptr)
{
    // Set object name FIRST for CSS styling to work
    setObjectName("DigiDollarReceiveRequestDialog");

    setupUI();
    GUIUtil::handleCloseWindowShortcut(this);

    // Force style refresh after object name is set
    style()->unpolish(this);
    style()->polish(this);
}

DigiDollarReceiveRequestDialog::~DigiDollarReceiveRequestDialog()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarReceiveRequestDialog::setupUI()
{
    setWindowTitle(tr("DigiDollar Payment Request"));
    setMinimumWidth(500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Title
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    // QR Code
    m_qrWidget = new QRImageWidget(this);
    m_qrWidget->setMinimumSize(300, 300);
    m_qrWidget->setMaximumSize(300, 300);

    QHBoxLayout* qrLayout = new QHBoxLayout();
    qrLayout->addStretch();
    qrLayout->addWidget(m_qrWidget);
    qrLayout->addStretch();
    mainLayout->addLayout(qrLayout);

    // Details grid
    QGridLayout* detailsLayout = new QGridLayout();
    detailsLayout->setColumnStretch(1, 1);
    detailsLayout->setVerticalSpacing(8);
    detailsLayout->setHorizontalSpacing(12);
    int row = 0;

    // URI
    m_uriTagLabel = new QLabel(tr("URI:"), this);
    m_uriTagLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
    QFont boldFont = m_uriTagLabel->font();
    boldFont.setBold(true);
    m_uriTagLabel->setFont(boldFont);

    m_uriContent = new QTextEdit(this);
    m_uriContent->setReadOnly(true);
    m_uriContent->setMaximumHeight(60);
    m_uriContent->setFont(GUIUtil::fixedPitchFont());

    detailsLayout->addWidget(m_uriTagLabel, row, 0);
    detailsLayout->addWidget(m_uriContent, row, 1);
    row++;

    // Address
    m_addressTagLabel = new QLabel(tr("Address:"), this);
    m_addressTagLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_addressTagLabel->setFont(boldFont);

    m_addressContent = new QLineEdit(this);
    m_addressContent->setReadOnly(true);
    m_addressContent->setFont(GUIUtil::fixedPitchFont());

    detailsLayout->addWidget(m_addressTagLabel, row, 0);
    detailsLayout->addWidget(m_addressContent, row, 1);
    row++;

    // Amount
    m_amountTagLabel = new QLabel(tr("Amount:"), this);
    m_amountTagLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_amountTagLabel->setFont(boldFont);

    m_amountContent = new QLabel(this);

    detailsLayout->addWidget(m_amountTagLabel, row, 0);
    detailsLayout->addWidget(m_amountContent, row, 1);
    row++;

    // Label
    m_labelTagLabel = new QLabel(tr("Label:"), this);
    m_labelTagLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_labelTagLabel->setFont(boldFont);

    m_labelContent = new QLabel(this);
    m_labelContent->setWordWrap(true);

    detailsLayout->addWidget(m_labelTagLabel, row, 0);
    detailsLayout->addWidget(m_labelContent, row, 1);
    row++;

    // Message
    m_messageTagLabel = new QLabel(tr("Message:"), this);
    m_messageTagLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_messageTagLabel->setFont(boldFont);

    m_messageContent = new QLabel(this);
    m_messageContent->setWordWrap(true);

    detailsLayout->addWidget(m_messageTagLabel, row, 0);
    detailsLayout->addWidget(m_messageContent, row, 1);
    row++;

    // Wallet
    m_walletTagLabel = new QLabel(tr("Wallet:"), this);
    m_walletTagLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_walletTagLabel->setFont(boldFont);

    m_walletContent = new QLabel(this);

    detailsLayout->addWidget(m_walletTagLabel, row, 0);
    detailsLayout->addWidget(m_walletContent, row, 1);
    row++;

    mainLayout->addLayout(detailsLayout);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_copyURIButton = new QPushButton(tr("Copy &URI"), this);
    m_copyURIButton->setToolTip(tr("Copy the payment request URI to the clipboard"));
    connect(m_copyURIButton, &QPushButton::clicked, this, &DigiDollarReceiveRequestDialog::onCopyURIClicked);

    m_copyAddressButton = new QPushButton(tr("Copy &Address"), this);
    m_copyAddressButton->setToolTip(tr("Copy the DigiDollar address to the clipboard"));
    connect(m_copyAddressButton, &QPushButton::clicked, this, &DigiDollarReceiveRequestDialog::onCopyAddressClicked);

    m_saveQRButton = new QPushButton(tr("&Save Image..."), this);
    m_saveQRButton->setToolTip(tr("Save the QR code as an image file"));
    connect(m_saveQRButton, &QPushButton::clicked, this, &DigiDollarReceiveRequestDialog::onSaveQRClicked);

    m_verifyButton = new QPushButton(tr("&Verify"), this);
    m_verifyButton->setToolTip(tr("Verify this address on external signer"));
    m_verifyButton->setVisible(false); // Hidden by default, shown if external signer available

    m_closeButton = new QPushButton(tr("&Close"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(m_copyURIButton);
    buttonLayout->addWidget(m_copyAddressButton);
    buttonLayout->addWidget(m_saveQRButton);
    buttonLayout->addWidget(m_verifyButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void DigiDollarReceiveRequestDialog::setModel(WalletModel *model)
{
    m_model = model;
    updateDisplayUnit();
}

void DigiDollarReceiveRequestDialog::setInfo(const SendCoinsRecipient &info)
{
    m_info = info;

    // Set title
    QString title = tr("Request payment to %1").arg(info.label.isEmpty() ? info.address : info.label);
    setWindowTitle(title);
    m_titleLabel->setText(title);

    // Generate URI
    QString uri = formatDDURI(info);

    // Set QR code
    if (m_qrWidget->setQR(uri, info.address)) {
        m_saveQRButton->setEnabled(true);
    } else {
        m_saveQRButton->setEnabled(false);
    }

    // Set URI content as clickable link
    m_uriContent->setHtml("<a href=\"" + uri + "\">" + GUIUtil::HtmlEscape(uri) + "</a>");

    // Set address
    m_addressContent->setText(info.address);

    // Set amount (or hide if not specified)
    if (info.amount > 0) {
        m_amountContent->setText(formatDDAmount(info.amount));
        m_amountTagLabel->setVisible(true);
        m_amountContent->setVisible(true);
    } else {
        m_amountTagLabel->setVisible(false);
        m_amountContent->setVisible(false);
    }

    // Set label (or hide if not specified)
    if (!info.label.isEmpty()) {
        m_labelContent->setText(info.label);
        m_labelTagLabel->setVisible(true);
        m_labelContent->setVisible(true);
    } else {
        m_labelTagLabel->setVisible(false);
        m_labelContent->setVisible(false);
    }

    // Set message (or hide if not specified)
    if (!info.message.isEmpty()) {
        m_messageContent->setText(info.message);
        m_messageTagLabel->setVisible(true);
        m_messageContent->setVisible(true);
    } else {
        m_messageTagLabel->setVisible(false);
        m_messageContent->setVisible(false);
    }

    // Set wallet name (or hide if not available)
    if (m_model && !m_model->getWalletName().isEmpty()) {
        m_walletContent->setText(m_model->getWalletName());
        m_walletTagLabel->setVisible(true);
        m_walletContent->setVisible(true);
    } else {
        m_walletTagLabel->setVisible(false);
        m_walletContent->setVisible(false);
    }

    // Show verify button if external signer available
    if (m_model) {
        m_verifyButton->setVisible(m_model->wallet().hasExternalSigner());
        if (m_verifyButton->isVisible()) {
            connect(m_verifyButton, &QPushButton::clicked, [this] {
                m_model->displayAddress(m_info.address.toStdString());
            });
        }
    }
}

void DigiDollarReceiveRequestDialog::updateDisplayUnit()
{
    if (m_model && m_info.amount > 0) {
        m_amountContent->setText(formatDDAmount(m_info.amount));
    }
}

void DigiDollarReceiveRequestDialog::onCopyURIClicked()
{
    QString uri = formatDDURI(m_info);
    GUIUtil::setClipboard(uri);
}

void DigiDollarReceiveRequestDialog::onCopyAddressClicked()
{
    GUIUtil::setClipboard(m_info.address);
}

void DigiDollarReceiveRequestDialog::onSaveQRClicked()
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString defaultFileName = defaultPath + "/digidollar-request-" + m_info.address.left(8) + ".png";

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save QR Code"),
                                                    defaultFileName, tr("PNG Image (*.png)"));
    if (fileName.isEmpty()) {
        return;
    }

    QImage qrImage = m_qrWidget->exportImage();
    if (!qrImage.save(fileName, "PNG")) {
        // Error saving - could show message box here
    }
}

QString DigiDollarReceiveRequestDialog::formatDDAmount(CAmount amount) const
{
    // Amount is stored in cents for DD (100 cents = 1 DD)
    double ddAmount = amount / 100.0;
    return QString::number(ddAmount, 'f', 2) + " DD";
}

QString DigiDollarReceiveRequestDialog::formatDDURI(const SendCoinsRecipient &info) const
{
    QString uri = "digidollar:" + info.address;
    QStringList params;

    if (!info.label.isEmpty()) {
        params << "label=" + QString(QUrl::toPercentEncoding(info.label));
    }
    if (info.amount > 0) {
        // Amount in DD (cents / 100)
        double ddAmount = info.amount / 100.0;
        params << "amount=" + QString::number(ddAmount, 'f', 8);
    }
    if (!info.message.isEmpty()) {
        params << "message=" + QString(QUrl::toPercentEncoding(info.message));
    }

    if (!params.isEmpty()) {
        uri += "?" + params.join("&");
    }

    return uri;
}
