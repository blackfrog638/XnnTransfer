#include "core/executor.h"
#include "sender/single_file_sender.h"
#include <asio/awaitable.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

class SingleFileSenderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        test_file_path_ = std::filesystem::temp_directory_path() / "test_file.txt";
        create_test_file(test_file_path_, test_content_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(test_file_path_, ec);
    }

    static void create_test_file(const std::filesystem::path& path, const std::string& content) {
        std::ofstream file(path, std::ios::binary);
        ASSERT_TRUE(file.is_open()) << "Failed to create test file";
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        file.close();
    }

    std::filesystem::path test_file_path_;
    const std::string test_content_ = "Hello, this is a test file for SingleFileSender!";
};

TEST_F(SingleFileSenderTest, SendFileSuccessfully) {
    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    constexpr std::uint16_t kPort = 50201;
    const std::string kAddress = "127.0.0.1";

    sender::SingleFileSender sender(executor, test_file_path_, kAddress, kPort);

    std::atomic<bool> send_started{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        co_await sender.send();
        send_started.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!send_started.load() && std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(send_started.load()) << "File send operation timed out";

    const auto& status = sender.status();
    EXPECT_FALSE(status.empty()) << "No chunks were created";

    bool has_sent_chunks = false;
    for (const auto& chunk_status : status) {
        if (chunk_status == sender::ChunkStatus::Sent
            || chunk_status == sender::ChunkStatus::Sending
            || chunk_status == sender::ChunkStatus::Pending) {
            has_sent_chunks = true;
            break;
        }
    }
    EXPECT_TRUE(has_sent_chunks) << "No chunks were sent";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

TEST_F(SingleFileSenderTest, SendLargeFile) {
    const std::size_t large_file_size = 2 * 1024 * 1024 + 512;
    std::string large_content(large_file_size, 'A');

    auto large_file_path = std::filesystem::temp_directory_path() / "large_test_file.dat";
    create_test_file(large_file_path, large_content);

    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    constexpr std::uint16_t kPort = 50202;
    const std::string kAddress = "127.0.0.1";

    sender::SingleFileSender sender(executor, large_file_path, kAddress, kPort);

    std::atomic<bool> send_started{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        co_await sender.send();
        send_started.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!send_started.load() && std::chrono::steady_clock::now() - start < 10s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(send_started.load()) << "Large file send operation timed out";

    const auto& status = sender.status();
    EXPECT_EQ(status.size(), 3) << "Expected 3 chunks for 2.5MB file";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    std::error_code ec;
    std::filesystem::remove(large_file_path, ec);
}

TEST_F(SingleFileSenderTest, SendEmptyFile) {
    auto empty_file_path = std::filesystem::temp_directory_path() / "empty_file.txt";
    create_test_file(empty_file_path, "");

    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    constexpr std::uint16_t kPort = 50203;
    const std::string kAddress = "127.0.0.1";

    sender::SingleFileSender sender(executor, empty_file_path, kAddress, kPort);

    std::atomic<bool> send_completed{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        co_await sender.send();
        send_completed.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!send_completed.load() && std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(send_completed.load()) << "Empty file send operation timed out";

    const auto& status = sender.status();
    EXPECT_TRUE(status.empty()) << "Empty file should have no chunks";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }

    std::error_code ec;
    std::filesystem::remove(empty_file_path, ec);
}

TEST_F(SingleFileSenderTest, SendNonExistentFile) {
    auto non_existent_path = std::filesystem::temp_directory_path() / "non_existent.txt";

    core::Executor executor;
    std::thread runner([&executor]() { executor.start(); });

    constexpr std::uint16_t kPort = 50204;
    const std::string kAddress = "127.0.0.1";

    sender::SingleFileSender sender(executor, non_existent_path, kAddress, kPort);

    std::atomic<bool> send_completed{false};
    executor.spawn([&]() -> asio::awaitable<void> {
        co_await sender.send();
        send_completed.store(true);
    });

    auto start = std::chrono::steady_clock::now();
    while (!send_completed.load() && std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_TRUE(send_completed.load()) << "Non-existent file send operation timed out";

    const auto& status = sender.status();
    EXPECT_TRUE(status.empty()) << "Non-existent file should have no chunks";

    executor.stop();
    if (runner.joinable()) {
        runner.join();
    }
}

} // namespace
