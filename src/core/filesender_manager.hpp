#pragma once

#include <string>
#include<utility>
#include <atomic>
#include <set>
#include "../broadcast/broadcast_manager.hpp"
#include "../authentication/authenticator.hpp"
#include "../broadcast/verificator.hpp"
#include "../transfer/transfer_manager.hpp"

namespace net = boost::asio;

class FilesenderManager {
    public:
        std::atomic<bool> global_stop_flag{false};
        std::set<std::string> whitelist;

        Account account;
        net::io_context io;
        
        Authenticator authenticator;
        BroadcastManager broadcast_manager;
        Verificator verificator;
        TransferManager transfer_manager;

        explicit FilesenderManager(short port);
        std::pair<std::string, std::string> run_broadcast();
        void run_verification(const std::string &target_user, const std::string &password)const;
        void verifying()const;
        void run_transfer(const std::string &target_ip, const std::string &file_path);
};