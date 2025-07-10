<<<<<<< HEAD
// Copyright (c) 2021 The DigiByte Core developers
=======
// Copyright (c) 2021-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txorphanage.h>

#include <consensus/validation.h>
#include <logging.h>
#include <policy/policy.h>

#include <cassert>

/** Expiration time for orphan transactions in seconds */
static constexpr int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;
/** Minimum time between orphan transactions expire time checks in seconds */
static constexpr int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;

<<<<<<< HEAD
RecursiveMutex g_cs_orphans;

bool TxOrphanage::AddTx(const CTransactionRef& tx, NodeId peer)
{
    AssertLockHeld(g_cs_orphans);

    const uint256& hash = tx->GetHash();
=======

bool TxOrphanage::AddTx(const CTransactionRef& tx, NodeId peer)
{
    LOCK(m_mutex);

    const uint256& hash = tx->GetHash();
    const uint256& wtxid = tx->GetWitnessHash();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    if (m_orphans.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 100 orphans, each of which is at most 100,000 bytes big is
    // at most 10 megabytes of orphans and somewhat more byprev index (in the worst case):
    unsigned int sz = GetTransactionWeight(*tx);
    if (sz > MAX_STANDARD_TX_WEIGHT)
    {
<<<<<<< HEAD
        LogPrint(BCLog::MEMPOOL, "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
=======
        LogPrint(BCLog::TXPACKAGES, "ignoring large orphan tx (size: %u, txid: %s, wtxid: %s)\n", sz, hash.ToString(), wtxid.ToString());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        return false;
    }

    auto ret = m_orphans.emplace(hash, OrphanTx{tx, peer, GetTime() + ORPHAN_TX_EXPIRE_TIME, m_orphan_list.size()});
    assert(ret.second);
    m_orphan_list.push_back(ret.first);
    // Allow for lookups in the orphan pool by wtxid, as well as txid
    m_wtxid_to_orphan_it.emplace(tx->GetWitnessHash(), ret.first);
    for (const CTxIn& txin : tx->vin) {
        m_outpoint_to_orphan_it[txin.prevout].insert(ret.first);
    }

<<<<<<< HEAD
    LogPrint(BCLog::MEMPOOL, "stored orphan tx %s (mapsz %u outsz %u)\n", hash.ToString(),
=======
    LogPrint(BCLog::TXPACKAGES, "stored orphan tx %s (wtxid=%s) (mapsz %u outsz %u)\n", hash.ToString(), wtxid.ToString(),
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
             m_orphans.size(), m_outpoint_to_orphan_it.size());
    return true;
}

int TxOrphanage::EraseTx(const uint256& txid)
{
<<<<<<< HEAD
    AssertLockHeld(g_cs_orphans);
=======
    LOCK(m_mutex);
    return EraseTxNoLock(txid);
}

int TxOrphanage::EraseTxNoLock(const uint256& txid)
{
    AssertLockHeld(m_mutex);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    std::map<uint256, OrphanTx>::iterator it = m_orphans.find(txid);
    if (it == m_orphans.end())
        return 0;
    for (const CTxIn& txin : it->second.tx->vin)
    {
        auto itPrev = m_outpoint_to_orphan_it.find(txin.prevout);
        if (itPrev == m_outpoint_to_orphan_it.end())
            continue;
        itPrev->second.erase(it);
        if (itPrev->second.empty())
            m_outpoint_to_orphan_it.erase(itPrev);
    }

    size_t old_pos = it->second.list_pos;
    assert(m_orphan_list[old_pos] == it);
    if (old_pos + 1 != m_orphan_list.size()) {
        // Unless we're deleting the last entry in m_orphan_list, move the last
        // entry to the position we're deleting.
        auto it_last = m_orphan_list.back();
        m_orphan_list[old_pos] = it_last;
        it_last->second.list_pos = old_pos;
    }
<<<<<<< HEAD
=======
    const auto& wtxid = it->second.tx->GetWitnessHash();
    LogPrint(BCLog::TXPACKAGES, "   removed orphan tx %s (wtxid=%s)\n", txid.ToString(), wtxid.ToString());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    m_orphan_list.pop_back();
    m_wtxid_to_orphan_it.erase(it->second.tx->GetWitnessHash());

    m_orphans.erase(it);
    return 1;
}

void TxOrphanage::EraseForPeer(NodeId peer)
{
<<<<<<< HEAD
    AssertLockHeld(g_cs_orphans);
=======
    LOCK(m_mutex);

    m_peer_work_set.erase(peer);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    int nErased = 0;
    std::map<uint256, OrphanTx>::iterator iter = m_orphans.begin();
    while (iter != m_orphans.end())
    {
        std::map<uint256, OrphanTx>::iterator maybeErase = iter++; // increment to avoid iterator becoming invalid
        if (maybeErase->second.fromPeer == peer)
        {
<<<<<<< HEAD
            nErased += EraseTx(maybeErase->second.tx->GetHash());
        }
    }
    if (nErased > 0) LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx from peer=%d\n", nErased, peer);
}

unsigned int TxOrphanage::LimitOrphans(unsigned int max_orphans)
{
    AssertLockHeld(g_cs_orphans);
=======
            nErased += EraseTxNoLock(maybeErase->second.tx->GetHash());
        }
    }
    if (nErased > 0) LogPrint(BCLog::TXPACKAGES, "Erased %d orphan tx from peer=%d\n", nErased, peer);
}

void TxOrphanage::LimitOrphans(unsigned int max_orphans)
{
    LOCK(m_mutex);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    unsigned int nEvicted = 0;
    static int64_t nNextSweep;
    int64_t nNow = GetTime();
    if (nNextSweep <= nNow) {
        // Sweep out expired orphan pool entries:
        int nErased = 0;
        int64_t nMinExpTime = nNow + ORPHAN_TX_EXPIRE_TIME - ORPHAN_TX_EXPIRE_INTERVAL;
        std::map<uint256, OrphanTx>::iterator iter = m_orphans.begin();
        while (iter != m_orphans.end())
        {
            std::map<uint256, OrphanTx>::iterator maybeErase = iter++;
            if (maybeErase->second.nTimeExpire <= nNow) {
<<<<<<< HEAD
                nErased += EraseTx(maybeErase->second.tx->GetHash());
=======
                nErased += EraseTxNoLock(maybeErase->second.tx->GetHash());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            } else {
                nMinExpTime = std::min(maybeErase->second.nTimeExpire, nMinExpTime);
            }
        }
        // Sweep again 5 minutes after the next entry that expires in order to batch the linear scan.
        nNextSweep = nMinExpTime + ORPHAN_TX_EXPIRE_INTERVAL;
<<<<<<< HEAD
        if (nErased > 0) LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx due to expiration\n", nErased);
=======
        if (nErased > 0) LogPrint(BCLog::TXPACKAGES, "Erased %d orphan tx due to expiration\n", nErased);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    FastRandomContext rng;
    while (m_orphans.size() > max_orphans)
    {
        // Evict a random orphan:
        size_t randompos = rng.randrange(m_orphan_list.size());
<<<<<<< HEAD
        EraseTx(m_orphan_list[randompos]->first);
        ++nEvicted;
    }
    return nEvicted;
}

void TxOrphanage::AddChildrenToWorkSet(const CTransaction& tx, std::set<uint256>& orphan_work_set) const
{
    AssertLockHeld(g_cs_orphans);
=======
        EraseTxNoLock(m_orphan_list[randompos]->first);
        ++nEvicted;
    }
    if (nEvicted > 0) LogPrint(BCLog::TXPACKAGES, "orphanage overflow, removed %u tx\n", nEvicted);
}

void TxOrphanage::AddChildrenToWorkSet(const CTransaction& tx)
{
    LOCK(m_mutex);


>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const auto it_by_prev = m_outpoint_to_orphan_it.find(COutPoint(tx.GetHash(), i));
        if (it_by_prev != m_outpoint_to_orphan_it.end()) {
            for (const auto& elem : it_by_prev->second) {
<<<<<<< HEAD
                orphan_work_set.insert(elem->first);
=======
                // Get this source peer's work set, emplacing an empty set if it didn't exist
                // (note: if this peer wasn't still connected, we would have removed the orphan tx already)
                std::set<uint256>& orphan_work_set = m_peer_work_set.try_emplace(elem->second.fromPeer).first->second;
                // Add this tx to the work set
                orphan_work_set.insert(elem->first);
                LogPrint(BCLog::TXPACKAGES, "added %s (wtxid=%s) to peer %d workset\n",
                         tx.GetHash().ToString(), tx.GetWitnessHash().ToString(), elem->second.fromPeer);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            }
        }
    }
}

bool TxOrphanage::HaveTx(const GenTxid& gtxid) const
{
<<<<<<< HEAD
    LOCK(g_cs_orphans);
=======
    LOCK(m_mutex);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    if (gtxid.IsWtxid()) {
        return m_wtxid_to_orphan_it.count(gtxid.GetHash());
    } else {
        return m_orphans.count(gtxid.GetHash());
    }
}

<<<<<<< HEAD
std::pair<CTransactionRef, NodeId> TxOrphanage::GetTx(const uint256& txid) const
{
    AssertLockHeld(g_cs_orphans);

    const auto it = m_orphans.find(txid);
    if (it == m_orphans.end()) return {nullptr, -1};
    return {it->second.tx, it->second.fromPeer};
=======
CTransactionRef TxOrphanage::GetTxToReconsider(NodeId peer)
{
    LOCK(m_mutex);

    auto work_set_it = m_peer_work_set.find(peer);
    if (work_set_it != m_peer_work_set.end()) {
        auto& work_set = work_set_it->second;
        while (!work_set.empty()) {
            uint256 txid = *work_set.begin();
            work_set.erase(work_set.begin());

            const auto orphan_it = m_orphans.find(txid);
            if (orphan_it != m_orphans.end()) {
                return orphan_it->second.tx;
            }
        }
    }
    return nullptr;
}

bool TxOrphanage::HaveTxToReconsider(NodeId peer)
{
    LOCK(m_mutex);

    auto work_set_it = m_peer_work_set.find(peer);
    if (work_set_it != m_peer_work_set.end()) {
        auto& work_set = work_set_it->second;
        return !work_set.empty();
    }
    return false;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

void TxOrphanage::EraseForBlock(const CBlock& block)
{
<<<<<<< HEAD
    LOCK(g_cs_orphans);
=======
    LOCK(m_mutex);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    std::vector<uint256> vOrphanErase;

    for (const CTransactionRef& ptx : block.vtx) {
        const CTransaction& tx = *ptx;

        // Which orphan pool entries must we evict?
        for (const auto& txin : tx.vin) {
            auto itByPrev = m_outpoint_to_orphan_it.find(txin.prevout);
            if (itByPrev == m_outpoint_to_orphan_it.end()) continue;
            for (auto mi = itByPrev->second.begin(); mi != itByPrev->second.end(); ++mi) {
                const CTransaction& orphanTx = *(*mi)->second.tx;
                const uint256& orphanHash = orphanTx.GetHash();
                vOrphanErase.push_back(orphanHash);
            }
        }
    }

    // Erase orphan transactions included or precluded by this block
    if (vOrphanErase.size()) {
        int nErased = 0;
        for (const uint256& orphanHash : vOrphanErase) {
<<<<<<< HEAD
            nErased += EraseTx(orphanHash);
        }
        LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx included or conflicted by block\n", nErased);
=======
            nErased += EraseTxNoLock(orphanHash);
        }
        LogPrint(BCLog::TXPACKAGES, "Erased %d orphan tx included or conflicted by block\n", nErased);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
}
