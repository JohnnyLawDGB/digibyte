// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_ORACLE_MOCK_ORACLE_H
#define DIGIBYTE_ORACLE_MOCK_ORACLE_H

#include <consensus/amount.h>
#include <primitives/oracle.h>
#include <sync.h>
#include <uint256.h>

#include <cstdint>

/**
 * Mock Oracle Manager for RegTest
 * Provides a singleton mock oracle for testing DigiDollar functionality in RegTest mode.
 * This allows testing of price-dependent features without requiring real oracle nodes.
 */
class MockOracleManager
{
private:
    static MockOracleManager* instance;

    CAmount mockPrice;              // Price in satoshis per USD (e.g., 1000000 = $0.01/DGB)
    int64_t lastUpdateHeight;       // Height of last price update
    bool enabled;                   // Whether mock oracle is enabled

    mutable RecursiveMutex cs_price;

    // Private constructor for singleton
    MockOracleManager();

public:
    // Singleton access
    static MockOracleManager& GetInstance();

    // Delete copy constructor and assignment operator
    MockOracleManager(const MockOracleManager&) = delete;
    MockOracleManager& operator=(const MockOracleManager&) = delete;

    /**
     * Get current mock oracle price
     * @return Price in satoshis per USD
     */
    CAmount GetCurrentPrice() const;

    /**
     * Set mock oracle price
     * @param price Price in satoshis per USD
     */
    void SetMockPrice(CAmount price);

    /**
     * Check if mock oracle is enabled
     * @return true if enabled
     */
    bool IsEnabled() const;

    /**
     * Enable or disable mock oracle
     * @param enable true to enable, false to disable
     */
    void SetEnabled(bool enable);

    /**
     * Get last update height
     * @return Block height of last price update
     */
    int64_t GetLastUpdateHeight() const;

    /**
     * Create mock oracle bundle for a given height
     * Simulates 8 of 15 oracle signatures with consistent price
     * @param height Block height for the bundle
     * @return Mock oracle bundle with valid structure
     */
    COracleBundle CreateMockBundle(int height);

    /**
     * Simulate price volatility for testing
     * @param percentChange Percentage change (positive or negative)
     */
    void SimulateVolatility(int percentChange);

    /**
     * Reset mock oracle to default state
     */
    void Reset();
};

#endif // DIGIBYTE_ORACLE_MOCK_ORACLE_H