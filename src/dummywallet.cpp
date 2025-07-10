<<<<<<< HEAD
// Copyright (c) 2018-2020 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/system.h>
#include <walletinitinterface.h>

class CWallet;
=======
// Copyright (c) 2018-2021 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/args.h>
#include <logging.h>
#include <walletinitinterface.h>

class ArgsManager;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

namespace interfaces {
class Chain;
class Handler;
class Wallet;
<<<<<<< HEAD
=======
class WalletLoader;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}

class DummyWalletInit : public WalletInitInterface {
public:

    bool HasWalletSupport() const override {return false;}
    void AddWalletOptions(ArgsManager& argsman) const override;
    bool ParameterInteraction() const override {return true;}
<<<<<<< HEAD
    void Construct(NodeContext& node) const override {LogPrintf("No wallet support compiled in!\n");}
=======
    void Construct(node::NodeContext& node) const override {LogPrintf("No wallet support compiled in!\n");}
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
};

void DummyWalletInit::AddWalletOptions(ArgsManager& argsman) const
{
    argsman.AddHiddenArgs({
        "-addresstype",
        "-avoidpartialspends",
        "-changetype",
<<<<<<< HEAD
        "-disabledandelion",
=======
        "-consolidatefeerate=<amt>",
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        "-disablewallet",
        "-discardfee=<amt>",
        "-fallbackfee=<amt>",
        "-keypool=<n>",
        "-maxapsfee=<n>",
        "-maxtxfee=<amt>",
        "-mintxfee=<amt>",
        "-paytxfee=<amt>",
<<<<<<< HEAD
        "-rescan",
        "-salvagewallet",
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        "-signer=<cmd>",
        "-spendzeroconfchange",
        "-txconfirmtarget=<n>",
        "-wallet=<path>",
        "-walletbroadcast",
        "-walletdir=<dir>",
        "-walletnotify=<cmd>",
        "-walletrbf",
        "-dblogsize=<n>",
        "-flushwallet",
        "-privdb",
        "-walletrejectlongchains",
<<<<<<< HEAD
=======
        "-walletcrosschain",
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        "-unsafesqlitesync",
    });
}

const WalletInitInterface& g_wallet_init_interface = DummyWalletInit();

namespace interfaces {

<<<<<<< HEAD
std::unique_ptr<Wallet> MakeWallet(const std::shared_ptr<CWallet>& wallet)
=======
std::unique_ptr<WalletLoader> MakeWalletLoader(Chain& chain, ArgsManager& args)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    throw std::logic_error("Wallet function called in non-wallet build.");
}

} // namespace interfaces
