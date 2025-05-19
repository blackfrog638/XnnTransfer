#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>

#include "client/client.hpp"
#include "discovery/broadcast.hpp"
#include "server/server.hpp"

namespace net = boost::asio;
using namespace nlohmann;

class Manager {
  public:
    explicit Manager(net::io_context& ioc_);

    net::awaitable<void> run();

    std::string id;
    std::string pw;
    std::string ip;
    short port;

    net::io_context& ioc;

  private:
    static json read_json_profile();
    std::string get_local_ip(net::io_context& ioc_);
    void console_input_loop();

    Broadcast bc;
    Server server;
    Client client;
    std::queue<json> response_queue;
    std::vector<std::string> whitelist;
};