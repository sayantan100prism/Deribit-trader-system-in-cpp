#include "order_manager.h"

#include <chrono>
#include <iostream>
#include <algorithm>

// Include JSON library
#define NLOHMANN_JSON_VERSION_MAJOR 3
#define NLOHMANN_JSON_VERSION_MINOR 11
#define NLOHMANN_JSON_VERSION_PATCH 2
#include <nlohmann/json.hpp>

using json = nlohmann::json;

OrderManager::OrderManager(std::shared_ptr<ApiClient> api_client)
    : api_client_(api_client) {
}

std::string OrderManager::placeOrder(const std::string& instrument, 
                                    Order::Side side, 
                                    double price, 
                                    double amount, 
                                    Order::Type type) {
    // Call the API client to place the order
    std::string api_response = api_client_->placeOrder(
        instrument, 
        side == Order::Side::BUY, 
        price, 
        amount, 
        type == Order::Type::LIMIT ? "limit" : "market"
    );
    
    // Parse the response (in a real implementation)
    // For now, mock the response
    std::string order_id = "order_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Create a new order
    Order order;
    order.order_id = order_id;
    order.instrument = instrument;
    order.side = side;
    order.type = type;
    order.price = price;
    order.amount = amount;
    order.status = Order::Status::OPEN;
    order.creation_timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    order.last_update_timestamp = order.creation_timestamp;
    
    // Add the order to our map
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        orders_[order_id] = order;
    }
    
    return order_id;
}

bool OrderManager::cancelOrder(const std::string& order_id) {
    // Call the API client to cancel the order
    bool success = api_client_->cancelOrder(order_id);
    
    if (success) {
        // Update the order status
        std::lock_guard<std::mutex> lock(orders_mutex_);
        if (orders_.find(order_id) != orders_.end()) {
            orders_[order_id].status = Order::Status::CANCELLED;
            orders_[order_id].last_update_timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        }
    }
    
    return success;
}

bool OrderManager::modifyOrder(const std::string& order_id, 
                             double new_price,
                             double new_amount) {
    // Call the API client to modify the order
    bool success = api_client_->modifyOrder(order_id, new_price, new_amount);
    
    if (success) {
        // Update the order
        std::lock_guard<std::mutex> lock(orders_mutex_);
        if (orders_.find(order_id) != orders_.end()) {
            orders_[order_id].price = new_price;
            orders_[order_id].amount = new_amount;
            orders_[order_id].last_update_timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        }
    }
    
    return success;
}

std::vector<Order> OrderManager::getAllOrders() const {
    std::vector<Order> result;
    
    std::lock_guard<std::mutex> lock(orders_mutex_);
    result.reserve(orders_.size());
    for (const auto& pair : orders_) {
        result.push_back(pair.second);
    }
    
    // Sort by creation time (newest first)
    std::sort(result.begin(), result.end(), [](const Order& a, const Order& b) {
        return a.creation_timestamp > b.creation_timestamp;
    });
    
    return result;
}

std::vector<Order> OrderManager::getOpenOrders() const {
    std::vector<Order> result;
    
    std::lock_guard<std::mutex> lock(orders_mutex_);
    for (const auto& pair : orders_) {
        if (pair.second.status == Order::Status::OPEN || 
            pair.second.status == Order::Status::PARTIALLY_FILLED) {
            result.push_back(pair.second);
        }
    }
    
    // Sort by creation time (newest first)
    std::sort(result.begin(), result.end(), [](const Order& a, const Order& b) {
        return a.creation_timestamp > b.creation_timestamp;
    });
    
    return result;
}

Order OrderManager::getOrder(const std::string& order_id) const {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    if (orders_.find(order_id) != orders_.end()) {
        return orders_.at(order_id);
    }
    
    // Return an empty order with REJECTED status if not found
    Order empty;
    empty.order_id = order_id;
    empty.status = Order::Status::REJECTED;
    empty.error_message = "Order not found";
    return empty;
}

std::map<std::string, double> OrderManager::getCurrentPositions() const {
    std::lock_guard<std::mutex> lock(positions_mutex_);
    return positions_;
}

void OrderManager::onOrderUpdate(const std::string& order_data) {
    try {
        // Parse the order update JSON
        json data = json::parse(order_data);
        
        // Extract order information
        std::string order_id = data["order_id"].get<std::string>();
        std::string status = data["state"].get<std::string>();
        double filled_amount = data["filled_amount"].get<double>();
        
        // Update our order record
        std::lock_guard<std::mutex> lock(orders_mutex_);
        if (orders_.find(order_id) != orders_.end()) {
            Order& order = orders_[order_id];
            order.filled_amount = filled_amount;
            order.last_update_timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            
            // Update status
            if (status == "open") {
                order.status = Order::Status::OPEN;
            } else if (status == "filled") {
                order.status = Order::Status::FILLED;
            } else if (status == "cancelled") {
                order.status = Order::Status::CANCELLED;
            } else if (status == "rejected") {
                order.status = Order::Status::REJECTED;
                if (data.contains("error")) {
                    order.error_message = data["error"].get<std::string>();
                }
            } else if (filled_amount > 0 && filled_amount < order.amount) {
                order.status = Order::Status::PARTIALLY_FILLED;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing order update: " << e.what() << std::endl;
    }
}

void OrderManager::onPositionUpdate(const std::string& position_data) {
    try {
        // Parse the position update JSON
        json data = json::parse(position_data);
        
        // Extract position information
        if (data.is_array()) {
            std::lock_guard<std::mutex> lock(positions_mutex_);
            positions_.clear();
            
            for (const auto& position : data) {
                std::string instrument = position["instrument_name"].get<std::string>();
                double size = position["size"].get<double>();
                positions_[instrument] = size;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing position update: " << e.what() << std::endl;
    }
}