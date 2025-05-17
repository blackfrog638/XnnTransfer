#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <boost/asio.hpp>

namespace fs = std::filesystem;
namespace net = boost::asio;

class ReceivingFileInfo {
  public:
    std::string original_file_name;
    std::string local_file_path;
    std::ofstream file_stream;
    uintmax_t expected_size;
    uint32_t total_chunks;
    uintmax_t bytes_received;
    std::vector<bool> received_chunks_mask;

    ReceivingFileInfo(const std::string& name, uintmax_t size, const std::string& save_dir);
    ReceivingFileInfo(ReceivingFileInfo&& other) noexcept;
    ReceivingFileInfo& operator=(ReceivingFileInfo&& other) noexcept;

    // 禁用拷贝
    ReceivingFileInfo(const ReceivingFileInfo&) = delete;
    ReceivingFileInfo& operator=(const ReceivingFileInfo&) = delete;

    ~ReceivingFileInfo() {
        if (file_stream.is_open()) {
            std::cout << "[Server] Closing file stream for: " << local_file_path << " in destructor." << std::endl;
            file_stream.close();
        }
    }
};