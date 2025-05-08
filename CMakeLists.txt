cmake_minimum_required(VERSION 3.21)
project(asio_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE CXX_SOURCE
    src/*.cpp
    src/*.h
    src/*.hpp
)

file(GLOB_RECURSE NOTED_FILES
    ${CMAKE_SOURCE_DIR}/src/server_client/*.cpp
    ${CMAKE_SOURCE_DIR}/src/server_client/*.hpp
)

list(REMOVE_ITEM CXX_SOURCE ${NOTED_FILES})

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