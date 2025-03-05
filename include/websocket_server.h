#pragma once

#include <string>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>

// Forward declarations
namespace boost {
    namespace asio {
        class io_context;
        namespace ip {
            class tcp;
        }
    }
}

// WebSocket client connection
class WebSocketConnection {
public:
    using Pointer = std::shared_ptr<WebSocketConnection>;
    using MessageHandler = std::function<void(Pointer, const std::string&)>;
    using CloseHandler = std::function<void(Pointer)>;
    
    virtual ~WebSocketConnection() = default;
    
    virtual void send(const std::string& message) = 0;
    virtual void close() = 0;
    virtual std::string getId() const = 0;
};

// WebSocket server
class WebSocketServer {
public:
    WebSocketServer(int port = 8080);
    ~WebSocketServer();
    
    // Server control
    void start();
    void stop();
    bool isRunning() const;
    
    // Broadcasting
    void broadcastOrderbook(const std::string& instrument, const std::string& orderbook_json);
    void broadcastToSubscribers(const std::string& instrument, const std::string& message);
    void broadcastToAll(const std::string& message);
    
    // Subscription management
    void addSubscription(const WebSocketConnection::Pointer& client, const std::string& instrument);
    void removeSubscription(const WebSocketConnection::Pointer& client, const std::string& instrument);
    void removeAllSubscriptions(const WebSocketConnection::Pointer& client);
    std::set<std::string> getSubscriptions(const WebSocketConnection::Pointer& client) const;
    
private:
    // Implementation details
    int port_;
    std::atomic<bool> running_;
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::ip::tcp> acceptor_;
    std::thread server_thread_;
    
    // Client tracking
    mutable std::mutex clients_mutex_;
    std::map<std::string, WebSocketConnection::Pointer> clients_;
    
    // Subscription tracking
    mutable std::mutex subscriptions_mutex_;
    std::map<std::string, std::set<std::string>> client_subscriptions_;  // client_id -> set of instruments
    std::map<std::string, std::set<std::string>> instrument_subscribers_; // instrument -> set of client_ids
    
    // Connection handlers
    void onAccept(WebSocketConnection::Pointer connection);
    void onMessage(WebSocketConnection::Pointer connection, const std::string& message);
    void onClose(WebSocketConnection::Pointer connection);
    
    // Connection setup
    void startAccept();
    void handleMessage(WebSocketConnection::Pointer connection, const std::string& message);
};