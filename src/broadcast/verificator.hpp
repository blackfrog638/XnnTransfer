#pragma once

#include <nlohmann/json.hpp>

#include "../authentication/profile.hpp"

using namespace nlohmann;

class Verificator {
    public:
    Account account;
    std::string target_user;
    Verificator(const std::string &target_user, const Account &account);
    Verificator() = default;

    int send_request(json &j)const;
    bool verify_user()const;
    void send_verification_request(const std::string &password)const;
};
