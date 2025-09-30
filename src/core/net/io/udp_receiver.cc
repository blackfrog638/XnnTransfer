#include "udp_receiver.h"
#include <asio/ip/multicast.hpp>
#include <cstddef>

namespace core::net::io {
UdpReceiver::UdpReceiver(Executor& executor, asio::ip::udp::socket& socket)
    : executor_(executor)
    , socket_(socket) {
    asio::ip::udp::endpoint local_endpoint(asio::ip::address_v4::any(), kMulticastPort);
    socket_.open(local_endpoint.protocol());
    socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    socket_.bind(local_endpoint);

    socket_.set_option(
        asio::ip::multicast::join_group(asio::ip::make_address_v4(std::string(kMulticastAddress))));
}

UdpReceiver::UdpReceiver(Executor& executor,
                         asio::ip::udp::socket& socket,
                         const asio::ip::address& address,
                         std::uint16_t port)
    : executor_(executor)
    , socket_(socket) {
    asio::ip::udp::endpoint local_endpoint(asio::ip::address_v4::any(), port);
    socket_.open(local_endpoint.protocol());
    socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    socket_.bind(local_endpoint);

    if (address.is_multicast()) {
        socket_.set_option(asio::ip::multicast::join_group(address.to_v4()));
        multicast_joined_ = true;
    }
}

asio::awaitable<std::size_t> UdpReceiver::receive(MutDataBlock& buffer) {
    if (buffer.empty()) {
        co_return 0;
    }
    std::size_t bytes_received = co_await socket_.async_receive(asio::buffer(static_cast<void*>(
                                                                                 buffer.data()),
                                                                             buffer.size()),
                                                                asio::use_awaitable);
    buffer = buffer.first(bytes_received);
    co_return bytes_received;
}
} // namespace core::net::io