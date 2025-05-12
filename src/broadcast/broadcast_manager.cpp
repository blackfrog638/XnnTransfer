#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "broadcast_manager.hpp"

using namespace nlohmann;

BroadcastManager::BroadcastManager(net::io_context &io_context,
                                   const Account &account) :
    account(account),
    io(io_context) {}

void BroadcastManager::broadcast_sender(short port){
    try {
        net::ip::udp::socket socket(io, net::ip::udp::v4());
        socket.set_option(net::socket_base::broadcast(true));

        const auto broadcast_ep = net::ip::udp::endpoint(
            net::ip::address_v4::broadcast(), port);
    
        socket.bind(net::ip::udp::endpoint(net::ip::udp::v4(), 0));
        auto local_ip = net::ip::host_name();
        auto resolver = net::ip::udp::resolver(io);
        auto endpoints = resolver.resolve(local_ip, "");
        local_ip = endpoints.begin()->endpoint().address().to_string();
        //std::cout<<"Local IP: " << local_ip << std::endl;
        account.ip = local_ip;

        while(true){
            json j;
            j["type"] = "broadcast";
            j["name"] = account.name;
            j["ip"] = account.ip;
            j["port"] = account.port;

            std::string msg = j.dump();
            socket.send_to(net::buffer(msg), broadcast_ep);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch(std::exception &e){
        std::cerr<<"An error has occurred: "<<e.what()<<std::endl;
    }
}

void BroadcastManager::broadcast_receiver(short port){
    while(!stop_flag){
        receiver_list = get_receiver_list(port);
        if(!receiver_list.empty()){
            std::cout << "Received messages: " << std::endl;
            for(const auto& message : receiver_list){
                std::cout << "name:" << message.name << " ip:" << message.ip
                          << " port:" << message.port << std::endl;
            }
        }
    }
}

std::vector<Account> BroadcastManager::get_receiver_list(short port){
    std::vector<Account> received_messages;
    try {
        net::ip::udp::socket socket(io, 
            net::ip::udp::endpoint(net::ip::udp::v4(), port));
        
        const size_t buffer_size = 1024;
        std::vector<char> buffer(buffer_size);
        net::ip::udp::endpoint remote_ep;
        //获取当时时钟，限时3秒刷新一次
        auto clock_start = std::chrono::steady_clock::now();

        while(!stop_flag){
            size_t len = socket.receive_from(net::buffer(buffer), remote_ep);
            std::string msg(buffer.begin(), buffer.begin() + (int)len);

            json j = json::parse(msg);
            if(j["type"] != "broadcast"){
                continue;
            }
            Account received_data;
            received_data.name = j["name"];
            received_data.ip = j["ip"];
            received_data.port = j["port"];

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