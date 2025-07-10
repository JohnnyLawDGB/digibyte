<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2012-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <test/util/setup_common.h>
#include <util/time.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sanity_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(basic_sanity)
{
<<<<<<< HEAD
  BOOST_CHECK_MESSAGE(glibcxx_sanity_test() == true, "stdlib sanity test");
=======
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
  BOOST_CHECK_MESSAGE(ECC_InitSanityCheck() == true, "secp256k1 sanity test");
  BOOST_CHECK_MESSAGE(ChronoSanityCheck() == true, "chrono epoch test");
}

BOOST_AUTO_TEST_SUITE_END()
