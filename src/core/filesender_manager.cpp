#include <iostream>
#include <ostream>
#include <string>
#include <thread>
#include "filesender_manager.hpp"

using namespace nlohmann;

FilesenderManager::FilesenderManager(short port):
    broadcast_manager(io, account),
    verificator(account.ip, account, io, whitelist),
    transfer_manager(io)
{
    authenticator.main_handler();
    account = authenticator.get_profile();
    account.port = port;
    broadcast_manager.account = account;
    verificator.account = account;
}

Account FilesenderManager::get_target_ip(const std::string& target_id)const{
    for (const auto& account : broadcast_manager.receiver_list) {
        if (account.name == target_id) {
            return  account;
        }
    }
    return Account();
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

    Account target_account = get_target_ip(target_user);
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

void FilesenderManager::run_transfer(const std::string &target_id, const std::string &file_path){
    Account target = get_target_ip(target_id);
    if (target.name.empty()) {
        std::cerr << "Target account not found!" << std::endl;
        throw std::runtime_error("Target account not found!");    
    }
    std::cout<<"Target account found: " << target.name<<std::endl;
    std::string file_path_copy = file_path;
    transfer_manager.run(target.ip, target.port, file_path_copy);
}

void FilesenderManager::verifying(){
    while(true){
        json status = verificator.verify_user();
        std::string status_type = status["type"];
        if(status_type == "verification_response"){
            if(status["status"] == "failure")continue;
            std::cout<<"Verification successful!"<<std::endl;
            std::cout<<"Transfer enabled!"<<std::endl;
            std::string ip = status["ip"], port = status["port"], file_path = "src\\temp\\test.txt";
            transfer_manager.run(ip, std::stoi(port), file_path);
        }
        if(status_type == "verification_request"){
            //std::string ip = status["ip"], port = status["port"], file_path = "src\\temp\\test.txt";
            //transfer_manager.run(ip, std::stoi(port), file_path);
        }
    }
            
}


