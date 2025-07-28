#include <array>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace net = boost::asio;
using namespace nlohmann;

class Broadcast {
  public:
    Broadcast(net::io_context& ioc_, std::string& id_, std::string& ip_, short port_);
    net::awaitable<void> broadcaster();
    net::awaitable<void> listener();
    net::awaitable<void> main_handler();
    net::awaitable<void> list_handler();
    std::string find_user(std::string& id);

  private:
    std::string& id;
    net::io_context& ioc;
    std::string& ip;
    short port;

    net::ip::udp::socket broadcast_socket;
    net::ip::udp::socket listen_socket;
    std::vector<std::array<std::string, 3>> receiver_list;
    std::vector<std::array<std::string, 3>> temp;
};