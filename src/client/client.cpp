#include "client.hpp"
#include "../util/encryption.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>

namespace fs = std::filesystem;

Client::Client(net::io_context& ioc_, std::string& id_, std::string& ip_, short port_,
               std::queue<json>& response_queue_)
    : ioc(ioc_), id(id_), ip(ip_), port(port_), client_socket(ioc), client_ep(net::ip::make_address(ip), port),
      client_resolver(ioc), response_queue(response_queue_) {}

net::awaitable<void> Client::sender(json msg, std::string ip) {
    try {
        msg["target_ip"] = ip;
        std::string data = msg.dump();
        // std::cout << data << std::endl;
        uint32_t data_len = static_cast<uint32_t>(data.size());
        // Convert length to network byte order before sending
        uint32_t net_data_len = htonl(data_len);
        auto endpoints = co_await client_resolver.async_resolve(ip, std::to_string(port), net::use_awaitable);
        co_await net::async_connect(client_socket, endpoints, net::use_awaitable);

        net::const_buffer buffer_to_send = net::buffer(data, data.size());

        co_await net::async_write(client_socket, net::buffer(&net_data_len, sizeof(net_data_len)), net::use_awaitable);
        co_await net::async_write(client_socket, buffer_to_send, net::use_awaitable);
    } catch (std::exception& e) {
        std::cout << "error on sender: " << e.what() << std::endl;
    }
    // std::cout << "send!!!!" << std::endl;
}

net::awaitable<void> Client::send_request(std::string target_ip, std::string pw) {
    json j;
    j["type"] = "verification_request";
    j["ip"] = ip;
    j["id"] = id;
    j["pw"] = pw;
    j["port"] = std::to_string(port);

    co_await sender(j, target_ip);
    co_return;
}

net::awaitable<void> Client::send_metadata(std::string target_ip, std::string file_path_str) {
    try {
        fs::path file_path(file_path_str);

        // 1. 验证文件路径
        if (!fs::exists(file_path)) {
            throw std::runtime_error("File not found: " + file_path_str);
        }
        if (!fs::is_regular_file(file_path)) {
            throw std::runtime_error("Path does not point to a regular file: " + file_path_str);
        }

        // 2. 获取文件名
        std::string file_name = file_path.filename().string();

        // 3. 获取文件大小
        uintmax_t file_size_val = 0;
        try {
            file_size_val = fs::file_size(file_path);
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Error getting file size for " + file_path_str + ": " + e.what());
        }

        json metadata_msg;
        metadata_msg["type"] = "file_metadata_basic";
        metadata_msg["file_name"] = file_name;
        metadata_msg["file_size"] = file_size_val;
        metadata_msg["ip"] = ip;

        std::cout << "[INFO] Prepared basic metadata for " << file_name << ":" << std::endl;
        std::cout << metadata_msg.dump(4) << std::endl;

        // 5. 调用 sender 函数
        co_await sender(metadata_msg, target_ip);

        std::cout << "[INFO] Basic metadata for " << file_name << " sent successfully to " << target_ip << "."
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to send basic metadata for file " << file_path_str << " to " << target_ip << ": "
                  << e.what() << std::endl;
    }
    co_return;
}

net::awaitable<void> Client::send_file_chunks(const std::string& target_ip, const std::string& file_path_str) {
    try {
        fs::path file_path(file_path_str);
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            throw std::runtime_error("File not found or not a regular file during chunk sending: " + file_path_str);
        }

        std::string file_name = file_path.filename().string();
        uintmax_t total_file_size = fs::file_size(file_path);

        if (total_file_size == 0) {
            std::cout << "[INFO] File " << file_name << " is empty. No chunks to send." << std::endl;
            // json empty_file_msg;
            // empty_file_msg["type"] = "file_empty_complete";
            // empty_file_msg["file_name"] = file_name;
            // co_await sender(empty_file_msg, target_ip);
            co_return;
        }

        std::ifstream file(file_path_str, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for sending chunks: " + file_path_str);
        }

        const std::size_t CHUNK_SIZE = 1024 * 1024; // 1MB
        std::vector<char> buffer(CHUNK_SIZE);
        uint32_t chunk_index = 0;
        uintmax_t bytes_sent = 0;

        uint32_t total_chunks = static_cast<uint32_t>((total_file_size + CHUNK_SIZE - 1) / CHUNK_SIZE);

        std::cout << "[INFO] Starting to send " << total_chunks << " chunks for file " << file_name
                  << " (total size: " << total_file_size << " bytes)." << std::endl;

        while (true) {
            file.read(buffer.data(), CHUNK_SIZE);
            std::streamsize bytes_read = file.gcount();

            if (bytes_read > 0) {
                std::vector<char> current_chunk_data(buffer.data(), buffer.data() + bytes_read);
                std::string encoded_data = base64_encode(current_chunk_data); // 使用内部函数

                json chunk_msg;
                chunk_msg["type"] = "file_chunk";
                chunk_msg["file_name"] = file_name;
                chunk_msg["chunk_index"] = chunk_index;
                chunk_msg["total_chunks"] = total_chunks;
                chunk_msg["data_b64"] = encoded_data;
                chunk_msg["size_original"] = static_cast<uint32_t>(bytes_read); // 原始二进制大小
                chunk_msg["is_last_chunk"] = (chunk_index == total_chunks - 1);
                if (chunk_index % 5 == 0) {
                    std::cout << "[INFO] Sending chunk " << chunk_index + 1 << "/" << total_chunks << " for "
                              << file_name << " (original size: " << bytes_read << " bytes)..." << std::endl;
                }
                co_await sender(chunk_msg, target_ip);

                bytes_sent += bytes_read;
                chunk_index++;

                if (bytes_sent == total_file_size) {
                    // 已经发送完所有数据
                    break;
                }
            } else {
                if (file.eof()) {
                    if (bytes_sent != total_file_size) {
                        throw std::runtime_error("EOF reached prematurely for file " + file_path_str + ". Expected " +
                                                 std::to_string(total_file_size) + " bytes, but only processed " +
                                                 std::to_string(bytes_sent) + " before EOF after reading 0 bytes.");
                    }
                    // 正常EOF，且所有字节已发送 (这种情况通常由 bytes_sent == total_file_size 在上面捕获)
                    break;
                } else {
                    // 文件读取错误
                    throw std::runtime_error("File read error on " + file_path_str + " after sending " +
                                             std::to_string(bytes_sent) + " bytes.");
                }
            }
        }

        if (bytes_sent == total_file_size) {
            std::cout << "[INFO] All " << total_chunks << " chunks for file " << file_name << " sent successfully."
                      << std::endl;
        } else {
            // 如果循环结束但字节数不匹配，这是一个逻辑问题或文件问题
            std::cerr << "[WARNING] Discrepancy after sending chunks for " << file_name << ". Sent " << bytes_sent
                      << ", expected " << total_file_size << std::endl;
        }
        // 文件流会自动关闭
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to send file chunks for " << file_path_str << " to " << target_ip << ": "
                  << e.what() << std::endl;
        throw;
    }
    co_return;
}

net::awaitable<void> Client::transfer_file(const std::string target_ip, const std::string file_path) {
    try {
        std::cout << "== Starting full file transfer for: " << file_path << " to " << target_ip << " ==" << std::endl;
        co_await send_metadata(target_ip, file_path);

        fs::path fpath(file_path);
        uintmax_t file_size = fs::file_size(fpath);

        if (file_size > 0) {
            co_await send_file_chunks(target_ip, file_path);
        } else {
            std::cout << "[INFO] File " << fpath.filename().string() << " is empty, skipping chunk sending phase."
                      << std::endl;
        }

        std::cout << "== Full file transfer for: " << file_path << " completed successfully. ==" << std::endl;
        // json transfer_complete_msg;
        // transfer_complete_msg["type"] = "file_transfer_status";
        // transfer_complete_msg["file_name"] = fpath.filename().string();
        // transfer_complete_msg["status"] = "completed";
        // transfer_complete_msg["total_size"] = file_size;
        // co_await client.sender(transfer_complete_msg, target_ip);

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Full file transfer for " << file_path << " failed: " << e.what() << std::endl;
        // 可以在这里发送一个总的“传输失败”消息
        // json transfer_failed_msg;
        // ...
        // co_await client.sender(transfer_failed_msg, target_ip);
        throw;
    }
    co_return;
}

net::awaitable<void> Client::handle_request() {
    net::steady_timer timer(client_socket.get_executor());
    while (true) {
        timer.expires_after(std::chrono::seconds(3));
        co_await timer.async_wait(net::use_awaitable);
        if (!response_queue.empty()) {
            json top = response_queue.front();
            std::string tar_ip = top["target_ip"];
            net::co_spawn(ioc, sender(top, tar_ip), net::detached);
            response_queue.pop();
        }
    }
}