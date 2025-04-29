#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>

namespace net = boost::asio;

class BroadcastManager {
    public:
        std::atomic<bool> stop_flag{false};

        void broadcast_sender(const short port)const{
            try {
                net::io_context io;
                net::ip::udp::socket socket(io, net::ip::udp::v4());
                socket.set_option(net::socket_base::broadcast(true));
        
                const auto broadcast_ep = net::ip::udp::endpoint(
                    net::ip::address_v4::broadcast(), port);
                
                int counter = 0;
                while(!stop_flag){
                    std::string msg = "broadcasting #" + std::to_string(counter++);
                    socket.send_to(net::buffer(msg), broadcast_ep);
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            } catch(std::exception &e){
                std::cerr<<"An error has occurred: "<<e.what()<<std::endl;
            }
        }
        
        void broadcast_receiver(const short port)const {
            try {
                net::io_context io;
                net::ip::udp::socket socket(io, 
                    net::ip::udp::endpoint(net::ip::udp::v4(), port));
                
                const size_t buffer_size = 1024;
                std::vector<char> buffer(buffer_size);
                net::ip::udp::endpoint remote_ep;
        
                while(!stop_flag){
                    size_t len = socket.receive_from(net::buffer(buffer), remote_ep);
                    std::string message(buffer.begin(), buffer.begin() + (int)len);
                    std::cout<<"Received ["<<remote_ep<<"]: "
                            << message<<std::endl;
                }
            }
            catch (const std::exception &e){
                std::cerr<<"Receiver error: " << e.what() << std::endl;
            }
        }     
};