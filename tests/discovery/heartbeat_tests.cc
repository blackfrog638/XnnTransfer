#include "core/executor.h"
#include "core/net/io/udp_receiver.h"
#include "core/timer/spawn_with_timeout.h"
#include "discovery/heartbeat.h"
#include "heartbeat.pb.h"
#include "util/settings.h"
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>

using namespace std::chrono_literals;

namespace discovery {
namespace {

class HeartbeatTest : public ::testing::Test {
  protected:
    void SetUp() override { util::Settings::instance().init("test_settings.json"); }
    void TearDown() override { std::remove("test_settings.json"); }
};

TEST_F(HeartbeatTest, StartAndSendHeartbeat) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100);
    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    core::net::io::UdpReceiver receiver(executor, receiver_socket);

    std::array<std::byte, 512> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            try {
                executor.spawn(heartbeat.start());

                auto result = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                       2s);

                if (!result.has_value()) {
                    spdlog::error("Timeout: no heartbeat received");
                    co_return false;
                }

                co_await heartbeat.stop();
                co_return true;
            } catch (const std::exception& e) {
                spdlog::error("Exception in test: {}", e.what());
                co_return false;
            }
        },
        asio::use_future);

    std::thread runner([&executor]() {
        try {
            executor.start();
        } catch (const std::exception& e) {
            spdlog::error("Exception in executor thread: {}", e.what());
        }
    });

    bool result = false;
    try {
        result = test_future.get();
    } catch (const std::exception& e) {
        spdlog::error("Exception getting future result: {}", e.what());
        result = false;
    }

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    ASSERT_TRUE(result) << "Failed to receive heartbeat within timeout";

    std::string serialized_data(reinterpret_cast<const char*>(buffer_span.data()),
                                buffer_span.size());
    HeartbeatRequest heartbeat_msg;
    ASSERT_TRUE(heartbeat_msg.ParseFromString(serialized_data))
        << "Failed to parse heartbeat message";
}

TEST_F(HeartbeatTest, SendMultipleHeartbeats) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100);

    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    core::net::io::UdpReceiver receiver(executor, receiver_socket);

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<int> {
            executor.spawn(heartbeat.start());

            int count = 0;
            std::array<std::byte, 512> buffer{};

            for (int i = 0; i < 3; ++i) {
                core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());
                auto result = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                       2s);

                if (result.has_value()) {
                    std::string serialized_data(reinterpret_cast<const char*>(buffer_span.data()),
                                                buffer_span.size());
                    HeartbeatRequest msg;
                    if (msg.ParseFromString(serialized_data)) {
                        count++;
                    }
                }
            }

            co_await heartbeat.stop();
            co_return count;
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    int received_count = test_future.get();
    EXPECT_GE(received_count, 2) << "Should receive at least 2 heartbeats";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST_F(HeartbeatTest, TimestampIncreases) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100);

    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    core::net::io::UdpReceiver receiver(executor, receiver_socket);

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            executor.spawn(heartbeat.start());

            std::array<std::byte, 512> buffer1{};
            std::array<std::byte, 512> buffer2{};
            core::net::io::MutDataBlock buffer_span1(buffer1.data(), buffer1.size());
            core::net::io::MutDataBlock buffer_span2(buffer2.data(), buffer2.size());

            auto result1 = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span1),
                                                                    2s);
            if (!result1.has_value()) {
                co_return false;
            }

            std::string data1(reinterpret_cast<const char*>(buffer_span1.data()),
                              buffer_span1.size());
            HeartbeatRequest msg1;
            if (!msg1.ParseFromString(data1)) {
                co_return false;
            }

            auto result2 = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span2),
                                                                    2s);
            if (!result2.has_value()) {
                co_return false;
            }

            std::string data2(reinterpret_cast<const char*>(buffer_span2.data()),
                              buffer_span2.size());
            HeartbeatRequest msg2;
            if (!msg2.ParseFromString(data2)) {
                co_return false;
            }

            co_await heartbeat.stop();
            bool timestamps_increase = msg2.timestamp_ms() > msg1.timestamp_ms();
            co_return timestamps_increase;
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    ASSERT_TRUE(test_future.get()) << "Timestamps should increase between heartbeats";
    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

// 测试4: 停止和重启心跳
TEST_F(HeartbeatTest, StopAndRestart) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100);

    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    core::net::io::UdpReceiver receiver(executor, receiver_socket);

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            std::array<std::byte, 512> buffer{};
            core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());

            executor.spawn(heartbeat.start());
            auto result1 = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                    2s);
            if (!result1.has_value()) {
                co_return false;
            }
            co_await heartbeat.stop();

            asio::steady_timer wait_timer(co_await asio::this_coro::executor,
                                          std::chrono::milliseconds(300));
            co_await wait_timer.async_wait(asio::use_awaitable);
            executor.spawn(heartbeat.restart());

            buffer_span = core::net::io::MutDataBlock(buffer.data(), buffer.size());
            auto result2 = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                    2s);

            co_await heartbeat.stop();
            co_return result2.has_value();
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    ASSERT_TRUE(test_future.get()) << "Should receive heartbeat after restart";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST_F(HeartbeatTest, MultipleStartCallsDoNotDuplicate) {
    core::Executor executor;
    Heartbeat heartbeat(executor, 100);

    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    core::net::io::UdpReceiver receiver(executor, receiver_socket);

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<int> {
            executor.spawn(heartbeat.start());
            executor.spawn(heartbeat.start());
            executor.spawn(heartbeat.start());

            int count = 0;
            auto start_time = std::chrono::steady_clock::now();

            while (std::chrono::steady_clock::now() - start_time < 500ms) {
                std::array<std::byte, 512> buffer{};
                core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());

                auto result = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                       100ms);

                if (result.has_value()) {
                    count++;
                }
            }

            co_await heartbeat.stop();
            co_return count;
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    int received_count = test_future.get();

    EXPECT_LE(received_count, 7) << "Multiple start calls should not create duplicate senders";
    EXPECT_GE(received_count, 2) << "Should still receive some heartbeats";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST_F(HeartbeatTest, HeartbeatContainsUsername) {
    auto& settings = util::Settings::instance().get();
    settings["username"] = "test_user_123";

    core::Executor executor;
    Heartbeat heartbeat(executor, 100);

    asio::ip::udp::socket receiver_socket(executor.get_io_context());
    core::net::io::UdpReceiver receiver(executor, receiver_socket);

    std::array<std::byte, 512> buffer{};
    core::net::io::MutDataBlock buffer_span(buffer.data(), buffer.size());

    auto test_future = executor.spawn(
        [&]() -> asio::awaitable<std::string> {
            executor.spawn(heartbeat.start());

            auto result = co_await core::timer::spawn_with_timeout(receiver.receive(buffer_span),
                                                                   2s);

            co_await heartbeat.stop();

            if (!result.has_value()) {
                co_return "";
            }

            std::string serialized_data(reinterpret_cast<const char*>(buffer_span.data()),
                                        buffer_span.size());
            HeartbeatRequest msg;
            if (!msg.ParseFromString(serialized_data)) {
                co_return "";
            }

            co_return msg.username();
        },
        asio::use_future);

    std::thread runner([&executor]() { executor.start(); });

    std::string username = test_future.get();
    EXPECT_EQ(username, "test_user_123") << "Heartbeat should contain correct username";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

} // namespace
} // namespace discovery
