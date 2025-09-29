#include "udp_sender.h"

namespace core::net::io {
UdpSender::UdpSender(Executor& executor, asio::ip::udp::socket& socket)
    : executor_(executor)
    , socket_(socket) {
    socket_.open(asio::ip::udp::v4());
    socket_.bind(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), 0));
}

asio::awaitable<void> UdpSender::send_to(const std::string_view data,
                                         const std::string_view host,
                                         const uint16_t port) {
    asio::ip::udp::endpoint endpoint(asio::ip::make_address(host.data()), port);
    executor_.spawn(socket_.async_send_to(asio::buffer(data), endpoint, asio::use_awaitable));
    co_return;
}
} // namespace core::net::io