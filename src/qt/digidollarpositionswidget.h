// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_DIGIDOLLARPOSITIONSWIDGET_H
#define DIGIBYTE_QT_DIGIDOLLARPOSITIONSWIDGET_H

#include <QWidget>

class WalletModel;
class ClientModel;

QT_BEGIN_NAMESPACE
class QTableWidget;
class QTableWidgetItem;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QHeaderView;
class QProgressBar;
QT_END_NAMESPACE

struct DigiDollarPosition {
    QString positionId;
    double ddMinted;
    double dgbCollateral;
    int lockTier;
    int blocksRemaining;
    double health;
    bool canRedeem;
};

/**
 * DigiDollar positions widget showing all active positions in a table.
 * This widget provides a comprehensive view of all DigiDollar positions
 * with ability to manage individual positions.
 */
class DigiDollarPositionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DigiDollarPositionsWidget(QWidget *parent = nullptr);
    ~DigiDollarPositionsWidget();

    void setWalletModel(WalletModel* model);
    void setClientModel(ClientModel* model);
    void updateView();

Q_SIGNALS:
    /** Fired when a message should be reported to the user */
    void message(const QString &title, const QString &message, unsigned int style);

public Q_SLOTS:
    /** Update positions table */
    void updatePositions();

private Q_SLOTS:
    /** Refresh button clicked */
    void onRefreshClicked();
    /** Position table cell clicked */
    void onPositionClicked(int row, int column);
    /** Redeem button clicked for a specific position */
    void onRedeemPositionClicked();
    /** Show context menu for table */
    void showContextMenu(const QPoint& point);

private:
    void setupUI();
    void setupTableHeader();
    void connectSignals();
    void populatePositionsTable();
    void loadPositionsFromWallet();
    void applyTheme();
    void addPositionToTable(const DigiDollarPosition& position, int row);
    QPushButton* createRedeemButton(const QString& positionId);

    QString formatDDAmount(double amount) const;
    QString formatDGBAmount(double amount) const;
    QString formatBlockTime(int blocks) const;
    QString formatHealthStatus(double health) const;
    QWidget* createHealthWidget(double health) const;

    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_headerLayout;
    QLabel* m_titleLabel;
    QPushButton* m_refreshButton;
    QTableWidget* m_positionsTable;
    QLabel* m_statusLabel;

    // Models
    WalletModel* m_walletModel;
    ClientModel* m_clientModel;

    // Data
    QList<DigiDollarPosition> m_positions;

    // Table columns
    enum PositionColumn {
        COL_POSITION_ID = 0,
        COL_DD_MINTED = 1,
        COL_DGB_COLLATERAL = 2,
        COL_LOCK_TIER = 3,
        COL_TIME_REMAINING = 4,
        COL_HEALTH = 5,
        COL_ACTIONS = 6,
        NUM_COLUMNS = 7
    };
};

#endif // DIGIBYTE_QT_DIGIDOLLARPOSITIONSWIDGET_H