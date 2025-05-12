#include <iostream>
#include <thread>
#include "filesender_manager.hpp"

FilesenderManager::FilesenderManager(short port):
    broadcast_manager(io, account),
    verificator(account.name, account, io, whitelist),
    transfer_manager(io)
{
    authenticator.main_handler();
    account = authenticator.get_profile();
    account.port = port;
    broadcast_manager.account = account;
    verificator.account = account;
}

std::pair<std::string, std::string> FilesenderManager::run_broadcast(){
    std::thread sender_thread([&]() { broadcast_manager.broadcast_sender(account.port); });
    std::thread receiver_thread([&]() { broadcast_manager.broadcast_receiver(account.port); });

    std::string target_user;
    std::cin>>target_user;
    broadcast_manager.stop_flag = true;

    std::string password;
    std::cout << "Enter password for " << target_user << ": ";
    std::cin >> password;

    // for(auto &i : broadcast_manager.receiver_list){
    //     std::cout<<"captured: "<<i.name<<std::endl;
    // }
    //std::cout<<"Target user: " << target_user << std::endl;

    Account target_account;
    for (const auto& account : broadcast_manager.receiver_list) {
        if (account.name == target_user) {
            target_account = account;
            break;
        }
    }

    if (target_account.name.empty()) {
        std::cerr << "Target account not found!" << std::endl;
        throw std::runtime_error("Target account not found!");    
    }

    std::cout<<"Target account found: " << target_account.name << 
    " " <<target_account.ip<< std::endl;
    std::string target_ip = target_account.ip;
    verificator.target_user = target_ip;
    verificator.send_verification_request(password);
    sender_thread.join();
    receiver_thread.join();
    
    return {target_ip, password};
}

void FilesenderManager::run_verification(const std::string &target_user, const std::string &password)const {
    verificator.send_verification_request(password);
}

void FilesenderManager::verifying()const{
    while(true){
        if(verificator.verify_user()){
            std::cout<<"Verification successful!"<<std::endl;
        }
        else{
            std::cout<<"Verification failed!"<<std::endl;
        }
    }
}

void FilesenderManager::run_transfer(const std::string &target_ip, const std::string &file_path) {
    std::cout<<"Starting file transfer..."<<std::endl;
    transfer_manager.run(target_ip, account.port, file_path);
    std::cout<<"File transfer completed!"<<std::endl;
}