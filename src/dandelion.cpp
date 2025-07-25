// Copyright (c) 2014-2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#if defined(HAVE_CONFIG_H)
#include <config/digibyte-config.h>
#endif

#include <net.h>
#include <shutdown.h>
#include <logging.h>
#include <random.h>
#include <validation.h>
#include <common/args.h>

bool CConnman::isDandelionInbound(const CNode* const pnode) const
{
    LOCK(m_nodes_mutex);
    return (std::find(vDandelionInbound.begin(), vDandelionInbound.end(), pnode) != vDandelionInbound.end());
}

bool CConnman::isLocalDandelionDestinationSet() const
{
    LOCK(m_nodes_mutex);
    return (localDandelionDestination != nullptr);
}

bool CConnman::setLocalDandelionDestination()
{
    LOCK(m_nodes_mutex);
    if (!isLocalDandelionDestinationSet()) {
        localDandelionDestination = SelectFromDandelionDestinations();
        LogPrint(BCLog::DANDELION, "Set local Dandelion destination:\n%s", GetDandelionRoutingDataDebugString());
    }
    return isLocalDandelionDestinationSet();
}

CNode* CConnman::getDandelionDestination(CNode* pfrom)
{
    LOCK(m_nodes_mutex);
    for (auto const& e : mDandelionRoutes) {
        if (pfrom == e.first) {
            return e.second;
        }
    }
    CNode* newPto = SelectFromDandelionDestinations();
    if (newPto != nullptr) {
        mDandelionRoutes.insert(std::make_pair(pfrom, newPto));
        LogPrint(BCLog::DANDELION, "Added Dandelion route:\n%s", GetDandelionRoutingDataDebugString());
    }
    return newPto;
}

bool CConnman::localDandelionDestinationPushInventory(const CInv& inv)
{
    LogPrintf("localDandelionDestinationPushInventory called for %s\n", inv.ToString());
    
    // Check if Dandelion is enabled
    if (!gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
        LogPrintf("localDandelionDestinationPushInventory: Dandelion disabled\n");
        return false;
    }
    
    CNode* destination = nullptr;
    {
        LOCK(m_nodes_mutex);
        if (localDandelionDestination) {
            destination = localDandelionDestination;
            LogPrintf("localDandelionDestinationPushInventory: Using existing destination peer=%d\n", destination->GetId());
        } else {
            // Try to set local destination
            localDandelionDestination = SelectFromDandelionDestinations();
            if (localDandelionDestination) {
                LogPrintf("Set local Dandelion destination: peer=%d\n", localDandelionDestination->GetId());
                destination = localDandelionDestination;
            } else {
                // No Dandelion destinations available yet
                LogPrintf("No Dandelion destinations available for %s\n", inv.ToString());
            }
        }
    }
    
    if (destination && m_msgproc) {
        LogPrintf("localDandelionDestinationPushInventory: Calling PushDandelionInventory for peer=%d\n", destination->GetId());
        return m_msgproc->PushDandelionInventory(destination, inv);
    }
    LogPrintf("localDandelionDestinationPushInventory: No destination or msgproc\n");
    return false;
}

bool CConnman::insertDandelionEmbargo(const uint256& hash, std::chrono::microseconds& embargo) {
    LOCK(m_dandelion_embargo_mutex);
    auto pair = mDandelionEmbargo.insert(std::make_pair(hash, embargo));
    return pair.second;
}

bool CConnman::isTxDandelionEmbargoed(const uint256& hash) const
{
    LOCK(m_dandelion_embargo_mutex);
    auto it = mDandelionEmbargo.find(hash);
    if (it == mDandelionEmbargo.end()) {
        return false;
    }
    // Compare current time with stored embargo time
    auto now = GetTime<std::chrono::microseconds>();
    if (now < it->second) {
        // Embargo not yet expired
        return true;
    }
    // Otherwise, embargo expired — best practice is to remove it here or in CheckDandelionEmbargoes()
    // We'll just return false, so that net_processing can proceed and eventually remove it.
    return false;
}


bool CConnman::removeDandelionEmbargo(const uint256& hash)
{
    LOCK(m_dandelion_embargo_mutex);
    bool removed = false;
    for (auto iter = mDandelionEmbargo.begin(); iter != mDandelionEmbargo.end(); )
    {
        if (iter->first == hash) {
            iter = mDandelionEmbargo.erase(iter);
            removed = true;
        } else {
            ++iter;
        }
    }
    return removed;
}


CNode* CConnman::SelectFromDandelionDestinations() const
{
    std::map<CNode*, uint64_t> mDandelionDestinationCounts;
    for (size_t i = 0; i < vDandelionDestination.size(); i++) {
        mDandelionDestinationCounts.insert(std::make_pair(vDandelionDestination.at(i), 0));
    }
    for (auto& e : mDandelionDestinationCounts) {
        for (auto const& f : mDandelionRoutes) {
            if (e.first == f.second) {
                e.second += 1;
            }
        }
    }
    unsigned int minNumConnections = vDandelionInbound.size();
    for (auto const& e : mDandelionDestinationCounts) {
        if (e.second < minNumConnections) {
            minNumConnections = e.second;
        }
    }
    std::vector<CNode*> candidateDestinations;
    for (auto const& e : mDandelionDestinationCounts) {
        if (e.second == minNumConnections) {
            candidateDestinations.push_back(e.first);
        }
    }
    FastRandomContext rng;
    CNode* dandelionDestination = nullptr;
    if (candidateDestinations.size() > 0) {
        dandelionDestination = candidateDestinations.at(rng.randrange(candidateDestinations.size()));
    }
    return dandelionDestination;
}

void CConnman::CloseDandelionConnections(const CNode* const pnode)
{
    AssertLockHeld(m_nodes_mutex);
    // Remove pnode from vDandelionInbound, if present
    for (auto iter = vDandelionInbound.begin(); iter != vDandelionInbound.end();) {
        if (*iter == pnode) {
            iter = vDandelionInbound.erase(iter);
        } else {
            iter++;
        }
    }
    // Remove pnode from vDandelionOutbound, if present
    for (auto iter = vDandelionOutbound.begin(); iter != vDandelionOutbound.end();) {
        if (*iter == pnode) {
            iter = vDandelionOutbound.erase(iter);
        } else {
            iter++;
        }
    }
    // Remove pnode from vDandelionDestination, if present
    bool isDandelionDestination = false;
    for (auto iter = vDandelionDestination.begin(); iter != vDandelionDestination.end();) {
        if (*iter == pnode) {
            isDandelionDestination = true;
            iter = vDandelionDestination.erase(iter);
        } else {
            iter++;
        }
    }
    // Generate a replacement Dandelion destination, if necessary
    if (isDandelionDestination) {
        // Gather a vector of candidate replacements (outbound peers that are not already destinations)
        std::vector<CNode*> candidateReplacements;
        for (auto iteri = vDandelionOutbound.begin(); iteri != vDandelionOutbound.end();) {
            bool eligibleCandidate = true;
            for (auto iterj = vDandelionDestination.begin(); iterj != vDandelionDestination.end();) {
                if (*iteri == *iterj) {
                    eligibleCandidate = false;
                    iterj = vDandelionDestination.end();
                } else {
                    iterj++;
                }
            }
            if (eligibleCandidate) {
                candidateReplacements.push_back(*iteri);
            }
            iteri++;
        }
        // Select a candidate to be the replacement destination
        FastRandomContext rng;
        CNode* replacementDestination = nullptr;
        if (candidateReplacements.size() > 0) {
            replacementDestination = candidateReplacements.at(rng.randrange(candidateReplacements.size()));
        }
        if (replacementDestination != nullptr) {
            vDandelionDestination.push_back(replacementDestination);
        }
    }
    // Generate a replacement pnode, to be used if necessary
    CNode* newPto = SelectFromDandelionDestinations();
    // Remove from mDandelionRoutes, if present; if destination, try to replace
    for (auto iter = mDandelionRoutes.begin(); iter != mDandelionRoutes.end();) {
        if (iter->first == pnode) {
            iter = mDandelionRoutes.erase(iter);
        } else if (iter->second == pnode) {
            if (newPto == nullptr) {
                iter = mDandelionRoutes.erase(iter);
            } else {
                iter->second = newPto;
                iter++;
            }
        } else {
            iter++;
        }
    }
    // Replace localDandelionDestination if equal to pnode
    if (localDandelionDestination == pnode) {
        localDandelionDestination = newPto;
    }
}

std::string CConnman::GetDandelionRoutingDataDebugString() const
{
    std::string dandelionRoutingDataDebugString = "";
    dandelionRoutingDataDebugString.append("  vDandelionInbound: ");
    for (auto const& e : vDandelionInbound) {
        dandelionRoutingDataDebugString.append(std::to_string(e->GetId()) + " ");
    }
    dandelionRoutingDataDebugString.append("\n  vDandelionOutbound: ");
    for (auto const& e : vDandelionOutbound) {
        dandelionRoutingDataDebugString.append(std::to_string(e->GetId()) + " ");
    }
    dandelionRoutingDataDebugString.append("\n  vDandelionDestination: ");
    for (auto const& e : vDandelionDestination) {
        dandelionRoutingDataDebugString.append(std::to_string(e->GetId()) + " ");
    }
    dandelionRoutingDataDebugString.append("\n  mDandelionRoutes: ");
    for (auto const& e : mDandelionRoutes) {
        dandelionRoutingDataDebugString.append("(" + std::to_string(e.first->GetId()) + "," + std::to_string(e.second->GetId()) + ") ");
    }
    dandelionRoutingDataDebugString.append("\n  localDandelionDestination: ");
    if (!localDandelionDestination) {
        dandelionRoutingDataDebugString.append("nullptr");
    } else {
        dandelionRoutingDataDebugString.append(std::to_string(localDandelionDestination->GetId()));
    }
    dandelionRoutingDataDebugString.append("\n");
    return dandelionRoutingDataDebugString;
}

void CConnman::DandelionShuffle()
{
    {
        // Lock node pointers
        LOCK(m_nodes_mutex);
        // Dandelion debug message
        LogPrint(BCLog::DANDELION, "Before Dandelion shuffle:\n%s", GetDandelionRoutingDataDebugString());
        // Iterate through mDandelionRoutes to facilitate bookkeeping
        for (auto iter = mDandelionRoutes.begin(); iter != mDandelionRoutes.end();) {
            iter = mDandelionRoutes.erase(iter);
        }
        // Set localDandelionDestination to nulltpr and perform bookkeeping
        if (localDandelionDestination != nullptr) {
            localDandelionDestination = nullptr;
        }
        // Clear vDandelionDestination
        //  (bookkeeping already done while iterating through mDandelionRoutes)
        vDandelionDestination.clear();
        // Repopulate vDandelionDestination
        while (vDandelionDestination.size() < DANDELION_MAX_DESTINATIONS && vDandelionDestination.size() < vDandelionOutbound.size()) {
            std::vector<CNode*> candidateDestinations;
            for (auto iteri = vDandelionOutbound.begin(); iteri != vDandelionOutbound.end();) {
                bool eligibleCandidate = true;
                for (auto iterj = vDandelionDestination.begin(); iterj != vDandelionDestination.end();) {
                    if (*iteri == *iterj) {
                        eligibleCandidate = false;
                        iterj = vDandelionDestination.end();
                    } else {
                        iterj++;
                    }
                }
                if (eligibleCandidate) {
                    candidateDestinations.push_back(*iteri);
                }
                iteri++;
            }
            FastRandomContext rng;
            if (candidateDestinations.size() > 0) {
                vDandelionDestination.push_back(candidateDestinations.at(rng.randrange(candidateDestinations.size())));
            } else {
                break;
            }
        }
        // Generate new routes
        for (auto pnode : vDandelionInbound) {
            CNode* pto = SelectFromDandelionDestinations();
            if (pto) {
                mDandelionRoutes.insert(std::make_pair(pnode, pto));
            }
        }
        localDandelionDestination = SelectFromDandelionDestinations();
        // Dandelion debug message
        LogPrint(BCLog::DANDELION, "After Dandelion shuffle:\n%s", GetDandelionRoutingDataDebugString());
    }
}

bool CConnman::usingDandelion() const
{
    LOCK(m_nodes_mutex);
    return vDandelionDestination.size() > 0;
}

CNode* CConnman::getLocalDandelionDestination() const
{
    LOCK(m_nodes_mutex);
    return localDandelionDestination;
}

void CConnman::AddDandelionDestination(CNode* pnode)
{
    // Only process if Dandelion is enabled
    if (!gArgs.GetBoolArg("-dandelion", DEFAULT_DANDELION)) {
        LogPrint(BCLog::DANDELION, "AddDandelionDestination: Dandelion disabled, not adding peer %d\n", pnode->GetId());
        return;
    }
    
    LOCK(m_nodes_mutex);
    
    // Check if already in vDandelionDestination
    if (std::find(vDandelionDestination.begin(), vDandelionDestination.end(), pnode) != vDandelionDestination.end()) {
        LogPrint(BCLog::DANDELION, "AddDandelionDestination: Peer %d already in destinations\n", pnode->GetId());
        return; // Already added
    }
    
    // Check if peer is in inbound or outbound list
    bool isInbound = std::find(vDandelionInbound.begin(), vDandelionInbound.end(), pnode) != vDandelionInbound.end();
    bool isOutbound = std::find(vDandelionOutbound.begin(), vDandelionOutbound.end(), pnode) != vDandelionOutbound.end();
    
    // If not in either list, add to the appropriate list based on connection type
    if (!isInbound && !isOutbound) {
        if (pnode->IsInboundConn()) {
            vDandelionInbound.push_back(pnode);
            isInbound = true;
            LogPrint(BCLog::DANDELION, "Added peer %d to vDandelionInbound during discovery\n", pnode->GetId());
        } else {
            vDandelionOutbound.push_back(pnode);
            isOutbound = true;
            LogPrint(BCLog::DANDELION, "Added peer %d to vDandelionOutbound during discovery\n", pnode->GetId());
        }
    }
    
    LogPrint(BCLog::DANDELION, "AddDandelionDestination: Peer %d - inbound=%d, outbound=%d, current destinations=%d\n", 
             pnode->GetId(), isInbound, isOutbound, vDandelionDestination.size());
    
    // Only add if we haven't reached the maximum destinations
    if (vDandelionDestination.size() < DANDELION_MAX_DESTINATIONS) {
        vDandelionDestination.push_back(pnode);
        LogPrint(BCLog::DANDELION, "Added peer %d to Dandelion destinations (total: %d)\n", 
                 pnode->GetId(), vDandelionDestination.size());
        
        // If this is the first destination and we don't have a local destination set, set it
        if (!localDandelionDestination) {
            localDandelionDestination = pnode;
            LogPrint(BCLog::DANDELION, "Set peer %d as local Dandelion destination\n", pnode->GetId());
        }
    } else {
        LogPrint(BCLog::DANDELION, "AddDandelionDestination: Max destinations reached (%d), not adding peer %d\n", 
                 DANDELION_MAX_DESTINATIONS, pnode->GetId());
    }
}

void CConnman::ThreadDandelionShuffle()
{
    // Start Dandelion shuffling immediately - no need to wait for IBD
    // Give the node a few seconds to establish some connections first
    if (!interruptNet.sleep_for(std::chrono::seconds(5))) {
        return;
    }

    auto now = GetTime<std::chrono::milliseconds>();
    auto nNextDandelionShuffle = GetExponentialRand(now, DANDELION_SHUFFLE_INTERVAL);

    while (!ShutdownRequested()) {
        now = GetTime<std::chrono::milliseconds>();
        if (now > nNextDandelionShuffle) {
            DandelionShuffle();
            nNextDandelionShuffle = GetExponentialRand(now, DANDELION_SHUFFLE_INTERVAL);
        }
        if (ShutdownRequested()) {
            return;
        }
        if (!interruptNet.sleep_for(std::chrono::milliseconds(100))) {
            return;
        }
    }
}
