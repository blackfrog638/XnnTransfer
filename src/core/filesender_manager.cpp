#include <iostream>
#include <thread>
#include "filesender_manager.hpp"

FilesenderManager::FilesenderManager(short port){
    authenticator.main_handler();
    account = authenticator.get_profile();
    account.port = port;
    broadcast_manager.account = account;
}

std::string FilesenderManager::run_broadcast(){
    std::thread sender_thread([&]() { broadcast_manager.broadcast_sender(account.port); });
    std::thread receiver_thread([&]() { broadcast_manager.broadcast_receiver(account.port); });
    //!TODO: 得到链接之后结束广播
    // while(true){
    //     if(broadcast_manager.stop_flag){
    //         break;
    //     }
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }
    std::string target_user;
    std::cin>>target_user;
    std::cout<<"captured target: "<<target_user<<std::endl;
    broadcast_manager.stop_flag = true;

    sender_thread.join();
    receiver_thread.join();
    return target_user;
}