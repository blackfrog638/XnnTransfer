#include "receiver/single_file_receiver.h"
#include "transfer.pb.h"
#include "util/hash.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

class SingleFileReceiverTest : public ::testing::Test {
  protected:
    void SetUp() override {
        received_dir_ = std::filesystem::current_path() / "received";
        if (std::filesystem::exists(received_dir_)) {
            std::filesystem::remove_all(received_dir_);
        }
    }

    void TearDown() override {
        if (std::filesystem::exists(received_dir_)) {
            std::filesystem::remove_all(received_dir_);
        }
    }

    // 创建测试块
    transfer::FileChunkRequest CreateChunk(const std::string& relative_path,
                                           uint64_t chunk_index,
                                           const std::string& data,
                                           const std::string& hash,
                                           bool is_last_chunk) {
        transfer::FileChunkRequest chunk;
        chunk.set_file_relative_path(relative_path);
        chunk.set_chunk_index(chunk_index);
        chunk.set_data(data);
        chunk.set_hash(hash);
        chunk.set_is_last_chunk(is_last_chunk);
        return chunk;
    }

    // 生成内容
    std::string GenerateContent(size_t size) {
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

    std::filesystem::path received_dir_;
};

TEST_F(SingleFileReceiverTest, ReceiveSmallFileInSingleChunk) {
    std::string relative_path = "test.txt";
    std::string content = "Hello, World!";

    // 计算内容的哈希
    const std::byte* bytes = reinterpret_cast<const std::byte*>(content.data());
    ConstDataBlock block(bytes, content.size());
    auto hash = util::hash::sha256_hex(block);
    ASSERT_TRUE(hash.has_value());

    // 创建接收器
    receiver::SingleFileReceiver receiver(relative_path, *hash);
    ASSERT_TRUE(receiver.is_valid());

    // 创建并处理块
    auto chunk = CreateChunk(relative_path, 0, content, *hash, true);
    bool result = receiver.handle_chunk(chunk);
    EXPECT_TRUE(result);

    // 完成并验证
    auto [ok, expected_hash, actual_hash] = receiver.finalize_and_verify();
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected_hash, *hash);
    EXPECT_EQ(actual_hash, *hash);

    // 验证文件内容
    auto file_path = received_dir_ / relative_path;
    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_TRUE(VerifyFile(file_path, content));
}

TEST_F(SingleFileReceiverTest, ReceiveLargeFileInMultipleChunks) {
    std::string relative_path = "large.bin";

    // 生成大内容并分块
    constexpr size_t kChunkSize = 1024 * 1024; // 1MB per chunk
    constexpr size_t kNumChunks = 3;
    std::vector<std::string> chunks;
    std::string full_content;

    for (size_t i = 0; i < kNumChunks; ++i) {
        std::string chunk_content = GenerateContent(kChunkSize);
        chunks.push_back(chunk_content);
        full_content += chunk_content;
    }

    // 计算完整文件的哈希（用于验证）
    const std::byte* bytes = reinterpret_cast<const std::byte*>(full_content.data());
    ConstDataBlock block(bytes, full_content.size());
    auto file_hash = util::hash::sha256_hex(block);
    ASSERT_TRUE(file_hash.has_value());

    // 创建接收器
    receiver::SingleFileReceiver receiver(relative_path, *file_hash);
    ASSERT_TRUE(receiver.is_valid());

    // 依次处理每个块
    for (size_t i = 0; i < kNumChunks; ++i) {
        // 计算块的哈希
        const std::byte* chunk_bytes = reinterpret_cast<const std::byte*>(chunks[i].data());
        ConstDataBlock chunk_block(chunk_bytes, chunks[i].size());
        auto chunk_hash = util::hash::sha256_hex(chunk_block);
        ASSERT_TRUE(chunk_hash.has_value());

        bool is_last = (i == kNumChunks - 1);
        auto chunk_request = CreateChunk(relative_path, i, chunks[i], *chunk_hash, is_last);

        bool result = receiver.handle_chunk(chunk_request);
        EXPECT_TRUE(result) << "Failed to handle chunk " << i;
    }

    // 完成并验证
    auto [ok, expected_hash, actual_hash] = receiver.finalize_and_verify();
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected_hash, *file_hash);
    EXPECT_EQ(actual_hash, *file_hash);

    // 验证文件内容
    auto file_path = received_dir_ / relative_path;
    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_TRUE(VerifyFile(file_path, full_content));
}

TEST_F(SingleFileReceiverTest, ReceiveFileWithSubdirectory) {
    std::string relative_path = "subdir/nested/file.txt";
    std::string content = "File in nested subdirectory";

    // 计算内容的哈希
    const std::byte* bytes = reinterpret_cast<const std::byte*>(content.data());
    ConstDataBlock block(bytes, content.size());
    auto hash = util::hash::sha256_hex(block);
    ASSERT_TRUE(hash.has_value());

    // 创建接收器
    receiver::SingleFileReceiver receiver(relative_path, *hash);
    ASSERT_TRUE(receiver.is_valid());

    // 创建并处理块
    auto chunk = CreateChunk(relative_path, 0, content, *hash, true);
    bool result = receiver.handle_chunk(chunk);
    EXPECT_TRUE(result);

    // 完成并验证
    auto [ok, expected_hash, actual_hash] = receiver.finalize_and_verify();
    EXPECT_TRUE(ok);

    // 验证文件和目录结构
    auto file_path = received_dir_ / relative_path;
    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_TRUE(std::filesystem::is_regular_file(file_path));
    EXPECT_TRUE(VerifyFile(file_path, content));
}

TEST_F(SingleFileReceiverTest, ReceiveEmptyFile) {
    std::string relative_path = "empty.txt";
    std::string content = "";

    // 计算空内容的哈希
    const std::byte* bytes = reinterpret_cast<const std::byte*>(content.data());
    ConstDataBlock block(bytes, content.size());
    auto hash = util::hash::sha256_hex(block);
    ASSERT_TRUE(hash.has_value());

    // 创建接收器
    receiver::SingleFileReceiver receiver(relative_path, *hash);
    ASSERT_TRUE(receiver.is_valid());

    // 创建并处理空块
    auto chunk = CreateChunk(relative_path, 0, content, *hash, true);
    bool result = receiver.handle_chunk(chunk);
    EXPECT_TRUE(result);

    // 完成并验证
    auto [ok, expected_hash, actual_hash] = receiver.finalize_and_verify();
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected_hash, *hash);
    EXPECT_EQ(actual_hash, *hash);

    // 验证空文件
    auto file_path = received_dir_ / relative_path;
    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_EQ(std::filesystem::file_size(file_path), 0);
}

TEST_F(SingleFileReceiverTest, RejectOutOfOrderChunks) {
    std::string relative_path = "ordered.txt";
    std::string chunk1_content = "First chunk";
    std::string chunk2_content = "Second chunk";

    // 计算块的哈希
    const std::byte* bytes1 = reinterpret_cast<const std::byte*>(chunk1_content.data());
    ConstDataBlock block1(bytes1, chunk1_content.size());
    auto hash1 = util::hash::sha256_hex(block1);
    ASSERT_TRUE(hash1.has_value());

    const std::byte* bytes2 = reinterpret_cast<const std::byte*>(chunk2_content.data());
    ConstDataBlock block2(bytes2, chunk2_content.size());
    auto hash2 = util::hash::sha256_hex(block2);
    ASSERT_TRUE(hash2.has_value());

    // 创建接收器
    receiver::SingleFileReceiver receiver(relative_path, "");
    ASSERT_TRUE(receiver.is_valid());

    // 先发送第二个块（应该失败）
    auto chunk2 = CreateChunk(relative_path, 1, chunk2_content, *hash2, false);
    bool result = receiver.handle_chunk(chunk2);
    EXPECT_FALSE(result) << "Should reject out-of-order chunk";

    // 发送第一个块（应该成功）
    auto chunk1 = CreateChunk(relative_path, 0, chunk1_content, *hash1, false);
    result = receiver.handle_chunk(chunk1);
    EXPECT_TRUE(result);

    // 现在发送第二个块（应该成功）
    result = receiver.handle_chunk(chunk2);
    EXPECT_TRUE(result);
}

TEST_F(SingleFileReceiverTest, RejectChunkWithWrongHash) {
    std::string relative_path = "hash_test.txt";
    std::string content = "Content with hash";

    // 计算正确的哈希
    const std::byte* bytes = reinterpret_cast<const std::byte*>(content.data());
    ConstDataBlock block(bytes, content.size());
    auto correct_hash = util::hash::sha256_hex(block);
    ASSERT_TRUE(correct_hash.has_value());

    // 创建接收器
    receiver::SingleFileReceiver receiver(relative_path, *correct_hash);
    ASSERT_TRUE(receiver.is_valid());

    // 使用错误的哈希创建块
    std::string wrong_hash = "0000000000000000000000000000000000000000000000000000000000000000";
    auto chunk = CreateChunk(relative_path, 0, content, wrong_hash, true);

    // 应该拒绝哈希不匹配的块
    bool result = receiver.handle_chunk(chunk);
    EXPECT_FALSE(result) << "Should reject chunk with wrong hash";
}

TEST_F(SingleFileReceiverTest, HandleChunkWithoutHash) {
    std::string relative_path = "no_hash.txt";
    std::string content = "Content without hash verification";

    // 创建接收器（不提供预期哈希）
    receiver::SingleFileReceiver receiver(relative_path, "");
    ASSERT_TRUE(receiver.is_valid());

    // 创建块（不提供哈希）
    auto chunk = CreateChunk(relative_path, 0, content, "", true);

    // 应该接受没有哈希的块
    bool result = receiver.handle_chunk(chunk);
    EXPECT_TRUE(result);

    // 完成（不验证哈希）
    auto [ok, expected_hash, actual_hash] = receiver.finalize_and_verify();
    EXPECT_TRUE(ok);

    // 验证文件内容
    auto file_path = received_dir_ / relative_path;
    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_TRUE(VerifyFile(file_path, content));
}

TEST_F(SingleFileReceiverTest, VerifyFinalFileHash) {
    std::string relative_path = "final_hash.txt";
    std::string content = "Content for final hash verification";

    // 计算预期的文件哈希
    const std::byte* bytes = reinterpret_cast<const std::byte*>(content.data());
    ConstDataBlock block(bytes, content.size());
    auto expected_file_hash = util::hash::sha256_hex(block);
    ASSERT_TRUE(expected_file_hash.has_value());

    // 创建接收器
    receiver::SingleFileReceiver receiver(relative_path, *expected_file_hash);
    ASSERT_TRUE(receiver.is_valid());

    // 处理块（使用块哈希）
    auto chunk_hash = util::hash::sha256_hex(block);
    auto chunk = CreateChunk(relative_path, 0, content, *chunk_hash, true);
    bool result = receiver.handle_chunk(chunk);
    EXPECT_TRUE(result);

    // 完成并验证文件哈希
    auto [ok, expected_hash, actual_hash] = receiver.finalize_and_verify();
    EXPECT_TRUE(ok) << "File hash should match";
    EXPECT_EQ(expected_hash, *expected_file_hash);
    EXPECT_EQ(actual_hash, *expected_file_hash);
}

TEST_F(SingleFileReceiverTest, DetectCorruptedFile) {
    std::string relative_path = "corrupted.txt";
    std::string content = "Original content";

    // 计算原始内容的哈希
    const std::byte* bytes = reinterpret_cast<const std::byte*>(content.data());
    ConstDataBlock block(bytes, content.size());
    auto original_hash = util::hash::sha256_hex(block);
    ASSERT_TRUE(original_hash.has_value());

    // 创建接收器，使用原始哈希
    receiver::SingleFileReceiver receiver(relative_path, *original_hash);
    ASSERT_TRUE(receiver.is_valid());

    // 发送不同的内容（模拟损坏）
    std::string corrupted_content = "Corrupted content";
    const std::byte* corrupted_bytes = reinterpret_cast<const std::byte*>(corrupted_content.data());
    ConstDataBlock corrupted_block(corrupted_bytes, corrupted_content.size());
    auto corrupted_chunk_hash = util::hash::sha256_hex(corrupted_block);

    auto chunk = CreateChunk(relative_path, 0, corrupted_content, *corrupted_chunk_hash, true);
    bool result = receiver.handle_chunk(chunk);
    EXPECT_TRUE(result); // 块本身的哈希是正确的

    // 完成时应检测到文件哈希不匹配
    auto [ok, expected_hash, actual_hash] = receiver.finalize_and_verify();
    EXPECT_FALSE(ok) << "Should detect corrupted file";
    EXPECT_EQ(expected_hash, *original_hash);
    EXPECT_NE(actual_hash, *original_hash);
}
