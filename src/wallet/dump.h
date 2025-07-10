<<<<<<< HEAD
// Copyright (c) 2020 The DigiByte Core developers
=======
// Copyright (c) 2020-2021 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_WALLET_DUMP_H
#define DIGIBYTE_WALLET_DUMP_H

<<<<<<< HEAD
#include <fs.h>

class CWallet;

struct bilingual_str;

bool DumpWallet(CWallet& wallet, bilingual_str& error);
bool CreateFromDump(const std::string& name, const fs::path& wallet_path, bilingual_str& error, std::vector<bilingual_str>& warnings);
=======
#include <util/fs.h>

#include <string>
#include <vector>

struct bilingual_str;
class ArgsManager;

namespace wallet {
class CWallet;
bool DumpWallet(const ArgsManager& args, CWallet& wallet, bilingual_str& error);
bool CreateFromDump(const ArgsManager& args, const std::string& name, const fs::path& wallet_path, bilingual_str& error, std::vector<bilingual_str>& warnings);
} // namespace wallet
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

#endif // DIGIBYTE_WALLET_DUMP_H
