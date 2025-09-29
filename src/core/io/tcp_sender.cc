#include "core/io/tcp_sender.h"
#include <asio/ip/tcp.hpp>

namespace core::net::io {
TcpSender::TcpSender(Executor& executor,
                     asio::ip::tcp::socket& socket,
                     std::string_view host,
                     uint16_t port)
    : executor_(executor)
    , socket_(socket)
    , connector_(executor, socket) {
    executor_.spawn(connector_.connect(host, port));
}

asio::awaitable<void> TcpSender::send(ConstDataBlock data) {
    if (!socket_.is_open() || data.empty()) {
        co_return;
    }
    co_await socket_.async_send(asio::buffer(static_cast<const void*>(data.data()), data.size()),
                                asio::use_awaitable);
}
} // namespace core::net::io