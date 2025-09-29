// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_DIGIDOLLARSENDWIDGET_H
#define DIGIBYTE_QT_DIGIDOLLARSENDWIDGET_H

#include <QWidget>

class WalletModel;
class ClientModel;
class DigiDollarAddressValidator;
class AmountValidator;

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QFrame;
class QValidator;
QT_END_NAMESPACE

/**
 * DigiDollar send widget for sending DD to other addresses.
 * This widget provides functionality to send DigiDollar with proper
 * address validation and fee calculation.
 */
class DigiDollarSendWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DigiDollarSendWidget(QWidget *parent = nullptr);
    ~DigiDollarSendWidget();

    void setWalletModel(WalletModel* model);
    void setClientModel(ClientModel* model);
    void updateView();

Q_SIGNALS:
    /** Fired when a message should be reported to the user */
    void message(const QString &title, const QString &message, unsigned int style);

public Q_SLOTS:
    /** Update balance display */
    void updateBalance();
    /** Update oracle price for USD equivalent calculation */
    void updateOraclePrice();

private Q_SLOTS:
    /** Address field changed */
    void onAddressChanged();
    /** Amount field changed */
    void onAmountChanged();
    /** Send button clicked */
    void onSendClicked();
    /** Clear all fields */
    void onClearClicked();

private:
    void setupUI();
    void setupAddressSection();
    void setupAmountSection();
    void setupFeeSection();
    void setupButtonSection();
    void connectSignals();
    void updateSendButton();
    void updateUSDEquivalent();
    void updateFeeDisplay();

    bool validateAddress() const;
    bool validateAmount() const;
    bool validateBalance() const;

    QString formatDDAmount(double amount) const;
    QString formatUSDAmount(double amount) const;

    // UI components
    QVBoxLayout* m_mainLayout;

    // Address section
    QFrame* m_addressFrame;
    QGridLayout* m_addressLayout;
    QLabel* m_addressLabel;
    QLineEdit* m_addressEdit;
    QLabel* m_addressValidationLabel;

    // Amount section
    QFrame* m_amountFrame;
    QGridLayout* m_amountLayout;
    QLabel* m_amountLabel;
    QLineEdit* m_amountEdit;
    QLabel* m_amountSuffix;
    QLabel* m_usdEquivalentLabel;
    QLabel* m_usdEquivalentValue;
    QLabel* m_availableBalanceLabel;
    QLabel* m_availableBalanceValue;

    // Fee section
    QFrame* m_feeFrame;
    QGridLayout* m_feeLayout;
    QLabel* m_feeLabel;
    QLabel* m_feeValue;
    QLabel* m_totalLabel;
    QLabel* m_totalValue;

    // Button section
    QFrame* m_buttonFrame;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_sendButton;
    QPushButton* m_clearButton;

    // Validators
    DigiDollarAddressValidator* m_addressValidator;
    AmountValidator* m_amountValidator;

    // Models
    WalletModel* m_walletModel;
    ClientModel* m_clientModel;

    // Data
    double m_availableBalance;
    double m_oraclePrice;
    double m_estimatedFee;
};

/**
 * Validator for DigiDollar addresses (DD, TD, RD prefixes)
 */
class DigiDollarAddressValidator : public QValidator
{
    Q_OBJECT

public:
    explicit DigiDollarAddressValidator(QObject* parent = nullptr);

    QValidator::State validate(QString& input, int& pos) const override;

private:
    bool isValidDDAddress(const QString& address) const;
};

/**
 * Validator for DigiDollar amounts
 */
class AmountValidator : public QValidator
{
    Q_OBJECT

public:
    explicit AmountValidator(double min = 0.00000001, double max = 999999999.99999999, QObject* parent = nullptr);

    QValidator::State validate(QString& input, int& pos) const override;

private:
    double m_min;
    double m_max;
};

#endif // DIGIBYTE_QT_DIGIDOLLARSENDWIDGET_H