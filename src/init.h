// Copyright (c) 2009-2010 Satoshi Nakamoto
<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_INIT_H
#define DIGIBYTE_INIT_H

#include <any>
#include <memory>
#include <string>

//! Default value for -daemon option
static constexpr bool DEFAULT_DAEMON = false;
//! Default value for -daemonwait option
static constexpr bool DEFAULT_DAEMONWAIT = false;

<<<<<<< HEAD
extern int miningAlgo;

class ArgsManager;
struct NodeContext;
namespace interfaces {
struct BlockAndHeaderTipInfo;
}

/** Interrupt threads */
void Interrupt(NodeContext& node);
void Shutdown(NodeContext& node);
=======
class ArgsManager;
namespace interfaces {
struct BlockAndHeaderTipInfo;
}
namespace kernel {
struct Context;
}
namespace node {
struct NodeContext;
} // namespace node

/** Interrupt threads */
void Interrupt(node::NodeContext& node);
void Shutdown(node::NodeContext& node);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
//!Initialize the logging infrastructure
void InitLogging(const ArgsManager& args);
//!Parameter interaction: change current parameters depending on various rules
void InitParameterInteraction(ArgsManager& args);

/** Initialize digibyte core: Basic context setup.
 *  @note This can be done before daemonization. Do not call Shutdown() if this function fails.
 *  @pre Parameters should be parsed and config file should be read.
 */
<<<<<<< HEAD
bool AppInitBasicSetup(const ArgsManager& args);
=======
bool AppInitBasicSetup(const ArgsManager& args, std::atomic<int>& exit_status);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
/**
 * Initialization: parameter interaction.
 * @note This can be done before daemonization. Do not call Shutdown() if this function fails.
 * @pre Parameters should be parsed and config file should be read, AppInitBasicSetup should have been called.
 */
bool AppInitParameterInteraction(const ArgsManager& args);
/**
 * Initialization sanity checks.
 * @note This can be done before daemonization. Do not call Shutdown() if this function fails.
 * @pre Parameters should be parsed and config file should be read, AppInitParameterInteraction should have been called.
 */
bool AppInitSanityChecks(const kernel::Context& kernel);
/**
 * Lock digibyte core data directory.
 * @note This should only be done after daemonization. Do not call Shutdown() if this function fails.
 * @pre Parameters should be parsed and config file should be read, AppInitSanityChecks should have been called.
 */
bool AppInitLockDataDirectory();
/**
 * Initialize node and wallet interface pointers. Has no prerequisites or side effects besides allocating memory.
 */
<<<<<<< HEAD
bool AppInitInterfaces(NodeContext& node);
=======
bool AppInitInterfaces(node::NodeContext& node);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
/**
 * DigiByte core main initialization.
 * @note This should only be done after daemonization. Call Shutdown() if this function fails.
 * @pre Parameters should be parsed and config file should be read, AppInitLockDataDirectory should have been called.
 */
<<<<<<< HEAD
bool AppInitMain(NodeContext& node, interfaces::BlockAndHeaderTipInfo* tip_info = nullptr);
=======
bool AppInitMain(node::NodeContext& node, interfaces::BlockAndHeaderTipInfo* tip_info = nullptr);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

/**
 * Register all arguments with the ArgsManager
 */
void SetupServerArgs(ArgsManager& argsman);

/** Validates requirements to run the indexes and spawns each index initial sync thread */
bool StartIndexBackgroundSync(node::NodeContext& node);

#endif // DIGIBYTE_INIT_H
