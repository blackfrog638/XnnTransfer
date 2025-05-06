#include "async_recv.hpp"
void async_receive(websocket::stream<net::ip::tcp::socket>& ws,
                   const std::string& file_path) {
    std::ofstream file(file_path, std::ios::binary);
    if(!file) throw std::runtime_error("Cannit create file");

    auto ctx = std::make_shared<ReceiveContext>(ws, std::move(file));

    std::function<void(error_code)> receive_next = [&](error_code ec) {
        if(ec) return;

        ctx->ws.async_read(
            ctx->buffer,
            [ctx, receive_next] (error_code ec, size_t bytes_read){
                if(ec) return receive_next(ec);

                ctx->file->write(static_cast<const char*>(ctx->buffer.data().data()),
                    bytes_read
                );
                ctx->buffer.consume(bytes_read);

                if(!ctx->ws.is_message_done()) {
                    receive_next(ec);
                }
            }
        );
    };
}