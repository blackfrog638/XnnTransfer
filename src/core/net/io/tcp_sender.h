#pragma once

#include "core/executor.h"
#include "core/net/connector.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <memory>
#include <string_view>

namespace core::net::io {
class TcpSender {
    // if we make a tcp connection everytime we want to send something,
    // the performance will be very bad.
    // so the TCP sender is for a specialized connection that is kept alive
    // and used to send multiple times(based on connections).
  public:
    TcpSender(Executor& executor,
              asio::ip::tcp::socket& socket,
              std::string_view host,
              uint16_t port);
    ~TcpSender() = default;

    TcpSender(const TcpSender&) = delete;
    TcpSender& operator=(const TcpSender&) = delete;

    void start_connect();
    asio::awaitable<void> send(ConstDataBlock data);
    asio::awaitable<void> receive(MutDataBlock& buffer);

  private:
    Executor& executor_;
    asio::ip::tcp::socket& socket_;
    core::net::Connector connector_;
    std::shared_ptr<asio::steady_timer> connect_signal_;
    std::string host_;
    std::uint16_t port_;
    std::atomic<bool> connected_{false};
};
} // namespace core::net::io