<<<<<<< HEAD
// Copyright (c) 2019-2020 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sstream>
#include <stdio.h>
=======
// Copyright (c) 2019-2022 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <tinyformat.h>
#include <util/bip32.h>
#include <util/strencodings.h>

<<<<<<< HEAD
=======
#include <cstdint>
#include <cstdio>
#include <sstream>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

bool ParseHDKeypath(const std::string& keypath_str, std::vector<uint32_t>& keypath)
{
    std::stringstream ss(keypath_str);
    std::string item;
    bool first = true;
    while (std::getline(ss, item, '/')) {
        if (item.compare("m") == 0) {
            if (first) {
                first = false;
                continue;
            }
            return false;
        }
        // Finds whether it is hardened
        uint32_t path = 0;
<<<<<<< HEAD
        size_t pos = item.find("'");
=======
        size_t pos = item.find('\'');
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        if (pos != std::string::npos) {
            // The hardened tick can only be in the last index of the string
            if (pos != item.size() - 1) {
                return false;
            }
            path |= 0x80000000;
            item = item.substr(0, item.size() - 1); // Drop the last character which is the hardened tick
        }

        // Ensure this is only numbers
        if (item.find_first_not_of( "0123456789" ) != std::string::npos) {
            return false;
        }
        uint32_t number;
        if (!ParseUInt32(item, &number)) {
            return false;
        }
        path |= number;

        keypath.push_back(path);
        first = false;
    }
    return true;
}

<<<<<<< HEAD
std::string FormatHDKeypath(const std::vector<uint32_t>& path)
=======
std::string FormatHDKeypath(const std::vector<uint32_t>& path, bool apostrophe)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    std::string ret;
    for (auto i : path) {
        ret += strprintf("/%i", (i << 1) >> 1);
<<<<<<< HEAD
        if (i >> 31) ret += '\'';
=======
        if (i >> 31) ret += apostrophe ? '\'' : 'h';
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }
    return ret;
}

<<<<<<< HEAD
std::string WriteHDKeypath(const std::vector<uint32_t>& keypath)
{
    return "m" + FormatHDKeypath(keypath);
=======
std::string WriteHDKeypath(const std::vector<uint32_t>& keypath, bool apostrophe)
{
    return "m" + FormatHDKeypath(keypath, apostrophe);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
}
