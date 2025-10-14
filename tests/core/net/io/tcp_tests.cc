#include "core/executor.h"
#include "core/net/io/tcp_receiver.h"
#include "core/net/io/tcp_sender.h"
#include <array>
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

TEST(TcpIoTest, SenderAndReceiverExchange) {
    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    asio::ip::tcp::socket receiver_socket(executor.get_io_context());
    asio::ip::tcp::socket sender_socket(executor.get_io_context());
    constexpr std::uint16_t kPort = 40201;

    std::array<std::byte, 256> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());
    const std::string payload_text = "tcp-hello";

    core::net::io::TcpReceiver receiver(executor, receiver_socket, kPort);
    core::net::io::TcpSender sender(executor, sender_socket, "127.0.0.1", kPort);

    std::atomic<bool> test_done{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        auto payload = std::as_bytes(
            std::span<const char>(payload_text.data(), payload_text.size()));
        co_await sender.send(payload);
        co_await receiver.receive(buffer_span);
        test_done.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!test_done.load() && std::chrono::steady_clock::now() - start < 10s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(test_done.load()) << "TCP test timed out";

    std::string received_payload(reinterpret_cast<const char*>(buffer_span.data()),
                                 buffer_span.size());
    EXPECT_EQ(received_payload, payload_text);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}
} // namespace