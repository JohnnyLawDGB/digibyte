// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_DIGIDOLLARTAB_H
#define DIGIBYTE_QT_DIGIDOLLARTAB_H

#include <QWidget>

class DigiDollarOverviewWidget;
class DigiDollarSendWidget;
class DigiDollarMintWidget;
class DigiDollarRedeemWidget;
class DigiDollarPositionsWidget;
class WalletModel;
class ClientModel;

QT_BEGIN_NAMESPACE
class QTabWidget;
class QVBoxLayout;
QT_END_NAMESPACE

/**
 * DigiDollar main tab widget containing all DigiDollar functionality.
 * This widget provides access to DigiDollar overview, sending, minting,
 * redeeming, and position management functionality.
 */
class DigiDollarTab : public QWidget
{
    Q_OBJECT

public:
    explicit DigiDollarTab(QWidget *parent = nullptr);
    ~DigiDollarTab();

    void setWalletModel(WalletModel* model);
    void setClientModel(ClientModel* model);
    void updateView();

    /** Show incoming DigiDollar transaction notification */
    void incomingDDTransaction(const QString& date, const QString& amount,
                               const QString& type, const QString& address);

Q_SIGNALS:
    /** Fired when a message should be reported to the user */
    void message(const QString &title, const QString &message, unsigned int style);

public Q_SLOTS:
    /** Update balance displays across all widgets */
    void updateBalance();
    /** Update oracle price displays */
    void updateOraclePrice();
    /** Update system health status */
    void updateSystemHealth();
    /** Update positions table */
    void updatePositions();

private Q_SLOTS:
    /** Handle tab change to update the active widget */
    void onTabChanged(int index);

private:
    void setupUI();
    void connectSignals();

    // UI components
    QTabWidget* m_tabWidget;
    QVBoxLayout* m_mainLayout;

    // Sub-widgets
    DigiDollarOverviewWidget* m_overviewWidget;
    DigiDollarSendWidget* m_sendWidget;
    DigiDollarMintWidget* m_mintWidget;
    DigiDollarRedeemWidget* m_redeemWidget;
    DigiDollarPositionsWidget* m_positionsWidget;

    // Models
    WalletModel* m_walletModel;
    ClientModel* m_clientModel;
};

#endif // DIGIBYTE_QT_DIGIDOLLARTAB_H