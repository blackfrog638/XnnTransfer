cmake_minimum_required(VERSION 3.21)
project(asio_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS asio serialization)

add_executable(main src/main.cpp)

target_link_libraries(main PRIVATE Boost::asio Boost::serialization)