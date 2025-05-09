#include "verificator.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

#include <boost/asio.hpp>
#include <nlohmann/detail/macro_scope.hpp>


namespace net = boost::asio;
using namespace nlohmann;

Verificator::Verificator(
    const std::string &target_user,
    const Account &account,
    net::io_context &io) :
    io(io),
    target_user(target_user),
    account(account) 
{}

int Verificator::send_request(json &j) const{
    const std::string &raw_ip = target_user;
    short raw_port = account.port;
    try{
        net::ip::tcp::endpoint
            ep(net::ip::make_address(raw_ip), raw_port);
        
        net::ip::tcp::socket socket(io);
        socket.connect(ep);

        std::string msg = j.dump();
        socket.send(net::buffer(msg));
    }
    catch(boost::system::system_error &e){
        std::cerr<<"An error has occurred: "<<e.what()<<std::endl;
        return e.code().value();
    }
    return 0;
}

void Verificator::send_verification_request(const std::string &password)const {
    json j;
    j["type"] = "verification_request";
    j["name"] = account.name;
    j["ip"] = account.ip;
    j["password"] = password;

    send_request(j);
}

bool Verificator::verify_user()const{
    net::ip::tcp::endpoint ep(net::ip::address_v4::any(),
		account.port);
    net::ip::tcp::acceptor acceptor(io, ep);
    acceptor.set_option(net::socket_base::reuse_address(true));
    acceptor.listen();

    net::ip::tcp::socket socket(io);
    acceptor.accept(socket);

    std::string msg(1024, '\0');
    boost::system::error_code error;
    size_t len = socket.read_some(net::buffer(msg), error);
    if (error) {
        std::cerr << "Error receiving message: " << error.message() << std::endl;
        return false;
    }

    msg.resize(len);
    json j = json::parse(msg);

    json response;
    response["type"] = "verification_response";
    response["name"] = account.name;
    response["ip"] = account.ip;

    if (j["type"] == "verification_request") {
        if(j["ip"] == account.ip && j["password"] == account.password) {
            std::cout << "Verification successful for user: " << j["name"] << std::endl;
            response["status"] = "success";
            send_request(response);
            return true;
        } else {
            std::cout << "Verification failed for user: " << j["name"] << std::endl;
            response["status"] = "failure";
            send_request(response);
        }
    }
    return false;
}