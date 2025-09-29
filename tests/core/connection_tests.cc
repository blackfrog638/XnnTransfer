#include "core/acceptor.h"
#include "core/connector.h"
#include "core/executor.h"
#include <array>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace {

TEST(ConnectionTest, ExecutorTest) {
    core::Executor executor;
    std::promise<void> done;
    auto future = done.get_future();

    executor.spawn([&done]() -> asio::awaitable<void> {
        done.set_value();
        co_return;
    });

    std::jthread runner([&executor]() { executor.start(); });

    ASSERT_EQ(future.wait_for(500ms), std::future_status::ready);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST(ConnectionTest, CoreNetworkingTest) {
    core::Executor executor;
    asio::ip::tcp::socket server_socket(executor.get_io_context());
    asio::ip::tcp::socket client_socket(executor.get_io_context());
    core::net::Acceptor acceptor(executor, server_socket);
    core::net::Connector connector(executor, client_socket);

    constexpr std::uint16_t kPort = 39001;
    acceptor.listen(kPort);

    executor.spawn(acceptor.accept());
    executor.spawn(connector.connect("127.0.0.1", kPort));

    std::jthread runner([&executor]() { executor.start(); });

    auto wait_until = [](auto&& condition) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (!condition()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    };

    ASSERT_TRUE(wait_until([&client_socket]() { return client_socket.is_open(); }));
    ASSERT_TRUE(wait_until([&server_socket]() { return server_socket.is_open(); }));

    std::array<char, 4> payload{'p', 'i', 'n', 'g'};
    asio::write(client_socket, asio::buffer(payload));

    std::array<char, 4> server_received{};
    asio::read(server_socket, asio::buffer(server_received));
    EXPECT_EQ(std::string_view(server_received.data(), server_received.size()), "ping");

    asio::write(server_socket, asio::buffer(server_received));

    std::array<char, 4> client_received{};
    asio::read(client_socket, asio::buffer(client_received));
    EXPECT_EQ(std::string_view(client_received.data(), client_received.size()), "ping");

    asio::error_code endpoint_ec;
    const auto client_remote = client_socket.remote_endpoint(endpoint_ec);
    EXPECT_FALSE(endpoint_ec);
    EXPECT_EQ(client_remote.port(), kPort);

    endpoint_ec.clear();
    const auto server_remote = server_socket.remote_endpoint(endpoint_ec);
    EXPECT_FALSE(endpoint_ec);
    EXPECT_TRUE(server_remote.address().is_loopback());

    connector.disconnect();
    acceptor.refuse();

    asio::error_code client_ec;
    std::array<char, 1> dummy{{'x'}};
    client_socket.write_some(asio::buffer(dummy), client_ec);
    EXPECT_TRUE(client_ec);

    asio::error_code server_ec;
    server_socket.write_some(asio::buffer(dummy), server_ec);
    EXPECT_TRUE(server_ec);

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
