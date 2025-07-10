<<<<<<< HEAD
// Copyright (c) 2019 The DigiByte Core developers
=======
// Copyright (c) 2019-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/string.h>
<<<<<<< HEAD
=======

#include <regex>
#include <string>

void ReplaceAll(std::string& in_out, const std::string& search, const std::string& substitute)
{
    if (search.empty()) return;
    in_out = std::regex_replace(in_out, std::regex(search), substitute);
}
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
