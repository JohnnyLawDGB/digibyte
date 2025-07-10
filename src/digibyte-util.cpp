<<<<<<< HEAD
// Copyright (c) 2009-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/digibyte-config.h>
#endif

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <clientversion.h>
<<<<<<< HEAD
#include <core_io.h>
#include <streams.h>
#include <util/system.h>
#include <util/translation.h>
=======
#include <common/args.h>
#include <common/system.h>
#include <compat/compat.h>
#include <core_io.h>
#include <streams.h>
#include <util/exception.h>
#include <util/strencodings.h>
#include <util/translation.h>
#include <version.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>

<<<<<<< HEAD
#include <boost/algorithm/string.hpp>

=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
static const int CONTINUE_EXECUTION=-1;

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

<<<<<<< HEAD
static void SetupDigiByteUtilArgs(ArgsManager &argsman)
=======
static void SetupBitcoinUtilArgs(ArgsManager &argsman)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    SetupHelpOptions(argsman);

    argsman.AddArg("-version", "Print version and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);

    argsman.AddCommand("grind", "Perform proof of work on hex header string");

    SetupChainParamsBaseOptions(argsman);
}

// This function returns either one of EXIT_ codes when it's expected to stop the process or
// CONTINUE_EXECUTION when it's expected to continue further.
static int AppInitUtil(ArgsManager& args, int argc, char* argv[])
{
<<<<<<< HEAD
    SetupDigiByteUtilArgs(args);
=======
    SetupBitcoinUtilArgs(args);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    std::string error;
    if (!args.ParseParameters(argc, argv, error)) {
        tfm::format(std::cerr, "Error parsing command line arguments: %s\n", error);
        return EXIT_FAILURE;
    }

    if (HelpRequested(args) || args.IsArgSet("-version")) {
        // First part of help message is specific to this utility
        std::string strUsage = PACKAGE_NAME " digibyte-util utility version " + FormatFullVersion() + "\n";
<<<<<<< HEAD
        if (!args.IsArgSet("-version")) {
=======

        if (args.IsArgSet("-version")) {
            strUsage += FormatParagraph(LicenseInfo());
        } else {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            strUsage += "\n"
                "Usage:  digibyte-util [options] [commands]  Do stuff\n";
            strUsage += "\n" + args.GetHelpMessage();
        }

        tfm::format(std::cout, "%s", strUsage);

        if (argc < 2) {
            tfm::format(std::cerr, "Error: too few parameters\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    // Check for chain settings (Params() calls are only valid after this clause)
    try {
<<<<<<< HEAD
        SelectParams(args.GetChainName());
=======
        SelectParams(args.GetChainType());
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    } catch (const std::exception& e) {
        tfm::format(std::cerr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return CONTINUE_EXECUTION;
}

<<<<<<< HEAD
static void grind_task(uint32_t nBits, CBlockHeader& header_orig, uint32_t offset, uint32_t step, std::atomic<bool>& found)
=======
static void grind_task(uint32_t nBits, CBlockHeader header, uint32_t offset, uint32_t step, std::atomic<bool>& found, uint32_t& proposed_nonce)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    arith_uint256 target;
    bool neg, over;
    target.SetCompact(nBits, &neg, &over);
    if (target == 0 || neg || over) return;
<<<<<<< HEAD
    CBlockHeader header = header_orig; // working copy
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    header.nNonce = offset;

    uint32_t finish = std::numeric_limits<uint32_t>::max() - step;
    finish = finish - (finish % step) + offset;

    while (!found && header.nNonce < finish) {
        const uint32_t next = (finish - header.nNonce < 5000*step) ? finish : header.nNonce + 5000*step;
        do {
            if (UintToArith256(header.GetHash()) <= target) {
                if (!found.exchange(true)) {
<<<<<<< HEAD
                    header_orig.nNonce = header.nNonce;
=======
                    proposed_nonce = header.nNonce;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                }
                return;
            }
            header.nNonce += step;
        } while(header.nNonce != next);
    }
}

static int Grind(const std::vector<std::string>& args, std::string& strPrint)
{
    if (args.size() != 1) {
        strPrint = "Must specify block header to grind";
        return EXIT_FAILURE;
    }

    CBlockHeader header;
    if (!DecodeHexBlockHeader(header, args[0])) {
        strPrint = "Could not decode block header";
        return EXIT_FAILURE;
    }

    uint32_t nBits = header.nBits;
    std::atomic<bool> found{false};
<<<<<<< HEAD

    std::vector<std::thread> threads;
    int n_tasks = std::max(1u, std::thread::hardware_concurrency());
    for (int i = 0; i < n_tasks; ++i) {
        threads.emplace_back( grind_task, nBits, std::ref(header), i, n_tasks, std::ref(found) );
=======
    uint32_t proposed_nonce{};

    std::vector<std::thread> threads;
    int n_tasks = std::max(1u, std::thread::hardware_concurrency());
    threads.reserve(n_tasks);
    for (int i = 0; i < n_tasks; ++i) {
        threads.emplace_back(grind_task, nBits, header, i, n_tasks, std::ref(found), std::ref(proposed_nonce));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    for (auto& t : threads) {
        t.join();
    }
<<<<<<< HEAD
    if (!found) {
=======
    if (found) {
        header.nNonce = proposed_nonce;
    } else {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        strPrint = "Could not satisfy difficulty target";
        return EXIT_FAILURE;
    }

<<<<<<< HEAD
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
=======
    DataStream ss{};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    ss << header;
    strPrint = HexStr(ss);
    return EXIT_SUCCESS;
}

<<<<<<< HEAD
#ifdef WIN32
// Export main() and ensure working ASLR on Windows.
// Exporting a symbol will prevent the linker from stripping
// the .reloc section from the binary, which is a requirement
// for ASLR. This is a temporary workaround until a fixed
// version of binutils is used for releases.
__declspec(dllexport) int main(int argc, char* argv[])
#else
int main(int argc, char* argv[])
#endif
=======
MAIN_FUNCTION
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    ArgsManager& args = gArgs;
    SetupEnvironment();

    try {
        int ret = AppInitUtil(args, argc, argv);
        if (ret != CONTINUE_EXECUTION) {
            return ret;
        }
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInitUtil()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInitUtil()");
        return EXIT_FAILURE;
    }

    const auto cmd = args.GetCommand();
    if (!cmd) {
        tfm::format(std::cerr, "Error: must specify a command\n");
        return EXIT_FAILURE;
    }

    int ret = EXIT_FAILURE;
    std::string strPrint;
    try {
        if (cmd->command == "grind") {
            ret = Grind(cmd->args, strPrint);
        } else {
            assert(false); // unknown command should be caught earlier
        }
    } catch (const std::exception& e) {
        strPrint = std::string("error: ") + e.what();
    } catch (...) {
        strPrint = "unknown error";
    }

    if (strPrint != "") {
        tfm::format(ret == 0 ? std::cout : std::cerr, "%s\n", strPrint);
    }

    return ret;
}
