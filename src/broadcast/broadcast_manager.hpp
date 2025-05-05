#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include <vector>

#include <boost/asio.hpp>

#include "../authentication/profile.hpp"

namespace net = boost::asio;

class BroadcastManager {
    public:
        Account account;
        std::atomic<bool> stop_flag{false};//使用原子变量控制线程停止

        void broadcast_sender(const short port){
            try {
                net::io_context io;
                net::ip::udp::socket socket(io, net::ip::udp::v4());
                socket.set_option(net::socket_base::broadcast(true));
        
                const auto broadcast_ep = net::ip::udp::endpoint(
                    net::ip::address_v4::broadcast(), port);
                
                std::string new_ip = broadcast_ep.address().to_string();
                account.ip = new_ip;

                while(!stop_flag){

                    std::ostringstream oss;
                    boost::archive::text_oarchive oa(oss);
                    oa << account;
                    std::string serialized_account = oss.str();

                    socket.send_to(net::buffer(serialized_account), broadcast_ep);
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
                        std::cout << "name:" << message.name << " ip:" << message.ip
                                  << " port:" << message.port << std::endl;
                    }
                }
            }
        }
        
    private:
        //!TODO: 改变返回类型
        std::vector<Account> receiver_list(const short port)const {
            std::vector<Account> received_messages;
            try {
                net::io_context io;
                net::ip::udp::socket socket(io, 
                    net::ip::udp::endpoint(net::ip::udp::v4(), port));
                
                const size_t buffer_size = 1024;
                std::string serialized_str;
                std::vector<char> buffer(buffer_size);
                net::ip::udp::endpoint remote_ep;
                //获取当时时钟，限时3秒刷新一次
                auto clock_start = std::chrono::high_resolution_clock::now();

                while(!stop_flag){
                    size_t len = socket.receive_from(net::buffer(buffer), remote_ep);
                    std::string serialized_str(buffer.begin(), buffer.begin() + (int)len);

                    std::istringstream iss(serialized_str);
                    boost::archive::text_iarchive ia(iss);
                    Account received_data;
                    ia >> received_data;

                    if(!std::count(received_messages.begin(), received_messages.end(), received_data)){
                        received_messages.push_back(received_data);
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