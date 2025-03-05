# Deribit Trader Usage Guide

This document provides detailed instructions on how to use the Deribit Trader system.

## Table of Contents

1. [System Overview](#system-overview)
2. [Getting Started](#getting-started)
3. [API Client](#api-client)
4. [Order Management](#order-management)
5. [Market Data](#market-data)
6. [WebSocket Server](#websocket-server)
7. [Performance Benchmarking](#performance-benchmarking)
8. [Examples](#examples)

## System Overview

The Deribit Trader system consists of the following main components:

- **API Client**: Handles communication with the Deribit Test API
- **Order Manager**: Manages orders and positions
- **Market Data Client**: Streams and processes market data
- **WebSocket Server**: Distributes real-time market data to clients

## Getting Started

### Building the System

```bash
# Create a build directory
mkdir build
cd build

# Configure the project
cmake ..

# Build the project
cmake --build .
```

### Running the System

```bash
# From the build directory
./deribit_trader
```

## API Client

The API client encapsulates all communication with the Deribit Test API.

### Authentication

```cpp
ApiClient::Auth auth;
auth.client_id = "your_client_id";
auth.client_secret = "your_client_secret";
auto api_client = std::make_shared<ApiClient>(auth);
```

### REST API Methods

```cpp
// Place an order
std::string order_id = api_client->placeOrder("BTC-PERPETUAL", true, 50000.0, 0.1, "limit");

// Cancel an order
bool success = api_client->cancelOrder(order_id);

// Modify an order
success = api_client->modifyOrder(order_id, 51000.0, 0.2);

// Get the orderbook
std::string orderbook_json = api_client->getOrderbook("BTC-PERPETUAL", 10);

// Get current positions
std::string positions_json = api_client->getCurrentPositions();
```

### WebSocket Methods

```cpp
// Connect to WebSocket
api_client->connectWebSocket([](const std::string& message) {
    // Handle message
    std::cout << "Received: " << message << std::endl;
});

// Subscribe to orderbook updates
api_client->subscribeToOrderbook("BTC-PERPETUAL");

// Unsubscribe from orderbook updates
api_client->unsubscribeFromOrderbook("BTC-PERPETUAL");

// Close WebSocket connection
api_client->closeWebSocket();
```

## Order Management

The Order Manager provides a high-level interface for managing orders and positions.

### Order Operations

```cpp
// Create order manager
auto order_manager = std::make_shared<OrderManager>(api_client);

// Place an order
std::string order_id = order_manager->placeOrder(
    "BTC-PERPETUAL",
    Order::Side::BUY,
    50000.0,
    0.1,
    Order::Type::LIMIT
);

// Cancel an order
bool success = order_manager->cancelOrder(order_id);

// Modify an order
success = order_manager->modifyOrder(order_id, 51000.0, 0.2);
```

### Order Queries

```cpp
// Get all orders
std::vector<Order> all_orders = order_manager->getAllOrders();

// Get open orders
std::vector<Order> open_orders = order_manager->getOpenOrders();

// Get a specific order
Order order = order_manager->getOrder(order_id);

// Get current positions
std::map<std::string, double> positions = order_manager->getCurrentPositions();
```

## Market Data

The Market Data client handles real-time market data streams.

### Using the Market Data Client

```cpp
// Create market data client
auto market_data = std::make_shared<MarketDataClient>(api_client);

// Set up callback for orderbook updates
market_data->setOrderbookCallback([](const Orderbook& orderbook) {
    std::cout << "Received orderbook for " << orderbook.instrument << std::endl;
    std::cout << "Top bid: " << orderbook.bids[0].price << " @ " << orderbook.bids[0].size << std::endl;
    std::cout << "Top ask: " << orderbook.asks[0].price << " @ " << orderbook.asks[0].size << std::endl;
});

// Start the client
market_data->start();

// Subscribe to instruments
market_data->subscribe("BTC-PERPETUAL");
market_data->subscribe("ETH-PERPETUAL");

// Get current orderbook
Orderbook btc_orderbook = market_data->getOrderbook("BTC-PERPETUAL");

// Stop the client
market_data->stop();
```

## WebSocket Server

The WebSocket server distributes real-time market data to connected clients.

### Using the WebSocket Server

```cpp
// Create WebSocket server on port 8080
auto ws_server = std::make_shared<WebSocketServer>(8080);

// Start the server
ws_server->start();

// Broadcast a message to all clients
ws_server->broadcastToAll("{\"type\":\"system\",\"message\":\"Server started\"}");

// Broadcast orderbook to subscribers
ws_server->broadcastOrderbook("BTC-PERPETUAL", orderbook_json);

// Stop the server
ws_server->stop();
```

### Client Protocol

Clients can send the following messages to the server:

#### Subscribe to an instrument

```json
{
  "type": "subscribe",
  "instrument": "BTC-PERPETUAL"
}
```

#### Unsubscribe from an instrument

```json
{
  "type": "unsubscribe",
  "instrument": "BTC-PERPETUAL"
}
```

## Performance Benchmarking

The system includes a benchmarking tool to measure performance metrics.

### Running Benchmarks

```bash
# From the build directory
./deribit_benchmark [iterations]
```

## Examples

### Complete Trading System Example

```cpp
#include "api_client.h"
#include "order_manager.h"
#include "market_data.h"
#include "websocket_server.h"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

int main() {
    // Create API client
    ApiClient::Auth auth;
    auth.client_id = "your_client_id";
    auth.client_secret = "your_client_secret";
    auto api_client = std::make_shared<ApiClient>(auth);
    
    // Create order manager
    auto order_manager = std::make_shared<OrderManager>(api_client);
    
    // Create market data client
    auto market_data = std::make_shared<MarketDataClient>(api_client);
    
    // Create WebSocket server
    auto ws_server = std::make_shared<WebSocketServer>(8080);
    
    // Set up market data callback
    market_data->setOrderbookCallback([&ws_server](const Orderbook& orderbook) {
        // Convert orderbook to JSON
        // (simplified for example)
        std::string json = "{\"instrument\":\"" + orderbook.instrument + "\"}";
        
        // Broadcast to subscribers
        ws_server->broadcastOrderbook(orderbook.instrument, json);
    });
    
    // Start the server and market data client
    ws_server->start();
    market_data->start();
    
    // Subscribe to some instruments
    market_data->subscribe("BTC-PERPETUAL");
    market_data->subscribe("ETH-PERPETUAL");
    
    // Place a limit order
    std::string order_id = order_manager->placeOrder(
        "BTC-PERPETUAL",
        Order::Side::BUY,
        50000.0,
        0.1,
        Order::Type::LIMIT
    );
    
    // Run for a while
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // Cancel the order
    order_manager->cancelOrder(order_id);
    
    // Stop the market data client and server
    market_data->stop();
    ws_server->stop();
    
    return 0;
}
```

### Simple WebSocket Client Example (Python)

```python
import websocket
import json
import threading
import time

def on_message(ws, message):
    data = json.loads(message)
    if data.get('type') == 'orderbook':
        instrument = data.get('instrument')
        bids = data.get('bids', [])
        asks = data.get('asks', [])
        
        print(f"Orderbook update for {instrument}")
        if bids:
            print(f"Top bid: {bids[0][0]} @ {bids[0][1]}")
        if asks:
            print(f"Top ask: {asks[0][0]} @ {asks[0][1]}")

def on_error(ws, error):
    print(f"Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print("Closed connection")

def on_open(ws):
    print("Connection opened")
    # Subscribe to BTC-PERPETUAL orderbook updates
    ws.send(json.dumps({
        "type": "subscribe",
        "instrument": "BTC-PERPETUAL"
    }))

# Connect to the WebSocket server
ws = websocket.WebSocketApp("ws://localhost:8080",
                            on_open=on_open,
                            on_message=on_message,
                            on_error=on_error,
                            on_close=on_close)

# Run the WebSocket client in a separate thread
wst = threading.Thread(target=ws.run_forever)
wst.daemon = True
wst.start()

# Run for 60 seconds
time.sleep(60)

# Unsubscribe and close
ws.send(json.dumps({
    "type": "unsubscribe",
    "instrument": "BTC-PERPETUAL"
}))
ws.close()
```