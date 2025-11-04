#pragma once

#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "util/data_block.h"
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <string_view>

namespace core::net::io {
class UdpSender {
    // the UDP sender does not maintain a persistent connection.
    // it creates a UDP socket and binds it to a random port on localhost.
    // so the UDP sender can be used to send datagrams to different addresses and ports.
  public:
    explicit UdpSender(Executor& executor, asio::ip::udp::socket& socket);
    ~UdpSender() = default;

    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    asio::awaitable<void> send_to(ConstDataBlock data,
                                  std::string_view host = kMulticastAddress,
                                  uint16_t port = kMulticastPort);

    // 获取可复用的发送缓冲区
    std::vector<std::byte>& get_send_buffer() { return send_buffer_; }

    // 优化的 Protobuf 发送：使用 SerializeToArray + 可复用 Buffer
    template<util::ProtobufMessage T>
    asio::awaitable<void> send_message_to(const T& message,
                                          std::string_view host = kMulticastAddress,
                                          uint16_t port = kMulticastPort) {
        // 获取消息大小
        const size_t size = message.ByteSizeLong();

        // 确保 buffer 足够大（复用 buffer，避免频繁分配）
        if (send_buffer_.size() < size) {
            send_buffer_.resize(size);
        }

        // 直接序列化到 buffer（零拷贝）
        if (!message.SerializeToArray(send_buffer_.data(), static_cast<int>(size))) {
            co_return;
        }

        // 发送
        ConstDataBlock data(send_buffer_.data(), size);
        co_await send_to(data, host, port);
    }

  private:
    Executor& executor_;
    asio::ip::udp::socket& socket_;

    // 可复用的发送缓冲区，避免频繁内存分配
    std::vector<std::byte> send_buffer_;
};
} // namespace core::net::io