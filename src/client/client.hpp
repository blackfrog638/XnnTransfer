#include <boost/asio/awaitable.hpp>
#include <queue>
#include <string>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
using namespace nlohmann;

class Client {
  public:
    Client(net::io_context& ioc_, std::string& id_, std::string& ip_, short port_,
           std::queue<std::string>& response_queue_);

    net::awaitable<void> send_request(std::string target_ip, std::string pw);
    net::awaitable<void> send_response(std::string target_ip, bool succeed);
    net::awaitable<void> transfer_file(const std::string target_ip, const std::string file_path);
    net::awaitable<void> handle_request();

  private:
    std::string& id;
    net::io_context& ioc;
    std::string& ip;
    short port;
    std::queue<std::string>& response_queue;

    net::ip::tcp::socket client_socket;
    net::ip::tcp::endpoint client_ep;
    net::ip::tcp::resolver client_resolver;

    net::awaitable<void> sender(json msg, std::string ip);
    net::awaitable<void> send_metadata(std::string target_ip, std::string file_path_str);
    net::awaitable<void> send_file_chunks(const std::string& target_ip, const std::string& file_path_str);
};