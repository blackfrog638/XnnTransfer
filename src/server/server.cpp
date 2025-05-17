#include "server.hpp"

#include <algorithm>
#include <array>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstddef>
#include <exception>
#include <iostream>

#include <boost/asio/awaitable.hpp>

Server::Server(net::io_context& ioc_, std::string& id_, std::string& ip_, std::string& pw_, short port_,
               std::vector<std::string>& whitelist_, std::queue<std::string>& response_queue_)
    : ioc(ioc_), id(id_), ip(ip_), pw(pw_), port(port_), server_socket(ioc), server_ep(net::ip::make_address(ip), port),
      whitelist(whitelist_), response_queue(response_queue_) {}

net::awaitable<void> Server::session_handler(net::ip::tcp::socket current_socket) {
    try {
        while (current_socket.is_open()) { // Loop to handle multiple messages on the same connection
            // 1. Read the length of the incoming JSON data
            uint32_t net_data_len;
            std::size_t n = co_await net::async_read(current_socket, net::buffer(&net_data_len, sizeof(net_data_len)),
                                                     net::use_awaitable);

            if (n == 0) { // Connection closed by peer (EOF)
                // std::cout << "Connection closed by peer while waiting for length." << std::endl;
                break; // Exit session handler
            }
            if (n != sizeof(net_data_len)) {
                std::cerr << "Error: Did not read full length header. Read " << n << " bytes." << std::endl;
                break;
            }

            uint32_t data_len = ntohl(net_data_len); // network to host long

            // Sanity check for data length to prevent excessively large allocations
            const size_t MAX_ALLOWED_JSON_SIZE = 10 * 1024 * 1024; // e.g., 10MB
            if (data_len == 0) {
                std::cerr << "Error: Received data length is 0. Closing connection." << std::endl;
                break; // Or handle as an empty message if protocol allows
            }
            if (data_len > MAX_ALLOWED_JSON_SIZE) {
                std::cerr << "Error: Requested data length " << data_len << " exceeds maximum allowed size "
                          << MAX_ALLOWED_JSON_SIZE << ". Closing connection." << std::endl;
                // Optionally, you might try to read and discard the oversized message
                // to clean the socket, but closing is safer.
                break;
            }

            // 2. Read the actual JSON data
            std::vector<char> body_buffer(data_len);
            n = co_await net::async_read(current_socket, net::buffer(body_buffer), net::use_awaitable);

            if (n == 0 && data_len > 0) { // Connection closed by peer (EOF)
                std::cerr << "Connection closed by peer while reading message body." << std::endl;
                break; // Exit session handler
            }
            if (n != data_len) {
                std::cerr << "Error: Did not read full JSON body. Expected " << data_len << ", got " << n << std::endl;
                break;
            }

            std::string data(body_buffer.data(), body_buffer.size());
            json msg = json::parse(data); // Now this should be a complete JSON
            std::string type = msg["type"];

            // std::cout << "Received complete message of type: " << type << ", size: " << data_len << std::endl;

            if (type == "verification_request") {
                verify_request(msg);
            } else if (type == "verification_response") {
                verify_response(msg);
            } else if (type == "file_metadata_basic") {
                // If handle_metadata needs to use the socket, it should accept it.
                // Be careful with socket ownership if spawning detached coroutines.
                // If the spawned coroutine takes a long time and this session_handler exits,
                // the socket might be closed prematurely if not managed carefully.
                // One way: pass std::move(current_socket) if the handler is the new owner.
                // But then this loop cannot continue for this socket.
                // For now, assume handlers are quick or don't need the socket further for this example.
                net::co_spawn(ioc, handle_metadata(msg), net::detached);
            } else if (type == "file_chunk") {
                net::co_spawn(ioc, handle_file_chunk(msg), net::detached);
            } else {
                std::cerr << "Unknown message type: " << type << std::endl;
            }
            // If you only expect one message per connection, you would break here or close the socket.
            // For multiple messages, the loop continues.
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error in session: " << e.what() << std::endl;
        // Potentially log the received data that failed to parse
    } catch (const boost::system::system_error& e) {
        if (e.code() == net::error::eof || e.code() == net::error::connection_reset ||
            e.code() == net::error::broken_pipe) {
            // std::cout << "Connection closed or reset by peer: " << e.what() << std::endl;
        } else {
            std::cerr << "Network error in session: " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in session_handler: " << e.what() << std::endl;
    }
    // Ensure socket is closed when session ends or on error
    if (current_socket.is_open()) {
        boost::system::error_code ec;
        current_socket.shutdown(net::ip::tcp::socket::shutdown_both, ec);
        current_socket.close(ec);
    }
    // std::cout << "Session ended." << std::endl;
    co_return;
}

net::awaitable<void> Server::receiver() {
    try {
        const size_t buffer_size = 3 * 1024 * 1024;
        std::vector<char> buffer(buffer_size);
        net::ip::tcp::endpoint ep(net::ip::address_v6::any(), port);
        net::ip::tcp::acceptor acceptor(ioc, ep);

        acceptor.set_option(net::socket_base::reuse_address(true));
        acceptor.listen();
        while (true) {
            server_socket = co_await acceptor.async_accept(net::use_awaitable);
            // std::cout << "Accepted new connection from: " << current_connection_socket.remote_endpoint() <<
            // std::endl;

            // Spawn a new coroutine to handle this client's session.
            // This allows the server to accept other connections concurrently.
            net::co_spawn(ioc, session_handler(std::move(server_socket)), net::detached);
        }

    } catch (const std::exception& e) {
        std::cout << "Error receiving messages " << e.what() << std::endl;
    }
    co_return;
}

void Server::verify_request(json j) {
    try {
        std::string ver_pw = j["pw"];
        std::string ver_ip = j["ip"];
        if (ver_pw == pw) {
            bool flag = false;
            for (auto& i : whitelist) {
                if (i == ver_ip) {
                    flag = true;
                    break;
                }
            }
            if (!flag) {
                whitelist.push_back(ver_ip);
                response_queue.push(ver_ip);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing verify_request" << e.what();
    }
}

void Server::verify_response(json j) {
    try {
        std::string ver_id = j["id"];
        std::string status = j["status"];
        if (status == "passed") {
            std::cout << "verification succeed for " << ver_id << std::endl;
        } else {
            std::cout << "verification for " << ver_id << " rejected" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing verify_response" << e.what();
    }
}

net::awaitable<void> Server::handle_metadata(const json& j) {
    try {
        std::string file_name = j.at("file_name").get<std::string>();
        uintmax_t file_size = j.at("file_size").get<uintmax_t>();

        std::string raw_ip = j["ip"];
        bool b = false;
        for (auto& i : whitelist) {
            if (i == raw_ip) {
                b = true;
            }
        }
        if (!b) {
            std::cout << "[Server] ip rejected: " << raw_ip << std::endl;
            co_return;
        }
        std::cout << "[Server] Received metadata for: " << file_name << ", Size: " << file_size << " bytes."
                  << std::endl;

        // 如果已存在同名文件传输，则覆盖（或根据策略处理）
        if (active_transfers_.count(file_name)) {
            std::cout << "[Server] Overwriting previous transfer session for: " << file_name << std::endl;
            active_transfers_.erase(file_name);
        }

        // 创建新的接收文件信息
        // piece_wise_construct 允许直接在 map 中构造对象，避免不必要的拷贝/移动
        active_transfers_.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(file_name), // key for map
                                  std::forward_as_tuple(file_name, file_size, save_directory_));

        // 对于空文件，元数据接收后即认为完成
        if (file_size == 0) {
            std::cout << "[Server] File " << file_name << " is empty. Finalizing immediately." << std::endl;
            auto it = active_transfers_.find(file_name);
            if (it != active_transfers_.end()) {
                finalize_file_transfer(file_name, it, true); // true for empty file
            }
        }

    } catch (const json::exception& e) {
        std::cerr << "[Server] JSON parsing error in metadata: " << e.what() << ". JSON: " << j.dump(2) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Server] Error handling metadata: " << e.what() << ". JSON: " << j.dump(2) << std::endl;
    }
    co_return;
}

net::awaitable<void> Server::handle_file_chunk(const json& j) {
    std::string file_name;
    try {
        file_name = j.at("file_name").get<std::string>();
        uint32_t chunk_index = j.at("chunk_index").get<uint32_t>();
        uint32_t total_chunks_from_client = j.at("total_chunks").get<uint32_t>();
        std::string encoded_data = j.at("data_b64").get<std::string>();
        uint32_t original_size = j.at("size_original").get<uint32_t>();

        auto it = active_transfers_.find(file_name);
        if (it == active_transfers_.end()) {
            std::cerr << "[Server] Received chunk for untracked file: " << file_name << ". Discarding." << std::endl;
            co_return;
        }

        ReceivingFileInfo& transfer_info = it->second;

        if (transfer_info.expected_size > 0 && transfer_info.total_chunks == 0) {
            transfer_info.total_chunks = total_chunks_from_client;
            if (transfer_info.total_chunks > 0) {
                transfer_info.received_chunks_mask.assign(transfer_info.total_chunks, false);
                std::cout << "[Server] Initialized " << file_name << " to expect " << transfer_info.total_chunks
                          << " chunks." << std::endl;
            } else if (transfer_info.expected_size > 0 && transfer_info.total_chunks == 0) {
                std::cerr << "[Server] Error: File " << file_name << " has size " << transfer_info.expected_size
                          << " but client reported total_chunks as 0. Discarding chunk." << std::endl;
                active_transfers_.erase(it);
                co_return;
            }
        } else if (transfer_info.total_chunks != total_chunks_from_client && transfer_info.expected_size > 0) {
            std::cerr << "[Server] Warning: Mismatch in total_chunks for " << file_name
                      << ". Server had: " << transfer_info.total_chunks << ", Client sent: " << total_chunks_from_client
                      << ". Correcting to client's value." << std::endl;
            transfer_info.total_chunks = total_chunks_from_client;
            if (transfer_info.total_chunks > 0) {
                transfer_info.received_chunks_mask.assign(transfer_info.total_chunks, false);
            } else if (transfer_info.expected_size > 0 && transfer_info.total_chunks == 0) {
                std::cerr << "[Server] Error: File " << file_name << " has size " << transfer_info.expected_size
                          << " but client (after correction) reported total_chunks as 0. Discarding chunk."
                          << std::endl;
                active_transfers_.erase(it);
                co_return;
            }
        }

        if (chunk_index >= transfer_info.total_chunks && transfer_info.total_chunks > 0) {
            std::cerr << "[Server] Chunk index " << chunk_index << " out of bounds for " << file_name
                      << " (total: " << transfer_info.total_chunks << "). Discarding." << std::endl;
            co_return;
        }
        if (transfer_info.total_chunks == 0 && transfer_info.expected_size > 0) {
            std::cerr << "[Server] Logical error: File " << file_name
                      << " expects data but total_chunks is 0. Discarding." << std::endl;
            active_transfers_.erase(it);
            co_return;
        }

        if (transfer_info.total_chunks > 0 && transfer_info.received_chunks_mask[chunk_index]) {
            std::cout << "[Server] Duplicate chunk " << chunk_index << " for " << file_name << ". Ignoring."
                      << std::endl;
            co_return;
        }

        std::vector<char> decoded_chunk = base64_decode(encoded_data);
        if (decoded_chunk.size() != original_size) {
            std::cerr << "[Server] Decoded chunk size (" << decoded_chunk.size() << ") != original_size ("
                      << original_size << ") for " << file_name << " chunk " << chunk_index << ". Discarding."
                      << std::endl;
            co_return;
        }

        if (!transfer_info.file_stream.is_open()) {
            std::cerr << "[Server] File stream for " << file_name << " is not open. Discarding chunk " << chunk_index
                      << std::endl;
            active_transfers_.erase(it); // 清理
            co_return;
        }

        // 定位并写入块数据
        uintmax_t seek_pos = static_cast<uintmax_t>(chunk_index) * CHUNK_SIZE;
        transfer_info.file_stream.seekp(seek_pos);
        if (!transfer_info.file_stream) {
            std::cerr << "[Server] Seek failed for " << file_name << " chunk " << chunk_index << " at pos " << seek_pos
                      << ". Discarding." << std::endl;
            transfer_info.file_stream.clear(); // 清除错误状态
            active_transfers_.erase(it);       // 清理
            co_return;
        }
        transfer_info.file_stream.write(decoded_chunk.data(), decoded_chunk.size());
        if (!transfer_info.file_stream) {
            std::cerr << "[Server] Write failed for " << file_name << " chunk " << chunk_index << ". Discarding."
                      << std::endl;
            transfer_info.file_stream.clear(); // 清除错误状态
            active_transfers_.erase(it);       // 清理
            co_return;
        }

        transfer_info.bytes_received += decoded_chunk.size();
        if (transfer_info.total_chunks > 0) {
            transfer_info.received_chunks_mask[chunk_index] = true;
        }
        if (chunk_index % 5 == 0) {
            std::cout << "[Server] Wrote chunk " << chunk_index << " for " << file_name
                      << " (size: " << decoded_chunk.size() << " B). Total received: " << transfer_info.bytes_received
                      << "/" << transfer_info.expected_size << " B." << std::endl;
        }

        bool all_chunks_accounted_for = false;
        if (transfer_info.total_chunks > 0) {
            all_chunks_accounted_for = std::all_of(transfer_info.received_chunks_mask.begin(),
                                                   transfer_info.received_chunks_mask.end(), [](bool b) { return b; });
        } else if (transfer_info.expected_size == 0) {
            all_chunks_accounted_for = true;
        }

        if (all_chunks_accounted_for) {
            std::cout << "[Server] All " << transfer_info.total_chunks << " chunks received for " << file_name << "."
                      << std::endl;
            finalize_file_transfer(file_name, it, false);
        }

    } catch (const json::exception& e) {
        std::cerr << "[Server] JSON parsing error in file_chunk for '" << file_name << "': " << e.what()
                  << ". JSON: " << j.dump(2) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Server] Error handling file_chunk for '" << file_name << "': " << e.what()
                  << ". JSON: " << j.dump(2) << std::endl;

        if (file_name != "N/A" && active_transfers_.count(file_name)) {
            active_transfers_.erase(file_name);
        }
    }
    co_return;
}

void Server::finalize_file_transfer(const std::string& file_name_key,
                                    std::map<std::string, ReceivingFileInfo>::iterator& it, bool is_empty_file) {
    ReceivingFileInfo& info = it->second;

    if (info.file_stream.is_open()) {
        info.file_stream.close(); // 确保文件流关闭并刷新
        std::cout << "[Server] Finalized and closed file stream for: " << info.local_file_path << std::endl;
    }

    if (!is_empty_file && info.bytes_received != info.expected_size) {
        std::cerr << "[Server] Size mismatch for " << info.original_file_name << "! Expected: " << info.expected_size
                  << ", Received: " << info.bytes_received << ". File: " << info.local_file_path << std::endl;
    } else {
        uintmax_t disk_size = 0;
        try {
            disk_size = fs::file_size(info.local_file_path);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[Server] Could not get file size for " << info.local_file_path << ": " << e.what()
                      << std::endl;
        }
        if (disk_size == info.expected_size) {
            std::cout << "[Server] File " << info.original_file_name << " successfully received as "
                      << info.local_file_path << " (Size: " << disk_size << " bytes)." << std::endl;
        } else {
            std::cerr << "[Server] On-disk size mismatch for " << info.original_file_name
                      << "! Expected: " << info.expected_size << ", On disk: " << disk_size
                      << ". File: " << info.local_file_path << std::endl;
        }
    }
    active_transfers_.erase(it);
}