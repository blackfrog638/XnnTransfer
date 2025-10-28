#pragma once

#include "core/executor.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <string_view>

namespace core::net {
class Connector {
  public:
    explicit Connector(Executor& executor, asio::ip::tcp::socket& socket)
        : executor_(executor)
        , socket_(socket) {}
    ~Connector() = default;

    Connector(const Connector&) = delete;
    Connector& operator=(const Connector&) = delete;

    asio::awaitable<bool> connect(std::string_view host, std::uint16_t port);
    void disconnect();

  private:
    Executor& executor_;
    asio::ip::tcp::socket& socket_;
};
} // namespace core::net