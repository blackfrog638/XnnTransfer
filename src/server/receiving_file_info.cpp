#include "receiving_file_info.hpp"

#include <filesystem>

ReceivingFileInfo::ReceivingFileInfo(const std::string& name, uintmax_t size, const std::string& save_dir)
    : original_file_name(name), expected_size(size), total_chunks(0), bytes_received(0) {
    fs::path dir_path(save_dir);
    if (!fs::exists(dir_path)) {
        fs::create_directories(dir_path);
        std::cout << "[Server] Created directory: " << save_dir << std::endl;
    }
    fs::path safe_filename = fs::path(name).filename();
    local_file_path = (dir_path / safe_filename).string();

    file_stream.open(local_file_path, std::ios::binary | std::ios::trunc);
    if (!file_stream.is_open()) {
        throw std::runtime_error("Server failed to open file for writing: " + local_file_path);
    }
    std::cout << "[Server] Receiving file: " << original_file_name << " -> " << local_file_path << std::endl;
}

ReceivingFileInfo::ReceivingFileInfo(ReceivingFileInfo&& other) noexcept
    : original_file_name(std::move(other.original_file_name)), local_file_path(std::move(other.local_file_path)),
      file_stream(std::move(other.file_stream)), // std::ofstream is movable
      expected_size(other.expected_size), total_chunks(other.total_chunks), bytes_received(other.bytes_received),
      received_chunks_mask(std::move(other.received_chunks_mask)) {}

ReceivingFileInfo& ReceivingFileInfo::operator=(ReceivingFileInfo&& other) noexcept {
    if (this != &other) {
        if (file_stream.is_open()) {
            file_stream.close();
        }
        original_file_name = std::move(other.original_file_name);
        local_file_path = std::move(other.local_file_path);
        file_stream = std::move(other.file_stream);
        expected_size = other.expected_size;
        total_chunks = other.total_chunks;
        bytes_received = other.bytes_received;
        received_chunks_mask = std::move(other.received_chunks_mask);
    }
    return *this;
}