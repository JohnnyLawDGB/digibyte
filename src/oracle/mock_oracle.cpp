// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/mock_oracle.h>

#include <chain.h>
#include <chainparams.h>
#include <logging.h>
#include <node/chainstate.h>
#include <util/time.h>
#include <validation.h>

#include <algorithm>

// Global instance pointer
MockOracleManager* MockOracleManager::instance = nullptr;

MockOracleManager::MockOracleManager()
    : mockPrice(1),            // Default: $0.01 per DGB = 1 cent per DGB
      lastUpdateHeight(0),
      enabled(true)
{
}

MockOracleManager& MockOracleManager::GetInstance()
{
    if (!instance) {
        instance = new MockOracleManager();
    }
    return *instance;
}

CAmount MockOracleManager::GetCurrentPrice() const
{
    LOCK(cs_price);
    return mockPrice;
}

void MockOracleManager::SetMockPrice(CAmount price)
{
    LOCK(cs_price);

    // Validate price is reasonable
    if (price <= 0) {
        LogPrintf("MockOracleManager: Invalid price %d, ignoring\n", price);
        return;
    }

    // Price is in cents per DGB
    // Range: $0.0001 per DGB (0.01 cents) to $1000 per DGB (100,000 cents)
    const CAmount MIN_PRICE = 1;              // 0.01 cents per DGB = $0.0001 per DGB (minimum reasonable)
    const CAmount MAX_PRICE = 100000;         // 100,000 cents per DGB = $1000 per DGB (maximum reasonable)

    if (price < MIN_PRICE || price > MAX_PRICE) {
        LogPrintf("MockOracleManager: Price %d out of reasonable range [%d, %d], clamping\n",
                  price, MIN_PRICE, MAX_PRICE);
        price = std::max(MIN_PRICE, std::min(MAX_PRICE, price));
    }

    mockPrice = price;

    // Update height from chain tip if available
    // Note: Height tracking is optional for mock oracle
    // In production this would integrate with node context
    lastUpdateHeight = 0; // TODO: Get from node context when available

    LogPrintf("MockOracleManager: Price updated to %d cents per DGB\n", mockPrice);
}

bool MockOracleManager::IsEnabled() const
{
    LOCK(cs_price);
    return enabled;
}

void MockOracleManager::SetEnabled(bool enable)
{
    LOCK(cs_price);
    enabled = enable;
    LogPrintf("MockOracleManager: %s\n",
              enable ? "Enabled" : "Disabled");
}

int64_t MockOracleManager::GetLastUpdateHeight() const
{
    LOCK(cs_price);
    return lastUpdateHeight;
}

COracleBundle MockOracleManager::CreateMockBundle(int height)
{
    LOCK(cs_price);

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(height);

    // Create 8 mock oracle messages (minimum required for consensus)
    for (uint32_t i = 0; i < ORACLE_CONSENSUS_REQUIRED; i++) {
        COraclePriceMessage msg;
        msg.oracle_id = i;
        msg.price_satoshis = mockPrice;
        msg.timestamp = GetTime();

        // Create mock signature (64 bytes of deterministic data)
        msg.signature.resize(64);
        for (size_t j = 0; j < 64; j++) {
            msg.signature[j] = static_cast<unsigned char>((i * 64 + j) % 256);
        }

        bundle.messages.push_back(msg);
    }

    LogPrint(BCLog::DIGIDOLLAR, "MockOracleManager: Created bundle for epoch %d with price %d\n",
             bundle.epoch, mockPrice);

    return bundle;
}

void MockOracleManager::SimulateVolatility(int percentChange)
{
    LOCK(cs_price);

    if (percentChange == 0) {
        return;
    }

    // Calculate new price based on percentage change
    CAmount oldPrice = mockPrice;
    CAmount change = (mockPrice * percentChange) / 100;
    CAmount newPrice = mockPrice + change;

    // Ensure price stays in reasonable range
    const CAmount MIN_PRICE = 1000;           // $0.00001 per DGB
    const CAmount MAX_PRICE = 10000000000LL;  // $100 per DGB

    newPrice = std::max(MIN_PRICE, std::min(MAX_PRICE, newPrice));

    mockPrice = newPrice;

    // Update height is optional for mock oracle
    lastUpdateHeight = 0; // TODO: Get from node context when available

    LogPrintf("MockOracleManager: Simulated %d%% volatility: %d -> %d satoshis/USD\n",
              percentChange, oldPrice, mockPrice);
}

void MockOracleManager::Reset()
{
    LOCK(cs_price);

    mockPrice = 1000000;      // Reset to $0.01 per DGB
    lastUpdateHeight = 0;
    enabled = true;

    LogPrintf("MockOracleManager: Reset to default state (price: %d)\n", mockPrice);
}