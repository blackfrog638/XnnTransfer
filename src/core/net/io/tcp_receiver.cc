#include "tcp_receiver.h"

namespace core::net::io {
TcpReceiver::TcpReceiver(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port)
    : executor_(executor)
    , socket_(socket)
    , acceptor_(executor, socket) {
    acceptor_.listen(port);
    executor_.spawn(acceptor_.accept());
}

asio::awaitable<void> TcpReceiver::receive(MutDataBlock& buffer) {
    if (!socket_.is_open() || buffer.empty()) {
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