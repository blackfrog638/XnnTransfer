#pragma once

#include "asio/awaitable.hpp"
#include "core/executor.h"
#include "core/net/io/udp_sender.h"

namespace discovery {
class Heartbeat {
  public:
    explicit Heartbeat(core::Executor& executor, int interval_ms = 1000)
        : interval_ms_(interval_ms)
        , executor_(executor)
        , socket_(executor.get_io_context())
        , sender_(executor_, socket_) {}
    ~Heartbeat() = default;

    Heartbeat(const Heartbeat&) = delete;
    Heartbeat& operator=(const Heartbeat&) = delete;

    asio::awaitable<void> start();
    asio::awaitable<void> stop();
    asio::awaitable<void> restart();

  private:
    int interval_ms_;
    std::atomic<bool> running_{false};
    core::Executor& executor_;
    asio::ip::udp::socket socket_;
    core::net::io::UdpSender sender_;
};
} // namespace discovery