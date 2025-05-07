cmake_minimum_required(VERSION 3.21)
project(asio_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE CXX_SOURCE
    src/*.cpp
    src/*.h
    src/*.hpp
)

find_package(Boost REQUIRED COMPONENTS asio serialization beast)
find_package(nlohmann_json REQUIRED)

add_executable(filesender ${CXX_SOURCE})

target_link_libraries(filesender 
    PRIVATE
        Boost::asio
        Boost::serialization
        Boost::beast
        nlohmann_json::nlohmann_json
)

target_include_directories(filesender
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
)