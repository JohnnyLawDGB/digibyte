// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/digidollartab.h>

#include <qt/digidollaroverviewwidget.h>
#include <qt/digidollarreceivewidget.h>
#include <qt/digidollarsendwidget.h>
#include <qt/digidollarmintwidget.h>
#include <qt/digidollarredeemwidget.h>
#include <qt/digidollarpositionswidget.h>
#include <qt/walletmodel.h>
#include <qt/clientmodel.h>

#include <QTabWidget>
#include <QVBoxLayout>
#include <QTimer>

DigiDollarTab::DigiDollarTab(QWidget *parent) :
    QWidget(parent),
    m_tabWidget(nullptr),
    m_mainLayout(nullptr),
    m_overviewWidget(nullptr),
    m_receiveWidget(nullptr),
    m_sendWidget(nullptr),
    m_mintWidget(nullptr),
    m_redeemWidget(nullptr),
    m_positionsWidget(nullptr),
    m_walletModel(nullptr),
    m_clientModel(nullptr)
{
    setupUI();
    connectSignals();
}

DigiDollarTab::~DigiDollarTab()
{
    // Qt will handle cleanup of child widgets
}

void DigiDollarTab::setupUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create tab widget
    m_tabWidget = new QTabWidget(this);

    // Create sub-widgets
    m_overviewWidget = new DigiDollarOverviewWidget(this);
    m_overviewWidget->setObjectName("overviewWidget");

    m_receiveWidget = new DigiDollarReceiveWidget(this);
    m_receiveWidget->setObjectName("receiveWidget");

    m_sendWidget = new DigiDollarSendWidget(this);
    m_sendWidget->setObjectName("sendWidget");

    m_mintWidget = new DigiDollarMintWidget(this);
    m_mintWidget->setObjectName("mintWidget");

    m_redeemWidget = new DigiDollarRedeemWidget(this);
    m_redeemWidget->setObjectName("redeemWidget");

    m_positionsWidget = new DigiDollarPositionsWidget(this);
    m_positionsWidget->setObjectName("positionsWidget");

    // Add tabs in order: Overview, Send, Receive, Mint, Redeem, Vault
    m_tabWidget->addTab(m_overviewWidget, tr("Overview"));
    m_tabWidget->addTab(m_sendWidget, tr("Send"));
    m_tabWidget->addTab(m_receiveWidget, tr("&Receive"));
    m_tabWidget->addTab(m_mintWidget, tr("Mint"));
    m_tabWidget->addTab(m_redeemWidget, tr("Redeem"));
    m_tabWidget->addTab(m_positionsWidget, tr("Vault"));

    // Add tab widget to main layout
    m_mainLayout->addWidget(m_tabWidget);

    setLayout(m_mainLayout);
}

void DigiDollarTab::connectSignals()
{
    // Connect tab change signal
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &DigiDollarTab::onTabChanged);

    // Connect sub-widget signals
    if (m_overviewWidget) {
        // Connect overview widget signals when they're implemented
    }

    if (m_receiveWidget) {
        connect(m_receiveWidget, &DigiDollarReceiveWidget::message,
                this, &DigiDollarTab::message);
    }

    if (m_sendWidget) {
        connect(m_sendWidget, &DigiDollarSendWidget::message,
                this, &DigiDollarTab::message);
    }

    if (m_mintWidget) {
        connect(m_mintWidget, &DigiDollarMintWidget::message,
                this, &DigiDollarTab::message);
    }

    if (m_redeemWidget) {
        connect(m_redeemWidget, &DigiDollarRedeemWidget::message,
                this, &DigiDollarTab::message);
    }

    if (m_positionsWidget) {
        connect(m_positionsWidget, &DigiDollarPositionsWidget::message,
                this, &DigiDollarTab::message);
    }
}

void DigiDollarTab::setWalletModel(WalletModel* model)
{
    m_walletModel = model;

    // Pass wallet model to sub-widgets
    if (m_overviewWidget)
        m_overviewWidget->setWalletModel(model);
    if (m_receiveWidget)
        m_receiveWidget->setWalletModel(model);
    if (m_sendWidget)
        m_sendWidget->setWalletModel(model);
    if (m_mintWidget)
        m_mintWidget->setWalletModel(model);
    if (m_redeemWidget)
        m_redeemWidget->setWalletModel(model);
    if (m_positionsWidget)
        m_positionsWidget->setWalletModel(model);

    // Update view when wallet model changes
    updateView();
}

void DigiDollarTab::setClientModel(ClientModel* model)
{
    m_clientModel = model;

    // Pass client model to sub-widgets
    if (m_overviewWidget)
        m_overviewWidget->setClientModel(model);
    if (m_receiveWidget)
        m_receiveWidget->setClientModel(model);
    if (m_sendWidget)
        m_sendWidget->setClientModel(model);
    if (m_mintWidget)
        m_mintWidget->setClientModel(model);
    if (m_redeemWidget)
        m_redeemWidget->setClientModel(model);
    if (m_positionsWidget)
        m_positionsWidget->setClientModel(model);
}

void DigiDollarTab::updateView()
{
    // Update all sub-widgets
    if (m_overviewWidget)
        m_overviewWidget->updateView();
    if (m_receiveWidget)
        m_receiveWidget->updateView();
    if (m_sendWidget)
        m_sendWidget->updateView();
    if (m_mintWidget)
        m_mintWidget->updateView();
    if (m_redeemWidget)
        m_redeemWidget->updateView();
    if (m_positionsWidget)
        m_positionsWidget->updateView();
}

void DigiDollarTab::incomingDDTransaction(const QString& date, const QString& amount,
                                         const QString& type, const QString& address)
{
    // Notify overview widget of incoming transaction
    if (m_overviewWidget) {
        m_overviewWidget->incomingDDTransaction(date, amount, type, address);
    }

    // Update balance displays
    updateBalance();
}

void DigiDollarTab::updateBalance()
{
    if (m_overviewWidget)
        m_overviewWidget->updateBalance();
    if (m_sendWidget)
        m_sendWidget->updateBalance();
    if (m_mintWidget)
        m_mintWidget->updateBalance();
    if (m_redeemWidget)
        m_redeemWidget->updateBalance();
}

void DigiDollarTab::updateOraclePrice()
{
    if (m_overviewWidget)
        m_overviewWidget->updateOraclePrice();
    if (m_sendWidget)
        m_sendWidget->updateOraclePrice();
    if (m_mintWidget)
        m_mintWidget->updateOraclePrice();
}

void DigiDollarTab::updateSystemHealth()
{
    if (m_overviewWidget)
        m_overviewWidget->updateSystemHealth();
}

void DigiDollarTab::updatePositions()
{
    if (m_positionsWidget)
        m_positionsWidget->updatePositions();
    if (m_redeemWidget)
        m_redeemWidget->updatePositions();
}

void DigiDollarTab::onTabChanged(int index)
{
    // Update the active tab when switching
    switch (index) {
    case 0: // Overview
        if (m_overviewWidget)
            m_overviewWidget->updateView();
        break;
    case 1: // Receive
        if (m_receiveWidget)
            m_receiveWidget->updateView();
        break;
    case 2: // Send
        if (m_sendWidget)
            m_sendWidget->updateView();
        break;
    case 3: // Mint
        if (m_mintWidget)
            m_mintWidget->updateView();
        break;
    case 4: // Redeem
        if (m_redeemWidget)
            m_redeemWidget->updateView();
        break;
    case 5: // Vault
        if (m_positionsWidget)
            m_positionsWidget->updateView();
        break;
    default:
        break;
    }
}