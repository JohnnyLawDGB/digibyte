// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_DIGIDOLLARTRANSACTIONSWIDGET_H
#define DIGIBYTE_QT_DIGIDOLLARTRANSACTIONSWIDGET_H

#include <QWidget>
#include <consensus/amount.h>

class WalletModel;
class ClientModel;

QT_BEGIN_NAMESPACE
class QTableWidget;
class QVBoxLayout;
class QHBoxLayout;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QMenu;
QT_END_NAMESPACE

/**
 * DigiDollar transactions widget showing complete transaction history.
 * This widget displays all DigiDollar transactions (mints, sends, receives, redemptions)
 * with filtering and search capabilities.
 */
class DigiDollarTransactionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DigiDollarTransactionsWidget(QWidget* parent = nullptr);
    ~DigiDollarTransactionsWidget();

    void setWalletModel(WalletModel* model);
    void setClientModel(ClientModel* model);
    void updateView();

Q_SIGNALS:
    void message(const QString& title, const QString& message, unsigned int style);

private Q_SLOTS:
    void updateTransactions();
    void onTypeFilterChanged(int index);
    void onSearchTextChanged();
    void showContextMenu(const QPoint& pos);
    void copyTxId();
    void copyAmount();
    void showDetails();

private:
    void setupUI();
    void setupFilterBar();
    void setupTable();
    void connectSignals();
    void populateTable();
    QString formatDDAmount(CAmount amount) const;
    QString formatTimestamp(uint64_t timestamp) const;
    QString formatConfirmations(int confirmations) const;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_filterLayout;
    QComboBox* m_typeFilter;
    QLineEdit* m_searchEdit;
    QPushButton* m_refreshButton;
    QTableWidget* m_table;
    QLabel* m_statusLabel;
    QMenu* m_contextMenu;

    // Models
    WalletModel* m_walletModel;
    ClientModel* m_clientModel;

    // Table column indices
    enum Column {
        Date = 0,
        Type,
        Amount,
        TxId,
        Confirmations,
        ColumnCount
    };
};

#endif // DIGIBYTE_QT_DIGIDOLLARTRANSACTIONSWIDGET_H
