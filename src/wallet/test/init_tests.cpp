<<<<<<< HEAD
// Copyright (c) 2018-2020 The DigiByte Core developers
=======
// Copyright (c) 2018-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

<<<<<<< HEAD
#include <noui.h>
#include <test/util/logging.h>
#include <test/util/setup_common.h>
#include <util/system.h>
#include <wallet/test/init_test_fixture.h>

=======
#include <common/args.h>
#include <noui.h>
#include <test/util/logging.h>
#include <test/util/setup_common.h>
#include <wallet/test/init_test_fixture.h>

namespace wallet {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
BOOST_FIXTURE_TEST_SUITE(init_tests, InitWalletDirTestingSetup)

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_default)
{
    SetWalletDir(m_walletdir_path_cases["default"]);
<<<<<<< HEAD
    bool result = m_wallet_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
=======
    bool result = m_wallet_loader->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = m_args.GetPathArg("-walletdir");
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK_EQUAL(walletdir, expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_custom)
{
    SetWalletDir(m_walletdir_path_cases["custom"]);
<<<<<<< HEAD
    bool result = m_wallet_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
=======
    bool result = m_wallet_loader->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = m_args.GetPathArg("-walletdir");
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["custom"]);
    BOOST_CHECK_EQUAL(walletdir, expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_does_not_exist)
{
    SetWalletDir(m_walletdir_path_cases["nonexistent"]);
    {
        ASSERT_DEBUG_LOG("does not exist");
<<<<<<< HEAD
        bool result = m_wallet_client->verify();
=======
        bool result = m_wallet_loader->verify();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        BOOST_CHECK(result == false);
    }
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_is_not_directory)
{
    SetWalletDir(m_walletdir_path_cases["file"]);
    {
        ASSERT_DEBUG_LOG("is not a directory");
<<<<<<< HEAD
        bool result = m_wallet_client->verify();
=======
        bool result = m_wallet_loader->verify();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        BOOST_CHECK(result == false);
    }
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_is_not_relative)
{
    SetWalletDir(m_walletdir_path_cases["relative"]);
    {
        ASSERT_DEBUG_LOG("is a relative path");
<<<<<<< HEAD
        bool result = m_wallet_client->verify();
=======
        bool result = m_wallet_loader->verify();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        BOOST_CHECK(result == false);
    }
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_no_trailing)
{
    SetWalletDir(m_walletdir_path_cases["trailing"]);
<<<<<<< HEAD
    bool result = m_wallet_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
=======
    bool result = m_wallet_loader->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = m_args.GetPathArg("-walletdir");
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK_EQUAL(walletdir, expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_no_trailing2)
{
    SetWalletDir(m_walletdir_path_cases["trailing2"]);
<<<<<<< HEAD
    bool result = m_wallet_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
=======
    bool result = m_wallet_loader->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = m_args.GetPathArg("-walletdir");
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK_EQUAL(walletdir, expected_path);
}

BOOST_AUTO_TEST_SUITE_END()
<<<<<<< HEAD
=======
} // namespace wallet
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
