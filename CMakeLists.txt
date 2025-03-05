cmake_minimum_required(VERSION 3.14)
project(deribit_trader VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable FetchContent for dependencies
include(FetchContent)

# Dependencies
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

# nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "Fetching nlohmann/json...")
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(json)
endif()

# Create library with core functionality
add_library(deribit_core
    src/api_client.cpp
    src/order_manager.cpp
    src/market_data.cpp
    src/websocket_server.cpp
)

target_include_directories(deribit_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(deribit_core PUBLIC
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Link to nlohmann_json if available
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(deribit_core PUBLIC nlohmann_json::nlohmann_json)
elseif(TARGET nlohmann_json)
    target_link_libraries(deribit_core PUBLIC nlohmann_json)
endif()

# Main executable
add_executable(deribit_trader src/main.cpp)
target_link_libraries(deribit_trader PRIVATE deribit_core)

# Benchmarking tool
add_executable(deribit_benchmark src/benchmark.cpp)
target_link_libraries(deribit_benchmark PRIVATE deribit_core)

# Find Catch2 for testing
find_package(Catch2 QUIET)
if(NOT Catch2_FOUND)
    message(STATUS "Fetching Catch2...")
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v2.13.9
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# Tests
enable_testing()
add_executable(run_tests 
    tests/test_main.cpp
    tests/api_client_test.cpp
    tests/order_manager_test.cpp
)
target_link_libraries(run_tests PRIVATE deribit_core)

# Link to Catch2 if available
if(TARGET Catch2::Catch2)
    target_link_libraries(run_tests PRIVATE Catch2::Catch2)
elseif(TARGET Catch2)
    target_link_libraries(run_tests PRIVATE Catch2)
endif()
add_test(NAME AllTests COMMAND run_tests)