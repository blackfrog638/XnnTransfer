#include "udp_sender.h"

namespace core::net::io {
UdpSender::UdpSender(Executor& executor, asio::ip::udp::socket& socket)
    : executor_(executor)
    , socket_(socket) {
    socket_.open(asio::ip::udp::v4());
    socket_.bind(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), 0));
}

asio::awaitable<void> UdpSender::send_to(ConstDataBlock data, std::string_view host, uint16_t port) {
    if (data.empty()) {
        co_return;
    }

    const auto endpoint = asio::ip::udp::endpoint(asio::ip::make_address(host), port);
    co_await socket_.async_send_to(asio::buffer(static_cast<const void*>(data.data()), data.size()),
                                   endpoint,
                                   asio::use_awaitable);
}
} // namespace core::net::io