#pragma once

#include "api_client.h"

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

// Structure to represent an order
struct Order {
    enum class Side { BUY, SELL };
    enum class Type { LIMIT, MARKET };
    enum class Status { PENDING, OPEN, FILLED, PARTIALLY_FILLED, CANCELLED, REJECTED };
    
    std::string order_id;
    std::string instrument;
    Side side;
    Type type;
    double price;
    double amount;
    double filled_amount = 0.0;
    Status status = Status::PENDING;
    std::string error_message;
    int64_t creation_timestamp;
    int64_t last_update_timestamp;
};

class OrderManager {
public:
    OrderManager(std::shared_ptr<ApiClient> api_client);
    
    // Order management functions
    std::string placeOrder(const std::string& instrument, 
                         Order::Side side, 
                         double price, 
                         double amount, 
                         Order::Type type = Order::Type::LIMIT);
                         
    bool cancelOrder(const std::string& order_id);
    
    bool modifyOrder(const std::string& order_id, 
                   double new_price,
                   double new_amount);
    
    // Query functions
    std::vector<Order> getAllOrders() const;
    std::vector<Order> getOpenOrders() const;
    Order getOrder(const std::string& order_id) const;
    std::map<std::string, double> getCurrentPositions() const;

    // Event callbacks - called when receiving WebSocket updates
    void onOrderUpdate(const std::string& order_data);
    void onPositionUpdate(const std::string& position_data);
    
private:
    std::shared_ptr<ApiClient> api_client_;
    mutable std::mutex orders_mutex_;
    std::map<std::string, Order> orders_;
    mutable std::mutex positions_mutex_;
    std::map<std::string, double> positions_;
};