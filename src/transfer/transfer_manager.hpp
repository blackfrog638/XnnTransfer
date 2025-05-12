#pragma once

#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <vector>

namespace net = boost::asio;
namespace beast = boost::beast;
class TransferManager : public std::enable_shared_from_this<TransferManager>{
public:
    explicit TransferManager(net::io_context& io);

    void run(const std::string& host, short port, const std::string &file_path);

private:
    void on_resolve(beast::error_code ec, 
                    net::ip::tcp::resolver::results_type results);
    void on_connect(beast::error_code ec,
                    net::ip::tcp::resolver::results_type::endpoint_type ep);
    void on_handshake(beast::error_code ec);
    void on_metadata_send(beast::error_code ec, std::size_t bytes_transferred);
    void on_chunk_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_chunk_send(beast::error_code ec, std::size_t bytes_transferred);
    void on_close(beast::error_code ec);

    void send_metadata();
    void read_next_chunk();
    void write_next_chunk(std::size_t bytes_read);
    void close();
    void handle_error(const beast::error_code& ec, const std::string& message);

    net::io_context& io;
    net::ip::tcp::resolver resolver;
    beast::websocket::stream<beast::tcp_stream> ws_; 
    beast::flat_buffer beast_buffer_;

    std::string host_;
    short port_;
    std::string file_path_;

    net::stream_file file {io};
    std::vector<char> file_chunk_buffer;
    std::size_t file_size_;
    std::size_t total_bytes_sent_;

    std::string metadata_serialized_;
    bool is_final_chunk_;
};