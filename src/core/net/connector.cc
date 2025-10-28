#include "connector.h"
#include <asio/awaitable.hpp>
#include <asio/connect.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/redirect_error.hpp>
#include <asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>

namespace core::net {
asio::awaitable<bool> Connector::connect(std::string_view host, std::uint16_t port) {
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host.data()), port);
    asio::error_code ec;
    co_await socket_.async_connect(endpoint, asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
        spdlog::error("Failed to connect to {}:{} - {}", host, port, ec.message());
        if (socket_.is_open()) {
            socket_.close();
        }
        co_return false;
    }

    spdlog::debug("Connected to {}:{}", host, port);
    co_return true;
}

void Connector::disconnect() {
    if (!socket_.is_open()) {
        return;
    }
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
    socket_.close();
    spdlog::debug("Disconnected. ");
}
} // namespace core::net