#pragma once

#include "core/executor.h"
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>

namespace core::net {
class UdpSender {
  public:
    explicit UdpSender(Executor& executor, asio::ip::udp::socket& socket);
    ~UdpSender() = default;

    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    asio::awaitable<void> send_to(std::string_view data, std::string_view host, uint16_t port);

  private:
    Executor& executor_;
    asio::ip::udp::socket& socket_;
};
} // namespace core::net