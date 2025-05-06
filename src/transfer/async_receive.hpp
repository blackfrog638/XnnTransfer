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

    ReceiveContext(websocket::stream<net::ip::tcp::socket>& ws, std::ofstream &&file);
};