#include "network.h"
#include <asio/io_context.hpp>
#include <asio/ip/host_name.hpp>
#include <asio/ip/tcp.hpp>
#include <spdlog/spdlog.h>


namespace util {

std::string get_local_ip(asio::io_context& io_context) {
    try {
        asio::ip::tcp::resolver resolver(io_context);
        std::string local_host = asio::ip::host_name();
        auto endpoints = resolver.resolve(asio::ip::tcp::v4(), local_host, "");
        std::string local_ip = endpoints.begin()->endpoint().address().to_string();
        spdlog::info("Local IP detected: {}", local_ip);
        return local_ip;
    } catch (const std::exception& e) {
        spdlog::warn("Failed to get local IP: {}", e.what());
        return "";
    }
}

} // namespace util
