
#include "core/executor.h"
#include "core/io/tcp_receiver.h"
#include "core/io/tcp_sender.h"
#include <array>
#include <asio/awaitable.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <atomic>
#include <chrono>
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

    std::jthread runner([&executor]() { executor.start(); });

    ASSERT_TRUE(wait_until([&sender_socket]() { return sender_socket.is_open(); }));
    ASSERT_TRUE(wait_until([&receiver_socket]() { return receiver_socket.is_open(); }));

    asio::error_code ec;
    ASSERT_TRUE(wait_until([&]() {
        ec.clear();
        sender_socket.remote_endpoint(ec);
        return !ec;
    }));
    ASSERT_TRUE(wait_until([&]() {
        ec.clear();
        receiver_socket.remote_endpoint(ec);
        return !ec;
    }));

    std::array<char, 256> buffer{};
    std::span<char> buffer_span(buffer.data(), buffer.size());
    std::string received_payload;
    std::atomic_bool received{false};
    std::atomic_bool send_done{false};
    std::atomic_bool receive_error{false};

    executor.spawn([&receiver, &buffer_span, &received_payload, &received, &receive_error]()
                       -> asio::awaitable<void> {
        try {
            co_await receiver.receive(buffer_span);
            received_payload.assign(buffer_span.data(), buffer_span.size());
            received.store(true, std::memory_order_release);
        } catch (...) {
            receive_error.store(true, std::memory_order_release);
        }
        co_return;
    });

    const std::string payload = "tcp-hello";
    executor.spawn([&sender, payload, &send_done]() -> asio::awaitable<void> {
        co_await sender.send(payload);
        send_done.store(true, std::memory_order_release);
        co_return;
    });

    ASSERT_TRUE(wait_until([&send_done]() { return send_done.load(std::memory_order_acquire); },
                           10ms,
                           3000ms));
    ASSERT_FALSE(receive_error.load(std::memory_order_acquire));
    ASSERT_TRUE(wait_until([&received]() { return received.load(std::memory_order_acquire); },
                           10ms,
                           3000ms));
    EXPECT_EQ(received_payload, payload);
    EXPECT_EQ(buffer_span.size(), payload.size());
    EXPECT_EQ(std::string_view(buffer_span.data(), buffer_span.size()), payload);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

} // namespace