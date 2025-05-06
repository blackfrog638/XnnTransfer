// #ifndef BLOCK_TRANS_HPP
// #define BLOCK_TRANS_HPP

// #include <boost/asio.hpp>
// //#include <boost/crc.hpp>
// #include <filesystem>
// #include <fstream>
// #include <vector>

// namespace fs = std::filesystem;
// using namespace boost::asio;
// using ip::tcp;

// class BlockTrans {
// public:
//     static const size_t BLOCK_SIZE = 1024 * 1024; // 1MB分块

//     // 客户端发送文件
//     static bool send_file(const std::string& file_path, const std::string& server_ip, unsigned short port) {
//         try {
//             if (!fs::exists(file_path)) {
//                 throw std::runtime_error("文件不存在: " + file_path);
//             }

//             io_context io;
//             tcp::socket socket(io);
//             socket.connect(tcp::endpoint(ip::address::from_string(server_ip), port));

//             // 1. 发送文件头信息
//             std::string filename = fs::path(file_path).filename().string();
//             uint32_t name_len = htonl(filename.size());
//             uint64_t file_size = fs::file_size(file_path);
//             uint32_t total_blocks = htonl((file_size + BLOCK_SIZE - 1) / BLOCK_SIZE);

//             write(socket, buffer(&name_len, 4));
//             write(socket, buffer(filename));
//             write(socket, buffer(&file_size, 8));
//             write(socket, buffer(&total_blocks, 4));

//             // 2. 分块发送文件内容
//             std::ifstream file(file_path, std::ios::binary);
//             char data_buffer[BLOCK_SIZE];
//             uint32_t block_id = 0;
//             size_t total_blocks_num = ntohl(total_blocks);

//             while (!file.eof()) {
//                 file.read(data_buffer, BLOCK_SIZE);
//                 size_t bytes_read = file.gcount();

//                 // 计算当前块的CRC
//                 boost::crc_32_type crc;
//                 crc.process_bytes(data_buffer, bytes_read);
//                 uint32_t block_crc = htonl(crc.checksum());

//                 // 发送块头信息
//                 uint32_t net_block_id = htonl(block_id);
//                 uint32_t net_block_size = htonl(bytes_read);
//                 write(socket, buffer(&net_block_id, 4));
//                 write(socket, buffer(&net_block_size, 4));

//                 // 发送块数据+CRC
//                 write(socket, buffer(data_buffer, bytes_read));
//                 write(socket, buffer(&block_crc, 4));

//                 block_id++;
//             }

//             // 3. 发送结束标记
//             uint32_t end_marker = htonl(0xFFFFFFFF);
//             write(socket, buffer(&end_marker, 4));

//             return true;
//         } catch (const std::exception& e) {
//             throw std::runtime_error("[客户端] 错误: " + std::string(e.what()));
//         }
//     }

//     // 服务端接收文件
//     static void receive_file(tcp::socket& socket) {
//         try {
//             // 1. 接收文件头信息
//             uint32_t name_len;
//             read(socket, buffer(&name_len, 4));
//             name_len = ntohl(name_len);

//             std::vector<char> filename_buf(name_len);
//             read(socket, buffer(filename_buf));
//             std::string filename(filename_buf.begin(), filename_buf.end());

//             uint64_t file_size;
//             uint32_t total_blocks;
//             read(socket, buffer(&file_size, 8));
//             read(socket, buffer(&total_blocks, 4));
//             total_blocks = ntohl(total_blocks);

//             // 2. 准备接收文件
//             fs::path save_path = fs::path("received") / filename;
//             fs::create_directories(save_path.parent_path());
            
//             std::ofstream output(save_path, std::ios::binary);
//             if (!output) throw std::runtime_error("无法创建文件");

//             // 3. 接收数据块
//             for (uint32_t received_count = 0; received_count < total_blocks; ++received_count) {
//                 uint32_t block_id, block_size;
//                 read(socket, buffer(&block_id, 4));
//                 read(socket, buffer(&block_size, 4));
//                 block_id = ntohl(block_id);
//                 block_size = ntohl(block_size);

//                 std::vector<char> block_data(block_size);
//                 read(socket, buffer(block_data));

//                 // 校验CRC
//                 uint32_t client_crc;
//                 read(socket, buffer(&client_crc, 4));
//                 client_crc = ntohl(client_crc);

//                 boost::crc_32_type crc;
//                 crc.process_bytes(block_data.data(), block_size);
//                 if (crc.checksum() != client_crc) {
//                     throw std::runtime_error("块 " + std::to_string(block_id) + " CRC校验失败");
//                 }

//                 // 写入文件
//                 output.seekp(block_id * BLOCK_SIZE);
//                 output.write(block_data.data(), block_size);
//             }

//             // 4. 验证结束标记
//             uint32_t end_marker;
//             read(socket, buffer(&end_marker, 4));
//             if (ntohl(end_marker) != 0xFFFFFFFF) {
//                 throw std::runtime_error("无效的结束标记");
//             }
//         } catch (const std::exception& e) {
//             throw std::runtime_error("[服务端] 传输错误: " + std::string(e.what()));
//         }
//     }
// };

// #endif // BLOCK_TRANS_HPP
