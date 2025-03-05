#include "api_client.h"
#include "order_manager.h"
#include "market_data.h"
#include "websocket_server.h"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <sstream>

// Include JSON library
#define NLOHMANN_JSON_VERSION_MAJOR 3
#define NLOHMANN_JSON_VERSION_MINOR 11
#define NLOHMANN_JSON_VERSION_PATCH 2
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// A simple benchmarking class
class Benchmark {
public:
    Benchmark(const std::string& name) : name_(name) {}
    
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    double stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_).count();
        durations_.push_back(duration);
        return duration / 1000.0; // Convert to milliseconds
    }
    
    void reset() {
        durations_.clear();
    }
    
    double getAverageMs() const {
        if (durations_.empty()) return 0.0;
        double sum = std::accumulate(durations_.begin(), durations_.end(), 0.0);
        return (sum / durations_.size()) / 1000.0; // Convert to milliseconds
    }
    
    double getMinMs() const {
        if (durations_.empty()) return 0.0;
        return (*std::min_element(durations_.begin(), durations_.end())) / 1000.0; // Convert to milliseconds
    }
    
    double getMaxMs() const {
        if (durations_.empty()) return 0.0;
        return (*std::max_element(durations_.begin(), durations_.end())) / 1000.0; // Convert to milliseconds
    }
    
    double getMedianMs() const {
        if (durations_.empty()) return 0.0;
        std::vector<double> sorted = durations_;
        std::sort(sorted.begin(), sorted.end());
        size_t mid = sorted.size() / 2;
        return (sorted.size() % 2 == 0 ? (sorted[mid-1] + sorted[mid]) / 2.0 : sorted[mid]) / 1000.0; // Convert to milliseconds
    }
    
    double getP95Ms() const {
        if (durations_.empty()) return 0.0;
        std::vector<double> sorted = durations_;
        std::sort(sorted.begin(), sorted.end());
        size_t p95_idx = static_cast<size_t>(sorted.size() * 0.95);
        return sorted[p95_idx] / 1000.0; // Convert to milliseconds
    }
    
    size_t getSampleCount() const {
        return durations_.size();
    }
    
    void printStatistics(std::ostream& out = std::cout) const {
        out << "Benchmark: " << name_ << "\n";
        out << "  Samples: " << getSampleCount() << "\n";
        out << "  Min:     " << std::fixed << std::setprecision(3) << getMinMs() << " ms\n";
        out << "  Max:     " << std::fixed << std::setprecision(3) << getMaxMs() << " ms\n";
        out << "  Average: " << std::fixed << std::setprecision(3) << getAverageMs() << " ms\n";
        out << "  Median:  " << std::fixed << std::setprecision(3) << getMedianMs() << " ms\n";
        out << "  P95:     " << std::fixed << std::setprecision(3) << getP95Ms() << " ms\n";
    }
    
    void saveToCSV(const std::string& filename) const {
        std::ofstream file(filename);
        file << "sample,duration_us,duration_ms\n";
        for (size_t i = 0; i < durations_.size(); ++i) {
            file << i << "," << durations_[i] << "," << (durations_[i] / 1000.0) << "\n";
        }
    }
    
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::vector<double> durations_;
};

// Main benchmarking function
void runBenchmarks(int iterations = 100) {
    std::cout << "Starting benchmarks with " << iterations << " iterations each...\n";
    
    // Create API client
    ApiClient::Auth auth;
    auth.client_id = "m_B5zE25";
    auth.client_secret = "qwHcammuk8D-MEK4idg8urGt_ZAkfk4r_MuIzT9v1LE";
    auto api_client = std::make_shared<ApiClient>(auth);
    
    // Create order manager
    auto order_manager = std::make_shared<OrderManager>(api_client);
    
    // Create market data client
    auto market_data = std::make_shared<MarketDataClient>(api_client);
    
    // Create WebSocket server
    auto ws_server = std::make_shared<WebSocketServer>(8081);  // Use different port for benchmarking
    
    // Set up market data callback for end-to-end latency measurement
    Benchmark end_to_end_benchmark("End-to-End Market Data -> WebSocket Broadcast");
    market_data->setOrderbookCallback([&ws_server, &end_to_end_benchmark](const Orderbook& orderbook) {
        // Start measuring when we receive market data
        end_to_end_benchmark.start();
        
        // Convert orderbook to JSON (this would be part of the latency we're measuring)
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
        
        // Broadcast to subscribers (actual WebSocket send would happen here)
        ws_server->broadcastOrderbook(orderbook.instrument, j.dump());
        
        // Stop measuring after the broadcast is queued
        end_to_end_benchmark.stop();
    });
    
    // Start the server and market data client
    ws_server->start();
    market_data->start();
    
    // Wait for systems to initialize
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Benchmark 1: Order placement latency
    Benchmark order_placement_benchmark("Order Placement");
    for (int i = 0; i < iterations; ++i) {
        order_placement_benchmark.start();
        std::string order_id = order_manager->placeOrder(
            "BTC-PERPETUAL", 
            Order::Side::BUY, 
            50000.0 + i, // Vary the price slightly
            0.1,
            Order::Type::LIMIT
        );
        order_placement_benchmark.stop();
        
        // Small delay between iterations to avoid rate limiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Benchmark 2: Order cancellation latency
    Benchmark order_cancel_benchmark("Order Cancellation");
    auto open_orders = order_manager->getOpenOrders();
    for (size_t i = 0; i < std::min(static_cast<size_t>(iterations), open_orders.size()); ++i) {
        order_cancel_benchmark.start();
        order_manager->cancelOrder(open_orders[i].order_id);
        order_cancel_benchmark.stop();
        
        // Small delay between iterations to avoid rate limiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Benchmark 3: Orderbook retrieval latency
    Benchmark orderbook_retrieval_benchmark("Orderbook Retrieval");
    for (int i = 0; i < iterations; ++i) {
        orderbook_retrieval_benchmark.start();
        api_client->getOrderbook("BTC-PERPETUAL", 10);
        orderbook_retrieval_benchmark.stop();
        
        // Small delay between iterations to avoid rate limiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Benchmark 4: WebSocket message propagation
    Benchmark ws_message_benchmark("WebSocket Message Propagation");
    for (int i = 0; i < iterations; ++i) {
        ws_message_benchmark.start();
        ws_server->broadcastToAll("{\"type\":\"test\",\"sequence\":" + std::to_string(i) + "}");
        ws_message_benchmark.stop();
        
        // Small delay between iterations
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Wait for end-to-end measurements to accumulate
    std::cout << "Waiting for market data updates to benchmark end-to-end latency...\n";
    market_data->subscribe("BTC-PERPETUAL");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Stop the services
    market_data->stop();
    ws_server->stop();
    
    // Print results
    std::cout << "\nBenchmark Results:\n";
    std::cout << "=====================================\n";
    order_placement_benchmark.printStatistics();
    std::cout << "-------------------------------------\n";
    order_cancel_benchmark.printStatistics();
    std::cout << "-------------------------------------\n";
    orderbook_retrieval_benchmark.printStatistics();
    std::cout << "-------------------------------------\n";
    ws_message_benchmark.printStatistics();
    std::cout << "-------------------------------------\n";
    end_to_end_benchmark.printStatistics();
    std::cout << "=====================================\n";
    
    // Save results to CSV
    order_placement_benchmark.saveToCSV("order_placement_benchmark.csv");
    order_cancel_benchmark.saveToCSV("order_cancel_benchmark.csv");
    orderbook_retrieval_benchmark.saveToCSV("orderbook_retrieval_benchmark.csv");
    ws_message_benchmark.saveToCSV("ws_message_benchmark.csv");
    end_to_end_benchmark.saveToCSV("end_to_end_benchmark.csv");
    
    std::cout << "Benchmark data saved to CSV files.\n";
}

// Entry point for the benchmark tool
int main(int argc, char* argv[]) {
    std::cout << "Deribit Trader Benchmark Tool\n";
    std::cout << "-----------------------------\n\n";
    
    int iterations = 100;
    if (argc > 1) {
        iterations = std::stoi(argv[1]);
    }
    
    runBenchmarks(iterations);
    
    return 0;
}