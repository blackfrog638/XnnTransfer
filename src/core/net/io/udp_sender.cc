#include "udp_sender.h"
#include <asio/ip/multicast.hpp>

namespace core::net::io {

UdpSender::UdpSender(Executor& executor, asio::ip::udp::socket& socket)
    : executor_(executor)
    , socket_(socket) {
    if (!socket_.is_open()) {
        socket_.open(asio::ip::udp::v4());
        socket_.set_option(asio::ip::udp::socket::reuse_address(true));
        socket_.bind(asio::ip::udp::endpoint(asio::ip::address_v4::any(), 0));
    }

    socket_.set_option(asio::ip::multicast::enable_loopback(true));
    socket_.set_option(asio::ip::multicast::hops(1));

    socket_.set_option(asio::ip::multicast::outbound_interface(asio::ip::address_v4::any()));
    socket_.set_option(asio::ip::multicast::outbound_interface(asio::ip::address_v4::loopback()));
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