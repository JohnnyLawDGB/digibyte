// Copyright (c) 2024-2026 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// MuSig2 bundle mining integration tests — placeholder pending Wave 3 session integration
// Full tests require complete AddOracleBundleToBlock + session orchestration wiring

#include <boost/test/unit_test.hpp>
#include <oracle/musig2_orchestrator.h>
#include <oracle/musig2_session.h>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(musig2_bundle_mining_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_global_session_map_accessible)
{
    // Verify the global signing session map is accessible
    LOCK(g_oracle_signing_sessions_mutex);
    // Can insert and erase sessions
    g_oracle_signing_sessions.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(42),
        std::forward_as_tuple(42, 9));
    BOOST_CHECK(g_oracle_signing_sessions.count(42) == 1);
    g_oracle_signing_sessions.erase(42);
    BOOST_CHECK(g_oracle_signing_sessions.count(42) == 0);
}

BOOST_AUTO_TEST_SUITE_END()
