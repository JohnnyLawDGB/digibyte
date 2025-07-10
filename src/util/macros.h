<<<<<<< HEAD
// Copyright (c) 2019 The DigiByte Core developers
=======
// Copyright (c) 2019-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_UTIL_MACROS_H
#define DIGIBYTE_UTIL_MACROS_H

#define PASTE(x, y) x ## y
#define PASTE2(x, y) PASTE(x, y)

<<<<<<< HEAD
=======
#define UNIQUE_NAME(name) PASTE2(name, __COUNTER__)

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
/**
 * Converts the parameter X to a string after macro replacement on X has been performed.
 * Don't merge these into one macro!
 */
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#endif // DIGIBYTE_UTIL_MACROS_H
