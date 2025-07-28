#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <core/manager.hpp>

namespace net = boost::asio;
using tcp = net::ip::tcp;

const short PORT = 8888;

// net::awaitable<void> handle_client(tcp::socket socket) {}

// net::awaitable<void> async_server() {
//   net::io_context ioc{static_cast<int>(std::thread::hardware_concurrency())};
//   net::ip::tcp::endpoint ep(net::ip::address_v6::any(), 9999);
//   net::ip::tcp::acceptor acceptor(ioc, ep);
//   acceptor.set_option(net::socket_base::reuse_address(true));
//   acceptor.listen();

//   for (;;) {
//     try {
//       auto clientSocket = co_await acceptor.async_accept();
//       net::co_spawn(ioc, handle_client(std::move(clientSocket)), net::detached);
//     } catch (std::exception e) {
//     }
//   }
// }

int main() {
    // std::cout<<"Starting broadcast sender and receiver..."<<std::endl;
    // FilesenderManager fm(PORT);
    // std::cout<<"Login successful!"<<std::endl;
    // std::thread broadcast_thread([&fm]() { fm.run_broadcast(); });
    // std::thread verification_thread([&fm]() { fm.verifying(); });
    // std::cout<<"Please Enter the target username..."<<std::endl;
    // std::cin.get();
    // broadcast_thread.join();
    // verification_thread.join();
    net::io_context ioc{static_cast<int>(std::thread::hardware_concurrency())};

    // ioc.run();
    Manager manager(ioc);
    // std::cout << manager.ip << std::endl;
    net::co_spawn(manager.ioc, manager.run(), net::detached);
    ioc.run();
    return 0;
}