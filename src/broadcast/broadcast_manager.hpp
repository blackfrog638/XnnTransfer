#pragma once

#include <atomic>
#include <vector>

#include <boost/asio.hpp>

#include "../authentication/profile.hpp"

namespace net = boost::asio;

class BroadcastManager {
    public:
        Account account;
        std::atomic<bool> stop_flag{false};//使用原子变量控制线程停止
        std::vector<Account> receiver_list;
        net::io_context &io;

        explicit BroadcastManager(net::io_context &io_context,
            Account &account);

        void broadcast_sender(short port);
        void broadcast_receiver(short port);
        
    private:
        //!TODO: 改变返回类型
        std::vector<Account> get_receiver_list(short port);
};