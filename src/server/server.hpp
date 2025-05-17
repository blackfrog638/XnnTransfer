#include "../util/encryption.hpp"
#include "receiving_file_info.hpp"

#include <queue>
#include <string>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <vector>

namespace net = boost::asio;
using namespace nlohmann;

class Server {
  public:
    Server(net::io_context& ioc_, std::string& id_, std::string& ip_, std::string& pw_, short port_,
           std::vector<std::string>& whitelist_, std::queue<std::string>& response_queue_);

    net::awaitable<void> receiver();
    void verify_request(json j);
    void verify_response(json j);

    net::awaitable<void> handle_metadata(const json& j);
    net::awaitable<void> handle_file_chunk(const json& j);
    void finalize_file_transfer(const std::string& file_name_key,
                                std::map<std::string, ReceivingFileInfo>::iterator& it, bool is_empty_file);
    std::vector<std::string>& whitelist;

  private:
    const std::size_t CHUNK_SIZE = 1024 * 8;
    std::string save_directory_ = "received_files";

    std::map<std::string, ReceivingFileInfo> active_transfers_;
    std::string& id;
    net::io_context& ioc;
    std::string& ip;
    std::string& pw;
    short port;
    std::queue<std::string>& response_queue;

    net::ip::tcp::socket server_socket;
    net::ip::tcp::endpoint server_ep;
};