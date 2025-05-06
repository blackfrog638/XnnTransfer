#pragma once

#include <fstream>
#include <memory>

#include <boost/asio.hpp>
#include <boost/beast/websocket.hpp>


namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;

class AsyncContext {
    public:
        websocket::stream<net::ip::tcp::socket> &ws;
        std::shared_ptr<std::ifstream> file_stream;
        std::shared_ptr<std::array<char, 8192>> buffer;
        bool is_first = true;
    
        AsyncContext(websocket::stream<net::ip::tcp::socket> &ws, std::ifstream file);
};


