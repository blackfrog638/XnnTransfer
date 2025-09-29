#pragma once

#include "core/connector.h"
#include "core/executor.h"
#include "core/io/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
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

    asio::awaitable<void> send(ConstDataBlock data);

  private:
    Executor& executor_;
    asio::ip::tcp::socket& socket_;
    core::net::Connector connector_;
};
} // namespace core::net::io