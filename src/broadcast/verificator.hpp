#pragma once

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include "../authentication/profile.hpp"

using namespace nlohmann;
namespace net = boost::asio;

class Verificator {
    public:
    Account account;
    std::string target_user;
    net::io_context &io;

    Verificator(const std::string &target_user, const Account &account, net::io_context &io);

    int send_request(json &j)const;
    bool verify_user()const;
    void send_verification_request(const std::string &password)const;
};
