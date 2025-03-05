#pragma once

#include "api_client.h"

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <memory>
#include <atomic>

// Structure to represent an orderbook
struct Orderbook {
    struct Level {
        double price;
        double size;
    };
    
    std::string instrument;
    std::vector<Level> bids;
    std::vector<Level> asks;
    int64_t timestamp;
};

// Market data client to handle orderbook updates
class MarketDataClient {
public:
    using OrderbookUpdateCallback = std::function<void(const Orderbook&)>;
    
    MarketDataClient(std::shared_ptr<ApiClient> api_client);
    ~MarketDataClient();
    
    // Start/stop processing
    void start();
    void stop();
    
    // Subscription management
    void subscribe(const std::string& instrument);
    void unsubscribe(const std::string& instrument);
    std::vector<std::string> getSubscribedInstruments() const;
    
    // Current market data
    Orderbook getOrderbook(const std::string& instrument) const;
    
    // Update callback registration
    void setOrderbookCallback(OrderbookUpdateCallback callback);
    
    // Process incoming market data
    void processMessage(const std::string& message);
    
private:
    std::shared_ptr<ApiClient> api_client_;
    std::atomic<bool> running_;
    
    // Subscriptions
    mutable std::mutex subscriptions_mutex_;
    std::vector<std::string> subscriptions_;
    
    // Orderbooks
    mutable std::mutex orderbooks_mutex_;
    std::map<std::string, Orderbook> orderbooks_;
    
    // Callbacks
    OrderbookUpdateCallback orderbook_callback_;
    
    // Initial fetch for new subscriptions
    void fetchInitialOrderbook(const std::string& instrument);
};