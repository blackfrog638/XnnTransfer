#include "../../test_timeout.h"
#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "core/net/io/udp_sender.h"
#include <array>
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <thread>

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
    test_utils::TimeoutGuard timeout_guard(executor, 5s);

    std::array<std::byte, 256> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());
    std::promise<std::size_t> bytes_ready;
    auto bytes_future = bytes_ready.get_future();

    executor.spawn([&receiver,
                    &buffer_span,
                    promise = std::move(bytes_ready)]() mutable -> asio::awaitable<void> {
        const auto bytes = co_await receiver.receive(buffer_span);
        promise.set_value(bytes);
        co_return;
    });

    const std::string payload_text = "udp-hello";
    executor.spawn([&sender, payload_text, kPort]() -> asio::awaitable<void> {
        auto payload = std::as_bytes(
            std::span<const char>(payload_text.data(), payload_text.size()));
        co_await sender.send_to(payload, "127.0.0.1", kPort);
        co_return;
    }());

    std::thread runner([&executor]() { executor.start(); });

    ASSERT_EQ(bytes_future.wait_for(2s), std::future_status::ready);
    const auto bytes = bytes_future.get();

    EXPECT_EQ(bytes, payload_text.size());
    EXPECT_EQ(buffer_span.size(), payload_text.size());
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(buffer_span.data()),
                               buffer_span.size()),
              payload_text);

    const auto sender_local_endpoint = sender_socket.local_endpoint();
    EXPECT_EQ(sender_local_endpoint.address(), asio::ip::address_v4::loopback());
    EXPECT_NE(sender_local_endpoint.port(), 0);

    timeout_guard.cancel();
    EXPECT_EQ(false, timeout_guard.timed_out());
    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

} // namespace
