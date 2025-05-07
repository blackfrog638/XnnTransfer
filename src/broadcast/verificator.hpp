#pragma once

#include "../authentication/profile.hpp"

class verificator {
    public:
    Account account;
    std::string target_user;

    int send_verification_request(const std::string& target_user);
    bool verify_user(const std::string& target_user);
};
