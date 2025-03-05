#include <string>
#include <chrono>
#include <thread>

// Define Catch version before including it
#define CATCH_VERSION_MAJOR 2
#define CATCH_VERSION_MINOR 13
#define CATCH_VERSION_PATCH 9
#include <catch2/catch.hpp>

#include "api_client.h"

TEST_CASE("ApiClient basic functionality", "[api_client]") {
    // Create API client with test credentials
    ApiClient::Auth auth;
    auth.client_id = "m_B5zE25";
    auth.client_secret = "qwHcammuk8D-MEK4idg8urGt_ZAkfk4r_MuIzT9v1LE";
    ApiClient api_client(auth);
    
    SECTION("Place order") {
        std::string response = api_client.placeOrder("BTC-PERPETUAL", true, 50000.0, 0.1);
        REQUIRE(!response.empty());
    }
    
    SECTION("Cancel order") {
        std::string order_id = "mock_order_id";
        bool success = api_client.cancelOrder(order_id);
        REQUIRE(success);
    }
    
    SECTION("Modify order") {
        std::string order_id = "mock_order_id";
        bool success = api_client.modifyOrder(order_id, 51000.0, 0.2);
        REQUIRE(success);
    }
    
    SECTION("Get orderbook") {
        std::string response = api_client.getOrderbook("BTC-PERPETUAL", 10);
        REQUIRE(!response.empty());
    }
    
    SECTION("Get positions") {
        std::string response = api_client.getCurrentPositions();
        REQUIRE(!response.empty());
    }
}

// Note: WebSocket tests require a running server and are more complex,
// so we're not including them in this basic test suite