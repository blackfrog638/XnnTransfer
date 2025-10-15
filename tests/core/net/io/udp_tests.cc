#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "core/net/io/udp_sender.h"
#include "core/timer/spawn_with_timeout.h"
#include <asio/use_future.hpp>
#include <gtest/gtest.h>
#include <span>
#include <string>

using namespace std::chrono_literals;

namespace {

TEST(UdpIoTest, SenderAndReceiverExchange) {
    core::Executor executor;
    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    asio::ip::udp::socket sender_socket(executor.get_io_context());
    const std::uint16_t kPort = 40101;
    core::net::io::UdpReceiver receiver(executor,
                                        receiver_socket,
                                        asio::ip::make_address("127.0.0.1"),
                                        kPort);
    core::net::io::UdpSender sender(executor, sender_socket);

    std::array<std::byte, 256> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());
    const std::string payload_text = "udp-hello";

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            auto payload_span = std::span<const char>(payload_text.data(), payload_text.size());
            auto payload_bytes = std::as_bytes(payload_span);
            co_await sender.send_to(payload_bytes, "127.0.0.1", kPort);

            auto result = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                   2s);

            if (!result.has_value()) {
                co_return false; // Timeout
            }

            const auto bytes = result.value();
            EXPECT_EQ(bytes, payload_text.size());
            co_return true;
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });
    ASSERT_TRUE(test_future.get()) << "UDP receive timed out within coroutine";

    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(buffer_span.data()),
                               buffer_span.size()),
              payload_text);

    const auto sender_local_endpoint = sender_socket.local_endpoint();
    EXPECT_EQ(sender_local_endpoint.address(), asio::ip::address_v4::loopback());
    EXPECT_NE(sender_local_endpoint.port(), 0);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

} // namespace
