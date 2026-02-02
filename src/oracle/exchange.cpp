// Copyright (c) 2024 The DigiByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <oracle/exchange.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

#include <logging.h>
#include <random.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/time.h>
#include <univalue.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

namespace ExchangeAPI {

// CURL callback function for writing response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response)
{
    size_t total_size = size * nmemb;
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

/**
 * BaseExchangeFetcher Implementation
 */

std::string BaseExchangeFetcher::HttpGet(const std::string& url)
{
#ifdef HAVE_LIBCURL
    // Real libcurl implementation
    CURL* curl = curl_easy_init();
    if (!curl) {
        LogPrint(BCLog::DIGIDOLLAR, "HttpGet: Failed to initialize CURL\n");
        return "";
    }

    std::string response;
    CURLcode res;

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));

    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DigiByte-Oracle/1.0");

    // Enable SSL certificate verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Perform request
    res = curl_easy_perform(curl);

    // Cleanup
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LogPrint(BCLog::DIGIDOLLAR, "HttpGet: CURL error: %s\n", curl_easy_strerror(res));
        return "";
    }

    LogPrint(BCLog::DIGIDOLLAR, "HttpGet: Successfully fetched %d bytes from %s\n", response.size(), url);
    return response;
#else
    // libcurl not available — cannot make HTTP requests
    // CRITICAL: Do NOT return fake/mock data here. Return empty string so the oracle fails gracefully.
    LogPrintf("Oracle: ERROR - libcurl not available, cannot fetch price from %s. Build with libcurl support.\n", url);
    return "";
#endif
}

std::string BaseExchangeFetcher::ExtractJsonValue(const std::string& json, const std::string& key)
{
    // Simple JSON value extraction - in real implementation would use proper JSON parser
    std::string search_key = "\"" + key + "\":";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return "";
    }

    pos += search_key.length();

    // Skip whitespace and quotes
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) {
        pos++;
    }

    // Extract value until quote, comma, or closing brace
    size_t end_pos = pos;
    while (end_pos < json.length() && json[end_pos] != '"' && json[end_pos] != ',' && json[end_pos] != '}') {
        end_pos++;
    }

    return json.substr(pos, end_pos - pos);
}

CAmount BaseExchangeFetcher::ConvertToMicroUSD(const std::string& price_str)
{
    try {
        double price_usd = std::stod(price_str);
        return ConvertToMicroUSD(price_usd);
    } catch (const std::exception& e) {
        LogPrintf("Oracle: Failed to convert price string '%s': %s\n", price_str, e.what());
        return 0;
    }
}

CAmount BaseExchangeFetcher::ConvertToMicroUSD(double price_usd)
{
    if (price_usd <= 0 || price_usd > 100) { // Sanity check
        return 0;
    }
    return static_cast<CAmount>(price_usd * 1000000); // Convert to micro-USD (1,000,000 = $1.00)
}

/**
 * BinanceFetcher Implementation
 */

BinanceFetcher::BinanceFetcher()
    : BaseExchangeFetcher("Binance", "https://data-api.binance.vision")  // Use vision API - not geo-blocked
{
}

CAmount BinanceFetcher::FetchPrice()
{
    // Try DGBUSDT first
    CAmount price = FetchDGBUSDT();
    if (price > 0) {
        return price;
    }

    // Fallback to DGB/BTC * BTC/USDT
    return FetchDGBBTC_BTCUSDT();
}

CAmount BinanceFetcher::FetchDGBUSDT()
{
    std::string url = base_url + "/api/v3/ticker/price?symbol=DGBUSDT";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Empty response\n");
        return 0;
    }

    // Parse JSON using UniValue
    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("price")) {
            LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Missing 'price' field\n");
            return 0;
        }

        std::string price_str = json["price"].get_str();
        CAmount price_micro_usd = ConvertToMicroUSD(price_str);

        LogPrint(BCLog::DIGIDOLLAR, "Binance: %s (%lld micro-USD)\n", price_str.c_str(), price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

CAmount BinanceFetcher::FetchDGBBTC_BTCUSDT()
{
    // Fetch DGB/BTC first, then BTC/USDT, and calculate DGB/USD
    // This is used when DGBUSDT pair is not available

    // Fetch DGB/BTC
    std::string dgb_btc_url = base_url + "/api/v3/ticker/price?symbol=DGBBTC";
    std::string dgb_btc_response = HttpGet(dgb_btc_url);

    if (dgb_btc_response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Empty DGB/BTC response\n");
        return 0;
    }

    // Fetch BTC/USDT
    std::string btc_usdt_url = base_url + "/api/v3/ticker/price?symbol=BTCUSDT";
    std::string btc_usdt_response = HttpGet(btc_usdt_url);

    if (btc_usdt_response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Empty BTC/USDT response\n");
        return 0;
    }

    try {
        UniValue dgb_btc_json;
        if (!dgb_btc_json.read(dgb_btc_response)) {
            LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Failed to parse DGB/BTC JSON\n");
            return 0;
        }

        UniValue btc_usdt_json;
        if (!btc_usdt_json.read(btc_usdt_response)) {
            LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Failed to parse BTC/USDT JSON\n");
            return 0;
        }

        if (!dgb_btc_json.isObject() || !dgb_btc_json.exists("price")) {
            LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Missing 'price' in DGB/BTC\n");
            return 0;
        }

        if (!btc_usdt_json.isObject() || !btc_usdt_json.exists("price")) {
            LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Missing 'price' in BTC/USDT\n");
            return 0;
        }

        double dgb_btc = std::stod(dgb_btc_json["price"].get_str());
        double btc_usdt = std::stod(btc_usdt_json["price"].get_str());
        double dgb_usd = dgb_btc * btc_usdt;

        CAmount price_micro_usd = ConvertToMicroUSD(dgb_usd);
        LogPrint(BCLog::DIGIDOLLAR, "Binance (via BTC): DGB/BTC=%f, BTC/USDT=%f, DGB/USD=%f (%lld micro-USD)\n",
                 dgb_btc, btc_usdt, dgb_usd, price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "BinanceFetcher: Error in DGB/BTC calculation: %s\n", e.what());
        return 0;
    }
}

/**
 * CoinbaseFetcher Implementation
 */

CoinbaseFetcher::CoinbaseFetcher()
    : BaseExchangeFetcher("Coinbase", "https://api.coinbase.com")
{
}

CAmount CoinbaseFetcher::FetchPrice()
{
    // Use Coinbase spot price API
    // Endpoint: /v2/prices/DGB-USD/spot
    // JSON format: {"data":{"amount":"0.01234","currency":"USD"}}
    std::string url = base_url + "/v2/prices/DGB-USD/spot";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "CoinbaseFetcher: Empty response\n");
        return 0;
    }

    // Parse nested JSON: data.amount
    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinbaseFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("data")) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinbaseFetcher: Missing 'data' field\n");
            return 0;
        }

        const UniValue& data = json["data"];
        if (!data.isObject() || !data.exists("amount")) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinbaseFetcher: Missing 'amount' field\n");
            return 0;
        }

        std::string priceStr = data["amount"].get_str();
        CAmount priceMicroUSD = ConvertToMicroUSD(priceStr);

        LogPrint(BCLog::DIGIDOLLAR, "Coinbase: %s (%lld micro-USD)\n", priceStr.c_str(), priceMicroUSD);
        return priceMicroUSD;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "CoinbaseFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

CAmount CoinbaseFetcher::FetchFromCoinbasePro()
{
    // Deprecated - keeping stub for compatibility
    return 0;
}

CAmount CoinbaseFetcher::FetchFromCoinbaseRegular()
{
    // Deprecated - now using FetchPrice() directly
    return 0;
}

/**
 * KrakenFetcher Implementation
 */

KrakenFetcher::KrakenFetcher()
    : BaseExchangeFetcher("Kraken", "https://api.kraken.com")
{
}

CAmount KrakenFetcher::FetchPrice()
{
    std::string url = base_url + "/0/public/Ticker?pair=DGBUSD";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "KrakenFetcher: Empty response\n");
        return 0;
    }

    // Parse JSON: {"result":{"DGBUSD":{"c":["0.01234","1.0"]}}}
    // Need to access array element c[0]
    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "KrakenFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("result")) {
            LogPrint(BCLog::DIGIDOLLAR, "KrakenFetcher: Missing 'result' field\n");
            return 0;
        }

        const UniValue& result = json["result"];
        if (!result.isObject() || !result.exists("DGBUSD")) {
            LogPrint(BCLog::DIGIDOLLAR, "KrakenFetcher: Missing 'DGBUSD' field\n");
            return 0;
        }

        const UniValue& dgbusd = result["DGBUSD"];
        if (!dgbusd.isObject() || !dgbusd.exists("c")) {
            LogPrint(BCLog::DIGIDOLLAR, "KrakenFetcher: Missing 'c' field\n");
            return 0;
        }

        const UniValue& cArray = dgbusd["c"];
        if (!cArray.isArray() || cArray.size() < 1) {
            LogPrint(BCLog::DIGIDOLLAR, "KrakenFetcher: 'c' field is not an array or is empty\n");
            return 0;
        }

        // Access first element of array: c[0]
        std::string priceStr = cArray[0].get_str();
        CAmount priceMicroUSD = ConvertToMicroUSD(priceStr);

        LogPrint(BCLog::DIGIDOLLAR, "Kraken: %s (%lld micro-USD)\n", priceStr.c_str(), priceMicroUSD);
        return priceMicroUSD;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "KrakenFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

std::string KrakenFetcher::ParseKrakenResponse(const std::string& response)
{
    // Deprecated - now using direct UniValue parsing in FetchPrice()
    return "0.05050";
}

/**
 * BittrexFetcher Implementation
 */

BittrexFetcher::BittrexFetcher()
    : BaseExchangeFetcher("Bittrex", "https://api.bittrex.com")
{
}

CAmount BittrexFetcher::FetchPrice()
{
    // Bittrex v3 API: https://api.bittrex.com/v3/markets/DGB-USD/ticker
    std::string url = base_url + "/v3/markets/DGB-USD/ticker";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "BittrexFetcher: Empty response\n");
        return 0;
    }

    // Parse JSON using UniValue
    // Expected format: {"symbol":"DGB-USD","lastTradeRate":"0.01234"}
    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "BittrexFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("lastTradeRate")) {
            LogPrint(BCLog::DIGIDOLLAR, "BittrexFetcher: Missing 'lastTradeRate' field\n");
            return 0;
        }

        std::string price_str = json["lastTradeRate"].get_str();
        CAmount price_micro_usd = ConvertToMicroUSD(price_str);

        // Validate range ($0.0001 to $10.00)
        if (price_micro_usd < 100 || price_micro_usd > 10000000) {
            LogPrint(BCLog::DIGIDOLLAR, "BittrexFetcher: Price out of range: %lld micro-USD\n", price_micro_usd);
            return 0;
        }

        LogPrint(BCLog::DIGIDOLLAR, "Bittrex: %s (%lld micro-USD)\n", price_str.c_str(), price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "BittrexFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

std::string BittrexFetcher::ParseBittrexResponse(const std::string& response)
{
    // Deprecated - now using direct UniValue parsing in FetchPrice()
    return "";
}

/**
 * PoloniexFetcher Implementation
 */

PoloniexFetcher::PoloniexFetcher()
    : BaseExchangeFetcher("Poloniex", "https://poloniex.com")
{
}

CAmount PoloniexFetcher::FetchPrice()
{
    // Poloniex API: https://api.poloniex.com/markets/DGB_USDT/price
    std::string url = "https://api.poloniex.com/markets/DGB_USDT/price";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "PoloniexFetcher: Empty response\n");
        return 0;
    }

    // Parse JSON using UniValue
    // Expected format: {"symbol":"DGB_USDT","price":"0.01234"}
    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "PoloniexFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("price")) {
            LogPrint(BCLog::DIGIDOLLAR, "PoloniexFetcher: Missing 'price' field\n");
            return 0;
        }

        std::string price_str = json["price"].get_str();
        CAmount price_micro_usd = ConvertToMicroUSD(price_str);

        // Validate range ($0.0001 to $10.00)
        if (price_micro_usd < 100 || price_micro_usd > 10000000) {
            LogPrint(BCLog::DIGIDOLLAR, "PoloniexFetcher: Price out of range: %lld micro-USD\n", price_micro_usd);
            return 0;
        }

        LogPrint(BCLog::DIGIDOLLAR, "Poloniex: %s (%lld micro-USD)\n", price_str.c_str(), price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "PoloniexFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

std::string PoloniexFetcher::ParsePoloniexResponse(const std::string& response)
{
    // Deprecated - now using direct UniValue parsing in FetchPrice()
    return "";
}

/**
 * MessariFetcher Implementation
 */

MessariFetcher::MessariFetcher()
    : BaseExchangeFetcher("Messari", "https://data.messari.io")
{
}

CAmount MessariFetcher::FetchPrice()
{
    std::string url = base_url + "/api/v1/assets/dgb/metrics/market-data";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "MessariFetcher: Empty response\n");
        return 0;
    }

    // Parse JSON: {"data":{"market_data":{"price_usd":0.01234}}}
    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "MessariFetcher: Failed to parse JSON\n");
            return 0;
        }

        // Navigate nested JSON structure: data.market_data.price_usd
        if (!json.isObject() || !json.exists("data")) {
            LogPrint(BCLog::DIGIDOLLAR, "MessariFetcher: Missing 'data' field\n");
            return 0;
        }

        const UniValue& data = json["data"];
        if (!data.isObject() || !data.exists("market_data")) {
            LogPrint(BCLog::DIGIDOLLAR, "MessariFetcher: Missing 'market_data' field\n");
            return 0;
        }

        const UniValue& marketData = data["market_data"];
        if (!marketData.isObject() || !marketData.exists("price_usd")) {
            LogPrint(BCLog::DIGIDOLLAR, "MessariFetcher: Missing 'price_usd' field\n");
            return 0;
        }

        double priceUSD = marketData["price_usd"].get_real();
        CAmount priceMicroUSD = ConvertToMicroUSD(priceUSD);

        LogPrint(BCLog::DIGIDOLLAR, "Messari: $%.6f (%lld micro-USD)\n", priceUSD, priceMicroUSD);
        return priceMicroUSD;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "MessariFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

/**
 * KuCoinFetcher Implementation
 */

KuCoinFetcher::KuCoinFetcher()
    : BaseExchangeFetcher("KuCoin", "https://api.kucoin.com")
{
}

CAmount KuCoinFetcher::FetchPrice()
{
    std::string url = base_url + "/api/v1/market/orderbook/level1?symbol=DGB-USDT";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "KuCoinFetcher: Empty response\n");
        return 0;
    }

    // Parse JSON using UniValue
    // Expected format: {"data":{"price":"0.01234"}}
    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "KuCoinFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("data")) {
            LogPrint(BCLog::DIGIDOLLAR, "KuCoinFetcher: Missing 'data' field\n");
            return 0;
        }

        const UniValue& data = json["data"];
        if (!data.isObject() || !data.exists("price")) {
            LogPrint(BCLog::DIGIDOLLAR, "KuCoinFetcher: Missing 'price' field in data\n");
            return 0;
        }

        std::string price_str = data["price"].get_str();
        CAmount price_micro_usd = ConvertToMicroUSD(price_str);

        // Validate range ($0.0001 to $10.00)
        if (price_micro_usd < 100 || price_micro_usd > 10000000) {
            LogPrint(BCLog::DIGIDOLLAR, "KuCoinFetcher: Price out of range: %lld micro-USD\n", price_micro_usd);
            return 0;
        }

        LogPrint(BCLog::DIGIDOLLAR, "KuCoin: %s (%lld micro-USD)\n", price_str.c_str(), price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "KuCoinFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

/**
 * CryptoComFetcher Implementation
 */

CryptoComFetcher::CryptoComFetcher()
    : BaseExchangeFetcher("Crypto.com", "https://api.crypto.com")
{
}

CAmount CryptoComFetcher::FetchPrice()
{
    // Crypto.com Exchange API v1: /exchange/v1/public/get-tickers
    // Response: {"result":{"data":[{"i":"DGB_USD","a":"0.006314",...}]}}
    std::string url = base_url + "/exchange/v1/public/get-tickers?instrument_name=DGB_USD";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: Empty response\n");
        return 0;
    }

    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("result")) {
            LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: Missing 'result' field\n");
            return 0;
        }

        const UniValue& result = json["result"];
        if (!result.isObject() || !result.exists("data")) {
            LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: Missing 'data' field in result\n");
            return 0;
        }

        const UniValue& data = result["data"];
        if (!data.isArray() || data.size() < 1) {
            LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: 'data' is not an array or is empty\n");
            return 0;
        }

        const UniValue& ticker = data[0];
        if (!ticker.isObject() || !ticker.exists("a")) {
            LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: Missing 'a' (ask/price) field\n");
            return 0;
        }

        std::string price_str = ticker["a"].get_str();
        CAmount price_micro_usd = ConvertToMicroUSD(price_str);

        // Validate range ($0.0001 to $10.00)
        if (price_micro_usd < 100 || price_micro_usd > 10000000) {
            LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: Price out of range: %lld micro-USD\n", price_micro_usd);
            return 0;
        }

        LogPrint(BCLog::DIGIDOLLAR, "Crypto.com: %s (%lld micro-USD)\n", price_str.c_str(), price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "CryptoComFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}


/**
 * GateIOFetcher Implementation
 */

GateIOFetcher::GateIOFetcher()
    : BaseExchangeFetcher("Gate.io", "https://api.gateio.ws")
{
}

CAmount GateIOFetcher::FetchPrice()
{
    // Gate.io API v4: /api/v4/spot/tickers?currency_pair=DGB_USDT
    // Response: [{"currency_pair":"DGB_USDT","last":"0.006272",...}]
    std::string url = base_url + "/api/v4/spot/tickers?currency_pair=DGB_USDT";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "GateIOFetcher: Empty response\n");
        return 0;
    }

    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "GateIOFetcher: Failed to parse JSON\n");
            return 0;
        }

        // Response is an array with one element
        if (!json.isArray() || json.size() < 1) {
            LogPrint(BCLog::DIGIDOLLAR, "GateIOFetcher: Response is not an array or is empty\n");
            return 0;
        }

        const UniValue& ticker = json[0];
        if (!ticker.isObject() || !ticker.exists("last")) {
            LogPrint(BCLog::DIGIDOLLAR, "GateIOFetcher: Missing 'last' field\n");
            return 0;
        }

        std::string price_str = ticker["last"].get_str();
        CAmount price_micro_usd = ConvertToMicroUSD(price_str);

        // Validate range ($0.0001 to $10.00)
        if (price_micro_usd < 100 || price_micro_usd > 10000000) {
            LogPrint(BCLog::DIGIDOLLAR, "GateIOFetcher: Price out of range: %lld micro-USD\n", price_micro_usd);
            return 0;
        }

        LogPrint(BCLog::DIGIDOLLAR, "Gate.io: %s (%lld micro-USD)\n", price_str.c_str(), price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "GateIOFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

/**
 * HTXFetcher Implementation
 */

HTXFetcher::HTXFetcher()
    : BaseExchangeFetcher("HTX", "https://api.htx.com")
{
}

CAmount HTXFetcher::FetchPrice()
{
    // HTX API: /market/detail/merged?symbol=dgbusdt
    // Response: {"tick":{"close":0.006272,...}}
    std::string url = base_url + "/market/detail/merged?symbol=dgbusdt";
    std::string response = HttpGet(url);

    if (response.empty()) {
        LogPrint(BCLog::DIGIDOLLAR, "HTXFetcher: Empty response\n");
        return 0;
    }

    try {
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "HTXFetcher: Failed to parse JSON\n");
            return 0;
        }

        if (!json.isObject() || !json.exists("tick")) {
            LogPrint(BCLog::DIGIDOLLAR, "HTXFetcher: Missing 'tick' field\n");
            return 0;
        }

        const UniValue& tick = json["tick"];
        if (!tick.isObject() || !tick.exists("close")) {
            LogPrint(BCLog::DIGIDOLLAR, "HTXFetcher: Missing 'close' field in tick\n");
            return 0;
        }

        double priceUSD = tick["close"].get_real();
        CAmount price_micro_usd = ConvertToMicroUSD(priceUSD);

        // Validate range ($0.0001 to $10.00)
        if (price_micro_usd < 100 || price_micro_usd > 10000000) {
            LogPrint(BCLog::DIGIDOLLAR, "HTXFetcher: Price out of range: %lld micro-USD\n", price_micro_usd);
            return 0;
        }

        LogPrint(BCLog::DIGIDOLLAR, "HTX: $%.6f (%lld micro-USD)\n", priceUSD, price_micro_usd);
        return price_micro_usd;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "HTXFetcher: Error parsing response: %s\n", e.what());
        return 0;
    }
}

/**
 * CoinMarketCapFetcher Implementation
 */

CoinMarketCapFetcher::CoinMarketCapFetcher()
    : BaseExchangeFetcher("CoinMarketCap", "https://pro-api.coinmarketcap.com")
{
}

CAmount CoinMarketCapFetcher::FetchPrice()
{
    try {
        // Get API key from config
        std::string apiKey = gArgs.GetArg("-coinmarketcap-api-key", "");
        if (apiKey.empty()) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: No API key configured (use -coinmarketcap-api-key)\n");
            return 0;
        }

        std::string url = base_url + "/v1/cryptocurrency/quotes/latest?symbol=DGB&convert=USD";

        std::string response;
#ifdef HAVE_LIBCURL
        // Real libcurl implementation with headers
        CURL* curl = curl_easy_init();
        if (!curl) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: Failed to initialize CURL\n");
            return 0;
        }

        CURLcode res;

        // Add API key header
        struct curl_slist* headers = nullptr;
        std::string authHeader = "X-CMC_PRO_API_KEY: " + apiKey;
        headers = curl_slist_append(headers, authHeader.c_str());
        headers = curl_slist_append(headers, "Accept: application/json");

        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Set headers
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set write callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Set timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));

        // Set User-Agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "DigiByte-Oracle/1.0");

        // Enable SSL
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Perform request
        res = curl_easy_perform(curl);

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: CURL error: %s\n", curl_easy_strerror(res));
            return 0;
        }
#else
        // libcurl not available — cannot fetch CoinMarketCap data
        LogPrintf("Oracle: ERROR - libcurl not available, cannot fetch CoinMarketCap price. Build with libcurl support.\n");
        return 0;
#endif

        // Parse JSON using UniValue
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: Failed to parse JSON response\n");
            return 0;
        }

        // Extract nested price: data.DGB.quote.USD.price
        if (!json["data"].isObject() || !json["data"]["DGB"].isObject()) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: Invalid JSON structure (missing data.DGB)\n");
            return 0;
        }

        UniValue dgb = json["data"]["DGB"];
        if (!dgb["quote"].isObject() || !dgb["quote"]["USD"].isObject()) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: Invalid JSON structure (missing quote.USD)\n");
            return 0;
        }

        UniValue usd = dgb["quote"]["USD"];
        if (!usd["price"].isNum()) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: Price field is not a number\n");
            return 0;
        }

        double priceUSD = usd["price"].get_real();
        CAmount priceMicroUSD = ConvertToMicroUSD(priceUSD);

        LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap: $%.6f (%lld micro-USD)\n", priceUSD, priceMicroUSD);
        return priceMicroUSD;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "CoinMarketCap fetch error: %s\n", e.what());
        return 0;
    }
}

/**
 * CoinGeckoFetcher Implementation
 */

CoinGeckoFetcher::CoinGeckoFetcher()
    : BaseExchangeFetcher("CoinGecko", "https://api.coingecko.com")
{
}

CAmount CoinGeckoFetcher::FetchPrice()
{
    try {
        std::string url = base_url + "/api/v3/simple/price?ids=digibyte&vs_currencies=usd";
        std::string response = HttpGet(url);

        if (response.empty()) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinGecko: Empty response\n");
            return 0;
        }

        // Parse JSON using UniValue
        UniValue json;
        if (!json.read(response)) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinGecko: Failed to parse JSON response\n");
            return 0;
        }

        // Extract nested price: digibyte.usd
        if (!json["digibyte"].isObject()) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinGecko: Invalid JSON structure (missing digibyte object)\n");
            return 0;
        }

        UniValue digibyte = json["digibyte"];
        if (!digibyte["usd"].isNum()) {
            LogPrint(BCLog::DIGIDOLLAR, "CoinGecko: USD price field is not a number\n");
            return 0;
        }

        double priceUSD = digibyte["usd"].get_real();
        CAmount priceMicroUSD = ConvertToMicroUSD(priceUSD);

        LogPrint(BCLog::DIGIDOLLAR, "CoinGecko: $%.6f (%lld micro-USD)\n", priceUSD, priceMicroUSD);
        return priceMicroUSD;

    } catch (const std::exception& e) {
        LogPrint(BCLog::DIGIDOLLAR, "CoinGecko fetch error: %s\n", e.what());
        return 0;
    }
}

/**
 * MultiExchangeAggregator Implementation
 */

MultiExchangeAggregator::MultiExchangeAggregator()
{
    InitializeFetchers();
}

MultiExchangeAggregator::~MultiExchangeAggregator()
{
}

void MultiExchangeAggregator::InitializeFetchers()
{
    // Initialize exchange fetchers - only use exchanges that actually list DGB
    // VERIFIED WORKING (Dec 2025):
    // - Binance (via data-api.binance.vision - not geo-blocked)
    // - CoinGecko (aggregator - always works)
    // - KuCoin (DGB-USDT)
    // - Gate.io (DGB_USDT)
    // - HTX/Huobi (dgbusdt)
    // - Crypto.com (DGB_USD via exchange/v1 API)
    //
    // NOT AVAILABLE / REMOVED:
    // - Coinbase: DGB not tradeable (info page only)
    // - Kraken: DGB not listed
    // - Messari: Requires API key now
    // - CoinMarketCap: Requires API key (kept but optional)

    fetchers.push_back(std::make_unique<BinanceFetcher>());
    fetchers.push_back(std::make_unique<CoinGeckoFetcher>());
    fetchers.push_back(std::make_unique<KuCoinFetcher>());
    fetchers.push_back(std::make_unique<GateIOFetcher>());
    fetchers.push_back(std::make_unique<HTXFetcher>());
    fetchers.push_back(std::make_unique<CryptoComFetcher>());
    fetchers.push_back(std::make_unique<CoinMarketCapFetcher>());  // Optional - requires API key

    LogPrintf("Oracle: Initialized %d exchange fetchers\n", fetchers.size());
}

CAmount MultiExchangeAggregator::FetchAggregatePrice()
{
    std::vector<ExchangePrice> prices = FetchAllPrices();
    last_prices = prices;

    if (prices.size() < min_required_sources) {
        LogPrintf("Oracle: Insufficient price sources (%d < %d required)\n",
                 prices.size(), min_required_sources);
        return 0;
    }

    // Filter outliers
    std::vector<ExchangePrice> filtered_prices = FilterOutliers(prices);

    if (filtered_prices.size() < min_required_sources) {
        LogPrintf("Oracle: Insufficient price sources after filtering (%d < %d required)\n",
                 filtered_prices.size(), min_required_sources);
        return 0;
    }

    // Calculate final price
    CAmount final_price;
    if (use_weighted_median) {
        final_price = CalculateWeightedMedian(filtered_prices);
    } else {
        final_price = CalculateMedianPrice(filtered_prices);
    }

    LogPriceResults(filtered_prices, final_price);
    return final_price;
}

std::vector<MultiExchangeAggregator::ExchangePrice> MultiExchangeAggregator::FetchAllPrices()
{
    std::vector<ExchangePrice> prices;
    int64_t timestamp = GetTime();

    for (const auto& fetcher : fetchers) {
        try {
            CAmount price = fetcher->FetchPrice();
            bool success = (price > 0);

            double weight = GetExchangeWeight(fetcher->GetExchangeName());
            prices.emplace_back(fetcher->GetExchangeName(), price, timestamp, success, weight);

            LogPrintf("Oracle: %s price: %lld micro-USD (success: %s)\n",
                     fetcher->GetExchangeName(), price, success ? "true" : "false");
        }
        catch (const std::exception& e) {
            LogPrintf("Oracle: Exception fetching from %s: %s\n",
                     fetcher->GetExchangeName(), e.what());
            prices.emplace_back(fetcher->GetExchangeName(), 0, timestamp, false);
        }
    }

    return FilterValidPrices(prices);
}

CAmount MultiExchangeAggregator::CalculateMedianPrice(const std::vector<ExchangePrice>& prices)
{
    if (prices.empty()) {
        return 0;
    }

    std::vector<CAmount> price_values;
    for (const auto& price : prices) {
        price_values.push_back(price.price_micro_usd);
    }

    std::sort(price_values.begin(), price_values.end());

    size_t size = price_values.size();
    if (size % 2 == 0) {
        // Even number - average of middle two
        return (price_values[size/2 - 1] + price_values[size/2]) / 2;
    } else {
        // Odd number - middle element
        return price_values[size/2];
    }
}

CAmount MultiExchangeAggregator::CalculateWeightedMedian(const std::vector<ExchangePrice>& prices)
{
    if (prices.empty()) {
        return 0;
    }

    // For simplicity, fall back to regular median for now
    // Real implementation would calculate proper weighted median
    return CalculateMedianPrice(prices);
}

CAmount MultiExchangeAggregator::CalculateWeightedAverage(const std::vector<ExchangePrice>& prices)
{
    if (prices.empty()) {
        return 0;
    }

    double weighted_sum = 0.0;
    double total_weight = 0.0;

    for (const auto& price : prices) {
        weighted_sum += price.price_micro_usd * price.weight;
        total_weight += price.weight;
    }

    if (total_weight == 0.0) {
        return 0;
    }

    return static_cast<CAmount>(weighted_sum / total_weight);
}

std::vector<MultiExchangeAggregator::ExchangePrice> MultiExchangeAggregator::FilterOutliers(
    const std::vector<ExchangePrice>& prices)
{
    if (prices.size() < 3) {
        return prices; // Need at least 3 data points for outlier detection
    }

    // Calculate median first
    CAmount median = CalculateMedianPrice(prices);
    if (median == 0) {
        return prices;
    }

    // Filter prices within threshold
    std::vector<ExchangePrice> filtered;
    CAmount threshold = static_cast<CAmount>(median * outlier_threshold);

    for (const auto& price : prices) {
        CAmount deviation = std::abs(price.price_micro_usd - median);
        if (deviation <= threshold) {
            filtered.push_back(price);
        } else {
            LogPrintf("Oracle: Filtered outlier from %s: %lld micro-USD (median: %lld, threshold: %lld)\n",
                     price.exchange, price.price_micro_usd, median, threshold);
        }
    }

    return filtered;
}

bool MultiExchangeAggregator::IsOutlier(CAmount price, const std::vector<ExchangePrice>& prices)
{
    CAmount median = CalculateMedianPrice(prices);
    if (median == 0) {
        return false;
    }

    CAmount threshold = static_cast<CAmount>(median * outlier_threshold);
    CAmount deviation = std::abs(price - median);
    return deviation > threshold;
}

bool MultiExchangeAggregator::HasSufficientData() const
{
    return GetSuccessfulSourceCount() >= min_required_sources;
}

size_t MultiExchangeAggregator::GetSuccessfulSourceCount() const
{
    size_t count = 0;
    for (const auto& price : last_prices) {
        if (price.success) {
            count++;
        }
    }
    return count;
}

void MultiExchangeAggregator::SetExchangeWeight(const std::string& exchange, double weight)
{
    // Store weights in last_prices for now
    for (auto& price : last_prices) {
        if (price.exchange == exchange) {
            price.weight = weight;
        }
    }
}

double MultiExchangeAggregator::GetExchangeWeight(const std::string& exchange)
{
    // Default weights for different exchanges (based on liquidity and reliability)
    if (exchange == "Binance") return 1.5;          // Higher weight for Binance (most liquid)
    if (exchange == "CoinMarketCap") return 1.4;    // High weight for CMC (aggregated data)
    if (exchange == "CoinGecko") return 1.4;        // High weight for CoinGecko (aggregated data)
    if (exchange == "Coinbase") return 1.3;         // High weight for Coinbase
    if (exchange == "Kraken") return 1.2;           // Good weight for Kraken
    if (exchange == "Messari") return 1.1;          // Good weight for Messari (verified data)
    if (exchange == "KuCoin") return 1.0;           // Standard weight for KuCoin
    if (exchange == "Crypto.com") return 1.0;       // Standard weight for Crypto.com
    if (exchange == "Bittrex") return 0.9;          // Lower weight for Bittrex
    if (exchange == "Poloniex") return 0.8;         // Lower weight for Poloniex

    return 1.0; // Default weight
}

std::vector<MultiExchangeAggregator::ExchangePrice> MultiExchangeAggregator::FilterValidPrices(
    const std::vector<ExchangePrice>& prices)
{
    std::vector<ExchangePrice> valid_prices;
    for (const auto& price : prices) {
        if (price.success && price.price_micro_usd > 0) {
            valid_prices.push_back(price);
        }
    }
    return valid_prices;
}

void MultiExchangeAggregator::LogPriceResults(const std::vector<ExchangePrice>& prices, CAmount final_price)
{
    LogPrintf("Oracle: Price aggregation results:\n");
    for (const auto& price : prices) {
        LogPrintf("Oracle:   %s: %lld micro-USD (weight: %.1f)\n",
                 price.exchange, price.price_micro_usd, price.weight);
    }
    LogPrintf("Oracle: Final aggregated price: %lld micro-USD ($%.6f)\n",
             final_price, static_cast<double>(final_price) / 1000000.0);
}

} // namespace ExchangeAPI