#include "core/executor.h"
#include "receiver/session.h"
#include "receiver/single_file_receiver.h"
#include "sender/session.h"
#include "sender/single_file_sender.h"
#include "util/hash.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

class FileTransferIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // 创建测试目录
        test_dir_ = std::filesystem::current_path() / "test_files";
        std::filesystem::create_directories(test_dir_);

        received_dir_ = std::filesystem::current_path() / "received";
        if (std::filesystem::exists(received_dir_)) {
            std::filesystem::remove_all(received_dir_);
        }
    }

    void TearDown() override {
        // 清理测试文件
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
        if (std::filesystem::exists(received_dir_)) {
            std::filesystem::remove_all(received_dir_);
        }
    }

    // 创建测试文件
    std::filesystem::path CreateTestFile(const std::string& filename, const std::string& content) {
        auto file_path = test_dir_ / filename;
        std::ofstream ofs(file_path, std::ios::binary);
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.close();
        return file_path;
    }

    // 生成随机内容
    std::string GenerateRandomContent(size_t size) {
        std::string content;
        content.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            content.push_back(static_cast<char>('A' + (i % 26)));
        }
        return content;
    }

    // 验证文件内容
    bool VerifyFile(const std::filesystem::path& path, const std::string& expected_content) {
        if (!std::filesystem::exists(path)) {
            return false;
        }

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            return false;
        }

        std::string actual_content((std::istreambuf_iterator<char>(ifs)),
                                   std::istreambuf_iterator<char>());
        return actual_content == expected_content;
    }

    // 验证文件哈希
    bool VerifyFileHash(const std::filesystem::path& path, const std::string& expected_hash) {
        auto actual_hash = util::hash::sha256_file_hex(path);
        if (!actual_hash.has_value()) {
            return false;
        }
        return *actual_hash == expected_hash;
    }

    // 运行 executor 直到完成或超时
    static void RunExecutor(core::Executor& executor,
                            std::atomic<bool>& done,
                            int timeout_sec = 30) {
        std::thread runner([&]() { executor.start(); });

        auto start = std::chrono::steady_clock::now();
        while (!done.load()
               && std::chrono::steady_clock::now() - start < std::chrono::seconds(timeout_sec)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        executor.stop();
        if (runner.joinable()) {
            runner.join();
        }
    }

    std::filesystem::path test_dir_;
    std::filesystem::path received_dir_;
};

// 测试发送和接收小文件
TEST_F(FileTransferIntegrationTest, SendAndReceiveSmallFile) {
    // 创建测试文件
    std::string content = "Hello, this is a test file!";
    auto file_path = CreateTestFile("small_test.txt", content);

    // 计算文件哈希
    auto expected_hash = util::hash::sha256_file_hex(file_path);
    ASSERT_TRUE(expected_hash.has_value());

    // 创建 executor 和会话
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15000;

    // 启动接收方
    auto receiver_session = std::make_unique<receiver::Session>(receiver_executor, port);

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 启动发送方
    auto sender_session = std::make_unique<sender::Session>(sender_executor,
                                                            "127.0.0.1",
                                                            port,
                                                            file_path);

    std::atomic<bool> sender_done{false};
    std::atomic<bool> receiver_done{false};

    // 运行发送方
    std::thread sender_thread([&]() {
        sender_executor.spawn([&]() -> asio::awaitable<void> {
            co_await sender_session->start();
            sender_done.store(true);
        }());
        sender_executor.start();
    });

    // 运行接收方
    std::thread receiver_thread([&]() {
        receiver_executor.spawn(
            [&]() -> asio::awaitable<void> { co_await receiver_session->start(); }());
        receiver_executor.start();
    });

    // 等待传输完成
    auto start = std::chrono::steady_clock::now();
    while (!sender_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止 executor
    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 验证接收的文件
    auto received_file = received_dir_ / "small_test.txt";
    ASSERT_TRUE(std::filesystem::exists(received_file));
    EXPECT_TRUE(VerifyFile(received_file, content));
    EXPECT_TRUE(VerifyFileHash(received_file, *expected_hash));
}

// 测试发送和接收大文件
TEST_F(FileTransferIntegrationTest, SendAndReceiveLargeFile) {
    // 创建大文件（5MB）
    std::string content = GenerateRandomContent(5 * 1024 * 1024);
    auto file_path = CreateTestFile("large_test.bin", content);

    // 计算文件哈希
    auto expected_hash = util::hash::sha256_file_hex(file_path);
    ASSERT_TRUE(expected_hash.has_value());

    // 创建 executor 和会话
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15001;

    // 启动接收方
    auto receiver_session = std::make_unique<receiver::Session>(receiver_executor, port);

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 启动发送方
    auto sender_session = std::make_unique<sender::Session>(sender_executor,
                                                            "127.0.0.1",
                                                            port,
                                                            file_path);

    std::atomic<bool> sender_done{false};

    // 运行发送方
    std::thread sender_thread([&]() {
        sender_executor.spawn([&]() -> asio::awaitable<void> {
            co_await sender_session->start();
            sender_done.store(true);
        }());
        sender_executor.start();
    });

    // 运行接收方
    std::thread receiver_thread([&]() {
        receiver_executor.spawn(
            [&]() -> asio::awaitable<void> { co_await receiver_session->start(); }());
        receiver_executor.start();
    });

    // 等待传输完成（大文件需要更长时间）
    auto start = std::chrono::steady_clock::now();
    while (!sender_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(30)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止 executor
    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 验证接收的文件
    auto received_file = received_dir_ / "large_test.bin";
    ASSERT_TRUE(std::filesystem::exists(received_file));
    EXPECT_TRUE(VerifyFileHash(received_file, *expected_hash));

    // 验证文件大小
    auto received_size = std::filesystem::file_size(received_file);
    EXPECT_EQ(received_size, content.size());
}

// 测试发送和接收多个文件
TEST_F(FileTransferIntegrationTest, SendAndReceiveMultipleFiles) {
    // 创建多个测试文件
    std::vector<std::pair<std::filesystem::path, std::string>> test_files
        = {{CreateTestFile("file1.txt", "Content of file 1"), "Content of file 1"},
           {CreateTestFile("file2.txt", "Content of file 2"), "Content of file 2"},
           {CreateTestFile("file3.txt", "Content of file 3"), "Content of file 3"}};

    // 计算文件哈希
    std::vector<std::string> expected_hashes;
    for (const auto& [path, _] : test_files) {
        auto hash = util::hash::sha256_file_hex(path);
        ASSERT_TRUE(hash.has_value());
        expected_hashes.push_back(*hash);
    }

    // 创建 executor 和会话
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15002;

    // 启动接收方
    auto receiver_session = std::make_unique<receiver::Session>(receiver_executor, port);

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 启动发送方（发送多个文件）
    auto sender_session = std::make_unique<sender::Session>(sender_executor,
                                                            "127.0.0.1",
                                                            port,
                                                            test_files[0].first,
                                                            test_files[1].first,
                                                            test_files[2].first);

    std::atomic<bool> sender_done{false};

    // 运行发送方
    std::thread sender_thread([&]() {
        sender_executor.spawn([&]() -> asio::awaitable<void> {
            co_await sender_session->start();
            sender_done.store(true);
        }());
        sender_executor.start();
    });

    // 运行接收方
    std::thread receiver_thread([&]() {
        receiver_executor.spawn(
            [&]() -> asio::awaitable<void> { co_await receiver_session->start(); }());
        receiver_executor.start();
    });

    // 等待传输完成
    auto start = std::chrono::steady_clock::now();
    while (!sender_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止 executor
    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 验证所有接收的文件
    for (size_t i = 0; i < test_files.size(); ++i) {
        auto received_file = received_dir_ / ("file" + std::to_string(i + 1) + ".txt");
        ASSERT_TRUE(std::filesystem::exists(received_file)) << "File " << i + 1 << " not found";
        EXPECT_TRUE(VerifyFile(received_file, test_files[i].second))
            << "File " << i + 1 << " content mismatch";
        EXPECT_TRUE(VerifyFileHash(received_file, expected_hashes[i]))
            << "File " << i + 1 << " hash mismatch";
    }
}

// 测试带子目录的文件传输
TEST_F(FileTransferIntegrationTest, SendAndReceiveFileWithSubdirectory) {
    // 创建子目录
    auto subdir = test_dir_ / "subdir";
    std::filesystem::create_directories(subdir);

    // 在子目录中创建文件
    std::string content = "File in subdirectory";
    auto file_path = subdir / "nested_file.txt";
    std::ofstream ofs(file_path, std::ios::binary);
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.close();

    // 计算文件哈希
    auto expected_hash = util::hash::sha256_file_hex(file_path);
    ASSERT_TRUE(expected_hash.has_value());

    // 创建 executor 和会话
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15003;

    // 启动接收方
    auto receiver_session = std::make_unique<receiver::Session>(receiver_executor, port);

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 启动发送方
    auto sender_session = std::make_unique<sender::Session>(sender_executor,
                                                            "127.0.0.1",
                                                            port,
                                                            file_path);

    std::atomic<bool> sender_done{false};

    // 运行发送方
    std::thread sender_thread([&]() {
        sender_executor.spawn([&]() -> asio::awaitable<void> {
            co_await sender_session->start();
            sender_done.store(true);
        }());
        sender_executor.start();
    });

    // 运行接收方
    std::thread receiver_thread([&]() {
        receiver_executor.spawn(
            [&]() -> asio::awaitable<void> { co_await receiver_session->start(); }());
        receiver_executor.start();
    });

    // 等待传输完成
    auto start = std::chrono::steady_clock::now();
    while (!sender_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止 executor
    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 验证接收的文件（应该在 received/subdir/nested_file.txt）
    auto received_file = received_dir_ / "subdir" / "nested_file.txt";
    ASSERT_TRUE(std::filesystem::exists(received_file));
    EXPECT_TRUE(VerifyFile(received_file, content));
    EXPECT_TRUE(VerifyFileHash(received_file, *expected_hash));
}

// 测试空文件传输
TEST_F(FileTransferIntegrationTest, SendAndReceiveEmptyFile) {
    // 创建空文件
    auto file_path = CreateTestFile("empty_file.txt", "");

    // 计算文件哈希
    auto expected_hash = util::hash::sha256_file_hex(file_path);
    ASSERT_TRUE(expected_hash.has_value());

    // 创建 executor 和会话
    core::Executor sender_executor;
    core::Executor receiver_executor;

    constexpr uint16_t port = 15004;

    // 启动接收方
    auto receiver_session = std::make_unique<receiver::Session>(receiver_executor, port);

    // 等待接收方启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 启动发送方
    auto sender_session = std::make_unique<sender::Session>(sender_executor,
                                                            "127.0.0.1",
                                                            port,
                                                            file_path);

    std::atomic<bool> sender_done{false};

    // 运行发送方
    std::thread sender_thread([&]() {
        sender_executor.spawn([&]() -> asio::awaitable<void> {
            co_await sender_session->start();
            sender_done.store(true);
        }());
        sender_executor.start();
    });

    // 运行接收方
    std::thread receiver_thread([&]() {
        receiver_executor.spawn(
            [&]() -> asio::awaitable<void> { co_await receiver_session->start(); }());
        receiver_executor.start();
    });

    // 等待传输完成
    auto start = std::chrono::steady_clock::now();
    while (!sender_done.load()
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止 executor
    sender_executor.stop();
    receiver_executor.stop();

    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    // 验证接收的空文件
    auto received_file = received_dir_ / "empty_file.txt";
    ASSERT_TRUE(std::filesystem::exists(received_file));
    EXPECT_TRUE(VerifyFile(received_file, ""));
    EXPECT_TRUE(VerifyFileHash(received_file, *expected_hash));
}
