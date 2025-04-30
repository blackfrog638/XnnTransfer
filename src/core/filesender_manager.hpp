#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <utility>

#include "../broadcast/broadcast_manager.hpp"

class FilesenderManager {
    public:
        std::atomic<bool> global_stop_flag{false};

        std::string name;
        std::string password;
        std::string ip_address;
        short port;

        BroadcastManager broadcast_manager;
        FilesenderManager(std::string name, std::string password,
                          const short port) :
            name(std::move(name)),
            password(std::move(password)), 
            port(port) {}
        
        explicit FilesenderManager(const short port) : port(port) {}

        void run_broadcast(){
            BroadcastManager bm;

            std::thread sender_thread([&bm, this]() { bm.broadcast_sender(port); });
            std::thread receiver_thread([&bm, this]() { bm.broadcast_receiver(port); });

            //TODO: 得到链接之后结束广播
            std::cin.get();
            bm.stop_flag = true;

            sender_thread.join();
            receiver_thread.join();
        }

    private:


};