<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2009-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_CORE_IO_H
#define DIGIBYTE_CORE_IO_H

<<<<<<< HEAD
#include <amount.h>
#include <attributes.h>
=======
#include <consensus/amount.h>
#include <util/result.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#include <string>
#include <vector>

class CBlock;
class CBlockHeader;
class CScript;
class CTransaction;
struct CMutableTransaction;
<<<<<<< HEAD
class uint256;
class UniValue;
class CTxUndo;
=======
class SigningProvider;
class uint256;
class UniValue;
class CTxUndo;

/**
 * Verbose level for block's transaction
 */
enum class TxVerbosity {
    SHOW_TXID,                //!< Only TXID for each block's transaction
    SHOW_DETAILS,             //!< Include TXID, inputs, outputs, and other common block's transaction information
    SHOW_DETAILS_AND_PREVOUT  //!< The same as previous option with information about prevouts if available
};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

// core_read.cpp
CScript ParseScript(const std::string& s);
std::string ScriptToAsmStr(const CScript& script, const bool fAttemptSighashDecode = false);
[[nodiscard]] bool DecodeHexTx(CMutableTransaction& tx, const std::string& hex_tx, bool try_no_witness = false, bool try_witness = true);
[[nodiscard]] bool DecodeHexBlk(CBlock&, const std::string& strHexBlk);
bool DecodeHexBlockHeader(CBlockHeader&, const std::string& hex_header);

/**
 * Parse a hex string into 256 bits
 * @param[in] strHex a hex-formatted, 64-character string
 * @param[out] result the result of the parsing
 * @returns true if successful, false if not
 *
 * @see ParseHashV for an RPC-oriented version of this
 */
bool ParseHashStr(const std::string& strHex, uint256& result);
<<<<<<< HEAD
std::vector<unsigned char> ParseHexUV(const UniValue& v, const std::string& strName);
int ParseSighashString(const UniValue& sighash);
=======
[[nodiscard]] util::Result<int> SighashFromStr(const std::string& sighash);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

// core_write.cpp
UniValue ValueFromAmount(const CAmount amount);
std::string FormatScript(const CScript& script);
std::string EncodeHexTx(const CTransaction& tx, const int serializeFlags = 0);
std::string SighashToStr(unsigned char sighash_type);
<<<<<<< HEAD
void ScriptPubKeyToUniv(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex, bool include_addresses);
void ScriptToUniv(const CScript& script, UniValue& out, bool include_address);
void TxToUniv(const CTransaction& tx, const uint256& hashBlock, bool include_addresses, UniValue& entry, bool include_hex = true, int serialize_flags = 0, const CTxUndo* txundo = nullptr);
=======
void ScriptToUniv(const CScript& script, UniValue& out, bool include_hex = true, bool include_address = false, const SigningProvider* provider = nullptr);
void TxToUniv(const CTransaction& tx, const uint256& block_hash, UniValue& entry, bool include_hex = true, int serialize_flags = 0, const CTxUndo* txundo = nullptr, TxVerbosity verbosity = TxVerbosity::SHOW_DETAILS);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#endif // DIGIBYTE_CORE_IO_H
