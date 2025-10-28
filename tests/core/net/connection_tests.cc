#include "core/executor.h"
#include "core/net/acceptor.h"
#include "core/net/connector.h"
#include "core/timer/spawn_with_timeout.h"
#include <array>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <asio/write.hpp>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace {
TEST(ConnectionTest, CoreNetworkTest) {
    core::Executor executor;
    asio::ip::tcp::socket server_socket(executor.get_io_context());
    asio::ip::tcp::socket client_socket(executor.get_io_context());
    core::net::Acceptor acceptor(executor, server_socket);
    core::net::Connector connector(executor, client_socket);

    constexpr std::uint16_t kPort = 39001;
    acceptor.listen(kPort);

    executor.spawn(acceptor.accept());
    executor.spawn(connector.connect("127.0.0.1", kPort));

    std::thread runner([&executor]() { executor.start(); });

    // Wait for connections to establish using spawn_with_timeout
    auto connection_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            // Wait for both sockets to open
            bool wait_success = co_await core::timer::spawn_with_timeout(
                [&]() -> asio::awaitable<void> {
                    while (!client_socket.is_open() || !server_socket.is_open()) {
                        asio::steady_timer timer(co_await asio::this_coro::executor);
                        timer.expires_after(10ms);
                        co_await timer.async_wait(asio::use_awaitable);
                    }
                }(),
                2s);

            co_return wait_success;
        },
        asio::use_future);

    ASSERT_EQ(connection_future.wait_for(3s), std::future_status::ready)
        << "Connection establishment timed out";
    ASSERT_TRUE(connection_future.get()) << "Sockets failed to open within timeout";

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

TEST(ConnectionTest, ConnectFailureReturnsFalse) {
    core::Executor executor;
    asio::ip::tcp::socket client_socket(executor.get_io_context());
    core::net::Connector connector(executor, client_socket);

    std::thread runner([&executor]() { executor.start(); });

    constexpr std::uint16_t kUnusedPort = 39999;

    auto connect_future = executor.spawn(
        [&]() -> asio::awaitable<bool> {
            co_return co_await connector.connect("127.0.0.1", kUnusedPort);
        },
        asio::use_future);

    ASSERT_EQ(connect_future.wait_for(3s), std::future_status::ready)
        << "Connector did not report result in time";
    EXPECT_FALSE(connect_future.get());
    EXPECT_FALSE(client_socket.is_open());

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

} // namespace