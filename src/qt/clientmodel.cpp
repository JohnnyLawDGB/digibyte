<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2011-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/clientmodel.h>

#include <qt/bantablemodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/peertablemodel.h>
#include <qt/peertablesortproxy.h>

#include <clientversion.h>
#include <common/args.h>
#include <common/system.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <net.h>
#include <netbase.h>
<<<<<<< HEAD
#include <util/system.h>
#include <util/threadnames.h>
=======
#include <util/threadnames.h>
#include <util/time.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <validation.h>

#include <stdint.h>
#include <functional>

#include <QDebug>
<<<<<<< HEAD
=======
#include <QMetaObject>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <QThread>
#include <QTimer>

static SteadyClock::time_point g_last_header_tip_update_notification{};
static SteadyClock::time_point g_last_block_tip_update_notification{};

ClientModel::ClientModel(interfaces::Node& node, OptionsModel *_optionsModel, QObject *parent) :
    QObject(parent),
    m_node(node),
    optionsModel(_optionsModel),
<<<<<<< HEAD
    peerTableModel(nullptr),
    banTableModel(nullptr),
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    m_thread(new QThread(this))
{
    cachedBestHeaderHeight = -1;
    cachedBestHeaderTime = -1;

    peerTableModel = new PeerTableModel(m_node, this);
    m_peer_table_sort_proxy = new PeerTableSortProxy(this);
    m_peer_table_sort_proxy->setSourceModel(peerTableModel);

    banTableModel = new BanTableModel(m_node, this);

    QTimer* timer = new QTimer;
    timer->setInterval(MODEL_UPDATE_DELAY);
    connect(timer, &QTimer::timeout, [this] {
        // no locking required at this point
        // the following calls will acquire the required lock
        Q_EMIT mempoolSizeChanged(m_node.getMempoolSize(), m_node.getMempoolDynamicUsage());
        Q_EMIT bytesChanged(m_node.getTotalBytesRecv(), m_node.getTotalBytesSent());
    });
    connect(m_thread, &QThread::finished, timer, &QObject::deleteLater);
    connect(m_thread, &QThread::started, [timer] { timer->start(); });
    // move timer to thread so that polling doesn't disturb main event loop
    timer->moveToThread(m_thread);
    m_thread->start();
    QTimer::singleShot(0, timer, []() {
        util::ThreadRename("qt-clientmodl");
    });

    subscribeToCoreSignals();
}

ClientModel::~ClientModel()
{
    unsubscribeFromCoreSignals();

    m_thread->quit();
    m_thread->wait();
}

int ClientModel::getNumConnections(unsigned int flags) const
{
    ConnectionDirection connections = ConnectionDirection::None;

    if(flags == CONNECTIONS_IN)
        connections = ConnectionDirection::In;
    else if (flags == CONNECTIONS_OUT)
        connections = ConnectionDirection::Out;
    else if (flags == CONNECTIONS_ALL)
        connections = ConnectionDirection::Both;

    return m_node.getNodeCount(connections);
}

int ClientModel::getHeaderTipHeight() const
{
    if (cachedBestHeaderHeight == -1) {
        // make sure we initially populate the cache via a cs_main lock
        // otherwise we need to wait for a tip update
        int height;
        int64_t blockTime;
        if (m_node.getHeaderTip(height, blockTime)) {
            cachedBestHeaderHeight = height;
            cachedBestHeaderTime = blockTime;
        }
    }
    return cachedBestHeaderHeight;
}

int64_t ClientModel::getHeaderTipTime() const
{
    if (cachedBestHeaderTime == -1) {
        int height;
        int64_t blockTime;
        if (m_node.getHeaderTip(height, blockTime)) {
            cachedBestHeaderHeight = height;
            cachedBestHeaderTime = blockTime;
        }
    }
    return cachedBestHeaderTime;
}

int ClientModel::getNumBlocks() const
{
    if (m_cached_num_blocks == -1) {
        m_cached_num_blocks = m_node.getNumBlocks();
    }
    return m_cached_num_blocks;
<<<<<<< HEAD
}

uint256 ClientModel::getBestBlockHash()
{
    uint256 tip{WITH_LOCK(m_cached_tip_mutex, return m_cached_tip_blocks)};

    if (!tip.IsNull()) {
        return tip;
    }

    // Lock order must be: first `cs_main`, then `m_cached_tip_mutex`.
    // The following will lock `cs_main` (and release it), so we must not
    // own `m_cached_tip_mutex` here.
    tip = m_node.getBestBlockHash();

    LOCK(m_cached_tip_mutex);
    // We checked that `m_cached_tip_blocks` is not null above, but then we
    // released the mutex `m_cached_tip_mutex`, so it could have changed in the
    // meantime. Thus, check again.
    if (m_cached_tip_blocks.IsNull()) {
        m_cached_tip_blocks = tip;
    }
    return m_cached_tip_blocks;
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

uint256 ClientModel::getBestBlockHash()
{
    uint256 tip{WITH_LOCK(m_cached_tip_mutex, return m_cached_tip_blocks)};

    if (!tip.IsNull()) {
        return tip;
    }

    // Lock order must be: first `cs_main`, then `m_cached_tip_mutex`.
    // The following will lock `cs_main` (and release it), so we must not
    // own `m_cached_tip_mutex` here.
    tip = m_node.getBestBlockHash();

    LOCK(m_cached_tip_mutex);
    // We checked that `m_cached_tip_blocks` is not null above, but then we
    // released the mutex `m_cached_tip_mutex`, so it could have changed in the
    // meantime. Thus, check again.
    if (m_cached_tip_blocks.IsNull()) {
        m_cached_tip_blocks = tip;
    }
    return m_cached_tip_blocks;
}

BlockSource ClientModel::getBlockSource() const
{
    if (m_node.isLoadingBlocks()) return BlockSource::DISK;
    if (getNumConnections() > 0) return BlockSource::NETWORK;
    return BlockSource::NONE;
}

QString ClientModel::getStatusBarWarnings() const
{
    return QString::fromStdString(m_node.getWarnings().translated);
}

OptionsModel *ClientModel::getOptionsModel()
{
    return optionsModel;
}

PeerTableModel *ClientModel::getPeerTableModel()
{
    return peerTableModel;
}

PeerTableSortProxy* ClientModel::peerTableSortProxy()
{
    return m_peer_table_sort_proxy;
}

BanTableModel *ClientModel::getBanTableModel()
{
    return banTableModel;
}

QString ClientModel::formatFullVersion() const
{
    return QString::fromStdString(FormatFullVersion());
}

QString ClientModel::formatSubVersion() const
{
    return QString::fromStdString(strSubVersion);
}

bool ClientModel::isReleaseVersion() const
{
    return CLIENT_VERSION_IS_RELEASE;
}

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::fromSecsSinceEpoch(GetStartupTime()).toString();
}

QString ClientModel::dataDir() const
{
<<<<<<< HEAD
    return GUIUtil::boostPathToQString(gArgs.GetDataDirNet());
}

QString ClientModel::blocksDir() const
{
    return GUIUtil::boostPathToQString(gArgs.GetBlocksDirPath());
=======
    return GUIUtil::PathToQString(gArgs.GetDataDirNet());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

QString ClientModel::blocksDir() const
{
    return GUIUtil::PathToQString(gArgs.GetBlocksDirPath());
}

void ClientModel::TipChanged(SynchronizationState sync_state, interfaces::BlockTip tip, double verification_progress, SyncType synctype)
{
<<<<<<< HEAD
    // emits signal "showProgress"
    bool invoked = QMetaObject::invokeMethod(clientmodel, "showProgress", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(title)),
                              Q_ARG(int, nProgress));
    assert(invoked);
}

static void NotifyNumConnectionsChanged(ClientModel *clientmodel, int newNumConnections)
{
    // Too noisy: qDebug() << "NotifyNumConnectionsChanged: " + QString::number(newNumConnections);
    bool invoked = QMetaObject::invokeMethod(clientmodel, "updateNumConnections", Qt::QueuedConnection,
                              Q_ARG(int, newNumConnections));
    assert(invoked);
}

static void NotifyNetworkActiveChanged(ClientModel *clientmodel, bool networkActive)
{
    bool invoked = QMetaObject::invokeMethod(clientmodel, "updateNetworkActive", Qt::QueuedConnection,
                              Q_ARG(bool, networkActive));
    assert(invoked);
}

static void NotifyAlertChanged(ClientModel *clientmodel)
{
    qDebug() << "NotifyAlertChanged";
    bool invoked = QMetaObject::invokeMethod(clientmodel, "updateAlert", Qt::QueuedConnection);
    assert(invoked);
}

static void BannedListChanged(ClientModel *clientmodel)
{
    qDebug() << QString("%1: Requesting update for peer banlist").arg(__func__);
    bool invoked = QMetaObject::invokeMethod(clientmodel, "updateBanlist", Qt::QueuedConnection);
    assert(invoked);
}

static void BlockTipChanged(ClientModel* clientmodel, SynchronizationState sync_state, interfaces::BlockTip tip, double verificationProgress, bool fHeader)
{
    if (fHeader) {
        // cache best headers time and height to reduce future cs_main locks
        clientmodel->cachedBestHeaderHeight = tip.block_height;
        clientmodel->cachedBestHeaderTime = tip.block_time;
    } else {
        clientmodel->m_cached_num_blocks = tip.block_height;
        WITH_LOCK(clientmodel->m_cached_tip_mutex, clientmodel->m_cached_tip_blocks = tip.block_hash;);
    }

    // Throttle GUI notifications about (a) blocks during initial sync, and (b) both blocks and headers during reindex.
    const bool throttle = (sync_state != SynchronizationState::POST_INIT && !fHeader) || sync_state == SynchronizationState::INIT_REINDEX;
    const int64_t now = throttle ? GetTimeMillis() : 0;
    int64_t& nLastUpdateNotification = fHeader ? nLastHeaderTipUpdateNotification : nLastBlockTipUpdateNotification;
=======
    if (synctype == SyncType::HEADER_SYNC) {
        // cache best headers time and height to reduce future cs_main locks
        cachedBestHeaderHeight = tip.block_height;
        cachedBestHeaderTime = tip.block_time;
    } else if (synctype == SyncType::BLOCK_SYNC) {
        m_cached_num_blocks = tip.block_height;
        WITH_LOCK(m_cached_tip_mutex, m_cached_tip_blocks = tip.block_hash;);
    }

    // Throttle GUI notifications about (a) blocks during initial sync, and (b) both blocks and headers during reindex.
    const bool throttle = (sync_state != SynchronizationState::POST_INIT && synctype == SyncType::BLOCK_SYNC) || sync_state == SynchronizationState::INIT_REINDEX;
    const auto now{throttle ? SteadyClock::now() : SteadyClock::time_point{}};
    auto& nLastUpdateNotification = synctype != SyncType::BLOCK_SYNC ? g_last_header_tip_update_notification : g_last_block_tip_update_notification;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    if (throttle && now < nLastUpdateNotification + MODEL_UPDATE_DELAY) {
        return;
    }

<<<<<<< HEAD
    bool invoked = QMetaObject::invokeMethod(clientmodel, "numBlocksChanged", Qt::QueuedConnection,
        Q_ARG(int, tip.block_height),
        Q_ARG(QDateTime, QDateTime::fromTime_t(tip.block_time)),
        Q_ARG(double, verificationProgress),
        Q_ARG(bool, fHeader),
        Q_ARG(SynchronizationState, sync_state));
    assert(invoked);
=======
    Q_EMIT numBlocksChanged(tip.block_height, QDateTime::fromSecsSinceEpoch(tip.block_time), verification_progress, synctype, sync_state);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    nLastUpdateNotification = now;
}

void ClientModel::subscribeToCoreSignals()
{
<<<<<<< HEAD
    // Connect signals to client
    m_handler_show_progress = m_node.handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2));
    m_handler_notify_num_connections_changed = m_node.handleNotifyNumConnectionsChanged(std::bind(NotifyNumConnectionsChanged, this, std::placeholders::_1));
    m_handler_notify_network_active_changed = m_node.handleNotifyNetworkActiveChanged(std::bind(NotifyNetworkActiveChanged, this, std::placeholders::_1));
    m_handler_notify_alert_changed = m_node.handleNotifyAlertChanged(std::bind(NotifyAlertChanged, this));
    m_handler_banned_list_changed = m_node.handleBannedListChanged(std::bind(BannedListChanged, this));
    m_handler_notify_block_tip = m_node.handleNotifyBlockTip(std::bind(BlockTipChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, false));
    m_handler_notify_header_tip = m_node.handleNotifyHeaderTip(std::bind(BlockTipChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, true));
=======
    m_handler_show_progress = m_node.handleShowProgress(
        [this](const std::string& title, int progress, [[maybe_unused]] bool resume_possible) {
            Q_EMIT showProgress(QString::fromStdString(title), progress);
        });
    m_handler_notify_num_connections_changed = m_node.handleNotifyNumConnectionsChanged(
        [this](int new_num_connections) {
            Q_EMIT numConnectionsChanged(new_num_connections);
        });
    m_handler_notify_network_active_changed = m_node.handleNotifyNetworkActiveChanged(
        [this](bool network_active) {
            Q_EMIT networkActiveChanged(network_active);
        });
    m_handler_notify_alert_changed = m_node.handleNotifyAlertChanged(
        [this]() {
            qDebug() << "ClientModel: NotifyAlertChanged";
            Q_EMIT alertsChanged(getStatusBarWarnings());
        });
    m_handler_banned_list_changed = m_node.handleBannedListChanged(
        [this]() {
            qDebug() << "ClienModel: Requesting update for peer banlist";
            QMetaObject::invokeMethod(banTableModel, [this] { banTableModel->refresh(); });
        });
    m_handler_notify_block_tip = m_node.handleNotifyBlockTip(
        [this](SynchronizationState sync_state, interfaces::BlockTip tip, double verification_progress) {
            TipChanged(sync_state, tip, verification_progress, SyncType::BLOCK_SYNC);
        });
    m_handler_notify_header_tip = m_node.handleNotifyHeaderTip(
        [this](SynchronizationState sync_state, interfaces::BlockTip tip, bool presync) {
            TipChanged(sync_state, tip, /*verification_progress=*/0.0, presync ? SyncType::HEADER_PRESYNC : SyncType::HEADER_SYNC);
        });
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

void ClientModel::unsubscribeFromCoreSignals()
{
    m_handler_show_progress->disconnect();
    m_handler_notify_num_connections_changed->disconnect();
    m_handler_notify_network_active_changed->disconnect();
    m_handler_notify_alert_changed->disconnect();
    m_handler_banned_list_changed->disconnect();
    m_handler_notify_block_tip->disconnect();
    m_handler_notify_header_tip->disconnect();
}

bool ClientModel::getProxyInfo(std::string& ip_port) const
{
    Proxy ipv4, ipv6;
    if (m_node.getProxy((Network) 1, ipv4) && m_node.getProxy((Network) 2, ipv6)) {
      ip_port = ipv4.proxy.ToStringAddrPort();
      return true;
    }
    return false;
}
