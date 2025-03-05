#include "market_data.h"

#include <iostream>
#include <algorithm>
#include <chrono>

// Include JSON library
#define NLOHMANN_JSON_VERSION_MAJOR 3
#define NLOHMANN_JSON_VERSION_MINOR 11
#define NLOHMANN_JSON_VERSION_PATCH 2
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MarketDataClient::MarketDataClient(std::shared_ptr<ApiClient> api_client)
    : api_client_(api_client), running_(false) {
}

MarketDataClient::~MarketDataClient() {
    stop();
}

void MarketDataClient::start() {
    if (running_) return;
    
    running_ = true;
    
    // Connect to the WebSocket
    api_client_->connectWebSocket([this](const std::string& message) {
        this->processMessage(message);
    });
    
    // Subscribe to all currently subscribed instruments
    std::vector<std::string> instruments;
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        instruments = subscriptions_;
    }
    
    for (const auto& instrument : instruments) {
        api_client_->subscribeToOrderbook(instrument);
    }
}

void MarketDataClient::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Unsubscribe from all instruments
    std::vector<std::string> instruments;
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        instruments = subscriptions_;
    }
    
    for (const auto& instrument : instruments) {
        api_client_->unsubscribeFromOrderbook(instrument);
    }
    
    // Close the WebSocket
    api_client_->closeWebSocket();
}

void MarketDataClient::subscribe(const std::string& instrument) {
    bool needs_subscribe = false;
    
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        if (std::find(subscriptions_.begin(), subscriptions_.end(), instrument) == subscriptions_.end()) {
            subscriptions_.push_back(instrument);
            needs_subscribe = true;
        }
    }
    
    if (needs_subscribe && running_) {
        // Fetch initial orderbook
        fetchInitialOrderbook(instrument);
        
        // Subscribe to updates
        api_client_->subscribeToOrderbook(instrument);
    }
}

void MarketDataClient::unsubscribe(const std::string& instrument) {
    bool needs_unsubscribe = false;
    
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = std::find(subscriptions_.begin(), subscriptions_.end(), instrument);
        if (it != subscriptions_.end()) {
            subscriptions_.erase(it);
            needs_unsubscribe = true;
        }
    }
    
    if (needs_unsubscribe && running_) {
        // Unsubscribe from updates
        api_client_->unsubscribeFromOrderbook(instrument);
        
        // Remove the orderbook
        std::lock_guard<std::mutex> lock(orderbooks_mutex_);
        orderbooks_.erase(instrument);
    }
}

std::vector<std::string> MarketDataClient::getSubscribedInstruments() const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    return subscriptions_;
}

Orderbook MarketDataClient::getOrderbook(const std::string& instrument) const {
    std::lock_guard<std::mutex> lock(orderbooks_mutex_);
    if (orderbooks_.find(instrument) != orderbooks_.end()) {
        return orderbooks_.at(instrument);
    }
    
    // Return an empty orderbook if the instrument is not found
    Orderbook empty;
    empty.instrument = instrument;
    return empty;
}

void MarketDataClient::setOrderbookCallback(OrderbookUpdateCallback callback) {
    orderbook_callback_ = callback;
}

void MarketDataClient::processMessage(const std::string& message) {
    try {
        // Parse the JSON message
        json data = json::parse(message);
        
        // Check if this is an orderbook update
        if (data.contains("method") && data["method"] == "subscription" &&
            data.contains("params") && data["params"].contains("channel")) {
            
            std::string channel = data["params"]["channel"];
            
            // Check if this is an orderbook update
            if (channel.find("book.") == 0) {
                std::string instrument = channel.substr(5);
                size_t first_dot = instrument.find(".");
                if (first_dot != std::string::npos) {
                    instrument = instrument.substr(0, first_dot);
                }
                
                // Extract orderbook data
                json orderbook_data = data["params"]["data"];
                
                // Create an orderbook object
                Orderbook orderbook;
                orderbook.instrument = instrument;
                orderbook.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
                
                // Process bids
                if (orderbook_data.contains("bids")) {
                    for (const auto& bid : orderbook_data["bids"]) {
                        if (bid.is_array() && bid.size() >= 2) {
                            Orderbook::Level level;
                            level.price = bid[0];
                            level.size = bid[1];
                            orderbook.bids.push_back(level);
                        }
                    }
                }
                
                // Process asks
                if (orderbook_data.contains("asks")) {
                    for (const auto& ask : orderbook_data["asks"]) {
                        if (ask.is_array() && ask.size() >= 2) {
                            Orderbook::Level level;
                            level.price = ask[0];
                            level.size = ask[1];
                            orderbook.asks.push_back(level);
                        }
                    }
                }
                
                // Store the orderbook
                {
                    std::lock_guard<std::mutex> lock(orderbooks_mutex_);
                    orderbooks_[instrument] = orderbook;
                }
                
                // Notify callback
                if (orderbook_callback_) {
                    orderbook_callback_(orderbook);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing market data message: " << e.what() << std::endl;
    }
}

void MarketDataClient::fetchInitialOrderbook(const std::string& instrument) {
    try {
        // Fetch the initial orderbook from the REST API
        std::string response = api_client_->getOrderbook(instrument);
        
        // Parse the response
        json data = json::parse(response);
        
        if (data.contains("result")) {
            json result = data["result"];
            
            // Create an orderbook object
            Orderbook orderbook;
            orderbook.instrument = instrument;
            orderbook.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            
            // Process bids
            if (result.contains("bids")) {
                for (const auto& bid : result["bids"]) {
                    if (bid.is_array() && bid.size() >= 2) {
                        Orderbook::Level level;
                        level.price = bid[0];
                        level.size = bid[1];
                        orderbook.bids.push_back(level);
                    }
                }
            }
            
            // Process asks
            if (result.contains("asks")) {
                for (const auto& ask : result["asks"]) {
                    if (ask.is_array() && ask.size() >= 2) {
                        Orderbook::Level level;
                        level.price = ask[0];
                        level.size = ask[1];
                        orderbook.asks.push_back(level);
                    }
                }
            }
            
            // Store the orderbook
            {
                std::lock_guard<std::mutex> lock(orderbooks_mutex_);
                orderbooks_[instrument] = orderbook;
            }
            
            // Notify callback
            if (orderbook_callback_) {
                orderbook_callback_(orderbook);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error fetching initial orderbook: " << e.what() << std::endl;
    }
}