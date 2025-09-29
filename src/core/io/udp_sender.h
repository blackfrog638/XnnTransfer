#pragma once

#include "core/executor.h"
#include "core/io/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <string_view>

namespace core::net::io {
class UdpSender {
    // the UDP sender does not maintain a persistent connection.
    // it creates a UDP socket and binds it to a random port on localhost.
    // so the UDP sender can be used to send datagrams to different addresses and ports.
  public:
    explicit UdpSender(Executor& executor, asio::ip::udp::socket& socket);
    ~UdpSender() = default;

    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    asio::awaitable<void> send_to(ConstDataBlock data, std::string_view host, uint16_t port);

  private:
    Executor& executor_;
    asio::ip::udp::socket& socket_;
};
} // namespace core::net::io