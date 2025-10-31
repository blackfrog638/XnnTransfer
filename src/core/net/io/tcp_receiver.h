#pragma once

#include "core/executor.h"
#include "core/net/acceptor.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <cstdint>
#include <memory>

namespace core::net::io {
class TcpReceiver {
  public:
    TcpReceiver(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port);
    ~TcpReceiver() = default;

    TcpReceiver(const TcpReceiver&) = delete;
    TcpReceiver& operator=(const TcpReceiver&) = delete;

    void start_accept();
    asio::awaitable<void> receive(MutDataBlock& buffer);
    asio::awaitable<void> send(ConstDataBlock data);

  private:
    Executor& executor_;
    asio::ip::tcp::socket& socket_;
    core::net::Acceptor acceptor_;
    std::shared_ptr<asio::steady_timer> accept_signal_;
    std::atomic<bool> accepted_{false};
};
} //namespace core::net::io