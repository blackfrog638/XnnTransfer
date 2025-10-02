#include "../../test_timeout.h"
#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "core/net/io/udp_sender.h"
#include <array>
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <asio/use_future.hpp>
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

    auto receive_future = asio::co_spawn(
        executor.get_io_context(),
        [&receiver, &buffer_span]() -> asio::awaitable<std::size_t> {
            co_return co_await receiver.receive(buffer_span);
        },
        asio::use_future);

    const std::string payload_text = "udp-hello";
    auto send_future = asio::co_spawn(
        executor.get_io_context(),
        [&sender, payload = std::string(payload_text), kPort]() mutable -> asio::awaitable<void> {
            auto payload_span = std::span<const char>(payload.data(), payload.size());
            auto payload_bytes = std::as_bytes(payload_span);
            co_await sender.send_to(payload_bytes, "127.0.0.1", kPort);
            co_return;
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    ASSERT_EQ(receive_future.wait_for(2s), std::future_status::ready);
    const auto bytes = receive_future.get();

    ASSERT_EQ(send_future.wait_for(2s), std::future_status::ready);
    ASSERT_NO_THROW(send_future.get());

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
