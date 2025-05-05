#pragma once

#include <atomic>
#include "../broadcast/broadcast_manager.hpp"
#include "../authentication/authenticator.hpp"

class FilesenderManager {
    public:
        std::atomic<bool> global_stop_flag{false};

        Account account;

        Authenticator authenticator;
        BroadcastManager broadcast_manager;

        explicit FilesenderManager(short port);
        void run_broadcast();
};