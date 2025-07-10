// Copyright (c) 2016-2020 The Bitcoin Core developers
// Copyright (c) 2016-2022 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_WALLET_TEST_WALLET_TEST_FIXTURE_H
#define DIGIBYTE_WALLET_TEST_WALLET_TEST_FIXTURE_H

#include <test/util/setup_common.h>

#include <interfaces/chain.h>
#include <interfaces/wallet.h>
#include <node/context.h>
#include <util/chaintype.h>
#include <util/check.h>
#include <wallet/wallet.h>

#include <memory>

namespace wallet {
/** Testing setup and teardown for wallet.
 */
struct WalletTestingSetup : public TestingSetup {
<<<<<<< HEAD
    explicit WalletTestingSetup(const std::string& chainName = CBaseChainParams::MAIN);

    std::unique_ptr<interfaces::WalletClient> m_wallet_client = interfaces::MakeWalletClient(*m_node.chain, *Assert(m_node.args));
=======
    explicit WalletTestingSetup(const ChainType chainType = ChainType::MAIN);
    ~WalletTestingSetup();

    std::unique_ptr<interfaces::WalletLoader> m_wallet_loader;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    CWallet m_wallet;
    std::unique_ptr<interfaces::Handler> m_chain_notifications_handler;
};
} // namespace wallet

#endif // DIGIBYTE_WALLET_TEST_WALLET_TEST_FIXTURE_H
