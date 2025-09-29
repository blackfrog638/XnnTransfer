#include "acceptor.h"
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
    co_return;
}

void Acceptor::refuse() {
    if (socket_.is_open()) {
        socket_.close();
    }
}
} // namespace core::net