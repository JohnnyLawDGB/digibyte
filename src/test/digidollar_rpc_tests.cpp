// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <digidollar/health.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include <rpc/digidollar.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <univalue.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(digidollar_rpc_tests)

struct DigiDollarRPCTestSetup : public TestingSetup {
    DigiDollarRPCTestSetup() : TestingSetup(ChainType::REGTEST) {
        // Initialize health monitoring system
        DigiDollar::SystemHealthMonitor::Initialize();

        // TODO: Set up RPC table with new CRPCCommand format
        // This test needs updating to the new RPC registration format
        // tableRPC.appendCommand("getdigidollarstatus", &getdigidollarstatus);

        // Create mock request structure
        mockRequest.URI = "/";
        mockRequest.authUser = "";
        mockRequest.mode = JSONRPCRequest::EXECUTE;
    }

    ~DigiDollarRPCTestSetup() {
        DigiDollar::SystemHealthMonitor::Shutdown();
    }

    JSONRPCRequest mockRequest;
};

// Test 1: Basic RPC Command Registration
/* TODO: Update all these tests to the new RPC registration format
BOOST_FIXTURE_TEST_CASE(test_rpc_command_registration, DigiDollarRPCTestSetup)
{
    // Test that the command is registered
    const CRPCCommand* command = tableRPC.find("getdigidollarstatus");
    BOOST_CHECK(command != nullptr);

    // Test command category
    std::string help = tableRPC["getdigidollarstatus"]->help;
    BOOST_CHECK(!help.empty());
}

// Test 2: Basic RPC Call Response
BOOST_FIXTURE_TEST_CASE(test_basic_rpc_call, DigiDollarRPCTestSetup)
{
    // Set up request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make RPC call
    UniValue result = getdigidollarstatus(mockRequest);

    // Should return a valid JSON object
    BOOST_CHECK(result.isObject());

    // Check required top-level fields
    BOOST_CHECK(result.exists("supply"));
    BOOST_CHECK(result.exists("collateral"));
    BOOST_CHECK(result.exists("health"));
    BOOST_CHECK(result.exists("dca_multiplier"));
    BOOST_CHECK(result.exists("err_active"));
    BOOST_CHECK(result.exists("volatility"));
    BOOST_CHECK(result.exists("minting_frozen"));
    BOOST_CHECK(result.exists("tiers"));
    BOOST_CHECK(result.exists("oracles"));
    BOOST_CHECK(result.exists("active_alerts"));
    BOOST_CHECK(result.exists("overall_status"));
    BOOST_CHECK(result.exists("recommended_action"));
}

// Test 3: JSON Response Format Validation
BOOST_FIXTURE_TEST_CASE(test_json_response_format, DigiDollarRPCTestSetup)
{
    // Set up request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make RPC call
    UniValue result = getdigidollarstatus(mockRequest);

    // Validate data types
    BOOST_CHECK(result["supply"].isNum());
    BOOST_CHECK(result["collateral"].isNum());
    BOOST_CHECK(result["health"].isNum());
    BOOST_CHECK(result["dca_multiplier"].isNum());
    BOOST_CHECK(result["err_active"].isBool());
    BOOST_CHECK(result["volatility"].isNum());
    BOOST_CHECK(result["minting_frozen"].isBool());
    BOOST_CHECK(result["tiers"].isArray());
    BOOST_CHECK(result["oracles"].isObject());
    BOOST_CHECK(result["active_alerts"].isArray());
    BOOST_CHECK(result["overall_status"].isStr());
    BOOST_CHECK(result["recommended_action"].isStr());

    // Validate numeric ranges
    BOOST_CHECK_GE(result["supply"].get_int64(), 0);
    BOOST_CHECK_GE(result["collateral"].get_int64(), 0);
    BOOST_CHECK_GE(result["health"].get_int(), 0);
    BOOST_CHECK_LE(result["health"].get_int(), 300);
    BOOST_CHECK_GE(result["dca_multiplier"].get_real(), 1.0);
    BOOST_CHECK_LE(result["dca_multiplier"].get_real(), 10.0);
    BOOST_CHECK_GE(result["volatility"].get_real(), 0.0);
    BOOST_CHECK_LE(result["volatility"].get_real(), 100.0);
}

// Test 4: Tier Array Structure
BOOST_FIXTURE_TEST_CASE(test_tier_array_structure, DigiDollarRPCTestSetup)
{
    // Set up request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make RPC call
    UniValue result = getdigidollarstatus(mockRequest);

    // Get tiers array
    const UniValue& tiers = result["tiers"];
    BOOST_CHECK(tiers.isArray());
    BOOST_CHECK_GT(tiers.size(), 0);

    // Validate each tier object
    for (size_t i = 0; i < tiers.size(); ++i) {
        const UniValue& tier = tiers[i];
        BOOST_CHECK(tier.isObject());

        // Check required fields
        BOOST_CHECK(tier.exists("lock_days"));
        BOOST_CHECK(tier.exists("dd_minted"));
        BOOST_CHECK(tier.exists("dgb_locked"));
        BOOST_CHECK(tier.exists("positions"));
        BOOST_CHECK(tier.exists("health"));
        BOOST_CHECK(tier.exists("status"));
        BOOST_CHECK(tier.exists("action"));

        // Check data types
        BOOST_CHECK(tier["lock_days"].isNum());
        BOOST_CHECK(tier["dd_minted"].isNum());
        BOOST_CHECK(tier["dgb_locked"].isNum());
        BOOST_CHECK(tier["positions"].isNum());
        BOOST_CHECK(tier["health"].isNum());
        BOOST_CHECK(tier["status"].isStr());
        BOOST_CHECK(tier["action"].isStr());

        // Validate ranges
        BOOST_CHECK_GT(tier["lock_days"].get_int(), 0);
        BOOST_CHECK_GE(tier["dd_minted"].get_int64(), 0);
        BOOST_CHECK_GE(tier["dgb_locked"].get_int64(), 0);
        BOOST_CHECK_GE(tier["positions"].get_int(), 0);
        BOOST_CHECK_GE(tier["health"].get_int(), 0);
        BOOST_CHECK_LE(tier["health"].get_int(), 300);

        // Validate status strings
        std::string status = tier["status"].get_str();
        BOOST_CHECK(status == "Healthy" || status == "Warning" || status == "Critical");

        // Validate action strings
        std::string action = tier["action"].get_str();
        BOOST_CHECK(!action.empty());
    }

    // Verify tier ordering (by lock days)
    for (size_t i = 1; i < tiers.size(); ++i) {
        int prevLockDays = tiers[i-1]["lock_days"].get_int();
        int currLockDays = tiers[i]["lock_days"].get_int();
        BOOST_CHECK_GT(currLockDays, prevLockDays);
    }
}

// Test 5: Oracle Object Structure
BOOST_FIXTURE_TEST_CASE(test_oracle_object_structure, DigiDollarRPCTestSetup)
{
    // Set up request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make RPC call
    UniValue result = getdigidollarstatus(mockRequest);

    // Get oracles object
    const UniValue& oracles = result["oracles"];
    BOOST_CHECK(oracles.isObject());

    // Check required fields
    BOOST_CHECK(oracles.exists("active_count"));
    BOOST_CHECK(oracles.exists("last_price"));
    BOOST_CHECK(oracles.exists("last_update"));
    BOOST_CHECK(oracles.exists("blocks_since_update"));
    BOOST_CHECK(oracles.exists("is_stale"));

    // Check data types
    BOOST_CHECK(oracles["active_count"].isNum());
    BOOST_CHECK(oracles["last_price"].isNum());
    BOOST_CHECK(oracles["last_update"].isNum());
    BOOST_CHECK(oracles["blocks_since_update"].isNum());
    BOOST_CHECK(oracles["is_stale"].isBool());

    // Validate ranges
    BOOST_CHECK_GE(oracles["active_count"].get_int(), 0);
    BOOST_CHECK_LE(oracles["active_count"].get_int(), 50); // Reasonable upper bound
    BOOST_CHECK_GE(oracles["last_price"].get_int64(), 0);
    BOOST_CHECK_GE(oracles["last_update"].get_int64(), 0);
    BOOST_CHECK_GE(oracles["blocks_since_update"].get_int64(), 0);
}

// Test 6: Active Alerts Array
BOOST_FIXTURE_TEST_CASE(test_active_alerts_array, DigiDollarRPCTestSetup)
{
    // Set up request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make RPC call
    UniValue result = getdigidollarstatus(mockRequest);

    // Get active alerts array
    const UniValue& alerts = result["active_alerts"];
    BOOST_CHECK(alerts.isArray());

    // All alerts should be valid strings
    for (size_t i = 0; i < alerts.size(); ++i) {
        BOOST_CHECK(alerts[i].isStr());
        std::string alert = alerts[i].get_str();
        BOOST_CHECK(!alert.empty());

        // Should be one of the known alert types
        BOOST_CHECK(alert == "system_health" ||
                   alert == "total_supply" ||
                   alert == "total_collateral" ||
                   alert == "oracle_status" ||
                   alert == "volatility" ||
                   alert == "position_count");
    }
}

// Test 7: Metric Accuracy Validation
BOOST_FIXTURE_TEST_CASE(test_metric_accuracy, DigiDollarRPCTestSetup)
{
    // Get health metrics directly
    DigiDollar::SystemMetrics directMetrics = DigiDollar::SystemHealthMonitor::GetSystemMetrics();

    // Set up RPC request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make RPC call
    UniValue rpcResult = getdigidollarstatus(mockRequest);

    // Compare RPC results with direct metrics
    BOOST_CHECK_EQUAL(rpcResult["supply"].get_int64(), directMetrics.totalDDSupply);
    BOOST_CHECK_EQUAL(rpcResult["collateral"].get_int64(), directMetrics.totalCollateral);
    BOOST_CHECK_EQUAL(rpcResult["health"].get_int(), directMetrics.systemHealth);

    BOOST_CHECK_CLOSE(rpcResult["dca_multiplier"].get_real(), directMetrics.dcaMultiplier, 0.01);
    BOOST_CHECK_EQUAL(rpcResult["err_active"].get_bool(), directMetrics.errActive);
    BOOST_CHECK_CLOSE(rpcResult["volatility"].get_real(), directMetrics.volatility, 0.01);
    BOOST_CHECK_EQUAL(rpcResult["minting_frozen"].get_bool(), directMetrics.mintingFrozen);

    // Compare tier metrics
    const UniValue& rpcTiers = rpcResult["tiers"];
    BOOST_CHECK_EQUAL(rpcTiers.size(), directMetrics.tiers.size());

    for (size_t i = 0; i < directMetrics.tiers.size(); ++i) {
        const auto& directTier = directMetrics.tiers[i];
        const UniValue& rpcTier = rpcTiers[i];

        BOOST_CHECK_EQUAL(rpcTier["lock_days"].get_int(), directTier.lockDays);
        BOOST_CHECK_EQUAL(rpcTier["dd_minted"].get_int64(), directTier.ddMinted);
        BOOST_CHECK_EQUAL(rpcTier["dgb_locked"].get_int64(), directTier.dgbLocked);
        BOOST_CHECK_EQUAL(rpcTier["positions"].get_int(), directTier.positions);
        BOOST_CHECK_EQUAL(rpcTier["health"].get_int(), directTier.healthRatio);
    }

    // Compare oracle metrics
    const UniValue& rpcOracles = rpcResult["oracles"];
    BOOST_CHECK_EQUAL(rpcOracles["active_count"].get_int(), directMetrics.activeOracles);
    BOOST_CHECK_EQUAL(rpcOracles["last_price"].get_int64(), directMetrics.lastOraclePrice);
    BOOST_CHECK_EQUAL(rpcOracles["last_update"].get_int64(), directMetrics.lastOracleUpdate);
}

// Test 8: Error Handling
BOOST_FIXTURE_TEST_CASE(test_error_handling, DigiDollarRPCTestSetup)
{
    // Test with extra parameters (should be ignored)
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("extra_param");

    // Should still work with extra parameters
    BOOST_CHECK_NO_THROW({
        UniValue result = getdigidollarstatus(mockRequest);
        BOOST_CHECK(result.isObject());
    });

    // Test help request
    mockRequest.mode = JSONRPCRequest::GET_HELP;
    BOOST_CHECK_THROW(getdigidollarstatus(mockRequest), std::runtime_error);

    // Reset mode
    mockRequest.mode = JSONRPCRequest::EXECUTE;
}

// Test 9: Performance Testing
BOOST_FIXTURE_TEST_CASE(test_rpc_performance, DigiDollarRPCTestSetup)
{
    // Set up request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Measure execution time
    auto start = GetTimeMillis();

    // Make multiple calls
    const int NUM_CALLS = 100;
    for (int i = 0; i < NUM_CALLS; ++i) {
        UniValue result = getdigidollarstatus(mockRequest);
        BOOST_CHECK(result.isObject());
    }

    auto end = GetTimeMillis();
    auto totalTime = end - start;

    // Should complete all calls in reasonable time (< 5 seconds)
    BOOST_CHECK_LT(totalTime, 5000);

    // Average time per call should be reasonable (< 50ms)
    auto avgTime = totalTime / NUM_CALLS;
    BOOST_CHECK_LT(avgTime, 50);

    LogPrint(BCLog::RPC, "DigiDollar RPC performance: %d calls in %dms (avg %dms)\n",
             NUM_CALLS, totalTime, avgTime);
}

// Test 10: Concurrent Access
BOOST_FIXTURE_TEST_CASE(test_concurrent_access, DigiDollarRPCTestSetup)
{
    // Set up requests
    std::vector<JSONRPCRequest> requests;
    for (int i = 0; i < 10; ++i) {
        JSONRPCRequest req;
        req.strMethod = "getdigidollarstatus";
        req.params = UniValue(UniValue::VARR);
        req.URI = "/";
        req.authUser = "";
        req.mode = JSONRPCRequest::EXECUTE;
        requests.push_back(req);
    }

    // Make concurrent calls (simulated)
    std::vector<UniValue> results;
    for (auto& req : requests) {
        UniValue result = getdigidollarstatus(req);
        BOOST_CHECK(result.isObject());
        results.push_back(result);
    }

    // All results should be valid and consistent
    for (size_t i = 1; i < results.size(); ++i) {
        // Basic consistency checks
        BOOST_CHECK_EQUAL(results[i]["supply"].get_int64(), results[0]["supply"].get_int64());
        BOOST_CHECK_EQUAL(results[i]["collateral"].get_int64(), results[0]["collateral"].get_int64());
        BOOST_CHECK_EQUAL(results[i]["health"].get_int(), results[0]["health"].get_int());
    }
}

// Test 11: Memory Usage
BOOST_FIXTURE_TEST_CASE(test_memory_usage, DigiDollarRPCTestSetup)
{
    // Set up request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make calls and check for memory leaks
    const int NUM_CALLS = 1000;
    for (int i = 0; i < NUM_CALLS; ++i) {
        UniValue result = getdigidollarstatus(mockRequest);
        BOOST_CHECK(result.isObject());

        // Clear result to prevent accumulation
        result.clear();
    }

    // If we reach here without running out of memory, the test passes
    BOOST_CHECK(true);
}

// Test 12: Integration with Health Monitor
BOOST_FIXTURE_TEST_CASE(test_health_monitor_integration, DigiDollarRPCTestSetup)
{
    // Get direct health report
    UniValue directReport = DigiDollar::SystemHealthMonitor::GetHealthReport();

    // Set up RPC request
    mockRequest.strMethod = "getdigidollarstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    // Make RPC call
    UniValue rpcResult = getdigidollarstatus(mockRequest);

    // The RPC result should match the direct health report structure
    // (Though RPC might format data differently)

    // Both should have same essential data
    BOOST_CHECK(directReport.exists("supply"));
    BOOST_CHECK(directReport.exists("collateral"));
    BOOST_CHECK(directReport.exists("health"));
    BOOST_CHECK(rpcResult.exists("supply"));
    BOOST_CHECK(rpcResult.exists("collateral"));
    BOOST_CHECK(rpcResult.exists("health"));

    // Values should match
    BOOST_CHECK_EQUAL(directReport["supply"].get_int64(), rpcResult["supply"].get_int64());
    BOOST_CHECK_EQUAL(directReport["collateral"].get_int64(), rpcResult["collateral"].get_int64());
    BOOST_CHECK_EQUAL(directReport["health"].get_int(), rpcResult["health"].get_int());
}

// Test 13: mintdigidollar RPC Command Tests
/* TODO: Update these tests to new RPC registration format
BOOST_FIXTURE_TEST_CASE(digidollar_test_mintdigidollar_rpc, DigiDollarRPCTestSetup)
{
    // Register command for testing
    tableRPC.appendCommand("mintdigidollar", &mintdigidollar);

    // Test 1: Valid mint parameters
    mockRequest.strMethod = "mintdigidollar";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back(100.0);  // DD amount
    mockRequest.params.push_back(3);      // Lock tier

    // Should not throw - method exists
    const CRPCCommand* command = tableRPC.find("mintdigidollar");
    BOOST_CHECK(command != nullptr);

    // Test 2: Invalid parameters
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back(-100.0);  // Negative amount
    mockRequest.params.push_back(3);

    // Should handle validation (when implemented)
    // BOOST_CHECK_THROW(mintdigidollar(mockRequest), JSONRPCError);

    // Test 3: Missing parameters
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back(100.0);  // Missing lock_tier
    // Should handle missing params (when implemented)

    // Test 4: Help text
    mockRequest.fHelp = true;
    BOOST_CHECK_THROW(mintdigidollar(mockRequest), std::runtime_error);
    mockRequest.fHelp = false;
}

// Test 14: senddigidollar RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_senddigidollar_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("senddigidollar", &senddigidollar);

    // Test valid send parameters
    mockRequest.strMethod = "senddigidollar";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("DDtestaddress123456789abcdef");  // DD address
    mockRequest.params.push_back(50.0);  // Amount

    const CRPCCommand* command = tableRPC.find("senddigidollar");
    BOOST_CHECK(command != nullptr);

    // Test invalid address format
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("invalid_address");
    mockRequest.params.push_back(50.0);
    // Should validate address format when implemented

    // Test negative amount
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("DDtestaddress123456789abcdef");
    mockRequest.params.push_back(-50.0);
    // Should reject negative amounts when implemented
}

// Test 15: redeemdigidollar RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_redeemdigidollar_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("redeemdigidollar", &redeemdigidollar);

    // Test valid redemption parameters
    mockRequest.strMethod = "redeemdigidollar";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");  // Position ID
    mockRequest.params.push_back(25.0);  // Amount to redeem

    const CRPCCommand* command = tableRPC.find("redeemdigidollar");
    BOOST_CHECK(command != nullptr);

    // Test invalid position ID format
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("invalid_txid");
    mockRequest.params.push_back(25.0);
    // Should validate position ID format when implemented
}

// Test 16: getdigidollaraddress RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_getdigidollaraddress_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("getdigidollaraddress", &getdigidollaraddress);

    // Test address generation
    mockRequest.strMethod = "getdigidollaraddress";
    mockRequest.params = UniValue(UniValue::VARR);

    const CRPCCommand* command = tableRPC.find("getdigidollaraddress");
    BOOST_CHECK(command != nullptr);

    // Test with label parameter
    mockRequest.params.push_back("test_label");
    // Should accept optional label when implemented
}

// Test 17: validateddaddress RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_validateddaddress_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("validateddaddress", &validateddaddress);

    // Test valid DD address
    mockRequest.strMethod = "validateddaddress";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("DDtestaddress123456789abcdef");

    const CRPCCommand* command = tableRPC.find("validateddaddress");
    BOOST_CHECK(command != nullptr);

    // Test invalid address formats
    std::vector<std::string> invalidAddresses = {
        "invalid_address",
        "1BitcoinAddress123456789",
        "dgb1qtest",
        "",
        "DD",  // Too short
        "DDtoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooolong"  // Too long
    };

    for (const auto& addr : invalidAddresses) {
        mockRequest.params = UniValue(UniValue::VARR);
        mockRequest.params.push_back(addr);
        // Should return isvalid: false for each when implemented
    }
}

// Test 18: getdigidollarbalance RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_getdigidollarbalance_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("getdigidollarbalance", &getdigidollarbalance);

    // Test total balance (no address)
    mockRequest.strMethod = "getdigidollarbalance";
    mockRequest.params = UniValue(UniValue::VARR);

    const CRPCCommand* command = tableRPC.find("getdigidollarbalance");
    BOOST_CHECK(command != nullptr);

    // Test balance for specific address
    mockRequest.params.push_back("DDtestaddress123456789abcdef");

    // Test with minconf parameter
    mockRequest.params.push_back(6);  // Min confirmations
}

// Test 19: estimatecollateral RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_estimatecollateral_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("estimatecollateral", &estimatecollateral);

    // Test collateral estimation
    mockRequest.strMethod = "estimatecollateral";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back(100.0);  // DD amount
    mockRequest.params.push_back(3);      // Lock tier

    const CRPCCommand* command = tableRPC.find("estimatecollateral");
    BOOST_CHECK(command != nullptr);

    // Test with custom oracle price
    mockRequest.params.push_back(5000);  // Price in cents
}

// Test 20: listdigidollarpositions RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_listdigidollarpositions_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("listdigidollarpositions", &listdigidollarpositions);

    // Test listing all positions
    mockRequest.strMethod = "listdigidollarpositions";
    mockRequest.params = UniValue(UniValue::VARR);

    const CRPCCommand* command = tableRPC.find("listdigidollarpositions");
    BOOST_CHECK(command != nullptr);

    // Test with active_only filter
    mockRequest.params.push_back(true);  // Active only

    // Test with specific tier filter
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back(false);  // Include inactive
    mockRequest.params.push_back(3);      // Specific tier
}

// Test 21: getoracleprice RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_getoracleprice_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("getoracleprice", &getoracleprice);

    // Test current oracle price
    mockRequest.strMethod = "getoracleprice";
    mockRequest.params = UniValue(UniValue::VARR);

    const CRPCCommand* command = tableRPC.find("getoracleprice");
    BOOST_CHECK(command != nullptr);
}

// Test 22: getredemptioninfo RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_getredemptioninfo_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("getredemptioninfo", &getredemptioninfo);

    // Test redemption info for position
    mockRequest.strMethod = "getredemptioninfo";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");

    const CRPCCommand* command = tableRPC.find("getredemptioninfo");
    BOOST_CHECK(command != nullptr);
}

// Test 23: listdigidollartxs RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_listdigidollartxs_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("listdigidollartxs", &listdigidollartxs);

    // Test listing all DD transactions
    mockRequest.strMethod = "listdigidollartxs";
    mockRequest.params = UniValue(UniValue::VARR);

    const CRPCCommand* command = tableRPC.find("listdigidollartxs");
    BOOST_CHECK(command != nullptr);

    // Test with count and skip parameters
    mockRequest.params.push_back(10);   // Count
    mockRequest.params.push_back(0);    // Skip

    // Test with address filter
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back(10);
    mockRequest.params.push_back(0);
    mockRequest.params.push_back("DDtestaddress123456789abcdef");
}

// Test 24: getprotectionstatus RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_getprotectionstatus_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("getprotectionstatus", &getprotectionstatus);

    // Test protection system status
    mockRequest.strMethod = "getprotectionstatus";
    mockRequest.params = UniValue(UniValue::VARR);

    const CRPCCommand* command = tableRPC.find("getprotectionstatus");
    BOOST_CHECK(command != nullptr);
}

// Test 25: listdigidollaraddresses RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_listdigidollaraddresses_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("listdigidollaraddresses", &listdigidollaraddresses);

    // Test listing all DD addresses
    mockRequest.strMethod = "listdigidollaraddresses";
    mockRequest.params = UniValue(UniValue::VARR);

    const CRPCCommand* command = tableRPC.find("listdigidollaraddresses");
    BOOST_CHECK(command != nullptr);
}

// Test 26: importdigidollaraddress RPC Command Tests
BOOST_FIXTURE_TEST_CASE(digidollar_test_importdigidollaraddress_rpc, DigiDollarRPCTestSetup)
{
    tableRPC.appendCommand("importdigidollaraddress", &importdigidollaraddress);

    // Test importing watch-only DD address
    mockRequest.strMethod = "importdigidollaraddress";
    mockRequest.params = UniValue(UniValue::VARR);
    mockRequest.params.push_back("DDtestaddress123456789abcdef");

    const CRPCCommand* command = tableRPC.find("importdigidollaraddress");
    BOOST_CHECK(command != nullptr);

    // Test with label
    mockRequest.params.push_back("watch_only_label");

    // Test with rescan flag
    mockRequest.params.push_back(true);  // Rescan
}

// Test 27: RPC Command Parameter Validation
BOOST_FIXTURE_TEST_CASE(digidollar_test_rpc_parameter_validation, DigiDollarRPCTestSetup)
{
    // Test that all RPC commands handle missing required parameters gracefully
    std::vector<std::string> commandsRequiringParams = {
        "mintdigidollar",
        "senddigidollar",
        "redeemdigidollar",
        "validateddaddress",
        "estimatecollateral",
        "getredemptioninfo",
        "importdigidollaraddress"
    };

    for (const auto& cmd : commandsRequiringParams) {
        // Register command first (this would be done in real implementation)
        mockRequest.strMethod = cmd;
        mockRequest.params = UniValue(UniValue::VARR);  // Empty params

        // Each command should handle missing parameters appropriately
        // (This will be tested when commands are actually implemented)
    }
}

// Test 28: RPC Response Format Consistency
BOOST_FIXTURE_TEST_CASE(digidollar_test_rpc_response_format, DigiDollarRPCTestSetup)
{
    // Test that all transaction-creating RPC commands return consistent format
    std::vector<std::string> txCommands = {
        "mintdigidollar",
        "senddigidollar",
        "redeemdigidollar"
    };

    // Each should return an object with at least txid field when successful
    // (This will be validated when commands are implemented)

    // Test that all informational commands return consistent data types
    std::vector<std::string> infoCommands = {
        "getdigidollarbalance",
        "getoracleprice",
        "getprotectionstatus"
    };

    // Each should return appropriate data types (numbers, booleans, strings, objects)
    // (This will be validated when commands are implemented)
}

// Test 29: RPC Error Handling Consistency
BOOST_FIXTURE_TEST_CASE(digidollar_test_rpc_error_handling, DigiDollarRPCTestSetup)
{
    // Test that all RPC commands use appropriate JSON-RPC error codes
    std::map<std::string, int> expectedErrorCodes = {
        {"RPC_INVALID_PARAMETER", -8},
        {"RPC_WALLET_ERROR", -4},
        {"RPC_WALLET_INSUFFICIENT_FUNDS", -6},
        {"RPC_INVALID_ADDRESS_OR_KEY", -5}
    };

    // Commands should use these standard error codes consistently
    // (This will be validated when error handling is implemented)
}

// Test 30: RPC Help Text Validation
BOOST_FIXTURE_TEST_CASE(digidollar_test_rpc_help_text, DigiDollarRPCTestSetup)
{
    std::vector<std::string> allCommands = {
        "mintdigidollar", "senddigidollar", "redeemdigidollar",
        "getdigidollaraddress", "validateddaddress", "listdigidollaraddresses",
        "importdigidollaraddress", "getdigidollarbalance", "estimatecollateral",
        "getredemptioninfo", "listdigidollartxs", "listdigidollarpositions",
        "getoracleprice", "getprotectionstatus"
    };

    // Each command should have comprehensive help text
    for (const auto& cmd : allCommands) {
        mockRequest.strMethod = cmd;
        mockRequest.mode = JSONRPCRequest::GET_HELP;

        // Should throw runtime_error with help text
        // (This will be validated when help text is implemented)
    }
}

*/

BOOST_AUTO_TEST_SUITE_END()