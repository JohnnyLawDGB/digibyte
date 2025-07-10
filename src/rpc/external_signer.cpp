<<<<<<< HEAD
// Copyright (c) 2018-2021 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparamsbase.h>
#include <external_signer.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>
#include <rpc/protocol.h>
=======
// Copyright (c) 2018-2022 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/args.h>
#include <common/system.h>
#include <external_signer.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#include <string>
#include <vector>

#ifdef ENABLE_EXTERNAL_SIGNER

static RPCHelpMan enumeratesigners()
{
    return RPCHelpMan{"enumeratesigners",
        "Returns a list of external signers from -signer.",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
<<<<<<< HEAD
                {RPCResult::Type::ARR, "signers", /* optional */ false, "",
                {
                    {RPCResult::Type::STR_HEX, "masterkeyfingerprint", "Master key fingerprint"},
                    {RPCResult::Type::STR, "name", "Device name"},
=======
                {RPCResult::Type::ARR, "signers", /*optional=*/false, "",
                {
                    {RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "fingerprint", "Master key fingerprint"},
                        {RPCResult::Type::STR, "name", "Device name"},
                    }},
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
                },
                }
            }
        },
        RPCExamples{
            HelpExampleCli("enumeratesigners", "")
            + HelpExampleRpc("enumeratesigners", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::string command = gArgs.GetArg("-signer", "");
            if (command == "") throw JSONRPCError(RPC_MISC_ERROR, "Error: restart digibyted with -signer=<cmd>");
<<<<<<< HEAD
            const std::string chain = gArgs.GetChainName();
=======
            const std::string chain = gArgs.GetChainTypeString();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
            UniValue signers_res = UniValue::VARR;
            try {
                std::vector<ExternalSigner> signers;
                ExternalSigner::Enumerate(command, signers, chain);
                for (const ExternalSigner& signer : signers) {
                    UniValue signer_res = UniValue::VOBJ;
                    signer_res.pushKV("fingerprint", signer.m_fingerprint);
                    signer_res.pushKV("name", signer.m_name);
                    signers_res.push_back(signer_res);
                }
            } catch (const std::exception& e) {
                throw JSONRPCError(RPC_MISC_ERROR, e.what());
            }
            UniValue result(UniValue::VOBJ);
            result.pushKV("signers", signers_res);
            return result;
        }
    };
}

<<<<<<< HEAD
void RegisterSignerRPCCommands(CRPCTable &t)
{
// clang-format off
static const CRPCCommand commands[] =
{ // category              actor (function)
  // --------------------- ------------------------
  { "signer",              &enumeratesigners,      },
};
// clang-format on
=======
void RegisterSignerRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"signer", &enumeratesigners},
    };
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}

#endif // ENABLE_EXTERNAL_SIGNER
