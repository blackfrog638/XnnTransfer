#include "verificator.hpp"
#include "../transfer/async_recv.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/detail/macro_scope.hpp>


namespace net = boost::asio;
namespace beast = boost::beast;
using namespace nlohmann;

Verificator::Verificator(
    std::string &target_user,
    Account &account,
    net::io_context &io, 
    std::set<std::string> &whitelist) :
    io(io),
    target_user(target_user),
    account(account),
    whitelist(whitelist)
{}

int Verificator::send_request(json &j, std::string &raw_ip) const{
    short raw_port = account.port;
    try{
        net::ip::tcp::endpoint
            ep(net::ip::make_address(raw_ip), raw_port);
        
        net::ip::tcp::socket socket(io);
        socket.connect(ep);
        // if(j["type"] == "verification_response"){
        //     std::cout<<raw_ip<<" "<<raw_port<<std::endl;
        // }
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
    j["port"] = account.port;
    j["password"] = password;

    send_request(j, target_user);
}

json Verificator::verify_user()const{
    net::ip::tcp::endpoint ep(net::ip::address_v6::any(),
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

    std::string j_type = j["type"];
    //j_type = j_type.substr(1, j.size() -2);
    std::cout<<j["ip"]<<" "<<account.ip<<std::endl;


    if (j_type == "verification_request") {
        if(j["ip"] == account.ip && j["password"] == account.password) {
            std::cout << "Verification successful for user: " << j["name"] << std::endl;
            response["status"] = "success";
            std::string raw_ip = j["ip"];
            whitelist.insert(raw_ip);
            send_request(response, raw_ip);
        } else {
            std::cout << "Verification failed for user: " << j["name"] << std::endl;
            response["status"] = "failure";
            std::string raw_ip = j["ip"];
            send_request(response, raw_ip);
        }
    }
    if(j_type == "file_transfer"){
        if(whitelist.find(j["ip"]) != whitelist.end()){
            std::cout << "File transfer request from: " << j["name"] << std::endl;
            std::string file_path = j["file_name"];
            beast::websocket::stream<net::ip::tcp::socket> ws(net::make_strand(io));
            async_receive(ws, file_path);
            return "file_transfer";
        }
        else {
            std::cout << "File transfer request from unverified user: " << j["name"] << std::endl;
        }
    }
    if(j_type == "verification_response"){
        std::cout<<"catch response"<<std::endl;
    }
    return j;
}