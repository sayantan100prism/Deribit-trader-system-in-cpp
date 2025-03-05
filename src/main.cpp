#include "api_client.h"
#include "order_manager.h"
#include "market_data.h"
#include "websocket_server.h"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

// Include JSON library
#define NLOHMANN_JSON_VERSION_MAJOR 3
#define NLOHMANN_JSON_VERSION_MINOR 11
#define NLOHMANN_JSON_VERSION_PATCH 2
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Signal handler flag
std::atomic<bool> running(true);

// Signal handler function
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

// Function to serialize an orderbook to JSON
std::string orderbookToJson(const Orderbook& orderbook) {
    json j;
    j["type"] = "orderbook";
    j["instrument"] = orderbook.instrument;
    j["timestamp"] = orderbook.timestamp;
    
    // Add bids
    j["bids"] = json::array();
    for (const auto& bid : orderbook.bids) {
        json level;
        level.push_back(bid.price);
        level.push_back(bid.size);
        j["bids"].push_back(level);
    }
    
    // Add asks
    j["asks"] = json::array();
    for (const auto& ask : orderbook.asks) {
        json level;
        level.push_back(ask.price);
        level.push_back(ask.size);
        j["asks"].push_back(level);
    }
    
    return j.dump();
}

int main(int argc, char* argv[]) {
    // Print welcome message
    std::cout << "Deribit Trader - High-Performance Trading System" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;
    
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Create API client with Deribit Test credentials
    ApiClient::Auth auth;
    auth.client_id = "m_B5zE25";
    auth.client_secret = "qwHcammuk8D-MEK4idg8urGt_ZAkfk4r_MuIzT9v1LE";
    auto api_client = std::make_shared<ApiClient>(auth);
    
    // Create order manager
    auto order_manager = std::make_shared<OrderManager>(api_client);
    
    // Create market data client
    auto market_data = std::make_shared<MarketDataClient>(api_client);
    
    // Create WebSocket server
    auto ws_server = std::make_shared<WebSocketServer>(8080);
    
    // Set up market data callback
    market_data->setOrderbookCallback([&ws_server](const Orderbook& orderbook) {
        // Convert orderbook to JSON and broadcast to subscribers
        std::string json = orderbookToJson(orderbook);
        ws_server->broadcastOrderbook(orderbook.instrument, json);
    });
    
    // Start the WebSocket server
    std::cout << "Starting WebSocket server on port 8080..." << std::endl;
    ws_server->start();
    std::cout << "WebSocket server running." << std::endl;
    
    // Start the market data client
    std::cout << "Starting market data client..." << std::endl;
    market_data->start();
    std::cout << "Market data client running." << std::endl;
    
    // Subscribe to some initial instruments
    std::cout << "Subscribing to initial instruments..." << std::endl;
    market_data->subscribe("BTC-PERPETUAL");
    market_data->subscribe("ETH-PERPETUAL");
    std::cout << "Subscribed to initial instruments." << std::endl;
    
    // Main event loop
    std::cout << "System is running. Press Ctrl+C to stop." << std::endl;
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Shutdown sequence
    std::cout << "Shutting down..." << std::endl;
    
    // Stop the market data client
    std::cout << "Stopping market data client..." << std::endl;
    market_data->stop();
    std::cout << "Market data client stopped." << std::endl;
    
    // Stop the WebSocket server
    std::cout << "Stopping WebSocket server..." << std::endl;
    ws_server->stop();
    std::cout << "WebSocket server stopped." << std::endl;
    
    std::cout << "Shutdown complete." << std::endl;
    
    return 0;
}