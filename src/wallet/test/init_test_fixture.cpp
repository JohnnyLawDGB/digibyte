<<<<<<< HEAD
// Copyright (c) 2018-2020 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <fs.h>
#include <univalue.h>
#include <util/check.h>
#include <util/system.h>

#include <wallet/test/init_test_fixture.h>

InitWalletDirTestingSetup::InitWalletDirTestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    m_wallet_client = MakeWalletClient(*m_node.chain, *Assert(m_node.args));

    std::string sep;
    sep += fs::path::preferred_separator;

    m_datadir = gArgs.GetDataDirNet();
=======
// Copyright (c) 2018-2022 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/args.h>
#include <univalue.h>
#include <util/chaintype.h>
#include <util/check.h>
#include <util/fs.h>

#include <fstream>
#include <string>

#include <wallet/test/init_test_fixture.h>

namespace wallet {
InitWalletDirTestingSetup::InitWalletDirTestingSetup(const ChainType chainType) : BasicTestingSetup(chainType)
{
    m_wallet_loader = MakeWalletLoader(*m_node.chain, m_args);

    const auto sep = fs::path::preferred_separator;

    m_datadir = m_args.GetDataDirNet();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    m_cwd = fs::current_path();

    m_walletdir_path_cases["default"] = m_datadir / "wallets";
    m_walletdir_path_cases["custom"] = m_datadir / "my_wallets";
    m_walletdir_path_cases["nonexistent"] = m_datadir / "path_does_not_exist";
    m_walletdir_path_cases["file"] = m_datadir / "not_a_directory.dat";
<<<<<<< HEAD
    m_walletdir_path_cases["trailing"] = m_datadir / "wallets" / sep;
    m_walletdir_path_cases["trailing2"] = m_datadir / "wallets" / sep / sep;
=======
    m_walletdir_path_cases["trailing"] = (m_datadir / "wallets") + sep;
    m_walletdir_path_cases["trailing2"] = (m_datadir / "wallets") + sep + sep;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    fs::current_path(m_datadir);
    m_walletdir_path_cases["relative"] = "wallets";

    fs::create_directories(m_walletdir_path_cases["default"]);
    fs::create_directories(m_walletdir_path_cases["custom"]);
    fs::create_directories(m_walletdir_path_cases["relative"]);
<<<<<<< HEAD
#if BOOST_VERSION >= 107700
    std::ofstream f(BOOST_FILESYSTEM_C_STR(m_walletdir_path_cases["file"]));
#else
    std::ofstream f(m_walletdir_path_cases["file"].BOOST_FILESYSTEM_C_STR);
#endif // BOOST_VERSION >= 107700
=======
    std::ofstream f{m_walletdir_path_cases["file"]};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    f.close();
}

InitWalletDirTestingSetup::~InitWalletDirTestingSetup()
{
<<<<<<< HEAD
    gArgs.LockSettings([&](util::Settings& settings) {
        settings.forced_settings.erase("walletdir");
    });
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    fs::current_path(m_cwd);
}

void InitWalletDirTestingSetup::SetWalletDir(const fs::path& walletdir_path)
{
<<<<<<< HEAD
    gArgs.ForceSetArg("-walletdir", walletdir_path.string());
}
=======
    m_args.ForceSetArg("-walletdir", fs::PathToString(walletdir_path));
}
} // namespace wallet
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
