// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIGIBYTE_ORACLE_NODE_H
#define DIGIBYTE_ORACLE_NODE_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <consensus/amount.h>
#include <key.h>
#include <primitives/oracle.h>
#include <pubkey.h>

/**
 * Oracle Node Daemon
 * Handles price fetching, signing, and broadcasting for oracle nodes
 */
class OracleNode
{
private:
    uint32_t oracle_id;
    CKey private_key;
    CPubKey public_key;
    std::vector<std::string> exchange_endpoints;
    std::thread price_thread;
    std::atomic<bool> running;
    std::atomic<bool> enabled;
    mutable std::mutex mtx_price;

    // Current price data
    CAmount current_price{0};
    int64_t last_update_time{0};
    int64_t last_broadcast_time{0};

    // Configuration
    int price_update_interval{30};  // seconds
    int broadcast_interval{300};    // 5 minutes

public:
    //! Constructor
    OracleNode();
    OracleNode(uint32_t oracle_id_in, const CKey& private_key_in);

    //! Destructor
    ~OracleNode();

    //! Configuration
    bool Initialize(uint32_t oracle_id_in, const std::string& private_key_hex);
    void SetExchangeEndpoints(const std::vector<std::string>& endpoints);
    void SetUpdateInterval(int seconds) { price_update_interval = seconds; }
    void SetBroadcastInterval(int seconds) { broadcast_interval = seconds; }

    //! Control functions
    void Start();
    void Stop();
    bool IsRunning() const { return running.load(); }
    bool IsEnabled() const { return enabled.load(); }
    void SetEnabled(bool enable) { enabled.store(enable); }

    //! Price functions
    CAmount GetCurrentPrice() const;
    int64_t GetLastUpdateTime() const;
    bool HasValidPrice() const;

    //! Oracle functions
    uint32_t GetOracleId() const { return oracle_id; }
    CPubKey GetPublicKey() const { return public_key; }

    //! Message creation and broadcasting
    COraclePriceMessage CreatePriceMessage(CAmount price, int64_t timestamp);
    bool BroadcastPriceMessage(const COraclePriceMessage& message);

private:
    //! Main thread function
    void PriceThreadFunc();

    //! Price fetching
    void FetchAndUpdatePrice();
    CAmount FetchMedianPrice();

    //! Price broadcasting
    void BroadcastCurrentPrice();
    bool ShouldBroadcast() const;

    //! Validation
    bool ValidateOracleId() const;
    bool ValidatePrivateKey() const;
};

/**
 * Exchange Price Fetcher
 * Fetches prices from multiple exchanges and calculates median
 */
class ExchangePriceFetcher
{
public:
    struct ExchangePrice {
        std::string exchange;
        CAmount price;
        int64_t timestamp;
        bool valid;

        ExchangePrice() : price(0), timestamp(0), valid(false) {}
        ExchangePrice(const std::string& exchange_in, CAmount price_in, int64_t timestamp_in)
            : exchange(exchange_in), price(price_in), timestamp(timestamp_in), valid(true) {}
    };

private:
    std::vector<std::string> exchange_endpoints;
    int timeout_seconds{10};

public:
    //! Constructor
    ExchangePriceFetcher();
    explicit ExchangePriceFetcher(const std::vector<std::string>& endpoints);

    //! Configuration
    void SetExchangeEndpoints(const std::vector<std::string>& endpoints);
    void SetTimeout(int seconds) { timeout_seconds = seconds; }

    //! Price fetching
    std::vector<ExchangePrice> FetchAllPrices();
    CAmount GetMedianPrice();
    CAmount GetMedianPrice(const std::vector<ExchangePrice>& prices);

    //! Individual exchange fetchers
    ExchangePrice FetchFromBinance();
    ExchangePrice FetchFromCoinbase();
    ExchangePrice FetchFromKraken();
    ExchangePrice FetchFromBittrex();
    ExchangePrice FetchFromPoloniex();

private:
    //! HTTP request helper
    std::string HttpRequest(const std::string& url);

    //! JSON parsing helpers
    CAmount ParseBinancePrice(const std::string& response);
    CAmount ParseCoinbasePrice(const std::string& response);
    CAmount ParseKrakenPrice(const std::string& response);
    CAmount ParseBittrexPrice(const std::string& response);
    CAmount ParsePoloniexPrice(const std::string& response);

    //! Utility functions
    bool IsValidPrice(CAmount price) const;
    std::vector<ExchangePrice> FilterValidPrices(const std::vector<ExchangePrice>& prices) const;
};

/**
 * Oracle Manager
 * Manages multiple oracle nodes and provides central control
 */
class OracleManager
{
private:
    std::vector<std::unique_ptr<OracleNode>> oracle_nodes;
    mutable std::mutex mtx_manager;
    bool initialized{false};

public:
    //! Constructor/Destructor
    OracleManager();
    ~OracleManager();

    //! Initialization
    bool Initialize();
    void Shutdown();

    //! Oracle management
    bool AddOracleNode(uint32_t oracle_id, const std::string& private_key_hex);
    bool RemoveOracleNode(uint32_t oracle_id);
    OracleNode* GetOracleNode(uint32_t oracle_id);

    //! Control functions
    void StartAll();
    void StopAll();
    void EnableOracle(uint32_t oracle_id, bool enable);

    //! Status functions
    size_t GetActiveOracleCount() const;
    std::vector<uint32_t> GetActiveOracleIds() const;
    bool IsOracleRunning(uint32_t oracle_id) const;

    //! Global functions
    static OracleManager& GetInstance();
    static void StartOracleService();
    static void StopOracleService();
};

//! Global oracle manager instance
extern std::unique_ptr<OracleManager> g_oracle_manager;

#endif // DIGIBYTE_ORACLE_NODE_H