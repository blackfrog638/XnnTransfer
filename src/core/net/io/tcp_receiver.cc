#include "tcp_receiver.h"
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>

namespace core::net::io {
TcpReceiver::TcpReceiver(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port)
    : executor_(executor)
    , socket_(socket)
    , acceptor_(executor, socket)
    , accept_signal_(std::make_shared<asio::steady_timer>(executor.get_io_context())) {
    acceptor_.listen(port);
    accept_signal_->expires_at(asio::steady_timer::time_point::max());
    start_accept();
}

void TcpReceiver::start_accept() {
    executor_.spawn([this]() -> asio::awaitable<void> {
        co_await acceptor_.accept();
        accepted_.store(true);
        accept_signal_->cancel();
    });
}

asio::awaitable<void> TcpReceiver::receive(MutDataBlock& buffer) {
    if (buffer.empty()) {
        co_return;
    }

    // Wait for connection to be accepted if not already accepted
    if (!accepted_.load()) {
        asio::error_code ec;
        co_await accept_signal_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
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

} //namespace core::net::io