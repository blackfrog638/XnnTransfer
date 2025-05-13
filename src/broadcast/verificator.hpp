#pragma once
#include <set>
#include <string>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include "../authentication/profile.hpp"

using namespace nlohmann;
namespace net = boost::asio;

class Verificator {
    public:
    Account &account;
    std::string &target_user;
    net::io_context &io;
    std::set<std::string> &whitelist;

    Verificator(std::string &target_user, Account &account,
        net::io_context &io, std::set<std::string> &whitelist);

    int send_request(json &j, std::string& raw_ip)const;
    json verify_user()const;
    void send_verification_request(const std::string &password)const;
};
