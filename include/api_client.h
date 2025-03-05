#pragma once

#include <string>
#include <map>
#include <functional>
#include <memory>

// Forward declarations
namespace boost {
    namespace asio {
        class io_context;
        namespace ssl {
            class context;
        }
    }
}

class ApiClient {
public:
    // Authentication details
    struct Auth {
        std::string client_id;
        std::string client_secret;
    };

    // Constructor
    ApiClient(const Auth& auth);
    ~ApiClient();

    // REST API methods
    std::string placeOrder(const std::string& instrument, 
                          bool is_buy, 
                          double price, 
                          double amount, 
                          const std::string& order_type = "limit");
    
    bool cancelOrder(const std::string& order_id);
    
    bool modifyOrder(const std::string& order_id, 
                    double new_price,
                    double new_amount);
    
    std::string getOrderbook(const std::string& instrument, int depth = 10);
    
    std::string getCurrentPositions();

    // WebSocket API methods
    void connectWebSocket(std::function<void(const std::string&)> message_handler);
    void subscribeToOrderbook(const std::string& instrument);
    void unsubscribeFromOrderbook(const std::string& instrument);
    void closeWebSocket();

private:
    Auth auth_;
    std::string generateSignature(const std::string& timestamp, const std::string& nonce, const std::string& data);
    std::string makeRequest(const std::string& method, const std::string& endpoint, const std::map<std::string, std::string>& params = {});
    
    // WebSocket implementation details
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    class WebSocketImpl;
    std::shared_ptr<WebSocketImpl> ws_impl_;
};