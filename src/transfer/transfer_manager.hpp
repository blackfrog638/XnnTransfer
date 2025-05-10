#include <boost/asio/io_context.hpp>
#include <boost/beast/core/error.hpp>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
class TransferManager : public std::enable_shared_from_this<TransferManager>{
public:
    explicit TransferManager(net::io_context& io, const std::string& target_ip, short port);

    void run(const std::string &file_path);

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
    
};