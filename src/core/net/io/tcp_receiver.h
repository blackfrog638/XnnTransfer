#pragma once

#include "core/executor.h"
#include "core/net/acceptor.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

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
    core::net::Acceptor acceptor_;
    std::shared_ptr<asio::steady_timer> accept_signal_;
    std::atomic<bool> accepted_{false};
};
} //namespace core::net::io