
#include "core/executor.h"
#include "core/net/io/tcp_receiver.h"
#include "core/net/io/tcp_sender.h"
#include "core/time_out_guard.h"
#include <array>
#include <asio/awaitable.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/use_future.hpp>
#include <chrono>
#include <cstddef>
#include <future>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace {

bool wait_until(auto&& condition,
                std::chrono::milliseconds interval = 10ms,
                std::chrono::milliseconds timeout = 2000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(interval);
    }
    return true;
}

TEST(TcpIoTest, SenderAndReceiverExchange) {
    core::Executor executor;
    asio::ip::tcp::socket receiver_socket(executor.get_io_context());
    asio::ip::tcp::socket sender_socket(executor.get_io_context());
    constexpr std::uint16_t kPort = 40201;
    core::net::io::TcpReceiver receiver(executor, receiver_socket, kPort);
    core::net::io::TcpSender sender(executor, sender_socket, "127.0.0.1", kPort);

    std::thread runner([&executor]() { executor.start(); });

    std::array<std::byte, 256> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());
    const std::string payload_text = "tcp-hello";

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            auto payload = std::as_bytes(
                std::span<const char>(payload_text.data(), payload_text.size()));
            co_await sender.send(payload);
            bool receive_success = co_await core::timer::spawn_with_timeout(receiver.receive(
                                                                                buffer_span),
                                                                            3s);
            co_return receive_success;
        },
        asio::use_future);

    ASSERT_EQ(test_future.wait_for(5s), std::future_status::ready) << "TCP test timed out";
    ASSERT_TRUE(test_future.get()) << "TCP test failed or timed out within coroutine";

    std::string received_payload(reinterpret_cast<const char*>(buffer_span.data()),
                                 buffer_span.size());
    EXPECT_EQ(received_payload, payload_text);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(buffer_span.data()),
                               buffer_span.size()),
              payload_text);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}
} // namespace