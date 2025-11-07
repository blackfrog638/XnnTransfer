#pragma once

#include "core/executor.h"
#include "core/net/acceptor.h"
#include "core/net/connector.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/error.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
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

    // Protobuf send using fixed 4-byte length prefix followed by message bytes
    template<util::ProtobufMessage T>
    asio::awaitable<void> send_message(const T& message) {
        const size_t payload_size = message.ByteSizeLong();
        if (payload_size > std::numeric_limits<uint32_t>::max()) {
            co_return;
        }
        const size_t total_size = sizeof(uint32_t) + payload_size;
        if (send_buffer_.size() < total_size) {
            send_buffer_.resize(total_size);
        }

        const uint32_t length = static_cast<uint32_t>(payload_size);
        auto* length_bytes = reinterpret_cast<unsigned char*>(send_buffer_.data());
        length_bytes[0] = static_cast<unsigned char>((length >> 24) & 0xFF);
        length_bytes[1] = static_cast<unsigned char>((length >> 16) & 0xFF);
        length_bytes[2] = static_cast<unsigned char>((length >> 8) & 0xFF);
        length_bytes[3] = static_cast<unsigned char>(length & 0xFF);

        if (!message.SerializeToArray(send_buffer_.data() + sizeof(uint32_t),
                                      static_cast<int>(payload_size))) {
            co_return;
        }

        ConstDataBlock data(send_buffer_.data(), total_size);
        co_await send(data);
    }

    // Protobuf receive using fixed 4-byte length prefix
    template<util::ProtobufMessage T>
    asio::awaitable<std::optional<T>> receive_message() {
        while (true) {
            if (residual_buffer_.size() >= sizeof(uint32_t)) {
                const auto* length_bytes
                    = reinterpret_cast<const unsigned char*>(residual_buffer_.data());
                const uint32_t length = (static_cast<uint32_t>(length_bytes[0]) << 24)
                                        | (static_cast<uint32_t>(length_bytes[1]) << 16)
                                        | (static_cast<uint32_t>(length_bytes[2]) << 8)
                                        | static_cast<uint32_t>(length_bytes[3]);

                const size_t total_required = sizeof(uint32_t) + static_cast<size_t>(length);
                if (residual_buffer_.size() >= total_required) {
                    T message;
                    if (!message.ParseFromArray(residual_buffer_.data() + sizeof(uint32_t),
                                                static_cast<int>(length))) {
                        residual_buffer_.erase(residual_buffer_.begin(),
                                               std::next(residual_buffer_.begin(),
                                                         static_cast<std::ptrdiff_t>(total_required)));
                        co_return std::nullopt;
                    }

                    residual_buffer_.erase(residual_buffer_.begin(),
                                           std::next(residual_buffer_.begin(),
                                                     static_cast<std::ptrdiff_t>(total_required)));
                    co_return message;
                }
            }

            if (receive_buffer_.size() < kDefaultBufferSize) {
                receive_buffer_.resize(kDefaultBufferSize);
            }

            MutDataBlock buf_span(receive_buffer_.data(), receive_buffer_.size());
            co_await receive(buf_span);

            if (buf_span.empty()) {
                co_return std::nullopt;
            }

            residual_buffer_.insert(residual_buffer_.end(), buf_span.begin(), buf_span.end());
        }
    }

  private:
        struct PendingSend {
                std::shared_ptr<std::vector<std::byte>> data;
                std::shared_ptr<asio::steady_timer> completion;
                std::optional<asio::error_code> result;
        };

    asio::awaitable<void> wait_for_ready();
        asio::awaitable<void> process_send_queue();

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
    std::vector<std::byte> residual_buffer_;

    asio::strand<asio::io_context::executor_type> send_strand_;
    std::deque<std::shared_ptr<PendingSend>> send_queue_;
    bool send_in_progress_{false};
};

} // namespace core::net::io
