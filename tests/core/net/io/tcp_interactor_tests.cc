#include "core/executor.h"
#include "core/net/io/tcp_interactor.h"
#include "transfer.pb.h"
#include <asio/ip/tcp.hpp>
#include <atomic>
#include <gtest/gtest.h>
#include <thread>


using namespace core::net::io;

class TcpInteractorTest : public ::testing::Test {
  protected:
    static void RunExecutor(core::Executor& executor, std::atomic<bool>& done, int timeout_sec = 5) {
        std::thread runner([&]() { executor.start(); });

        auto start = std::chrono::steady_clock::now();
        while (!done.load()
               && std::chrono::steady_clock::now() - start < std::chrono::seconds(timeout_sec)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        executor.stop();
        if (runner.joinable()) {
            runner.join();
        }
    }
};

TEST_F(TcpInteractorTest, BasicProtobufMessage) {
    core::Executor executor;
    asio::ip::tcp::socket server_socket(executor.get_io_context());
    asio::ip::tcp::socket client_socket(executor.get_io_context());

    TcpInteractor server(executor, server_socket, 14650);
    TcpInteractor client(executor, client_socket, "127.0.0.1", 14650);

    server.start();
    client.start();

    std::atomic<bool> done{false};
    std::optional<transfer::FileInfoRequest> received;

    executor.spawn([&]() -> asio::awaitable<void> {
        transfer::FileInfoRequest req;
        req.set_relative_path("test.txt");
        req.set_size(1024);
        req.set_hash("abc123");
        co_await client.send_message(req);
    });

    executor.spawn([&]() -> asio::awaitable<void> {
        received = co_await server.receive_message<transfer::FileInfoRequest>();
        done.store(true);
        executor.stop();
    });

    RunExecutor(executor, done);

    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->relative_path(), "test.txt");
    EXPECT_EQ(received->size(), 1024);
    EXPECT_EQ(received->hash(), "abc123");
}

TEST_F(TcpInteractorTest, ProtobufRequestResponse) {
    core::Executor executor;
    asio::ip::tcp::socket server_socket(executor.get_io_context());
    asio::ip::tcp::socket client_socket(executor.get_io_context());

    TcpInteractor server(executor, server_socket, 14651);
    TcpInteractor client(executor, client_socket, "127.0.0.1", 14651);

    server.start();
    client.start();

    std::atomic<bool> done{false};
    std::optional<transfer::FileInfoResponse> response;

    executor.spawn([&]() -> asio::awaitable<void> {
        transfer::FileInfoRequest req;
        req.set_relative_path("file.txt");
        co_await client.send_message(req);
        response = co_await client.receive_message<transfer::FileInfoResponse>();
        done.store(true);
        executor.stop();
    });

    executor.spawn([&]() -> asio::awaitable<void> {
        auto req = co_await server.receive_message<transfer::FileInfoRequest>();
        transfer::FileInfoResponse resp;
        resp.set_relative_path(req->relative_path());
        resp.set_status(transfer::FileInfoResponse::SUCCESS);
        resp.set_message("OK");
        co_await server.send_message(resp);
    });

    RunExecutor(executor, done);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status(), transfer::FileInfoResponse::SUCCESS);
    EXPECT_EQ(response->message(), "OK");
}

TEST_F(TcpInteractorTest, MultipleMessages) {
    core::Executor executor;
    asio::ip::tcp::socket server_socket(executor.get_io_context());
    asio::ip::tcp::socket client_socket(executor.get_io_context());

    TcpInteractor server(executor, server_socket, 14652);
    TcpInteractor client(executor, client_socket, "127.0.0.1", 14652);

    server.start();
    client.start();

    constexpr int kCount = 10;
    std::atomic<int> received_count{0};
    std::atomic<bool> done{false};

    executor.spawn([&]() -> asio::awaitable<void> {
        for (int i = 0; i < kCount; ++i) {
            transfer::FileInfoRequest req;
            req.set_relative_path("file_" + std::to_string(i) + ".txt");
            req.set_size(i * 100);
            co_await client.send_message(req);
        }
    });

    executor.spawn([&]() -> asio::awaitable<void> {
        for (int i = 0; i < kCount; ++i) {
            auto req = co_await server.receive_message<transfer::FileInfoRequest>();
            if (req) {
                EXPECT_EQ(req->relative_path(), "file_" + std::to_string(i) + ".txt");
                EXPECT_EQ(req->size(), i * 100);
                received_count.fetch_add(1);
            }
        }
        done.store(true);
        executor.stop();
    });

    RunExecutor(executor, done);
    EXPECT_EQ(received_count.load(), kCount);
}
