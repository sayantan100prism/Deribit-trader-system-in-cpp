#include "websocket_server.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <random>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Concrete implementation of a WebSocket connection
class WebSocketConnectionImpl : public WebSocketConnection, public std::enable_shared_from_this<WebSocketConnectionImpl> {
public:
    // Constructor for upgrading an HTTP connection to WebSocket
    WebSocketConnectionImpl(tcp::socket&& socket, MessageHandler message_handler, CloseHandler close_handler)
        : ws_(std::move(socket)),
          message_handler_(message_handler),
          close_handler_(close_handler),
          id_(generateRandomId()) {
    }

    // Start the connection
    void start() {
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(beast::http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " deribit-trader-websocket-server");
            }));

        // Accept the websocket handshake
        ws_.async_accept(
            beast::bind_front_handler(
                &WebSocketConnectionImpl::on_accept,
                shared_from_this()));
    }

    // Send a message to the client
    void send(const std::string& message) override {
        // Post our work to the strand
        net::post(
            ws_.get_executor(),
            beast::bind_front_handler(
                &WebSocketConnectionImpl::on_send,
                shared_from_this(),
                message));
    }

    // Close the connection
    void close() override {
        // Post our work to the strand
        net::post(
            ws_.get_executor(),
            beast::bind_front_handler(
                &WebSocketConnectionImpl::on_close,
                shared_from_this()));
    }

    // Get the connection ID
    std::string getId() const override {
        return id_;
    }

private:
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    MessageHandler message_handler_;
    CloseHandler close_handler_;
    std::string id_;

    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "WebSocket accept error: " << ec.message() << std::endl;
            return;
        }

        // Start reading messages
        read();
    }

    void read() {
        // Read a message
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &WebSocketConnectionImpl::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == websocket::error::closed) {
            if (close_handler_) {
                close_handler_(shared_from_this());
            }
            return;
        }

        if (ec) {
            std::cerr << "WebSocket read error: " << ec.message() << std::endl;
            return;
        }

        // Process the message
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        // Call the message handler
        if (message_handler_) {
            message_handler_(shared_from_this(), msg);
        }

        // Read the next message
        read();
    }

    void on_send(std::string message) {
        // Send the message
        ws_.async_write(
            net::buffer(message),
            beast::bind_front_handler(
                &WebSocketConnectionImpl::on_write,
                shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            std::cerr << "WebSocket write error: " << ec.message() << std::endl;
            return;
        }
    }

    void on_close() {
        // Close the WebSocket connection
        ws_.async_close(
            websocket::close_code::normal,
            beast::bind_front_handler(
                &WebSocketConnectionImpl::on_close_complete,
                shared_from_this()));
    }

    void on_close_complete(beast::error_code ec) {
        if (ec) {
            std::cerr << "WebSocket close error: " << ec.message() << std::endl;
            return;
        }

        // Call the close handler
        if (close_handler_) {
            close_handler_(shared_from_this());
        }
    }

    // Generate a random connection ID
    static std::string generateRandomId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static const char* hex = "0123456789abcdef";

        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += hex[dis(gen)];
        }
        return id;
    }
};

// WebSocket server implementation
class WebSocketListener : public std::enable_shared_from_this<WebSocketListener> {
public:
    WebSocketListener(net::io_context& ioc, tcp::endpoint endpoint,
                    std::function<void(WebSocketConnection::Pointer)> on_accept,
                    std::function<void(WebSocketConnection::Pointer, const std::string&)> on_message,
                    std::function<void(WebSocketConnection::Pointer)> on_close)
        : ioc_(ioc),
          acceptor_(ioc),
          on_accept_(on_accept),
          on_message_(on_message),
          on_close_(on_close) {
        
        beast::error_code ec;
        
        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            std::cerr << "Error opening acceptor: " << ec.message() << std::endl;
            return;
        }
        
        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            std::cerr << "Error setting reuse address: " << ec.message() << std::endl;
            return;
        }
        
        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec) {
            std::cerr << "Error binding to " << endpoint << ": " << ec.message() << std::endl;
            return;
        }
        
        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            std::cerr << "Error listening: " << ec.message() << std::endl;
            return;
        }
    }
    
    // Start accepting incoming connections
    void run() {
        accept();
    }
    
    // Stop accepting incoming connections
    void stop() {
        beast::error_code ec;
        acceptor_.close(ec);
        if (ec) {
            std::cerr << "Error closing acceptor: " << ec.message() << std::endl;
        }
    }

private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::function<void(WebSocketConnection::Pointer)> on_accept_;
    std::function<void(WebSocketConnection::Pointer, const std::string&)> on_message_;
    std::function<void(WebSocketConnection::Pointer)> on_close_;
    
    void accept() {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &WebSocketListener::on_accept,
                shared_from_this()));
    }
    
    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            std::cerr << "Error accepting connection: " << ec.message() << std::endl;
        } else {
            // Create the WebSocket connection
            auto connection = std::make_shared<WebSocketConnectionImpl>(
                std::move(socket),
                on_message_,
                on_close_);
            
            // Start the connection
            connection->start();
            
            // Notify the server
            if (on_accept_) {
                on_accept_(connection);
            }
        }
        
        // Accept another connection
        accept();
    }
};

// WebSocketServer implementation
WebSocketServer::WebSocketServer(int port)
    : port_(port), running_(false) {
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start() {
    if (running_) return;
    
    running_ = true;
    
    // Create the IO context
    io_context_ = std::make_unique<net::io_context>();
    
    // Create the listener
    auto listener = std::make_shared<WebSocketListener>(
        *io_context_,
        tcp::endpoint(tcp::v4(), port_),
        [this](WebSocketConnection::Pointer connection) { this->onAccept(connection); },
        [this](WebSocketConnection::Pointer connection, const std::string& message) { this->onMessage(connection, message); },
        [this](WebSocketConnection::Pointer connection) { this->onClose(connection); }
    );
    
    // Start the listener
    listener->run();
    
    // Run the IO context in a separate thread
    server_thread_ = std::thread([this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            std::cerr << "WebSocket server error: " << e.what() << std::endl;
        }
    });
}

void WebSocketServer::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Close all connections
    std::vector<WebSocketConnection::Pointer> connections;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& pair : clients_) {
            connections.push_back(pair.second);
        }
    }
    
    for (auto& connection : connections) {
        connection->close();
    }
    
    // Stop the IO context
    if (io_context_) {
        io_context_->stop();
    }
    
    // Wait for the server thread to finish
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

bool WebSocketServer::isRunning() const {
    return running_;
}

void WebSocketServer::broadcastOrderbook(const std::string& instrument, const std::string& orderbook_json) {
    broadcastToSubscribers(instrument, orderbook_json);
}

void WebSocketServer::broadcastToSubscribers(const std::string& instrument, const std::string& message) {
    std::vector<std::string> client_ids;
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        if (instrument_subscribers_.find(instrument) != instrument_subscribers_.end()) {
            client_ids = std::vector<std::string>(
                instrument_subscribers_[instrument].begin(),
                instrument_subscribers_[instrument].end()
            );
        }
    }
    
    std::vector<WebSocketConnection::Pointer> clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& client_id : client_ids) {
            if (clients_.find(client_id) != clients_.end()) {
                clients.push_back(clients_[client_id]);
            }
        }
    }
    
    for (auto& client : clients) {
        client->send(message);
    }
}

void WebSocketServer::broadcastToAll(const std::string& message) {
    std::vector<WebSocketConnection::Pointer> clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& pair : clients_) {
            clients.push_back(pair.second);
        }
    }
    
    for (auto& client : clients) {
        client->send(message);
    }
}

void WebSocketServer::addSubscription(const WebSocketConnection::Pointer& client, const std::string& instrument) {
    std::string client_id = client->getId();
    
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        // Add to client_subscriptions
        client_subscriptions_[client_id].insert(instrument);
        
        // Add to instrument_subscribers
        instrument_subscribers_[instrument].insert(client_id);
    }
    
    // Send a confirmation message
    client->send("{\"type\":\"subscription\",\"instrument\":\"" + instrument + "\",\"status\":\"subscribed\"}");
}

void WebSocketServer::removeSubscription(const WebSocketConnection::Pointer& client, const std::string& instrument) {
    std::string client_id = client->getId();
    
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        // Remove from client_subscriptions
        if (client_subscriptions_.find(client_id) != client_subscriptions_.end()) {
            client_subscriptions_[client_id].erase(instrument);
        }
        
        // Remove from instrument_subscribers
        if (instrument_subscribers_.find(instrument) != instrument_subscribers_.end()) {
            instrument_subscribers_[instrument].erase(client_id);
            
            // If no more subscribers, remove the instrument entry
            if (instrument_subscribers_[instrument].empty()) {
                instrument_subscribers_.erase(instrument);
            }
        }
    }
    
    // Send a confirmation message
    client->send("{\"type\":\"subscription\",\"instrument\":\"" + instrument + "\",\"status\":\"unsubscribed\"}");
}

void WebSocketServer::removeAllSubscriptions(const WebSocketConnection::Pointer& client) {
    std::string client_id = client->getId();
    
    std::set<std::string> instruments;
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        // Get all instruments this client is subscribed to
        if (client_subscriptions_.find(client_id) != client_subscriptions_.end()) {
            instruments = client_subscriptions_[client_id];
        }
        
        // Remove from client_subscriptions
        client_subscriptions_.erase(client_id);
        
        // Remove from all instrument_subscribers
        for (const auto& instrument : instruments) {
            if (instrument_subscribers_.find(instrument) != instrument_subscribers_.end()) {
                instrument_subscribers_[instrument].erase(client_id);
                
                // If no more subscribers, remove the instrument entry
                if (instrument_subscribers_[instrument].empty()) {
                    instrument_subscribers_.erase(instrument);
                }
            }
        }
    }
}

std::set<std::string> WebSocketServer::getSubscriptions(const WebSocketConnection::Pointer& client) const {
    std::string client_id = client->getId();
    
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    if (client_subscriptions_.find(client_id) != client_subscriptions_.end()) {
        return client_subscriptions_.at(client_id);
    }
    
    return {};
}

void WebSocketServer::onAccept(WebSocketConnection::Pointer connection) {
    // Add the client to our map
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_[connection->getId()] = connection;
    
    // Send a welcome message
    connection->send("{\"type\":\"welcome\",\"message\":\"Welcome to Deribit Trader WebSocket Server\"}");
}

void WebSocketServer::onMessage(WebSocketConnection::Pointer connection, const std::string& message) {
    // Handle the message (parse JSON, etc.)
    // This is a simple implementation that just echoes the message back
    
    // For a real implementation, you would parse the JSON and handle commands
    // like subscribe/unsubscribe, etc.
    handleMessage(connection, message);
}

void WebSocketServer::onClose(WebSocketConnection::Pointer connection) {
    std::string client_id = connection->getId();
    
    // Remove all subscriptions
    removeAllSubscriptions(connection);
    
    // Remove the client from our map
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.erase(client_id);
}

void WebSocketServer::startAccept() {
    // This is handled by the WebSocketListener
}

void WebSocketServer::handleMessage(WebSocketConnection::Pointer connection, const std::string& message) {
    try {
        // Parse the message as JSON
        // For brevity, we're using a simple string comparison approach
        // In a real implementation, you would use a JSON parser
        
        if (message.find("\"type\":\"subscribe\"") != std::string::npos) {
            // Extract the instrument
            size_t pos = message.find("\"instrument\":");
            if (pos != std::string::npos) {
                pos += 13; // Length of "instrument":
                size_t end = message.find("\"", pos + 1);
                if (end != std::string::npos) {
                    std::string instrument = message.substr(pos + 1, end - pos - 1);
                    addSubscription(connection, instrument);
                }
            }
        } else if (message.find("\"type\":\"unsubscribe\"") != std::string::npos) {
            // Extract the instrument
            size_t pos = message.find("\"instrument\":");
            if (pos != std::string::npos) {
                pos += 13; // Length of "instrument":
                size_t end = message.find("\"", pos + 1);
                if (end != std::string::npos) {
                    std::string instrument = message.substr(pos + 1, end - pos - 1);
                    removeSubscription(connection, instrument);
                }
            }
        } else {
            // Echo the message back
            connection->send("{\"type\":\"error\",\"message\":\"Unknown command\"}");
        }
    } catch (const std::exception& e) {
        // Send an error message
        connection->send("{\"type\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}");
    }
}