#pragma once

#include "core/executor.h"
#include "core/net/acceptor.h"
#include "core/net/connector.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace core::net::io {

enum class TcpInteractorMode { Client, Server };

class TcpInteractor {
  public:
    TcpInteractor(Executor& executor,
                  asio::ip::tcp::socket& socket,
                  std::string_view host,
                  uint16_t port);

    TcpInteractor(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port);

    ~TcpInteractor() = default;

    TcpInteractor(const TcpInteractor&) = delete;
    TcpInteractor& operator=(const TcpInteractor&) = delete;

    TcpInteractorMode mode() const { return mode_; }

    void start();

    bool is_connected() const { return connected_.load(); }

    asio::awaitable<void> send(ConstDataBlock data);
    asio::awaitable<void> receive(MutDataBlock& buffer);

    std::vector<std::byte>& get_send_buffer() { return send_buffer_; }
    std::vector<std::byte>& get_receive_buffer() { return receive_buffer_; }

    // 优化的 Protobuf 发送：使用 SerializeToArray + 可复用 Buffer
    template<util::ProtobufMessage T>
    asio::awaitable<void> send_message(const T& message) {
        const size_t size = message.ByteSizeLong();

        if (send_buffer_.size() < size) {
            send_buffer_.resize(size);
        }
        if (!message.SerializeToArray(send_buffer_.data(), static_cast<int>(size))) {
            co_return;
        }

        ConstDataBlock data(send_buffer_.data(), size);
        co_await send(data);
    }

    // 优化的 Protobuf 接收：使用可复用 Buffer
    template<util::ProtobufMessage T>
    asio::awaitable<std::optional<T>> receive_message() {
        if (receive_buffer_.size() < kDefaultBufferSize) {
            receive_buffer_.resize(kDefaultBufferSize);
        }

        MutDataBlock buf_span(receive_buffer_.data(), receive_buffer_.size());
        co_await receive(buf_span);

        if (buf_span.empty()) {
            co_return std::nullopt;
        }

        T message;
        if (!message.ParseFromArray(buf_span.data(), static_cast<int>(buf_span.size()))) {
            co_return std::nullopt;
        }

        co_return message;
    }

  private:
    asio::awaitable<void> wait_for_ready();

    Executor& executor_;
    asio::ip::tcp::socket& socket_;
    TcpInteractorMode mode_;

    std::optional<core::net::Connector> connector_;
    std::string host_;
    std::uint16_t port_;

    std::optional<core::net::Acceptor> acceptor_;

    std::shared_ptr<asio::steady_timer> ready_signal_;
    std::atomic<bool> connected_{false};

    std::vector<std::byte> send_buffer_;
    std::vector<std::byte> receive_buffer_;
};

} // namespace core::net::io
