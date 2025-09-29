#pragma once

#include "core/executor.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <cstdint>

namespace core::net {
class Acceptor {
  public:
    explicit Acceptor(Executor& executor, asio::ip::tcp::socket& socket)
        : executor_(executor)
        , acceptor_(executor_.get_io_context())
        , socket_(socket) {}
    ~Acceptor();

    void listen(std::uint16_t port);
    asio::awaitable<void> accept();
    void refuse();

  private:
    Executor& executor_;
    asio::ip::tcp::acceptor acceptor_;
    asio::ip::tcp::socket& socket_;
};
} // namespace core::net