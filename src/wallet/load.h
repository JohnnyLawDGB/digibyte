// Copyright (c) 2009-2010 Satoshi Nakamoto
<<<<<<< HEAD
// Copyright (c) 2009-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2021 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_WALLET_LOAD_H
#define DIGIBYTE_WALLET_LOAD_H

#include <string>
#include <vector>

class ArgsManager;
class CScheduler;

namespace interfaces {
class Chain;
} // namespace interfaces

<<<<<<< HEAD
//! Responsible for reading and validating the -wallet arguments and verifying the wallet database.
bool VerifyWallets(interfaces::Chain& chain);

//! Load wallet databases.
bool LoadWallets(interfaces::Chain& chain);

//! Complete startup of wallets.
void StartWallets(CScheduler& scheduler, const ArgsManager& args);

//! Flush all wallets in preparation for shutdown.
void FlushWallets();

//! Stop all wallets. Wallets will be flushed first.
void StopWallets();

//! Close all wallets.
void UnloadWallets();
=======
namespace wallet {
struct WalletContext;

//! Responsible for reading and validating the -wallet arguments and verifying the wallet database.
bool VerifyWallets(WalletContext& context);

//! Load wallet databases.
bool LoadWallets(WalletContext& context);

//! Complete startup of wallets.
void StartWallets(WalletContext& context, CScheduler& scheduler);

//! Flush all wallets in preparation for shutdown.
void FlushWallets(WalletContext& context);

//! Stop all wallets. Wallets will be flushed first.
void StopWallets(WalletContext& context);

//! Close all wallets.
void UnloadWallets(WalletContext& context);
} // namespace wallet
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#endif // DIGIBYTE_WALLET_LOAD_H
