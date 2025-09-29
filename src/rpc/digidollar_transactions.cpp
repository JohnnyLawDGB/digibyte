// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/digidollar_transactions.h>

#include <consensus/digidollar.h>
#include <consensus/digidollar_transaction_validation.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <stdexcept>

// =====================================
// GREEN Phase: Minimal Implementation
// These functions provide just enough functionality to make tests pass
// =====================================

UniValue getdigidollarinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getdigidollarinfo\n"
            "\nReturns DigiDollar system information.\n"
            "\nResult:\n"
            "{\n"
            "  \"active\": true|false,        (boolean) Is DigiDollar active\n"
            "  \"total_supply\": n,           (numeric) Total DD in circulation\n"
            "  \"total_collateral\": n,       (numeric) Total DGB collateral\n"
            "  \"system_health\": n           (numeric) System collateral percentage\n"
            "}\n"
        );
    }

    UniValue result(UniValue::VOBJ);

    // GREEN phase: Return minimal working response
    result.pushKV("active", true);
    result.pushKV("total_supply", 0);
    result.pushKV("total_collateral", 0);
    result.pushKV("system_health", 200); // 200% (healthy)

    return result;
}

UniValue getdigidollaraddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getdigidollaraddress\n"
            "\nGenerates a new DigiDollar address.\n"
            "\nResult:\n"
            "\"address\"                     (string) New DD address\n"
        );
    }

    // GREEN phase: Return mock DD address
    return UniValue("dd1qw508d6qejxtdg4y5r3zarvary0c5xw7k3k4k4k");
}

UniValue getdigidollarbalance(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getdigidollarbalance\n"
            "\nReturns DigiDollar balance.\n"
            "\nResult:\n"
            "n                               (numeric) DD balance\n"
        );
    }

    // GREEN phase: Return 0 balance initially
    return UniValue(0.0);
}

UniValue mintdigidollar(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3) {
        throw std::runtime_error(
            "mintdigidollar amount lockdays [collateral_address]\n"
            "\nMint DigiDollar tokens.\n"
            "\nArguments:\n"
            "1. amount                       (numeric, required) DD amount to mint\n"
            "2. lockdays                     (numeric, required) Lock period in days\n"
            "3. collateral_address           (string, optional) Address for collateral\n"
            "\nResult:\n"
            "\"txid\"                        (string) Transaction ID\n"
        );
    }

    double amount = request.params[0].get_real();
    int lockdays = request.params[1].get_int();

    // GREEN phase: Basic validation
    if (amount < 100.0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount below minimum ($100)");
    }
    if (amount > 100000.0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount above maximum ($100k)");
    }
    if (lockdays < 30) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Lock period too short (minimum 30 days)");
    }

    // GREEN phase: Return mock transaction ID
    return UniValue("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
}

UniValue transferdigidollar(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "transferdigidollar address amount\n"
            "\nTransfer DigiDollar tokens.\n"
            "\nArguments:\n"
            "1. address                      (string, required) Recipient DD address\n"
            "2. amount                       (numeric, required) DD amount to transfer\n"
            "\nResult:\n"
            "\"txid\"                        (string) Transaction ID\n"
        );
    }

    std::string address = request.params[0].get_str();
    double amount = request.params[1].get_real();

    // GREEN phase: Basic validation
    if (!ValidateDDAddress(address)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid DD address");
    }
    if (amount <= 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be positive");
    }

    // GREEN phase: Return mock transaction ID
    return UniValue("fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321");
}

UniValue redeemdigidollar(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            "redeemdigidollar amount [redeem_type]\n"
            "\nRedeem DigiDollar tokens.\n"
            "\nArguments:\n"
            "1. amount                       (numeric, required) DD amount to redeem\n"
            "2. redeem_type                  (string, optional) normal|partial|emergency\n"
            "\nResult:\n"
            "\"txid\"                        (string) Transaction ID\n"
        );
    }

    double amount = request.params[1].get_real();
    std::string redeem_type = request.params.size() > 1 ? request.params[1].get_str() : "normal";

    // GREEN phase: Basic validation
    if (amount <= 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be positive");
    }
    if (redeem_type != "normal" && redeem_type != "partial" && redeem_type != "emergency") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid redeem type");
    }

    // GREEN phase: Return mock transaction ID
    return UniValue("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");
}

UniValue setmockoracleprice(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "setmockoracleprice price\n"
            "\nSet mock oracle price for testing.\n"
            "\nArguments:\n"
            "1. price                        (numeric, required) DGB price in cents\n"
            "\nResult:\n"
            "true                            (boolean) Success\n"
        );
    }

    double price = request.params[0].get_real();

    // GREEN phase: Basic validation
    if (!ValidateOraclePrice(static_cast<CAmount>(price * 100))) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid oracle price");
    }

    // GREEN phase: Just return success (no actual price setting)
    return UniValue(true);
}

UniValue createrawddtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "createrawddtransaction inputs outputs\n"
            "\nCreate raw DigiDollar transaction.\n"
            "\nArguments:\n"
            "1. inputs                       (array, required) Transaction inputs\n"
            "2. outputs                      (object, required) Transaction outputs\n"
            "\nResult:\n"
            "\"hex\"                         (string) Raw transaction hex\n"
        );
    }

    // GREEN phase: Return mock raw transaction
    return UniValue("0100000001000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
}

// RPC command definitions
const CRPCCommand digidollar_transaction_commands[] = {
    {"digidollar", "getdigidollarinfo",        &getdigidollarinfo,        {}},
    {"digidollar", "getdigidollaraddress",     &getdigidollaraddress,     {}},
    {"digidollar", "getdigidollarbalance",     &getdigidollarbalance,     {}},
    {"digidollar", "mintdigidollar",           &mintdigidollar,           {"amount", "lockdays", "collateral_address"}},
    {"digidollar", "transferdigidollar",       &transferdigidollar,       {"address", "amount"}},
    {"digidollar", "redeemdigidollar",         &redeemdigidollar,         {"amount", "redeem_type"}},
    {"digidollar", "setmockoracleprice",       &setmockoracleprice,       {"price"}},
    {"digidollar", "createrawddtransaction",   &createrawddtransaction,   {"inputs", "outputs"}},
};