#include "async_send.hpp"

#include <iostream>
#include <functional>
#include <fstream>
#include <memory>
#include <ostream>
#include <stdexcept>

AsyncContext::AsyncContext(websocket::stream<net::ip::tcp::socket> &ws,
                             std::ifstream file) : 
    ws(ws),
    file_stream(std::make_shared<std::ifstream>(std::move(file))),
    buffer(std::make_shared<std::array<char, 8192>>())
{}

void async_send(websocket::stream<net::ip::tcp::socket> &ws,
                const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if(!file) throw std::runtime_error("Failed to open file: " + file_path);
    file.seekg(0);

    auto ctx = std::make_shared<AsyncContext>(ws, std::move(file));
    std::function<void(std::error_code)> send_next = [&](std::error_code ec) {
        if(ec) {
            std::cerr << "Error sending file: " << ec.message() << std::endl;
            return;
        }
        
        ctx->file_stream->read(ctx->buffer->data(), ctx->buffer->size());
        std::size_t bytes_read = ctx->file_stream->gcount();
        if(bytes_read == 0) {
            if(ctx -> file_stream -> eof())return;
            else throw std::runtime_error("Error reading file");
        }

        bool is_fin = ctx->file_stream->eof();

        ctx->ws.async_write(
            net::buffer(ctx->buffer->data(), bytes_read),
            [ctx, send_next, is_fin](std::error_code ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    send_next(ec); // 传递错误
                    return;
                }
                ctx->is_first = false;
                if (!is_fin) {
                    send_next(ec); // 继续发送下一块
                }
            }
        );
    };

}