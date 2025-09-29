#pragma once

#include "core/acceptor.h"
#include "core/executor.h"
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <span>

namespace core::net::io {
class TcpReceiver {
  public:
    TcpReceiver(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port);
    ~TcpReceiver() = default;

    TcpReceiver(const TcpReceiver&) = delete;
    TcpReceiver& operator=(const TcpReceiver&) = delete;

    asio::awaitable<void> receive(std::span<char>& buffer);

  private:
    Executor& executor_;
    asio::ip::tcp::socket& socket_;
    core::net::Acceptor acceptor_;
};
} //namespace core::net::io