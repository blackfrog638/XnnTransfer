#include "manager.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <thread>

json Manager::read_json_profile() {
    std::filesystem::path p;
    p /= "src";
    p /= "temp";
    p /= "profile.json";
    std::ifstream inputFile(p);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to fetch file \"" << p << "\"" << std::endl;
        return 1;
    }
    json json_data;
    try {
        inputFile >> json_data;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse_error: " << e.what() << " offset " << e.byte << "" << std::endl;
        inputFile.close();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "unexpected: " << e.what() << std::endl;
        inputFile.close();
        return 1;
    }
    return json_data;
}

std::string Manager::get_local_ip(net::io_context& ioc_) {
    try {
        auto local_ip = net::ip::host_name();
        auto resolver = net::ip::udp::resolver(ioc_);
        auto endpoints = resolver.resolve(local_ip, "");
        local_ip = endpoints.begin()->endpoint().address().to_string();
        return local_ip;
    } catch (std::exception& e) {
        std::cout << "error: " << e.what() << std::endl;
    }
    return "";
}

Manager::Manager(net::io_context& ioc_)
    : ioc(ioc_), port(12345), ip(get_local_ip(ioc_)), bc(ioc, id, ip, port),
      server(ioc, id, ip, pw, port, whitelist, response_queue), client(ioc, id, ip, port, response_queue) {
    json profile = read_json_profile();
    id = profile["id"];
    pw = profile["pw"];
    // ioc.run();
}

net::awaitable<void> Manager::run() {
    std::cout << "Welcome to Filesender for ByteSpark! Please enter the number" << std::endl;
    std::cout << "1. verification message" << std::endl;
    std::cout << "2. Send File" << std::endl;
    net::co_spawn(ioc, bc.main_handler(), net::detached);
    net::co_spawn(ioc, client.handle_request(), net::detached);
    net::co_spawn(ioc, server.receiver(), net::detached);
    std::thread input_thread([this]() { this->console_input_loop(); });
    input_thread.detach();
    int op;
    co_return;
}

void Manager::console_input_loop() {
    int op;
    while (true) {
        std::cin >> op;
        if (op == 1) {
            std::string tar_id, tar_pw;
            std::cout << "Username:" << "\n";
            std::cin >> tar_id;
            std::cout << "Password for " << tar_id << ":" << std::endl;
            std::cin >> tar_pw;
            std::string tar_ip = bc.find_user(tar_id);
            net::co_spawn(ioc, client.send_request(tar_ip, tar_pw), net::detached);
        }
        if (op == 2) {
            std::string tar_id, file_path;
            std::cout << "Username:" << "\n";
            std::cin >> tar_id;
            std::cout << "File path for " << tar_id << ":" << std::endl;
            std::cin >> file_path;
            std::string tar_ip = bc.find_user(tar_id);
            net::co_spawn(ioc, client.transfer_file(tar_ip, file_path), net::detached);
        }
    }
}
// E:\code\FileSender-ByteSpark\src\temp\profile.json
// E:\code\FileSender-ByteSpark\src\temp\video.mp4
// E:\code\FileSender-ByteSpark\src\temp\20.in