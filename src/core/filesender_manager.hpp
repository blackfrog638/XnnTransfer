#pragma once

#include <atomic>
#include <set>
#include "../broadcast/broadcast_manager.hpp"
#include "../authentication/authenticator.hpp"

class FilesenderManager {
    public:
        std::atomic<bool> global_stop_flag{false};
        std::set<std::string> whitelist;

        Account account;

        Authenticator authenticator;
        BroadcastManager broadcast_manager;

        explicit FilesenderManager(short port);
        std::string run_broadcast();
};