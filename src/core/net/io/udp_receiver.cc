#include "udp_receiver.h"
#include <asio/ip/multicast.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>

namespace core::net::io {

UdpReceiver::UdpReceiver(Executor& executor, asio::ip::udp::socket& socket)
    : executor_(executor)
    , socket_(socket) {
    if (!socket_.is_open()) {
        socket_.open(asio::ip::udp::v4());
        socket_.set_option(asio::ip::udp::socket::reuse_address(true));

        asio::ip::udp::endpoint local_endpoint(asio::ip::address_v4::any(), kMulticastPort);
        socket_.bind(local_endpoint);

        asio::ip::address_v4 multicast_addr = asio::ip::make_address_v4(
            std::string(kMulticastAddress));
        socket_.set_option(asio::ip::multicast::join_group(multicast_addr));
        socket_.set_option(asio::ip::multicast::enable_loopback(true));
        
        // macOS 调试：记录 socket 配置
        spdlog::debug("UDP Receiver initialized: bound to {}:{}, joined multicast {}",
                     local_endpoint.address().to_string(),
                     local_endpoint.port(),
                     multicast_addr.to_string());
    }
}

UdpReceiver::UdpReceiver(Executor& executor,
                         asio::ip::udp::socket& socket,
                         const asio::ip::address& address,
                         std::uint16_t port)
    : executor_(executor)
    , socket_(socket) {
    asio::ip::udp::endpoint local_endpoint;
    if (address.is_v4()) {
        local_endpoint = asio::ip::udp::endpoint(asio::ip::address_v4::any(), port);
        socket_.open(asio::ip::udp::v4());
    } else {
        local_endpoint = asio::ip::udp::endpoint(asio::ip::address_v6::any(), port);
        socket_.open(asio::ip::udp::v6());
    }

    socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    socket_.bind(local_endpoint);

    if (address.is_multicast()) {
        if (address.is_v4()) {
            socket_.set_option(asio::ip::multicast::join_group(address.to_v4()));
        } else if (address.is_v6()) {
            socket_.set_option(asio::ip::multicast::join_group(address.to_v6()));
        }
        socket_.set_option(asio::ip::multicast::enable_loopback(true));
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