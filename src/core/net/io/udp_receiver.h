#pragma once

#include "core/executor.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace core::net::io {
static constexpr std::string_view kMulticastAddress = "239.255.0.1";
static constexpr std::uint16_t kMulticastPort = 40101;
class UdpReceiver {
  public:
    // default constructor (without giving address and port) for joining default multicast group
    UdpReceiver(Executor& executor, asio::ip::udp::socket& socket);
    UdpReceiver(Executor& executor,
                asio::ip::udp::socket& socket,
                const asio::ip::address& address,
                std::uint16_t port);
    ~UdpReceiver() = default;

    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    asio::awaitable<std::size_t> receive(MutDataBlock& buffer);

    // 获取可复用的接收缓冲区
    std::vector<std::byte>& get_receive_buffer() { return receive_buffer_; }

    // 优化的 Protobuf 接收：使用 ParseFromArray + 可复用 Buffer
    template<util::ProtobufMessage T>
    asio::awaitable<std::optional<T>> receive_message() {
        // 确保接收 buffer 足够大（UDP 最大数据报大小）
        constexpr size_t kMaxUdpSize = 65536;
        if (receive_buffer_.size() < kMaxUdpSize) {
            receive_buffer_.resize(kMaxUdpSize);
        }

        MutDataBlock buf_span(receive_buffer_.data(), receive_buffer_.size());
        std::size_t bytes_received = co_await receive(buf_span);

        if (bytes_received == 0) {
            co_return std::nullopt;
        }

        // 直接从 buffer 解析（零拷贝）
        T message;
        if (!message.ParseFromArray(receive_buffer_.data(), static_cast<int>(bytes_received))) {
            co_return std::nullopt;
        }

        co_return message;
    }

  private:
    Executor& executor_;
    asio::ip::udp::socket& socket_;
    bool multicast_joined_ = false;

    // 可复用的接收缓冲区，避免频繁内存分配
    std::vector<std::byte> receive_buffer_;
};
} // namespace core::net::io