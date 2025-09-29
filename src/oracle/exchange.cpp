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
#include <util/time.h>

namespace ExchangeAPI {

// CURL callback function for writing response data
[[maybe_unused]] static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response)
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
    // For mock implementation, return simulated responses
    LogPrintf("Oracle: Mock HTTP GET to %s\n", url);

    // Simulate different responses based on URL
    if (url.find("binance") != std::string::npos) {
        return R"({"symbol":"DGBUSDT","price":"0.05000"})";
    } else if (url.find("coinbase") != std::string::npos) {
        return R"({"data":{"rates":{"DGB":"0.04950"}}})";
    } else if (url.find("kraken") != std::string::npos) {
        return R"({"result":{"DGBUSD":{"c":["0.05050","1.00000000"]}}})";
    } else if (url.find("bittrex") != std::string::npos) {
        return R"({"success":true,"message":"","result":{"Last":0.05025}})";
    } else if (url.find("poloniex") != std::string::npos) {
        return R"({"USDT_DGB":{"last":"0.04975"}})";
    }

    return R"({"error":"Mock response"})";
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

CAmount BaseExchangeFetcher::ConvertToCents(const std::string& price_str)
{
    try {
        double price_usd = std::stod(price_str);
        return ConvertToCents(price_usd);
    } catch (const std::exception& e) {
        LogPrintf("Oracle: Failed to convert price string '%s': %s\n", price_str, e.what());
        return 0;
    }
}

CAmount BaseExchangeFetcher::ConvertToCents(double price_usd)
{
    if (price_usd <= 0 || price_usd > 100) { // Sanity check
        return 0;
    }
    return static_cast<CAmount>(price_usd * 100); // Convert to cents
}

/**
 * BinanceFetcher Implementation
 */

BinanceFetcher::BinanceFetcher()
    : BaseExchangeFetcher("Binance", "https://api.binance.com")
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
        return 0;
    }

    std::string price_str = ExtractJsonValue(response, "price");
    return ConvertToCents(price_str);
}

CAmount BinanceFetcher::FetchDGBBTC_BTCUSDT()
{
    // Mock implementation - would fetch both DGB/BTC and BTC/USDT rates
    return ConvertToCents(0.05); // $0.05
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
    // Try Coinbase Pro first
    CAmount price = FetchFromCoinbasePro();
    if (price > 0) {
        return price;
    }

    // Fallback to regular Coinbase API
    return FetchFromCoinbaseRegular();
}

CAmount CoinbaseFetcher::FetchFromCoinbasePro()
{
    std::string url = "https://api.pro.coinbase.com/products/DGB-USD/ticker";
    std::string response = HttpGet(url);

    if (response.empty()) {
        return 0;
    }

    std::string price_str = ExtractJsonValue(response, "price");
    return ConvertToCents(price_str);
}

CAmount CoinbaseFetcher::FetchFromCoinbaseRegular()
{
    std::string url = base_url + "/v2/exchange-rates?currency=DGB";
    std::string response = HttpGet(url);

    if (response.empty()) {
        return 0;
    }

    // Extract from nested JSON structure
    // For mock, return fixed value
    return ConvertToCents(0.0495); // $0.04950
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
        return 0;
    }

    std::string price_str = ParseKrakenResponse(response);
    return ConvertToCents(price_str);
}

std::string KrakenFetcher::ParseKrakenResponse(const std::string& response)
{
    // Kraken returns nested JSON with ticker data
    // For mock implementation, extract hardcoded value
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
    std::string url = base_url + "/v3/markets/USD-DGB/ticker";
    std::string response = HttpGet(url);

    if (response.empty()) {
        return 0;
    }

    std::string price_str = ParseBittrexResponse(response);
    return ConvertToCents(price_str);
}

std::string BittrexFetcher::ParseBittrexResponse(const std::string& response)
{
    // Extract last price from Bittrex response
    return "0.05025";
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
    std::string url = base_url + "/public?command=returnTicker";
    std::string response = HttpGet(url);

    if (response.empty()) {
        return 0;
    }

    std::string price_str = ParsePoloniexResponse(response);
    return ConvertToCents(price_str);
}

std::string PoloniexFetcher::ParsePoloniexResponse(const std::string& response)
{
    // Extract USDT_DGB price from Poloniex ticker
    return "0.04975";
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
    fetchers.push_back(std::make_unique<BinanceFetcher>());
    fetchers.push_back(std::make_unique<CoinbaseFetcher>());
    fetchers.push_back(std::make_unique<KrakenFetcher>());
    fetchers.push_back(std::make_unique<BittrexFetcher>());
    fetchers.push_back(std::make_unique<PoloniexFetcher>());

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

            LogPrintf("Oracle: %s price: %d cents (success: %s)\n",
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
        price_values.push_back(price.price_cents);
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
        weighted_sum += price.price_cents * price.weight;
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
        CAmount deviation = std::abs(price.price_cents - median);
        if (deviation <= threshold) {
            filtered.push_back(price);
        } else {
            LogPrintf("Oracle: Filtered outlier from %s: %d cents (median: %d, threshold: %d)\n",
                     price.exchange, price.price_cents, median, threshold);
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
    // Default weights for different exchanges
    if (exchange == "Binance") return 1.5;      // Higher weight for Binance (most liquid)
    if (exchange == "Coinbase") return 1.3;     // High weight for Coinbase
    if (exchange == "Kraken") return 1.2;       // Good weight for Kraken
    if (exchange == "Bittrex") return 1.0;      // Standard weight
    if (exchange == "Poloniex") return 0.8;     // Lower weight for Poloniex

    return 1.0; // Default weight
}

std::vector<MultiExchangeAggregator::ExchangePrice> MultiExchangeAggregator::FilterValidPrices(
    const std::vector<ExchangePrice>& prices)
{
    std::vector<ExchangePrice> valid_prices;
    for (const auto& price : prices) {
        if (price.success && price.price_cents > 0) {
            valid_prices.push_back(price);
        }
    }
    return valid_prices;
}

void MultiExchangeAggregator::LogPriceResults(const std::vector<ExchangePrice>& prices, CAmount final_price)
{
    LogPrintf("Oracle: Price aggregation results:\n");
    for (const auto& price : prices) {
        LogPrintf("Oracle:   %s: %d cents (weight: %.1f)\n",
                 price.exchange, price.price_cents, price.weight);
    }
    LogPrintf("Oracle: Final aggregated price: %d cents ($%.4f)\n",
             final_price, static_cast<double>(final_price) / 100.0);
}

} // namespace ExchangeAPI