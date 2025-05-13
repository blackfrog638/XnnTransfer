#include <memory>
#include <fstream>

#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using error_code = boost::system::error_code;

class ReceiveContext{
    public:
    websocket::stream<net::ip::tcp::socket>& ws;
    std::shared_ptr<std::ofstream> file;
    beast::flat_buffer buffer;

    ReceiveContext(websocket::stream<net::ip::tcp::socket>& ws,
        std::ofstream file)
    : ws(ws),
    file(std::make_shared<std::ofstream>(std::move(file)))
    {}
};

inline void async_receive(websocket::stream<net::ip::tcp::socket>& ws,
                   const std::string& file_path) {
    std::ofstream file(file_path, std::ios::binary);
    if(!file) throw std::runtime_error("Cannot create file");

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