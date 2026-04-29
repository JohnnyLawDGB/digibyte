// Copyright (c) 2025 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/mock_oracle.h>

#include <chain.h>
#include <chainparams.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <logging.h>
#include <util/strencodings.h>
#include <node/chainstate.h>
#include <util/time.h>
#include <validation.h>

#include <algorithm>

// Global instance pointer
MockOracleManager* MockOracleManager::instance = nullptr;

MockOracleManager::MockOracleManager()
    : mockPriceMicroUSD(6500),   // Default: $0.0065 per DGB = 6500 micro-USD (realistic DGB price)
      lastUpdateHeight(0),
      enabled(true)
{
    InitTestKeys();
}

void MockOracleManager::InitTestKeys()
{
    // Generate deterministic test oracle private keys from SHA256("digibyte_regtest_oracle_N")
    // 7 oracles for regtest (matches testnet 4-of-7 consensus)
    for (uint32_t i = 0; i < 7; i++) {
        std::string seed = "digibyte_regtest_oracle_" + std::to_string(i);
        uint256 hash;
        CSHA256().Write((const unsigned char*)seed.data(), seed.size()).Finalize(hash.begin());

        CKey key;
        key.Set(hash.begin(), hash.end(), true);
        if (key.IsValid()) {
            testOracleKeys[i] = key;
            LogPrintf("MockOracleManager: Initialized test key for oracle %d (pubkey=%s)\n",
                     i, HexStr(key.GetPubKey()));
        } else {
            LogPrintf("MockOracleManager: WARNING - Failed to create test key for oracle %d\n", i);
        }
    }
}

CKey MockOracleManager::GetTestKey(uint32_t oracle_id) const
{
    LOCK(cs_price);
    auto it = testOracleKeys.find(oracle_id);
    if (it != testOracleKeys.end()) {
        return it->second;
    }
    return CKey(); // Invalid key
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
    // SECURITY (DGB-SEC-005): Runtime guard — reject on non-REGTEST networks
    if (Params().GetChainType() != ChainType::REGTEST) {
        return 0;
    }
    LOCK(cs_price);
    return mockPriceMicroUSD;
}

void MockOracleManager::SetMockPrice(CAmount price_micro_usd)
{
    // SECURITY (DGB-SEC-005): Runtime guard — mock oracle must only operate in REGTEST
    if (Params().GetChainType() != ChainType::REGTEST) {
        LogPrintf("MockOracleManager: SECURITY - SetMockPrice rejected on non-REGTEST network\n");
        return;
    }

    LOCK(cs_price);

    // Validate price is reasonable
    if (price_micro_usd <= 0) {
        LogPrintf("MockOracleManager: Invalid price %lld micro-USD, ignoring\n", price_micro_usd);
        return;
    }

    // Price is in micro-USD (1,000,000 = $1.00)
    // Range: $0.0001 per DGB (100 micro-USD) to $1000 per DGB (1,000,000,000 micro-USD)
    const CAmount MIN_PRICE = 100;              // 100 micro-USD = $0.0001 per DGB (minimum reasonable)
    const CAmount MAX_PRICE = 1000000000;       // 1,000,000,000 micro-USD = $1000 per DGB (maximum reasonable)

    if (price_micro_usd < MIN_PRICE || price_micro_usd > MAX_PRICE) {
        LogPrintf("MockOracleManager: Price %lld micro-USD out of reasonable range [%lld, %lld], clamping\n",
                  price_micro_usd, MIN_PRICE, MAX_PRICE);
        price_micro_usd = std::max(MIN_PRICE, std::min(MAX_PRICE, price_micro_usd));
    }

    mockPriceMicroUSD = price_micro_usd;

    // Update height from chain tip if available
    // Note: Height tracking is optional for mock oracle
    // In production this would integrate with node context
    lastUpdateHeight = 0; // TODO: Get from node context when available

    LogPrintf("MockOracleManager: Price updated to %lld micro-USD ($%.6f per DGB)\n",
              mockPriceMicroUSD, static_cast<double>(mockPriceMicroUSD) / 1000000.0);
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

COracleBundle MockOracleManager::CreateMockBundle(int height, int64_t block_time)
{
    LOCK(cs_price);

    COracleBundle bundle;
    bundle.epoch = GetCurrentEpoch(height);
    const int64_t bundle_timestamp = block_time > 0 ? block_time : GetTime();

    // Determine how many oracle messages to create based on chain config
    // Use min(available test keys, ORACLE_CONSENSUS_REQUIRED) for backward compat
    uint32_t num_messages = std::min(static_cast<uint32_t>(testOracleKeys.size()),
                                     static_cast<uint32_t>(ORACLE_CONSENSUS_REQUIRED));
    if (num_messages == 0) num_messages = ORACLE_CONSENSUS_REQUIRED; // fallback

    for (uint32_t i = 0; i < num_messages; i++) {
        COraclePriceMessage msg;
        msg.oracle_id = i;
        msg.price_micro_usd = mockPriceMicroUSD;
        msg.timestamp = bundle_timestamp;
        msg.block_height = height;

        // Sign with real Schnorr signature if test key is available
        auto key_it = testOracleKeys.find(i);
        if (key_it != testOracleKeys.end()) {
            msg.oracle_pubkey = XOnlyPubKey(key_it->second.GetPubKey());
            if (!msg.SignPhase2(key_it->second)) {
                LogPrintf("MockOracleManager: WARNING - Failed to sign message for oracle %d\n", i);
            }
        } else {
            // Fallback: fake signature (will fail verification but maintains backward compat)
            msg.schnorr_sig.resize(64);
            for (size_t j = 0; j < 64; j++) {
                msg.schnorr_sig[j] = static_cast<unsigned char>((i * 64 + j) % 256);
            }
        }

        bundle.messages.push_back(msg);
    }

    bundle.median_price_micro_usd = mockPriceMicroUSD;
    bundle.timestamp = bundle_timestamp;

    LogPrint(BCLog::DIGIDOLLAR, "MockOracleManager: Created bundle for epoch %d with %d messages, price %lld micro-USD\n",
             bundle.epoch, num_messages, mockPriceMicroUSD);

    return bundle;
}

void MockOracleManager::SimulateVolatility(int percentChange)
{
    LOCK(cs_price);

    if (percentChange == 0) {
        return;
    }

    // Calculate new price based on percentage change
    CAmount oldPrice = mockPriceMicroUSD;
    CAmount change = (mockPriceMicroUSD * percentChange) / 100;
    CAmount newPrice = mockPriceMicroUSD + change;

    // Ensure price stays in reasonable range (micro-USD: 1,000,000 = $1.00)
    const CAmount MIN_PRICE = 100;              // 100 micro-USD = $0.0001 per DGB
    const CAmount MAX_PRICE = 1000000000;       // 1,000,000,000 micro-USD = $1000 per DGB

    newPrice = std::max(MIN_PRICE, std::min(MAX_PRICE, newPrice));

    mockPriceMicroUSD = newPrice;

    // Update height is optional for mock oracle
    lastUpdateHeight = 0; // TODO: Get from node context when available

    LogPrintf("MockOracleManager: Simulated %d%% volatility: %lld -> %lld micro-USD ($%.6f -> $%.6f per DGB)\n",
              percentChange, oldPrice, mockPriceMicroUSD,
              static_cast<double>(oldPrice) / 1000000.0, static_cast<double>(mockPriceMicroUSD) / 1000000.0);
}

void MockOracleManager::Reset()
{
    LOCK(cs_price);

    mockPriceMicroUSD = 6500;  // Reset to 6500 micro-USD = $0.0065 per DGB (realistic DGB price)
    lastUpdateHeight = 0;
    enabled = true;

    LogPrintf("MockOracleManager: Reset to default state (price: %lld micro-USD = $%.6f per DGB)\n",
              mockPriceMicroUSD, static_cast<double>(mockPriceMicroUSD) / 1000000.0);
}
