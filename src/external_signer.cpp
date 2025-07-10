<<<<<<< HEAD
// Copyright (c) 2018-2021 The DigiByte Core developers
=======
// Copyright (c) 2018-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
<<<<<<< HEAD
#include <core_io.h>
#include <psbt.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <external_signer.h>

=======
#include <common/run_command.h>
#include <core_io.h>
#include <psbt.h>
#include <util/strencodings.h>
#include <external_signer.h>

#include <algorithm>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <stdexcept>
#include <string>
#include <vector>

ExternalSigner::ExternalSigner(const std::string& command, const std::string chain, const std::string& fingerprint, const std::string name): m_command(command), m_chain(chain), m_fingerprint(fingerprint), m_name(name) {}

<<<<<<< HEAD
const std::string ExternalSigner::NetworkArg() const
=======
std::string ExternalSigner::NetworkArg() const
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    return " --chain " + m_chain;
}

bool ExternalSigner::Enumerate(const std::string& command, std::vector<ExternalSigner>& signers, const std::string chain)
{
    // Call <command> enumerate
    const UniValue result = RunCommandParseJSON(command + " enumerate");
    if (!result.isArray()) {
        throw std::runtime_error(strprintf("'%s' received invalid response, expected array of signers", command));
    }
<<<<<<< HEAD
    for (UniValue signer : result.getValues()) {
        // Check for error
        const UniValue& error = find_value(signer, "error");
=======
    for (const UniValue& signer : result.getValues()) {
        // Check for error
        const UniValue& error = signer.find_value("error");
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        if (!error.isNull()) {
            if (!error.isStr()) {
                throw std::runtime_error(strprintf("'%s' error", command));
            }
            throw std::runtime_error(strprintf("'%s' error: %s", command, error.getValStr()));
        }
        // Check if fingerprint is present
<<<<<<< HEAD
        const UniValue& fingerprint = find_value(signer, "fingerprint");
        if (fingerprint.isNull()) {
            throw std::runtime_error(strprintf("'%s' received invalid response, missing signer fingerprint", command));
        }
        const std::string fingerprintStr = fingerprint.get_str();
=======
        const UniValue& fingerprint = signer.find_value("fingerprint");
        if (fingerprint.isNull()) {
            throw std::runtime_error(strprintf("'%s' received invalid response, missing signer fingerprint", command));
        }
        const std::string& fingerprintStr{fingerprint.get_str()};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        // Skip duplicate signer
        bool duplicate = false;
        for (const ExternalSigner& signer : signers) {
            if (signer.m_fingerprint.compare(fingerprintStr) == 0) duplicate = true;
        }
        if (duplicate) break;
<<<<<<< HEAD
        std::string name = "";
        const UniValue& model_field = find_value(signer, "model");
        if (model_field.isStr() && model_field.getValStr() != "") {
            name += model_field.getValStr();
        }
        signers.push_back(ExternalSigner(command, chain, fingerprintStr, name));
=======
        std::string name;
        const UniValue& model_field = signer.find_value("model");
        if (model_field.isStr() && model_field.getValStr() != "") {
            name += model_field.getValStr();
        }
        signers.emplace_back(command, chain, fingerprintStr, name);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    return true;
}

UniValue ExternalSigner::DisplayAddress(const std::string& descriptor) const
{
    return RunCommandParseJSON(m_command + " --fingerprint \"" + m_fingerprint + "\"" + NetworkArg() + " displayaddress --desc \"" + descriptor + "\"");
}

UniValue ExternalSigner::GetDescriptors(const int account)
{
    return RunCommandParseJSON(m_command + " --fingerprint \"" + m_fingerprint + "\"" + NetworkArg() + " getdescriptors --account " + strprintf("%d", account));
}

bool ExternalSigner::SignTransaction(PartiallySignedTransaction& psbtx, std::string& error)
{
    // Serialize the PSBT
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbtx;
<<<<<<< HEAD

    // Check if signer fingerprint matches any input master key fingerprint
    bool match = false;
    for (unsigned int i = 0; i < psbtx.inputs.size(); ++i) {
        const PSBTInput& input = psbtx.inputs[i];
        for (const auto& entry : input.hd_keypaths) {
            if (m_fingerprint == strprintf("%08x", ReadBE32(entry.second.fingerprint))) match = true;
        }
    }

    if (!match) {
=======
    // parse ExternalSigner master fingerprint
    std::vector<unsigned char> parsed_m_fingerprint = ParseHex(m_fingerprint);
    // Check if signer fingerprint matches any input master key fingerprint
    auto matches_signer_fingerprint = [&](const PSBTInput& input) {
        for (const auto& entry : input.hd_keypaths) {
            if (parsed_m_fingerprint == MakeUCharSpan(entry.second.fingerprint)) return true;
        }
        for (const auto& entry : input.m_tap_bip32_paths) {
            if (parsed_m_fingerprint == MakeUCharSpan(entry.second.second.fingerprint)) return true;
        }
        return false;
    };

    if (!std::any_of(psbtx.inputs.begin(), psbtx.inputs.end(), matches_signer_fingerprint)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        error = "Signer fingerprint " + m_fingerprint + " does not match any of the inputs:\n" + EncodeBase64(ssTx.str());
        return false;
    }

    const std::string command = m_command + " --stdin --fingerprint \"" + m_fingerprint + "\"" + NetworkArg();
    const std::string stdinStr = "signtx \"" + EncodeBase64(ssTx.str()) + "\"";

    const UniValue signer_result = RunCommandParseJSON(command, stdinStr);

<<<<<<< HEAD
    if (find_value(signer_result, "error").isStr()) {
        error = find_value(signer_result, "error").get_str();
        return false;
    }

    if (!find_value(signer_result, "psbt").isStr()) {
=======
    if (signer_result.find_value("error").isStr()) {
        error = signer_result.find_value("error").get_str();
        return false;
    }

    if (!signer_result.find_value("psbt").isStr()) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        error = "Unexpected result from signer";
        return false;
    }

    PartiallySignedTransaction signer_psbtx;
    std::string signer_psbt_error;
<<<<<<< HEAD
    if (!DecodeBase64PSBT(signer_psbtx, find_value(signer_result, "psbt").get_str(), signer_psbt_error)) {
=======
    if (!DecodeBase64PSBT(signer_psbtx, signer_result.find_value("psbt").get_str(), signer_psbt_error)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        error = strprintf("TX decode failed %s", signer_psbt_error);
        return false;
    }

    psbtx = signer_psbtx;

    return true;
}
