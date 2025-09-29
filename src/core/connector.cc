#include "connector.h"
#include <asio/awaitable.hpp>
#include <asio/connect.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/redirect_error.hpp>
#include <asio/use_awaitable.hpp>

namespace core::net {
asio::awaitable<void> Connector::connect(std::string_view host, std::uint16_t port) {
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host.data()), port);
    co_await socket_.async_connect(endpoint, asio::use_awaitable);
    co_return;
}

void Connector::disconnect() {
    if (!socket_.is_open()) {
        return;
    }
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
    socket_.close();
}
} // namespace core::net