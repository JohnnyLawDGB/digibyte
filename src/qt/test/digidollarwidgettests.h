// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_QT_TEST_DIGIDOLLARWIDGETTESTS_H
#define DIGIBYTE_QT_TEST_DIGIDOLLARWIDGETTESTS_H

#include <QObject>
#include <QTest>

namespace interfaces {
class Node;
}

class DigiDollarWidgetTests : public QObject
{
public:
    explicit DigiDollarWidgetTests(interfaces::Node& node) : m_node(node) {}
    interfaces::Node& m_node;

    Q_OBJECT

private Q_SLOTS:
    void overviewWidgetTests();
    void mintWidgetTests();
    void sendWidgetTests();
    void sendWidgetNoteFieldTests();
    void receiveWidgetTests();
    void redeemWidgetTests();
    void positionsWidgetTests();
    void addressBookTests();
    void transactionsWidgetTests();
    void transactionsWidgetExportTests();
    void privacyTabSetPrivacySlotTests();
    void privacyOverviewMaskTests();
    void privacySendMaskTests();
    void privacyMintMaskTests();
    void privacyRedeemMaskTests();
    void privacyPositionsMaskTests();
    void privacyTransactionsMaskTests();
    void privacySignalPropagationTests();
    void mintValidationUpdatesOnBalanceChange();
    void ddTabRefreshesBalancesOnWalletSignal();
    void walletViewRefreshesDigiDollarPageOnOpen();
    void ddReceivePanelFollowsSelectedRow();
    void darkThemePeerDetailWidgetHasExplicitRule();
};

#endif // DIGIBYTE_QT_TEST_DIGIDOLLARWIDGETTESTS_H
