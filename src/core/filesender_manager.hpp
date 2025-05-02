#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <utility>

#include "../broadcast/broadcast_manager.hpp"
#include "../authentication/authenticator.hpp"

class FilesenderManager {
    public:
        std::atomic<bool> global_stop_flag{false};

        std::string name;
        std::string password;
        std::string ip_address;
        short port;

        Authenticator authenticator;
        BroadcastManager broadcast_manager;

        FilesenderManager(std::string name, std::string password,
                          const short port) :
            name(name),
            password(std::move(password)), 
            port(port),
            broadcast_manager(std::move(name)) {}
        
        explicit FilesenderManager(const short port) : 
            port(port),
            //!TODO: 这里的name和password应该是从配置文件中读取的
            broadcast_manager(name) {}

        void run_broadcast(){
            std::thread sender_thread([&]() { broadcast_manager.broadcast_sender(port); });
            std::thread receiver_thread([&]() { broadcast_manager.broadcast_receiver(port); });
            //!TODO: 得到链接之后结束广播
            while(1){
                if(broadcast_manager.stop_flag){
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            broadcast_manager.stop_flag = true;

            sender_thread.join();
            receiver_thread.join();
        }

    private:


};