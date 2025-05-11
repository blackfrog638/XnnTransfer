#include "transfer_manager.hpp"

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

TransferManager::TransferManager(net::io_context& io)
    : io(io),
      resolver(io),
      ws_(net::make_strand(io)),
      file_chunk_buffer(8192), // 8KB buffer
      is_final_chunk_(false) {}

void TransferManager::run(const std::string& host, const std::string& port, const std::string &file_path) {
    host_ = host;
    port_ = static_cast<short>(std::stoi(port));
    file_path_ = file_path;

    beast::error_code ec;
    file.open(file_path_.c_str(), net::file_base::read_only, ec);
    if(ec) {
        handle_error(ec, "Failed to open file");
        return;
    }
    file_size_ = file.size(ec);
    if(ec) {
        handle_error(ec, "Failed to get file size");
        return;
    }
    if(file_size_ == 0) {
        handle_error(ec, "File is empty");
        return;
    }
    total_bytes_sent_ = 0;
    resolver.async_resolve(host_, std::to_string(port_), 
        beast::bind_front_handler(&TransferManager::on_resolve, shared_from_this()));
}

void TransferManager::on_resolve(beast::error_code ec, 
                                  net::ip::tcp::resolver::results_type results) {
    if(ec) {
        handle_error(ec, "Resolve failed");
        return;
    }
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    beast::get_lowest_layer(ws_).async_connect(results,
        beast::bind_front_handler(&TransferManager::on_connect, shared_from_this()));
}

void TransferManager::on_handshake(beast::error_code ec) {
    if(ec) {
        handle_error(ec, "Handshake failed");
        return;
    }
    send_metadata();
}

void TransferManager::send_metadata() {
    json metadata_json;
    metadata_json["file_name"] = std::filesystem::path(file_path_).filename().string();
    metadata_json["file_size"] = file_size_;
    metadata_serialized_ = metadata_json.dump();

    ws_.text(true);
    ws_.async_write(net::buffer(metadata_serialized_),
        beast::bind_front_handler(&TransferManager::on_metadata_send, shared_from_this()));
}

void TransferManager::on_metadata_send(beast::error_code ec,
    std::size_t bytes_transferred){
    if(ec){
        handle_error(ec, "Metadata sed failed");
        return;
    }
    read_next_chunk();
}

void TransferManager::read_next_chunk() {
    beast::error_code ec;
    file.async_read_some(net::buffer(file_chunk_buffer));
    beast::bind_front_handler(&TransferManager::on_chunk_read, shared_from_this());
    if(ec){
        handle_error(ec, "Failed to read next file chunk");
        return;
    }
}

void TransferManager::on_chunk_read(beast::error_code ec,
    std::size_t bytes_transferred){
    if(ec == net::error::eof){
        if(bytes_transferred == 0 && total_bytes_sent_ == file_size_){
            close();
            return;
        }
        if(total_bytes_sent_ == file_size_){
            is_final_chunk_ = true;
        }
    }
    if(ec){
        handle_error(ec, "Failed to read file chunk");
        return;
    }
    if(bytes_transferred > 0){
        write_next_chunk(bytes_transferred);
    }
}

void TransferManager::write_next_chunk(std::size_t bytes_read) {
    total_bytes_sent_ += bytes_read;
    is_final_chunk_ = (total_bytes_sent_ == file_size_);
    ws_.auto_fragment(false);
    if(is_final_chunk_){
        ws_.async_write(net::buffer(file_chunk_buffer.data(), bytes_read),
            beast::bind_front_handler(&TransferManager::on_chunk_send, shared_from_this()));   
    }
    else{
        ws_.async_write_some(false, net::buffer(file_chunk_buffer.data(), bytes_read),
            beast::bind_front_handler(&TransferManager::on_chunk_send, shared_from_this()));
    }
}    

void TransferManager::on_chunk_send(beast::error_code ec,
    std::size_t bytes_transferred){
    if(ec){
        handle_error(ec, "Failed to send file chunk");
        return;
    }
    if(total_bytes_sent_ < file_size_){
        read_next_chunk();
    }
    if(total_bytes_sent_ == file_size_){
        close();
    }
}

void TransferManager::on_close(beast::error_code ec) {
    if(ec) {
        handle_error(ec, "Close failed");
        return;
    }
    std::cout << "Connection closed" << std::endl;
    file.close();
} 
    

void TransferManager::close() {
    beast::error_code ec;
    ws_.async_close(beast::websocket::close_code::normal,
        beast::bind_front_handler(&TransferManager::on_close, shared_from_this()));
}

void TransferManager::handle_error(const beast::error_code& ec, const std::string& message) {
    std::cerr << message << ": " << ec.message() << std::endl;
    close();
    beast::error_code close_ec;
    if (ws_.is_open())
        ws_.close(beast::websocket::close_code::internal_error, close_ec); // 同步关闭以简化
    if (file.is_open())
        file.close(close_ec);
}