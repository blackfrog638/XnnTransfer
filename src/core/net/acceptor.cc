#include "acceptor.h"
#include "spdlog/spdlog.h"
#include <asio/use_awaitable.hpp>

namespace core::net {
Acceptor::~Acceptor() = default;

void Acceptor::listen(uint16_t port) {
    acceptor_.open(asio::ip::tcp::v4());
    acceptor_.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
    acceptor_.listen();
}

asio::awaitable<void> Acceptor::accept() {
    if (socket_.is_open()) {
        socket_.close();
    }
    co_await acceptor_.async_accept(socket_, asio::use_awaitable);
    const auto endpoint = socket_.remote_endpoint();
    spdlog::debug("Connection accepted from {}:{}", endpoint.address().to_string(), endpoint.port());
    co_return;
}

void Acceptor::refuse() {
    if (socket_.is_open()) {
        socket_.close();
    }
    const auto endpoint = socket_.remote_endpoint();
    spdlog::debug("Connection refused from {}:{}", endpoint.address().to_string(), endpoint.port());
}
} // namespace core::net