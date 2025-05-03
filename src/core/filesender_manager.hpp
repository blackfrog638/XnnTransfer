#include <thread>
#include <atomic>
#include "../broadcast/broadcast_manager.hpp"
#include "../authentication/authenticator.hpp"

class FilesenderManager {
    public:
        std::atomic<bool> global_stop_flag{false};

        Account account;

        Authenticator authenticator;
        BroadcastManager broadcast_manager;

        explicit FilesenderManager(short port){
            authenticator.main_handler();
            account = authenticator.get_profile();
            account.port = port;
            broadcast_manager.account = account;
        }

        void run_broadcast(){
            std::thread sender_thread([&]() { broadcast_manager.broadcast_sender(account.port); });
            std::thread receiver_thread([&]() { broadcast_manager.broadcast_receiver(account.port); });
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