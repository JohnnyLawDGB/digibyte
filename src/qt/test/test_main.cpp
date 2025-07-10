<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/digibyte-config.h>
#endif

<<<<<<< HEAD
#include <interfaces/node.h>
#include <qt/digibyte.h>
#include <qt/test/apptests.h>
#include <qt/test/rpcnestedtests.h>
#include <qt/test/uritests.h>
#include <test/util/setup_common.h>
=======
#include <interfaces/init.h>
#include <interfaces/node.h>
#include <qt/digibyte.h>
#include <qt/test/apptests.h>
#include <qt/test/optiontests.h>
#include <qt/test/rpcnestedtests.h>
#include <qt/test/uritests.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#ifdef ENABLE_WALLET
#include <qt/test/addressbooktests.h>
#include <qt/test/wallettests.h>
#endif // ENABLE_WALLET

#include <QApplication>
#include <QDebug>
#include <QObject>
#include <QTest>

<<<<<<< HEAD
=======
#include <functional>

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if defined(QT_QPA_PLATFORM_MINIMAL)
Q_IMPORT_PLUGIN(QMinimalIntegrationPlugin);
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_ANDROID)
Q_IMPORT_PLUGIN(QAndroidPlatformIntegrationPlugin)
#endif
#endif

const std::function<void(const std::string&)> G_TEST_LOG_FUN{};
<<<<<<< HEAD
=======

const std::function<std::vector<const char*>()> G_TEST_COMMAND_LINE_ARGUMENTS{};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

// This is all you need to run all the tests
int main(int argc, char* argv[])
{
    // Initialize persistent globals with the testing setup state for sanity.
    // E.g. -datadir in gArgs is set to a temp directory dummy value (instead
    // of defaulting to the default datadir), or globalChainParams is set to
    // regtest params.
    //
    // All tests must use their own testing setup (if needed).
<<<<<<< HEAD
    {
        BasicTestingSetup dummy{CBaseChainParams::REGTEST};
    }

    NodeContext node_context;
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode(&node_context);
    gArgs.ForceSetArg("-listen", "0");
    gArgs.ForceSetArg("-listenonion", "0");
    gArgs.ForceSetArg("-discover", "0");
    gArgs.ForceSetArg("-dnsseed", "0");
    gArgs.ForceSetArg("-fixedseeds", "0");
    gArgs.ForceSetArg("-upnp", "0");
    gArgs.ForceSetArg("-natpmp", "0");
=======
    fs::create_directories([] {
        BasicTestingSetup dummy{ChainType::REGTEST};
        return gArgs.GetDataDirNet() / "blocks";
    }());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    std::unique_ptr<interfaces::Init> init = interfaces::MakeGuiInit(argc, argv);
    gArgs.ForceSetArg("-listen", "0");
    gArgs.ForceSetArg("-listenonion", "0");
    gArgs.ForceSetArg("-discover", "0");
    gArgs.ForceSetArg("-dnsseed", "0");
    gArgs.ForceSetArg("-fixedseeds", "0");
    gArgs.ForceSetArg("-upnp", "0");
    gArgs.ForceSetArg("-natpmp", "0");

    std::string error;
    if (!gArgs.ReadConfigFiles(error, true)) QWARN(error.c_str());

    // Prefer the "minimal" platform for the test instead of the normal default
    // platform ("xcb", "windows", or "cocoa") so tests can't unintentionally
    // interfere with any background GUIs and don't require extra resources.
    #if defined(WIN32)
        if (getenv("QT_QPA_PLATFORM") == nullptr) _putenv_s("QT_QPA_PLATFORM", "minimal");
    #else
<<<<<<< HEAD
        setenv("QT_QPA_PLATFORM", "minimal", /* overwrite */ 0);
    #endif

    // Don't remove this, it's needed to access
    // QApplication:: and QCoreApplication:: in the tests
    DigiByteApplication app;
    app.setNode(*node);
    app.setApplicationName("DigiByte-Qt-test");
=======
        setenv("QT_QPA_PLATFORM", "minimal", 0 /* overwrite */);
    #endif

    DigiByteApplication app;
    app.setApplicationName("DigiByte-Qt-test");
    app.createNode(*init);

    int num_test_failures{0};

    AppTests app_tests(app);
    num_test_failures += QTest::qExec(&app_tests);

    OptionTests options_tests(app.node());
    num_test_failures += QTest::qExec(&options_tests);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    app.node().context()->args = &gArgs;     // Make gArgs available in the NodeContext
    AppTests app_tests(app);
    if (QTest::qExec(&app_tests) != 0) {
        fInvalid = true;
    }
    URITests test1;
<<<<<<< HEAD
    if (QTest::qExec(&test1) != 0) {
        fInvalid = true;
    }
    RPCNestedTests test3(app.node());
    if (QTest::qExec(&test3) != 0) {
        fInvalid = true;
    }
#ifdef ENABLE_WALLET
    WalletTests test5(app.node());
    if (QTest::qExec(&test5) != 0) {
        fInvalid = true;
    }
    AddressBookTests test6(app.node());
    if (QTest::qExec(&test6) != 0) {
        fInvalid = true;
    }
#endif

    return fInvalid;
=======
    num_test_failures += QTest::qExec(&test1);

    RPCNestedTests test3(app.node());
    num_test_failures += QTest::qExec(&test3);

#ifdef ENABLE_WALLET
    WalletTests test5(app.node());
    num_test_failures += QTest::qExec(&test5);

    AddressBookTests test6(app.node());
    num_test_failures += QTest::qExec(&test6);
#endif

    if (num_test_failures) {
        qWarning("\nFailed tests: %d\n", num_test_failures);
    } else {
        qDebug("\nAll tests passed.\n");
    }
    return num_test_failures;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}
