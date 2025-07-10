<<<<<<< HEAD
// Copyright (c) 2020 The DigiByte Core developers
=======
// Copyright (c) 2020-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_DEPLOYMENTSTATUS_H
#define DIGIBYTE_DEPLOYMENTSTATUS_H

#include <chain.h>
#include <versionbits.h>

#include <limits>

<<<<<<< HEAD
/** Global cache for versionbits deployment status */
extern VersionBitsCache g_versionbitscache;

/** Determine if a deployment is active for the next block */
inline bool DeploymentActiveAfter(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::BuriedDeployment dep)
=======
/** Determine if a deployment is active for the next block */
inline bool DeploymentActiveAfter(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::BuriedDeployment dep, [[maybe_unused]] VersionBitsCache& versionbitscache)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    assert(Consensus::ValidDeployment(dep));
    return (pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1) >= params.DeploymentHeight(dep);
}

<<<<<<< HEAD
inline bool DeploymentActiveAfter(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::DeploymentPos dep)
{
    assert(Consensus::ValidDeployment(dep));
    return ThresholdState::ACTIVE == g_versionbitscache.State(pindexPrev, params, dep);
}

/** Determine if a deployment is active for this block */
inline bool DeploymentActiveAt(const CBlockIndex& index, const Consensus::Params& params, Consensus::BuriedDeployment dep)
=======
inline bool DeploymentActiveAfter(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::DeploymentPos dep, VersionBitsCache& versionbitscache)
{
    assert(Consensus::ValidDeployment(dep));
    return ThresholdState::ACTIVE == versionbitscache.State(pindexPrev, params, dep);
}

/** Determine if a deployment is active for this block */
inline bool DeploymentActiveAt(const CBlockIndex& index, const Consensus::Params& params, Consensus::BuriedDeployment dep, [[maybe_unused]] VersionBitsCache& versionbitscache)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    assert(Consensus::ValidDeployment(dep));
    return index.nHeight >= params.DeploymentHeight(dep);
}

<<<<<<< HEAD
inline bool DeploymentActiveAt(const CBlockIndex& index, const Consensus::Params& params, Consensus::DeploymentPos dep)
{
    assert(Consensus::ValidDeployment(dep));
    return DeploymentActiveAfter(index.pprev, params, dep);
=======
inline bool DeploymentActiveAt(const CBlockIndex& index, const Consensus::Params& params, Consensus::DeploymentPos dep, VersionBitsCache& versionbitscache)
{
    assert(Consensus::ValidDeployment(dep));
    return DeploymentActiveAfter(index.pprev, params, dep, versionbitscache);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

/** Determine if a deployment is enabled (can ever be active) */
inline bool DeploymentEnabled(const Consensus::Params& params, Consensus::BuriedDeployment dep)
{
    assert(Consensus::ValidDeployment(dep));
    return params.DeploymentHeight(dep) != std::numeric_limits<int>::max();
}

inline bool DeploymentEnabled(const Consensus::Params& params, Consensus::DeploymentPos dep)
{
    assert(Consensus::ValidDeployment(dep));
    return params.vDeployments[dep].nStartTime != Consensus::BIP9Deployment::NEVER_ACTIVE;
}

#endif // DIGIBYTE_DEPLOYMENTSTATUS_H
