#include "core/io/tcp_receiver.h"

namespace core::net::io {
TcpReceiver::TcpReceiver(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port)
    : executor_(executor)
    , socket_(socket)
    , acceptor_(executor, socket) {
    acceptor_.listen(port);
    executor_.spawn(acceptor_.accept());
}

asio::awaitable<void> TcpReceiver::receive(std::span<char>& buffer) {
    if (!socket_.is_open()) {
        co_return;
    }
    std::size_t bytes_received = co_await socket_.async_receive(asio::buffer(buffer),
                                                                asio::use_awaitable);
    buffer = buffer.subspan(0, bytes_received);
    co_return;
}

} //namespace core::net::io