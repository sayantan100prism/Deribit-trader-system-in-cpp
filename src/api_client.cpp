#include "api_client.h"

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// WebSocket implementation class
class ApiClient::WebSocketImpl : public std::enable_shared_from_this<ApiClient::WebSocketImpl> {
public:
    WebSocketImpl(boost::asio::io_context& ioc, ssl::context& ctx, const ApiClient::Auth& auth) 
        : resolver_(net::make_strand(ioc)), 
          ws_(net::make_strand(ioc), ctx),
          auth_(auth) {
    }

    void connect(const std::string& host, const std::string& port, 
                std::function<void(const std::string&)> message_handler) {
        host_ = host;
        message_handler_ = message_handler;
        
        // Set up the TCP resolver
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &WebSocketImpl::on_resolve,
                shared_from_this()));
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
        if(ec) {
            std::cerr << "Error resolving: " << ec.message() << std::endl;
            return;
        }

        // Connect to the endpoint
        beast::get_lowest_layer(ws_).async_connect(
            results,
            beast::bind_front_handler(
                &WebSocketImpl::on_connect,
                shared_from_this()));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
        if(ec) {
            std::cerr << "Error connecting: " << ec.message() << std::endl;
            return;
        }

        // Perform the SSL handshake
        ws_.next_layer().async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(
                &WebSocketImpl::on_ssl_handshake,
                shared_from_this()));
    }

    void on_ssl_handshake(beast::error_code ec) {
        if(ec) {
            std::cerr << "Error SSL handshake: " << ec.message() << std::endl;
            return;
        }

        // Set up the WebSocket handshake
        ws_.set_option(websocket::stream_base::timeout::suggested(
            beast::role_type::client));

        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " deribit-trader-websocket");
            }));

        // Perform the WebSocket handshake
        ws_.async_handshake(host_, "/ws/api/v2",
            beast::bind_front_handler(
                &WebSocketImpl::on_handshake,
                shared_from_this()));
    }

    void on_handshake(beast::error_code ec) {
        if(ec) {
            std::cerr << "Error handshake: " << ec.message() << std::endl;
            return;
        }

        // Authentication
        authenticate();

        // Start reading
        read();
    }

    void authenticate() {
        // Create authentication JSON
        std::stringstream ss;
        ss << "{\n"
           << "  \"jsonrpc\": \"2.0\",\n"
           << "  \"id\": 9929,\n"
           << "  \"method\": \"public/auth\",\n"
           << "  \"params\": {\n"
           << "    \"grant_type\": \"client_credentials\",\n"
           << "    \"client_id\": \"" << auth_.client_id << "\",\n"
           << "    \"client_secret\": \"" << auth_.client_secret << "\"\n"
           << "  }\n"
           << "}";

        // Send the message
        write(ss.str());
    }

    void read() {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &WebSocketImpl::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if(ec) {
            std::cerr << "Error reading: " << ec.message() << std::endl;
            return;
        }

        // Process the message
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        // Call the message handler
        if (message_handler_) {
            message_handler_(msg);
        }

        // Read the next message
        read();
    }

    void write(const std::string& msg) {
        // Post our work to the strand
        net::post(
            ws_.get_executor(),
            beast::bind_front_handler(
                &WebSocketImpl::on_write,
                shared_from_this(),
                msg));
    }

    void on_write(std::string msg) {
        // Send the message
        ws_.async_write(
            net::buffer(msg),
            beast::bind_front_handler(
                &WebSocketImpl::on_write_complete,
                shared_from_this()));
    }

    void on_write_complete(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if(ec) {
            std::cerr << "Error writing: " << ec.message() << std::endl;
            return;
        }
    }

    void close() {
        // Close the WebSocket connection
        net::post(
            ws_.get_executor(),
            beast::bind_front_handler(
                &WebSocketImpl::on_close,
                shared_from_this()));
    }

    void on_close() {
        // Send close frame
        ws_.async_close(websocket::close_code::normal,
            beast::bind_front_handler(
                &WebSocketImpl::on_close_complete,
                shared_from_this()));
    }

    void on_close_complete(beast::error_code ec) {
        if(ec) {
            std::cerr << "Error closing: " << ec.message() << std::endl;
            return;
        }
    }

private:
    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    ApiClient::Auth auth_;
    std::function<void(const std::string&)> message_handler_;
};

// Generate random nonce
std::string generateNonce() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 9);
    
    std::string nonce;
    for (int i = 0; i < 8; ++i) {
        nonce += std::to_string(dis(gen));
    }
    return nonce;
}

// Convert bytes to hex string
std::string bytesToHex(const unsigned char* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

// API Client implementation
ApiClient::ApiClient(const Auth& auth) : auth_(auth) {
    // Initialize IO context
    io_context_ = std::make_unique<boost::asio::io_context>();
    
    // Initialize SSL context
    ssl_context_ = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
    ssl_context_->set_default_verify_paths();
    ssl_context_->set_verify_mode(ssl::verify_peer);
}

ApiClient::~ApiClient() {
    // Ensure WebSocket is closed
    closeWebSocket();
}

std::string ApiClient::generateSignature(const std::string& timestamp, const std::string& nonce, const std::string& data) {
    std::string message = timestamp + "\n" + nonce + "\n" + data;
    
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    
    HMAC(EVP_sha256(), auth_.client_secret.c_str(), auth_.client_secret.length(),
         reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
         digest, &digest_len);
    
    return bytesToHex(digest, digest_len);
}

std::string ApiClient::makeRequest(const std::string& method, const std::string& endpoint, const std::map<std::string, std::string>& params) {
    // Create HTTP client and request
    // This is a placeholder - in a real implementation, you would use boost::beast::http
    // to make the actual HTTP request to the Deribit API
    
    // For now, return a mock response
    return "{\"result\": \"success\"}";
}

std::string ApiClient::placeOrder(const std::string& instrument, bool is_buy, double price, double amount, const std::string& order_type) {
    // Prepare parameters
    std::map<std::string, std::string> params;
    params["instrument_name"] = instrument;
    params["type"] = order_type;
    params["side"] = is_buy ? "buy" : "sell";
    params["price"] = std::to_string(price);
    params["amount"] = std::to_string(amount);
    
    // Make API request
    return makeRequest("POST", "/api/v2/private/buy", params);
}

bool ApiClient::cancelOrder(const std::string& order_id) {
    // Prepare parameters
    std::map<std::string, std::string> params;
    params["order_id"] = order_id;
    
    // Make API request
    std::string response = makeRequest("POST", "/api/v2/private/cancel", params);
    
    // In real implementation, parse the response to determine success
    return true;
}

bool ApiClient::modifyOrder(const std::string& order_id, double new_price, double new_amount) {
    // Prepare parameters
    std::map<std::string, std::string> params;
    params["order_id"] = order_id;
    params["price"] = std::to_string(new_price);
    params["amount"] = std::to_string(new_amount);
    
    // Make API request
    std::string response = makeRequest("POST", "/api/v2/private/edit", params);
    
    // In real implementation, parse the response to determine success
    return true;
}

std::string ApiClient::getOrderbook(const std::string& instrument, int depth) {
    // Prepare parameters
    std::map<std::string, std::string> params;
    params["instrument_name"] = instrument;
    params["depth"] = std::to_string(depth);
    
    // Make API request
    return makeRequest("GET", "/api/v2/public/get_order_book", params);
}

std::string ApiClient::getCurrentPositions() {
    // Make API request
    return makeRequest("GET", "/api/v2/private/get_positions", {});
}

void ApiClient::connectWebSocket(std::function<void(const std::string&)> message_handler) {
    // Initialize the WebSocket implementation using make_shared instead of make_unique
    auto impl = std::make_shared<WebSocketImpl>(*io_context_, *ssl_context_, auth_);
    ws_impl_ = impl;
    
    // Connect to the WebSocket server
    impl->connect("test.deribit.com", "443", message_handler);
    
    // Start the IO context in a separate thread
    std::thread t([this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            std::cerr << "WebSocket error: " << e.what() << std::endl;
        }
    });
    t.detach();
}

void ApiClient::subscribeToOrderbook(const std::string& instrument) {
    if (!ws_impl_) return;
    
    // Create subscription message
    std::stringstream ss;
    ss << "{\n"
       << "  \"jsonrpc\": \"2.0\",\n"
       << "  \"id\": 3600,\n"
       << "  \"method\": \"public/subscribe\",\n"
       << "  \"params\": {\n"
       << "    \"channels\": [\"book." << instrument << ".none.10.100ms\"]\n"
       << "  }\n"
       << "}";
    
    // Send the subscription message
    auto impl = ws_impl_;
    if (impl) {
        impl->write(ss.str());
    }
}

void ApiClient::unsubscribeFromOrderbook(const std::string& instrument) {
    if (!ws_impl_) return;
    
    // Create unsubscription message
    std::stringstream ss;
    ss << "{\n"
       << "  \"jsonrpc\": \"2.0\",\n"
       << "  \"id\": 8691,\n"
       << "  \"method\": \"public/unsubscribe\",\n"
       << "  \"params\": {\n"
       << "    \"channels\": [\"book." << instrument << ".none.10.100ms\"]\n"
       << "  }\n"
       << "}";
    
    // Send the unsubscription message
    auto impl = ws_impl_;
    if (impl) {
        impl->write(ss.str());
    }
}

void ApiClient::closeWebSocket() {
    auto impl = ws_impl_;
    if (impl) {
        impl->close();
        ws_impl_.reset();
    }
    
    if (io_context_) {
        io_context_->stop();
    }
}