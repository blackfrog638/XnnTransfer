#include "core/executor.h"
#include "sender/session.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace {

class SessionsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "session_test_dir";
        std::filesystem::create_directories(test_dir_);

        single_file_ = std::filesystem::temp_directory_path() / "single_test_file.txt";
        create_test_file(single_file_, "Single file content for testing");

        file1_ = test_dir_ / "file1.txt";
        file2_ = test_dir_ / "file2.txt";
        file3_ = test_dir_ / "file3.txt";
        create_test_file(file1_, "Content of file 1");
        create_test_file(file2_, "Content of file 2 - longer content here");
        create_test_file(file3_, "File 3");

        nested_dir_ = test_dir_ / "nested";
        std::filesystem::create_directories(nested_dir_ / "subdir1");
        std::filesystem::create_directories(nested_dir_ / "subdir2");

        nested_file1_ = nested_dir_ / "root_file.txt";
        nested_file2_ = nested_dir_ / "subdir1" / "nested1.txt";
        nested_file3_ = nested_dir_ / "subdir2" / "nested2.txt";

        create_test_file(nested_file1_, "Root level file content");
        create_test_file(nested_file2_, "Nested file 1 content");
        create_test_file(nested_file3_, "Nested file 2 content in subdir2");

        large_file_ = test_dir_ / "large_file.dat";
        std::string large_content(2 * 1024 * 1024 + 512, 'X');
        create_test_file(large_file_, large_content);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
        std::filesystem::remove(single_file_, ec);
    }

    static void create_test_file(const std::filesystem::path& path, const std::string& content) {
        std::ofstream file(path, std::ios::binary);
        if (file.is_open()) {
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            file.close();
        }
    }

    static uint64_t get_file_size(const std::filesystem::path& path) {
        std::error_code ec;
        auto size = std::filesystem::file_size(path, ec);
        return ec ? 0 : size;
    }

    std::filesystem::path test_dir_;
    std::filesystem::path single_file_;
    std::filesystem::path file1_, file2_, file3_;
    std::filesystem::path nested_dir_;
    std::filesystem::path nested_file1_, nested_file2_, nested_file3_;
    std::filesystem::path large_file_;
};

TEST_F(SessionsTest, SingleFileMetadataGeneration) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.100";

    sender::Session session(executor, kAddress, single_file_);
    const auto& metadata = session.metadata_request();

    EXPECT_FALSE(metadata.source().empty()) << "Source IP should be set";
    EXPECT_EQ(metadata.destination(), kAddress);
    EXPECT_EQ(metadata.session_id(), session.session_id_);

    ASSERT_EQ(metadata.files_size(), 1) << "Should have exactly 1 file";

    const auto& file_info = metadata.files(0);
    EXPECT_EQ(file_info.relative_path(), single_file_.filename().string());
    EXPECT_EQ(file_info.size(), get_file_size(single_file_));
    EXPECT_FALSE(file_info.hash().empty()) << "File hash should be generated";

    EXPECT_EQ(metadata.total_size(), get_file_size(single_file_));
}

TEST_F(SessionsTest, MultipleFilesMetadataGeneration) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.200";

    sender::Session session(executor, kAddress, file1_, file2_, file3_);
    const auto& metadata = session.metadata_request();

    EXPECT_EQ(metadata.destination(), kAddress);
    EXPECT_FALSE(metadata.session_id().empty());
    EXPECT_FALSE(metadata.source().empty());

    ASSERT_EQ(metadata.files_size(), 3) << "Should have 3 files";

    uint64_t expected_total = 0;
    for (int i = 0; i < metadata.files_size(); ++i) {
        const auto& file_info = metadata.files(i);
        EXPECT_FALSE(file_info.relative_path().empty()) << "File path should not be empty";
        EXPECT_GT(file_info.size(), 0) << "File size should be greater than 0";
        EXPECT_FALSE(file_info.hash().empty()) << "File hash should not be empty";
        expected_total += file_info.size();
    }

    EXPECT_EQ(metadata.total_size(), expected_total);
    EXPECT_EQ(metadata.total_size(),
              get_file_size(file1_) + get_file_size(file2_) + get_file_size(file3_));
}

TEST_F(SessionsTest, DirectoryMetadataGeneration) {
    core::Executor executor;
    const std::string kAddress = "10.0.0.1";

    sender::Session session(executor, kAddress, test_dir_);
    const auto& metadata = session.metadata_request();

    EXPECT_EQ(metadata.destination(), kAddress);
    EXPECT_EQ(metadata.session_id(), session.session_id_);
    EXPECT_GT(metadata.files_size(), 0) << "Directory should contain files";

    uint64_t calculated_total = 0;
    for (int i = 0; i < metadata.files_size(); ++i) {
        const auto& file_info = metadata.files(i);
        EXPECT_FALSE(file_info.relative_path().empty());
        EXPECT_GE(file_info.size(), 0);
        EXPECT_FALSE(file_info.hash().empty());
        calculated_total += file_info.size();
    }

    EXPECT_EQ(metadata.total_size(), calculated_total);
}

TEST_F(SessionsTest, NestedDirectoryMetadataGeneration) {
    core::Executor executor;
    const std::string kAddress = "172.16.0.1";

    sender::Session session(executor, kAddress, nested_dir_);
    const auto& metadata = session.metadata_request();

    EXPECT_EQ(metadata.destination(), kAddress);
    EXPECT_FALSE(metadata.session_id().empty());
    ASSERT_EQ(metadata.files_size(), 3) << "Nested directory should have 3 files";

    bool has_root_file = false;
    bool has_subdir1_file = false;
    bool has_subdir2_file = false;

    for (int i = 0; i < metadata.files_size(); ++i) {
        const auto& file_info = metadata.files(i);
        const std::string& path = file_info.relative_path();

        if (path.find("root_file.txt") != std::string::npos) {
            has_root_file = true;
        }
        if (path.find("subdir1") != std::string::npos) {
            has_subdir1_file = true;
        }
        if (path.find("subdir2") != std::string::npos) {
            has_subdir2_file = true;
        }

        EXPECT_FALSE(file_info.hash().empty());
        EXPECT_GT(file_info.size(), 0);
    }

    EXPECT_TRUE(has_root_file) << "Should include root level file";
    EXPECT_TRUE(has_subdir1_file) << "Should include subdir1 file";
    EXPECT_TRUE(has_subdir2_file) << "Should include subdir2 file";

    uint64_t expected_total = get_file_size(nested_file1_) + get_file_size(nested_file2_)
                              + get_file_size(nested_file3_);
    EXPECT_EQ(metadata.total_size(), expected_total);
}

TEST_F(SessionsTest, LargeFileMetadataGeneration) {
    core::Executor executor;
    const std::string kAddress = "192.168.100.50";

    sender::Session session(executor, kAddress, large_file_);
    const auto& metadata = session.metadata_request();

    ASSERT_EQ(metadata.files_size(), 1);

    const auto& file_info = metadata.files(0);
    EXPECT_EQ(file_info.size(), 2 * 1024 * 1024 + 512);
    EXPECT_EQ(metadata.total_size(), file_info.size());
    EXPECT_FALSE(file_info.hash().empty()) << "Large file should have hash";
}

TEST_F(SessionsTest, MixedFilesAndDirectoriesMetadataGeneration) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.1";

    sender::Session session(executor, kAddress, single_file_, file1_, nested_dir_);
    const auto& metadata = session.metadata_request();

    EXPECT_GE(metadata.files_size(), 5) << "Should have at least 5 files";

    for (int i = 0; i < metadata.files_size(); ++i) {
        const auto& file_info = metadata.files(i);
        EXPECT_FALSE(file_info.relative_path().empty());
        EXPECT_GE(file_info.size(), 0);
        EXPECT_FALSE(file_info.hash().empty());
    }

    EXPECT_GT(metadata.total_size(), 0);
}

TEST_F(SessionsTest, EmptyDirectoryMetadataGeneration) {
    auto empty_dir = test_dir_ / "empty_dir";
    std::filesystem::create_directories(empty_dir);

    core::Executor executor;
    const std::string kAddress = "127.0.0.1";

    sender::Session session(executor, kAddress, empty_dir);
    const auto& metadata = session.metadata_request();

    EXPECT_EQ(metadata.files_size(), 0);
    EXPECT_EQ(metadata.total_size(), 0);

    EXPECT_FALSE(metadata.session_id().empty());
    EXPECT_EQ(metadata.destination(), kAddress);
}

TEST_F(SessionsTest, NonExistentPathMetadataGeneration) {
    auto non_existent = test_dir_ / "does_not_exist.txt";

    core::Executor executor;
    const std::string kAddress = "192.168.1.1";

    sender::Session session(executor, kAddress, non_existent);
    const auto& metadata = session.metadata_request();

    EXPECT_EQ(metadata.files_size(), 0);
    EXPECT_EQ(metadata.total_size(), 0);
}

TEST_F(SessionsTest, SessionIdConsistency) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.1";

    sender::Session session(executor, kAddress, single_file_);

    EXPECT_EQ(session.session_id_, session.session_id_view_);
    EXPECT_EQ(session.metadata_request().session_id(), session.session_id_);
}

TEST_F(SessionsTest, MultipleSessionsUniqueIds) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.1";

    sender::Session session1(executor, kAddress, file1_);
    sender::Session session2(executor, kAddress, file2_);
    sender::Session session3(executor, kAddress, file3_);

    EXPECT_NE(session1.metadata_request().session_id(), session2.metadata_request().session_id());
    EXPECT_NE(session1.metadata_request().session_id(), session3.metadata_request().session_id());
    EXPECT_NE(session2.metadata_request().session_id(), session3.metadata_request().session_id());
}

TEST_F(SessionsTest, MetadataSerializable) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.1";

    sender::Session session(executor, kAddress, file1_, file2_);
    const auto& metadata = session.metadata_request();

    std::string serialized;
    ASSERT_TRUE(metadata.SerializeToString(&serialized)) << "Metadata should be serializable";
    EXPECT_GT(serialized.size(), 0) << "Serialized data should not be empty";

    transfer::TransferMetadataRequest deserialized;
    ASSERT_TRUE(deserialized.ParseFromString(serialized)) << "Should be able to deserialize";

    EXPECT_EQ(deserialized.session_id(), metadata.session_id());
    EXPECT_EQ(deserialized.source(), metadata.source());
    EXPECT_EQ(deserialized.destination(), metadata.destination());
    EXPECT_EQ(deserialized.total_size(), metadata.total_size());
    EXPECT_EQ(deserialized.files_size(), metadata.files_size());
}

TEST_F(SessionsTest, FileHashGeneration) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.1";

    sender::Session session(executor, kAddress, file1_);
    const auto& metadata = session.metadata_request();

    ASSERT_EQ(metadata.files_size(), 1);

    const auto& file_info = metadata.files(0);

    EXPECT_FALSE(file_info.hash().empty());
    EXPECT_GT(file_info.hash().length(), 10) << "Hash should be reasonably long";

    for (char c : file_info.hash()) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            << "Hash should contain only hex characters";
    }
}

TEST_F(SessionsTest, SameFileGeneratesSameHash) {
    core::Executor executor;
    const std::string kAddress = "192.168.1.1";

    sender::Session session1(executor, kAddress, file1_);
    sender::Session session2(executor, kAddress, file1_);

    const auto& metadata1 = session1.metadata_request();
    const auto& metadata2 = session2.metadata_request();

    ASSERT_EQ(metadata1.files_size(), 1);
    ASSERT_EQ(metadata2.files_size(), 1);

    EXPECT_EQ(metadata1.files(0).hash(), metadata2.files(0).hash());
}

} // namespace
