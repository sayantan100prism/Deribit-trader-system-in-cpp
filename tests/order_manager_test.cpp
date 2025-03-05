#include <memory>
#include <string>

// Define Catch version before including it
#define CATCH_VERSION_MAJOR 2
#define CATCH_VERSION_MINOR 13
#define CATCH_VERSION_PATCH 9
#include <catch2/catch.hpp>

#include "order_manager.h"
#include "api_client.h"

TEST_CASE("OrderManager basic functionality", "[order_manager]") {
    // Create API client with test credentials
    ApiClient::Auth auth;
    auth.client_id = "m_B5zE25";
    auth.client_secret = "qwHcammuk8D-MEK4idg8urGt_ZAkfk4r_MuIzT9v1LE";
    auto api_client = std::make_shared<ApiClient>(auth);
    
    // Create order manager
    OrderManager order_manager(api_client);
    
    SECTION("Place order") {
        std::string order_id = order_manager.placeOrder(
            "BTC-PERPETUAL", 
            Order::Side::BUY, 
            50000.0, 
            0.1,
            Order::Type::LIMIT
        );
        
        REQUIRE(!order_id.empty());
        
        // Verify the order was added to the manager
        Order order = order_manager.getOrder(order_id);
        REQUIRE(order.order_id == order_id);
        REQUIRE(order.instrument == "BTC-PERPETUAL");
        REQUIRE(order.side == Order::Side::BUY);
        REQUIRE(order.price == 50000.0);
        REQUIRE(order.amount == 0.1);
        REQUIRE(order.type == Order::Type::LIMIT);
        REQUIRE(order.status == Order::Status::OPEN);
    }
    
    SECTION("Get open orders") {
        // Place a few orders
        std::string order1 = order_manager.placeOrder("BTC-PERPETUAL", Order::Side::BUY, 50000.0, 0.1);
        std::string order2 = order_manager.placeOrder("ETH-PERPETUAL", Order::Side::SELL, 3000.0, 1.0);
        
        // Get open orders
        std::vector<Order> open_orders = order_manager.getOpenOrders();
        
        // Verify we got both orders
        REQUIRE(open_orders.size() == 2);
    }
    
    SECTION("Cancel order") {
        // Place an order
        std::string order_id = order_manager.placeOrder("BTC-PERPETUAL", Order::Side::BUY, 50000.0, 0.1);
        
        // Cancel the order
        bool success = order_manager.cancelOrder(order_id);
        REQUIRE(success);
        
        // Verify the order was canceled
        Order order = order_manager.getOrder(order_id);
        REQUIRE(order.status == Order::Status::CANCELLED);
    }
    
    SECTION("Modify order") {
        // Place an order
        std::string order_id = order_manager.placeOrder("BTC-PERPETUAL", Order::Side::BUY, 50000.0, 0.1);
        
        // Modify the order
        bool success = order_manager.modifyOrder(order_id, 51000.0, 0.2);
        REQUIRE(success);
        
        // Verify the order was modified
        Order order = order_manager.getOrder(order_id);
        REQUIRE(order.price == 51000.0);
        REQUIRE(order.amount == 0.2);
    }
    
    SECTION("Order update callback") {
        // Place an order
        std::string order_id = order_manager.placeOrder("BTC-PERPETUAL", Order::Side::BUY, 50000.0, 0.1);
        
        // Create a mock order update message
        std::string update = R"({
            "order_id": ")" + order_id + R"(",
            "state": "filled",
            "filled_amount": 0.1
        })";
        
        // Process the update
        order_manager.onOrderUpdate(update);
        
        // Verify the order was updated
        Order order = order_manager.getOrder(order_id);
        REQUIRE(order.status == Order::Status::FILLED);
        REQUIRE(order.filled_amount == 0.1);
    }
    
    SECTION("Position update callback") {
        // Create a mock position update message
        std::string update = R"([
            {
                "instrument_name": "BTC-PERPETUAL",
                "size": 0.5
            },
            {
                "instrument_name": "ETH-PERPETUAL",
                "size": -1.0
            }
        ])";
        
        // Process the update
        order_manager.onPositionUpdate(update);
        
        // Verify positions were updated
        auto positions = order_manager.getCurrentPositions();
        REQUIRE(positions.size() == 2);
        REQUIRE(positions.at("BTC-PERPETUAL") == 0.5);
        REQUIRE(positions.at("ETH-PERPETUAL") == -1.0);
    }
}