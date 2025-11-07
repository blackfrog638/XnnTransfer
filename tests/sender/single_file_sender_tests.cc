#include "core/executor.h"
#include "core/net/io/tcp_interactor.h"
#include "sender/single_file_sender.h"
#include "transfer.pb.h"
#include "util/hash.h"
#include <asio/ip/tcp.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <thread>
#include <vector>


// 测试用的接收会话，用于捕获发送的块
class TestReceiverSession {
  public:
    TestReceiverSession(core::Executor& executor, uint16_t port)
        : executor_(executor)
        , socket_(executor.get_io_context())
        , interactor_(executor, socket_, port) {}

    void start() {
        interactor_.start();
        executor_.spawn([this]() -> asio::awaitable<void> { co_await receive_loop(); }());
    }

    std::vector<transfer::FileChunkRequest> received_chunks;
    std::atomic<bool> done{false};

  private:
    asio::awaitable<void> receive_loop() {
        while (!done.load()) {
            auto chunk_opt = co_await interactor_.receive_message<transfer::FileChunkRequest>();
            if (chunk_opt.has_value()) {
                received_chunks.push_back(*chunk_opt);
                if (chunk_opt->is_last_chunk()) {
                    done.store(true);
                }
            }
        }
    }

    core::Executor& executor_;
    asio::ip::tcp::socket socket_;
    core::net::io::TcpInteractor interactor_;
};

// 测试用的发送会话
class TestSenderSession : public core::net::io::Session {
  public:
    TestSenderSession(core::Executor& executor, std::string_view host, uint16_t port)
        : core::net::io::Session(executor, host, port) {}

  protected:
    asio::awaitable<void> handle_message(const MessageWrapper&) override { co_return; }
};

class SingleFileSenderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        test_dir_ = std::filesystem::current_path() / "sender_test_files";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }

    std::filesystem::path CreateTestFile(const std::string& filename, const std::string& content) {
        auto file_path = test_dir_ / filename;
        std::ofstream ofs(file_path, std::ios::binary);
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.close();
        return file_path;
    }

    static std::string GenerateContent(size_t size) {
        std::string content;
        content.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            content.push_back(static_cast<char>('A' + (i % 26)));
        }
        return content;
    }

    std::filesystem::path test_dir_;
};

TEST_F(SingleFileSenderTest, SendSmallFile) {
    // 创建小文件
    std::string content = "Hello, World!";
    auto file_path = CreateTestFile("small.txt", content);

    auto hash = util::hash::sha256_file_hex(file_path);
    ASSERT_TRUE(hash.has_value());

    // 创建 FileInfoRequest
    transfer::FileInfoRequest file_info;
    file_info.set_relative_path(file_path.string());
    file_info.set_size(content.size());
    file_info.set_hash(*hash);

    // 创建发送方和接收方 executor
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15100;

    // 启动测试接收器
    auto test_receiver = std::make_shared<TestReceiverSession>(receiver_executor, port);
    test_receiver->start();

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 启动发送会话
    auto sender_session = std::make_shared<TestSenderSession>(sender_executor, "127.0.0.1", port);

    // 创建 SingleFileSender (传递绝对路径)
    sender::SingleFileSender file_sender(sender_executor, *sender_session, file_info, file_path);

    // 运行接收方
    std::thread receiver_thread([&]() { receiver_executor.start(); });

    // 发送文件
    std::atomic<bool> send_done{false};
    sender_executor.spawn([&]() -> asio::awaitable<void> {
        co_await file_sender.send_file();
        send_done.store(true);
    }());

    std::thread sender_thread([&]() { sender_executor.start(); });

    // 等待完成
    auto start = std::chrono::steady_clock::now();
    while (!test_receiver->done.load() && !send_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 验证发送的数据块
    ASSERT_EQ(test_receiver->received_chunks.size(), 1);

    const auto& chunk = test_receiver->received_chunks[0];
    EXPECT_EQ(chunk.file_relative_path(), file_path.string());
    EXPECT_EQ(chunk.chunk_index(), 0);
    EXPECT_EQ(chunk.data(), content);
    EXPECT_EQ(chunk.hash(), *hash);
    EXPECT_TRUE(chunk.is_last_chunk());
}

TEST_F(SingleFileSenderTest, SendLargeFileInChunks) {
    // 创建大文件（3MB）
    std::string content = GenerateContent(3 * 1024 * 1024);
    auto file_path = CreateTestFile("large.bin", content);

    auto hash = util::hash::sha256_file_hex(file_path);
    ASSERT_TRUE(hash.has_value());

    // 创建 FileInfoRequest
    transfer::FileInfoRequest file_info;
    file_info.set_relative_path(file_path.string());
    file_info.set_size(content.size());
    file_info.set_hash(*hash);

    // 创建发送方和接收方 executor
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15101;

    // 启动测试接收器
    auto test_receiver = std::make_shared<TestReceiverSession>(receiver_executor, port);
    test_receiver->start();

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 启动发送会话
    auto sender_session = std::make_shared<TestSenderSession>(sender_executor, "127.0.0.1", port);

    // 创建 SingleFileSender (传递绝对路径)
    sender::SingleFileSender file_sender(sender_executor, *sender_session, file_info, file_path);

    // 运行接收方
    std::thread receiver_thread([&]() { receiver_executor.start(); });

    // 发送文件
    std::atomic<bool> send_done{false};
    sender_executor.spawn([&]() -> asio::awaitable<void> {
        co_await file_sender.send_file();
        send_done.store(true);
    }());

    std::thread sender_thread([&]() { sender_executor.start(); });

    // 等待完成
    auto start = std::chrono::steady_clock::now();
    while (!test_receiver->done.load() && !send_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 验证发送的数据块
    ASSERT_GT(test_receiver->received_chunks.size(), 1); // 应该有多个块

    // 验证每个块
    size_t total_data_size = 0;
    for (size_t i = 0; i < test_receiver->received_chunks.size(); ++i) {
        const auto& chunk = test_receiver->received_chunks[i];
        EXPECT_EQ(chunk.file_relative_path(), file_path.string());
        EXPECT_EQ(chunk.chunk_index(), i);
        EXPECT_EQ(chunk.hash(), *hash);

        total_data_size += chunk.data().size();

        // 只有最后一个块应该标记为 is_last_chunk
        if (i == test_receiver->received_chunks.size() - 1) {
            EXPECT_TRUE(chunk.is_last_chunk());
        }
    }

    // 验证总大小
    EXPECT_EQ(total_data_size, content.size());
}

TEST_F(SingleFileSenderTest, UpdateChunkStatus) {
    // 创建测试文件
    std::string content = GenerateContent(2 * 1024 * 1024); // 2MB
    auto file_path = CreateTestFile("test.bin", content);

    auto hash = util::hash::sha256_file_hex(file_path);
    ASSERT_TRUE(hash.has_value());

    // 创建 FileInfoRequest
    transfer::FileInfoRequest file_info;
    file_info.set_relative_path(file_path.string());
    file_info.set_size(content.size());
    file_info.set_hash(*hash);

    // 创建发送方和接收方 executor
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15102;

    // 启动测试接收器
    auto test_receiver = std::make_shared<TestReceiverSession>(receiver_executor, port);
    test_receiver->start();

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 启动发送会话
    auto sender_session = std::make_shared<TestSenderSession>(sender_executor, "127.0.0.1", port);

    // 创建 SingleFileSender (传递绝对路径)
    auto file_sender = std::make_shared<sender::SingleFileSender>(sender_executor,
                                                                  *sender_session,
                                                                  file_info,
                                                                  file_path);

    // 运行接收方
    std::thread receiver_thread([&]() { receiver_executor.start(); });

    // 发送文件
    std::atomic<bool> send_done{false};
    sender_executor.spawn([&]() -> asio::awaitable<void> {
        co_await file_sender->send_file();
        send_done.store(true);
    }());

    std::thread sender_thread([&]() { sender_executor.start(); });

    // 等待完成
    auto start = std::chrono::steady_clock::now();
    while (!test_receiver->done.load() && !send_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 更新块状态
    ASSERT_GT(test_receiver->received_chunks.size(), 0);

    // 测试成功更新
    file_sender->update_chunk_status(0, true);

    // 测试失败更新
    if (test_receiver->received_chunks.size() > 1) {
        file_sender->update_chunk_status(1, false);
    }

    // 测试无效索引
    file_sender->update_chunk_status(9999, true); // 应该不会崩溃
}
