#include "tcp_sender.h"
#include <asio/ip/tcp.hpp>
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>

namespace core::net::io {
TcpSender::TcpSender(Executor& executor,
                     asio::ip::tcp::socket& socket,
                     std::string_view host,
                     uint16_t port)
    : executor_(executor)
    , socket_(socket)
    , connector_(executor, socket)
    , connect_signal_(std::make_shared<asio::steady_timer>(executor.get_io_context()))
    , host_(host)
    , port_(port) {
    connect_signal_->expires_at(asio::steady_timer::time_point::max());
    start_connect();
}

void TcpSender::start_connect() {
    executor_.spawn([this]() -> asio::awaitable<void> {
        const bool connected = co_await connector_.connect(host_, port_);
        connected_.store(connected);
        connect_signal_->cancel();
    });
}

asio::awaitable<void> TcpSender::send(ConstDataBlock data) {
    if (data.empty()) {
        co_return;
    }
    if (!connected_.load()) {
        asio::error_code ec;
        co_await connect_signal_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
    }
    if (!socket_.is_open()) {
        co_return;
    }
    co_await socket_.async_send(asio::buffer(static_cast<const void*>(data.data()), data.size()),
                                asio::use_awaitable);
}

asio::awaitable<void> TcpSender::receive(MutDataBlock& buffer) {
    if (buffer.empty()) {
        co_return;
    }
    if (!connected_.load()) {
        asio::error_code ec;
        co_await connect_signal_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
    }
    if (!socket_.is_open()) {
        co_return;
    }

    std::size_t bytes_received = co_await socket_.async_receive(asio::buffer(static_cast<void*>(
                                                                                 buffer.data()),
                                                                             buffer.size()),
                                                                asio::use_awaitable);
    buffer = buffer.first(bytes_received);
    co_return;
}
} // namespace core::net::io