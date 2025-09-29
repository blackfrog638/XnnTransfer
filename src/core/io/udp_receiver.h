#pragma once

#include "core/executor.h"
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace core::net::io {
class UdpReceiver {
  public:
    static constexpr std::string_view kMulticastAddress = "239.255.0.1";
    static constexpr std::uint16_t kMulticastPort = 30001;

    // default constructor (without giving address and port) for joining default multicast group
    UdpReceiver(Executor& executor, asio::ip::udp::socket& socket);
    UdpReceiver(Executor& executor,
                asio::ip::udp::socket& socket,
                const asio::ip::address& address,
                std::uint16_t port);
    ~UdpReceiver() = default;

    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    asio::awaitable<size_t> receive(std::span<char>& buffer);

  private:
    Executor& executor_;
    asio::ip::udp::socket& socket_;
    bool multicast_joined_ = false;
};
} // namespace core::net::io