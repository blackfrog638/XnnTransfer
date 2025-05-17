#include "broadcast.hpp"

#include <array>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <exception>
#include <iostream>
#include <ostream>

Broadcast::Broadcast(net::io_context& ioc_, std::string& id_, std::string& ip_, short port_)
    : id(id_), ip(ip_), port(port_), ioc(ioc_), broadcast_socket(ioc), listen_socket(ioc) {}

net::awaitable<void> Broadcast::broadcaster() {
    try {
        const std::string broadcast_address = "255.255.255.255";
        net::ip::udp::endpoint ep(net::ip::make_address(broadcast_address), port);

        json msg;
        msg["type"] = "broadcast";
        msg["id"] = id;
        msg["ip"] = ip;
        msg["port"] = std::to_string(port);

        std::string data = msg.dump();
        std::cout << "Start broadcasting..." << std::endl;

        // 使用steadytimer计时防止阻塞
        net::steady_timer timer(broadcast_socket.get_executor());

        while (true) {
            co_await broadcast_socket.async_send_to(net::buffer(data), ep);
            // std::cout << "loop" << std::endl;
            timer.expires_after(std::chrono::seconds(1));
            co_await timer.async_wait(net::use_awaitable);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in broadcaster: " << e.what();
    }
}

net::awaitable<void> Broadcast::listener() {
    try {
        const size_t buffer_size = 1024;
        std::array<char, buffer_size> buffer;

        net::ip::udp::endpoint ep;

        auto clock_start = std::chrono::steady_clock::now();
        std::cout << "start listening broadcast..." << std::endl;

        // net::steady_timer timer(broadcast_socket.get_executor());
        while (true) {

            size_t recv_size = co_await listen_socket.async_receive_from(net::buffer(buffer), ep, net::use_awaitable);
            std::string data(buffer.data(), recv_size);
            try {
                json j = json::parse(data);
                if (j["type"] == "broadcast") {
                    bool b = false;
                    for (auto& [i, jj, k] : temp) {
                        // std::cout << i << " " << jj << " " << k << std::endl;
                        if (j["ip"].dump() == jj) {
                            b = true;
                        }
                    }
                    if (!b)
                        temp.push_back({j["id"].dump(), j["ip"].dump(), j["port"].dump()});
                }
            } catch (const std::exception& e) {
                std::cerr << "Parse error: " << e.what();
            }

            // timer.expires_after(std::chrono::seconds(3));
            // co_await timer.async_wait(net::use_awaitable);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in broadcaster: " << e.what();
    }
}

net::awaitable<void> Broadcast::list_handler() {
    net::steady_timer timer(broadcast_socket.get_executor());
    while (true) {

        timer.expires_after(std::chrono::seconds(3));
        co_await timer.async_wait(net::use_awaitable);
        receiver_list = temp;
        for (auto& [i, j, k] : receiver_list) {
            i = i.substr(1, i.size() - 2);
            j = j.substr(1, j.size() - 2);
            k = k.substr(1, k.size() - 2);
            std::cout << "[DEVICE] id:" << i << " ip:" << j << " port:" << k << std::endl;
        }
        temp.clear();
    }
}

net::awaitable<void> Broadcast::main_handler() {
    try {
        broadcast_socket.open(net::ip::udp::v4());
        broadcast_socket.set_option(net::socket_base::broadcast(true));

        net::ip::udp::endpoint listen_endpoint(net::ip::udp::v4(), port);
        listen_socket.open(listen_endpoint.protocol());
        listen_socket.bind(listen_endpoint);

        net::co_spawn(ioc, broadcaster(), net::detached);
        net::co_spawn(ioc, listener(), net::detached);
        net::co_spawn(ioc, list_handler(), net::detached);
    } catch (const std::exception& e) {
        std::cerr << "Error!" << e.what();
    }
    co_return;
}

std::string Broadcast::find_user(std::string& id) {
    for (auto& [i, j, k] : receiver_list) {
        if (i == id) {
            return j;
        }
    }
    std::cerr << "[ERROR] No such user!" << std::endl;
    return "";
}