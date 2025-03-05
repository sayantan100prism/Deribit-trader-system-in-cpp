# Deribit Trader - Comprehensive Documentation

A high-performance order execution and management system for trading on Deribit Test exchange using C++.

## Table of Contents

1. [System Overview](#system-overview)
2. [Features](#features)
3. [Installation Requirements](#installation-requirements)
4. [Building and Running](#building-and-running)
5. [System Architecture](#system-architecture)
6. [WebSocket Protocol](#websocket-protocol)
7. [Connecting External Clients](#connecting-external-clients)
8. [Performance Benchmarking](#performance-benchmarking)
9. [Configuration Options](#configuration-options)
10. [Troubleshooting](#troubleshooting)

## System Overview

Deribit Trader is a complete trading system for the Deribit cryptocurrency exchange, providing:

- Real-time order management
- Market data streaming
- WebSocket server for client connections
- High-performance, low-latency architecture

The system acts as both a trading client to Deribit and a server for your own client applications.

## Features

- **Order Management**
  - Place limit and market orders
  - Cancel existing orders
  - Modify order parameters
  - Track order status and execution

- **Market Data**
  - Real-time orderbook updates
  - Efficient data structures for market data
  - Customizable update frequency

- **WebSocket Server**
  - Client subscription management
  - Real-time data broadcasting
  - Secure, efficient communication

- **Performance Optimization**
  - Asynchronous I/O operations
  - Lock-free algorithms where possible
  - Memory-efficient design
  - Benchmark tooling

## Installation Requirements

The following dependencies are required:

- **C++ Compiler**: C++17 compatible (GCC 8+, Clang 7+, MSVC 19.14+)
- **CMake**: 3.14 or higher
- **Boost**: 1.70.0 or higher (for Asio, Beast, etc.)
- **OpenSSL**: 1.1.1 or higher
- **nlohmann/json**: Automatically fetched if not found

### Installing Dependencies

#### macOS:
```bash
brew install cmake boost openssl
```

#### Ubuntu/Debian:
```bash
sudo apt update
sudo apt install build-essential cmake libboost-all-dev libssl-dev
```

#### Windows (using vcpkg):
```bash
vcpkg install boost:x64-windows openssl:x64-windows nlohmann-json:x64-windows
```

## Building and Running

### Clone and Build

```bash
# Clone the repository (if applicable)
git clone https://your-repository/deribit-trader.git
cd deribit-trader

# Create a build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)  # macOS only
# For Windows with vcpkg: cmake .. -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake

# Build the project
cmake --build .
```

### Running the System

```bash
# Start the trading system
./deribit_trader
```

This will:
1. Initialize the WebSocket server on port 8080
2. Connect to Deribit Test API
3. Subscribe to initial instruments (BTC-PERPETUAL, ETH-PERPETUAL)
4. Begin processing market data and orders

### Running the Benchmark Tool

```bash
# Run performance benchmarks
./deribit_benchmark [iterations]
```

This will measure:
- Order placement latency
- Market data processing time
- WebSocket propagation delay
- End-to-end trading loop latency

### Running Tests

```bash
# Run unit tests
./run_tests
```

## System Architecture

The system is built on a component-based architecture:

1. **ApiClient**: Handles communication with Deribit API
   - REST API requests for order management
   - WebSocket connection for real-time updates
   - Authentication and encryption

2. **OrderManager**: Manages trading operations
   - Order lifecycle tracking
   - Position management
   - Status updates and notifications

3. **MarketDataClient**: Processes market data
   - Orderbook updates and management
   - Subscription handling
   - Efficient data structures

4. **WebSocketServer**: Serves client applications
   - Client connection management
   - Subscription tracking
   - Efficient broadcasting

## WebSocket Protocol

Clients connect to the WebSocket server on port 8080 (by default) and can send/receive the following messages:

### Client to Server

#### Subscribe to an instrument:
```json
{
  "type": "subscribe",
  "instrument": "BTC-PERPETUAL"
}
```

#### Unsubscribe from an instrument:
```json
{
  "type": "unsubscribe",
  "instrument": "BTC-PERPETUAL"
}
```

### Server to Client

#### Welcome message (on connection):
```json
{
  "type": "welcome",
  "message": "Welcome to Deribit Trader WebSocket Server"
}
```

#### Subscription confirmation:
```json
{
  "type": "subscription",
  "instrument": "BTC-PERPETUAL",
  "status": "subscribed"
}
```

#### Orderbook update:
```json
{
  "type": "orderbook",
  "instrument": "BTC-PERPETUAL",
  "timestamp": 1647356812345,
  "bids": [
    [50000.0, 1.5],
    [49995.0, 2.0]
  ],
  "asks": [
    [50005.0, 1.0],
    [50010.0, 2.5]
  ]
}
```

## Connecting External Clients

You can connect to the WebSocket server using any WebSocket client. Here are examples:

### Browser JavaScript
```javascript
const socket = new WebSocket('ws://localhost:8080');

socket.onopen = () => {
  console.log('Connected to WebSocket server');
  
  // Subscribe to BTC-PERPETUAL orderbook
  socket.send(JSON.stringify({
    type: 'subscribe',
    instrument: 'BTC-PERPETUAL'
  }));
};

socket.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Received:', data);
  
  if (data.type === 'orderbook') {
    // Process orderbook update
    console.log(`Top bid: ${data.bids[0][0]} @ ${data.bids[0][1]}`);
    console.log(`Top ask: ${data.asks[0][0]} @ ${data.asks[0][1]}`);
  }
};

socket.onclose = () => {
  console.log('Connection closed');
};
```

### Python with websockets
```python
import asyncio
import websockets
import json

async def connect_to_server():
    uri = "ws://localhost:8080"
    async with websockets.connect(uri) as websocket:
        # Subscribe to BTC-PERPETUAL
        await websocket.send(json.dumps({
            "type": "subscribe",
            "instrument": "BTC-PERPETUAL"
        }))
        
        # Receive and process messages
        while True:
            message = await websocket.recv()
            data = json.loads(message)
            
            if data.get('type') == 'orderbook':
                instrument = data.get('instrument')
                bids = data.get('bids', [])
                asks = data.get('asks', [])
                
                if bids and asks:
                    print(f"Orderbook for {instrument}")
                    print(f"Top bid: {bids[0][0]} @ {bids[0][1]}")
                    print(f"Top ask: {asks[0][0]} @ {asks[0][1]}")
                    print(f"Spread: {asks[0][0] - bids[0][0]}")

asyncio.get_event_loop().run_until_complete(connect_to_server())
```

### C++ with Boost.Beast
```cpp
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

int main() {
    try {
        // Create I/O context
        net::io_context ioc;
        
        // Resolve the host and port
        tcp::resolver resolver{ioc};
        auto const results = resolver.resolve("localhost", "8080");
        
        // Create websocket stream
        websocket::stream<tcp::socket> ws{ioc};
        
        // Connect to the server
        net::connect(ws.next_layer(), results.begin(), results.end());
        
        // Perform WebSocket handshake
        ws.handshake("localhost", "/");
        
        // Subscribe to BTC-PERPETUAL
        std::string subscribe_msg = R"({"type":"subscribe","instrument":"BTC-PERPETUAL"})";
        ws.write(net::buffer(subscribe_msg));
        
        // Receive and process messages
        while(true) {
            beast::flat_buffer buffer;
            ws.read(buffer);
            std::cout << beast::make_printable(buffer.data()) << std::endl;
        }
    } catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

## Performance Benchmarking

The system includes benchmarking tools to measure performance metrics:

1. **Order Placement Latency**: Time from order creation to order placement
2. **Market Data Processing**: Time to process and update internal orderbook
3. **WebSocket Message Propagation**: Time to broadcast updates to clients
4. **End-to-End Trading Loop**: Complete cycle time

To run benchmarks:
```bash
./deribit_benchmark [iterations]
```

Results are output to the console and saved to CSV files for further analysis.

## Configuration Options

The system can be configured by editing constants in the code or via command-line arguments:

- `WebSocketServer`: Port number (default: 8080)
- Initial instruments (default: BTC-PERPETUAL, ETH-PERPETUAL)
- Authentication credentials for Deribit Test API

## Troubleshooting

### Common Issues

#### Connection Errors to Deribit API
- Check internet connectivity
- Verify API credentials are correct
- Ensure the Deribit Test API is available

#### Build Issues
- Make sure all dependencies are installed
- Check compiler version supports C++17
- Verify OpenSSL paths are correctly specified

#### Performance Issues
- Run the benchmark tool to identify bottlenecks
- Check system resource usage
- Consider reducing the number of subscriptions

### Error Messages

- `WebSocket accept error`: Usually relates to client connection issues
- `Error writing: Operation canceled`: Can occur during shutdown or network interruption
- `Error resolving: ...`: DNS resolution failure

## Support and Additional Resources

- [Deribit API Documentation](https://docs.deribit.com/)
- [Boost.Beast WebSocket Documentation](https://www.boost.org/doc/libs/1_76_0/libs/beast/doc/html/beast/using_websocket.html)
- [nlohmann/json Documentation](https://github.com/nlohmann/json)

## License

This project is licensed under the MIT License - see the LICENSE file for details.