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
    MutDataBlock buffer_span(buffer.data(), buffer.size());
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

TEST(TcpIoTest, SenderReceivesData) {
    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    asio::ip::tcp::socket receiver_socket(executor.get_io_context());
    asio::ip::tcp::socket sender_socket(executor.get_io_context());
    constexpr std::uint16_t kPort = 40202;

    std::array<std::byte, 256> buffer{};
    MutDataBlock buffer_span(buffer.data(), buffer.size());
    const std::string response_text = "response-from-receiver";

    core::net::io::TcpReceiver receiver(executor, receiver_socket, kPort);
    core::net::io::TcpSender sender(executor, sender_socket, "127.0.0.1", kPort);

    std::atomic<bool> test_done{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        // Sender sends a request
        const std::string request_text = "request";
        auto request_payload = std::as_bytes(
            std::span<const char>(request_text.data(), request_text.size()));
        co_await sender.send(request_payload);

        // Receiver receives the request
        std::array<std::byte, 256> recv_buffer{};
        MutDataBlock recv_span(recv_buffer.data(), recv_buffer.size());
        co_await receiver.receive(recv_span);

        // Receiver sends a response
        auto response_payload = std::as_bytes(
            std::span<const char>(response_text.data(), response_text.size()));
        co_await receiver.send(response_payload);

        // Sender receives the response
        co_await sender.receive(buffer_span);

        test_done.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!test_done.load() && std::chrono::steady_clock::now() - start < 10s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(test_done.load()) << "TCP sender receive test timed out";

    std::string received_response(reinterpret_cast<const char*>(buffer_span.data()),
                                  buffer_span.size());
    EXPECT_EQ(received_response, response_text);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST(TcpIoTest, ReceiverSendsData) {
    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    asio::ip::tcp::socket receiver_socket(executor.get_io_context());
    asio::ip::tcp::socket sender_socket(executor.get_io_context());
    constexpr std::uint16_t kPort = 40203;

    std::array<std::byte, 256> buffer{};
    MutDataBlock buffer_span(buffer.data(), buffer.size());
    const std::string message_from_receiver = "hello-from-receiver";

    core::net::io::TcpReceiver receiver(executor, receiver_socket, kPort);
    core::net::io::TcpSender sender(executor, sender_socket, "127.0.0.1", kPort);

    std::atomic<bool> test_done{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        // Receiver sends data first
        auto payload = std::as_bytes(
            std::span<const char>(message_from_receiver.data(), message_from_receiver.size()));
        co_await receiver.send(payload);

        // Sender receives the data
        co_await sender.receive(buffer_span);

        test_done.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!test_done.load() && std::chrono::steady_clock::now() - start < 10s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(test_done.load()) << "TCP receiver send test timed out";

    std::string received_message(reinterpret_cast<const char*>(buffer_span.data()),
                                 buffer_span.size());
    EXPECT_EQ(received_message, message_from_receiver);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST(TcpIoTest, BidirectionalCommunication) {
    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    asio::ip::tcp::socket receiver_socket(executor.get_io_context());
    asio::ip::tcp::socket sender_socket(executor.get_io_context());
    constexpr std::uint16_t kPort = 40204;

    std::array<std::byte, 256> sender_buffer{};
    std::array<std::byte, 256> receiver_buffer{};
    MutDataBlock sender_buffer_span(sender_buffer.data(), sender_buffer.size());
    MutDataBlock receiver_buffer_span(receiver_buffer.data(), receiver_buffer.size());

    const std::string message1 = "ping";
    const std::string message2 = "pong";
    const std::string message3 = "final-ping";
    const std::string message4 = "final-pong";

    core::net::io::TcpReceiver receiver(executor, receiver_socket, kPort);
    core::net::io::TcpSender sender(executor, sender_socket, "127.0.0.1", kPort);

    std::atomic<bool> test_done{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        // Round 1: sender -> receiver
        auto msg1 = std::as_bytes(std::span<const char>(message1.data(), message1.size()));
        co_await sender.send(msg1);
        co_await receiver.receive(receiver_buffer_span);

        // Round 2: receiver -> sender
        auto msg2 = std::as_bytes(std::span<const char>(message2.data(), message2.size()));
        co_await receiver.send(msg2);
        co_await sender.receive(sender_buffer_span);

        // Round 3: sender -> receiver
        auto msg3 = std::as_bytes(std::span<const char>(message3.data(), message3.size()));
        co_await sender.send(msg3);
        MutDataBlock temp_buffer(receiver_buffer.data(), receiver_buffer.size());
        co_await receiver.receive(temp_buffer);

        // Round 4: receiver -> sender
        auto msg4 = std::as_bytes(std::span<const char>(message4.data(), message4.size()));
        co_await receiver.send(msg4);
        MutDataBlock temp_sender_buffer(sender_buffer.data(), sender_buffer.size());
        co_await sender.receive(temp_sender_buffer);

        test_done.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!test_done.load() && std::chrono::steady_clock::now() - start < 10s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(test_done.load()) << "TCP bidirectional communication test timed out";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}
} // namespace