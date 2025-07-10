// Copyright (c) 2009-2010 Satoshi Nakamoto
<<<<<<< HEAD
// Copyright (c) 2009-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/load.h>

<<<<<<< HEAD
#include <fs.h>
#include <interfaces/chain.h>
#include <scheduler.h>
#include <util/string.h>
#include <util/system.h>
#include <util/translation.h>
=======
#include <common/args.h>
#include <interfaces/chain.h>
#include <scheduler.h>
#include <util/check.h>
#include <util/fs.h>
#include <util/string.h>
#include <util/translation.h>
#include <wallet/context.h>
#include <wallet/spend.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <wallet/wallet.h>
#include <wallet/walletdb.h>

#include <univalue.h>

<<<<<<< HEAD
bool VerifyWallets(interfaces::Chain& chain)
{
    if (gArgs.IsArgSet("-walletdir")) {
        fs::path wallet_dir = gArgs.GetArg("-walletdir", "");
        boost::system::error_code error;
        // The canonical path cleans the path, preventing >1 Berkeley environment instances for the same directory
        fs::path canonical_wallet_dir = fs::canonical(wallet_dir, error).remove_trailing_separator();
        if (error || !fs::exists(wallet_dir)) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" does not exist"), wallet_dir.string()));
            return false;
        } else if (!fs::is_directory(wallet_dir)) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" is not a directory"), wallet_dir.string()));
            return false;
        // The canonical path transforms relative paths into absolute ones, so we check the non-canonical version
        } else if (!wallet_dir.is_absolute()) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" is a relative path"), wallet_dir.string()));
            return false;
        }
        gArgs.ForceSetArg("-walletdir", canonical_wallet_dir.string());
    }

    LogPrintf("Using wallet directory %s\n", GetWalletDir().string());
=======
#include <system_error>

namespace wallet {
bool VerifyWallets(WalletContext& context)
{
    interfaces::Chain& chain = *context.chain;
    ArgsManager& args = *Assert(context.args);

    if (args.IsArgSet("-walletdir")) {
        const fs::path wallet_dir{args.GetPathArg("-walletdir")};
        std::error_code error;
        // The canonical path cleans the path, preventing >1 Berkeley environment instances for the same directory
        // It also lets the fs::exists and fs::is_directory checks below pass on windows, since they return false
        // if a path has trailing slashes, and it strips trailing slashes.
        fs::path canonical_wallet_dir = fs::canonical(wallet_dir, error);
        if (error || !fs::exists(canonical_wallet_dir)) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" does not exist"), fs::PathToString(wallet_dir)));
            return false;
        } else if (!fs::is_directory(canonical_wallet_dir)) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" is not a directory"), fs::PathToString(wallet_dir)));
            return false;
        // The canonical path transforms relative paths into absolute ones, so we check the non-canonical version
        } else if (!wallet_dir.is_absolute()) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" is a relative path"), fs::PathToString(wallet_dir)));
            return false;
        }
        args.ForceSetArg("-walletdir", fs::PathToString(canonical_wallet_dir));
    }

    LogPrintf("Using wallet directory %s\n", fs::PathToString(GetWalletDir()));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    chain.initMessage(_("Verifying wallet(s)…").translated);

    // For backwards compatibility if an unnamed top level wallet exists in the
    // wallets directory, include it in the default list of wallets to load.
<<<<<<< HEAD
    if (!gArgs.IsArgSet("wallet")) {
        DatabaseOptions options;
        DatabaseStatus status;
=======
    if (!args.IsArgSet("wallet")) {
        DatabaseOptions options;
        DatabaseStatus status;
        ReadDatabaseArgs(args, options);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        bilingual_str error_string;
        options.require_existing = true;
        options.verify = false;
        if (MakeWalletDatabase("", options, status, error_string)) {
<<<<<<< HEAD
            gArgs.LockSettings([&](util::Settings& settings) {
                util::SettingsValue wallets(util::SettingsValue::VARR);
                wallets.push_back(""); // Default wallet name is ""
                settings.rw_settings["wallet"] = wallets;
            });
=======
            common::SettingsValue wallets(common::SettingsValue::VARR);
            wallets.push_back(""); // Default wallet name is ""
            // Pass write=false because no need to write file and probably
            // better not to. If unnamed wallet needs to be added next startup
            // and the setting is empty, this code will just run again.
            chain.updateRwSetting("wallet", wallets, /* write= */ false);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        }
    }

    // Keep track of each wallet absolute path to detect duplicates.
    std::set<fs::path> wallet_paths;

<<<<<<< HEAD
    for (const auto& wallet_file : gArgs.GetArgs("-wallet")) {
        const fs::path path = fsbridge::AbsPathJoin(GetWalletDir(), wallet_file);
=======
    for (const auto& wallet : chain.getSettingsList("wallet")) {
        const auto& wallet_file = wallet.get_str();
        const fs::path path = fsbridge::AbsPathJoin(GetWalletDir(), fs::PathFromString(wallet_file));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

        if (!wallet_paths.insert(path).second) {
            chain.initWarning(strprintf(_("Ignoring duplicate -wallet %s."), wallet_file));
            continue;
        }

        DatabaseOptions options;
        DatabaseStatus status;
<<<<<<< HEAD
=======
        ReadDatabaseArgs(args, options);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        options.require_existing = true;
        options.verify = true;
        bilingual_str error_string;
        if (!MakeWalletDatabase(wallet_file, options, status, error_string)) {
            if (status == DatabaseStatus::FAILED_NOT_FOUND) {
                chain.initWarning(Untranslated(strprintf("Skipping -wallet path that doesn't exist. %s", error_string.original)));
            } else {
                chain.initError(error_string);
                return false;
            }
        }
    }

    return true;
}

<<<<<<< HEAD
bool LoadWallets(interfaces::Chain& chain)
{
    try {
        std::set<fs::path> wallet_paths;
        for (const std::string& name : gArgs.GetArgs("-wallet")) {
            if (!wallet_paths.insert(name).second) {
=======
bool LoadWallets(WalletContext& context)
{
    interfaces::Chain& chain = *context.chain;
    try {
        std::set<fs::path> wallet_paths;
        for (const auto& wallet : chain.getSettingsList("wallet")) {
            const auto& name = wallet.get_str();
            if (!wallet_paths.insert(fs::PathFromString(name)).second) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                continue;
            }
            DatabaseOptions options;
            DatabaseStatus status;
<<<<<<< HEAD
=======
            ReadDatabaseArgs(*context.args, options);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            options.require_existing = true;
            options.verify = false; // No need to verify, assuming verified earlier in VerifyWallets()
            bilingual_str error;
            std::vector<bilingual_str> warnings;
            std::unique_ptr<WalletDatabase> database = MakeWalletDatabase(name, options, status, error);
            if (!database && status == DatabaseStatus::FAILED_NOT_FOUND) {
                continue;
            }
            chain.initMessage(_("Loading wallet…").translated);
<<<<<<< HEAD
            std::shared_ptr<CWallet> pwallet = database ? CWallet::Create(&chain, name, std::move(database), options.create_flags, error, warnings) : nullptr;
=======
            std::shared_ptr<CWallet> pwallet = database ? CWallet::Create(context, name, std::move(database), options.create_flags, error, warnings) : nullptr;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            if (!warnings.empty()) chain.initWarning(Join(warnings, Untranslated("\n")));
            if (!pwallet) {
                chain.initError(error);
                return false;
            }
<<<<<<< HEAD
            AddWallet(pwallet);
=======

            NotifyWalletLoaded(context, pwallet);
            AddWallet(context, pwallet);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        }
        return true;
    } catch (const std::runtime_error& e) {
        chain.initError(Untranslated(e.what()));
        return false;
    }
}

<<<<<<< HEAD
void StartWallets(CScheduler& scheduler, const ArgsManager& args)
{
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
=======
void StartWallets(WalletContext& context, CScheduler& scheduler)
{
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets(context)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        pwallet->postInitProcess();
    }

    // Schedule periodic wallet flushes and tx rebroadcasts
<<<<<<< HEAD
    if (args.GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET)) {
        scheduler.scheduleEvery(MaybeCompactWalletDB, std::chrono::milliseconds{500});
    }
    scheduler.scheduleEvery(MaybeResendWalletTxs, std::chrono::milliseconds{1000});
}

void FlushWallets()
{
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
=======
    if (context.args->GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET)) {
        scheduler.scheduleEvery([&context] { MaybeCompactWalletDB(context); }, std::chrono::milliseconds{500});
    }
    scheduler.scheduleEvery([&context] { MaybeResendWalletTxs(context); }, 1min);
}

void FlushWallets(WalletContext& context)
{
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets(context)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        pwallet->Flush();
    }
}

<<<<<<< HEAD
void StopWallets()
{
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
=======
void StopWallets(WalletContext& context)
{
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets(context)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        pwallet->Close();
    }
}

<<<<<<< HEAD
void UnloadWallets()
{
    auto wallets = GetWallets();
=======
void UnloadWallets(WalletContext& context)
{
    auto wallets = GetWallets(context);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    while (!wallets.empty()) {
        auto wallet = wallets.back();
        wallets.pop_back();
        std::vector<bilingual_str> warnings;
<<<<<<< HEAD
        RemoveWallet(wallet, std::nullopt, warnings);
        UnloadWallet(std::move(wallet));
    }
}
=======
        RemoveWallet(context, wallet, /* load_on_start= */ std::nullopt, warnings);
        UnloadWallet(std::move(wallet));
    }
}
} // namespace wallet
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
