#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>

#include <boost/asio.hpp>

#include "../authentication/profile.hpp"

namespace net = boost::asio;

class BroadcastManager {
    public:
        Account account;
        std::atomic<bool> stop_flag{false};//使用原子变量控制线程停止

        void broadcast_sender(const short port)const{
            try {
                net::io_context io;
                net::ip::udp::socket socket(io, net::ip::udp::v4());
                socket.set_option(net::socket_base::broadcast(true));
        
                const auto broadcast_ep = net::ip::udp::endpoint(
                    net::ip::address_v4::broadcast(), port);
                
                while(!stop_flag){
                    std::string msg = "name: " + account.name + " ip: " + net::ip::host_name();
                    socket.send_to(net::buffer(msg), broadcast_ep);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            } catch(std::exception &e){
                std::cerr<<"An error has occurred: "<<e.what()<<std::endl;
            }
        }

        void broadcast_receiver(const short port)const{
            while(!stop_flag){
                auto received_messages = receiver_list(port);
                if(!received_messages.empty()){
                    std::cout << "Received messages: " << std::endl;
                    for(const auto& message : received_messages){
                        std::cout << message << std::endl;
                    }
                }
            }
        }
        
    private:
        //!TODO: 改变返回类型
        std::vector<std::string> receiver_list(const short port)const {
            std::vector<std::string> received_messages;
            try {
                net::io_context io;
                net::ip::udp::socket socket(io, 
                    net::ip::udp::endpoint(net::ip::udp::v4(), port));
                
                const size_t buffer_size = 1024;
                std::vector<char> buffer(buffer_size);
                net::ip::udp::endpoint remote_ep;
                //获取当时始终，限时3秒刷新一次
                auto clock_start = std::chrono::high_resolution_clock::now();

                while(!stop_flag){
                    size_t len = socket.receive_from(net::buffer(buffer), remote_ep);
                    std::string message(buffer.begin(), buffer.begin() + (int)len);

                    if(!std::count(received_messages.begin(), received_messages.end(), message)){
                        received_messages.push_back(message);
                    }

                    auto clock_end = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(clock_end - clock_start).count();
                    if(elapsed > 3){
                        break;
                    }
                }
            }
            catch (const std::exception &e){
                std::cerr<<"Receiver error: " << e.what() << std::endl;
            }
            return received_messages;
        }     
};