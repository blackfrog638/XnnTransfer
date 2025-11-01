#pragma once

#include "core/executor.h"
#include "core/net/connector.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

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

    // Template method to send protobuf messages directly
    template<util::ProtobufMessage T>
    asio::awaitable<void> send_message(const T& message) {
        auto serialized = util::serialize_message(message);
        ConstDataBlock data(serialized.data(), serialized.size());
        co_await send(data);
    }

    // Template method to receive protobuf messages directly
    template<util::ProtobufMessage T>
    asio::awaitable<std::optional<T>> receive_message() {
        std::vector<std::byte> buffer(1024 * 1024 + 512); // 1MB + slack
        MutDataBlock buf_span(buffer.data(), buffer.size());
        co_await receive(buf_span);

        if (buf_span.empty()) {
            co_return std::nullopt;
        }

        co_return util::deserialize_message<T>(buf_span);
    }

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