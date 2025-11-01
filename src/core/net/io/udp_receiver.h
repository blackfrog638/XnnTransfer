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

    // Template method to receive protobuf messages directly
    template<util::ProtobufMessage T>
    asio::awaitable<std::optional<T>> receive_message() {
        std::vector<std::byte> buffer(65536); // Max UDP datagram size
        MutDataBlock buf_span(buffer.data(), buffer.size());
        std::size_t bytes_received = co_await receive(buf_span);

        if (bytes_received == 0) {
            co_return std::nullopt;
        }

        co_return util::deserialize_message<T>(buf_span);
    }

  private:
    Executor& executor_;
    asio::ip::udp::socket& socket_;
    bool multicast_joined_ = false;
};
} // namespace core::net::io